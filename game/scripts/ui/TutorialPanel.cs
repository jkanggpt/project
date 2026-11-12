// TutorialPanel.cs — 처음 목표 체크리스트 (왼쪽 위). 목표를 이루었는지는 C 가 정하고(요약의 tutorial_done 비트),
// 여기서는 보여 주기만 합니다. 모두 이루면 몇 초 뒤 저절로 숨고, [숨기기]로 언제든 접을 수 있습니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.UI;

public partial class TutorialPanel : PanelContainer
{
    private readonly List<Label> _rows = new();
    private Label _title = null!;
    private bool _hidden;
    private bool _finishing;

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        MouseFilter = MouseFilterEnum.Stop;
        var box = new VBoxContainer();
        AddChild(box);
        var header = new HBoxContainer();
        _title = new Label { SizeFlagsHorizontal = SizeFlags.ExpandFill };
        header.AddChild(_title);
        var hide = new Button { Text = Strings.TutorialHide, FocusMode = FocusModeEnum.None };
        hide.Pressed += () =>
        {
            _hidden = true;
            Visible = false;
        };
        header.AddChild(hide);
        box.AddChild(header);
        for (int step = 0; step < (int)TutorialStep.Count; step++)
        {
            var label = new Label { MouseFilter = MouseFilterEnum.Stop, TooltipText = Strings.TutorialHint(step) };
            box.AddChild(label);
            _rows.Add(label);
        }
    }

    public void ApplyScale(int scale)
    {
        CustomMinimumSize = new Vector2(170 * scale, 0);
    }

    /// <summary>새 게임: 다시 보여 줌</summary>
    public void Reset()
    {
        _hidden = false;
        _finishing = false;
    }

    public void Refresh(in CoreSummaryView summary)
    {
        int all = (1 << (int)TutorialStep.Count) - 1;
        int done = 0;
        for (int step = 0; step < _rows.Count; step++)
        {
            bool ok = (summary.TutorialDone & (1 << step)) != 0;
            done += ok ? 1 : 0;
            _rows[step].Text = (ok ? "[v] " : "[ ] ") + Strings.TutorialName(step);
            _rows[step].Modulate = ok ? new Color(0.65f, 1f, 0.65f, 0.8f) : Colors.White;
        }
        _title.Text = Strings.TutorialProgress(done, _rows.Count);
        if (summary.TutorialDone == all && !_finishing && !_hidden)
        {
            // 다 이루면 잠깐 보여 주고 숨김
            _finishing = true;
            SceneTreeTimer timer = GetTree().CreateTimer(6.0, processAlways: true);
            timer.Timeout += () =>
            {
                _hidden = true;
                Visible = false;
            };
        }
        Visible = !_hidden;
    }
}
