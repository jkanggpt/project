// GameClock.cs — 고정 틱 누적기와 배속 (자동 로드 이름: GameClock)
//
// C 코어에는 "몇 초 지났다" 같은 가변 시간(dt)을 절대 넘기지 않습니다.
// 실제 시간 × 배속을 모아 두었다가, 한 틱 분량이 찰 때마다 core_tick 을 정수 틱 수로 부릅니다.
// 그래서 같은 명령 순서면 컴퓨터가 빠르든 느리든 게임 결과가 같습니다.
using System;
using Godot;

namespace MagicCastle.Core;

public partial class GameClock : Node
{
    /// <summary>한 프레임에 따라잡을 최대 틱 수. 컴퓨터가 잠깐 멈춰도 한꺼번에 몰아서 돌지 않게 합니다.</summary>
    private const int MaxTicksPerFrame = 6;

    private double _accumulator;
    private int _speedBeforePause = 1;

    public static GameClock Instance { get; private set; } = null!;

    /// <summary>0 = 일시정지, 1~3 = 배속</summary>
    public int Speed { get; private set; } = 1;

    /// <summary>true 면 실제 시간으로 진행하지 않고 Advance 를 직접 부를 때만 진행합니다 (스모크 테스트, 스크린숏).</summary>
    public bool ManualMode { get; set; }

    /// <summary>다음 틱까지 얼마나 왔는지 0..1 (낮밤 색을 부드럽게 바꾸는 데 씀)</summary>
    public double TickFraction => SecondsPerTick > 0 ? Math.Clamp(_accumulator / SecondsPerTick, 0.0, 1.0) : 0.0;

    private static double SecondsPerTick => CoreBridge.Instance.Config.RealMsPerTick / 1000.0;

    [Signal]
    public delegate void TickedEventHandler(int ticks);

    [Signal]
    public delegate void SpeedChangedEventHandler(int speed);

    public override void _EnterTree()
    {
        Instance = this;
        // 일시정지 중에도 배속 단축키와 시계는 돌아야 합니다
        ProcessMode = ProcessModeEnum.Always;
    }

    public override void _Process(double delta)
    {
        if (ManualMode || Speed == 0 || !CoreBridge.Instance.HasWorld || SecondsPerTick <= 0)
        {
            return;
        }
        _accumulator += delta * Speed;
        int ticks = (int)(_accumulator / SecondsPerTick);
        if (ticks <= 0)
        {
            return;
        }
        if (ticks > MaxTicksPerFrame)
        {
            ticks = MaxTicksPerFrame;
            _accumulator = 0; // 밀린 시간은 버립니다 (따라잡느라 멈춘 것처럼 보이지 않게)
        }
        else
        {
            _accumulator -= ticks * SecondsPerTick;
        }
        Advance(ticks);
    }

    /// <summary>ticks 만큼 코어를 진행하고 Ticked 신호를 보냅니다.</summary>
    public bool Advance(int ticks)
    {
        if (ticks <= 0 || !CoreBridge.Instance.Tick(ticks).Ok)
        {
            return false;
        }
        EmitSignal(SignalName.Ticked, ticks);
        return true;
    }

    public void SetSpeed(int speed)
    {
        int max = Math.Max(1, CoreBridge.Instance.Config.MaxSpeed);
        speed = Math.Clamp(speed, 0, max);
        if (speed > 0)
        {
            _speedBeforePause = speed;
        }
        Speed = speed;
        // 일시정지하면 장면 트리를 멈춥니다. UI 와 카메라는 ProcessMode.Always 라서 계속 움직입니다.
        GetTree().Paused = speed == 0;
        EmitSignal(SignalName.SpeedChanged, speed);
    }

    public void TogglePause()
    {
        SetSpeed(Speed == 0 ? _speedBeforePause : 0);
    }

    public void ResetAccumulator()
    {
        _accumulator = 0;
    }
}
