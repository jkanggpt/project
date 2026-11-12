// Effects.cs — 한 번 터지고 사라지는 작은 효과 (손맛): 승급·연구 완료의 반짝임, 건물을 다 지었을 때의 먼지
using Godot;

namespace MagicCastle.World;

public static class Effects
{
    /// <summary>위로 퍼지는 반짝임 (승급은 금색, 연구는 보라색)</summary>
    public static void Sparkle(Node parent, Vector2 position, Color color, int amount = 28)
    {
        Burst(parent, position, color, amount, new Vector2(0, -20), 30f, 70f, 1.1, 1.5f, 3f);
    }

    /// <summary>바닥에서 옆으로 퍼지는 흙먼지 (건물을 다 지었을 때)</summary>
    public static void Dust(Node parent, Vector2 position, float width)
    {
        CpuParticles2D dust = Burst(parent, position, new Color(0.85f, 0.78f, 0.65f, 0.7f), 14, new Vector2(0, -6), 12f, 30f, 0.6, 2f, 3.5f);
        dust.EmissionShape = CpuParticles2D.EmissionShapeEnum.Rectangle;
        dust.EmissionRectExtents = new Vector2(width / 2f, 1f);
        dust.Direction = Vector2.Up;
        dust.Spread = 80f;
    }

    private static CpuParticles2D Burst(Node parent, Vector2 position, Color color, int amount, Vector2 gravity,
        float minSpeed, float maxSpeed, double lifetime, float minScale, float maxScale)
    {
        var particles = new CpuParticles2D
        {
            Position = position,
            Emitting = false,
            OneShot = true,
            Explosiveness = 0.9f,
            Amount = amount,
            Lifetime = lifetime,
            Gravity = gravity,
            Direction = Vector2.Up,
            Spread = 70f,
            InitialVelocityMin = minSpeed,
            InitialVelocityMax = maxSpeed,
            ScaleAmountMin = minScale,
            ScaleAmountMax = maxScale,
            Color = color,
            ZIndex = 5,
        };
        parent.AddChild(particles);
        particles.Emitting = true;
        particles.Finished += particles.QueueFree;
        return particles;
    }
}
