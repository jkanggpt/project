// SlotDialog.cs — 저장하기 / 불러오기 창: 자동 저장(불러오기만)과 슬롯 셋
//
// 한 줄 = 단추 하나 (슬롯 이름 · 저장한 시각 / 성채 이름 — 날짜 · 등급 · 주민 · 난이도).
// 요약은 C 가 파일 전체를 검사하고 만든 것이라(core_peek_save), "불러올 수 있음"으로 보인 줄은 정말 불러올 수 있습니다.
// 망가진 슬롯은 예비 파일(.bak)이 멀쩡하면 그것을 제안하고, 다른 버전의 세이브는 이유를 보여 주고 막습니다.
using System;
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.UI;

public partial class SlotDialog : Control
{
    private PanelContainer _panel = null!;
    private Label _title = null!;
    private VBoxContainer _rows = null!;
    private readonly List<Button> _buttons = new();
    private bool _saveMode;
    private bool _confirmLoss;
    private int _scale = 1;

    /// <summary>확인 창 (MenuLayer 가 넣어 줌)</summary>
    public ConfirmPanel Confirm { get; set; } = null!;

    public event Action<int>? SaveRequested;

    /// <summary>슬롯, 예비 파일인지</summary>
    public event Action<int, bool>? LoadRequested;

    public event Action? Closed;

    public bool SaveMode => _saveMode;

    public override void _Ready()
    {
        MenuParts.MakeFullScreen(this);
        Visible = false;
        MenuParts.AddDim(this, 0.35f);
        _panel = MenuParts.AddCenteredPanel(this, out VBoxContainer box);
        _title = MenuParts.Heading("");
        box.AddChild(_title);
        _rows = new VBoxContainer();
        box.AddChild(_rows);
        var close = MenuParts.MenuButton(Strings.Close, Close);
        close.SizeFlagsHorizontal = SizeFlags.ShrinkCenter;
        box.AddChild(close);
    }

    public void ApplyScale(int scale)
    {
        _scale = scale;
        _panel.CustomMinimumSize = new Vector2(380 * scale, 0);
        foreach (Button button in _buttons)
        {
            button.CustomMinimumSize = new Vector2(370 * scale, 0);
        }
    }

    /// <summary>saveMode: 저장하기 창. confirmLoss: 불러오기 전에 "저장하지 않은 진행은 사라집니다"를 물음</summary>
    public void Open(bool saveMode, bool confirmLoss)
    {
        _saveMode = saveMode;
        _confirmLoss = confirmLoss;
        _title.Text = saveMode ? Strings.SaveDialogTitle : Strings.LoadDialogTitle;
        Refresh();
        Visible = true;
        foreach (Button button in _buttons)
        {
            if (!button.Disabled)
            {
                button.GrabFocus();
                break;
            }
        }
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

    /// <summary>파일을 다시 읽어 줄을 만듭니다 (창을 열 때마다)</summary>
    public void Refresh()
    {
        foreach (Button button in _buttons)
        {
            button.QueueFree();
        }
        _buttons.Clear();
        int first = _saveMode ? 1 : SaveSlots.AutosaveSlot;
        for (int slot = first; slot <= SaveSlots.SlotCount; slot++)
        {
            SlotInfo info = SaveSlots.Peek(slot);
            var button = new Button
            {
                Text = Strings.SlotLine(info),
                Alignment = HorizontalAlignment.Left,
                CustomMinimumSize = new Vector2(370 * _scale, 0),
                ClipText = true,
            };
            // 불러오기: 빈 슬롯과 고칠 수 없는 슬롯은 누를 수 없음. 저장하기: 언제나 누를 수 있음
            button.Disabled = !_saveMode && !info.Ok && !info.OnlyBackup;
            if (!info.Ok && info.Exists)
            {
                button.AddThemeColorOverride("font_color", new Color(1f, 0.7f, 0.55f));
                button.AddThemeColorOverride("font_disabled_color", new Color(1f, 0.6f, 0.55f, 0.6f));
            }
            button.Pressed += () => OnSlotPressed(info);
            _rows.AddChild(button);
            _buttons.Add(button);
        }
    }

    private void OnSlotPressed(SlotInfo info)
    {
        if (_saveMode)
        {
            if (info.Exists)
            {
                Confirm.Ask(Strings.ConfirmOverwrite(info), () => Finish(() => SaveRequested?.Invoke(info.Slot)));
            }
            else
            {
                Finish(() => SaveRequested?.Invoke(info.Slot));
            }
            return;
        }
        if (info.Ok)
        {
            if (_confirmLoss)
            {
                Confirm.Ask(Strings.ConfirmLoseProgress, () => Finish(() => LoadRequested?.Invoke(info.Slot, false)));
            }
            else
            {
                Finish(() => LoadRequested?.Invoke(info.Slot, false));
            }
        }
        else if (info.OnlyBackup)
        {
            Confirm.Ask(Strings.ConfirmBackup(info), () => Finish(() => LoadRequested?.Invoke(info.Slot, true)));
        }
    }

    private void Finish(Action action)
    {
        Visible = false;
        action();
    }
}
