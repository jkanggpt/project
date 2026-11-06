// Main.cs — 메인 장면: 새 게임을 만들고, 지도·건물·주민·카메라·HUD·낮밤을 연결합니다.
// (제목 화면과 불러오기 메뉴는 마일스톤 6 에서 이 앞에 붙습니다)
//
// 화면을 맞추는 때는 두 가지입니다.
//   1) 틱이 진행됐을 때 (GameClock.Ticked)
//   2) 플레이어 명령이 성공했을 때 (CoreBridge.CommandApplied) — 일시정지 중에 지어도 바로 보이게
// 둘 다 WorldSync.Reconcile 한 번으로 스냅숏 전체를 다시 맞춥니다.
using System;
using Godot;
using MagicCastle.Audio;
using MagicCastle.Core;
using MagicCastle.Debugging;
using MagicCastle.UI;
using MagicCastle.World;

namespace MagicCastle;

public partial class Main : Node2D
{
    private bool _reconcileQueued;
    private int _shownOutcome; // 이미 보여 준 결과 (같은 화면을 두 번 띄우지 않음)

    public MapView Map { get; private set; } = null!;
    public CameraRig Camera { get; private set; } = null!;
    public DayNight DayNight { get; private set; } = null!;
    public Hud Hud { get; private set; } = null!;
    public WorldSync Sync { get; private set; } = null!;
    public BuildMode Tools { get; private set; } = null!;
    public WeatherFx Weather { get; private set; } = null!;
    public CombatDirector Combat { get; private set; } = null!;
    public Sfx Sounds { get; private set; } = null!;

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        InputActions.EnsureRegistered();
        Map = GetNode<MapView>("World/Map");
        Camera = GetNode<CameraRig>("Camera");
        DayNight = GetNode<DayNight>("DayNight");
        Hud = GetNode<Hud>("Hud");
        Sync = GetNode<WorldSync>("WorldSync");
        Tools = GetNode<BuildMode>("World/BuildMode");
        Weather = GetNode<WeatherFx>("WeatherFx");
        Sync.Map = Map;
        Sync.BuildingsLayer = GetNode<Node2D>("World/Buildings");
        Sync.UnitsLayer = GetNode<Node2D>("World/Units");
        Sync.Floating = GetNode<FloatingText>("World/FloatingText");
        Sounds = new Sfx { Name = "Sfx" };
        AddChild(Sounds);
        // 싸움은 일시정지되면 멈춰야 하므로 World(일시정지 가능) 아래에 둡니다
        Combat = new CombatDirector { Name = "Combat", Sync = Sync, UnitsLayer = Sync.UnitsLayer, Camera = Camera };
        GetNode<Node2D>("World").AddChild(Combat);

        ApplyUiScale();
        GetViewport().SizeChanged += ApplyUiScale;
        // 창을 닫을 때 소리를 먼저 멈추고 잠깐 뒤에 끝냅니다 (소리가 나는 중에 끝나면 종료 경고가 남음)
        GetTree().AutoAcceptQuit = false;

        LaunchOptions options = LaunchOptions.Current;
        Camera.EdgeScrollEnabled = !options.Automated;

        CoreBridge core = CoreBridge.Instance;
        if (core.Status != CoreStatus.Ready)
        {
            Hud.ShowFatal(core.StatusMessage, core.StatusDetail);
            if (options.Automated)
            {
                GD.PrintErr($"castle_core not ready: {core.StatusDetail}");
                GetTree().Quit(2);
            }
            return;
        }

        int seed = options.Seed ?? (int)(GD.Randi() & 0x7FFFFFFF);
        CoreResult result = core.NewGame(Strings.DefaultCastleName, seed, options.Difficulty);
        if (!result.Ok)
        {
            Hud.ShowFatal(Strings.NewGameFailed, result.Name);
            if (options.Automated)
            {
                GetTree().Quit(2);
            }
            return;
        }

        DayNight.Configure(core.Config.NightStartMinute, core.Config.NightEndMinute);
        GameClock.Instance.Ticked += OnTicked;
        core.CommandApplied += QueueReconcile;
        Sync.Notify += Hud.ShowNotice;
        Sync.EventPolled += Combat.OnCoreEvent;
        Sync.Refreshed += OnRefreshed;
        Tools.SelectionChanged += Hud.Inspector.ShowSelection;
        Tools.Message += Hud.ShowToast;
        Tools.HintChanged += Hud.SetHint;
        Hud.Villagers.VillagerChosen += OnVillagerChosen;
        Hud.Toasts.JumpRequested += position => Camera.CenterOn(position);
        Hud.Outcome.NewGameRequested += StartNewGame;
        Hud.Outcome.ContinueRequested += () => GameClock.Instance.SetSpeed(1);
        RebuildWorldView();
        foreach (string warning in core.DataWarnings)
        {
            Hud.ShowToast(warning);
        }

        if (options.SmokeTest)
        {
            AddChild(new SmokeTest());
        }
        else if (options.ScreenshotDir != null)
        {
            AddChild(new ScreenshotTour());
        }
    }

    public override void _Notification(int what)
    {
        if (what == NotificationWMCloseRequest)
        {
            Sfx.QuitQuietly(GetTree(), 0);
        }
    }

    public override void _ExitTree()
    {
        GameClock.Instance.Ticked -= OnTicked;
        CoreBridge.Instance.CommandApplied -= QueueReconcile;
    }

    public override void _Process(double delta)
    {
        // 낮밤 색은 틱 사이에도 부드럽게 바뀌도록 매 프레임 계산합니다
        if (CoreBridge.Instance.HasWorld)
        {
            double minute = Sync.Summary.MinuteOfDay + GameClock.Instance.TickFraction * CoreBridge.Instance.Config.MinutesPerTick;
            DayNight.SetMinute(minute);
        }
    }

    /// <summary>새 게임이나 불러오기 뒤에 지도와 카메라를 처음부터 다시 맞춥니다.</summary>
    public void RebuildWorldView()
    {
        Tools.Cancel();
        Tools.Select(0, 0);
        byte[]? terrain = CoreBridge.Instance.CopyTerrain(out int width, out int height);
        if (terrain == null || !CoreBridge.Instance.TryGetSummary(out CoreSummaryView summary))
        {
            Hud.ShowFatal(Strings.NewGameFailed, "core_copy_terrain");
            return;
        }
        Map.Build(terrain, width, height, summary);
        Combat.Reset();
        Sync.Reset();
        Camera.SetBounds(Map.PixelBounds);
        Camera.CenterOn(Map.TileCenter(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize));
        GameClock.Instance.ResetAccumulator();
        Hud.Bar.Rebuild();
        Hud.UpdateSummary(Sync.Summary);
        DayNight.SetMinute(Sync.Summary.MinuteOfDay);
    }

    private void OnTicked(int ticks)
    {
        _reconcileQueued = false;
        Sync.Reconcile();
    }

    /// <summary>명령이 여러 개 연달아 와도 프레임 끝에 한 번만 맞춥니다.</summary>
    private void QueueReconcile()
    {
        if (_reconcileQueued)
        {
            return;
        }
        _reconcileQueued = true;
        Callable.From(() =>
        {
            if (_reconcileQueued)
            {
                _reconcileQueued = false;
                Sync.Reconcile();
            }
        }).CallDeferred();
    }

    private void OnRefreshed()
    {
        CoreSummaryView summary = Sync.Summary;
        Combat.OnRefreshed();
        CheckOutcome(summary);
        Map.SetLook(summary.Season, summary.Weather, summary.NatureEvent);
        Weather.SetWeather(summary.Weather);
        Hud.UpdateSummary(summary);
        Hud.Inspector.Refresh();
        Hud.Villagers.Refresh();
    }

    /// <summary>
    /// 승리·패배를 처음 보면 시간을 멈추고 결과 화면을 띄웁니다. 진 뒤에는 C 가 시간을 멈추므로 새 게임만 할 수 있고,
    /// 이긴 뒤에는 "계속 다스리기"로 이어 갈 수 있습니다.
    /// </summary>
    private void CheckOutcome(in CoreSummaryView summary)
    {
        if (summary.Outcome == (int)Outcome.None || summary.Outcome == _shownOutcome)
        {
            return;
        }
        _shownOutcome = summary.Outcome;
        if (!LaunchOptions.Current.Automated)
        {
            GameClock.Instance.SetSpeed(0);
        }
        Hud.Outcome.ShowOutcome();
    }

    /// <summary>결과 화면의 "새 게임": 같은 난이도, 새 시드 (제목·새 게임 화면은 마일스톤 6)</summary>
    public void StartNewGame()
    {
        CoreBridge core = CoreBridge.Instance;
        int difficulty = CoreBridge.Instance.HasWorld ? Sync.Summary.Difficulty : (int)LaunchOptions.Current.Difficulty;
        int seed = (int)(GD.Randi() & 0x7FFFFFFF);
        CoreResult result = core.NewGame(Strings.DefaultCastleName, seed, (Difficulty)difficulty);
        if (!result.Ok)
        {
            Hud.ShowToast(Strings.NewGameFailed);
            return;
        }
        _shownOutcome = (int)Outcome.None;
        Hud.Tutorial.Reset();
        RebuildWorldView();
        GameClock.Instance.SetSpeed(1);
    }

    private void OnVillagerChosen(int villagerId)
    {
        Tools.Select(villagerId, 0);
        if (Sync.AgentFor(villagerId) is VillagerAgent agent)
        {
            Camera.CenterOn(agent.Position);
        }
    }

    private void ApplyUiScale()
    {
        UiScale.Apply(UiScale.DefaultFor(DisplayServer.WindowGetSize()));
        Hud.ApplyScale(UiScale.Current);
    }
}
