// BuildMode.cs — 지도 위의 마우스: 고르기, 건물 놓기(유령 그림 + 배치 미리보기), 줄 놓기(도로·성벽), 철거
//
// 놓을 수 있는지는 언제나 C 에 묻습니다(core_can_place). Godot 은 규칙을 모릅니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Audio;
using MagicCastle.Core;
using MagicCastle.UI;

namespace MagicCastle.World;

public enum ToolMode
{
    Select,
    Build,
    Demolish,
}

public partial class BuildMode : Node2D
{
    private const int MaxLineGhosts = 256;
    private static readonly Color Good = new(0.55f, 1f, 0.55f, 0.75f);
    private static readonly Color Bad = new(1f, 0.45f, 0.45f, 0.75f);
    private static readonly Color OverlayCounted = new(1f, 0.9f, 0.35f, 0.3f);
    private static readonly Color OverlayCountedEdge = new(1f, 0.92f, 0.4f, 0.9f);
    private static readonly Color OverlayRadius = new(1f, 0.95f, 0.6f, 0.8f);
    private static readonly Color OverlayDoor = new(0.5f, 0.85f, 1f, 0.9f);

    private readonly List<Sprite2D> _lineGhosts = new();
    private readonly List<Vector2I> _linePath = new();
    private Sprite2D _ghost = null!;
    private Vector2I _hoverTile = new(-9999, -9999);
    private Vector2I _dragStart;
    private bool _dragging;
    private CoreBuildingDefView _def;
    private Rect2I _highlight;
    private bool _highlightOn;
    private Rect2I _footprint;
    private Vector2I _door = new(-1, -1);
    private bool _footprintOn;

    /// <summary>작업 반경 안의 지형을 칠할 때 읽는 지도 (Main 이 넣어 줌)</summary>
    public MapView? Map { get; set; }

    /// <summary>마우스 아래 적의 이름·체력을 보여 주려고 (Main 이 넣어 줌)</summary>
    public CombatDirector? Combat { get; set; }

    private int _hoverEnemy;

    public static BuildMode? Instance { get; private set; }

    public ToolMode Mode { get; private set; } = ToolMode.Select;
    public BuildingType Type { get; private set; }
    public int SelectedVillager { get; private set; }
    public int SelectedBuilding { get; private set; }

    /// <summary>고른 대상이 바뀜 (주민 id, 건물 id; 없으면 0)</summary>
    [Signal]
    public delegate void SelectionChangedEventHandler(int villagerId, int buildingId);

    /// <summary>마우스 옆에 보여 줄 도움말 (빈 문자열이면 숨김)</summary>
    [Signal]
    public delegate void HintChangedEventHandler(string text);

    /// <summary>알림으로 보여 줄 문장</summary>
    [Signal]
    public delegate void MessageEventHandler(string text);

    [Signal]
    public delegate void ModeChangedEventHandler(int mode, int buildingType);

    public override void _EnterTree()
    {
        Instance = this;
    }

    public override void _Ready()
    {
        // 일시정지 중에도 짓고 고를 수 있어야 합니다
        ProcessMode = ProcessModeEnum.Always;
        ZIndex = 40;
        _ghost = new Sprite2D { Centered = false, Visible = false };
        AddChild(_ghost);
        for (int i = 0; i < MaxLineGhosts; i++)
        {
            var sprite = new Sprite2D { Centered = false, Visible = false };
            AddChild(sprite);
            _lineGhosts.Add(sprite);
        }
    }

    // ---- 모드 ------------------------------------------------------------

    public void BeginBuild(BuildingType type)
    {
        if (!CoreBridge.Instance.TryGetBuildingDef(type, out _def))
        {
            return;
        }
        Mode = ToolMode.Build;
        Type = type;
        _dragging = false;
        _ghost.Texture = type == BuildingType.Road ? RoadGhost() : Art.Building(_def.GetKey(), _def.W, _def.H);
        _hoverTile = new Vector2I(-9999, -9999);
        EmitSignal(SignalName.ModeChanged, (int)Mode, (int)type);
    }

    public void BeginDemolish()
    {
        Mode = ToolMode.Demolish;
        _dragging = false;
        _footprintOn = false;
        HideGhosts();
        _hoverTile = new Vector2I(-9999, -9999);
        EmitSignal(SignalName.ModeChanged, (int)Mode, -1);
    }

    public void Cancel()
    {
        Mode = ToolMode.Select;
        _footprintOn = false;
        _dragging = false;
        HideGhosts();
        _highlightOn = false;
        QueueRedraw();
        EmitSignal(SignalName.HintChanged, "");
        EmitSignal(SignalName.ModeChanged, (int)Mode, -1);
    }

    public void Select(int villagerId, int buildingId)
    {
        WorldSync? sync = WorldSync.Instance;
        if (sync != null)
        {
            if (sync.AgentFor(SelectedVillager) is VillagerAgent oldAgent)
            {
                oldAgent.Selected = false;
            }
            if (sync.NodeFor(SelectedBuilding) is BuildingNode oldNode)
            {
                oldNode.Selected = false;
            }
            if (sync.AgentFor(villagerId) is VillagerAgent agent)
            {
                agent.Selected = true;
            }
            if (sync.NodeFor(buildingId) is BuildingNode node)
            {
                node.Selected = true;
            }
        }
        SelectedVillager = villagerId;
        SelectedBuilding = buildingId;
        EmitSignal(SignalName.SelectionChanged, villagerId, buildingId);
    }

    private static Texture2D RoadGhost()
    {
        Texture2D? atlas = GD.Load<Texture2D>("res://assets/tiles/roads.png");
        return atlas != null
            ? new AtlasTexture { Atlas = atlas, Region = new Rect2(0, 0, Art.Tile, Art.Tile) }
            : Art.Construction(1, 1);
    }

    private void HideGhosts()
    {
        _ghost.Visible = false;
        foreach (Sprite2D sprite in _lineGhosts)
        {
            sprite.Visible = false;
        }
    }

    // ---- 마우스 ----------------------------------------------------------

    /// <summary>고르기 도구일 때 마우스 아래 적이 있으면 이름과 체력을 마우스 옆에 (적은 검사창이 없음)</summary>
    private void UpdateEnemyHover()
    {
        EnemyAgent? found = null;
        if (Combat != null && Combat.EnemyCount > 0)
        {
            Vector2 mouse = GetGlobalMousePosition();
            float best = 10f * 10f;
            foreach (EnemyAgent enemy in Combat.Enemies)
            {
                float distance = enemy.Position.DistanceSquaredTo(mouse + new Vector2(0, 6)); // 그림은 발밑보다 위
                if (distance < best)
                {
                    best = distance;
                    found = enemy;
                }
            }
        }
        if (found == null)
        {
            if (_hoverEnemy != 0)
            {
                _hoverEnemy = 0;
                EmitSignal(SignalName.HintChanged, "");
            }
            return;
        }
        _hoverEnemy = found.Id;
        EmitSignal(SignalName.HintChanged, Strings.EnemyTooltip(found.Type, found.Hp, found.MaxHp));
    }

    private Vector2I MouseTile()
    {
        Vector2 mouse = GetGlobalMousePosition();
        return new Vector2I(Mathf.FloorToInt(mouse.X / Art.Tile), Mathf.FloorToInt(mouse.Y / Art.Tile));
    }

    /// <summary>마우스가 건물 가운데에 오도록 한 왼쪽 위 칸</summary>
    private Vector2I Origin(Vector2I mouseTile) => mouseTile - new Vector2I((_def.W - 1) / 2, (_def.H - 1) / 2);

    public override void _Process(double delta)
    {
        if (!CoreBridge.Instance.HasWorld)
        {
            return;
        }
        if (Mode == ToolMode.Select)
        {
            UpdateEnemyHover();
            return;
        }
        Vector2I tile = MouseTile();
        if (tile == _hoverTile)
        {
            return;
        }
        _hoverTile = tile;
        if (Mode == ToolMode.Build)
        {
            UpdateBuildPreview(tile);
        }
        else if (Mode == ToolMode.Demolish)
        {
            UpdateDemolishPreview(tile);
        }
    }

    private void UpdateBuildPreview(Vector2I tile)
    {
        if (_def.Line != 0 && _dragging)
        {
            _footprintOn = false;
            _ghost.Visible = false;
            UpdateLinePreview(_dragStart, tile);
            return;
        }
        Vector2I origin = _def.Line != 0 ? tile : Origin(tile);
        CorePlacementView view = CoreBridge.Instance.CanPlace(Type, origin);
        _footprint = new Rect2I(origin, new Vector2I(_def.W, _def.H));
        _door = new Vector2I(view.DoorX, view.DoorY);
        _footprintOn = _def.Line == 0;
        QueueRedraw();
        _ghost.Visible = true;
        _ghost.Position = new Vector2(origin.X * Art.Tile, origin.Y * Art.Tile);
        _ghost.Modulate = view.Result == (int)PlaceResult.Ok ? Good : Bad;
        string hint = Strings.PlacementHint(view, Type) + "\n" + (_def.Line != 0 ? Strings.LineModeHint : Strings.BuildModeHint);
        EmitSignal(SignalName.HintChanged, hint);
    }

    /// <summary>C 와 같은 방식(가로 먼저, 그다음 세로)으로 줄의 칸을 구합니다</summary>
    private void LinePath(Vector2I from, Vector2I to)
    {
        _linePath.Clear();
        Vector2I cell = from;
        while (_linePath.Count < MaxLineGhosts)
        {
            _linePath.Add(cell);
            if (cell == to)
            {
                break;
            }
            if (cell.X != to.X)
            {
                cell.X += to.X > cell.X ? 1 : -1;
            }
            else
            {
                cell.Y += to.Y > cell.Y ? 1 : -1;
            }
        }
    }

    private void UpdateLinePreview(Vector2I from, Vector2I to)
    {
        LinePath(from, to);
        int good = 0;
        for (int i = 0; i < _lineGhosts.Count; i++)
        {
            Sprite2D sprite = _lineGhosts[i];
            if (i >= _linePath.Count)
            {
                sprite.Visible = false;
                continue;
            }
            CorePlacementView view = CoreBridge.Instance.CanPlace(Type, _linePath[i]);
            bool ok = view.Result == (int)PlaceResult.Ok || view.Result == (int)PlaceResult.AlreadyThere;
            good += view.Result == (int)PlaceResult.Ok ? 1 : 0;
            sprite.Texture = _ghost.Texture;
            sprite.Position = new Vector2(_linePath[i].X * Art.Tile, _linePath[i].Y * Art.Tile);
            sprite.Modulate = ok ? Good : Bad;
            sprite.Visible = true;
        }
        EmitSignal(SignalName.HintChanged, $"{Strings.BuildingName(Type)} {good}칸\n{Strings.Cost(_def)} × {good}");
    }

    private void UpdateDemolishPreview(Vector2I tile)
    {
        WorldSync? sync = WorldSync.Instance;
        _highlightOn = false;
        string hint = Strings.DemolishModeHint;
        if (sync != null)
        {
            int id = sync.BuildingAt(tile);
            if (id != 0 && sync.TryGetBuilding(id, out CoreBuildingView building) && building.Type != (int)BuildingType.Keep)
            {
                _highlight = new Rect2I(building.X, building.Y, building.W, building.H);
                _highlightOn = true;
                hint = $"{Strings.BuildingName(building.Type)} 철거 (비용의 절반 환불)";
            }
            else if (sync.IsRoad(tile))
            {
                _highlight = new Rect2I(tile.X, tile.Y, 1, 1);
                _highlightOn = true;
                hint = $"{Strings.BuildingName(BuildingType.Road)} 철거";
            }
        }
        EmitSignal(SignalName.HintChanged, hint);
        QueueRedraw();
    }

    public override void _Draw()
    {
        if (Mode == ToolMode.Build && _footprintOn)
        {
            DrawPlacementOverlay();
        }
        if (Mode == ToolMode.Demolish && _highlightOn)
        {
            var rect = new Rect2(_highlight.Position * Art.Tile, _highlight.Size * Art.Tile);
            DrawRect(rect, new Color(1f, 0.3f, 0.3f, 0.35f));
            DrawRect(rect, new Color(1f, 0.4f, 0.4f), false, 1f);
        }
    }

    /// <summary>
    /// 건설 미리보기 겹쳐 그리기: 작업 반경(점선 테두리), 그 안에서 생산성에 세는 지형 칸(노란 칸),
    /// 주민이 드나드는 문 칸(작은 네모). 어떤 지형을 세는지는 C 가 정의에 담아 줍니다(count_terrain).
    /// </summary>
    private void DrawPlacementOverlay()
    {
        int radius = _def.WorkRadius;
        if (radius > 0 && _def.CountTerrain >= 0 && Map != null)
        {
            var area = new Rect2I(_footprint.Position - new Vector2I(radius, radius), _footprint.Size + new Vector2I(radius * 2, radius * 2));
            for (int y = area.Position.Y; y < area.End.Y; y++)
            {
                for (int x = area.Position.X; x < area.End.X; x++)
                {
                    var cell = new Vector2I(x, y);
                    bool inside = _footprint.HasPoint(cell);
                    if (Map.TerrainAt(cell) != _def.CountTerrain || (inside && _def.CountTerrain != (int)Terrain.Mana))
                    {
                        continue;
                    }
                    var tileRect = new Rect2(cell * Art.Tile + new Vector2(1, 1), new Vector2(Art.Tile - 2, Art.Tile - 2));
                    DrawRect(tileRect, OverlayCounted);
                    DrawRect(tileRect, OverlayCountedEdge, false, 1f);
                }
            }
            DrawDashedRect(new Rect2(area.Position * Art.Tile, area.Size * Art.Tile), OverlayRadius);
        }
        if (_door.X >= 0 && _door.Y >= 0)
        {
            DrawRect(new Rect2(_door * Art.Tile + new Vector2(5, 5), new Vector2(Art.Tile - 10, Art.Tile - 10)), OverlayDoor);
        }
    }

    private void DrawDashedRect(Rect2 rect, Color color)
    {
        Vector2 a = rect.Position;
        Vector2 b = rect.Position + new Vector2(rect.Size.X, 0);
        Vector2 c = rect.End;
        Vector2 d = rect.Position + new Vector2(0, rect.Size.Y);
        DrawDashedLine(a, b, color, 1f, 4f);
        DrawDashedLine(b, c, color, 1f, 4f);
        DrawDashedLine(c, d, color, 1f, 4f);
        DrawDashedLine(d, a, color, 1f, 4f);
    }

    public override void _UnhandledInput(InputEvent inputEvent)
    {
        if (!CoreBridge.Instance.HasWorld)
        {
            return;
        }
        if (inputEvent.IsActionPressed("ui_cancel") ||
            inputEvent is InputEventMouseButton { ButtonIndex: MouseButton.Right, Pressed: true })
        {
            if (Mode != ToolMode.Select)
            {
                Cancel();
            }
            else
            {
                Select(0, 0);
            }
            GetViewport().SetInputAsHandled();
            return;
        }
        if (inputEvent.IsActionPressed("tool_demolish"))
        {
            BeginDemolish();
            GetViewport().SetInputAsHandled();
            return;
        }
        if (inputEvent is not InputEventMouseButton { ButtonIndex: MouseButton.Left } click)
        {
            return;
        }
        Vector2I tile = MouseTile();
        switch (Mode)
        {
            case ToolMode.Select when click.Pressed:
                ClickSelect(tile);
                break;
            case ToolMode.Build:
                ClickBuild(tile, click.Pressed);
                break;
            case ToolMode.Demolish when click.Pressed:
                ClickDemolish(tile);
                break;
        }
        GetViewport().SetInputAsHandled();
    }

    private void ClickSelect(Vector2I tile)
    {
        WorldSync? sync = WorldSync.Instance;
        if (sync == null)
        {
            return;
        }
        int villager = sync.VillagerNear(GetGlobalMousePosition(), 8f);
        if (villager != 0)
        {
            Select(villager, 0);
            return;
        }
        Select(0, sync.BuildingAt(tile));
    }

    private void ClickBuild(Vector2I tile, bool pressed)
    {
        if (_def.Line != 0)
        {
            if (pressed)
            {
                _dragging = true;
                _dragStart = tile;
                UpdateLinePreview(tile, tile);
                return;
            }
            if (!_dragging)
            {
                return;
            }
            _dragging = false;
            HideGhosts();
            CoreBridge.Instance.PlaceLine(Type, _dragStart, tile, out int placed, out PlaceResult reason);
            if (placed == 0)
            {
                Sfx.Instance?.Play(Sound.Denied);
            }
            EmitSignal(SignalName.Message, placed > 0 ? Strings.LinePlaced(Type, placed) : Strings.BuildRejected(reason));
            _hoverTile = new Vector2I(-9999, -9999);
            return;
        }
        if (!pressed)
        {
            return;
        }
        CoreResult result = CoreBridge.Instance.PlaceBuilding(Type, Origin(tile), out int id, out PlaceResult why);
        if (!result.Ok)
        {
            Sfx.Instance?.Play(Sound.Denied);
            EmitSignal(SignalName.Message, Strings.BuildRejected(why));
        }
        _ = id;
        _hoverTile = new Vector2I(-9999, -9999); // 자원이 줄었으니 미리보기를 다시
    }

    private void ClickDemolish(Vector2I tile)
    {
        WorldSync? sync = WorldSync.Instance;
        if (sync == null)
        {
            return;
        }
        int id = sync.BuildingAt(tile);
        if (id != 0)
        {
            CoreResult result = CoreBridge.Instance.Demolish(id);
            if (!result.Ok)
            {
                Sfx.Instance?.Play(Sound.Denied);
                EmitSignal(SignalName.Message, result.Message);
            }
        }
        else if (sync.IsRoad(tile))
        {
            CoreBridge.Instance.RemoveRoad(tile);
        }
        _highlightOn = false;
        _hoverTile = new Vector2I(-9999, -9999);
        QueueRedraw();
    }
}
