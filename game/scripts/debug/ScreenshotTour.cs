// ScreenshotTour.cs — 주요 장면을 PNG 로 저장하는 자동 둘러보기 (화면이 없는 서버에서 그림을 확인하는 용도)
// 실행: xvfb-run -a -s "-screen 0 1280x720x24" godot --path game --rendering-driver opengl3 -- --screenshots <폴더> --seed 42
using System;
using System.IO;
using System.Threading.Tasks;
using Godot;
using MagicCastle.Core;

namespace MagicCastle.Debugging;

public partial class ScreenshotTour : Node
{
    private Main _main = null!;
    private string _directory = "";

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        _main = GetParent<Main>();
        GameClock.Instance.ManualMode = true;
        _directory = Path.GetFullPath(LaunchOptions.Current.ScreenshotDir ?? "screenshots");
        Directory.CreateDirectory(_directory);
        RunTour();
    }

    private async void RunTour()
    {
        bool ok = true;
        try
        {
            CoreSummaryView summary = _main.Sync.Summary;
            Vector2 keep = _main.Map.TileCenter(summary.KeepX, summary.KeepY);

            await AdvanceTo(10 * 60);
            _main.Camera.SetZoomLevel(1);
            _main.Camera.CenterOn(keep);
            await Capture("01_overview_zoom1");

            _main.Camera.SetZoomLevel(3);
            _main.Camera.CenterOn(keep);
            _main.Hud.ShowToast(UI.Strings.ScreenshotSampleToast);
            await Capture("02_keep_zoom3");

            _main.Camera.SetZoomLevel(2);
            await AdvanceTo(19 * 60 + 40);
            await Capture("03_dusk_zoom2");

            await AdvanceTo(23 * 60);
            await Capture("04_night_zoom2");

            // 25일째(겨울) 한낮
            int ticksPerDay = CoreBridge.Instance.Config.TicksPerDay;
            while (_main.Sync.Summary.Day < 25)
            {
                GameClock.Instance.Advance(ticksPerDay);
            }
            await AdvanceTo(12 * 60);
            await Capture("05_winter_day25");

            GameClock.Instance.SetSpeed(0);
            await Capture("06_paused");
            GameClock.Instance.SetSpeed(1);
        }
        catch (Exception exception)
        {
            GD.PushError($"screenshot tour failed: {exception}");
            ok = false;
        }
        int errors = ErrorCounter.Count;
        GD.Print(errors == 0 && ok
            ? $"SCREENSHOTS SAVED to {_directory}"
            : $"SCREENSHOTS FAILED ({errors} engine errors/warnings)\n{ErrorCounter.Summary()}");
        GetTree().Quit(errors == 0 && ok ? 0 : 1);
    }

    /// <summary>오늘(또는 내일)의 minute 시각까지 진행합니다.</summary>
    private async Task AdvanceTo(int minute)
    {
        CoreConfigView config = CoreBridge.Instance.Config;
        int now = _main.Sync.Summary.MinuteOfDay;
        int ticks = ((minute - now + 24 * 60) % (24 * 60)) / config.MinutesPerTick;
        while (ticks > 0)
        {
            int step = Math.Min(ticks, config.MaxTicksPerCall);
            GameClock.Instance.Advance(step);
            ticks -= step;
        }
        await WaitFrames(2);
    }

    private async Task Capture(string name)
    {
        await WaitFrames(3);
        await ToSignal(RenderingServer.Singleton, RenderingServer.SignalName.FramePostDraw);
        Image image = GetViewport().GetTexture().GetImage();
        string path = Path.Combine(_directory, name + ".png");
        Error error = image.SavePng(path);
        if (error != Error.Ok)
        {
            throw new IOException($"could not save {path}: {error}");
        }
        GD.Print($"saved {path}");
    }

    private async Task WaitFrames(int count)
    {
        for (int i = 0; i < count; i++)
        {
            await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
        }
    }
}
