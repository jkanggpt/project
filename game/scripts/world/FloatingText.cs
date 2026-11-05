// FloatingText.cs — "+3 식량" 처럼 떠올랐다 사라지는 글자 (미리 만들어 두고 돌려 씀)
using System.Collections.Generic;
using Godot;

namespace MagicCastle.World;

public partial class FloatingText : Node2D
{
    private const int PoolSize = 24;
    private readonly List<Label> _pool = new();
    private int _next;

    public override void _Ready()
    {
        ZIndex = 50;
        for (int i = 0; i < PoolSize; i++)
        {
            var label = new Label
            {
                Visible = false,
                HorizontalAlignment = HorizontalAlignment.Center,
                Size = new Vector2(80, 14),
            };
            label.AddThemeFontSizeOverride("font_size", 12);
            label.AddThemeColorOverride("font_outline_color", new Color(0.1f, 0.08f, 0.1f));
            label.AddThemeConstantOverride("outline_size", 3);
            AddChild(label);
            _pool.Add(label);
        }
    }

    public void Show(Vector2 worldPosition, string text, Color color)
    {
        Label label = _pool[_next];
        _next = (_next + 1) % _pool.Count;
        label.Text = text;
        label.Modulate = color;
        label.Position = worldPosition - new Vector2(40, 12);
        label.Scale = new Vector2(0.5f, 0.5f); // 12px 글꼴을 월드 픽셀의 절반 크기로 (확대해도 또렷)
        label.Visible = true;
        Tween tween = label.CreateTween();
        tween.SetParallel();
        tween.TweenProperty(label, "position:y", label.Position.Y - 10, 1.2);
        tween.TweenProperty(label, "modulate:a", 0f, 1.2).SetDelay(0.4);
        tween.Chain().TweenCallback(Callable.From(() => label.Visible = false));
    }
}
