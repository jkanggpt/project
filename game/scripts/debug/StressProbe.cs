// StressProbe.cs — 성능 측정 (--stress-probe): 주민 150명의 마을을 3배속으로 돌리다가 마물 40명의 습격을 일으키고
// 프레임마다 걸린 시간을 잽니다 (SPEC §5: 주민 150명 + 적 40명에서 60 fps = 프레임 16.7 ms).
//
// 2초(120프레임) 자리를 잡게 한 뒤 습격을 일으키고 600프레임을 잽니다. 프레임 시간은 직전 프레임부터의 실제 시간입니다.
// 클라우드의 헤드리스 실행은 그리지 않으므로 스크립트·C 호출만의 시간이고, Xvfb 는 그림을 CPU 로 그려(llvmpipe) 실제 노트북보다
// 느립니다. FPS 목표는 학생 노트북에서 --stress 로 확인합니다 (docs/QA.md).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using Godot;
using MagicCastle.Audio;
using MagicCastle.Core;

namespace MagicCastle.Debugging;

public partial class StressProbe : Node
{
    private const int WarmupFrames = 120;
    private const int MeasuredFrames = 600;
    private const double BudgetMs = 1000.0 / 60.0;

    private readonly List<double> _frameMs = new(MeasuredFrames);
    private Main _main = null!;
    private int _frame;
    private long _last;
    private int _minVillagers = int.MaxValue;
    private int _maxEnemies;

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        _main = GetParent<Main>();
        GameClock.Instance.SetSpeed(3);
    }

    public override void _Process(double delta)
    {
        long now = Stopwatch.GetTimestamp();
        double ms = _last == 0 ? 0 : (now - _last) * 1000.0 / Stopwatch.Frequency;
        _last = now;
        _frame++;
        if (_frame == WarmupFrames)
        {
            CoreResult raid = CoreBridge.Instance.DebugStress(Main.StressVillagers, Main.StressEnemies);
            GD.Print($"stress: raid with {Main.StressEnemies} enemies ({raid.Name})");
        }
        if (_frame <= WarmupFrames)
        {
            return;
        }
        _frameMs.Add(ms);
        _minVillagers = Math.Min(_minVillagers, _main.Sync.VillagerCount);
        _maxEnemies = Math.Max(_maxEnemies, _main.Combat.EnemyCount);
        if (_frameMs.Count < MeasuredFrames)
        {
            return;
        }
        SetProcess(false);
        Report();
    }

    private void Report()
    {
        double total = 0;
        int slow = 0;
        foreach (double ms in _frameMs)
        {
            total += ms;
            slow += ms > BudgetMs ? 1 : 0;
        }
        _frameMs.Sort();
        double p99 = _frameMs[(int)(_frameMs.Count * 0.99) - 1];
        double max = _frameMs[^1];
        int errors = ErrorCounter.Count;
        GD.Print($"STRESS: {MeasuredFrames} frames, villagers >= {_minVillagers}, enemies up to {_maxEnemies}, " +
                 $"frame avg {total / _frameMs.Count:0.00} ms, p99 {p99:0.00} ms, max {max:0.00} ms, " +
                 $"over 16.7 ms {slow} frames, engine errors {errors}");
        Sfx.QuitQuietly(GetTree(), errors == 0 ? 0 : 1);
    }
}
