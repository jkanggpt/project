// HelpPanel.cs — 도움말 (F1 또는 일시정지 메뉴의 "도움말"): 조작과 꼭 알아야 할 규칙을 한 화면에
using System;
using Godot;

namespace MagicCastle.UI;

public partial class HelpPanel : Control
{
    private PanelContainer _panel = null!;
    private ScrollContainer _scroll = null!;
    private Label _text = null!;

    public event Action? Closed;

    public override void _Ready()
    {
        MenuParts.MakeFullScreen(this);
        Visible = false;
        MenuParts.AddDim(this, 0.35f);
        _panel = MenuParts.AddCenteredPanel(this, out VBoxContainer box);
        box.AddChild(MenuParts.Heading(Strings.HelpTitle));
        _scroll = new ScrollContainer { HorizontalScrollMode = ScrollContainer.ScrollMode.Disabled };
        _text = new Label { Text = Strings.HelpText, AutowrapMode = TextServer.AutowrapMode.WordSmart };
        _text.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        _scroll.AddChild(_text);
        box.AddChild(_scroll);
        var close = MenuParts.MenuButton(Strings.Close, Close);
        close.SizeFlagsHorizontal = SizeFlags.ShrinkCenter;
        box.AddChild(close);
    }

    public void ApplyScale(int scale)
    {
        _panel.CustomMinimumSize = new Vector2(440 * scale, 0);
        _scroll.CustomMinimumSize = new Vector2(430 * scale, 250 * scale);
    }

    public void Open()
    {
        _scroll.ScrollVertical = 0;
        Visible = true;
    }

    public void Close()
    {
        if (!Visible)
        {
            return;
        }
        Visible = false;
        Closed?.Invoke();
    }
}
