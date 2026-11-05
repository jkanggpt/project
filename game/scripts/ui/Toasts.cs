// Toasts.cs — 화면 오른쪽 위에 잠깐 떴다 사라지는 알림
// 알림 노드는 미리 몇 개 만들어 두고 돌려 씁니다 (매번 만들고 지우지 않음).
using System.Collections.Generic;
using Godot;

namespace MagicCastle.UI;

public partial class Toasts : VBoxContainer
{
    private const int PoolSize = 5;
    private const double ShowSeconds = 4.0;
    private const double FadeSeconds = 0.6;

    private readonly List<PanelContainer> _pool = new();
    private readonly Dictionary<PanelContainer, Tween> _tweens = new();
    private int _next;

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        MouseFilter = MouseFilterEnum.Ignore;
        for (int i = 0; i < PoolSize; i++)
        {
            var panel = new PanelContainer { Visible = false, MouseFilter = MouseFilterEnum.Ignore };
            var label = new Label { AutowrapMode = TextServer.AutowrapMode.WordSmart };
            panel.AddChild(label);
            AddChild(panel);
            _pool.Add(panel);
        }
    }

    public void Show(string text)
    {
        // 가장 오래된 칸을 다시 씁니다
        PanelContainer panel = _pool[_next];
        _next = (_next + 1) % _pool.Count;
        if (_tweens.TryGetValue(panel, out Tween? old) && old.IsValid())
        {
            old.Kill();
        }
        panel.GetChild<Label>(0).Text = text;
        panel.Modulate = Colors.White;
        panel.Visible = true;
        MoveChild(panel, GetChildCount() - 1); // 새 알림이 맨 아래

        Tween tween = CreateTween();
        tween.SetPauseMode(Tween.TweenPauseMode.Process); // 일시정지 중에도 사라지게
        tween.TweenInterval(ShowSeconds);
        tween.TweenProperty(panel, "modulate:a", 0.0f, FadeSeconds);
        tween.TweenCallback(Callable.From(() => panel.Visible = false));
        _tweens[panel] = tween;
    }
}
