// DayNight.cs — 시각에 따라 세상 색을 바꾸는 CanvasModulate (HUD 는 다른 CanvasLayer 라서 영향 없음)
// 밤이 시작·끝나는 시각은 C 코어의 설정값을 씁니다 (규칙과 그림이 어긋나지 않게).
using System;
using Godot;

namespace MagicCastle.World;

public partial class DayNight : CanvasModulate
{
    private static readonly Color NightColor = new(0.40f, 0.44f, 0.70f);
    private static readonly Color DawnColor = new(0.98f, 0.82f, 0.76f);
    private static readonly Color DayColor = new(1f, 1f, 1f);
    private static readonly Color DuskColor = new(1f, 0.74f, 0.60f);

    private (float Minute, Color Color)[] _keys = Array.Empty<(float, Color)>();

    public void Configure(int nightStartMinute, int nightEndMinute)
    {
        _keys = new (float, Color)[]
        {
            (0, NightColor),
            (nightEndMinute, NightColor),
            (nightEndMinute + 60, DawnColor),
            (nightEndMinute + 150, DayColor),
            (nightStartMinute - 150, DayColor),
            (nightStartMinute - 60, DuskColor),
            (nightStartMinute, NightColor),
            (24 * 60, NightColor),
        };
    }

    /// <summary>minute: 0..1440 (소수 가능)</summary>
    public void SetMinute(double minute)
    {
        if (_keys.Length == 0)
        {
            return;
        }
        float m = (float)(minute % (24 * 60));
        for (int i = 1; i < _keys.Length; i++)
        {
            if (m <= _keys[i].Minute)
            {
                var (startMinute, startColor) = _keys[i - 1];
                var (endMinute, endColor) = _keys[i];
                float span = Math.Max(1f, endMinute - startMinute);
                Color = startColor.Lerp(endColor, Math.Clamp((m - startMinute) / span, 0f, 1f));
                return;
            }
        }
        Color = NightColor;
    }
}
