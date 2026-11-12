// NewGameDialog.cs — 새 게임 창: 성채 이름, 지도 시드(비우면 무작위), 난이도 셋
//
// 이름은 C 의 규칙(core_check_castle_name)으로 바로 검사하고, 난이도 설명의 숫자는 C 의 프리셋 표(core_get_difficulty)에서 읽습니다.
using System;
using System.Globalization;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.UI;

public partial class NewGameDialog : Control
{
    private PanelContainer _panel = null!;
    private LineEdit _name = null!;
    private Label _nameError = null!;
    private LineEdit _seed = null!;
    private Label _seedError = null!;
    private readonly Button[] _difficulties = new Button[3];
    private Label _difficultyText = null!;
    private Button _start = null!;
    private int _difficulty = (int)Difficulty.Normal;

    /// <summary>이름, 시드, 난이도</summary>
    public event Action<string, int, int>? StartRequested;

    public event Action? Cancelled;

    public override void _Ready()
    {
        MenuParts.MakeFullScreen(this);
        Visible = false;
        MenuParts.AddDim(this, 0.35f);
        _panel = MenuParts.AddCenteredPanel(this, out VBoxContainer box);
        box.AddChild(MenuParts.Heading(Strings.NewGameTitle));

        box.AddChild(new Label { Text = Strings.NewGameName });
        _name = new LineEdit { Text = Strings.DefaultCastleName, MaxLength = 40, SelectAllOnFocus = true };
        _name.TextChanged += _ => Validate();
        _name.TextSubmitted += _ => TryStart();
        box.AddChild(_name);
        _nameError = new Label { Text = Strings.NewGameNameInvalid, Visible = false, AutowrapMode = TextServer.AutowrapMode.WordSmart };
        _nameError.AddThemeColorOverride("font_color", new Color(1f, 0.6f, 0.55f));
        box.AddChild(_nameError);

        box.AddChild(new Label { Text = Strings.NewGameSeed });
        var seedRow = new HBoxContainer();
        _seed = new LineEdit { PlaceholderText = Strings.NewGameSeedHint, MaxLength = 10, SizeFlagsHorizontal = SizeFlags.ExpandFill };
        _seed.TextChanged += _ => Validate();
        _seed.TextSubmitted += _ => TryStart();
        seedRow.AddChild(_seed);
        seedRow.AddChild(MenuParts.MenuButton(Strings.NewGameSeedRandom, () =>
        {
            _seed.Text = ((int)(GD.Randi() & 0x7FFFFFFF)).ToString(CultureInfo.InvariantCulture);
            Validate();
        }));
        box.AddChild(seedRow);
        _seedError = new Label { Text = Strings.NewGameSeedInvalid, Visible = false, AutowrapMode = TextServer.AutowrapMode.WordSmart };
        _seedError.AddThemeColorOverride("font_color", new Color(1f, 0.6f, 0.55f));
        box.AddChild(_seedError);

        box.AddChild(new Label { Text = Strings.NewGameDifficulty });
        var group = new ButtonGroup();
        var difficultyRow = new HBoxContainer();
        for (int i = 0; i < _difficulties.Length; i++)
        {
            int captured = i;
            var button = new Button
            {
                Text = Strings.DifficultyName(i),
                ToggleMode = true,
                ButtonGroup = group,
                SizeFlagsHorizontal = SizeFlags.ExpandFill,
            };
            button.Pressed += () => SelectDifficulty(captured);
            difficultyRow.AddChild(button);
            _difficulties[i] = button;
        }
        box.AddChild(difficultyRow);
        _difficultyText = new Label { AutowrapMode = TextServer.AutowrapMode.WordSmart, Modulate = new Color(1, 1, 1, 0.8f) };
        box.AddChild(_difficultyText);

        var buttons = new HBoxContainer { Alignment = BoxContainer.AlignmentMode.Center };
        _start = MenuParts.MenuButton(Strings.NewGameStart, TryStart);
        buttons.AddChild(_start);
        buttons.AddChild(MenuParts.MenuButton(Strings.Cancel, Cancel));
        box.AddChild(buttons);
    }

    public void ApplyScale(int scale)
    {
        _panel.CustomMinimumSize = new Vector2(320 * scale, 0);
        _difficultyText.CustomMinimumSize = new Vector2(310 * scale, 0);
    }

    public void Open(int difficulty)
    {
        _seed.Text = "";
        SelectDifficulty(Math.Clamp(difficulty, 0, _difficulties.Length - 1));
        Validate();
        Visible = true;
        _name.GrabFocus();
    }

    public void Cancel()
    {
        if (!Visible)
        {
            return;
        }
        Visible = false;
        Cancelled?.Invoke();
    }

    private void SelectDifficulty(int difficulty)
    {
        _difficulty = difficulty;
        for (int i = 0; i < _difficulties.Length; i++)
        {
            _difficulties[i].SetPressedNoSignal(i == difficulty);
        }
        _difficultyText.Text = CoreBridge.Instance.TryGetDifficulty((Difficulty)difficulty, out CoreDifficultyView view)
            ? Strings.DifficultyText(view)
            : "";
    }

    private bool Validate()
    {
        bool nameOk = CoreBridge.Instance.IsValidCastleName(_name.Text.Trim());
        bool seedOk = TryReadSeed(out _);
        _nameError.Visible = !nameOk;
        _seedError.Visible = !seedOk;
        _start.Disabled = !nameOk || !seedOk;
        return nameOk && seedOk;
    }

    /// <summary>비어 있으면 무작위 (-1)</summary>
    private bool TryReadSeed(out int seed)
    {
        string text = _seed.Text.Trim();
        if (text.Length == 0)
        {
            seed = -1;
            return true;
        }
        return int.TryParse(text, NumberStyles.None, CultureInfo.InvariantCulture, out seed) && seed >= 0;
    }

    private void TryStart()
    {
        if (!Visible || !Validate() || !TryReadSeed(out int seed))
        {
            return;
        }
        if (seed < 0)
        {
            seed = (int)(GD.Randi() & 0x7FFFFFFF);
        }
        Visible = false;
        StartRequested?.Invoke(_name.Text.Trim(), seed, _difficulty);
    }
}
