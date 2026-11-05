// EnemyAgent.cs — 습격하는 적 한 명 (그림, 체력 막대, 걷기, 행동)
//
// 적의 수치(체력, 피해, 공격 사이, 속도, 거리, 행동, 훔치는 양)는 모두 C 의 정의(data/enemies.def)에서 옵니다.
// 여기서는 "어디로 가서 무엇을 때리는지"만 정하고, 결과는 CombatDirector 를 거쳐 core_report_* 로 C 에 알립니다.
//
// 행동 (CoreEnemyBehavior)
//   도적(Thief):   가장 가까운 저장소(성채·창고)로 가서 훔치고, 온 쪽 가장자리로 달아납니다.
//   병사(Raider):  성채 쪽에 가까운 건물부터 부숩니다. 성채는 체력 1 에서 버티므로 그다음 건물로 갑니다.
//   마물(Monster): 근처에 나와 있는 주민·경비병을 먼저 노리고, 없으면 집을 부수거나 저장소에서 훔쳐 달아납니다.
// 모두: 다음 칸이 성벽·성문이면 멈춰서 부숩니다 (SPEC §3.3 규칙 6). 곁에 경비병이 있으면 먼저 맞서 싸웁니다.
//
// 시간은 "게임 속 분"으로 셉니다 (CombatDirector.Step 이 넘겨 줌). 그래서 배속을 바꾸거나
// 스모크 테스트가 빨리 돌려도 같은 규칙으로 움직입니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.World;

public partial class EnemyAgent : Node2D
{
    private enum Mode { Seek, Flee, Gone }

    private enum TargetKind { None, Building, Villager }

    private readonly List<Vector2I> _path = new();
    private readonly HashSet<int> _unreachable = new();
    private Sprite2D _body = null!;
    private CoreEnemyDefView _def;
    private Vector2 _jitter;
    private Mode _mode = Mode.Seek;
    private TargetKind _targetKind;
    private int _targetId;
    private Vector2I _goal;
    private Vector2I _home; // 달아날 가장자리 칸
    private int _pathIndex;
    private bool _hasPath;
    private float _cooldown;
    private float _retarget;
    private float _bobTime;

    public int Id { get; private set; }
    public int Type { get; private set; }
    public int Hp { get; private set; }
    public int MaxHp { get; private set; }

    /// <summary>날아오는 중인 화살의 피해 (여러 탑이 이미 죽을 적을 함께 쏘지 않게)</summary>
    public int IncomingDamage { get; set; }

    public bool Alive => _mode != Mode.Gone && Hp > 0;
    public bool Looted { get; private set; }
    public EnemyBehavior Behavior => (EnemyBehavior)_def.Behavior;
    public float RangeTiles => System.Math.Max(1, _def.Range);

    public static EnemyAgent Create(int id, in CoreEnemyDefView def, Vector2I spawn, Vector2I home)
    {
        var agent = new EnemyAgent { Id = id, Type = def.Type, Name = $"Enemy{id}", _def = def, _home = home };
        agent.Hp = def.Hp;
        agent.MaxHp = System.Math.Max(1, def.Hp);
        agent._body = new Sprite2D { Texture = Art.Enemy(def.Type), Offset = new Vector2(0, -6) };
        agent.AddChild(agent._body);
        var random = new RandomNumberGenerator { Seed = (ulong)id * 40503UL + 7UL };
        agent._jitter = new Vector2(random.RandfRange(-3, 3), random.RandfRange(-2, 2));
        agent.Position = VillagerAgent.CellCenter(spawn) + agent._jitter;
        agent._cooldown = random.RandfRange(0, def.Cooldown); // 한꺼번에 때리지 않게 조금씩 어긋남
        return agent;
    }

    public Vector2I CurrentCell =>
        new(Mathf.FloorToInt(Position.X / Art.Tile), Mathf.FloorToInt(Position.Y / Art.Tile));

    /// <summary>길찾기 격자가 바뀜 (성벽이 무너짐 등): 다음 걸음에서 길을 다시 찾습니다</summary>
    public void Repath()
    {
        _hasPath = false;
        _unreachable.Clear();
    }

    /// <summary>화살·경비병에게 맞음. 쓰러졌으면 true</summary>
    public bool TakeDamage(int damage)
    {
        if (!Alive)
        {
            return false;
        }
        Hp = System.Math.Max(0, Hp - damage);
        _body.Modulate = new Color(1f, 0.4f, 0.4f);
        Tween tween = CreateTween();
        tween.TweenProperty(_body, "modulate", Colors.White, 0.2f);
        QueueRedraw();
        return Hp <= 0;
    }

    /// <summary>쓰러지거나 달아나 사라짐 (그림만 천천히 지움)</summary>
    public void Vanish(bool killed)
    {
        _mode = Mode.Gone;
        QueueRedraw();
        Tween tween = CreateTween();
        if (killed)
        {
            tween.TweenProperty(_body, "rotation", Mathf.Pi / 2, 0.2f);
        }
        tween.TweenProperty(this, "modulate:a", 0.0f, killed ? 0.6f : 0.4f);
        tween.TweenCallback(Callable.From(QueueFree));
    }

    // ---- 한 걸음 (CombatDirector 가 부름) --------------------------------------

    public void Step(CombatDirector director, float minutes)
    {
        if (_mode == Mode.Gone)
        {
            return;
        }
        _cooldown = System.Math.Max(0, _cooldown - minutes);
        _retarget -= minutes;
        if (_mode == Mode.Flee)
        {
            if (!_hasPath && !PlanPath(director, _home))
            {
                director.Escaped(this); // 달아날 길도 없으면 그 자리에서 물러난 것으로 봄
                return;
            }
            if (Walk(director, minutes))
            {
                director.Escaped(this);
            }
            return;
        }

        // 1) 곁에 있는 경비병(마물은 주민도)에게 먼저 맞섬
        VillagerAgent? victim = director.VillagerInReach(Position, RangeTiles + 0.4f, Behavior == EnemyBehavior.Monster);
        if (victim != null)
        {
            Face(victim.Position);
            if (_cooldown <= 0)
            {
                _cooldown = _def.Cooldown;
                director.StrikeVillager(this, victim, _def.Damage);
            }
            return;
        }

        // 2) 목표 고르기 (가끔 다시 고름: 더 가까운 저장소가 생기거나 목표가 무너졌을 수 있음)
        if (_targetKind == TargetKind.None || _retarget <= 0 || !TargetValid(director))
        {
            _retarget = CombatDirector.RetargetMinutes;
            if (!ChooseTarget(director))
            {
                StartFlee();
                return;
            }
        }

        // 3) 목표에 닿았으면 훔치거나 때림
        if (_targetKind == TargetKind.Building && director.TryGetBuilding(_targetId, out CoreBuildingView target) &&
            CombatDirector.DistanceToRect(Position, target) <= RangeTiles + 0.25f)
        {
            Face(CombatDirector.RectCenter(target));
            if (CombatDirector.IsStorage(target) && WantsLoot())
            {
                director.Loot(this, target);
                Looted = true;
                StartFlee();
                return;
            }
            if (_cooldown <= 0)
            {
                _cooldown = _def.Cooldown;
                director.StrikeBuilding(this, target, _def.Damage);
            }
            return;
        }

        // 4) 걷기 (마물이 주민을 쫓을 때는 주민이 움직였으면 길을 다시 찾음)
        if (_targetKind == TargetKind.Villager && director.Sync.AgentFor(_targetId) is VillagerAgent prey &&
            prey.CurrentCell != _goal)
        {
            _hasPath = false;
        }
        if (!_hasPath)
        {
            Vector2I goal = GoalCell(director);
            if (!PlanPath(director, goal))
            {
                _unreachable.Add(_targetId);
                _targetKind = TargetKind.None;
                return;
            }
        }
        if (Walk(director, minutes))
        {
            _hasPath = false; // 목표 칸에 왔는데 닿지 않음 (목표가 옮겨짐): 다음에 다시 찾음
        }
    }

    private bool WantsLoot() =>
        !Looted && (Behavior == EnemyBehavior.Thief || Behavior == EnemyBehavior.Monster);

    private void StartFlee()
    {
        _mode = Mode.Flee;
        _hasPath = false;
        _targetKind = TargetKind.None;
    }

    private bool TargetValid(CombatDirector director)
    {
        switch (_targetKind)
        {
            case TargetKind.Building:
                return director.TryGetBuilding(_targetId, out CoreBuildingView b) && director.Attackable(b, this);
            case TargetKind.Villager:
                return director.Sync.AgentFor(_targetId) is VillagerAgent v && v.Exposed &&
                       Position.DistanceTo(v.Position) <= CombatDirector.MonsterSightTiles * Art.Tile * 1.5f;
            default:
                return false;
        }
    }

    private bool ChooseTarget(CombatDirector director)
    {
        _hasPath = false;
        if (Behavior == EnemyBehavior.Monster &&
            director.NearestExposedVillager(Position, CombatDirector.MonsterSightTiles, _unreachable) is VillagerAgent prey)
        {
            _targetKind = TargetKind.Villager;
            _targetId = prey.Id;
            return true;
        }
        int building = director.ChooseBuilding(this, Behavior, _unreachable);
        if (building == 0)
        {
            _targetKind = TargetKind.None;
            return false;
        }
        _targetKind = TargetKind.Building;
        _targetId = building;
        return true;
    }

    private Vector2I GoalCell(CombatDirector director)
    {
        if (_targetKind == TargetKind.Villager && director.Sync.AgentFor(_targetId) is VillagerAgent prey)
        {
            return prey.CurrentCell;
        }
        if (_targetKind == TargetKind.Building && director.TryGetBuilding(_targetId, out CoreBuildingView b))
        {
            return director.ApproachCell(b, Position);
        }
        return CurrentCell;
    }

    private bool PlanPath(CombatDirector director, Vector2I goal)
    {
        Pathfinder paths = director.Sync.EnemyPaths;
        if (!paths.Walkable(goal) && !paths.TryNearestWalkable(goal, out goal))
        {
            return false;
        }
        if (!paths.FindPath(CurrentCell, goal, _path))
        {
            return false;
        }
        _goal = goal;
        _pathIndex = 0;
        _hasPath = true;
        return true;
    }

    /// <summary>길을 따라 걷습니다. 다음 칸이 서 있는 성벽·성문이면 멈춰서 부숩니다. 끝에 닿으면 true</summary>
    private bool Walk(CombatDirector director, float minutes)
    {
        Pathfinder paths = director.Sync.EnemyPaths;
        CoreConfigView config = CoreBridge.Instance.Config;
        float tilesPerMinute = _def.Speed / 1000f / System.Math.Max(1, config.MinutesPerTick);
        float step = tilesPerMinute * Art.Tile * minutes * paths.SpeedFactor(CurrentCell);
        bool moved = false;
        while (step > 0 && _pathIndex < _path.Count)
        {
            Vector2I next = _path[_pathIndex];
            if (next != CurrentCell && Pathfinder.IsBarrier(paths.CostAt(next)))
            {
                int wall = director.Sync.BuildingAt(next);
                if (wall != 0 && director.TryGetBuilding(wall, out CoreBuildingView barrier) &&
                    barrier.State == (int)BuildingState.Active)
                {
                    Face(VillagerAgent.CellCenter(next));
                    if (_cooldown <= 0)
                    {
                        _cooldown = _def.Cooldown;
                        director.StrikeBuilding(this, barrier, _def.Damage);
                    }
                    break; // 무너질 때까지 여기서 기다림 (무너지면 격자가 바뀌어 길을 다시 찾음)
                }
            }
            Vector2 goal = VillagerAgent.CellCenter(next) + _jitter;
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
            moved = true;
        }
        if (moved)
        {
            _bobTime += minutes * 0.6f;
            _body.Position = new Vector2(0, -Mathf.Abs(Mathf.Sin(_bobTime)) * 1.5f);
        }
        else
        {
            _body.Position = Vector2.Zero;
        }
        return _pathIndex >= _path.Count;
    }

    private void Face(Vector2 point)
    {
        if (Mathf.Abs(point.X - Position.X) > 0.5f)
        {
            _body.FlipH = point.X < Position.X;
        }
        _body.Position = Vector2.Zero;
    }

    public override void _Draw()
    {
        // 발밑 붉은 그림자 (적이라는 표시)
        DrawSetTransform(new Vector2(0, 1), 0, new Vector2(1, 0.5f));
        DrawCircle(Vector2.Zero, 5, new Color(0.6f, 0f, 0f, 0.35f));
        DrawSetTransform(Vector2.Zero, 0, Vector2.One);
        if (_mode == Mode.Gone || Hp >= MaxHp)
        {
            return;
        }
        // 체력 막대 (머리 위)
        var frame = new Rect2(-6, -17, 12, 3);
        DrawRect(frame, new Color(0.1f, 0.05f, 0.05f, 0.9f));
        float fill = 10f * Hp / MaxHp;
        DrawRect(new Rect2(-5, -16, fill, 1), new Color(0.95f, 0.25f, 0.2f));
    }
}
