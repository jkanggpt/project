// TitleScreen.cs — 제목 화면: 이어하기 / 새 게임 / 불러오기 / 설정 / 종료
//
// 뒤에는 멈춰 둔 지도가 보입니다 (Main 이 만든 배경용 월드, 또는 제목으로 돌아오기 전의 마을).
using System;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.UI;

public partial class TitleScreen : Control
{
    private PanelContainer _panel = null!;
    private Label _title = null!;
    private Label _subtitle = null!;
    private Button _continue = null!;
    private Label _continueInfo = null!;
    private Button _newGame = null!;
    private Label _credits = null!;

    public event Action? ContinuePressed;
    public event Action? NewGamePressed;
    public event Action? LoadPressed;
    public event Action? SettingsPressed;
    public event Action? QuitPressed;

    public override void _Ready()
    {
        MenuParts.MakeFullScreen(this);
        Visible = false;
        MenuParts.AddDim(this, 0.3f);

        _panel = new PanelContainer();
        AddChild(_panel);
        // 앵커만 바꾸면 Godot 이 지금 자리를 지키려고 오프셋을 다시 계산하므로, 오프셋까지 함께 정합니다
        _panel.SetAnchorsAndOffsetsPreset(LayoutPreset.CenterLeft);
        _panel.GrowVertical = GrowDirection.Both;
        var box = new VBoxContainer();
        _panel.AddChild(box);
        _title = new Label { Text = Strings.GameTitle, HorizontalAlignment = HorizontalAlignment.Center };
        _title.AddThemeColorOverride("font_color", new Color(1f, 0.86f, 0.5f));
        _title.AddThemeColorOverride("font_shadow_color", new Color(0.25f, 0.12f, 0.05f));
        box.AddChild(_title);
        _subtitle = new Label { Text = Strings.GameSubtitle, HorizontalAlignment = HorizontalAlignment.Center, Modulate = new Color(1, 1, 1, 0.75f) };
        box.AddChild(_subtitle);
        box.AddChild(new HSeparator());

        _continue = MenuParts.MenuButton(Strings.TitleContinue, () => ContinuePressed?.Invoke());
        box.AddChild(_continue);
        _continueInfo = new Label
        {
            HorizontalAlignment = HorizontalAlignment.Center,
            AutowrapMode = TextServer.AutowrapMode.WordSmart,
            Modulate = new Color(1, 1, 1, 0.7f),
        };
        box.AddChild(_continueInfo);
        _newGame = MenuParts.MenuButton(Strings.TitleNewGame, () => NewGamePressed?.Invoke());
        box.AddChild(_newGame);
        box.AddChild(MenuParts.MenuButton(Strings.TitleLoad, () => LoadPressed?.Invoke()));
        box.AddChild(MenuParts.MenuButton(Strings.TitleSettings, () => SettingsPressed?.Invoke()));
        box.AddChild(MenuParts.MenuButton(Strings.TitleQuit, () => QuitPressed?.Invoke()));

        _credits = new Label { Text = Strings.TitleCredits, Modulate = new Color(1, 1, 1, 0.6f) };
        AddChild(_credits);
        _credits.SetAnchorsAndOffsetsPreset(LayoutPreset.BottomRight);
        _credits.GrowHorizontal = GrowDirection.Begin;
        _credits.GrowVertical = GrowDirection.Begin;
    }

    public void ApplyScale(int scale)
    {
        _panel.CustomMinimumSize = new Vector2(220 * scale, 0);
        _panel.OffsetLeft = _panel.OffsetRight = 24 * scale;
        _panel.OffsetTop = _panel.OffsetBottom = 0;
        _title.AddThemeFontSizeOverride("font_size", 24 * scale);
        _title.AddThemeConstantOverride("shadow_offset_x", scale);
        _title.AddThemeConstantOverride("shadow_offset_y", scale);
        _continueInfo.CustomMinimumSize = new Vector2(210 * scale, 0);
        _credits.OffsetLeft = _credits.OffsetRight = -6 * scale;
        _credits.OffsetTop = _credits.OffsetBottom = -4 * scale;
    }

    /// <summary>latestSlot 이 -1 이면 이어할 세이브가 없음</summary>
    public void Open(int latestSlot, in SlotInfo latest)
    {
        bool canContinue = latestSlot >= 0;
        _continue.Disabled = !canContinue;
        _continueInfo.Text = canContinue ? Strings.TitleContinueInfo(latest) : Strings.TitleNoSave;
        Visible = true;
        (canContinue ? _continue : _newGame).GrabFocus();
    }
}
