// VillagerAgent.cs — 주민 한 명의 그림과 걷기 (핸드셰이크의 Godot 쪽)
//
// 1. C 의 의도(intent_rev)가 바뀌면 길을 찾아 걷기 시작합니다.
// 2. 목표에 닿으면 core_villager_arrived(id, rev) 를 보고합니다.
// 3. 길이 없으면 core_villager_unreachable(id, rev) 를 보고합니다.
// 걷는 속도는 C 가 준 값(한 틱에 걷는 칸)을 실제 시간과 배속으로 바꿔 씁니다. 도로 위에서는 빨라집니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.World;

public partial class VillagerAgent : Node2D
{
    /// <summary>스모크 테스트처럼 시간을 크게 건너뛸 때: 길만 확인하고 바로 도착</summary>
    public static bool InstantTravel { get; set; }

    private readonly List<Vector2I> _path = new();
    private Sprite2D _body = null!;
    private Sprite2D _tool = null!;
    private Sprite2D _bubble = null!;
    private Vector2 _jitter;
    private int _pathIndex;
    private int _walkingRev = -1;
    private int _reportedRev = -1;
    private float _bobTime;
    private bool _selected;

    public int Id { get; private set; }
    public CoreVillagerView View { get; private set; }
    public bool Walking => _walkingRev >= 0;

    public static VillagerAgent Create(in CoreVillagerView view)
    {
        var agent = new VillagerAgent { Id = view.Id, Name = $"Villager{view.Id}" };
        agent._body = new Sprite2D { Texture = Art.Villager(view.Id), Offset = new Vector2(0, -6) };
        agent._tool = new Sprite2D { Scale = new Vector2(0.5f, 0.5f), Position = new Vector2(5, -3), Visible = false };
        agent._bubble = new Sprite2D { Position = new Vector2(0, -17), Visible = false };
        agent.AddChild(agent._body);
        agent.AddChild(agent._tool);
        agent.AddChild(agent._bubble);
        // 같은 칸에 여럿이 서도 겹치지 않게 주민마다 조금씩 비켜 섬
        var random = new RandomNumberGenerator { Seed = (ulong)view.Id * 2654435761UL };
        agent._jitter = new Vector2(random.RandfRange(-4, 4), random.RandfRange(-3, 3));
        agent.Position = CellCenter(new Vector2I(view.LocX, view.LocY)) + agent._jitter;
        return agent;
    }

    public static Vector2 CellCenter(Vector2I cell) => new((cell.X + 0.5f) * Art.Tile, (cell.Y + 0.5f) * Art.Tile);

    public Vector2I CurrentCell =>
        new(Mathf.FloorToInt(Position.X / Art.Tile), Mathf.FloorToInt(Position.Y / Art.Tile));

    /// <summary>C 의 새 뷰를 받아 모습과 걷기를 맞춥니다</summary>
    public void Sync(in CoreVillagerView view, Pathfinder paths, string? jobKey)
    {
        View = view;
        UpdateLooks(view, jobKey);
        if (view.AtTarget != 0 || view.Holding)
        {
            _walkingRev = -1; // 이미 도착했거나 제자리에 있으라는 의도
            var location = new Vector2I(view.LocX, view.LocY);
            if (view.AtTarget != 0 && CurrentCell != location)
            {
                // C 가 옮겨 놓은 경우 (불러오기, 건물에 깔려 옮겨짐): C 의 위치로 맞춥니다
                Position = CellCenter(location) + _jitter;
            }
            return;
        }
        if (view.IntentRev == _walkingRev || view.IntentRev == _reportedRev)
        {
            return; // 같은 의도로 걷는 중이거나 이미 보고함
        }
        var target = new Vector2I(view.TargetX, view.TargetY);
        if (!paths.FindPath(CurrentCell, target, _path))
        {
            _reportedRev = view.IntentRev;
            _walkingRev = -1;
            CoreBridge.Instance.VillagerUnreachable(view.Id, view.IntentRev);
            return;
        }
        _walkingRev = view.IntentRev;
        _pathIndex = 0;
        if (InstantTravel)
        {
            Position = CellCenter(target) + _jitter;
            Arrive();
        }
    }

    private void UpdateLooks(in CoreVillagerView view, string? jobKey)
    {
        // 집 안에서 자는 주민은 보이지 않음
        Visible = !(view.State == (int)VillagerState.Sleep && view.AtTarget != 0 && view.HomeId != 0 &&
                    view.TargetBuilding == view.HomeId);
        Texture2D? tool = jobKey != null && view.State == (int)VillagerState.Working ? Art.JobIcon(jobKey) : null;
        _tool.Texture = tool;
        _tool.Visible = tool != null;
        string? bubble = null;
        if (view.Holding)
        {
            bubble = "bubble_blocked";
        }
        else if (view.Has(VillagerFlags.Sick))
        {
            bubble = "bubble_sick";
        }
        else if (view.Hunger < 20)
        {
            bubble = "bubble_hungry";
        }
        else if (view.Has(VillagerFlags.Unhappy))
        {
            bubble = "bubble_angry";
        }
        else if (view.State == (int)VillagerState.Sleep)
        {
            bubble = "bubble_sleep";
        }
        _bubble.Texture = bubble != null ? Art.Icon(bubble) : null;
        _bubble.Visible = _bubble.Texture != null;
        _body.Modulate = view.State == (int)VillagerState.Leaving ? new Color(1, 1, 1, 0.6f) : Colors.White;
    }

    public override void _Process(double delta)
    {
        if (_walkingRev < 0)
        {
            _body.Position = Vector2.Zero;
            return;
        }
        if (_pathIndex >= _path.Count)
        {
            Arrive();
            return;
        }
        CoreConfigView config = CoreBridge.Instance.Config;
        float secondsPerTick = config.RealMsPerTick / 1000f;
        WorldSync? sync = WorldSync.Instance;
        float factor = sync != null ? sync.Paths.SpeedFactor(CurrentCell) : 1f;
        // 한 틱에 걷는 칸 × 16px ÷ 한 틱의 실제 초 × 배속 = 초당 픽셀
        float speed = View.WalkSpeed / 1000f * Art.Tile / secondsPerTick * GameClock.Instance.Speed * factor;
        float step = speed * (float)delta;
        while (step > 0 && _pathIndex < _path.Count)
        {
            Vector2 goal = CellCenter(_path[_pathIndex]) + _jitter;
            Vector2 offset = goal - Position;
            float distance = offset.Length();
            if (distance <= step)
            {
                Position = goal;
                step -= distance;
                _pathIndex++;
            }
            else
            {
                Position += offset / distance * step;
                _body.FlipH = offset.X < 0;
                step = 0;
            }
        }
        _bobTime += (float)delta * 12f;
        _body.Position = new Vector2(0, -Mathf.Abs(Mathf.Sin(_bobTime)) * 1.5f);
        if (_pathIndex >= _path.Count)
        {
            Arrive();
        }
    }

    private void Arrive()
    {
        int rev = _walkingRev;
        _walkingRev = -1;
        _reportedRev = rev;
        _body.Position = Vector2.Zero;
        if (rev >= 0)
        {
            CoreBridge.Instance.VillagerArrived(Id, rev);
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

    public override void _Draw()
    {
        // 발밑 납작한 그림자, 고르면 노란 테두리 (원을 세로로 반 눌러 타원으로)
        DrawSetTransform(new Vector2(0, 1), 0, new Vector2(1, 0.5f));
        DrawCircle(Vector2.Zero, 5, new Color(0, 0, 0, 0.25f));
        if (_selected)
        {
            DrawArc(Vector2.Zero, 7, 0, Mathf.Tau, 20, new Color(1f, 0.95f, 0.5f), 1.5f);
        }
        DrawSetTransform(Vector2.Zero, 0, Vector2.One);
    }
}
