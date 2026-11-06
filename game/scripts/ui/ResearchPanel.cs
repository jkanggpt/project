// ResearchPanel.cs — 연구 창: 갈래 셋(경제·방어·마법)을 세로 줄로, 연구마다 단추 하나
//
// 연구 목록·비용·상태·진행은 모두 C 의 연구 뷰(core_get_research)에서 읽고, 단추는 CoreBridge.StartResearch 만 부릅니다.
// 연구가 몇 개인지는 정의 파일이 정하므로(최대 32개) 창을 열 때마다 0 번부터 읽어 다시 만듭니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;
using MagicCastle.World;

namespace MagicCastle.UI;

public partial class ResearchPanel : PanelContainer
{
    private readonly List<(int Index, Button Button, ProgressBar Bar)> _nodes = new();
    private readonly VBoxContainer[] _columns = new VBoxContainer[(int)ResearchBranch.Count];
    private HBoxContainer _branches = null!;
    private Label _status = null!;
    private int _builtCount = -1;
    private int _scale = 1;

    /// <summary>시작 결과 문장 (알림으로)</summary>
    [Signal]
    public delegate void MessageEventHandler(string text);

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        Visible = false;
        var box = new VBoxContainer();
        AddChild(box);
        var header = new HBoxContainer();
        header.AddChild(new Label { Text = Strings.ResearchTitle, SizeFlagsHorizontal = SizeFlags.ExpandFill });
        var close = new Button { Text = "×", TooltipText = Strings.Close, FocusMode = FocusModeEnum.None };
        close.Pressed += () => Visible = false;
        header.AddChild(close);
        box.AddChild(header);
        _status = new Label { AutowrapMode = TextServer.AutowrapMode.WordSmart };
        box.AddChild(_status);
        _branches = new HBoxContainer();
        box.AddChild(_branches);
        for (int branch = 0; branch < _columns.Length; branch++)
        {
            var column = new VBoxContainer { SizeFlagsHorizontal = SizeFlags.ExpandFill };
            column.AddChild(new Label { Text = Strings.BranchName(branch), HorizontalAlignment = HorizontalAlignment.Center });
            _branches.AddChild(column);
            _columns[branch] = column;
        }
        box.AddChild(new Label
        {
            Text = Strings.ResearchHelp,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            Modulate = new Color(1, 1, 1, 0.75f),
        });
    }

    public void ApplyScale(int scale)
    {
        _scale = scale;
        CustomMinimumSize = new Vector2(430 * scale, 0);
        _branches.AddThemeConstantOverride("separation", 6 * scale);
        foreach (var (_, button, bar) in _nodes)
        {
            button.CustomMinimumSize = new Vector2(100 * scale, 30 * scale);
            bar.CustomMinimumSize = new Vector2(0, 3 * scale);
        }
    }

    public void Toggle()
    {
        Visible = !Visible;
        Refresh();
    }

    public void Open()
    {
        Visible = true;
        Refresh();
    }

    /// <summary>연구 정의의 개수가 바뀌었으면(새 게임·불러오기) 단추를 다시 만듭니다</summary>
    private void BuildNodes()
    {
        int count = 0;
        while (CoreBridge.Instance.TryGetResearch(count, out _))
        {
            count++;
        }
        if (count == _builtCount)
        {
            return;
        }
        foreach (var (_, button, bar) in _nodes)
        {
            button.GetParent().QueueFree();
        }
        _nodes.Clear();
        for (int i = 0; i < count; i++)
        {
            if (!CoreBridge.Instance.TryGetResearch(i, out CoreResearchView view) || view.Branch < 0 ||
                view.Branch >= _columns.Length)
            {
                continue;
            }
            int index = i;
            var cell = new VBoxContainer();
            var button = new Button
            {
                FocusMode = FocusModeEnum.None,
                AutowrapMode = TextServer.AutowrapMode.WordSmart,
                CustomMinimumSize = new Vector2(100 * _scale, 30 * _scale),
            };
            button.Pressed += () => Start(index);
            var bar = new ProgressBar { ShowPercentage = false, CustomMinimumSize = new Vector2(0, 3 * _scale) };
            cell.AddChild(button);
            cell.AddChild(bar);
            _columns[view.Branch].AddChild(cell);
            _nodes.Add((index, button, bar));
        }
        _builtCount = count;
    }

    private void Start(int index)
    {
        CoreResult result = CoreBridge.Instance.StartResearch(index, out DenyReason reason);
        if (!result.Ok)
        {
            EmitSignal(SignalName.Message, Strings.ResearchRejected(reason));
        }
        Refresh();
    }

    public void Refresh()
    {
        if (!Visible)
        {
            return;
        }
        BuildNodes();
        CoreSummaryView summary = WorldSync.Instance?.Summary ?? default;
        _status.Text = Strings.ResearchStatus(summary);
        foreach (var (index, button, bar) in _nodes)
        {
            if (!CoreBridge.Instance.TryGetResearch(index, out CoreResearchView view))
            {
                continue;
            }
            string? requires = view.Requires >= 0 && CoreBridge.Instance.TryGetResearch(view.Requires, out CoreResearchView before)
                ? before.GetKey()
                : null;
            var state = (ResearchState)view.State;
            button.Text = Strings.ResearchNodeText(view);
            button.TooltipText = Strings.ResearchTooltip(view, requires);
            button.Disabled = state != ResearchState.Available || summary.ResearchActive >= 0 ||
                              summary.GetResource(ResourceKind.Gold) < view.CostGold ||
                              summary.GetResource(ResourceKind.Mana) < view.CostMana;
            button.Modulate = state switch
            {
                ResearchState.Done => new Color(0.65f, 1f, 0.65f),
                ResearchState.Active => new Color(1f, 0.95f, 0.6f),
                ResearchState.Locked => new Color(1f, 1f, 1f, 0.55f),
                _ => Colors.White,
            };
            bar.Visible = state == ResearchState.Active;
            bar.MaxValue = System.Math.Max(1, view.Points);
            bar.Value = view.Progress;
        }
    }
}
