// WorldSync.cs — 틱이 끝날 때마다 C 의 스냅숏을 읽어 화면을 맞춥니다.
//
// 상태는 이벤트가 아니라 스냅숏(요약, 주민·건물 뷰 전체)으로 맞춥니다 (SPEC §3.1 규칙 3):
//   - 뷰에 있는데 노드가 없으면 만들고, 있으면 고치고, 노드가 있는데 뷰에 없으면 지웁니다 (id 로).
// 이벤트 큐는 소리, 알림, 떠오르는 숫자 같은 피드백에만 씁니다. 이벤트를 놓쳐도 화면이 어긋나지 않습니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;
using MagicCastle.UI;

namespace MagicCastle.World;

public partial class WorldSync : Node
{
    private readonly List<CoreEventView> _events = new();
    private readonly Dictionary<int, VillagerAgent> _agents = new();
    private readonly Dictionary<int, BuildingNode> _buildingNodes = new();
    private readonly HashSet<int> _seen = new();
    private readonly List<int> _stale = new();
    private readonly string[] _buildingKeys = new string[(int)BuildingType.Count];
    private readonly Dictionary<int, int[]> _producedSince = new();
    private CoreVillagerView[] _villagers = new CoreVillagerView[64];
    private CoreBuildingView[] _buildings = new CoreBuildingView[64];
    private byte[] _cost = System.Array.Empty<byte>();
    private byte[] _roads = System.Array.Empty<byte>();
    private int _knownCostGridRev = -1;
    private double _floatTimer;

    public static WorldSync? Instance { get; private set; }

    public CoreSummaryView Summary { get; private set; }
    public Pathfinder Paths { get; } = new();
    /// <summary>적용 길찾기: 성벽·성문을 비싼 칸으로 지나감 (CombatDirector 가 씀)</summary>
    public Pathfinder EnemyPaths { get; } = new(forEnemies: true);
    public int VillagerCount { get; private set; }
    public int BuildingCount { get; private set; }

    /// <summary>Main 이 정해 주는 그림 부모 노드들</summary>
    public Node2D? UnitsLayer { get; set; }
    public Node2D? BuildingsLayer { get; set; }
    public MapView? Map { get; set; }
    public FloatingText? Floating { get; set; }

    /// <summary>보여 줄 알림 문장이 생겼을 때 (월드 위치가 있으면 함께)</summary>
    [Signal]
    public delegate void NotifyEventHandler(string text, Vector2 worldPosition, bool hasPosition);

    /// <summary>꺼낸 피드백 이벤트 하나하나 (소리, 화면 흔들기 같은 효과용)</summary>
    public event System.Action<CoreEventView>? EventPolled;

    /// <summary>스냅숏을 새로 맞춘 뒤 (검사창, 목록이 다시 그리도록)</summary>
    [Signal]
    public delegate void RefreshedEventHandler();

    public override void _EnterTree()
    {
        Instance = this;
    }

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
    }

    public System.ReadOnlySpan<CoreVillagerView> Villagers => new(_villagers, 0, VillagerCount);
    public System.ReadOnlySpan<CoreBuildingView> Buildings => new(_buildings, 0, BuildingCount);

    public string BuildingKey(int type) =>
        type >= 0 && type < _buildingKeys.Length ? _buildingKeys[type] ?? "" : "";

    public bool TryGetVillager(int id, out CoreVillagerView view)
    {
        for (int i = 0; i < VillagerCount; i++)
        {
            if (_villagers[i].Id == id)
            {
                view = _villagers[i];
                return true;
            }
        }
        view = default;
        return false;
    }

    public bool TryGetBuilding(int id, out CoreBuildingView view)
    {
        for (int i = 0; i < BuildingCount; i++)
        {
            if (_buildings[i].Id == id)
            {
                view = _buildings[i];
                return true;
            }
        }
        view = default;
        return false;
    }

    /// <summary>그 칸을 차지한 건물 id (없으면 0)</summary>
    public int BuildingAt(Vector2I tile)
    {
        for (int i = 0; i < BuildingCount; i++)
        {
            ref CoreBuildingView b = ref _buildings[i];
            if (tile.X >= b.X && tile.Y >= b.Y && tile.X < b.X + b.W && tile.Y < b.Y + b.H)
            {
                return b.Id;
            }
        }
        return 0;
    }

    public bool IsRoad(Vector2I tile)
    {
        int width = Summary.MapWidth;
        int index = tile.Y * width + tile.X;
        return tile.X >= 0 && tile.Y >= 0 && tile.X < width && index >= 0 && index < _roads.Length && _roads[index] != 0;
    }

    public IEnumerable<VillagerAgent> Agents => _agents.Values;

    public VillagerAgent? AgentFor(int id) => _agents.TryGetValue(id, out VillagerAgent? agent) ? agent : null;
    public BuildingNode? NodeFor(int id) => _buildingNodes.TryGetValue(id, out BuildingNode? node) ? node : null;

    /// <summary>마우스 근처의 주민 (없으면 0)</summary>
    public int VillagerNear(Vector2 worldPosition, float radius)
    {
        int best = 0;
        float bestDistance = radius * radius;
        foreach (VillagerAgent agent in _agents.Values)
        {
            if (!agent.Visible)
            {
                continue;
            }
            float distance = (agent.Position - worldPosition).LengthSquared();
            if (distance <= bestDistance)
            {
                best = agent.Id;
                bestDistance = distance;
            }
        }
        return best;
    }

    /// <summary>새 게임이나 불러오기 직후: 노드를 모두 지우고 처음부터 맞춥니다.</summary>
    public void Reset()
    {
        foreach (VillagerAgent agent in _agents.Values)
        {
            agent.QueueFree();
        }
        foreach (BuildingNode node in _buildingNodes.Values)
        {
            node.QueueFree();
        }
        _agents.Clear();
        _buildingNodes.Clear();
        _knownCostGridRev = -1;
        _events.Clear();
        _producedSince.Clear();
        for (int type = 0; type < _buildingKeys.Length; type++)
        {
            _buildingKeys[type] = CoreBridge.Instance.TryGetBuildingDef((BuildingType)type, out CoreBuildingDefView def)
                ? def.GetKey()
                : "";
        }
        Reconcile();
    }

    public void Reconcile()
    {
        CoreBridge core = CoreBridge.Instance;
        if (!core.TryGetSummary(out CoreSummaryView summary))
        {
            return;
        }
        Summary = summary;
        if (summary.CostGridRev != _knownCostGridRev)
        {
            RefreshGrids(summary);
        }
        BuildingCount = core.CopyBuildings(ref _buildings);
        VillagerCount = core.CopyVillagers(ref _villagers);
        ReconcileBuildings();
        ReconcileVillagers();
        DrainEvents();
        EmitSignal(SignalName.Refreshed);
    }

    private void RefreshGrids(in CoreSummaryView summary)
    {
        int cells = summary.MapWidth * summary.MapHeight;
        if (_cost.Length != cells)
        {
            _cost = new byte[cells];
            _roads = new byte[cells];
        }
        if (!CoreBridge.Instance.CopyCostGrid(_cost, out int revision).Ok)
        {
            return;
        }
        CoreBridge.Instance.CopyRoads(_roads);
        // AStarGrid2D 는 Rebuild 가 배열을 그대로 기억하므로 복사본을 넘깁니다
        Paths.Rebuild((byte[])_cost.Clone(), summary.MapWidth, summary.MapHeight, revision);
        EnemyPaths.Rebuild((byte[])_cost.Clone(), summary.MapWidth, summary.MapHeight, revision);
        Map?.SetRoads(_roads, summary.MapWidth, summary.MapHeight);
        // 지은 자리의 숲은 풀밭이 됨: 지형도 바뀐 칸만 다시 (비용 격자가 바뀔 때만이라 드묾)
        byte[]? terrain = CoreBridge.Instance.CopyTerrain(out _, out _);
        if (terrain != null)
        {
            Map?.UpdateTerrain(terrain);
        }
        _knownCostGridRev = revision;
    }

    private void ReconcileBuildings()
    {
        _seen.Clear();
        for (int i = 0; i < BuildingCount; i++)
        {
            ref CoreBuildingView view = ref _buildings[i];
            _seen.Add(view.Id);
            if (!_buildingNodes.TryGetValue(view.Id, out BuildingNode? node))
            {
                node = BuildingNode.Create(view);
                _buildingNodes[view.Id] = node;
                BuildingsLayer?.AddChild(node);
            }
            node.Sync(view, BuildingKey(view.Type));
        }
        RemoveUnseen(_buildingNodes);
    }

    private void ReconcileVillagers()
    {
        _seen.Clear();
        for (int i = 0; i < VillagerCount; i++)
        {
            ref CoreVillagerView view = ref _villagers[i];
            _seen.Add(view.Id);
            if (!_agents.TryGetValue(view.Id, out VillagerAgent? agent))
            {
                agent = VillagerAgent.Create(view);
                _agents[view.Id] = agent;
                UnitsLayer?.AddChild(agent);
            }
            agent.Sync(view, Paths, view.Job >= 0 ? BuildingKey(view.Job) : null);
        }
        RemoveUnseen(_agents);
    }

    private void RemoveUnseen<T>(Dictionary<int, T> nodes) where T : Node
    {
        _stale.Clear();
        foreach (int id in nodes.Keys)
        {
            if (!_seen.Contains(id))
            {
                _stale.Add(id);
            }
        }
        foreach (int id in _stale)
        {
            nodes[id].QueueFree();
            nodes.Remove(id);
        }
    }

    private void DrainEvents()
    {
        _events.Clear();
        CoreBridge.Instance.PollEvents(_events);
        foreach (CoreEventView coreEvent in _events)
        {
            if (coreEvent.Type == (int)CoreEventType.Produced)
            {
                AddProduced(coreEvent.A, coreEvent.B, coreEvent.C);
                continue;
            }
            if (coreEvent.Type is (int)CoreEventType.BuildingDamaged or (int)CoreEventType.BuildingRuined &&
                _buildingNodes.TryGetValue(coreEvent.A, out BuildingNode? hit))
            {
                hit.Flash();
            }
            EventPolled?.Invoke(coreEvent);
            // 습격 중에는 맞을 때마다 "부서졌습니다" 가 쏟아지므로 깜빡임만 보여 줍니다 (무너짐은 알림)
            if (coreEvent.Type == (int)CoreEventType.BuildingDamaged && Summary.RaidId != 0)
            {
                continue;
            }
            string? text = Strings.EventText(coreEvent);
            if (text == null)
            {
                continue;
            }
            Vector2 position = Vector2.Zero;
            bool hasPosition = EventPosition(coreEvent, ref position);
            EmitSignal(SignalName.Notify, text, position, hasPosition);
        }
    }

    /// <summary>알림을 누르면 카메라가 갈 곳</summary>
    private bool EventPosition(in CoreEventView coreEvent, ref Vector2 position)
    {
        switch ((CoreEventType)coreEvent.Type)
        {
            case CoreEventType.BuildingPlaced:
            case CoreEventType.BuildingCompleted:
            case CoreEventType.BuildingRemoved:
            case CoreEventType.BuildingDamaged:
            case CoreEventType.BuildingRuined:
            case CoreEventType.BuildingRepaired:
                position = new Vector2((coreEvent.C + 1) * Art.Tile, (coreEvent.D + 1) * Art.Tile);
                return true;
            case CoreEventType.LootStolen:
                if (_buildingNodes.TryGetValue(coreEvent.C, out BuildingNode? robbed))
                {
                    position = robbed.Position + new Vector2(robbed.View.W, robbed.View.H) * Art.Tile / 2f;
                    return true;
                }
                return false;
            case CoreEventType.ThreatScheduled:
            case CoreEventType.ThreatNear:
                position = EdgeCenter(coreEvent.B);
                return true;
            case CoreEventType.RaidStarted:
                position = EdgeCenter(coreEvent.D);
                return true;
            case CoreEventType.Immigrant:
            case CoreEventType.VillagerLeaving:
            case CoreEventType.PathBlocked:
            case CoreEventType.Story:
            case CoreEventType.VillagerSick:
            case CoreEventType.VillagerCured:
                if (_agents.TryGetValue(coreEvent.A, out VillagerAgent? agent))
                {
                    position = agent.Position;
                    return true;
                }
                return false;
            default:
                return false;
        }
    }

    /// <summary>지도 가장자리 가운데 (위협이 오는 방향)</summary>
    public Vector2 EdgeCenter(int direction)
    {
        int w = Summary.MapWidth;
        int h = Summary.MapHeight;
        Vector2I cell = (Direction)direction switch
        {
            Direction.North => new Vector2I(w / 2, 0),
            Direction.East => new Vector2I(w - 1, h / 2),
            Direction.South => new Vector2I(w / 2, h - 1),
            _ => new Vector2I(0, h / 2),
        };
        return VillagerAgent.CellCenter(cell);
    }

    // ---- 떠오르는 숫자: 같은 건물의 생산을 1초 동안 모아서 "+3 식량" 하나로 ----

    private void AddProduced(int buildingId, int resource, int amount)
    {
        if (resource < 0 || resource >= CoreSummaryView.ResourceCount)
        {
            return;
        }
        if (!_producedSince.TryGetValue(buildingId, out int[]? totals))
        {
            totals = new int[CoreSummaryView.ResourceCount];
            _producedSince[buildingId] = totals;
        }
        totals[resource] += amount;
    }

    public override void _Process(double delta)
    {
        _floatTimer += delta;
        if (_floatTimer < 1.0 || _producedSince.Count == 0)
        {
            return;
        }
        _floatTimer = 0;
        foreach (var (buildingId, totals) in _producedSince)
        {
            if (!_buildingNodes.TryGetValue(buildingId, out BuildingNode? node))
            {
                continue;
            }
            float offset = 0;
            for (int resource = 0; resource < totals.Length; resource++)
            {
                if (totals[resource] > 0)
                {
                    Vector2 top = node.Position + new Vector2(node.View.W * Art.Tile / 2f, offset);
                    Floating?.Show(top, Strings.Produced(resource, totals[resource]), new Color(0.95f, 1f, 0.75f));
                    offset -= 7;
                }
            }
        }
        _producedSince.Clear();
    }
}
