// MenuLayer.cs — 메뉴 화면들을 한 층(CanvasLayer)에 모읍니다: 제목, 일시정지, 새 게임, 슬롯, 설정, 확인 창
//
// 입력: 이 층은 Main 의 마지막 자식이라 처리되지 않은 입력(_UnhandledInput)을 가장 먼저 받습니다.
// 메뉴가 하나라도 열려 있으면 키 입력을 모두 여기서 먹어서, 뒤의 게임(배속·철거·카메라 단축키)이 움직이지 않게 합니다.
// 마우스는 메뉴마다 화면 전체를 덮는 막이 막습니다. Esc 는 맨 위의 창부터 하나씩 닫습니다.
// 게임 중에 아무 메뉴도 없을 때의 Esc 는 InGameEscape 에 맡깁니다 (Main: 창 닫기 → 도구 취소 → 일시정지 메뉴).
using System;
using Godot;

namespace MagicCastle.UI;

public partial class MenuLayer : CanvasLayer
{
    public TitleScreen Title { get; private set; } = null!;
    public PauseMenu Pause { get; private set; } = null!;
    public NewGameDialog NewGame { get; private set; } = null!;
    public SlotDialog Slots { get; private set; } = null!;
    public SettingsPanel Settings { get; private set; } = null!;
    public HelpPanel Help { get; private set; } = null!;
    public ConfirmPanel Confirm { get; private set; } = null!;

    /// <summary>게임 중 Esc (메뉴가 없을 때). true 면 처리함</summary>
    public Func<bool>? InGameEscape { get; set; }

    public bool AnyOpen => Title.Visible || Pause.Visible || NewGame.Visible || Slots.Visible || Settings.Visible ||
                           Help.Visible || Confirm.Visible;

    /// <summary>게임 중 F1 (메뉴가 없을 때): Main 이 일시정지 메뉴를 열고 도움말을 띄움</summary>
    public Action? InGameHelp { get; set; }

    /// <summary>제목이나 일시정지 위에 뜨는 창이 열려 있음</summary>
    public bool DialogOpen => NewGame.Visible || Slots.Visible || Settings.Visible || Help.Visible || Confirm.Visible;

    public override void _Ready()
    {
        Layer = 20; // HUD(1) 위
        ProcessMode = ProcessModeEnum.Always;
        var root = new Control { Name = "Root", MouseFilter = Control.MouseFilterEnum.Ignore };
        AddChild(root);
        root.SetAnchorsAndOffsetsPreset(Control.LayoutPreset.FullRect);
        // 나중에 넣은 것이 위에 그려집니다: 제목·일시정지 → 대화 상자 → 확인 창
        Title = Add(root, new TitleScreen { Name = "Title" });
        Pause = Add(root, new PauseMenu { Name = "Pause" });
        NewGame = Add(root, new NewGameDialog { Name = "NewGame" });
        Slots = Add(root, new SlotDialog { Name = "Slots" });
        Settings = Add(root, new SettingsPanel { Name = "Settings" });
        Help = Add(root, new HelpPanel { Name = "Help" });
        Confirm = Add(root, new ConfirmPanel { Name = "Confirm" });
        Slots.Confirm = Confirm;
    }

    private static T Add<T>(Control root, T child) where T : Control
    {
        root.AddChild(child);
        return child;
    }

    public void ApplyScale(int scale)
    {
        Title.ApplyScale(scale);
        Pause.ApplyScale(scale);
        NewGame.ApplyScale(scale);
        Slots.ApplyScale(scale);
        Settings.ApplyScale(scale);
        Help.ApplyScale(scale);
        Confirm.ApplyScale(scale);
    }

    /// <summary>모든 메뉴를 닫습니다 (게임으로 들어갈 때)</summary>
    public void CloseAll()
    {
        Title.Visible = false;
        Pause.Visible = false;
        NewGame.Visible = false;
        Slots.Visible = false;
        Settings.Visible = false;
        Help.Visible = false;
        Confirm.Visible = false;
    }

    public override void _UnhandledInput(InputEvent inputEvent)
    {
        bool escape = inputEvent.IsActionPressed("ui_cancel");
        if (inputEvent.IsActionPressed("help") && !Title.Visible && !Help.Visible && !DialogOpen)
        {
            if (Pause.Visible)
            {
                Help.Open();
            }
            else
            {
                InGameHelp?.Invoke();
            }
            GetViewport().SetInputAsHandled();
            return;
        }
        if (!AnyOpen)
        {
            if (escape && InGameEscape != null && InGameEscape())
            {
                GetViewport().SetInputAsHandled();
            }
            return;
        }
        if (escape)
        {
            Back();
        }
        // 메뉴가 열려 있는 동안 게임 단축키(스페이스·1~3·X·V·카메라)는 먹습니다
        if (inputEvent is InputEventKey or InputEventMouseButton or InputEventMouseMotion)
        {
            GetViewport().SetInputAsHandled();
        }
    }

    /// <summary>맨 위의 창을 닫습니다 (Esc). 일시정지 메뉴는 "계속하기", 제목 화면은 그대로.</summary>
    public void Back()
    {
        if (Confirm.Visible)
        {
            Confirm.Answer(false);
        }
        else if (Settings.Visible)
        {
            Settings.Close();
        }
        else if (Help.Visible)
        {
            Help.Close();
        }
        else if (Slots.Visible)
        {
            Slots.Close();
        }
        else if (NewGame.Visible)
        {
            NewGame.Cancel();
        }
        else if (Pause.Visible)
        {
            Pause.EmitResume();
        }
    }
}
