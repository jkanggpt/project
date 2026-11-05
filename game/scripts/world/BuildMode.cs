// BuildMode.cs — 지도 위의 마우스: 고르기, 건물 놓기(유령 그림 + 배치 미리보기), 줄 놓기(도로·성벽), 철거
//
// 놓을 수 있는지는 언제나 C 에 묻습니다(core_can_place). Godot 은 규칙을 모릅니다.
using System.Collections.Generic;
using Godot;
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

    private readonly List<Sprite2D> _lineGhosts = new();
    private readonly List<Vector2I> _linePath = new();
    private Sprite2D _ghost = null!;
    private Vector2I _hoverTile = new(-9999, -9999);
    private Vector2I _dragStart;
    private bool _dragging;
    private CoreBuildingDefView _def;
    private Rect2I _highlight;
    private bool _highlightOn;

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
        HideGhosts();
        _hoverTile = new Vector2I(-9999, -9999);
        EmitSignal(SignalName.ModeChanged, (int)Mode, -1);
    }

    public void Cancel()
    {
        Mode = ToolMode.Select;
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

    private Vector2I MouseTile()
    {
        Vector2 mouse = GetGlobalMousePosition();
        return new Vector2I(Mathf.FloorToInt(mouse.X / Art.Tile), Mathf.FloorToInt(mouse.Y / Art.Tile));
    }

    /// <summary>마우스가 건물 가운데에 오도록 한 왼쪽 위 칸</summary>
    private Vector2I Origin(Vector2I mouseTile) => mouseTile - new Vector2I((_def.W - 1) / 2, (_def.H - 1) / 2);

    public override void _Process(double delta)
    {
        if (!CoreBridge.Instance.HasWorld || Mode == ToolMode.Select)
        {
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
            _ghost.Visible = false;
            UpdateLinePreview(_dragStart, tile);
            return;
        }
        Vector2I origin = _def.Line != 0 ? tile : Origin(tile);
        CorePlacementView view = CoreBridge.Instance.CanPlace(Type, origin);
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
        if (Mode == ToolMode.Demolish && _highlightOn)
        {
            var rect = new Rect2(_highlight.Position * Art.Tile, _highlight.Size * Art.Tile);
            DrawRect(rect, new Color(1f, 0.3f, 0.3f, 0.35f));
            DrawRect(rect, new Color(1f, 0.4f, 0.4f), false, 1f);
        }
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
