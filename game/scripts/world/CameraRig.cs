// CameraRig.cs — 카메라: WASD/화살표 이동, 화면 가장자리 스크롤, 가운데 버튼 끌기, 정수 배율 확대/축소, 지도 경계
//
// 픽셀 아트가 뭉개지지 않도록 확대는 1, 2, 3 … 처럼 정수 배율만 씁니다.
using System;
using Godot;

namespace MagicCastle.World;

public partial class CameraRig : Camera2D
{
    public const int MinZoom = 1;
    public const int MaxZoom = 6;
    private const float PanSpeed = 900f; // 화면 픽셀/초
    private const int EdgeMargin = 6;    // 가장자리 스크롤이 시작되는 폭 (픽셀)

    private Rect2 _bounds = new(0, 0, 1536, 1536);
    private bool _dragging;

    public int ZoomLevel { get; private set; } = 3;

    /// <summary>화면 가장자리에 마우스를 대면 스크롤 (설정에서 끌 수 있게 될 예정)</summary>
    public bool EdgeScrollEnabled { get; set; } = true;

    public override void _Ready()
    {
        // 일시정지 중에도 둘러볼 수 있어야 합니다
        ProcessMode = ProcessModeEnum.Always;
        Zoom = new Vector2(ZoomLevel, ZoomLevel);
    }

    public void SetBounds(Rect2 bounds)
    {
        _bounds = bounds;
        ClampAndSnap();
    }

    public void CenterOn(Vector2 worldPosition)
    {
        Position = worldPosition;
        ClampAndSnap();
    }

    /// <summary>배율을 바꿉니다. screenAnchor(화면 좌표) 아래의 지점이 그대로 있도록 위치를 맞춥니다.</summary>
    public void SetZoomLevel(int level, Vector2? screenAnchor = null)
    {
        level = Math.Clamp(level, MinZoom, MaxZoom);
        if (level == ZoomLevel)
        {
            return;
        }
        Vector2 viewSize = GetViewportRect().Size;
        Vector2 anchor = screenAnchor ?? viewSize / 2;
        Vector2 offset = anchor - viewSize / 2;
        Vector2 worldUnderAnchor = Position + offset / ZoomLevel;
        ZoomLevel = level;
        Zoom = new Vector2(level, level);
        Position = worldUnderAnchor - offset / level;
        ClampAndSnap();
    }

    public override void _Process(double delta)
    {
        Vector2 direction = Input.GetVector("camera_left", "camera_right", "camera_up", "camera_down");
        if (EdgeScrollEnabled && DisplayServer.WindowIsFocused())
        {
            direction += EdgeScrollDirection();
        }
        if (direction != Vector2.Zero)
        {
            Position += direction.LimitLength(1f) * PanSpeed * (float)delta / ZoomLevel;
            ClampAndSnap();
        }
    }

    public override void _UnhandledInput(InputEvent inputEvent)
    {
        switch (inputEvent)
        {
            case InputEventMouseButton { ButtonIndex: MouseButton.WheelUp, Pressed: true } wheel:
                SetZoomLevel(ZoomLevel + 1, wheel.Position);
                GetViewport().SetInputAsHandled();
                break;
            case InputEventMouseButton { ButtonIndex: MouseButton.WheelDown, Pressed: true } wheel:
                SetZoomLevel(ZoomLevel - 1, wheel.Position);
                GetViewport().SetInputAsHandled();
                break;
            case InputEventMouseButton { ButtonIndex: MouseButton.Middle } middle:
                _dragging = middle.Pressed;
                GetViewport().SetInputAsHandled();
                break;
            case InputEventMouseMotion motion when _dragging:
                Position -= motion.Relative / ZoomLevel;
                ClampAndSnap();
                GetViewport().SetInputAsHandled();
                break;
            default:
                if (inputEvent.IsActionPressed("zoom_in"))
                {
                    SetZoomLevel(ZoomLevel + 1);
                    GetViewport().SetInputAsHandled();
                }
                else if (inputEvent.IsActionPressed("zoom_out"))
                {
                    SetZoomLevel(ZoomLevel - 1);
                    GetViewport().SetInputAsHandled();
                }
                break;
        }
    }

    private Vector2 EdgeScrollDirection()
    {
        Rect2 view = GetViewportRect();
        Vector2 mouse = GetViewport().GetMousePosition();
        if (!view.HasPoint(mouse))
        {
            return Vector2.Zero;
        }
        var direction = Vector2.Zero;
        if (mouse.X < EdgeMargin)
        {
            direction.X -= 1;
        }
        else if (mouse.X > view.Size.X - EdgeMargin)
        {
            direction.X += 1;
        }
        if (mouse.Y < EdgeMargin)
        {
            direction.Y -= 1;
        }
        else if (mouse.Y > view.Size.Y - EdgeMargin)
        {
            direction.Y += 1;
        }
        return direction;
    }

    /// <summary>화면 가운데가 지도 밖으로 나가지 않게 하고, 화면 픽셀에 맞춰 반올림합니다 (픽셀 흔들림 방지).</summary>
    private void ClampAndSnap()
    {
        Vector2 halfView = GetViewportRect().Size / (2f * ZoomLevel);
        float x = ClampAxis(Position.X, _bounds.Position.X, _bounds.End.X, halfView.X);
        float y = ClampAxis(Position.Y, _bounds.Position.Y, _bounds.End.Y, halfView.Y);
        Position = new Vector2(MathF.Round(x * ZoomLevel) / ZoomLevel, MathF.Round(y * ZoomLevel) / ZoomLevel);
    }

    private static float ClampAxis(float value, float min, float max, float halfView)
    {
        // 화면이 지도보다 넓으면 지도를 가운데에 둡니다
        if (max - min <= halfView * 2)
        {
            return (min + max) / 2;
        }
        return Math.Clamp(value, min + halfView, max - halfView);
    }
}
