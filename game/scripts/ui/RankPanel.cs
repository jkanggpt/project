// RankPanel.cs — 등급 창: 다음 등급의 조건(인구·기분·방어·연구)과 공물, 오르면 생기는 것, [승급] 단추
//
// 조건과 지금 값은 모두 C 의 등급 뷰(core_get_rank)에서 읽고, 단추는 CoreBridge.RankUp 만 부릅니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.UI;

public partial class RankPanel : PanelContainer
{
    private readonly List<Label> _requirements = new();
    private Label _current = null!;
    private Label _next = null!;
    private Label _tribute = null!;
    private Label _rewards = null!;
    private Button _rankUp = null!;

    [Signal]
    public delegate void MessageEventHandler(string text);

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        Visible = false;
        var box = new VBoxContainer();
        AddChild(box);
        var header = new HBoxContainer();
        header.AddChild(new Label { Text = Strings.RankTitle, SizeFlagsHorizontal = SizeFlags.ExpandFill });
        var close = new Button { Text = "×", TooltipText = Strings.Close, FocusMode = FocusModeEnum.None };
        close.Pressed += () => Visible = false;
        header.AddChild(close);
        box.AddChild(header);
        _current = new Label();
        _next = new Label();
        box.AddChild(_current);
        box.AddChild(_next);
        for (int i = 0; i < 4; i++)
        {
            var label = new Label { MouseFilter = MouseFilterEnum.Stop };
            box.AddChild(label);
            _requirements.Add(label);
        }
        _requirements[2].TooltipText = Strings.DefenseHelp;
        _tribute = new Label();
        _rewards = new Label { AutowrapMode = TextServer.AutowrapMode.WordSmart };
        box.AddChild(_tribute);
        box.AddChild(_rewards);
        _rankUp = new Button { Text = Strings.RankUpButton, FocusMode = FocusModeEnum.None };
        _rankUp.Pressed += RankUp;
        box.AddChild(_rankUp);
    }

    public void ApplyScale(int scale)
    {
        CustomMinimumSize = new Vector2(230 * scale, 0);
    }

    public void Toggle()
    {
        Visible = !Visible;
        Refresh();
    }

    private void RankUp()
    {
        CoreResult result = CoreBridge.Instance.RankUp(out DenyReason reason);
        if (!result.Ok)
        {
            EmitSignal(SignalName.Message, Strings.RankUpRejected(reason));
        }
        Refresh();
    }

    public void Refresh()
    {
        if (!Visible || !CoreBridge.Instance.TryGetRank(out CoreRankView rank))
        {
            return;
        }
        _current.Text = Strings.RankCurrent(rank.Rank);
        bool max = rank.NextRank < 0;
        _next.Text = max ? Strings.RankMax : Strings.RankNext(rank.NextRank);
        foreach (Label label in _requirements)
        {
            label.Visible = !max;
        }
        _tribute.Visible = !max;
        _rewards.Visible = !max;
        _rankUp.Visible = !max;
        if (max)
        {
            return;
        }
        SetRequirement(0, Strings.ReqPopulation, rank.HavePopulation, rank.NeedPopulation);
        SetRequirement(1, Strings.ReqMood, rank.HaveMood, rank.NeedMood);
        SetRequirement(2, Strings.ReqDefense, rank.HaveDefense, rank.NeedDefense);
        SetRequirement(3, Strings.ReqResearch, rank.HaveResearch, rank.NeedResearch);
        _tribute.Text = Strings.Tribute(rank.TributeGold, rank.TributeMana);
        var buildings = new List<string>();
        for (int type = 0; type < (int)BuildingType.Count; type++)
        {
            if ((rank.Unlocks & (1 << type)) != 0)
            {
                buildings.Add(Strings.BuildingName((BuildingType)type));
            }
        }
        _rewards.Text = Strings.RankRewards(rank.StorageBonus, string.Join(", ", buildings), rank.ResearchUnlocks);
        _rankUp.Disabled = rank.Ready == 0;
    }

    private void SetRequirement(int row, string what, int have, int need)
    {
        Label label = _requirements[row];
        label.Visible = need > 0; // 필요 없는 조건(마을의 연구 0개 등)은 숨김
        label.Text = Strings.Requirement(what, have, need);
        label.AddThemeColorOverride("font_color", have >= need ? new Color(0.65f, 1f, 0.65f) : new Color(1f, 0.6f, 0.55f));
    }
}
