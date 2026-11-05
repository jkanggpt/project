// WorldSync.cs — 틱이 끝날 때마다 C 의 스냅숏을 읽어 화면을 맞춥니다.
//
// 상태는 이벤트가 아니라 스냅숏(요약, 나중에는 주민·건물 뷰 전체)으로 맞춥니다 (SPEC §3.1 규칙 3).
// 이벤트 큐는 소리와 알림 같은 피드백에만 씁니다. 이벤트를 놓쳐도 화면이 어긋나지 않습니다.
// 마일스톤 2부터 주민·건물 노드를 id 로 만들고/고치고/지우는 일이 여기에 더해집니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;
using MagicCastle.UI;

namespace MagicCastle.World;

public partial class WorldSync : Node
{
    private readonly List<CoreEventView> _events = new();
    private int _knownCostGridRev = -1;

    public CoreSummaryView Summary { get; private set; }

    /// <summary>비용 격자가 바뀌었을 때 (길찾기 격자를 다시 만들어야 할 때, 마일스톤 2)</summary>
    [Signal]
    public delegate void CostGridChangedEventHandler(int revision);

    /// <summary>보여 줄 알림 문장이 생겼을 때</summary>
    [Signal]
    public delegate void NotifyEventHandler(string text);

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
    }

    /// <summary>새 게임이나 불러오기 직후: 기억해 둔 것을 모두 잊고 다시 맞춥니다.</summary>
    public void Reset()
    {
        _knownCostGridRev = -1;
        _events.Clear();
        Reconcile();
    }

    public void Reconcile()
    {
        if (!CoreBridge.Instance.TryGetSummary(out CoreSummaryView summary))
        {
            return;
        }
        Summary = summary;
        if (summary.CostGridRev != _knownCostGridRev)
        {
            _knownCostGridRev = summary.CostGridRev;
            EmitSignal(SignalName.CostGridChanged, summary.CostGridRev);
        }
        _events.Clear();
        CoreBridge.Instance.PollEvents(_events);
        foreach (CoreEventView coreEvent in _events)
        {
            string? text = Strings.EventText(coreEvent);
            if (text != null)
            {
                EmitSignal(SignalName.Notify, text);
            }
        }
    }
}
