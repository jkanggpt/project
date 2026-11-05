// Main.cs — 메인 장면: 새 게임을 만들고, 지도·카메라·HUD·낮밤을 연결합니다.
// (제목 화면과 불러오기 메뉴는 마일스톤 6 에서 이 앞에 붙습니다)
using System;
using Godot;
using MagicCastle.Core;
using MagicCastle.Debugging;
using MagicCastle.UI;
using MagicCastle.World;

namespace MagicCastle;

public partial class Main : Node2D
{
    public MapView Map { get; private set; } = null!;
    public CameraRig Camera { get; private set; } = null!;
    public DayNight DayNight { get; private set; } = null!;
    public Hud Hud { get; private set; } = null!;
    public WorldSync Sync { get; private set; } = null!;

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        InputActions.EnsureRegistered();
        Map = GetNode<MapView>("World/Map");
        Camera = GetNode<CameraRig>("Camera");
        DayNight = GetNode<DayNight>("DayNight");
        Hud = GetNode<Hud>("Hud");
        Sync = GetNode<WorldSync>("WorldSync");

        ApplyUiScale();
        GetViewport().SizeChanged += ApplyUiScale;

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
        CoreResult result = core.NewGame(Strings.DefaultCastleName, seed, Difficulty.Normal);
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
        Sync.Notify += Hud.ShowToast;
        RebuildWorldView();

        if (options.SmokeTest)
        {
            AddChild(new SmokeTest());
        }
        else if (options.ScreenshotDir != null)
        {
            AddChild(new ScreenshotTour());
        }
    }

    public override void _ExitTree()
    {
        GameClock.Instance.Ticked -= OnTicked;
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
        Sync.Reset();
        CoreSummaryView summary = Sync.Summary;
        byte[]? terrain = CoreBridge.Instance.CopyTerrain(out int width, out int height);
        if (terrain == null)
        {
            Hud.ShowFatal(Strings.NewGameFailed, "core_copy_terrain");
            return;
        }
        Map.Build(terrain, width, height, summary);
        Camera.SetBounds(Map.PixelBounds);
        Camera.CenterOn(Map.TileCenter(summary.KeepX, summary.KeepY));
        GameClock.Instance.ResetAccumulator();
        Hud.UpdateSummary(summary);
        DayNight.SetMinute(summary.MinuteOfDay);
    }

    private void OnTicked(int ticks)
    {
        Sync.Reconcile();
        Hud.UpdateSummary(Sync.Summary);
    }

    private void ApplyUiScale()
    {
        UiScale.Apply(UiScale.DefaultFor(DisplayServer.WindowGetSize()));
        Hud.ApplyScale(UiScale.Current);
    }
}
