// BuildingNode.cs — 건물 하나의 그림 (C 의 건물 뷰를 보고 모습을 맞춤)
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
            _sprite.Modulate = view.State == (int)BuildingState.Ruined ? new Color(0.45f, 0.4f, 0.4f) : Colors.White;
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
        if (!Mathf.IsEqualApprox(progress, _progress))
        {
            _progress = progress;
            QueueRedraw();
        }
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
        if (_selected)
        {
            DrawRect(new Rect2(-1, -1, w + 2, h + 2), new Color(1f, 0.95f, 0.5f), false, 1f);
        }
    }
}
