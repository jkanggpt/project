// BuildingNode.cs — 건물 하나의 그림 (C 의 건물 뷰를 보고 모습을 맞춤)
//   짓는 중: 공사 현장 그림 + 진행 막대 / 부서짐: 체력 막대 / 무너짐: 어둡게 + 기울임 / 추운 집: 눈송이 표시
//   일꾼이 한 명도 없는 일터: 오른쪽 위에 붉은 "!" (자동 배치는 놀고 있는 주민만 보내므로 눈에 띄게)
using Godot;
using MagicCastle.Core;

namespace MagicCastle.World;

public partial class BuildingNode : Node2D
{
    private Sprite2D _sprite = null!;
    private CpuParticles2D _smoke = null!;
    private int _shownState = -1;
    private bool _selected;
    private float _progress = -1f;
    private float _health = 1f;
    private bool _cold;
    private bool _unstaffed;
    private float _pulse;

    public int Id { get; private set; }
    public CoreBuildingView View { get; private set; }

    public static BuildingNode Create(in CoreBuildingView view)
    {
        var node = new BuildingNode { Id = view.Id, Name = $"Building{view.Id}" };
        node._sprite = new Sprite2D { Centered = false };
        node.AddChild(node._sprite);
        node._smoke = new CpuParticles2D
        {
            Emitting = false,
            Amount = 6,
            Lifetime = 1.6,
            Gravity = new Vector2(0, -12),
            InitialVelocityMin = 2,
            InitialVelocityMax = 6,
            Direction = Vector2.Up,
            Spread = 25,
            ScaleAmountMin = 1.5f,
            ScaleAmountMax = 2.5f,
            Color = new Color(0.85f, 0.85f, 0.88f, 0.55f),
        };
        node.AddChild(node._smoke);
        return node;
    }

    public void Sync(in CoreBuildingView view, string key)
    {
        View = view;
        Position = new Vector2(view.X * Art.Tile, view.Y * Art.Tile);
        // 아래쪽이 앞에 오도록 (주민과 겹칠 때 자연스럽게)
        ZIndex = 0;
        if (view.State != _shownState)
        {
            bool firstShow = _shownState < 0;
            _shownState = view.State;
            _sprite.Texture = view.State == (int)BuildingState.Constructing
                ? Art.Construction(view.W, view.H)
                : Art.Building(key, view.W, view.H);
            bool ruined = view.State == (int)BuildingState.Ruined;
            _sprite.Modulate = ruined ? new Color(0.45f, 0.4f, 0.4f) : Colors.White;
            _sprite.Skew = ruined ? 0.12f : 0f; // 무너진 건물은 살짝 기울게
            if (!firstShow || view.State == (int)BuildingState.Constructing)
            {
                Squash();
            }
        }
        _smoke.Position = new Vector2(view.W * Art.Tile * 0.7f, 2);
        bool smoky = view.WorkingNow > 0 && view.Type is (int)BuildingType.Lumber or (int)BuildingType.Quarry
            or (int)BuildingType.ManaMine or (int)BuildingType.Gatherer or (int)BuildingType.House;
        if (_smoke.Emitting != smoky)
        {
            _smoke.Emitting = smoky;
        }
        float progress = view.State == (int)BuildingState.Constructing && view.BuildTotal > 0
            ? view.BuildProgress / (float)view.BuildTotal
            : -1f;
        float health = view.MaxHp > 0 && view.State == (int)BuildingState.Active ? view.Hp / (float)view.MaxHp : 1f;
        bool cold = (view.Flags & (int)BuildingFlags.Cold) != 0;
        bool unstaffed = view.State == (int)BuildingState.Active && view.WorkerSlots > 0 && view.Workers == 0;
        if (!Mathf.IsEqualApprox(progress, _progress) || !Mathf.IsEqualApprox(health, _health) || cold != _cold ||
            unstaffed != _unstaffed)
        {
            _progress = progress;
            _health = health;
            _cold = cold;
            _unstaffed = unstaffed;
            SetProcess(unstaffed);
            QueueRedraw();
        }
    }

    /// <summary>부서지는 순간: 붉게 번쩍이고 흔들림</summary>
    public void Flash()
    {
        _sprite.Modulate = new Color(1.6f, 0.6f, 0.6f);
        Tween tween = CreateTween();
        tween.TweenProperty(_sprite, "modulate", View.State == (int)BuildingState.Ruined ? new Color(0.45f, 0.4f, 0.4f) : Colors.White, 0.4);
        Vector2 home = _sprite.Position;
        Tween shake = CreateTween();
        shake.TweenProperty(_sprite, "position", home + new Vector2(1.5f, 0), 0.05);
        shake.TweenProperty(_sprite, "position", home - new Vector2(1.5f, 0), 0.08);
        shake.TweenProperty(_sprite, "position", home, 0.05);
    }

    public bool Selected
    {
        get => _selected;
        set
        {
            if (_selected != value)
            {
                _selected = value;
                QueueRedraw();
            }
        }
    }

    /// <summary>놓을 때·다 지었을 때 살짝 눌렸다 펴지는 효과</summary>
    private void Squash()
    {
        Scale = new Vector2(1.08f, 0.9f);
        Tween tween = CreateTween();
        tween.TweenProperty(this, "scale", Vector2.One, 0.25).SetTrans(Tween.TransitionType.Back).SetEase(Tween.EaseType.Out);
    }

    public override void _Draw()
    {
        float w = View.W * Art.Tile;
        float h = View.H * Art.Tile;
        if (_progress >= 0)
        {
            DrawRect(new Rect2(2, h - 4, w - 4, 3), new Color(0, 0, 0, 0.6f));
            DrawRect(new Rect2(2, h - 4, (w - 4) * _progress, 3), new Color(0.95f, 0.8f, 0.3f));
        }
        else if (_health < 1f)
        {
            // 부서진 건물: 위쪽에 체력 막대 (어두운 테두리 + 많이 부서질수록 빨강)
            Color fill = _health < 0.34f ? new Color(0.95f, 0.25f, 0.2f)
                : _health < 0.67f ? new Color(1f, 0.72f, 0.2f)
                : new Color(0.45f, 0.95f, 0.4f);
            DrawRect(new Rect2(1, -6, w - 2, 5), new Color(0.08f, 0.06f, 0.1f, 0.85f));
            DrawRect(new Rect2(2, -5, (w - 4) * _health, 3), fill);
        }
        if (_cold)
        {
            // 추운 집: 지붕 위 작은 눈송이 (+ 와 × 겹침)
            var center = new Vector2(w - 4, 3);
            var ice = new Color(0.75f, 0.9f, 1f);
            DrawLine(center + new Vector2(-2.5f, 0), center + new Vector2(2.5f, 0), ice, 1f);
            DrawLine(center + new Vector2(0, -2.5f), center + new Vector2(0, 2.5f), ice, 1f);
            DrawLine(center + new Vector2(-1.8f, -1.8f), center + new Vector2(1.8f, 1.8f), ice, 1f);
            DrawLine(center + new Vector2(-1.8f, 1.8f), center + new Vector2(1.8f, -1.8f), ice, 1f);
        }
        if (_unstaffed)
        {
            // 일꾼 없음: 오른쪽 위 붉은 원에 흰 "!" (천천히 밝아졌다 어두워짐)
            var center = new Vector2(w - 3, 3);
            float glow = 0.75f + 0.25f * Mathf.Sin(_pulse * 3f);
            DrawCircle(center, 3.5f, new Color(0.1f, 0.05f, 0.05f, 0.85f));
            DrawCircle(center, 2.8f, new Color(0.85f * glow, 0.18f, 0.15f));
            DrawLine(center + new Vector2(0, -1.8f), center + new Vector2(0, 0.6f), Colors.White, 1f);
            DrawRect(new Rect2(center + new Vector2(-0.5f, 1.2f), new Vector2(1f, 1f)), Colors.White);
        }
        if (_selected)
        {
            DrawRect(new Rect2(-1, -1, w + 2, h + 2), new Color(1f, 0.95f, 0.5f), false, 1f);
        }
    }

    public override void _Ready()
    {
        SetProcess(_unstaffed); // 깜빡임은 일꾼 없는 일터만 (건물마다 매 프레임 다시 그리지 않게)
    }

    public override void _Process(double delta)
    {
        _pulse += (float)delta;
        QueueRedraw();
    }
}
