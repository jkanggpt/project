// Hud.cs — 화면 위의 UI 를 한데 모읍니다.
//   위쪽 막대: 성채 이름, 날짜, 시각, 배속 단추, 날씨·자연 사건·겨울 준비, 상인·시장 단추,
//              자원(아이콘 + 하루 변화, 툴팁에 장부 내역), 주민 수
//   아래쪽: 건설 막대 / 왼쪽 아래: 검사창 / 가운데: 주민 목록 / 오른쪽 위: 알림
//   마우스 옆: 건설·철거 안내 문구
//
// 숫자는 모두 C 코어가 준 요약(CoreSummaryView)에서 읽기만 합니다. HUD 는 게임 상태를 바꾸지 않습니다
// (단추는 CoreBridge 명령이나 BuildMode 를 부를 뿐입니다).
using System;
using Godot;
using MagicCastle.Core;
using MagicCastle.World;

namespace MagicCastle.UI;

public partial class Hud : CanvasLayer
{
    private Control _root = null!;
    private Control _topBar = null!;
    private Label _castleName = null!;
    private Label _date = null!;
    private Label _clock = null!;
    private Control _paused = null!;
    private HBoxContainer _resources = null!;
    private Button[] _speedButtons = Array.Empty<Button>();
    private Label[] _resourceLabels = Array.Empty<Label>();
    private TextureRect[] _resourceIcons = Array.Empty<TextureRect>();
    private Label _population = null!;
    private Label _weather = null!;
    private Label _nature = null!;
    private Label _winter = null!;
    private Label _threat = null!;
    private PanelContainer _raidBanner = null!;
    private Label _raidLabel = null!;
    private double _bannerTime;
    private Button _merchantButton = null!;
    private Button _marketButton = null!;
    private Button _rankButton = null!;
    private Button _researchButton = null!;
    private PanelContainer _hint = null!;
    private Label _hintLabel = null!;
    private Label _saved = null!;
    private Label _perf = null!;
    private Tween? _savedTween;
    private Control _fatal = null!;
    private Label _fatalMessage = null!;
    private Label _fatalDetail = null!;

    public Toasts Toasts { get; private set; } = null!;
    public BuildBar Bar { get; private set; } = null!;
    public Inspector Inspector { get; private set; } = null!;
    public VillagerList Villagers { get; private set; } = null!;
    public MarketPanel Market { get; private set; } = null!;
    public MerchantPanel Merchant { get; private set; } = null!;
    public ResearchPanel Research { get; private set; } = null!;
    public RankPanel Ranks { get; private set; } = null!;
    public TutorialPanel Tutorial { get; private set; } = null!;
    public OutcomePanel Outcome { get; private set; } = null!;

    /// <summary>위쪽 막대의 "메뉴" 단추 (일시정지 메뉴)</summary>
    [Signal]
    public delegate void MenuRequestedEventHandler();

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        _root = GetNode<Control>("Root");
        _topBar = GetNode<Control>("Root/TopBar");
        _castleName = GetNode<Label>("Root/TopBar/Row/Left/CastleName");
        _date = GetNode<Label>("Root/TopBar/Row/Left/Date");
        _clock = GetNode<Label>("Root/TopBar/Row/Left/Clock");
        _paused = GetNode<Control>("Root/Paused");
        _resources = GetNode<HBoxContainer>("Root/TopBar/Row/Resources");
        Toasts = GetNode<Toasts>("Root/Toasts");
        _fatal = GetNode<Control>("Root/Fatal");
        _fatalMessage = GetNode<Label>("Root/Fatal/Box/Message");
        _fatalDetail = GetNode<Label>("Root/Fatal/Box/Detail");
        GetNode<Label>("Root/Fatal/Box/Title").Text = Strings.FatalTitle;

        BuildSpeedButtons(GetNode<HBoxContainer>("Root/TopBar/Row/Left/Speed"));
        BuildMenuButton(GetNode<HBoxContainer>("Root/TopBar/Row/Left"));
        BuildStatus(GetNode<Control>("Root/TopBar/Row"));
        BuildResourceLabels();
        BuildPanels();
        BuildRaidBanner();
        GetNode<Label>("Root/Paused/Label").Text = Strings.PausedBanner;
        _paused.Visible = false;
        _topBar.Visible = false;

        GameClock.Instance.SpeedChanged += OnSpeedChanged;
        CoreBridge.Instance.CoreError += ShowToast;
    }

    public override void _ExitTree()
    {
        GameClock.Instance.SpeedChanged -= OnSpeedChanged;
        CoreBridge.Instance.CoreError -= ShowToast;
    }

    public override void _UnhandledInput(InputEvent inputEvent)
    {
        if (!CoreBridge.Instance.HasWorld)
        {
            return;
        }
        if (inputEvent.IsActionPressed("speed_pause"))
        {
            GameClock.Instance.TogglePause();
        }
        else if (inputEvent.IsActionPressed("speed_1"))
        {
            GameClock.Instance.SetSpeed(1);
        }
        else if (inputEvent.IsActionPressed("speed_2"))
        {
            GameClock.Instance.SetSpeed(2);
        }
        else if (inputEvent.IsActionPressed("speed_3"))
        {
            GameClock.Instance.SetSpeed(3);
        }
        else if (inputEvent.IsActionPressed("toggle_villagers"))
        {
            Villagers.Toggle();
        }
        else
        {
            return;
        }
        GetViewport().SetInputAsHandled();
    }

    /// <summary>
    /// 맨 왼쪽의 "메뉴" 단추와, 오른쪽 아래(건설 막대 위)에 잠깐 떴다 사라지는 "자동 저장" 표시.
    /// 표시는 위쪽 막대 밖에 띄웁니다 (막대 안에 두면 1280×720 에서 막대가 한 줄 더 접힘).
    /// </summary>
    private void BuildMenuButton(HBoxContainer left)
    {
        var menu = new Button { Text = Strings.MenuButton, TooltipText = Strings.MenuButtonTooltip, FocusMode = Control.FocusModeEnum.None };
        menu.Pressed += () => EmitSignal(SignalName.MenuRequested);
        left.AddChild(menu);
        left.MoveChild(menu, 0);
        _saved = new Label { Text = Strings.Autosaved, Modulate = new Color(0.7f, 1f, 0.7f, 0f), MouseFilter = Control.MouseFilterEnum.Ignore };
        _saved.AddThemeColorOverride("font_shadow_color", new Color(0, 0, 0, 0.8f));
        _root.AddChild(_saved);
        _saved.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.BottomRight);
        _saved.GrowHorizontal = Control.GrowDirection.Begin;
        _saved.GrowVertical = Control.GrowDirection.Begin;
        _perf = new Label { Visible = false, MouseFilter = Control.MouseFilterEnum.Ignore };
        _perf.AddThemeColorOverride("font_color", new Color(1f, 1f, 0.6f));
        _perf.AddThemeColorOverride("font_shadow_color", new Color(0, 0, 0, 0.9f));
        _root.AddChild(_perf);
        _perf.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.BottomRight);
        _perf.GrowHorizontal = Control.GrowDirection.Begin;
        _perf.GrowVertical = Control.GrowDirection.Begin;
    }

    /// <summary>성능 측정(--stress) 중 오른쪽 아래의 FPS 줄</summary>
    public void ShowPerf(bool visible) => _perf.Visible = visible;

    public void SetPerf(string text) => _perf.Text = text;

    /// <summary>저장했다는 표시를 잠깐 보여 줍니다 (자동 저장마다 알림을 쌓지 않으려고)</summary>
    public void FlashSaved(string text)
    {
        _saved.Text = text;
        _savedTween?.Kill();
        _saved.Modulate = new Color(0.7f, 1f, 0.7f, 1f);
        _savedTween = CreateTween();
        _savedTween.SetPauseMode(Tween.TweenPauseMode.Process);
        _savedTween.TweenInterval(2.0);
        _savedTween.TweenProperty(_saved, "modulate:a", 0.0f, 1.0);
    }

    /// <summary>Esc: 열려 있는 창(주민 목록·시장·상인·연구·등급) 하나를 닫습니다. 닫았으면 true</summary>
    public bool CloseOpenPanel()
    {
        foreach (Control panel in new Control[] { Research, Ranks, Market, Merchant, Villagers })
        {
            if (panel.Visible)
            {
                panel.Visible = false;
                return true;
            }
        }
        return false;
    }

    private void BuildSpeedButtons(HBoxContainer container)
    {
        int maxSpeed = Math.Max(1, CoreBridge.Instance.Config.MaxSpeed);
        var group = new ButtonGroup();
        _speedButtons = new Button[maxSpeed + 1];
        for (int speed = 0; speed <= maxSpeed; speed++)
        {
            int captured = speed;
            var button = new Button
            {
                Text = speed == 0 ? Strings.SpeedPause : Strings.SpeedLabel(speed),
                TooltipText = speed == 0 ? Strings.SpeedPauseTooltip : Strings.SpeedTooltip(speed),
                ToggleMode = true,
                ButtonGroup = group,
                FocusMode = Control.FocusModeEnum.None, // 스페이스바가 버튼을 누르지 않게
            };
            button.Pressed += () => GameClock.Instance.SetSpeed(captured);
            container.AddChild(button);
            _speedButtons[speed] = button;
        }
        OnSpeedChanged(GameClock.Instance.Speed);
    }

    /// <summary>날씨, 자연 사건, 겨울 준비, 상인·시장 단추 (날짜 오른쪽)</summary>
    private void BuildStatus(Control row)
    {
        var status = new HBoxContainer { Name = "Status" };
        row.AddChild(status);
        row.MoveChild(status, 1);
        status.AddChild(new VSeparator());
        _weather = new Label { MouseFilter = Control.MouseFilterEnum.Stop };
        _nature = new Label { MouseFilter = Control.MouseFilterEnum.Stop };
        _winter = new Label { MouseFilter = Control.MouseFilterEnum.Stop };
        status.AddChild(_weather);
        status.AddChild(_nature);
        status.AddChild(_winter);
        _threat = new Label { MouseFilter = Control.MouseFilterEnum.Stop };
        status.AddChild(_threat);
        _merchantButton = new Button { FocusMode = Control.FocusModeEnum.None, TooltipText = Strings.MerchantButtonTooltip, Visible = false };
        _merchantButton.Pressed += () => Merchant.Toggle();
        _marketButton = new Button { Text = Strings.MarketButton, FocusMode = Control.FocusModeEnum.None, TooltipText = Strings.MarketButtonTooltip, Visible = false };
        _marketButton.Pressed += () => Market.Toggle();
        status.AddChild(_merchantButton);
        status.AddChild(_marketButton);
        // 등급과 연구 (날짜 바로 오른쪽: 늘 보임)
        _rankButton = new Button { FocusMode = Control.FocusModeEnum.None, TooltipText = Strings.RankButtonTooltip };
        _rankButton.Pressed += () => Ranks.Toggle();
        _researchButton = new Button { Text = Strings.ResearchButton, FocusMode = Control.FocusModeEnum.None, TooltipText = Strings.ResearchButtonTooltip };
        _researchButton.Pressed += () => Research.Toggle();
        status.AddChild(_rankButton);
        status.MoveChild(_rankButton, 1);
        status.AddChild(_researchButton);
        status.MoveChild(_researchButton, 2);
    }

    /// <summary>습격 중 화면 위 가운데의 붉은 띠 (남은 적 수)</summary>
    private void BuildRaidBanner()
    {
        var style = new StyleBoxFlat
        {
            BgColor = new Color(0.45f, 0.06f, 0.06f, 0.92f),
            BorderColor = new Color(1f, 0.55f, 0.4f),
        };
        style.SetBorderWidthAll(1);
        style.SetContentMarginAll(4);
        _raidBanner = new PanelContainer { Name = "RaidBanner", Visible = false, MouseFilter = Control.MouseFilterEnum.Stop };
        _raidBanner.AddThemeStyleboxOverride("panel", style);
        _raidBanner.SetAnchorsPreset(Control.LayoutPreset.CenterTop);
        _raidBanner.GrowHorizontal = Control.GrowDirection.Both;
        _raidBanner.OffsetLeft = 0;
        _raidBanner.OffsetRight = 0;
        _raidLabel = new Label { HorizontalAlignment = HorizontalAlignment.Center };
        _raidLabel.AddThemeColorOverride("font_color", new Color(1f, 0.92f, 0.85f));
        _raidBanner.AddChild(_raidLabel);
        _raidBanner.TooltipText = Strings.RaidBannerTooltip;
        _root.AddChild(_raidBanner);
    }

    private void BuildResourceLabels()
    {
        _resourceLabels = new Label[CoreSummaryView.ResourceCount];
        _resourceIcons = new TextureRect[CoreSummaryView.ResourceCount];
        for (int i = 0; i < _resourceLabels.Length; i++)
        {
            // 아이콘과 숫자를 한 묶음으로: 툴팁은 묶음 전체에 겁니다
            var group = new HBoxContainer { MouseFilter = Control.MouseFilterEnum.Stop };
            var icon = new TextureRect
            {
                Texture = Art.ResourceIcon((ResourceKind)i),
                ExpandMode = TextureRect.ExpandModeEnum.IgnoreSize,
                StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
                TextureFilter = CanvasItem.TextureFilterEnum.Nearest,
                MouseFilter = Control.MouseFilterEnum.Ignore,
            };
            var label = new Label { MouseFilter = Control.MouseFilterEnum.Ignore };
            group.AddChild(icon);
            group.AddChild(label);
            _resources.AddChild(group);
            _resourceLabels[i] = label;
            _resourceIcons[i] = icon;
        }
        _resources.AddChild(new VSeparator());
        _population = new Label { MouseFilter = Control.MouseFilterEnum.Stop };
        _resources.AddChild(_population);
    }

    private void BuildPanels()
    {
        // 튜토리얼 목록은 왼쪽 위에 늘 떠 있으므로, 가운데 창들이 그 위에 그려지도록 먼저 넣습니다
        Tutorial = new TutorialPanel { Name = "Tutorial", Visible = false };
        _root.AddChild(Tutorial);
        Tutorial.SetAnchorsPreset(Control.LayoutPreset.TopLeft);

        Bar = new BuildBar { Name = "BuildBar", Visible = false };
        _root.AddChild(Bar);
        Bar.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.BottomWide, Control.LayoutPresetMode.KeepHeight);
        Bar.GrowVertical = Control.GrowDirection.Begin;
        Bar.VillagerListRequested += () => Villagers.Toggle();

        Inspector = new Inspector { Name = "Inspector" };
        _root.AddChild(Inspector);
        Inspector.SetAnchorsPreset(Control.LayoutPreset.BottomLeft);
        Inspector.GrowVertical = Control.GrowDirection.Begin;

        Villagers = new VillagerList { Name = "VillagerList" };
        _root.AddChild(Villagers);
        CenterPanel(Villagers);

        Market = new MarketPanel { Name = "Market" };
        _root.AddChild(Market);
        CenterPanel(Market);
        Market.Message += ShowToast;
        Merchant = new MerchantPanel { Name = "Merchant" };
        _root.AddChild(Merchant);
        CenterPanel(Merchant);
        Merchant.Message += ShowToast;
        Inspector.TradeRequested += () => Market.Open();
        Inspector.Message += ShowToast;

        _hint = new PanelContainer { Name = "Hint", Visible = false, MouseFilter = Control.MouseFilterEnum.Ignore };
        _hintLabel = new Label { MouseFilter = Control.MouseFilterEnum.Ignore };
        _hint.AddChild(_hintLabel);
        _root.AddChild(_hint);
        _hint.Modulate = new Color(1, 1, 1, 0.92f);

        Research = new ResearchPanel { Name = "Research" };
        _root.AddChild(Research);
        CenterPanel(Research);
        Research.Message += ShowToast;
        Ranks = new RankPanel { Name = "Ranks" };
        _root.AddChild(Ranks);
        CenterPanel(Ranks);
        Ranks.Message += ShowToast;
        Outcome = new OutcomePanel { Name = "Outcome" };
        _root.AddChild(Outcome);
        CenterPanel(Outcome);

        // 패널이 위쪽 막대(툴팁 포함)보다 아래에 그려지도록 순서를 맞춥니다
        _root.MoveChild(_fatal, _root.GetChildCount() - 1);
    }

    /// <summary>가운데 한 점(크기 0)에 두고 최소 크기만큼 양쪽으로 자라게 → 언제나 화면 가운데</summary>
    private static void CenterPanel(Control panel)
    {
        panel.SetAnchorsPreset(Control.LayoutPreset.Center);
        panel.OffsetLeft = panel.OffsetRight = panel.OffsetTop = panel.OffsetBottom = 0;
        panel.GrowHorizontal = Control.GrowDirection.Both;
        panel.GrowVertical = Control.GrowDirection.Both;
    }

    private void OnSpeedChanged(int speed)
    {
        // SetPressedNoSignal 은 같은 ButtonGroup 의 다른 버튼을 풀어 주지 않으므로 모두 직접 맞춥니다
        for (int i = 0; i < _speedButtons.Length; i++)
        {
            _speedButtons[i].SetPressedNoSignal(i == speed);
        }
        _paused.Visible = speed == 0;
    }

    public void UpdateSummary(in CoreSummaryView summary)
    {
        CoreConfigView config = CoreBridge.Instance.Config;
        _topBar.Visible = true;
        Bar.Visible = true;
        _castleName.Text = summary.GetCastleName();
        _date.Text = Strings.Date(summary);
        _date.TooltipText = Strings.DateTooltip(summary, config);
        _clock.Text = Strings.Clock(summary.MinuteOfDay);
        _clock.TooltipText = Strings.ClockTooltip(config);
        bool hasLedger = CoreBridge.Instance.TryGetLedger(out CoreLedgerView ledger);
        for (int i = 0; i < _resourceLabels.Length; i++)
        {
            var kind = (ResourceKind)i;
            int amount = summary.GetResource(kind);
            int capacity = summary.GetCapacity(kind);
            int delta = summary.GetDelta(kind);
            Label label = _resourceLabels[i];
            label.Text = (_resourceIcons[i].Texture == null ? Strings.ResourceName(kind) + " " : "") + Strings.ResourceLabel(amount, delta);
            // 창고가 가득 차면 노랗게, 하루 변화가 마이너스면 붉게
            Color color = capacity > 0 && amount >= capacity ? new Color(1f, 0.85f, 0.4f)
                : delta < 0 ? new Color(1f, 0.6f, 0.55f)
                : Colors.White;
            label.AddThemeColorOverride("font_color", color);
            ((Control)label.GetParent()).TooltipText = hasLedger
                ? Strings.ResourceTooltip(kind, amount, capacity, delta, ledger)
                : Strings.ResourceTooltip(kind, amount, capacity, delta);
        }
        _population.Text = Strings.Population(summary);
        _population.TooltipText = Strings.PopulationTooltip(summary);
        UpdateStatus(summary);
        Bar.Refresh(summary);
    }

    private void UpdateStatus(in CoreSummaryView summary)
    {
        _weather.Text = Strings.WeatherLabel(summary);
        _weather.TooltipText = Strings.WeatherTooltip(summary);
        string nature = Strings.NatureLabel(summary);
        _nature.Visible = nature.Length > 0;
        _nature.Text = nature;
        _nature.TooltipText = nature.Length > 0 ? Strings.NatureTooltip(summary) : "";
        _nature.AddThemeColorOverride("font_color", summary.NatureEvent == (int)NatureEvent.Bumper
            ? new Color(0.7f, 1f, 0.6f)
            : new Color(1f, 0.75f, 0.45f));
        string winter = Strings.WinterLabel(summary);
        _winter.Visible = winter.Length > 0;
        _winter.Text = winter;
        _winter.TooltipText = winter.Length > 0 ? Strings.WinterTooltip(summary) : "";
        _winter.AddThemeColorOverride("font_color", Strings.WinterShort(summary) ? new Color(1f, 0.55f, 0.5f) : new Color(0.75f, 0.9f, 1f));
        UpdateThreat(summary);
        _merchantButton.Visible = summary.MerchantDaysLeft > 0;
        _merchantButton.Text = Strings.MerchantButton(summary.MerchantDaysLeft);
        _marketButton.Visible = HasMarket();
        Market.Refresh();
        Merchant.Refresh();
        _rankButton.Text = Strings.RankLabel(summary.Rank, summary.RankReady != 0);
        _rankButton.Modulate = summary.RankReady != 0 ? new Color(1f, 0.92f, 0.5f) : Colors.White;
        _researchButton.Text = summary.ResearchActive >= 0 && CoreBridge.Instance.TryGetResearch(summary.ResearchActive, out CoreResearchView active)
            ? Strings.ResearchShort(active.GetKey(), summary.ResearchProgress, summary.ResearchPoints)
            : Strings.ResearchButton;
        Research.Refresh();
        Ranks.Refresh();
        Tutorial.Refresh(summary);
    }

    private void UpdateThreat(in CoreSummaryView summary)
    {
        bool raid = summary.RaidId != 0;
        _raidBanner.Visible = raid;
        if (raid)
        {
            int total = CoreBridge.Instance.TryGetRaid(out CoreRaidView view) ? view.EnemyCount : summary.RaidEnemiesLeft;
            _raidLabel.Text = Strings.RaidBanner(summary.RaidType, summary.RaidEnemiesLeft, total, summary.Guards);
        }
        _threat.Visible = !raid;
        if (summary.ThreatType == (int)ThreatType.None)
        {
            _threat.Text = Strings.NoThreat;
            _threat.TooltipText = Strings.NoThreatTooltip;
            _threat.AddThemeColorOverride("font_color", new Color(0.8f, 0.8f, 0.8f));
            return;
        }
        _threat.Text = Strings.ThreatLabel(summary.ThreatType, summary.ThreatDirection, summary.ThreatTicksLeft);
        _threat.TooltipText = Strings.ThreatTooltip(summary.ThreatType, summary.ThreatDirection, summary.ThreatTicksLeft, summary.ThreatStrength);
        // 반나절 안이면 붉게, 하루 안이면 주황
        int ticksPerDay = CoreBridge.Instance.Config.TicksPerDay;
        Color color = summary.ThreatTicksLeft <= ticksPerDay / 2 ? new Color(1f, 0.45f, 0.4f)
            : summary.ThreatTicksLeft <= ticksPerDay ? new Color(1f, 0.75f, 0.45f)
            : Colors.White;
        _threat.AddThemeColorOverride("font_color", color);
    }

    private static bool HasMarket()
    {
        WorldSync? sync = WorldSync.Instance;
        if (sync == null)
        {
            return false;
        }
        foreach (CoreBuildingView building in sync.Buildings)
        {
            if (building.Type == (int)BuildingType.Market && building.State == (int)BuildingState.Active)
            {
                return true;
            }
        }
        return false;
    }

    public void ShowToast(string text) => Toasts.Show(text);

    /// <summary>월드 위치가 있는 알림 (누르면 카메라가 그곳으로)</summary>
    public void ShowNotice(string text, Vector2 worldPosition, bool hasPosition) =>
        Toasts.Show(text, hasPosition ? worldPosition : null);

    /// <summary>마우스 옆 안내 문구 (빈 문자열이면 숨김)</summary>
    public void SetHint(string text)
    {
        _hint.Visible = text.Length > 0;
        _hintLabel.Text = text;
    }

    public override void _Process(double delta)
    {
        int scale = UiScale.Current;
        // 위쪽 막대가 두 줄로 접히면 알림을 그만큼 아래에 두고, 검사창은 건설 막대 바로 위에 둡니다
        float top = _topBar.Visible ? _topBar.Size.Y : 0;
        float bottom = Bar.Visible ? Bar.Size.Y : 0;
        Toasts.OffsetTop = top + 4 * scale;
        _raidBanner.OffsetTop = top + 4 * scale;
        Tutorial.OffsetLeft = 4 * scale;
        Tutorial.OffsetTop = top + 4 * scale;
        _raidBanner.OffsetBottom = _raidBanner.OffsetTop;
        _paused.OffsetTop = top + 4 * scale + (_raidBanner.Visible ? _raidBanner.Size.Y + 2 * scale : 0);
        if (_raidBanner.Visible)
        {
            // 습격 띠는 천천히 깜빡여 눈에 띄게 함 (일시정지 중에도)
            _bannerTime += delta;
            _raidBanner.Modulate = new Color(1, 1, 1, 0.9f + 0.1f * (float)Math.Sin(_bannerTime * 4));
        }
        _saved.OffsetLeft = _saved.OffsetRight = -6 * scale;
        _saved.OffsetTop = _saved.OffsetBottom = -bottom - 4 * scale;
        _perf.OffsetLeft = _perf.OffsetRight = -6 * scale;
        _perf.OffsetTop = _perf.OffsetBottom = -bottom - 18 * scale;
        Vector2 inspectorSize = Inspector.GetCombinedMinimumSize();
        Inspector.OffsetLeft = 4 * scale;
        Inspector.OffsetRight = Inspector.OffsetLeft + inspectorSize.X;
        Inspector.OffsetBottom = -bottom - 4 * scale;
        Inspector.OffsetTop = Inspector.OffsetBottom - inspectorSize.Y;

        if (_hint.Visible)
        {
            // 마우스 오른쪽 아래에 두되 화면 밖으로 나가지 않게
            Vector2 mouse = _root.GetLocalMousePosition();
            Vector2 size = _hint.GetCombinedMinimumSize();
            _hint.Size = size;
            Vector2 position = mouse + new Vector2(12, 12) * scale;
            Vector2 screen = _root.Size;
            position.X = Math.Min(position.X, screen.X - size.X - 2);
            position.Y = Math.Min(position.Y, screen.Y - bottom - size.Y - 2);
            _hint.Position = position;
        }
    }

    /// <summary>UI 배율이 바뀌면 픽셀 단위로 정한 크기도 함께 바꿉니다.</summary>
    public void ApplyScale(int scale)
    {
        _resources.AddThemeConstantOverride("separation", 6 * scale);
        foreach (TextureRect icon in _resourceIcons)
        {
            icon.CustomMinimumSize = new Vector2(16 * scale, 16 * scale); // 16px 그림을 정수배로 (흐려지지 않게)
        }
        Toasts.CustomMinimumSize = new Vector2(200 * scale, 0);
        _paused.OffsetTop = 24 * scale;
        Toasts.OffsetRight = -4 * scale;
        Toasts.OffsetLeft = -204 * scale;
        _fatalMessage.CustomMinimumSize = new Vector2(320 * scale, 0);
        _hintLabel.CustomMinimumSize = new Vector2(0, 0);
        Bar.ApplyScale(scale);
        Inspector.ApplyScale(scale);
        Villagers.ApplyScale(scale);
        Market.ApplyScale(scale);
        Merchant.ApplyScale(scale);
        Research.ApplyScale(scale);
        Ranks.ApplyScale(scale);
        Tutorial.ApplyScale(scale);
        Outcome.ApplyScale(scale);
    }

    public void ShowFatal(string message, string detail)
    {
        _topBar.Visible = false;
        Bar.Visible = false;
        _fatal.Visible = true;
        _fatalMessage.Text = message;
        _fatalDetail.Text = detail;
    }
}
