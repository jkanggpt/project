// Main.cs — 메인 장면: 지도·건물·주민·카메라·HUD·낮밤을 연결하고, 한 판의 흐름(제목 → 게임 → 저장·불러오기)을 맡습니다.
//
// 화면을 맞추는 때는 두 가지입니다.
//   1) 틱이 진행됐을 때 (GameClock.Ticked)
//   2) 플레이어 명령이 성공했을 때 (CoreBridge.CommandApplied) — 일시정지 중에 지어도 바로 보이게
// 둘 다 WorldSync.Reconcile 한 번으로 스냅숏 전체를 다시 맞춥니다.
//
// 흐름 (마일스톤 6)
//   시작 → 제목 화면 (뒤에는 배경용 월드를 멈춰 둠) → 새 게임 / 이어하기 / 불러오기 → 게임
//   게임 중 Esc 또는 "메뉴" → 일시정지 메뉴: 저장하기, 불러오기, 설정, 제목 화면, 게임 종료
//   자동 저장: 하루가 시작될 때(자정), 습격 중이면 습격이 끝날 때, 그리고 제목으로 가거나 끝낼 때.
//   저장 자체(임시 파일 → 바꿔치기, .bak)와 습격 중 저장 금지는 C 가 지킵니다 (core_save).
// 자동 검사(스모크 테스트·스크린숏)는 제목 화면 없이 바로 게임을 시작합니다.
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
    /// <summary>제목 화면 뒤에 보여 줄 지도의 시드 (숲·물·바위가 고루 있는 지도)</summary>
    private const int TitleBackdropSeed = 42;

    /// <summary>성능 측정(--stress)의 마을 크기 (SPEC §5: 주민 150명 + 적 40명에서 60 fps)</summary>
    public const int StressVillagers = 150;
    public const int StressEnemies = 40;
    private const int StressSeed = 7;
    private double _perfTimer;

    private bool _reconcileQueued;
    private int _shownOutcome; // 이미 보여 준 결과 (같은 화면을 두 번 띄우지 않음)
    private bool _autosavePending; // 습격 중이라 미룬 자동 저장
    private int _speedBeforeMenu = 1;
    private int _lastSaveTick = -1;
    private int _lastSaveDay;
    private int _lastSaveMinute;
    private int _lastSaveSlot = -1;
    private float _titleTime;

    public MapView Map { get; private set; } = null!;
    public CameraRig Camera { get; private set; } = null!;
    public DayNight DayNight { get; private set; } = null!;
    public Hud Hud { get; private set; } = null!;
    public WorldSync Sync { get; private set; } = null!;
    public BuildMode Tools { get; private set; } = null!;
    public WeatherFx Weather { get; private set; } = null!;
    public CombatDirector Combat { get; private set; } = null!;
    public Sfx Sounds { get; private set; } = null!;
    public MenuLayer Menus { get; private set; } = null!;

    /// <summary>플레이어의 게임이 진행 중 (false = 제목 화면 뒤의 배경 월드)</summary>
    public bool InGame { get; private set; }

    /// <summary>자동 저장한 횟수, 습격 때문에 미룬 횟수 (스모크 테스트가 확인)</summary>
    public int AutosaveCount { get; private set; }
    public int AutosaveDeferred { get; private set; }

    /// <summary>마지막으로 저장하거나 불러온 뒤 시간이 흘렀는지</summary>
    public bool HasUnsavedProgress => InGame && Sync.Summary.Tick != _lastSaveTick;

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
        Tools.Map = Map;
        Sync.BuildingsLayer = GetNode<Node2D>("World/Buildings");
        Sync.UnitsLayer = GetNode<Node2D>("World/Units");
        Sync.Floating = GetNode<FloatingText>("World/FloatingText");
        Sounds = new Sfx { Name = "Sfx" };
        AddChild(Sounds);
        // 싸움은 일시정지되면 멈춰야 하므로 World(일시정지 가능) 아래에 둡니다
        Combat = new CombatDirector { Name = "Combat", Sync = Sync, UnitsLayer = Sync.UnitsLayer, Camera = Camera };
        GetNode<Node2D>("World").AddChild(Combat);
        Tools.Combat = Combat;
        // 메뉴 층은 마지막 자식이어야 처리되지 않은 입력을 가장 먼저 받습니다 (MenuLayer 설명)
        Menus = new MenuLayer { Name = "Menus" };
        AddChild(Menus);

        GameSettings.Load();
        ApplySettings();
        GetViewport().SizeChanged += ApplyUiScale;
        // 창을 닫을 때 자동 저장하고, 소리를 먼저 멈추고 잠깐 뒤에 끝냅니다 (소리가 나는 중에 끝나면 종료 경고가 남음)
        GetTree().AutoAcceptQuit = false;

        LaunchOptions options = LaunchOptions.Current;
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

        DayNight.Configure(core.Config.NightStartMinute, core.Config.NightEndMinute);
        GameClock.Instance.Ticked += OnTicked;
        core.CommandApplied += QueueReconcile;
        Sync.Notify += Hud.ShowNotice;
        Sync.EventPolled += Combat.OnCoreEvent;
        Sync.EventPolled += OnSessionEvent;
        Sync.Refreshed += OnRefreshed;
        Tools.SelectionChanged += Hud.Inspector.ShowSelection;
        Tools.Message += Hud.ShowToast;
        Tools.HintChanged += Hud.SetHint;
        Hud.Villagers.VillagerChosen += OnVillagerChosen;
        Hud.Toasts.JumpRequested += position => Camera.CenterOn(position);
        Hud.Outcome.NewGameRequested += () => Menus.NewGame.Open(Sync.Summary.Difficulty);
        Hud.Outcome.ContinueRequested += () => GameClock.Instance.SetSpeed(1);
        Hud.MenuRequested += OpenPauseMenu;
        WireMenus();
        // 단추 소리: 지금 있는 단추와 앞으로 생기는 단추(연구 창 등) 모두
        ConnectClicks(this);
        GetTree().NodeAdded += node => ConnectClicks(node, recursive: false);

        if (options.Automated || options.Stress)
        {
            SaveSlots.ClearAutomatedSaves();
            int seed = options.Seed ?? (options.Stress ? StressSeed : (int)(GD.Randi() & 0x7FFFFFFF));
            if (!StartNewGame(Strings.DefaultCastleName, seed, (int)options.Difficulty))
            {
                GetTree().Quit(2);
                return;
            }
            if (options.Stress)
            {
                StartStress();
            }
            if (options.SmokeTest)
            {
                AddChild(new SmokeTest());
            }
            else if (options.ScreenshotDir != null)
            {
                AddChild(new ScreenshotTour());
            }
            else if (options.StressProbe)
            {
                AddChild(new StressProbe());
            }
        }
        else
        {
            // 제목 화면 뒤의 배경: 정해진 시드의 지도를 만들어 멈춰 둡니다 (저장하지 않음)
            CoreResult backdrop = core.NewGame(Strings.DefaultCastleName, TitleBackdropSeed, Difficulty.Normal);
            if (!backdrop.Ok)
            {
                Hud.ShowFatal(Strings.NewGameFailed, backdrop.Name);
                return;
            }
            RebuildWorldView();
            ShowTitle();
        }
        // 정의 파일 경고(학생이 고친 .def 에 틀린 줄): 게임 중이면 알림, 제목 화면이면 HUD 가 숨어 있으므로 창으로
        if (InGame)
        {
            foreach (string warning in core.DataWarnings)
            {
                Hud.ShowToast(warning);
            }
        }
        else if (core.DataWarnings.Count > 0)
        {
            Menus.Confirm.Tell(string.Join("\n", core.DataWarnings));
        }
    }

    private void WireMenus()
    {
        Menus.InGameEscape = OnEscapeInGame;
        TitleScreen title = Menus.Title;
        title.ContinuePressed += ContinueLatest;
        title.NewGamePressed += () => Menus.NewGame.Open((int)LaunchOptions.Current.Difficulty);
        title.LoadPressed += () => Menus.Slots.Open(false, false);
        title.SettingsPressed += () => Menus.Settings.Open();
        title.QuitPressed += QuitGame;

        PauseMenu pause = Menus.Pause;
        pause.ResumePressed += ResumeFromPause;
        pause.SavePressed += () => Menus.Slots.Open(true, false);
        pause.LoadPressed += () => Menus.Slots.Open(false, HasUnsavedProgress);
        pause.SettingsPressed += () => Menus.Settings.Open();
        pause.HelpPressed += () => Menus.Help.Open();
        Menus.InGameHelp = () =>
        {
            OpenPauseMenu();
            if (Menus.Pause.Visible)
            {
                Menus.Help.Open();
            }
        };
        pause.TitlePressed += () => LeaveGame(ShowTitle);
        pause.QuitPressed += () => LeaveGame(QuitGame);

        Menus.NewGame.StartRequested += (name, seed, difficulty) => StartNewGame(name, seed, difficulty);
        Menus.NewGame.Cancelled += () =>
        {
            // 패배 화면에서 "새 게임"을 눌렀다가 그만두면 패배 화면을 다시 (진 월드는 시간이 흐르지 않음)
            if (InGame && !Menus.Pause.Visible && Sync.Summary.Outcome == (int)Outcome.Lost)
            {
                Hud.Outcome.ShowOutcome();
            }
        };
        Menus.Slots.SaveRequested += slot => SaveToSlot(slot);
        Menus.Slots.LoadRequested += (slot, backup) => LoadSlot(slot, backup);
        Menus.Settings.Changed += ApplySettings;
    }

    public override void _Notification(int what)
    {
        if (what == NotificationWMCloseRequest)
        {
            // 창의 X: 저장할 수 있으면 자동 저장하고 끝냅니다 (습격 중이면 그대로 끝냄 — 마지막 자동 저장이 남아 있음)
            TryAutosave();
            GameSettings.Current.Save();
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
        if (LaunchOptions.Current.Stress)
        {
            UpdatePerf(delta);
        }
        // 제목 화면: 지도 위를 천천히 흘러갑니다
        if (!InGame && Menus.Title.Visible && CoreBridge.Instance.HasWorld)
        {
            _titleTime += (float)delta;
            CoreSummaryView summary = Sync.Summary;
            Vector2 keep = Map.TileCenter(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize / 2);
            Camera.CenterOn(keep + new Vector2(Mathf.Sin(_titleTime * 0.05f) * 160f, Mathf.Cos(_titleTime * 0.035f) * 60f));
        }
    }

    // ---- 한 판의 흐름 -------------------------------------------------------

    /// <summary>제목 화면: 지금 월드를 멈춘 배경으로 두고 메뉴를 띄웁니다</summary>
    public void ShowTitle()
    {
        InGame = false;
        _autosavePending = false;
        Hud.Visible = false;
        Tools.Cancel();
        Tools.Select(0, 0);
        Camera.InputEnabled = false;
        Camera.SetZoomLevel(2);
        GameClock.Instance.SetSpeed(0);
        Menus.CloseAll();
        int latest = SaveSlots.Latest(out SlotInfo info);
        Menus.Title.Open(latest, info);
    }

    /// <summary>게임으로 들어갑니다 (새 게임·불러오기 뒤)</summary>
    public void EnterGame()
    {
        Menus.CloseAll();
        InGame = true;
        _autosavePending = false;
        Hud.Visible = true;
        Hud.Tutorial.Reset();
        Camera.InputEnabled = true;
        GameClock.Instance.SetSpeed(1);
    }

    public bool StartNewGame(string castleName, int seed, int difficulty)
    {
        CoreResult result = CoreBridge.Instance.NewGame(castleName, seed, (Difficulty)difficulty);
        if (!result.Ok)
        {
            Menus.Confirm.Tell($"{Strings.NewGameFailed}: {result.Message}");
            return false;
        }
        _shownOutcome = (int)Outcome.None;
        _lastSaveTick = -1;
        _lastSaveSlot = -1;
        EnterGame();
        RebuildWorldView();
        // 새 게임도 바로 한 번 자동 저장해 두면 "이어하기"가 늘 이번 판을 가리킵니다
        TryAutosave();
        return true;
    }

    /// <summary>이어하기: 가장 최근 세이브 (망가졌으면 예비 파일을 물음)</summary>
    public void ContinueLatest()
    {
        int slot = SaveSlots.Latest(out SlotInfo info);
        if (slot < 0)
        {
            return;
        }
        if (info.Ok)
        {
            LoadSlot(slot, false);
        }
        else
        {
            Menus.Confirm.Ask(Strings.ConfirmBackup(info), () => LoadSlot(slot, true));
        }
    }

    public bool LoadSlot(int slot, bool backup)
    {
        CoreResult result = SaveSlots.Load(slot, backup, out int line);
        if (!result.Ok)
        {
            // 실패하면 지금 월드는 그대로 (C 가 반쯤 읽은 것을 모두 해제함)
            Menus.Confirm.Tell(Strings.LoadFailed(result, line));
            return false;
        }
        CoreBridge.Instance.TryGetSummary(out CoreSummaryView summary);
        // 이긴 세이브는 승리 화면을 다시 띄우지 않고, 진 세이브는 패배 화면을 다시 띄웁니다 (새 게임을 고를 수 있게)
        _shownOutcome = summary.Outcome == (int)Outcome.Won ? (int)Outcome.Won : (int)Outcome.None;
        RecordSave(slot, summary);
        EnterGame();
        RebuildWorldView();
        Hud.ShowToast(Strings.Loaded(slot, backup));
        return true;
    }

    public CoreResult SaveToSlot(int slot)
    {
        CoreResult result = SaveSlots.Save(slot);
        if (result.Ok)
        {
            RecordSave(slot, Sync.Summary);
            Hud.ShowToast(Strings.Saved(slot));
        }
        else
        {
            Hud.ShowToast(result.Code == CoreResultCode.Rejected ? Strings.SaveBlockedRaid : Strings.SaveFailed(result));
        }
        return result;
    }

    /// <summary>
    /// 자동 저장을 해 봅니다. 습격 중이면 C 가 거절하므로(REJECTED) 습격이 끝날 때로 미룹니다.
    /// 진 게임과 제목 화면의 배경 월드는 저장하지 않습니다.
    /// </summary>
    public CoreResult TryAutosave()
    {
        // 성능 측정 마을(--stress)은 저장하지 않음: 학생의 자동 저장을 덮지 않게
        if (!InGame || !CoreBridge.Instance.HasWorld || Sync.Summary.Outcome == (int)Outcome.Lost || LaunchOptions.Current.Stress)
        {
            return new CoreResult(CoreResultCode.Ignored);
        }
        CoreResult result = SaveSlots.Save(SaveSlots.AutosaveSlot);
        if (result.Ok)
        {
            _autosavePending = false;
            AutosaveCount++;
            CoreBridge.Instance.TryGetSummary(out CoreSummaryView summary);
            RecordSave(SaveSlots.AutosaveSlot, summary);
            Hud.FlashSaved(Strings.Autosaved);
        }
        else if (result.Code == CoreResultCode.Rejected)
        {
            _autosavePending = true;
            AutosaveDeferred++;
        }
        else
        {
            Hud.ShowToast(Strings.SaveFailed(result));
        }
        return result;
    }

    private void RecordSave(int slot, in CoreSummaryView summary)
    {
        _lastSaveSlot = slot;
        _lastSaveTick = summary.Tick;
        _lastSaveDay = summary.Day;
        _lastSaveMinute = summary.MinuteOfDay;
    }

    /// <summary>하루가 시작될 때 자동 저장, 미룬 자동 저장은 습격이 끝날 때. 그리고 손맛 (소리·반짝임)</summary>
    private void OnSessionEvent(CoreEventView coreEvent)
    {
        if (!InGame)
        {
            return;
        }
        PlayFeedback(coreEvent);
        switch ((CoreEventType)coreEvent.Type)
        {
            case CoreEventType.DayStarted:
                TryAutosave();
                break;
            case CoreEventType.RaidEnded when _autosavePending:
                TryAutosave();
                break;
        }
    }

    /// <summary>큰 일에는 소리와 반짝임: 건물 완성(먼지), 승급(성채 위 금색), 연구 완료(마탑 위 보라색), 목표·승리</summary>
    private void PlayFeedback(in CoreEventView coreEvent)
    {
        Sfx? sfx = Sfx.Instance;
        Node2D layer = Sync.UnitsLayer ?? this;
        CoreSummaryView summary = Sync.Summary;
        Vector2 keep = Map.TileCenter(summary.KeepX + summary.KeepSize / 2, summary.KeepY);
        switch ((CoreEventType)coreEvent.Type)
        {
            case CoreEventType.BuildingCompleted when coreEvent.B != (int)BuildingType.Road && coreEvent.B != (int)BuildingType.Wall:
                sfx?.Play(Sound.Built);
                if (Sync.TryGetBuilding(coreEvent.A, out CoreBuildingView built))
                {
                    Effects.Dust(layer, new Vector2((built.X + built.W / 2f) * Art.Tile, (built.Y + built.H) * Art.Tile), built.W * Art.Tile);
                }
                break;
            case CoreEventType.RankUp:
            case CoreEventType.Victory:
                sfx?.Play(Sound.Fanfare);
                Effects.Sparkle(layer, keep, new Color(1f, 0.85f, 0.35f), 48);
                break;
            case CoreEventType.ResearchDone:
                sfx?.Play(Sound.Fanfare);
                Effects.Sparkle(layer, MageTowerTop() ?? keep, new Color(0.75f, 0.55f, 1f));
                break;
            case CoreEventType.TutorialDone:
                sfx?.Play(Sound.Fanfare);
                break;
            case CoreEventType.BuildingPlaced when coreEvent.B != (int)BuildingType.Road && coreEvent.B != (int)BuildingType.Wall:
                sfx?.Play(Sound.Place);
                break;
        }
    }

    private Vector2? MageTowerTop()
    {
        foreach (CoreBuildingView building in Sync.Buildings)
        {
            if (building.Type == (int)BuildingType.MageTower && building.State == (int)BuildingState.Active)
            {
                return new Vector2((building.X + building.W / 2f) * Art.Tile, building.Y * Art.Tile);
            }
        }
        return null;
    }

    /// <summary>단추를 누르면 딸깍 (UI 버스)</summary>
    private static void ConnectClicks(Node node, bool recursive = true)
    {
        if (node is BaseButton button)
        {
            button.Pressed += () => Sfx.Instance?.Play(Sound.Click);
        }
        if (!recursive)
        {
            return;
        }
        foreach (Node child in node.GetChildren())
        {
            ConnectClicks(child);
        }
    }

    public void OpenPauseMenu()
    {
        if (!InGame || Menus.AnyOpen)
        {
            return;
        }
        _speedBeforeMenu = GameClock.Instance.Speed;
        GameClock.Instance.SetSpeed(0);
        Tools.Cancel();
        Camera.InputEnabled = false;
        string lastSave = _lastSaveSlot >= 0 ? Strings.LastSave(_lastSaveDay, _lastSaveMinute, _lastSaveSlot) : Strings.NoSaveYet;
        string blocked = Sync.Summary.RaidId != 0 ? Strings.SaveBlockedRaid : "";
        Menus.Pause.Open(lastSave, blocked);
    }

    public void ResumeFromPause()
    {
        Menus.CloseAll();
        Camera.InputEnabled = true;
        bool decided = Sync.Summary.Outcome == (int)Outcome.Lost || Hud.Outcome.Visible;
        GameClock.Instance.SetSpeed(decided ? 0 : _speedBeforeMenu);
    }

    /// <summary>제목으로 가거나 끝내기 전에 자동 저장. 습격 중이라 저장할 수 없으면 물어봅니다.</summary>
    private void LeaveGame(Action then)
    {
        CoreResult saved = TryAutosave();
        if (saved.Code == CoreResultCode.Rejected)
        {
            Menus.Confirm.Ask(Strings.ConfirmQuitInRaid, then);
            return;
        }
        then();
    }

    public void QuitGame()
    {
        GameSettings.Current.Save();
        Sfx.QuitQuietly(GetTree(), 0);
    }

    /// <summary>게임 중 Esc: 열린 창 닫기 → 건설·철거 도구 취소 → 고른 것 풀기 → 일시정지 메뉴</summary>
    private bool OnEscapeInGame()
    {
        if (!InGame)
        {
            return false;
        }
        if (Hud.CloseOpenPanel())
        {
            return true;
        }
        if (Tools.Mode != ToolMode.Select)
        {
            Tools.Cancel();
            return true;
        }
        if (Tools.SelectedBuilding != 0 || Tools.SelectedVillager != 0)
        {
            Tools.Select(0, 0);
            return true;
        }
        OpenPauseMenu();
        return true;
    }

    // ---- 성능 측정 (--stress) -----------------------------------------------

    /// <summary>주민 150명 + 마물 40명의 큰 마을을 만들고 FPS 를 보여 줍니다</summary>
    private void StartStress()
    {
        // 측정(--stress-probe)은 마을만 먼저 만들고, 적은 측정 중에 StressProbe 가 부름 (나타나는 순간도 재려고)
        CoreResult result = CoreBridge.Instance.DebugStress(StressVillagers, LaunchOptions.Current.StressProbe ? 0 : StressEnemies);
        if (!result.Ok)
        {
            Hud.ShowToast($"{Strings.StressFailed}: {result.Name}");
            return;
        }
        Sync.Reconcile();
        Camera.SetZoomLevel(1);
        Hud.ShowPerf(true);
        GD.Print($"stress: {Sync.VillagerCount} villagers, {Sync.BuildingCount} buildings");
    }

    private void UpdatePerf(double delta)
    {
        _perfTimer += delta;
        if (_perfTimer < 0.5)
        {
            return;
        }
        _perfTimer = 0;
        double processMs = Performance.GetMonitor(Performance.Monitor.TimeProcess) * 1000.0;
        Hud.SetPerf(Strings.PerfLine((int)Engine.GetFramesPerSecond(), processMs, Sync.VillagerCount, Combat.EnemyCount));
    }

    // ---- 화면 맞추기 --------------------------------------------------------

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
    /// 이긴 뒤에는 "계속 다스리기"로 이어 갈 수 있습니다. 제목 화면의 배경 월드는 상관없습니다.
    /// </summary>
    private void CheckOutcome(in CoreSummaryView summary)
    {
        if (!InGame || summary.Outcome == (int)Outcome.None || summary.Outcome == _shownOutcome)
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

    private void OnVillagerChosen(int villagerId)
    {
        Tools.Select(villagerId, 0);
        if (Sync.AgentFor(villagerId) is VillagerAgent agent)
        {
            Camera.CenterOn(agent.Position);
        }
    }

    private void ApplySettings()
    {
        GameSettings settings = GameSettings.Current;
        settings.ApplyAudio();
        settings.ApplyWindow();
        Camera.EdgeScrollEnabled = settings.EdgeScroll && !LaunchOptions.Current.Automated;
        ApplyUiScale();
    }

    private void ApplyUiScale()
    {
        UiScale.Apply(GameSettings.Current.ResolveUiScale(DisplayServer.WindowGetSize()));
        Hud.ApplyScale(UiScale.Current);
        Menus.ApplyScale(UiScale.Current);
    }
}
