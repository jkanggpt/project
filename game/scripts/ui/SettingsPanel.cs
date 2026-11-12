// SettingsPanel.cs — 설정 창: 음량 셋, UI 크기, 전체 화면, 가장자리 스크롤. 바꾸면 바로 적용, 닫을 때 파일에 씀.
using System;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.UI;

public partial class SettingsPanel : Control
{
    private PanelContainer _panel = null!;
    private HSlider _master = null!;
    private HSlider _music = null!;
    private HSlider _sfx = null!;
    private Label _masterValue = null!;
    private Label _musicValue = null!;
    private Label _sfxValue = null!;
    private OptionButton _uiScale = null!;
    private CheckBox _fullscreen = null!;
    private CheckBox _edgeScroll = null!;
    private bool _loading;

    /// <summary>UI 크기·가장자리 스크롤처럼 Main 이 다시 적용해야 하는 값이 바뀜</summary>
    public event Action? Changed;

    public event Action? Closed;

    public override void _Ready()
    {
        MenuParts.MakeFullScreen(this);
        Visible = false;
        MenuParts.AddDim(this, 0.35f);
        _panel = MenuParts.AddCenteredPanel(this, out VBoxContainer box);
        box.AddChild(MenuParts.Heading(Strings.SettingsTitle));
        var grid = new GridContainer { Columns = 3 };
        box.AddChild(grid);
        _master = AddSlider(grid, Strings.SettingsMaster, out _masterValue);
        _music = AddSlider(grid, Strings.SettingsMusic, out _musicValue);
        _sfx = AddSlider(grid, Strings.SettingsSfx, out _sfxValue);
        grid.AddChild(new Label { Text = Strings.SettingsUiScale });
        _uiScale = new OptionButton();
        _uiScale.AddItem(Strings.SettingsUiAuto, 0);
        for (int scale = 1; scale <= 4; scale++)
        {
            _uiScale.AddItem(Strings.SettingsUiFixed(scale), scale);
        }
        _uiScale.ItemSelected += _ => OnChanged();
        grid.AddChild(_uiScale);
        grid.AddChild(new Control());
        _fullscreen = new CheckBox { Text = Strings.SettingsFullscreen, Flat = true };
        _fullscreen.Toggled += _ => OnChanged();
        box.AddChild(_fullscreen);
        _edgeScroll = new CheckBox { Text = Strings.SettingsEdgeScroll, Flat = true };
        _edgeScroll.Toggled += _ => OnChanged();
        box.AddChild(_edgeScroll);
        box.AddChild(MenuParts.Note(Strings.SettingsMusicNote));
        var close = MenuParts.MenuButton(Strings.Close, Close);
        close.SizeFlagsHorizontal = SizeFlags.ShrinkCenter;
        box.AddChild(close);
    }

    private HSlider AddSlider(GridContainer grid, string name, out Label value)
    {
        grid.AddChild(new Label { Text = name });
        var slider = new HSlider { MinValue = 0, MaxValue = 100, Step = 5, SizeFlagsHorizontal = SizeFlags.ExpandFill };
        slider.SizeFlagsVertical = SizeFlags.ShrinkCenter;
        slider.ValueChanged += _ => OnChanged();
        grid.AddChild(slider);
        value = new Label { HorizontalAlignment = HorizontalAlignment.Right };
        grid.AddChild(value);
        return slider;
    }

    public void ApplyScale(int scale)
    {
        _panel.CustomMinimumSize = new Vector2(300 * scale, 0);
        foreach (HSlider slider in new[] { _master, _music, _sfx })
        {
            slider.CustomMinimumSize = new Vector2(120 * scale, 8 * scale);
        }
        foreach (Label value in new[] { _masterValue, _musicValue, _sfxValue })
        {
            value.CustomMinimumSize = new Vector2(30 * scale, 0);
        }
    }

    public void Open()
    {
        GameSettings settings = GameSettings.Current;
        _loading = true;
        _master.Value = Math.Round(settings.MasterVolume * 100);
        _music.Value = Math.Round(settings.MusicVolume * 100);
        _sfx.Value = Math.Round(settings.SfxVolume * 100);
        _uiScale.Select(Math.Clamp(settings.UiScale, 0, 4));
        _fullscreen.ButtonPressed = settings.Fullscreen;
        _edgeScroll.ButtonPressed = settings.EdgeScroll;
        _loading = false;
        UpdateValueLabels();
        Visible = true;
        _master.GrabFocus();
    }

    public void Close()
    {
        if (!Visible)
        {
            return;
        }
        Visible = false;
        GameSettings.Current.Save();
        Closed?.Invoke();
    }

    private void OnChanged()
    {
        UpdateValueLabels();
        if (_loading)
        {
            return;
        }
        GameSettings settings = GameSettings.Current;
        settings.MasterVolume = (float)_master.Value / 100f;
        settings.MusicVolume = (float)_music.Value / 100f;
        settings.SfxVolume = (float)_sfx.Value / 100f;
        settings.UiScale = _uiScale.GetSelectedId();
        settings.Fullscreen = _fullscreen.ButtonPressed;
        settings.EdgeScroll = _edgeScroll.ButtonPressed;
        settings.ApplyAudio();
        settings.ApplyWindow();
        Changed?.Invoke();
    }

    private void UpdateValueLabels()
    {
        _masterValue.Text = $"{_master.Value:0}";
        _musicValue.Text = $"{_music.Value:0}";
        _sfxValue.Text = $"{_sfx.Value:0}";
    }
}
