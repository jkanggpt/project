// Hud.cs — 위쪽 막대: 성채 이름, 날짜, 시각, 배속 버튼, 자원 / 알림 / 시작 실패 화면
//
// 숫자는 모두 C 코어가 준 요약(CoreSummaryView)에서 읽기만 합니다. HUD 는 게임 상태를 바꾸지 않습니다.
using System;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.UI;

public partial class Hud : CanvasLayer
{
    private Control _topBar = null!;
    private Label _castleName = null!;
    private Label _date = null!;
    private Label _clock = null!;
    private Control _paused = null!;
    private HBoxContainer _resources = null!;
    private Button[] _speedButtons = Array.Empty<Button>();
    private Label[] _resourceLabels = Array.Empty<Label>();
    private Toasts _toasts = null!;
    private Control _fatal = null!;
    private Label _fatalMessage = null!;
    private Label _fatalDetail = null!;

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        _topBar = GetNode<Control>("Root/TopBar");
        _castleName = GetNode<Label>("Root/TopBar/Row/Left/CastleName");
        _date = GetNode<Label>("Root/TopBar/Row/Left/Date");
        _clock = GetNode<Label>("Root/TopBar/Row/Left/Clock");
        _paused = GetNode<Control>("Root/Paused");
        _resources = GetNode<HBoxContainer>("Root/TopBar/Row/Resources");
        _toasts = GetNode<Toasts>("Root/Toasts");
        _fatal = GetNode<Control>("Root/Fatal");
        _fatalMessage = GetNode<Label>("Root/Fatal/Box/Message");
        _fatalDetail = GetNode<Label>("Root/Fatal/Box/Detail");
        GetNode<Label>("Root/Fatal/Box/Title").Text = Strings.FatalTitle;

        BuildSpeedButtons(GetNode<HBoxContainer>("Root/TopBar/Row/Left/Speed"));
        BuildResourceLabels();
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
        else
        {
            return;
        }
        GetViewport().SetInputAsHandled();
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

    private void BuildResourceLabels()
    {
        _resourceLabels = new Label[CoreSummaryView.ResourceCount];
        for (int i = 0; i < _resourceLabels.Length; i++)
        {
            var label = new Label { MouseFilter = Control.MouseFilterEnum.Pass };
            _resources.AddChild(label);
            _resourceLabels[i] = label;
        }
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
        _castleName.Text = summary.GetCastleName();
        _date.Text = Strings.Date(summary);
        _date.TooltipText = Strings.DateTooltip(summary, config);
        _clock.Text = Strings.Clock(summary.MinuteOfDay);
        _clock.TooltipText = Strings.ClockTooltip(config);
        for (int i = 0; i < _resourceLabels.Length; i++)
        {
            var kind = (ResourceKind)i;
            int amount = summary.GetResource(kind);
            int capacity = summary.GetCapacity(kind);
            _resourceLabels[i].Text = $"{Strings.ResourceName(kind)} {amount}";
            _resourceLabels[i].TooltipText = Strings.ResourceTooltip(kind, amount, capacity);
        }
    }

    public void ShowToast(string text) => _toasts.Show(text);

    public override void _Process(double delta)
    {
        // 위쪽 막대가 두 줄로 접히면 알림을 그만큼 아래에 둡니다
        float top = _topBar.Visible ? _topBar.Size.Y : 0;
        _toasts.OffsetTop = top + 4 * UiScale.Current;
        _paused.OffsetTop = top + 4 * UiScale.Current;
    }

    /// <summary>UI 배율이 바뀌면 픽셀 단위로 정한 크기도 함께 바꿉니다.</summary>
    public void ApplyScale(int scale)
    {
        _resources.AddThemeConstantOverride("separation", 8 * scale);
        _toasts.CustomMinimumSize = new Vector2(200 * scale, 0);
        _paused.OffsetTop = 24 * scale;
        _toasts.OffsetRight = -4 * scale;
        _fatalMessage.CustomMinimumSize = new Vector2(320 * scale, 0);
    }

    public void ShowFatal(string message, string detail)
    {
        _topBar.Visible = false;
        _fatal.Visible = true;
        _fatalMessage.Text = message;
        _fatalDetail.Text = detail;
    }
}
