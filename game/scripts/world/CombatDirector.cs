// CombatDirector.cs — 습격의 Godot 쪽: 적을 내보내고, 감시탑·성채·경비병이 싸우고, 결과를 C 에 알립니다.
//
// 습격 핸드셰이크 (SPEC §3.3)
//   1. C 가 습격을 만들면(core_get_raid 의 id 가 바뀜) 적을 지도 가장자리(spawn)에 내보냅니다.
//   2. 싸움은 여기서 풉니다. 물리 엔진은 쓰지 않습니다: 적은 AStarGrid2D 길을 따라 걷고,
//      감시탑·성채 망루·경비병은 공격 사이마다 거리 안의 가장 가까운 적을 노립니다.
//      화살은 그림만 날아가고, 피해는 화살이 닿을 때 정합니다.
//   3. 결과는 core_report_* 로 알립니다: 건물 피해, 주민 피해, 처치, 약탈, 달아남, 끝.
//      C 가 규칙대로 적용합니다 (체력, 무너짐, 죽음, 마정석, 빼앗긴 금화). 마지막 적이 사라지면 C 가 습격을 끝냅니다.
//   4. C 가 습격을 끝내면(시간 초과 포함) 남은 적은 물러납니다.
//
// 수치(피해, 거리, 공격 사이, 체력, 속도)는 모두 C 의 정의에서 읽습니다. 여기의 상수는 싸움을 "어떻게 보여 줄지"와
// 적·경비병이 무엇을 노릴지 고르는 방식(시야, 다시 고르는 간격, 화살 속도)뿐입니다.
//
// 시간: Step(분) 이 게임 속 분만큼 싸움을 진행합니다. 보통은 _Process 가 실제 시간 × 배속으로 부르고,
// 스모크 테스트는 ManualStep 을 켜고 직접 부릅니다 (빠르고, 같은 규칙).
using System.Collections.Generic;
using Godot;
using MagicCastle.Audio;
using MagicCastle.Core;
using MagicCastle.UI;

namespace MagicCastle.World;

public partial class CombatDirector : Node2D
{
    /// <summary>적이 목표를 다시 고르는 간격 (게임 속 분)</summary>
    public const float RetargetMinutes = 30f;
    /// <summary>마물이 주민을 알아채는 거리 (칸)</summary>
    public const float MonsterSightTiles = 8f;
    /// <summary>경비병이 자기 병영에서 이만큼 떨어진 적까지 쫓아감 (칸)</summary>
    public const float GuardLeashTiles = 8f;
    /// <summary>화살이 한 칸 나는 데 걸리는 게임 속 분 (보통 배속에서 한 칸에 0.05초)</summary>
    private const float ArrowMinutesPerTile = 1.0f;
    /// <summary>한 번에 진행하는 가장 긴 시간 (프레임이 끊겨도 적이 성벽을 뚫고 지나가지 않게)</summary>
    private const float MaxSubstepMinutes = 2f;

    private sealed class Projectile
    {
        public Sprite2D Sprite = null!;
        public Vector2 From;
        public Vector2 To;
        public float Elapsed;
        public float Duration;
        public EnemyAgent? Target;
        public int Damage;
        public int VillagerId;      // 적 궁수가 경비병을 쏜 경우
        public int BuildingId;      // 적 궁수가 건물을 쏜 경우
    }

    private sealed class Guard
    {
        public int VillagerId;
        public Vector2I Post;
        public float Cooldown;
        public float Repath;
        public int TargetId;
        public readonly List<Vector2I> Path = new();
        public int PathIndex;
    }

    private readonly Dictionary<int, EnemyAgent> _enemies = new();
    private readonly HashSet<int> _gone = new();            // 이번 습격에서 처치·달아남을 이미 알린 적
    private readonly Dictionary<int, float> _towerCooldown = new();
    private readonly Dictionary<int, Guard> _guards = new();
    private readonly List<Projectile> _projectiles = new();
    private readonly Stack<Sprite2D> _arrowPool = new();
    private readonly List<int> _scratch = new();
    private readonly List<EnemyAgent> _stepList = new();
    private CoreEnemyView[] _enemyViews = new CoreEnemyView[64];
    private CoreEnemyDefView[] _enemyDefs = System.Array.Empty<CoreEnemyDefView>();
    private readonly CoreBuildingDefView[] _buildingDefs = new CoreBuildingDefView[(int)BuildingType.Count];
    private Node2D _projectileLayer = null!;
    private int _raidId;
    private int _gridRevision = -1;
    private int _researchDone = -1; // 연구가 끝나면 건물 정의(감시탑·경비병·성채 공격)가 바뀌므로 다시 읽음

    public WorldSync Sync { get; set; } = null!;
    public Node2D? UnitsLayer { get; set; }
    public CameraRig? Camera { get; set; }

    /// <summary>켜면 _Process 가 시간을 진행하지 않음 (스모크 테스트가 Step 을 직접 부름)</summary>
    public bool ManualStep { get; set; }

    public CoreRaidView Raid { get; private set; }
    public int EnemyCount => _enemies.Count;
    public int ActiveProjectiles => _projectiles.Count;

    /// <summary>날아가는 중간쯤인 화살이 있는가 (스크린숏을 찍기 좋은 때)</summary>
    public bool ArrowMidFlight
    {
        get
        {
            foreach (Projectile projectile in _projectiles)
            {
                float t = projectile.Elapsed / projectile.Duration;
                if (t > 0.3f && t < 0.7f)
                {
                    return true;
                }
            }
            return false;
        }
    }

    /// <summary>이번 습격에서 감시탑·성채·경비병이 쓰러뜨린 적 (스모크 테스트 기록용)</summary>
    public int KillsThisRaid { get; private set; }

    // 이번 습격의 기록 (로그로 남겨 성벽·경비병이 실제로 싸웠는지 확인)
    private int _arrowsFired;
    private int _guardStrikes;
    private int _barrierHits;
    private int _loots;

    public IEnumerable<EnemyAgent> Enemies => _enemies.Values;

    public override void _Ready()
    {
        _projectileLayer = new Node2D { Name = "Projectiles", ZIndex = 5 };
        AddChild(_projectileLayer);
    }

    /// <summary>새 게임·불러오기: 정의를 다시 읽고 남은 적을 모두 지웁니다 (WorldSync.Reset 보다 먼저)</summary>
    public void Reset()
    {
        Clear();
        var defs = new List<CoreEnemyDefView>();
        for (int type = 0; CoreBridge.Instance.TryGetEnemyDef(type, out CoreEnemyDefView def); type++)
        {
            defs.Add(def);
        }
        _enemyDefs = defs.ToArray();
        ReloadBuildingDefs();
        _gridRevision = -1;
        _researchDone = -1;
    }

    private void ReloadBuildingDefs()
    {
        for (int type = 0; type < _buildingDefs.Length; type++)
        {
            CoreBridge.Instance.TryGetBuildingDef((BuildingType)type, out _buildingDefs[type]);
        }
    }

    private void Clear()
    {
        foreach (EnemyAgent enemy in _enemies.Values)
        {
            enemy.QueueFree();
        }
        _enemies.Clear();
        _gone.Clear();
        ReleaseGuards();
        foreach (Projectile projectile in _projectiles)
        {
            Recycle(projectile.Sprite);
        }
        _projectiles.Clear();
        _towerCooldown.Clear();
        _raidId = 0;
        Raid = default;
    }

    // ---- 스냅숏 맞추기 (틱마다, WorldSync.Refreshed 뒤) -------------------------

    public void OnRefreshed()
    {
        CoreBridge core = CoreBridge.Instance;
        if (Sync.Summary.ResearchDone != _researchDone)
        {
            _researchDone = Sync.Summary.ResearchDone;
            ReloadBuildingDefs(); // "날카로운 화살" 같은 연구가 공격 수치를 바꿈
        }
        if (!core.TryGetRaid(out CoreRaidView raid) || raid.Id == 0)
        {
            if (_raidId != 0)
            {
                EndRaid();
            }
            Raid = default;
            return;
        }
        if (raid.Id != _raidId)
        {
            Clear();
            _raidId = raid.Id;
            KillsThisRaid = 0;
            _arrowsFired = 0;
            _guardStrikes = 0;
            _barrierHits = 0;
            _loots = 0;
            GD.Print($"raid {raid.Id}: {Strings.ThreatName(raid.Type)} x{raid.EnemyCount} from ({raid.SpawnX},{raid.SpawnY})");
        }
        Raid = raid;
        int count = core.CopyEnemies(ref _enemyViews);
        for (int i = 0; i < count; i++)
        {
            ref CoreEnemyView view = ref _enemyViews[i];
            bool alive = view.State == (int)EnemyState.Alive;
            if (alive && !_enemies.ContainsKey(view.Id) && !_gone.Contains(view.Id))
            {
                Spawn(view, raid, i);
            }
            else if (!alive && _enemies.TryGetValue(view.Id, out EnemyAgent? stale))
            {
                // C 가 먼저 정리함 (예: 다른 경로로 처치·달아남이 알려짐)
                _enemies.Remove(view.Id);
                stale.Vanish(view.State == (int)EnemyState.Killed);
            }
        }
        SyncGuards();
        if (_enemies.Count == 0)
        {
            // 살아 있다는 적이 있는데 내보낼 수 없음 (정의가 없는 종류 등): 습격을 끝냄
            core.ReportRaidEnded(_raidId);
        }
    }

    private void Spawn(in CoreEnemyView view, in CoreRaidView raid, int index)
    {
        if (view.Type < 0 || view.Type >= _enemyDefs.Length || UnitsLayer == null)
        {
            _gone.Add(view.Id);
            CoreBridge.Instance.ReportEnemyEscaped(_raidId, view.Id);
            return;
        }
        var home = new Vector2I(raid.SpawnX, raid.SpawnY);
        // 가장자리를 따라 조금씩 벌려 세움 (한 칸에 모두 겹치지 않게)
        int spread = (index % 5) - 2;
        Vector2I along = raid.Direction is (int)Direction.North or (int)Direction.South ? new Vector2I(spread, 0) : new Vector2I(0, spread);
        Vector2I spawn = Sync.EnemyPaths.Walkable(home + along) ? home + along : home;
        EnemyAgent agent = EnemyAgent.Create(view.Id, _enemyDefs[view.Type], spawn, home);
        _enemies[view.Id] = agent;
        UnitsLayer.AddChild(agent);
    }

    private void EndRaid()
    {
        GD.Print($"raid {_raidId} over: {KillsThisRaid} killed by defenders, arrows {_arrowsFired}, guard strikes {_guardStrikes}," +
                 $" wall/gate hits {_barrierHits}, loots {_loots}");
        foreach (EnemyAgent enemy in _enemies.Values)
        {
            enemy.Vanish(false); // 남은 적은 물러남 (C 가 달아난 것으로 셈)
        }
        _enemies.Clear();
        _gone.Clear();
        ReleaseGuards();
        _raidId = 0;
    }

    private void SyncGuards()
    {
        _scratch.Clear();
        foreach (CoreVillagerView villager in Sync.Villagers)
        {
            if (villager.State != (int)VillagerState.GuardPost || villager.AtTarget == 0)
            {
                continue;
            }
            _scratch.Add(villager.Id);
            if (!_guards.ContainsKey(villager.Id) && Sync.AgentFor(villager.Id) is VillagerAgent agent)
            {
                agent.BeginCombat();
                _guards[villager.Id] = new Guard { VillagerId = villager.Id, Post = new Vector2I(villager.LocX, villager.LocY) };
            }
        }
        var released = new List<int>();
        foreach (int id in _guards.Keys)
        {
            if (!_scratch.Contains(id))
            {
                released.Add(id);
            }
        }
        foreach (int id in released)
        {
            Sync.AgentFor(id)?.EndCombat(Sync.Paths);
            _guards.Remove(id);
        }
    }

    private void ReleaseGuards()
    {
        foreach (int id in _guards.Keys)
        {
            Sync.AgentFor(id)?.EndCombat(Sync.Paths);
        }
        _guards.Clear();
    }

    // ---- 시간 진행 -----------------------------------------------------------

    public override void _Process(double delta)
    {
        if (ManualStep || _raidId == 0 && _projectiles.Count == 0)
        {
            return;
        }
        CoreConfigView config = CoreBridge.Instance.Config;
        float secondsPerTick = System.Math.Max(1, config.RealMsPerTick) / 1000f;
        float minutes = (float)delta / secondsPerTick * config.MinutesPerTick * GameClock.Instance.Speed;
        Step(minutes);
    }

    /// <summary>게임 속 minutes 분만큼 싸움을 진행합니다</summary>
    public void Step(float minutes)
    {
        while (minutes > 0)
        {
            float slice = System.Math.Min(minutes, MaxSubstepMinutes);
            StepOnce(slice);
            minutes -= slice;
        }
    }

    private void StepOnce(float minutes)
    {
        StepProjectiles(minutes);
        if (_raidId == 0 || !CoreBridge.Instance.HasWorld)
        {
            return;
        }
        if (Sync.EnemyPaths.Revision != _gridRevision)
        {
            _gridRevision = Sync.EnemyPaths.Revision;
            foreach (EnemyAgent enemy in _enemies.Values)
            {
                enemy.Repath();
            }
        }
        // Step 안에서 적이 사라질 수 있으므로 목록을 복사해서 돕니다
        _stepList.Clear();
        _stepList.AddRange(_enemies.Values);
        foreach (EnemyAgent enemy in _stepList)
        {
            if (_raidId == 0)
            {
                return; // 마지막 적이 사라져 C 가 습격을 끝냄
            }
            enemy.Step(this, minutes);
        }
        StepTowers(minutes);
        StepGuards(minutes);
    }

    // ---- 감시탑, 성채 망루 ------------------------------------------------------

    private void StepTowers(float minutes)
    {
        foreach (CoreBuildingView building in Sync.Buildings)
        {
            if (building.State != (int)BuildingState.Active ||
                building.Type != (int)BuildingType.Tower && building.Type != (int)BuildingType.Keep)
            {
                continue;
            }
            CoreBuildingDefView def = _buildingDefs[building.Type];
            if (def.Attack <= 0 || def.AttackCooldown <= 0)
            {
                continue;
            }
            float cooldown = _towerCooldown.TryGetValue(building.Id, out float left) ? left - minutes : 0;
            if (cooldown > 0)
            {
                _towerCooldown[building.Id] = cooldown;
                continue;
            }
            Vector2 center = RectCenter(building);
            EnemyAgent? target = NearestEnemy(center, def.AttackRange + building.W / 2f, needsDamage: true);
            if (target == null)
            {
                _towerCooldown[building.Id] = 0;
                continue;
            }
            _towerCooldown[building.Id] = def.AttackCooldown;
            Vector2 top = new(center.X, building.Y * Art.Tile + 4);
            FireArrow(top, target, def.Attack);
        }
    }

    private EnemyAgent? NearestEnemy(Vector2 from, float rangeTiles, bool needsDamage)
    {
        EnemyAgent? best = null;
        float bestDistance = rangeTiles * Art.Tile * (rangeTiles * Art.Tile);
        foreach (EnemyAgent enemy in _enemies.Values)
        {
            if (!enemy.Alive || needsDamage && enemy.IncomingDamage >= enemy.Hp)
            {
                continue;
            }
            float distance = (enemy.Position - from).LengthSquared();
            if (distance <= bestDistance)
            {
                best = enemy;
                bestDistance = distance;
            }
        }
        return best;
    }

    // ---- 경비병 -------------------------------------------------------------

    private void StepGuards(float minutes)
    {
        CoreBuildingDefView def = _buildingDefs[(int)BuildingType.Barracks];
        CoreConfigView config = CoreBridge.Instance.Config;
        foreach (Guard guard in _guards.Values)
        {
            if (Sync.AgentFor(guard.VillagerId) is not VillagerAgent agent || !agent.CombatControlled)
            {
                continue;
            }
            guard.Cooldown = System.Math.Max(0, guard.Cooldown - minutes);
            guard.Repath -= minutes;
            Vector2 post = VillagerAgent.CellCenter(guard.Post);
            EnemyAgent? enemy = _enemies.TryGetValue(guard.TargetId, out EnemyAgent? current) && current.Alive &&
                                current.Position.DistanceTo(post) <= GuardLeashTiles * Art.Tile
                ? current
                : NearestEnemy(post, GuardLeashTiles, needsDamage: false);
            if (enemy != null && enemy.Id != guard.TargetId)
            {
                guard.TargetId = enemy.Id;
                guard.Repath = 0;
            }
            float reach = System.Math.Max(1, def.AttackRange) + 0.4f;
            if (enemy != null && agent.Position.DistanceTo(enemy.Position) <= reach * Art.Tile)
            {
                agent.CombatMove(agent.Position, 0);
                if (guard.Cooldown <= 0 && def.Attack > 0)
                {
                    guard.Cooldown = System.Math.Max(1, def.AttackCooldown);
                    _guardStrikes++;
                    Sfx.Instance?.Play(Sound.Strike);
                    if (enemy.TakeDamage(def.Attack))
                    {
                        Kill(enemy);
                    }
                }
                continue;
            }
            Vector2I goal = enemy?.CurrentCell ?? guard.Post;
            if (guard.Repath <= 0 || guard.PathIndex >= guard.Path.Count)
            {
                guard.Repath = 10f;
                if (!Sync.Paths.Walkable(goal) && !Sync.Paths.TryNearestWalkable(goal, out goal) ||
                    !Sync.Paths.FindPath(agent.CurrentCell, goal, guard.Path))
                {
                    guard.Path.Clear();
                }
                guard.PathIndex = 0;
            }
            float tilesPerMinute = agent.View.WalkSpeed / 1000f / System.Math.Max(1, config.MinutesPerTick);
            float step = tilesPerMinute * Art.Tile * minutes;
            Vector2 position = agent.Position;
            while (step > 0 && guard.PathIndex < guard.Path.Count)
            {
                Vector2 target = VillagerAgent.CellCenter(guard.Path[guard.PathIndex]);
                Vector2 offset = target - position;
                float distance = offset.Length();
                if (distance <= step)
                {
                    position = target;
                    step -= distance;
                    guard.PathIndex++;
                }
                else
                {
                    position += offset / distance * step;
                    step = 0;
                }
            }
            agent.CombatMove(position, minutes * 0.05f);
        }
    }

    // ---- 적이 부르는 질문과 보고 ------------------------------------------------

    public bool TryGetBuilding(int id, out CoreBuildingView view) => Sync.TryGetBuilding(id, out view);

    public static bool IsStorage(in CoreBuildingView b) =>
        b.Type == (int)BuildingType.Keep || b.Type == (int)BuildingType.Warehouse;

    /// <summary>이 적이 노릴 만한 건물인가 (행동에 따라)</summary>
    public bool Attackable(in CoreBuildingView b, EnemyAgent enemy)
    {
        if (b.State != (int)BuildingState.Active || b.Type is (int)BuildingType.Road or (int)BuildingType.Wall or (int)BuildingType.Gate)
        {
            return false;
        }
        bool canLoot = IsStorage(b) && !enemy.Looted;
        return enemy.Behavior switch
        {
            EnemyBehavior.Thief => canLoot,
            EnemyBehavior.Monster => canLoot || b.Type == (int)BuildingType.House,
            // 병사는 무엇이든 부숨 (성채가 무너지면 패배)
            _ => true,
        };
    }

    /// <summary>목표 건물 고르기: 가까운 순, 병사는 성채에 가까운 건물을 더 좋아함. 없으면 0</summary>
    public int ChooseBuilding(EnemyAgent enemy, EnemyBehavior behavior, HashSet<int> unreachable)
    {
        CoreSummaryView summary = Sync.Summary;
        Vector2 keep = new((summary.KeepX + summary.KeepSize / 2f) * Art.Tile, (summary.KeepY + summary.KeepSize / 2f) * Art.Tile);
        int best = 0;
        float bestScore = float.MaxValue;
        foreach (CoreBuildingView b in Sync.Buildings)
        {
            if (unreachable.Contains(b.Id) || !Attackable(b, enemy))
            {
                continue;
            }
            Vector2 center = RectCenter(b);
            float score = enemy.Position.DistanceTo(center);
            if (behavior == EnemyBehavior.Raider)
            {
                score += 0.5f * center.DistanceTo(keep);
            }
            if (score < bestScore)
            {
                bestScore = score;
                best = b.Id;
            }
        }
        return best;
    }

    /// <summary>건물 옆의 서 있을 칸 (적에게 가장 가까운 칸)</summary>
    public Vector2I ApproachCell(in CoreBuildingView b, Vector2 from)
    {
        Pathfinder paths = Sync.EnemyPaths;
        Vector2I best = new(b.DoorX, b.DoorY);
        float bestDistance = float.MaxValue;
        for (int y = b.Y - 1; y <= b.Y + b.H; y++)
        {
            for (int x = b.X - 1; x <= b.X + b.W; x++)
            {
                bool edge = x == b.X - 1 || x == b.X + b.W || y == b.Y - 1 || y == b.Y + b.H;
                var cell = new Vector2I(x, y);
                if (!edge || !paths.Walkable(cell) || Pathfinder.IsBarrier(paths.CostAt(cell)))
                {
                    continue;
                }
                float distance = VillagerAgent.CellCenter(cell).DistanceSquaredTo(from);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = cell;
                }
            }
        }
        return best;
    }

    public static Vector2 RectCenter(in CoreBuildingView b) =>
        new((b.X + b.W / 2f) * Art.Tile, (b.Y + b.H / 2f) * Art.Tile);

    /// <summary>점에서 건물 사각형까지의 거리 (칸)</summary>
    public static float DistanceToRect(Vector2 point, in CoreBuildingView b)
    {
        float px = point.X / Art.Tile;
        float py = point.Y / Art.Tile;
        float dx = System.Math.Max(System.Math.Max(b.X - px, 0), px - (b.X + b.W));
        float dy = System.Math.Max(System.Math.Max(b.Y - py, 0), py - (b.Y + b.H));
        return Mathf.Sqrt(dx * dx + dy * dy);
    }

    /// <summary>거리 안의 경비병 (includeCivilians 면 밖에 나와 있는 주민도)</summary>
    public VillagerAgent? VillagerInReach(Vector2 from, float reachTiles, bool includeCivilians)
    {
        VillagerAgent? best = null;
        float bestDistance = reachTiles * Art.Tile * (reachTiles * Art.Tile);
        foreach (VillagerAgent agent in Sync.Agents)
        {
            if (!(agent.CombatControlled || includeCivilians && agent.Exposed))
            {
                continue;
            }
            float distance = (agent.Position - from).LengthSquared();
            if (distance <= bestDistance)
            {
                best = agent;
                bestDistance = distance;
            }
        }
        return best;
    }

    public VillagerAgent? NearestExposedVillager(Vector2 from, float sightTiles, HashSet<int> unreachable)
    {
        VillagerAgent? best = null;
        float bestDistance = sightTiles * Art.Tile * (sightTiles * Art.Tile);
        foreach (VillagerAgent agent in Sync.Agents)
        {
            if (!agent.Exposed || unreachable.Contains(agent.Id))
            {
                continue;
            }
            float distance = (agent.Position - from).LengthSquared();
            if (distance <= bestDistance)
            {
                best = agent;
                bestDistance = distance;
            }
        }
        return best;
    }

    public void StrikeBuilding(EnemyAgent enemy, in CoreBuildingView building, int damage)
    {
        if (enemy.RangeTiles > 1.5f)
        {
            FireAt(enemy.Position + new Vector2(0, -6), RectCenter(building), damage, 0, building.Id);
            return;
        }
        HitBuilding(building.Id, damage);
    }

    private void HitBuilding(int buildingId, int damage)
    {
        if (Sync.TryGetBuilding(buildingId, out CoreBuildingView hit) && hit.Type is (int)BuildingType.Wall or (int)BuildingType.Gate)
        {
            _barrierHits++;
        }
        CoreBridge.Instance.ReportBuildingDamage(_raidId, buildingId, damage);
        Sync.NodeFor(buildingId)?.Flash();
        Sfx.Instance?.Play(Sound.WallHit);
    }

    public void StrikeVillager(EnemyAgent enemy, VillagerAgent villager, int damage)
    {
        if (enemy.RangeTiles > 1.5f)
        {
            FireAt(enemy.Position + new Vector2(0, -6), villager.Position + new Vector2(0, -4), damage, villager.Id, 0);
            return;
        }
        HitVillager(villager.Id, damage);
    }

    private void HitVillager(int villagerId, int damage)
    {
        CoreBridge.Instance.ReportVillagerDamage(_raidId, villagerId, damage);
        Sync.AgentFor(villagerId)?.Hit();
        Sfx.Instance?.Play(Sound.Strike);
    }

    public void Loot(EnemyAgent enemy, in CoreBuildingView building)
    {
        CoreBridge.Instance.ReportLoot(_raidId, enemy.Id, building.Id, out int amount);
        _loots++;
        if (amount > 0 && enemy.Type >= 0 && enemy.Type < _enemyDefs.Length)
        {
            Vector2 top = RectCenter(building) - new Vector2(0, building.H * Art.Tile / 2f);
            Sync.Floating?.Show(top, $"-{amount} {Strings.ResourceName(_enemyDefs[enemy.Type].LootResource)}", new Color(1f, 0.55f, 0.45f));
            Sfx.Instance?.Play(Sound.Loot);
        }
    }

    public void Escaped(EnemyAgent enemy)
    {
        if (!_enemies.Remove(enemy.Id))
        {
            return;
        }
        _gone.Add(enemy.Id);
        enemy.Vanish(false);
        CoreBridge.Instance.ReportEnemyEscaped(_raidId, enemy.Id);
    }

    private void Kill(EnemyAgent enemy)
    {
        if (!_enemies.Remove(enemy.Id))
        {
            return;
        }
        _gone.Add(enemy.Id);
        KillsThisRaid++;
        enemy.Vanish(true);
        Sfx.Instance?.Play(Sound.EnemyDown);
        int mana = enemy.Type >= 0 && enemy.Type < _enemyDefs.Length ? _enemyDefs[enemy.Type].ManaReward : 0;
        if (mana > 0)
        {
            Sync.Floating?.Show(enemy.Position + new Vector2(0, -12), $"+{mana} {Strings.ResourceName((int)ResourceKind.Mana)}",
                new Color(0.7f, 0.85f, 1f));
        }
        // 마지막 적이면 C 가 이 안에서 습격을 끝냄 (다음 틱의 스냅숏에서 정리)
        CoreBridge.Instance.ReportEnemyKilled(_raidId, enemy.Id);
    }

    // ---- 화살 (그림만 날아가고, 닿을 때 피해) -------------------------------------

    private void FireArrow(Vector2 from, EnemyAgent target, int damage)
    {
        target.IncomingDamage += damage;
        _arrowsFired++;
        Projectile projectile = Launch(from, target.Position + new Vector2(0, -4));
        projectile.Target = target;
        projectile.Damage = damage;
        Sfx.Instance?.Play(Sound.ArrowShot);
    }

    private void FireAt(Vector2 from, Vector2 to, int damage, int villagerId, int buildingId)
    {
        Projectile projectile = Launch(from, to);
        projectile.Damage = damage;
        projectile.VillagerId = villagerId;
        projectile.BuildingId = buildingId;
    }

    private Projectile Launch(Vector2 from, Vector2 to)
    {
        Sprite2D sprite = _arrowPool.Count > 0 ? _arrowPool.Pop() : CreateArrow();
        sprite.Visible = true;
        sprite.Position = from;
        sprite.Rotation = (to - from).Angle();
        var projectile = new Projectile
        {
            Sprite = sprite,
            From = from,
            To = to,
            Duration = System.Math.Max(1f, from.DistanceTo(to) / Art.Tile * ArrowMinutesPerTile),
        };
        _projectiles.Add(projectile);
        return projectile;
    }

    private Sprite2D CreateArrow()
    {
        var sprite = new Sprite2D { Texture = Art.Arrow() };
        _projectileLayer.AddChild(sprite);
        return sprite;
    }

    private void Recycle(Sprite2D sprite)
    {
        sprite.Visible = false;
        _arrowPool.Push(sprite);
    }

    private void StepProjectiles(float minutes)
    {
        for (int i = _projectiles.Count - 1; i >= 0; i--)
        {
            Projectile p = _projectiles[i];
            if (p.Target != null && p.Target.Alive)
            {
                p.To = p.Target.Position + new Vector2(0, -4); // 움직이는 적을 따라감 (빗나가지 않음)
            }
            p.Elapsed += minutes;
            float t = System.Math.Min(1f, p.Elapsed / p.Duration);
            float arc = Mathf.Sin(t * Mathf.Pi) * 6f;
            Vector2 position = p.From.Lerp(p.To, t) - new Vector2(0, arc);
            Vector2 ahead = p.From.Lerp(p.To, System.Math.Min(1f, t + 0.05f)) - new Vector2(0, Mathf.Sin(System.Math.Min(1f, t + 0.05f) * Mathf.Pi) * 6f);
            p.Sprite.Position = position;
            if ((ahead - position).LengthSquared() > 0.001f)
            {
                p.Sprite.Rotation = (ahead - position).Angle();
            }
            if (t < 1f)
            {
                continue;
            }
            _projectiles.RemoveAt(i);
            Recycle(p.Sprite);
            Land(p);
        }
    }

    private void Land(Projectile p)
    {
        if (p.Target != null)
        {
            p.Target.IncomingDamage = System.Math.Max(0, p.Target.IncomingDamage - p.Damage);
            if (_raidId != 0 && _enemies.ContainsKey(p.Target.Id))
            {
                Sfx.Instance?.Play(Sound.ArrowHit);
                if (p.Target.TakeDamage(p.Damage))
                {
                    Kill(p.Target);
                }
            }
            return;
        }
        if (_raidId == 0)
        {
            return;
        }
        if (p.VillagerId != 0)
        {
            HitVillager(p.VillagerId, p.Damage);
        }
        else if (p.BuildingId != 0)
        {
            HitBuilding(p.BuildingId, p.Damage);
        }
    }

    // ---- 피드백 이벤트 (소리, 화면 흔들기) ----------------------------------------

    public void OnCoreEvent(CoreEventView coreEvent)
    {
        switch ((CoreEventType)coreEvent.Type)
        {
            case CoreEventType.ThreatNear:
            case CoreEventType.RaidStarted:
                Sfx.Instance?.Play(Sound.Alarm);
                break;
            case CoreEventType.RaidEnded:
                Sfx.Instance?.Play(Sound.Victory);
                break;
            case CoreEventType.BuildingRuined:
                Sfx.Instance?.Play(Sound.Collapse);
                if (coreEvent.B is (int)BuildingType.Wall or (int)BuildingType.Gate)
                {
                    Camera?.Shake();
                }
                break;
        }
    }
}
