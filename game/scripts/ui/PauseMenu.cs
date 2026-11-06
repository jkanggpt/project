// PauseMenu.cs — 일시정지 메뉴 (Esc 또는 위쪽 막대의 "메뉴"): 계속하기, 저장하기, 불러오기, 설정, 제목 화면, 게임 종료
using System;
using Godot;

namespace MagicCastle.UI;

public partial class PauseMenu : Control
{
    private PanelContainer _panel = null!;
    private Label _lastSave = null!;
    private Button _resume = null!;
    private Button _save = null!;

    public event Action? ResumePressed;
    public event Action? SavePressed;
    public event Action? LoadPressed;
    public event Action? SettingsPressed;
    public event Action? TitlePressed;
    public event Action? QuitPressed;

    public override void _Ready()
    {
        MenuParts.MakeFullScreen(this);
        Visible = false;
        MenuParts.AddDim(this, 0.45f);
        _panel = MenuParts.AddCenteredPanel(this, out VBoxContainer box);
        box.AddChild(MenuParts.Heading(Strings.PauseTitle));
        _lastSave = MenuParts.Note("");
        _lastSave.HorizontalAlignment = HorizontalAlignment.Center;
        box.AddChild(_lastSave);
        _resume = MenuParts.MenuButton(Strings.PauseResume, () => ResumePressed?.Invoke());
        box.AddChild(_resume);
        _save = MenuParts.MenuButton(Strings.PauseSave, () => SavePressed?.Invoke());
        box.AddChild(_save);
        box.AddChild(MenuParts.MenuButton(Strings.PauseLoad, () => LoadPressed?.Invoke()));
        box.AddChild(MenuParts.MenuButton(Strings.PauseSettings, () => SettingsPressed?.Invoke()));
        box.AddChild(MenuParts.MenuButton(Strings.PauseToTitle, () => TitlePressed?.Invoke()));
        box.AddChild(MenuParts.MenuButton(Strings.PauseQuit, () => QuitPressed?.Invoke()));
        var note = MenuParts.Note(Strings.QuitAutosaveNote);
        note.HorizontalAlignment = HorizontalAlignment.Center;
        box.AddChild(note);
    }

    public void ApplyScale(int scale)
    {
        _panel.CustomMinimumSize = new Vector2(200 * scale, 0);
    }

    /// <summary>saveBlockedReason 이 비어 있지 않으면 저장 단추를 끄고 툴팁으로 이유를 보여 줍니다 (습격 중)</summary>
    public void Open(string lastSave, string saveBlockedReason)
    {
        _lastSave.Text = lastSave;
        _save.Disabled = saveBlockedReason.Length > 0;
        _save.TooltipText = saveBlockedReason;
        Visible = true;
        _resume.GrabFocus();
    }

    /// <summary>Esc = 계속하기</summary>
    public void EmitResume() => ResumePressed?.Invoke();
}
