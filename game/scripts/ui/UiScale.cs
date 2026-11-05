// UiScale.cs — UI 크기를 정수 배율로 맞춥니다.
//
// Galmuri11 은 12px 에 맞춰 그린 픽셀 글꼴이라 12의 정수배(24, 36 …)일 때만 또렷합니다.
// 그래서 테마(ui/theme.tres)는 1배 크기로 만들어 두고, 여기서 글꼴 크기·여백·테두리를 배율만큼 곱합니다.
// 프로젝트 테마를 바꾸므로 툴팁 같은 팝업에도 똑같이 적용됩니다.
using System;
using System.Collections.Generic;
using Godot;

namespace MagicCastle.UI;

public static class UiScale
{
    private const int BaseFontSize = 12;

    private static readonly Dictionary<StyleBox, float[]> BaseMargins = new();
    private static readonly Dictionary<StyleBoxFlat, int[]> BaseBorders = new();
    private static readonly Dictionary<(StringName Name, StringName Type), int> BaseConstants = new();
    private static bool _captured;

    public static int Current { get; private set; } = 1;

    /// <summary>창 크기에 맞는 기본 배율 (1280×720 → 2, 1920×1080 → 3, 1024×768 → 1)</summary>
    public static int DefaultFor(Vector2I windowSize) =>
        Math.Clamp(Math.Min(windowSize.Y / 360, windowSize.X / 640), 1, 4);

    public static void Apply(int scale)
    {
        Theme? theme = ThemeDB.GetProjectTheme();
        if (theme == null)
        {
            return;
        }
        scale = Math.Clamp(scale, 1, 4);
        Capture(theme);
        theme.DefaultFontSize = BaseFontSize * scale;
        foreach (var (style, margins) in BaseMargins)
        {
            for (int side = 0; side < 4; side++)
            {
                style.SetContentMargin((Side)side, margins[side] * scale);
            }
        }
        foreach (var (style, borders) in BaseBorders)
        {
            for (int side = 0; side < 4; side++)
            {
                style.SetBorderWidth((Side)side, borders[side] * scale);
            }
        }
        foreach (var ((name, type), value) in BaseConstants)
        {
            theme.SetConstant(name, type, value * scale);
        }
        Current = scale;
    }

    /// <summary>처음 한 번, 테마의 1배 값을 기억해 둡니다 (배율을 여러 번 바꿔도 누적되지 않게).</summary>
    private static void Capture(Theme theme)
    {
        if (_captured)
        {
            return;
        }
        _captured = true;
        foreach (string type in theme.GetStyleboxTypeList())
        {
            foreach (string name in theme.GetStyleboxList(type))
            {
                StyleBox style = theme.GetStylebox(name, type);
                if (!BaseMargins.ContainsKey(style))
                {
                    BaseMargins[style] = new[]
                    {
                        style.GetContentMargin(Side.Left), style.GetContentMargin(Side.Top),
                        style.GetContentMargin(Side.Right), style.GetContentMargin(Side.Bottom),
                    };
                }
                if (style is StyleBoxFlat flat && !BaseBorders.ContainsKey(flat))
                {
                    BaseBorders[flat] = new[]
                    {
                        flat.GetBorderWidth(Side.Left), flat.GetBorderWidth(Side.Top),
                        flat.GetBorderWidth(Side.Right), flat.GetBorderWidth(Side.Bottom),
                    };
                }
            }
        }
        foreach (string type in theme.GetConstantTypeList())
        {
            foreach (string name in theme.GetConstantList(type))
            {
                BaseConstants[(name, type)] = theme.GetConstant(name, type);
            }
        }
    }
}
