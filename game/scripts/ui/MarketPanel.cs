// MarketPanel.cs — 시장 창: 자원마다 사는 값·파는 값과 [사기] [팔기] 단추 (한 번에 한 묶음 = 10개)
//
// 값은 모두 C 의 시장 뷰(core_get_market)에서 읽고, 단추는 CoreBridge.Trade 만 부릅니다.
// 게임 규칙(값, 차이, 값의 움직임)은 C 에 있습니다.
using System.Collections.Generic;
using Godot;
using MagicCastle.Audio;
using MagicCastle.Core;
using MagicCastle.World;

namespace MagicCastle.UI;

public partial class MarketPanel : PanelContainer
{
    private static readonly ResourceKind[] Goods = { ResourceKind.Food, ResourceKind.Wood, ResourceKind.Stone, ResourceKind.Herbs };

    private readonly List<(ResourceKind Kind, Label Stock, Label Price, Label Trend, Button Buy, Button Sell)> _rows = new();
    private readonly List<TextureRect> _icons = new();
    private GridContainer _grid = null!;
    private Label _status = null!;
    private Label _help = null!;

    /// <summary>거래 결과 문장 (알림으로)</summary>
    [Signal]
    public delegate void MessageEventHandler(string text);

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        Visible = false;
        var box = new VBoxContainer();
        AddChild(box);
        var header = new HBoxContainer();
        header.AddChild(new Label { Text = Strings.MarketTitle, SizeFlagsHorizontal = SizeFlags.ExpandFill });
        var close = new Button { Text = "×", TooltipText = Strings.Close, FocusMode = FocusModeEnum.None };
        close.Pressed += () => Visible = false;
        header.AddChild(close);
        box.AddChild(header);
        _status = new Label { AutowrapMode = TextServer.AutowrapMode.WordSmart };
        box.AddChild(_status);
        var grid = new GridContainer { Columns = 6 };
        _grid = grid;
        box.AddChild(grid);
        foreach (ResourceKind kind in Goods)
        {
            ResourceKind captured = kind;
            var name = new HBoxContainer();
            var icon = new TextureRect
            {
                Texture = Art.ResourceIcon(kind),
                ExpandMode = TextureRect.ExpandModeEnum.IgnoreSize,
                StretchMode = TextureRect.StretchModeEnum.KeepAspectCentered,
                TextureFilter = TextureFilterEnum.Nearest,
                CustomMinimumSize = new Vector2(16, 16),
            };
            name.AddChild(icon);
            _icons.Add(icon);
            name.AddChild(new Label { Text = Strings.ResourceName(kind) });
            var stock = new Label { HorizontalAlignment = HorizontalAlignment.Right };
            var price = new Label { MouseFilter = MouseFilterEnum.Pass };
            var trend = new Label();
            var buy = new Button { Text = $"{Strings.Buy} {CoreMarketView.Lot}", FocusMode = FocusModeEnum.None };
            var sell = new Button { Text = $"{Strings.Sell} {CoreMarketView.Lot}", FocusMode = FocusModeEnum.None };
            buy.Pressed += () => Trade(captured, 1);
            sell.Pressed += () => Trade(captured, -1);
            grid.AddChild(name);
            grid.AddChild(stock);
            grid.AddChild(price);
            grid.AddChild(trend);
            grid.AddChild(buy);
            grid.AddChild(sell);
            _rows.Add((kind, stock, price, trend, buy, sell));
        }
        _help = new Label { Text = Strings.MarketHelp, AutowrapMode = TextServer.AutowrapMode.WordSmart, Modulate = new Color(1, 1, 1, 0.75f) };
        box.AddChild(_help);
    }

    public void ApplyScale(int scale)
    {
        CustomMinimumSize = new Vector2(250 * scale, 0);
        _grid.AddThemeConstantOverride("h_separation", 6 * scale);
        foreach (TextureRect icon in _icons)
        {
            icon.CustomMinimumSize = new Vector2(16 * scale, 16 * scale);
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

    private void Trade(ResourceKind kind, int lots)
    {
        CoreResult result = CoreBridge.Instance.Trade(kind, lots, out int gold, out DenyReason reason);
        if (!result.Ok)
        {
            Sfx.Instance?.Play(Sound.Denied);
        }
        EmitSignal(SignalName.Message, result.Ok ? Strings.Traded(kind, lots, gold) : Strings.TradeRejected(reason));
        Refresh();
    }

    public void Refresh()
    {
        if (!Visible || !CoreBridge.Instance.TryGetMarket(out CoreMarketView market))
        {
            return;
        }
        CoreSummaryView summary = WorldSync.Instance?.Summary ?? default;
        bool open = market.Open != 0;
        _status.Visible = !open;
        _status.Text = open ? "" : Strings.MarketClosed((DenyReason)market.ClosedReason);
        foreach (var (kind, stock, price, trend, buy, sell) in _rows)
        {
            int have = summary.GetResource(kind);
            int capacity = summary.GetCapacity(kind);
            stock.Text = capacity > 0 ? $"{have}/{capacity}" : have.ToString();
            price.Text = Strings.PriceLine(market.Buy(kind), market.Sell(kind));
            price.TooltipText = Strings.PriceTooltip(kind, market.Base(kind), market.TrendPercent(kind));
            trend.Text = Strings.TrendText(market.TrendPercent(kind));
            buy.Disabled = !open || summary.GetResource(ResourceKind.Gold) < market.Buy(kind) ||
                           (capacity > 0 && capacity - have < CoreMarketView.Lot);
            sell.Disabled = !open || have < CoreMarketView.Lot;
        }
    }
}
