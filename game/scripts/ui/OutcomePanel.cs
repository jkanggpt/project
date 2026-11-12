// OutcomePanel.cs — 승리·패배 화면: 제목, 원인, 한 판의 기록, [계속 다스리기](승리일 때) [새 게임]
// 기록은 C 의 core_get_run_stats 에서 읽습니다. 새 게임은 Main 이 만듭니다 (신호).
using Godot;
using MagicCastle.Core;

namespace MagicCastle.UI;

public partial class OutcomePanel : PanelContainer
{
    private Label _title = null!;
    private Label _reason = null!;
    private Label _stats = null!;
    private Button _continue = null!;
    private Button _newGame = null!;

    [Signal]
    public delegate void NewGameRequestedEventHandler();

    [Signal]
    public delegate void ContinueRequestedEventHandler();

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        Visible = false;
        var box = new VBoxContainer();
        AddChild(box);
        _title = new Label { HorizontalAlignment = HorizontalAlignment.Center };
        _reason = new Label { HorizontalAlignment = HorizontalAlignment.Center, AutowrapMode = TextServer.AutowrapMode.WordSmart };
        _stats = new Label();
        box.AddChild(_title);
        box.AddChild(_reason);
        box.AddChild(new HSeparator());
        box.AddChild(_stats);
        var buttons = new HBoxContainer { Alignment = BoxContainer.AlignmentMode.Center };
        _continue = new Button { Text = Strings.ContinueButton, FocusMode = FocusModeEnum.None };
        _continue.Pressed += () =>
        {
            Visible = false;
            EmitSignal(SignalName.ContinueRequested);
        };
        _newGame = new Button { Text = Strings.NewGameButton, FocusMode = FocusModeEnum.None };
        _newGame.Pressed += () =>
        {
            Visible = false;
            EmitSignal(SignalName.NewGameRequested);
        };
        buttons.AddChild(_continue);
        buttons.AddChild(_newGame);
        box.AddChild(buttons);
    }

    public void ApplyScale(int scale)
    {
        CustomMinimumSize = new Vector2(260 * scale, 0);
        _title.AddThemeFontSizeOverride("font_size", 16 * scale);
    }

    public void ShowOutcome()
    {
        if (CoreBridge.Instance.TryGetRunStats(out CoreRunStatsView stats))
        {
            Present(stats);
        }
    }

    /// <summary>기록을 받아 화면을 채웁니다. 스크린숏 둘러보기는 지금 기록에 결과만 바꿔 넣어 미리 보기로 씁니다(C 상태는 그대로).</summary>
    public void Present(in CoreRunStatsView stats)
    {
        bool won = stats.Outcome == (int)Outcome.Won;
        _title.Text = won ? Strings.VictoryTitle : Strings.DefeatTitle;
        _title.AddThemeColorOverride("font_color", won ? new Color(1f, 0.9f, 0.45f) : new Color(1f, 0.55f, 0.5f));
        _reason.Text = won ? Strings.VictoryText(stats) : Strings.LoseReasonText(stats.LoseReason);
        _stats.Text = Strings.RunStatsText(stats);
        _continue.Visible = won;
        Visible = true;
    }
}
