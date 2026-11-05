// WeatherFx.cs — 비·눈·폭풍 입자 (화면 공간, 카메라와 상관없이 화면 전체에 내림)
//
// 날씨는 C 가 하루에 한 번 정하고 요약 뷰로 알려 줍니다. 여기는 보여 주기만 합니다.
using Godot;
using MagicCastle.Core;

namespace MagicCastle.World;

public partial class WeatherFx : CanvasLayer
{
    private CpuParticles2D _rain = null!;
    private CpuParticles2D _snow = null!;
    private ColorRect _gloom = null!;
    private int _weather = -1;

    public override void _Ready()
    {
        Layer = 5; // 월드 위, HUD(10) 아래
        _gloom = new ColorRect { Color = new Color(0.1f, 0.12f, 0.2f, 0f), MouseFilter = Control.MouseFilterEnum.Ignore };
        _gloom.SetAnchorsPreset(Control.LayoutPreset.FullRect);
        AddChild(_gloom);
        _rain = MakeParticles(MakeDot(1, 6, new Color(0.78f, 0.86f, 1f, 0.75f)), 380, 1.4, new Vector2(30, 900));
        _snow = MakeParticles(MakeDot(2, 2, new Color(1f, 1f, 1f, 0.9f)), 180, 6.0, new Vector2(8, 45));
        _snow.InitialVelocityMin = 10;
        _snow.InitialVelocityMax = 30;
        _snow.Spread = 40;
        AddChild(_rain);
        AddChild(_snow);
        GetViewport().SizeChanged += Resize;
        Resize();
    }

    private static CpuParticles2D MakeParticles(Texture2D texture, int amount, double lifetime, Vector2 gravity) => new()
    {
        Emitting = false,
        Texture = texture,
        Amount = amount,
        Lifetime = lifetime,
        Preprocess = lifetime,
        EmissionShape = CpuParticles2D.EmissionShapeEnum.Rectangle,
        Direction = Vector2.Down,
        Spread = 5,
        Gravity = gravity,
        InitialVelocityMin = 60,
        InitialVelocityMax = 90,
        TextureFilter = CanvasItem.TextureFilterEnum.Nearest,
    };

    private static ImageTexture MakeDot(int width, int height, Color color)
    {
        var image = Image.CreateEmpty(width, height, false, Image.Format.Rgba8);
        image.Fill(color);
        return ImageTexture.CreateFromImage(image);
    }

    private void Resize()
    {
        Vector2 size = GetViewport().GetVisibleRect().Size;
        foreach (CpuParticles2D particles in new[] { _rain, _snow })
        {
            // 화면 위쪽 띠 전체에서 내리게 (바람에 밀리는 만큼 왼쪽으로 더 넓게)
            particles.Position = new Vector2(size.X * 0.5f, -20);
            particles.EmissionRectExtents = new Vector2(size.X * 0.7f, 10);
            int scale = Mathf.Max(1, (int)(size.Y / 360));
            particles.ScaleAmountMin = scale;
            particles.ScaleAmountMax = scale;
        }
    }

    /// <summary>날씨가 바뀌었을 때만 입자를 켜고 끕니다</summary>
    public void SetWeather(int weather)
    {
        if (weather == _weather)
        {
            return;
        }
        _weather = weather;
        bool storm = weather == (int)Weather.Storm;
        _rain.Emitting = weather == (int)Weather.Rain || storm;
        _rain.Amount = storm ? 700 : 380;
        _rain.Gravity = storm ? new Vector2(260, 900) : new Vector2(30, 900); // 폭풍은 옆바람
        _snow.Emitting = weather == (int)Weather.Snow;
        Tween tween = CreateTween();
        float gloom = storm ? 0.3f : weather == (int)Weather.Rain ? 0.16f : 0f;
        tween.TweenProperty(_gloom, "color:a", gloom, 1.5);
    }
}
