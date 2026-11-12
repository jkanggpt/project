// Inspector.cs — 오른쪽 검사창: 고른 건물이나 주민의 자세한 정보와 명령 단추
//
// 숫자는 모두 WorldSync 의 스냅숏(C 의 뷰)에서 읽고, 단추는 CoreBridge 명령만 부릅니다.
using System.Collections.Generic;
using System.Linq;
using Godot;
using MagicCastle.Audio;
using MagicCastle.Core;
using MagicCastle.World;

namespace MagicCastle.UI;

public partial class Inspector : PanelContainer
{
    private const int MaxRows = 8;

    private readonly List<(Label Name, Button Remove)> _workerRows = new();
    private readonly List<int> _idleIds = new();
    private readonly List<int> _newIdleIds = new();
    private Label _title = null!;
    private Label _lines = null!;
    private Label _mood = null!;
    private VBoxContainer _buildingBox = null!;
    private VBoxContainer _villagerBox = null!;
    private CheckBox _autoAssign = null!;
    private OptionButton _idlePicker = null!;
    private Button _addWorker = null!;
    private Button _demolish = null!;
    private Button _repair = null!;
    private Button _trade = null!;
    private Button _release = null!;
    private Button _fire = null!;
    private int _villagerId;
    private int _buildingId;

    /// <summary>시장 창을 열어 달라는 요청 (HUD 가 받음)</summary>
    [Signal]
    public delegate void TradeRequestedEventHandler();

    /// <summary>알림으로 보여 줄 문장</summary>
    [Signal]
    public delegate void MessageEventHandler(string text);

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        Visible = false;
        var box = new VBoxContainer();
        AddChild(box);
        var header = new HBoxContainer();
        box.AddChild(header);
        _title = new Label { SizeFlagsHorizontal = SizeFlags.ExpandFill };
        header.AddChild(_title);
        var close = new Button { Text = "×", TooltipText = Strings.Close, FocusMode = FocusModeEnum.None };
        close.Pressed += () => BuildMode.Instance?.Select(0, 0);
        header.AddChild(close);
        _lines = new Label { AutowrapMode = TextServer.AutowrapMode.WordSmart };
        box.AddChild(_lines);
        _mood = new Label { MouseFilter = MouseFilterEnum.Pass };
        box.AddChild(_mood);

        // 건물 부분
        _buildingBox = new VBoxContainer();
        box.AddChild(_buildingBox);
        for (int i = 0; i < MaxRows; i++)
        {
            var row = new HBoxContainer();
            var name = new Label { SizeFlagsHorizontal = SizeFlags.ExpandFill, ClipText = true };
            var remove = new Button { Text = Strings.RemoveWorker, TooltipText = Strings.RemoveWorkerTooltip, FocusMode = FocusModeEnum.None };
            int index = i;
            remove.Pressed += () => RemoveWorker(index);
            row.AddChild(name);
            row.AddChild(remove);
            _buildingBox.AddChild(row);
            _workerRows.Add((name, remove));
        }
        var assignRow = new HBoxContainer();
        _idlePicker = new OptionButton { SizeFlagsHorizontal = SizeFlags.ExpandFill, FocusMode = FocusModeEnum.None, ClipText = true };
        _addWorker = new Button { Text = Strings.AddWorker, TooltipText = Strings.AddWorkerTooltip, FocusMode = FocusModeEnum.None };
        _addWorker.Pressed += AddWorker;
        assignRow.AddChild(_idlePicker);
        assignRow.AddChild(_addWorker);
        _buildingBox.AddChild(assignRow);
        _autoAssign = new CheckBox { Text = Strings.AutoAssign, TooltipText = Strings.AutoAssignTooltip, FocusMode = FocusModeEnum.None };
        _autoAssign.Toggled += on => CoreBridge.Instance.SetAutoAssign(_buildingId, on);
        _buildingBox.AddChild(_autoAssign);
        _demolish = new Button { Text = Strings.Demolish, TooltipText = Strings.DemolishTooltip, FocusMode = FocusModeEnum.None };
        _demolish.Pressed += () =>
        {
            CoreBridge.Instance.Demolish(_buildingId);
            BuildMode.Instance?.Select(0, 0);
        };
        _buildingBox.AddChild(_demolish);
        var actions = new HBoxContainer();
        _repair = new Button { Text = Strings.Repair, FocusMode = FocusModeEnum.None, SizeFlagsHorizontal = SizeFlags.ExpandFill };
        _repair.Pressed += () =>
        {
            CoreResult result = CoreBridge.Instance.Repair(_buildingId, out DenyReason reason);
            if (!result.Ok)
            {
                Sfx.Instance?.Play(Sound.Denied);
                EmitSignal(SignalName.Message, Strings.RepairRejected(reason));
            }
        };
        _trade = new Button { Text = Strings.TradeButton, FocusMode = FocusModeEnum.None, SizeFlagsHorizontal = SizeFlags.ExpandFill };
        _trade.Pressed += () => EmitSignal(SignalName.TradeRequested);
        actions.AddChild(_repair);
        actions.AddChild(_trade);
        _buildingBox.AddChild(actions);
        _buildingBox.MoveChild(actions, _buildingBox.GetChildCount() - 2); // 철거 단추 위에

        // 주민 부분
        _villagerBox = new VBoxContainer();
        box.AddChild(_villagerBox);
        _release = new Button { Text = Strings.ReleaseWorker, TooltipText = Strings.ReleaseWorkerTooltip, FocusMode = FocusModeEnum.None };
        _release.Pressed += () => CoreBridge.Instance.ReleaseWorker(_villagerId);
        _villagerBox.AddChild(_release);
        _fire = new Button { Text = Strings.RemoveWorker, TooltipText = Strings.RemoveWorkerTooltip, FocusMode = FocusModeEnum.None };
        _fire.Pressed += () => CoreBridge.Instance.UnassignWorker(_villagerId);
        _villagerBox.AddChild(_fire);
    }

    public void ApplyScale(int scale)
    {
        CustomMinimumSize = new Vector2(170 * scale, 0);
    }

    public void ShowSelection(int villagerId, int buildingId)
    {
        _villagerId = villagerId;
        _buildingId = buildingId;
        Refresh();
    }

    public void Refresh()
    {
        WorldSync? sync = WorldSync.Instance;
        if (sync != null && _villagerId != 0 && sync.TryGetVillager(_villagerId, out CoreVillagerView villager))
        {
            ShowVillager(villager);
        }
        else if (sync != null && _buildingId != 0 && sync.TryGetBuilding(_buildingId, out CoreBuildingView building))
        {
            ShowBuilding(sync, building);
        }
        else
        {
            Visible = false; // 고른 대상이 사라짐 (철거, 떠남)
        }
    }

    private void ShowVillager(in CoreVillagerView v)
    {
        Visible = true;
        _buildingBox.Visible = false;
        _villagerBox.Visible = true;
        _title.Text = Strings.VillagerTitle(v);
        string traitA = CoreBridge.Instance.TryGetTraitDef(v.TraitA, out CoreTraitDefView a) ? a.GetKey() : "";
        string traitB = CoreBridge.Instance.TryGetTraitDef(v.TraitB, out CoreTraitDefView b) ? b.GetKey() : "";
        string state = v.Has(VillagerFlags.Sick) ? Strings.SickLine : Strings.VillagerStateText(v.State);
        _lines.Text = string.Join("\n", state, Strings.JobLine(v.Job),
            Strings.HomeLine(v.HomeId != 0), Strings.Traits(traitA, traitB), Strings.Stats(v), Strings.Needs(v));
        _mood.Visible = true;
        _mood.Text = Strings.MoodLine(v);
        _mood.TooltipText = Strings.MoodTooltip(CoreBridge.Instance.GetMoodTerms(v.Id));
        _release.Visible = v.Has(VillagerFlags.Manual);
        _fire.Visible = v.WorkplaceId != 0;
    }

    private void ShowBuilding(WorldSync sync, in CoreBuildingView b)
    {
        Visible = true;
        _buildingBox.Visible = true;
        _villagerBox.Visible = false;
        _mood.Visible = false;
        _title.Text = Strings.BuildingName(b.Type);
        var lines = new List<string>
        {
            b.State == (int)BuildingState.Constructing ? Strings.Progress(b.BuildProgress, b.BuildTotal)
            : b.State == (int)BuildingState.Ruined ? Strings.RuinedLine
            : Strings.BuildingStateText(b.State),
        };
        lines.Add(Strings.Hp(b.Hp, b.MaxHp));
        if ((b.Flags & (int)BuildingFlags.Cold) != 0)
        {
            lines.Add(Strings.ColdHouseLine);
        }
        if (b.Type == (int)BuildingType.Healer)
        {
            lines.Add(Strings.HealerLine(sync.Summary.GetResource(ResourceKind.Herbs)));
        }
        if (b.Type == (int)BuildingType.MageTower)
        {
            lines.Add(sync.Summary.ResearchActive >= 0 && CoreBridge.Instance.TryGetResearch(sync.Summary.ResearchActive, out CoreResearchView research)
                ? Strings.ResearchShort(research.GetKey(), sync.Summary.ResearchProgress, sync.Summary.ResearchPoints)
                : Strings.MageTowerIdle);
        }
        if (b.WorkerSlots > 0)
        {
            lines.Add(Strings.Workers(b.Workers, b.WorkerSlots));
            if (b.Workers == 0 && b.State == (int)BuildingState.Active)
            {
                lines.Add(Strings.NoWorkersHint);
            }
            lines.Add(Strings.Productivity(b.Productivity));
        }
        if (b.ResidentSlots > 0)
        {
            lines.Add(Strings.Residents(b.Residents, b.ResidentSlots));
        }
        _lines.Text = string.Join("\n", lines);

        // 일꾼 줄 (또는 거주민)
        int row = 0;
        foreach (CoreVillagerView v in sync.Villagers)
        {
            bool worker = b.WorkerSlots > 0 && v.WorkplaceId == b.Id;
            bool resident = b.ResidentSlots > 0 && v.HomeId == b.Id;
            if ((worker || resident) && row < MaxRows)
            {
                var (name, remove) = _workerRows[row];
                name.Text = Strings.WorkerLine(v.GetName(), v.State);
                name.SetMeta("villager", v.Id);
                remove.Visible = worker;
                name.GetParent<Control>().Visible = true;
                row++;
            }
        }
        for (; row < MaxRows; row++)
        {
            _workerRows[row].Name.GetParent<Control>().Visible = false;
        }
        bool hasJobs = b.WorkerSlots > 0 && b.State == (int)BuildingState.Active;
        _idlePicker.GetParent<Control>().Visible = hasJobs;
        _autoAssign.Visible = b.WorkerSlots > 0;
        _autoAssign.SetPressedNoSignal(b.AutoAssign != 0);
        _demolish.Visible = b.Type != (int)BuildingType.Keep;
        bool damaged = b.State != (int)BuildingState.Constructing && b.Hp < b.MaxHp;
        _repair.Visible = damaged;
        if (damaged)
        {
            int[] cost = CoreBridge.Instance.GetRepairCost(b.Id);
            bool raid = sync.Summary.RaidId != 0;
            _repair.TooltipText = raid ? Strings.RepairDuringRaid : Strings.RepairTooltip(cost);
            _repair.Disabled = raid || !CanAfford(sync.Summary, cost);
        }
        _trade.Visible = b.Type == (int)BuildingType.Market && b.State == (int)BuildingState.Active;
        _repair.GetParent<Control>().Visible = _repair.Visible || _trade.Visible;
        if (hasJobs)
        {
            FillIdlePicker(sync);
        }
    }

    private static bool CanAfford(in CoreSummaryView summary, int[] cost)
    {
        for (int i = 0; i < cost.Length; i++)
        {
            if (summary.GetResource((ResourceKind)i) < cost[i])
            {
                return false;
            }
        }
        return true;
    }

    private void FillIdlePicker(WorldSync sync)
    {
        // 틱마다 다시 채우므로, 플레이어가 골라 둔 주민(id)을 기억했다가 다시 고릅니다.
        // 목록이 그대로면 아예 건드리지 않습니다 (펼친 목록이 닫히지 않게).
        _newIdleIds.Clear();
        foreach (CoreVillagerView v in sync.Villagers)
        {
            if (v.WorkplaceId == 0 && v.State != (int)VillagerState.Leaving)
            {
                _newIdleIds.Add(v.Id);
            }
        }
        if (_newIdleIds.Count > 0 && _newIdleIds.SequenceEqual(_idleIds))
        {
            return;
        }
        int selectedId = _idlePicker.Selected >= 0 && _idlePicker.Selected < _idleIds.Count ? _idleIds[_idlePicker.Selected] : 0;
        _idleIds.Clear();
        _idlePicker.Clear();
        foreach (CoreVillagerView v in sync.Villagers)
        {
            if (_newIdleIds.Contains(v.Id))
            {
                _idlePicker.AddItem(v.GetName());
                _idleIds.Add(v.Id);
            }
        }
        if (_idleIds.Count == 0)
        {
            _idlePicker.AddItem(Strings.NoIdleVillagers);
        }
        int keep = _idleIds.IndexOf(selectedId);
        _idlePicker.Select(keep >= 0 ? keep : 0);
        _addWorker.Disabled = _idleIds.Count == 0;
    }

    private void AddWorker()
    {
        int index = _idlePicker.Selected;
        if (index >= 0 && index < _idleIds.Count)
        {
            CoreBridge.Instance.AssignWorker(_idleIds[index], _buildingId);
        }
    }

    private void RemoveWorker(int row)
    {
        Label name = _workerRows[row].Name;
        if (name.HasMeta("villager"))
        {
            CoreBridge.Instance.UnassignWorker(name.GetMeta("villager").AsInt32());
        }
    }
}
