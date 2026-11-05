// MerchantPanel.cs — 떠돌이 상인 창: 제안 3~4개와 [거래] 단추
//
// 제안은 C 가 정합니다 (core_get_merchant). 단추는 CoreBridge.AcceptDeal 만 부릅니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;
using MagicCastle.World;

namespace MagicCastle.UI;

public partial class MerchantPanel : PanelContainer
{
    private readonly List<(Label Text, Button Accept)> _rows = new();
    private Label _title = null!;

    [Signal]
    public delegate void MessageEventHandler(string text);

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        Visible = false;
        var box = new VBoxContainer();
        AddChild(box);
        var header = new HBoxContainer();
        _title = new Label { SizeFlagsHorizontal = SizeFlags.ExpandFill };
        header.AddChild(_title);
        var close = new Button { Text = "×", TooltipText = Strings.Close, FocusMode = FocusModeEnum.None };
        close.Pressed += () => Visible = false;
        header.AddChild(close);
        box.AddChild(header);
        for (int i = 0; i < CoreMerchantView.MaxDeals; i++)
        {
            int index = i;
            var row = new HBoxContainer();
            var text = new Label { SizeFlagsHorizontal = SizeFlags.ExpandFill };
            var accept = new Button { Text = Strings.Accept, FocusMode = FocusModeEnum.None };
            accept.Pressed += () => Accept(index);
            row.AddChild(text);
            row.AddChild(accept);
            box.AddChild(row);
            _rows.Add((text, accept));
        }
        box.AddChild(new Label
        {
            Text = Strings.MerchantHelp,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            Modulate = new Color(1, 1, 1, 0.75f),
        });
    }

    public void ApplyScale(int scale)
    {
        CustomMinimumSize = new Vector2(220 * scale, 0);
    }

    public void Toggle()
    {
        Visible = !Visible;
        Refresh();
    }

    private void Accept(int index)
    {
        CoreResult result = CoreBridge.Instance.AcceptDeal(index, out DenyReason reason);
        if (!result.Ok)
        {
            EmitSignal(SignalName.Message, Strings.TradeRejected(reason));
        }
        Refresh();
    }

    public void Refresh()
    {
        if (!Visible || !CoreBridge.Instance.TryGetMerchant(out CoreMerchantView merchant))
        {
            return;
        }
        if (merchant.DaysLeft <= 0)
        {
            Visible = false; // 상인이 떠남 (알림은 C 의 MERCHANT_LEFT 이벤트가 냄)
            return;
        }
        CoreSummaryView summary = WorldSync.Instance?.Summary ?? default;
        _title.Text = Strings.MerchantTitle(merchant.DaysLeft);
        for (int i = 0; i < _rows.Count; i++)
        {
            var (text, accept) = _rows[i];
            bool shown = i < merchant.DealCount;
            text.GetParent<Control>().Visible = shown;
            if (!shown)
            {
                continue;
            }
            CoreDealView deal = merchant.GetDeal(i);
            text.Text = Strings.DealText(deal);
            bool taken = deal.Taken != 0;
            accept.Text = taken ? Strings.DealDone : Strings.Accept;
            accept.Disabled = taken || summary.GetResource((ResourceKind)deal.GiveResource) < deal.GiveAmount;
        }
    }
}
