// VillagerAgent.cs — 주민 한 명의 그림과 걷기 (핸드셰이크의 Godot 쪽)
//
// 1. C 의 의도(intent_rev)가 바뀌면 길을 찾아 걷기 시작합니다.
// 2. 목표에 닿으면 core_villager_arrived(id, rev) 를 보고합니다.
// 3. 길이 없으면 core_villager_unreachable(id, rev) 를 보고합니다.
// 걷는 속도는 C 가 준 값(한 틱에 걷는 칸)을 실제 시간과 배속으로 바꿔 씁니다. 도로 위에서는 빨라집니다.
// 습격 중: 성채에 피신한 주민은 숨기고, 자리에 선 경비병은 CombatDirector 가 움직입니다(CombatControlled).
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
    private bool _silent; // 싸움을 마치고 C 가 아는 자리로 걸어 돌아가는 중 (이미 도착한 것으로 되어 있어 보고하지 않음)

    public int Id { get; private set; }
    public CoreVillagerView View { get; private set; }
    public bool Walking => _walkingRev >= 0;

    /// <summary>경비병이 싸우는 동안 CombatDirector 가 위치를 정합니다 (이때는 C 의 의도대로 걷지 않음)</summary>
    public bool CombatControlled { get; private set; }

    /// <summary>밖에 나와 있어 적에게 보이는가 (성채에 숨었거나 집에서 자면 아님)</summary>
    public bool Exposed => Visible && View.State != (int)VillagerState.Leaving;

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
        if (CombatControlled)
        {
            return;
        }
        if (view.AtTarget != 0 || view.Holding)
        {
            if (_silent && _walkingRev >= 0 && view.IntentRev == _walkingRev)
            {
                return; // 싸운 자리에서 걸어 돌아가는 중
            }
            _silent = false;
            _walkingRev = -1; // 이미 도착했거나 제자리에 있으라는 의도
            var location = new Vector2I(view.LocX, view.LocY);
            if (view.AtTarget != 0 && CurrentCell != location)
            {
                // C 가 옮겨 놓은 경우 (불러오기, 건물에 깔려 옮겨짐): C 의 위치로 맞춥니다
                Position = CellCenter(location) + _jitter;
            }
            return;
        }
        if (!_silent && (view.IntentRev == _walkingRev || view.IntentRev == _reportedRev))
        {
            return; // 같은 의도로 걷는 중이거나 이미 보고함
        }
        _silent = false;
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

    /// <summary>경비병을 넘겨받습니다 (CombatDirector)</summary>
    public void BeginCombat()
    {
        CombatControlled = true;
        _walkingRev = -1;
        _silent = false;
    }

    /// <summary>
    /// 경비병을 돌려줍니다. C 는 경비병이 아직 자리에 있다고 알고 있으므로, 싸운 곳에서 그 자리까지 조용히 걸어 돌아갑니다.
    /// 그 사이 C 가 새 의도를 주면 평소처럼 그 의도를 따릅니다.
    /// </summary>
    public void EndCombat(Pathfinder paths)
    {
        if (!CombatControlled)
        {
            return;
        }
        CombatControlled = false;
        _body.Position = Vector2.Zero;
        var location = new Vector2I(View.LocX, View.LocY);
        if (View.AtTarget != 0 && CurrentCell != location && paths.FindPath(CurrentCell, location, _path))
        {
            _walkingRev = View.IntentRev;
            _pathIndex = 0;
            _silent = true;
        }
    }

    /// <summary>싸우는 동안 CombatDirector 가 옮김 (방향과 걷는 흔들림도 맞춤)</summary>
    public void CombatMove(Vector2 position, float delta)
    {
        Vector2 offset = position - Position;
        if (offset.LengthSquared() > 0.0001f)
        {
            _body.FlipH = offset.X < 0;
            _bobTime += delta * 12f;
            _body.Position = new Vector2(0, -Mathf.Abs(Mathf.Sin(_bobTime)) * 1.5f);
        }
        else
        {
            _body.Position = Vector2.Zero;
        }
        Position = position;
    }

    /// <summary>적에게 맞음 (빨갛게 깜빡임)</summary>
    public void Hit()
    {
        _body.Modulate = new Color(1f, 0.35f, 0.35f);
        Tween tween = CreateTween();
        tween.TweenProperty(_body, "modulate", Colors.White, 0.25f);
    }

    private void UpdateLooks(in CoreVillagerView view, string? jobKey)
    {
        // 집 안에서 자는 주민, 습격 중 성채에 피신한 주민은 보이지 않음
        bool asleepInside = view.State == (int)VillagerState.Sleep && view.AtTarget != 0 && view.HomeId != 0 &&
                            view.TargetBuilding == view.HomeId;
        bool sheltered = view.State == (int)VillagerState.Flee && view.AtTarget != 0 && !CombatControlled;
        Visible = !(asleepInside || sheltered);
        bool showsTool = view.State is (int)VillagerState.Working or (int)VillagerState.GuardPost;
        Texture2D? tool = jobKey != null && showsTool ? Art.JobIcon(jobKey) : null;
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
        else if (view.State == (int)VillagerState.Flee)
        {
            bubble = "bubble_alarm";
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
        if (CombatControlled)
        {
            return;
        }
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
        _body.Position = Vector2.Zero;
        if (_silent)
        {
            _silent = false; // 제자리로 돌아옴 (C 는 이미 도착한 것으로 알고 있음)
            return;
        }
        _reportedRev = rev;
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
