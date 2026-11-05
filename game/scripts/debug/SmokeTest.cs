// SmokeTest.cs — 화면 없이 게임을 빠르게 돌려 보는 자동 검사
// 실행: godot --headless --path game -- --smoke-test [--days 20] [--seed 42]
//
// 하는 일: N일을 진행하며 매 프레임 불변 조건을 검사하고, 카메라·배속을 건드려 보고,
// 끝에 저장 → 불러오기 해시를 비교합니다. 엔진 오류·경고가 하나라도 있으면 실패(종료 코드 1).
// 마일스톤 2부터는 자동 조종(오토파일럿)이 건물과 도로를 짓고, 검사창·주민 목록·일꾼 명령도 눌러 봅니다.
// 마일스톤 3부터는 부서진 건물을 고치고, 시장에서 사고팔고, 떠돌이 상인과 거래하고, 시장·상인 창도 열어 봅니다.
using System;
using System.Collections.Generic;
using Godot;
using MagicCastle.Core;
using MagicCastle.World;

namespace MagicCastle.Debugging;

public partial class SmokeTest : Node
{
    private const int MaxFrames = 20000;
    private const int SearchRadius = 22;
    private const string SavePath = "user://smoke_test.sav";

    // 자동 조종이 짓는 순서 (돈이 모자라면 다음 프레임에 다시 시도)
    internal static readonly BuildingType[] Plan =
    {
        BuildingType.Farm, BuildingType.House, BuildingType.Lumber, BuildingType.House, BuildingType.Gatherer,
        BuildingType.Quarry, BuildingType.Warehouse, BuildingType.Farm, BuildingType.House, BuildingType.Healer,
        BuildingType.Market, BuildingType.House, BuildingType.Farm, BuildingType.House, BuildingType.Lumber,
        BuildingType.House,
    };

    private readonly List<int> _placed = new();
    private Main _main = null!;
    private int _frames;
    private int _targetTick;
    private int _planIndex;
    private int _planTries;
    private bool _roadsDone;
    private int _lastReportDay;
    private bool _finished;

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        _main = GetParent<Main>();
        GameClock.Instance.ManualMode = true;
        VillagerAgent.InstantTravel = true; // 걷는 애니메이션을 기다리지 않고 바로 도착을 알립니다
        CoreBridge.Instance.TryGetSummary(out CoreSummaryView summary);
        _targetTick = summary.Tick + LaunchOptions.Current.SmokeDays * CoreBridge.Instance.Config.TicksPerDay;
        GD.Print($"Smoke test: {LaunchOptions.Current.SmokeDays} days, seed {summary.Seed}");
    }

    public override void _Process(double delta)
    {
        if (_finished)
        {
            return;
        }
        _frames++;
        if (_frames > MaxFrames)
        {
            Finish(false, "timed out");
            return;
        }
        CoreConfigView config = CoreBridge.Instance.Config;

        // 1/8일씩 진행 (렌더링·UI 갱신·도착 보고가 여러 번 돌도록 프레임을 나눔)
        if (!GameClock.Instance.Advance(config.TicksPerDay / 8))
        {
            Finish(false, "core_tick failed");
            return;
        }
        int violations = CoreBridge.Instance.Validate();
        if (violations != 0)
        {
            Finish(false, $"core_validate reported {violations} violations");
            return;
        }
        if (!NodesMatchViews(out string mismatch))
        {
            Finish(false, mismatch);
            return;
        }
        Autopilot();
        Exercise(_frames);

        if (_main.Sync.Summary.Tick >= _targetTick)
        {
            Finish(CheckColony(out string colony) && CheckSaveLoad(out colony), colony);
        }
    }

    // ---- 자동 조종 --------------------------------------------------------

    private void Autopilot()
    {
        CoreSummaryView summary = _main.Sync.Summary;
        if (summary.Day != _lastReportDay && summary.Day % 2 == 0)
        {
            _lastReportDay = summary.Day;
            GD.Print($"day {summary.Day}: population {summary.Population}, food {summary.GetResource(ResourceKind.Food)}" +
                     $" ({summary.GetDelta(ResourceKind.Food):+#;-#;0}/day), wood {summary.GetResource(ResourceKind.Wood)}," +
                     $" gold {summary.GetResource(ResourceKind.Gold)}, mood {summary.AverageMood}, weather {summary.Weather}," +
                     $" event {summary.NatureEvent}, sick {summary.SickCount}, cold {summary.ColdHouses}");
        }
        Vector2I keepDoor = new(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize);
        if (!_roadsDone)
        {
            // 성채 문에서 아래로, 그리고 옆으로 도로 한 줄씩
            CoreBridge.Instance.PlaceLine(BuildingType.Road, keepDoor, keepDoor + new Vector2I(0, 6), out int down, out _);
            CoreBridge.Instance.PlaceLine(BuildingType.Road, keepDoor + new Vector2I(-6, 6), keepDoor + new Vector2I(6, 6), out int across, out _);
            _roadsDone = true;
            GD.Print($"autopilot: roads {down}+{across}");
        }
        if (_frames % 8 == 0)
        {
            KeepUp(summary);
        }
        if (_planIndex >= Plan.Length || _frames % 2 != 0)
        {
            return;
        }
        BuildingType type = Plan[_planIndex];
        if (TryFindSpot(type, new Vector2I(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize / 2), out Vector2I spot))
        {
            CoreResult result = CoreBridge.Instance.PlaceBuilding(type, spot, out int id, out PlaceResult reason);
            if (result.Ok && reason == PlaceResult.Ok)
            {
                _placed.Add(id);
                GD.Print($"autopilot: {type} at {spot} (day {summary.Day})");
                _planIndex++;
                _planTries = 0;
                return;
            }
        }
        if (++_planTries > 40)
        {
            GD.Print($"autopilot: gave up on {type}");
            _planIndex++;
            _planTries = 0;
        }
    }

    /// <summary>하루에 한 번: 부서진 건물 고치기, 겨울 준비가 모자라면 시장에서 사기, 넘치는 석재 팔기, 상인의 마정석 사기</summary>
    private void KeepUp(in CoreSummaryView summary)
    {
        foreach (CoreBuildingView building in _main.Sync.Buildings)
        {
            if (building.State != (int)BuildingState.Constructing && building.Hp < building.MaxHp)
            {
                CoreBridge.Instance.Repair(building.Id, out _);
            }
        }
        if (summary.MarketOpen != 0)
        {
            if (summary.GetResource(ResourceKind.Food) < summary.WinterFoodNeed)
            {
                CoreBridge.Instance.Trade(ResourceKind.Food, 3, out _, out _);
            }
            if (summary.GetResource(ResourceKind.Wood) < summary.WinterWoodNeed)
            {
                CoreBridge.Instance.Trade(ResourceKind.Wood, 2, out _, out _);
            }
            if (summary.GetResource(ResourceKind.Stone) * 10 > summary.GetCapacity(ResourceKind.Stone) * 9)
            {
                CoreBridge.Instance.Trade(ResourceKind.Stone, -2, out _, out _);
            }
        }
        if (summary.MerchantDaysLeft > 0 && CoreBridge.Instance.TryGetMerchant(out CoreMerchantView merchant))
        {
            CoreDealView deal = merchant.GetDeal(0);
            if (deal.Taken == 0 && summary.GetResource(ResourceKind.Gold) > deal.GiveAmount + 60)
            {
                CoreBridge.Instance.AcceptDeal(0, out _);
                GD.Print($"autopilot: bought {deal.GetAmount} mana from the merchant");
            }
        }
    }

    /// <summary>성채에서 가까운 순으로 지을 수 있는 자리를 찾고, 생산성이 가장 높은 곳을 고릅니다.</summary>
    internal static bool TryFindSpot(BuildingType type, Vector2I center, out Vector2I best)
    {
        best = default;
        int bestScore = int.MinValue;
        for (int dy = -SearchRadius; dy <= SearchRadius; dy++)
        {
            for (int dx = -SearchRadius; dx <= SearchRadius; dx++)
            {
                Vector2I tile = center + new Vector2I(dx, dy);
                CorePlacementView view = CoreBridge.Instance.CanPlace(type, tile);
                if (view.Result != (int)PlaceResult.Ok)
                {
                    continue;
                }
                // 생산성 우선, 그다음 가까운 곳 (건물 사이에 한 칸씩 띄우도록 홀수 칸은 살짝 손해)
                int score = view.Productivity * 4 - (Math.Abs(dx) + Math.Abs(dy)) - ((tile.X + tile.Y) & 1);
                if (score > bestScore)
                {
                    bestScore = score;
                    best = tile;
                }
            }
        }
        return bestScore != int.MinValue;
    }

    /// <summary>사람이 할 법한 조작을 흉내 냅니다.</summary>
    private void Exercise(int frame)
    {
        WorldSync sync = _main.Sync;
        switch (frame % 16)
        {
            case 1:
                _main.Camera.SetZoomLevel(CameraRig.MinZoom + frame % CameraRig.MaxZoom);
                break;
            case 3:
                // 아무 건물이나 골라 검사창을 엽니다
                if (sync.BuildingCount > 0)
                {
                    _main.Tools.Select(0, sync.Buildings[frame % sync.BuildingCount].Id);
                }
                break;
            case 5:
                if (sync.VillagerCount > 0)
                {
                    _main.Tools.Select(sync.Villagers[frame % sync.VillagerCount].Id, 0);
                }
                break;
            case 6:
                _main.Camera.CenterOn(_main.Map.PixelBounds.Position + _main.Map.PixelBounds.Size * (frame % 7) / 6f);
                break;
            case 7:
                GameClock.Instance.SetSpeed(frame % 4);
                break;
            case 8:
                _main.Tools.BeginBuild(BuildingType.House);
                break;
            case 9:
                _main.Tools.Cancel();
                GameClock.Instance.SetSpeed(1);
                break;
            case 10:
                _main.Hud.Villagers.Toggle();
                break;
            case 11:
                ExerciseWorkers(sync, frame);
                break;
            case 12:
                _main.Hud.Market.Toggle();
                _main.Hud.Merchant.Toggle();
                break;
            case 13:
                _main.Tools.BeginDemolish();
                _main.Tools.Cancel();
                _main.Tools.Select(0, 0);
                break;
            case 14:
                _main.Hud.Market.Toggle();
                _main.Hud.Merchant.Toggle();
                break;
        }
    }

    /// <summary>일꾼 직접 배치 → 빼기 → 자동에 맡기기 명령을 번갈아 불러 봅니다.</summary>
    private static void ExerciseWorkers(WorldSync sync, int frame)
    {
        if (sync.VillagerCount == 0)
        {
            return;
        }
        CoreVillagerView villager = sync.Villagers[frame % sync.VillagerCount];
        switch (frame / 16 % 3)
        {
            case 0:
                foreach (CoreBuildingView building in sync.Buildings)
                {
                    if (building.State == (int)BuildingState.Active && building.Workers < building.WorkerSlots)
                    {
                        CoreBridge.Instance.AssignWorker(villager.Id, building.Id);
                        break;
                    }
                }
                break;
            case 1:
                CoreBridge.Instance.UnassignWorker(villager.Id);
                break;
            default:
                CoreBridge.Instance.ReleaseWorker(villager.Id);
                break;
        }
    }

    // ---- 검사 ------------------------------------------------------------

    private bool NodesMatchViews(out string reason)
    {
        WorldSync sync = _main.Sync;
        foreach (CoreBuildingView building in sync.Buildings)
        {
            if (sync.NodeFor(building.Id) == null)
            {
                reason = $"building {building.Id} has no node";
                return false;
            }
        }
        foreach (CoreVillagerView villager in sync.Villagers)
        {
            if (sync.AgentFor(villager.Id) == null)
            {
                reason = $"villager {villager.Id} has no agent";
                return false;
            }
        }
        reason = "";
        return true;
    }

    /// <summary>마을이 실제로 돌아갔는지: 지은 건물이 완성되고 주민이 남아 있어야 합니다.</summary>
    private bool CheckColony(out string reason)
    {
        WorldSync sync = _main.Sync;
        int active = 0;
        foreach (CoreBuildingView building in sync.Buildings)
        {
            if (building.Type != (int)BuildingType.Keep && building.Type != (int)BuildingType.Road &&
                building.State == (int)BuildingState.Active)
            {
                active++;
            }
        }
        CoreSummaryView summary = sync.Summary;
        string counts = $"{active} buildings done, population {summary.Population}, employed {summary.Employed}";
        GD.Print($"colony: {counts}");
        int minimum = Math.Min(3, LaunchOptions.Current.SmokeDays / 3);
        if (active < minimum || summary.Population == 0)
        {
            reason = $"colony did not grow: {counts}";
            return false;
        }
        reason = counts;
        return true;
    }

    private bool CheckSaveLoad(out string reason)
    {
        CoreBridge core = CoreBridge.Instance;
        ulong before = core.StateHash();
        CoreResult saved = core.Save(SavePath);
        if (!saved.Ok)
        {
            reason = $"save failed: {saved.Name}";
            return false;
        }
        CoreResult loaded = core.Load(SavePath, out int line);
        if (!loaded.Ok)
        {
            reason = $"load failed: {loaded.Name} (line {line})";
            return false;
        }
        _main.RebuildWorldView();
        ulong after = core.StateHash();
        if (before != after)
        {
            reason = $"state hash changed after save/load: {before:x16} -> {after:x16}";
            return false;
        }
        if (!NodesMatchViews(out reason))
        {
            return false;
        }
        reason = $"state hash {after:x16}";
        return true;
    }

    private void Finish(bool ok, string reason)
    {
        _finished = true;
        GameClock.Instance.SetSpeed(1);
        int errors = ErrorCounter.Count;
        if (ok && errors == 0)
        {
            GD.Print($"SMOKE TEST PASSED: day {_main.Sync.Summary.Day}, {_frames} frames, {reason}");
            GetTree().Quit(0);
            return;
        }
        string why = errors > 0 ? $"{errors} engine errors/warnings:\n{ErrorCounter.Summary()}" : reason;
        GD.Print($"SMOKE TEST FAILED: {why}");
        Console.Error.WriteLine($"SMOKE TEST FAILED: {why}");
        GetTree().Quit(1);
    }
}
