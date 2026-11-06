// ScreenshotTour.cs — 주요 장면을 PNG 로 저장하는 자동 둘러보기 (화면이 없는 서버에서 그림을 확인하는 용도)
// 실행: xvfb-run -a -s "-screen 0 1280x720x24" godot --path game --rendering-driver opengl3 -- --screenshots <폴더> --seed 42
using System;
using System.IO;
using System.Threading.Tasks;
using Godot;
using MagicCastle.Audio;
using MagicCastle.Core;
using MagicCastle.World;

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
            Vector2 keep = _main.Map.TileCenter(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize);
            VillagerAgent.InstantTravel = true; // 빨리 감는 동안 걷기를 기다리지 않음

            await AdvanceTo(8 * 60);
            _main.Camera.SetZoomLevel(1);
            _main.Camera.CenterOn(keep);
            await Capture("01_overview_zoom1");

            // 초반 마을: 도로와 건물 몇 채를 놓고 짓는 중인 모습
            BuildSome(4);
            _main.Camera.SetZoomLevel(3);
            _main.Camera.CenterOn(keep);
            _main.Hud.ShowToast(UI.Strings.ScreenshotSampleToast);
            await AdvanceTo(10 * 60);
            await Capture("02_construction_zoom3");

            // 건설 도구: 마우스 아래 미리보기와 안내
            _main.Tools.BeginBuild(BuildingType.House);
            Input.WarpMouse(GetViewport().GetVisibleRect().Size * new Vector2(0.62f, 0.55f));
            await WaitFrames(4);
            await Capture("03_build_ghost_zoom3");
            _main.Tools.Cancel();

            // 사흘 뒤: 위협이 오는 쪽에 감시탑, 그리고 남은 계획
            await AdvanceDays(3);
            BuildToward(BuildingType.Tower, 5);
            BuildSome(SmokeTest.Plan.Length);

            // 5일째 13시: 첫 도적 습격. 싸움 한가운데(화살이 나는 중)와 끝난 뒤
            await PlayRaid("15_raid_zoom3", "16_raid_over");

            // 6일째 한낮: 다 지은 건물과 일하는 주민, 건물 검사창
            await AdvanceTo(11 * 60);
            _main.Camera.SetZoomLevel(2);
            _main.Camera.CenterOn(keep);
            int farm = FirstBuilding(BuildingType.Farm);
            _main.Tools.Select(0, farm);
            await Capture("04_colony_day6_inspector");

            // 주민 검사창 + 주민 목록
            if (_main.Sync.VillagerCount > 0)
            {
                _main.Tools.Select(_main.Sync.Villagers[0].Id, 0);
            }
            _main.Hud.Villagers.Toggle();
            await Capture("05_villager_list");
            _main.Hud.Villagers.Toggle();
            _main.Tools.Select(0, 0);

            // 등급 창(다음 등급 "마을"의 조건)과 연구 창(요새부터 풀리는 연구는 잠김)
            _main.Hud.Ranks.Toggle();
            await Capture("17_rank_panel");
            _main.Hud.Ranks.Toggle();
            _main.Hud.Research.Open();
            await Capture("18_research_panel");
            _main.Hud.Research.Toggle();

            await AdvanceTo(19 * 60 + 40);
            await Capture("06_dusk_zoom2");

            await AdvanceTo(23 * 60);
            await Capture("07_night_zoom2");

            // 7일째: 떠돌이 상인이 처음 옴. 남은 계획(치료소, 시장 …)도 짓기
            await AdvanceDays(1);
            BuildSome(SmokeTest.Plan.Length);
            await AdvanceTo(12 * 60);
            if (_main.Sync.Summary.MerchantDaysLeft > 0)
            {
                _main.Hud.Merchant.Toggle();
                await Capture("08_merchant");
                _main.Hud.Merchant.Toggle();
            }

            // 시장이 다 지어지면 시장 창
            await AdvanceDays(2);
            BuildSome(SmokeTest.Plan.Length);
            await AdvanceDays(1);
            await AdvanceTo(11 * 60);
            // 시장이 있으면 그 검사창과 함께, 없으면 "시장이 없습니다" 상태의 창이라도
            int market = FirstBuilding(BuildingType.Market);
            _main.Tools.Select(0, market);
            _main.Hud.Market.Open();
            await Capture("09_market");
            _main.Hud.Market.Toggle();
            _main.Tools.Select(0, 0);

            // 비 오는 날 (열흘 안에)
            for (int i = 0; i < 10 && _main.Sync.Summary.Weather != (int)Weather.Rain; i++)
            {
                await AdvanceDays(1);
            }
            await AdvanceTo(13 * 60);
            _main.Camera.SetZoomLevel(2);
            _main.Camera.CenterOn(keep);
            await WaitSeconds(2.5); // 비 입자와 하늘 색이 자리 잡도록 (실제 시간)
            await Capture("10_rain_zoom2");

            // 부서진 건물 (폭풍을 맞았으면): 체력 막대와 수리 단추
            if (_main.Sync.TryGetBuilding(FirstDamaged(), out CoreBuildingView damaged))
            {
                _main.Camera.SetZoomLevel(3);
                _main.Camera.CenterOn(new Vector2((damaged.X + damaged.W / 2f) * 16, (damaged.Y + damaged.H / 2f) * 16));
                _main.Tools.Select(0, damaged.Id);
                await Capture("10b_damaged_inspector");
                _main.Tools.Select(0, 0);
            }

            // 가을: 겨울 준비 표시
            int ticksPerDay = CoreBridge.Instance.Config.TicksPerDay;
            while (_main.Sync.Summary.Day < 18)
            {
                GameClock.Instance.Advance(ticksPerDay / 8);
                await WaitFrames(1);
            }
            await AdvanceTo(12 * 60);
            await WaitSeconds(2.5); // 가을 땅 색으로 바뀌는 동안
            await Capture("11_autumn_winter_prep");

            // 25일째(겨울) 한낮: 눈
            while (_main.Sync.Summary.Day < 25)
            {
                GameClock.Instance.Advance(ticksPerDay / 8);
                await WaitFrames(1);
            }
            await AdvanceTo(12 * 60);
            _main.Camera.SetZoomLevel(1);
            _main.Camera.CenterOn(keep);
            await WaitSeconds(2.5); // 눈이 덮이는 동안
            await Capture("12_winter_day25_zoom1");
            _main.Camera.SetZoomLevel(3);
            _main.Camera.CenterOn(keep);
            await Capture("13_winter_zoom3");

            GameClock.Instance.SetSpeed(0);
            await Capture("14_paused");
            GameClock.Instance.SetSpeed(1);

            // 승리·패배 화면 미리 보기: 지금까지의 기록에 결과만 바꿔 넣음 (C 의 상태는 바꾸지 않음)
            if (CoreBridge.Instance.TryGetRunStats(out CoreRunStatsView stats))
            {
                stats.Outcome = (int)Outcome.Won;
                stats.Rank = (int)Rank.Royal;
                stats.VictoryDay = stats.Days;
                _main.Hud.Outcome.Present(stats);
                await Capture("19_victory_preview");
                stats.Outcome = (int)Outcome.Lost;
                stats.LoseReason = (int)LoseReason.Keep;
                _main.Hud.Outcome.Present(stats);
                await Capture("20_defeat_preview");
                _main.Hud.Outcome.Visible = false;
            }
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
        Sfx.QuitQuietly(GetTree(), errors == 0 && ok ? 0 : 1);
    }

    /// <summary>예고된 위협이 오는 쪽으로 distance 칸 떨어진 곳 근처에 짓습니다</summary>
    private void BuildToward(BuildingType type, int distance)
    {
        CoreSummaryView summary = _main.Sync.Summary;
        var center = new Vector2I(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize / 2);
        Vector2I toward = (Direction)summary.ThreatDirection switch
        {
            Direction.North => new Vector2I(0, -distance),
            Direction.East => new Vector2I(distance, 0),
            Direction.South => new Vector2I(0, distance),
            _ => new Vector2I(-distance, 0),
        };
        if (SmokeTest.TryFindSpot(type, center + toward, out Vector2I spot))
        {
            CoreBridge.Instance.PlaceBuilding(type, spot, out _, out _);
        }
        _main.Sync.Reconcile();
    }

    /// <summary>
    /// 다음 습격이 올 때까지 한 틱씩 진행하고, 습격 중에는 싸움을 1분씩 직접 진행합니다 (실제 플레이와 같은 순서:
    /// 틱 사이에 싸움). 화살을 맞은 적이 있고 다음 화살이 날고 있을 때 한 장, 끝나고 알림이 뜬 뒤에 한 장 찍습니다.
    /// </summary>
    private async Task PlayRaid(string fightShot, string overShot)
    {
        CoreConfigView config = CoreBridge.Instance.Config;
        int limit = _main.Sync.Summary.Tick + 3 * config.TicksPerDay;
        while (_main.Sync.Summary.RaidId == 0 && _main.Sync.Summary.Tick < limit)
        {
            GameClock.Instance.Advance(1);
            await WaitFrames(1);
        }
        if (_main.Sync.Summary.RaidId == 0)
        {
            GD.Print("screenshot tour: no raid came");
            return;
        }
        CombatDirector combat = _main.Combat;
        combat.ManualStep = true;
        _main.Camera.SetZoomLevel(3);
        bool shot = false;
        int minutes = 0;
        while (_main.Sync.Summary.RaidId != 0 && minutes < config.TicksPerDay * config.MinutesPerTick)
        {
            combat.Step(1);
            minutes++;
            if (minutes % config.MinutesPerTick == 0)
            {
                GameClock.Instance.Advance(1);
            }
            FollowFight(combat);
            await WaitFrames(1);
            if (!shot && combat.ArrowMidFlight && AnyEnemyHurt(combat))
            {
                shot = true;
                await Capture(fightShot);
            }
        }
        combat.ManualStep = false;
        await WaitFrames(2);
        await Capture(overShot);
    }

    /// <summary>화살을 맞은 적이 있는가 (체력 막대가 보이는 때)</summary>
    private static bool AnyEnemyHurt(CombatDirector combat)
    {
        foreach (EnemyAgent enemy in combat.Enemies)
        {
            if (enemy.Hp < enemy.MaxHp)
            {
                return true;
            }
        }
        return false;
    }

    /// <summary>카메라를 성채와 적들 사이에 둡니다</summary>
    private void FollowFight(CombatDirector combat)
    {
        CoreSummaryView summary = _main.Sync.Summary;
        Vector2 keep = _main.Map.TileCenter(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize / 2);
        Vector2 sum = Vector2.Zero;
        int count = 0;
        foreach (EnemyAgent enemy in combat.Enemies)
        {
            sum += enemy.Position;
            count++;
        }
        _main.Camera.CenterOn(count > 0 ? (keep + sum / count) / 2 : keep);
    }

    /// <summary>자동 조종 계획의 앞쪽 count 개를 지을 수 있는 만큼 짓고 도로도 놓습니다.</summary>
    private void BuildSome(int count)
    {
        CoreSummaryView summary = _main.Sync.Summary;
        var center = new Vector2I(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize / 2);
        var door = new Vector2I(center.X, summary.KeepY + summary.KeepSize);
        CoreBridge.Instance.PlaceLine(BuildingType.Road, door, door + new Vector2I(0, 6), out _, out _);
        CoreBridge.Instance.PlaceLine(BuildingType.Road, door + new Vector2I(-6, 6), door + new Vector2I(6, 6), out _, out _);
        for (int i = 0; i < count && i < SmokeTest.Plan.Length; i++)
        {
            BuildingType type = SmokeTest.Plan[i];
            if (FirstBuilding(type) != 0 && i < 4)
            {
                continue; // 첫 묶음은 이미 지었음
            }
            if (SmokeTest.TryFindSpot(type, center, out Vector2I spot))
            {
                CoreBridge.Instance.PlaceBuilding(type, spot, out _, out _);
            }
        }
        _main.Sync.Reconcile();
    }

    private int FirstDamaged()
    {
        foreach (CoreBuildingView building in _main.Sync.Buildings)
        {
            if (building.State != (int)BuildingState.Constructing && building.Hp < building.MaxHp)
            {
                return building.Id;
            }
        }
        return 0;
    }

    private int FirstBuilding(BuildingType type)
    {
        foreach (CoreBuildingView building in _main.Sync.Buildings)
        {
            if (building.Type == (int)type)
            {
                return building.Id;
            }
        }
        return 0;
    }

    private async Task AdvanceDays(int days)
    {
        int ticks = days * CoreBridge.Instance.Config.TicksPerDay;
        while (ticks > 0)
        {
            int step = Math.Min(ticks, CoreBridge.Instance.Config.TicksPerDay / 8);
            GameClock.Instance.Advance(step);
            ticks -= step;
            await WaitFrames(1);
        }
    }

    /// <summary>오늘(또는 내일)의 minute 시각까지 진행합니다.</summary>
    private async Task AdvanceTo(int minute)
    {
        CoreConfigView config = CoreBridge.Instance.Config;
        int now = _main.Sync.Summary.MinuteOfDay;
        int ticks = ((minute - now + 24 * 60) % (24 * 60)) / config.MinutesPerTick;
        while (ticks > 0)
        {
            int step = Math.Min(ticks, config.TicksPerDay / 8);
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

    private async Task WaitSeconds(double seconds)
    {
        await ToSignal(GetTree().CreateTimer(seconds, processAlways: true), SceneTreeTimer.SignalName.Timeout);
    }

    private async Task WaitFrames(int count)
    {
        for (int i = 0; i < count; i++)
        {
            await ToSignal(GetTree(), SceneTree.SignalName.ProcessFrame);
        }
    }
}
