// Toasts.cs — 화면 오른쪽 위의 알림 목록 (월드 위치가 있는 알림은 누르면 카메라가 그곳으로 갑니다)
// 알림 노드는 미리 몇 개 만들어 두고 돌려 씁니다 (매번 만들고 지우지 않음).
using System.Collections.Generic;
using Godot;

namespace MagicCastle.UI;

public partial class Toasts : VBoxContainer
{
    private const int PoolSize = 6;
    private const double ShowSeconds = 5.0;
    private const double FadeSeconds = 0.6;

    private readonly List<PanelContainer> _pool = new();
    private readonly Dictionary<PanelContainer, Tween> _tweens = new();
    private readonly Dictionary<PanelContainer, Vector2?> _targets = new();
    private int _next;

    /// <summary>위치가 있는 알림을 눌렀을 때</summary>
    [Signal]
    public delegate void JumpRequestedEventHandler(Vector2 worldPosition);

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        MouseFilter = MouseFilterEnum.Ignore;
        for (int i = 0; i < PoolSize; i++)
        {
            var panel = new PanelContainer { Visible = false, MouseFilter = MouseFilterEnum.Ignore };
            var label = new Label { AutowrapMode = TextServer.AutowrapMode.WordSmart, MouseFilter = MouseFilterEnum.Ignore };
            panel.AddChild(label);
            panel.GuiInput += inputEvent => OnPanelInput(panel, inputEvent);
            AddChild(panel);
            _pool.Add(panel);
            _targets[panel] = null;
        }
    }

    public void Show(string text) => Show(text, null);

    public void Show(string text, Vector2? worldPosition)
    {
        // 가장 오래된 칸을 다시 씁니다
        PanelContainer panel = _pool[_next];
        _next = (_next + 1) % _pool.Count;
        if (_tweens.TryGetValue(panel, out Tween? old) && old.IsValid())
        {
            old.Kill();
        }
        _targets[panel] = worldPosition;
        var label = panel.GetChild<Label>(0);
        label.Text = worldPosition.HasValue ? text + "  ▸" : text;
        panel.TooltipText = worldPosition.HasValue ? Strings.ToastJumpTooltip : "";
        panel.MouseFilter = worldPosition.HasValue ? MouseFilterEnum.Stop : MouseFilterEnum.Ignore;
        panel.MouseDefaultCursorShape = worldPosition.HasValue ? CursorShape.PointingHand : CursorShape.Arrow;
        panel.Modulate = Colors.White;
        panel.Visible = true;
        MoveChild(panel, GetChildCount() - 1); // 새 알림이 맨 아래

        Tween tween = CreateTween();
        tween.SetPauseMode(Tween.TweenPauseMode.Process); // 일시정지 중에도 사라지게
        tween.TweenInterval(ShowSeconds);
        tween.TweenProperty(panel, "modulate:a", 0.0f, FadeSeconds);
        tween.TweenCallback(Callable.From(() =>
        {
            panel.Visible = false;
            panel.MouseFilter = MouseFilterEnum.Ignore;
        }));
        _tweens[panel] = tween;
    }

    private void OnPanelInput(PanelContainer panel, InputEvent inputEvent)
    {
        if (inputEvent is InputEventMouseButton { Pressed: true, ButtonIndex: MouseButton.Left } &&
            _targets.TryGetValue(panel, out Vector2? target) && target.HasValue)
        {
            EmitSignal(SignalName.JumpRequested, target.Value);
            AcceptEvent();
        }
    }
}
