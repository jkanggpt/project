// InputActions.cs — 키 입력 동작을 코드로 등록합니다 (project.godot 의 입력 맵을 손으로 쓰지 않으려고)
using Godot;

namespace MagicCastle.Core;

public static class InputActions
{
    public static void EnsureRegistered()
    {
        Add("camera_left", Key.A, Key.Left);
        Add("camera_right", Key.D, Key.Right);
        Add("camera_up", Key.W, Key.Up);
        Add("camera_down", Key.S, Key.Down);
        Add("zoom_in", Key.Equal, Key.KpAdd);
        Add("zoom_out", Key.Minus, Key.KpSubtract);
        Add("speed_pause", Key.Space);
        Add("speed_1", Key.Key1);
        Add("speed_2", Key.Key2);
        Add("speed_3", Key.Key3);
        Add("tool_demolish", Key.X);
        Add("toggle_villagers", Key.V);
        Add("help", Key.F1);
    }

    private static void Add(string action, params Key[] keys)
    {
        if (InputMap.HasAction(action))
        {
            return;
        }
        InputMap.AddAction(action);
        foreach (Key key in keys)
        {
            InputMap.ActionAddEvent(action, new InputEventKey { PhysicalKeycode = key });
        }
    }
}
