// MenuParts.cs — 메뉴 화면들이 같이 쓰는 작은 도우미: 화면 전체를 덮는 어두운 막, 가운데 창, 확인 창
using System;
using Godot;

namespace MagicCastle.UI;

public static class MenuParts
{
    /// <summary>화면 전체를 덮어 뒤쪽(지도·HUD)의 클릭을 막는 반투명 막</summary>
    public static ColorRect AddDim(Control parent, float alpha)
    {
        var dim = new ColorRect { Color = new Color(0.04f, 0.03f, 0.06f, alpha), MouseFilter = Control.MouseFilterEnum.Stop };
        parent.AddChild(dim);
        dim.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        return dim;
    }

    /// <summary>가운데에 놓이고 내용만큼 커지는 창</summary>
    public static PanelContainer AddCenteredPanel(Control parent, out VBoxContainer box)
    {
        var panel = new PanelContainer();
        parent.AddChild(panel);
        panel.SetAnchorsPreset(Control.LayoutPreset.Center);
        panel.OffsetLeft = panel.OffsetRight = panel.OffsetTop = panel.OffsetBottom = 0;
        panel.GrowHorizontal = Control.GrowDirection.Both;
        panel.GrowVertical = Control.GrowDirection.Both;
        box = new VBoxContainer();
        panel.AddChild(box);
        return panel;
    }

    public static Label Heading(string text)
    {
        var label = new Label { Text = text, HorizontalAlignment = HorizontalAlignment.Center };
        label.AddThemeColorOverride("font_color", new Color(1f, 0.88f, 0.55f));
        return label;
    }

    public static Label Note(string text)
    {
        return new Label
        {
            Text = text,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            Modulate = new Color(1, 1, 1, 0.72f),
        };
    }

    public static Button MenuButton(string text, Action onPressed)
    {
        var button = new Button { Text = text, FocusMode = Control.FocusModeEnum.All };
        button.Pressed += onPressed;
        return button;
    }

    /// <summary>화면 전체를 덮는 메뉴 하나 (제목·일시정지·대화 상자)의 공통 바탕</summary>
    public static void MakeFullScreen(Control control)
    {
        control.ProcessMode = Node.ProcessModeEnum.Always;
        control.MouseFilter = Control.MouseFilterEnum.Ignore;
        control.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
    }
}

/// <summary>예/아니오를 묻는 창. 다른 메뉴 위에 뜹니다.</summary>
public partial class ConfirmPanel : Control
{
    private PanelContainer _panel = null!;
    private Label _message = null!;
    private Button _yes = null!;
    private Button _no = null!;
    private Action? _onYes;
    private Action? _onNo;

    public override void _Ready()
    {
        MenuParts.MakeFullScreen(this);
        Visible = false;
        MenuParts.AddDim(this, 0.35f);
        _panel = MenuParts.AddCenteredPanel(this, out VBoxContainer box);
        _message = new Label { AutowrapMode = TextServer.AutowrapMode.WordSmart, HorizontalAlignment = HorizontalAlignment.Center };
        box.AddChild(_message);
        var buttons = new HBoxContainer { Alignment = BoxContainer.AlignmentMode.Center };
        _yes = MenuParts.MenuButton(Strings.YesButton, () => Answer(true));
        _no = MenuParts.MenuButton(Strings.NoButton, () => Answer(false));
        buttons.AddChild(_yes);
        buttons.AddChild(_no);
        box.AddChild(buttons);
    }

    public void ApplyScale(int scale)
    {
        _panel.CustomMinimumSize = new Vector2(280 * scale, 0);
        _message.CustomMinimumSize = new Vector2(270 * scale, 0);
    }

    public void Ask(string message, Action onYes, Action? onNo = null)
    {
        _message.Text = message;
        _onYes = onYes;
        _onNo = onNo;
        _yes.Text = Strings.YesButton;
        _no.Visible = true;
        Visible = true;
        _no.GrabFocus(); // 실수로 Enter 를 눌러도 위험한 쪽이 아니게
    }

    /// <summary>알리기만 하는 창 (확인 단추 하나)</summary>
    public void Tell(string message, Action? then = null)
    {
        _message.Text = message;
        _onYes = then;
        _onNo = then;
        _yes.Text = Strings.OkButton;
        _no.Visible = false;
        Visible = true;
        _yes.GrabFocus();
    }

    public void Answer(bool yes)
    {
        Visible = false;
        Action? action = yes ? _onYes : _onNo;
        _onYes = null;
        _onNo = null;
        action?.Invoke();
    }
}
