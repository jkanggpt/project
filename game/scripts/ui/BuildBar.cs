// BuildBar.cs — 아래쪽 건설 막대: 분류 탭 + 건물 단추 (툴팁: 비용, 일자리, 못 짓는 이유)
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;
using MagicCastle.World;

namespace MagicCastle.UI;

public partial class BuildBar : PanelContainer
{
    private static readonly BuildCategory[] Categories =
    {
        BuildCategory.Housing, BuildCategory.Resource, BuildCategory.Storage, BuildCategory.Defense,
        BuildCategory.Service, BuildCategory.Road,
    };

    private readonly List<(BuildingType Type, Button Button)> _buttons = new();
    private readonly List<Button> _categoryButtons = new();
    private HBoxContainer _buildingRow = null!;
    private BuildCategory _current = BuildCategory.Housing;
    private int _scale = 1;

    [Signal]
    public delegate void VillagerListRequestedEventHandler();

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        var row = new HBoxContainer();
        AddChild(row);
        var categoryGroup = new ButtonGroup();
        foreach (BuildCategory category in Categories)
        {
            BuildCategory captured = category;
            var button = new Button
            {
                Text = Strings.CategoryName(category),
                ToggleMode = true,
                ButtonGroup = categoryGroup,
                FocusMode = FocusModeEnum.None,
                ButtonPressed = category == _current,
            };
            button.Pressed += () => SelectCategory(captured);
            row.AddChild(button);
            _categoryButtons.Add(button);
        }
        row.AddChild(new VSeparator());
        _buildingRow = new HBoxContainer();
        row.AddChild(_buildingRow);
        row.AddChild(new Control { SizeFlagsHorizontal = SizeFlags.ExpandFill, MouseFilter = MouseFilterEnum.Ignore });
        var demolish = new Button { Text = Strings.DemolishTool, TooltipText = Strings.DemolishTooltip, FocusMode = FocusModeEnum.None };
        demolish.Pressed += () => BuildMode.Instance?.BeginDemolish();
        row.AddChild(demolish);
        var villagers = new Button { Text = Strings.VillagerListButton, TooltipText = Strings.VillagerListTooltip, FocusMode = FocusModeEnum.None };
        villagers.Pressed += () => EmitSignal(SignalName.VillagerListRequested);
        row.AddChild(villagers);
    }

    public void ApplyScale(int scale)
    {
        _scale = scale;
        foreach (var (_, button) in _buttons)
        {
            button.AddThemeConstantOverride("icon_max_width", 16 * scale);
        }
    }

    private void SelectCategory(BuildCategory category)
    {
        _current = category;
        Rebuild();
    }

    /// <summary>지금 분류의 건물 단추를 다시 만듭니다 (새 게임, 분류 바꿀 때)</summary>
    public void Rebuild()
    {
        foreach (var (_, button) in _buttons)
        {
            button.QueueFree();
        }
        _buttons.Clear();
        for (int type = 1; type < (int)BuildingType.Count; type++)
        {
            if (!CoreBridge.Instance.TryGetBuildingDef((BuildingType)type, out CoreBuildingDefView def) ||
                def.Category != (int)_current)
            {
                continue;
            }
            var buildingType = (BuildingType)type;
            var button = new Button
            {
                Text = Strings.BuildingName(buildingType),
                Icon = buildingType == BuildingType.Road ? null : Art.Building(def.GetKey(), def.W, def.H),
                FocusMode = FocusModeEnum.None,
                ExpandIcon = false,
            };
            button.AddThemeConstantOverride("icon_max_width", 16 * _scale);
            button.Pressed += () => BuildMode.Instance?.BeginBuild(buildingType);
            _buildingRow.AddChild(button);
            _buttons.Add((buildingType, button));
        }
        Refresh(WorldSync.Instance?.Summary ?? default);
    }

    /// <summary>자원이 바뀔 때마다: 툴팁과 "지을 수 있는지" 표시</summary>
    public void Refresh(in CoreSummaryView summary)
    {
        foreach (var (type, button) in _buttons)
        {
            if (!CoreBridge.Instance.TryGetBuildingDef(type, out CoreBuildingDefView def))
            {
                continue;
            }
            string? reason = null;
            if (def.Available == 0)
            {
                reason = Strings.RankRequired(def.RequiredRank);
            }
            else if (!CanAfford(def, summary))
            {
                reason = Strings.PlaceResultText(PlaceResult.CannotAfford);
            }
            button.Disabled = def.Available == 0;
            button.Modulate = reason == null ? Colors.White : new Color(1, 1, 1, 0.55f);
            button.TooltipText = Strings.BuildTooltip(def, reason);
        }
    }

    private static bool CanAfford(in CoreBuildingDefView def, in CoreSummaryView summary)
    {
        for (int i = 0; i < CoreSummaryView.ResourceCount; i++)
        {
            if (def.GetCost((ResourceKind)i) > summary.GetResource((ResourceKind)i))
            {
                return false;
            }
        }
        return true;
    }
}
