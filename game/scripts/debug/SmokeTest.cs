// SmokeTest.cs — 화면 없이 게임을 빠르게 돌려 보는 자동 검사
// 실행: godot --headless --path game -- --smoke-test [--days 20] [--seed 42]
//
// 하는 일: N일을 진행하며 매 프레임 불변 조건을 검사하고, 카메라·배속을 건드려 보고,
// 끝에 저장 → 불러오기 해시를 비교합니다. 엔진 오류·경고가 하나라도 있으면 실패(종료 코드 1).
// 마일스톤 2부터는 자동 조종(오토파일럿)이 건물과 도로를 짓고, 검사창·주민 목록·일꾼 명령도 눌러 봅니다.
// 마일스톤 3부터는 부서진 건물을 고치고, 시장에서 사고팔고, 떠돌이 상인과 거래하고, 시장·상인 창도 열어 봅니다.
// 마일스톤 4부터는 위협이 오는 쪽에 감시탑을 짓고 병영을 세웁니다. 습격이 오면 한 프레임에 한 틱씩 진행하며
// CombatDirector 를 직접 돌려(같은 규칙, 빠르게) 적·화살·경비병·보고가 모두 실제로 돌아가는지 봅니다.
// 습격 중에는 저장이 거절되는지도 확인합니다. 20일이면 첫 습격(5일째 도적)이 반드시 옵니다.
// 마일스톤 5부터는 조건이 차면 승급하고, 요새가 되면 마탑을 지어 가장 싼 연구를 시작합니다.
// 등급이 모자랄 때 연구가 거절되는지, 튜토리얼 목표가 차례로 채워지는지도 확인하고, 연구·등급 창을 열어 봅니다.
// 패배(성채 함락·주민 0명)하면 실패로 봅니다.
using System;
using System.Collections.Generic;
using Godot;
using MagicCastle.Audio;
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
    private int _raidsStarted;
    private int _raidsEnded;
    private int _raidFrames;
    private int _totalKills;
    private bool _saveBlockChecked;
    private readonly HashSet<int> _walledDirections = new();
    private bool _lockedChecked;
    private int _ranksGained;
    private int _researchStarted;
    private int _researchDone;
    private int _tutorialSteps;

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        _main = GetParent<Main>();
        GameClock.Instance.ManualMode = true;
        VillagerAgent.InstantTravel = true; // 걷는 애니메이션을 기다리지 않고 바로 도착을 알립니다
        _main.Combat.ManualStep = true;     // 싸움은 아래에서 틱마다 직접 진행
        _main.Sync.EventPolled += OnCoreEvent;
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

        if (_main.Sync.Summary.RaidId != 0)
        {
            if (!StepRaid(config))
            {
                return;
            }
        }
        // 1/8일씩 진행 (렌더링·UI 갱신·도착 보고가 여러 번 돌도록 프레임을 나눔)
        else if (!GameClock.Instance.Advance(config.TicksPerDay / 8))
        {
            Finish(false, TickFailure());
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
        if (_finished)
        {
            return;
        }
        Exercise(_frames);

        if (_main.Sync.Summary.Tick >= _targetTick && _main.Sync.Summary.RaidId == 0)
        {
            Finish(CheckColony(out string colony) && CheckRaids(out colony) && CheckProgress(out colony) &&
                   CheckSaveLoad(out colony), colony);
        }
    }

    // ---- 습격 ------------------------------------------------------------

    /// <summary>습격 중: 한 틱 진행 + 싸움 10분 진행. 실패하면 false (Finish 를 이미 부름)</summary>
    private bool StepRaid(in CoreConfigView config)
    {
        _raidFrames++;
        if (!_saveBlockChecked)
        {
            _saveBlockChecked = true;
            CoreResult saved = CoreBridge.Instance.Save(SavePath);
            if (saved.Code != CoreResultCode.Rejected)
            {
                Finish(false, $"save during a raid should be rejected, got {saved.Name}");
                return false;
            }
            GD.Print("raid: save correctly blocked");
        }
        // 싸움을 먼저 진행해 보고가 이번 틱 안에 들어가게 함 (보통 플레이와 같은 순서: 틱 사이에 싸움)
        _main.Combat.Step(config.MinutesPerTick);
        if (!GameClock.Instance.Advance(1))
        {
            Finish(false, TickFailure());
            return false;
        }
        if (_raidFrames > config.TicksPerDay * 2)
        {
            Finish(false, "raid did not end within two days (C timeout should end it after one)");
            return false;
        }
        return true;
    }

    private void OnCoreEvent(CoreEventView coreEvent)
    {
        switch ((CoreEventType)coreEvent.Type)
        {
            case CoreEventType.ThreatScheduled:
                GD.Print($"threat: {(ThreatType)coreEvent.A} x{coreEvent.D} from {(Direction)coreEvent.B} in {coreEvent.C} ticks");
                break;
            case CoreEventType.RaidStarted:
                _raidsStarted++;
                _raidFrames = 0;
                GD.Print($"raid {coreEvent.A} started: {(ThreatType)coreEvent.B} x{coreEvent.C} (day {_main.Sync.Summary.Day})");
                break;
            case CoreEventType.RaidEnded:
                _raidsEnded++;
                _totalKills += coreEvent.A;
                GD.Print($"raid ended after {_raidFrames} ticks: killed {coreEvent.A}/{coreEvent.B}, stolen {coreEvent.C}, fallen {coreEvent.D}");
                break;
            case CoreEventType.RankUp:
                _ranksGained++;
                GD.Print($"rank up: {(Rank)coreEvent.A} (day {_main.Sync.Summary.Day})");
                break;
            case CoreEventType.ResearchStarted:
                _researchStarted++;
                GD.Print($"research started: {coreEvent.GetText()} (day {_main.Sync.Summary.Day})");
                break;
            case CoreEventType.ResearchDone:
                _researchDone++;
                GD.Print($"research done: {coreEvent.GetText()} (day {_main.Sync.Summary.Day})");
                break;
            case CoreEventType.TutorialStep:
                _tutorialSteps++;
                GD.Print($"tutorial: {(TutorialStep)coreEvent.A} done ({coreEvent.B}/{coreEvent.C}, day {_main.Sync.Summary.Day})");
                break;
            case CoreEventType.TutorialDone:
                GD.Print($"tutorial: all objectives done, reward {coreEvent.A} gold {coreEvent.B} mana");
                break;
            case CoreEventType.Defeat:
                GD.Print($"defeat: {(LoseReason)coreEvent.A} on day {coreEvent.B}");
                break;
        }
    }

    /// <summary>습격이 와서 싸움이 돌았는지: 20일이면 첫 도적 습격(5일째)이 와서 끝나야 합니다</summary>
    private bool CheckRaids(out string reason)
    {
        string counts = $"raids {_raidsEnded}/{_raidsStarted}, enemies killed {_totalKills}";
        GD.Print($"raids: {counts}");
        if (LaunchOptions.Current.SmokeDays >= 6 && _raidsStarted == 0)
        {
            reason = "no raid arrived (the first bandit raid comes on day 5)";
            return false;
        }
        if (_raidsEnded != _raidsStarted)
        {
            reason = $"a raid did not end: {counts}";
            return false;
        }
        if (_main.Combat.EnemyCount != 0)
        {
            reason = $"{_main.Combat.EnemyCount} enemy agents left after the raids";
            return false;
        }
        foreach (CoreVillagerView villager in _main.Sync.Villagers)
        {
            if (_main.Sync.AgentFor(villager.Id) is VillagerAgent agent && agent.CombatControlled)
            {
                reason = $"villager {villager.Id} still under combat control after the raids";
                return false;
            }
        }
        reason = counts;
        return true;
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
        if (_frames % 4 == 2)
        {
            Defend(summary);
        }
        if (_frames % 8 == 4)
        {
            Progress(summary);
        }
        if (_planIndex >= 6 && _frames % 16 == 6)
        {
            Grow(summary);
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

    /// <summary>위협이 예고되면 그쪽에 감시탑을, 주민이 늘면 병영을 짓습니다 (CLI 봇의 bot_defend 와 같은 생각)</summary>
    private void Defend(in CoreSummaryView summary)
    {
        if (summary.ThreatType == (int)ThreatType.None || summary.RaidId != 0)
        {
            return;
        }
        // 먹을 것·집·벌목장을 먼저 (위협이 이틀 안으로 다가오면 그때 짓기 시작)
        bool soon = summary.ThreatTicksLeft <= 2 * CoreBridge.Instance.Config.TicksPerDay;
        if (_planIndex < 3 || !soon && _planIndex < 6)
        {
            return;
        }
        int towers = 0;
        int barracks = 0;
        foreach (CoreBuildingView building in _main.Sync.Buildings)
        {
            towers += building.Type == (int)BuildingType.Tower ? 1 : 0;
            barracks += building.Type == (int)BuildingType.Barracks ? 1 : 0;
        }
        Vector2I keep = new(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize / 2);
        Vector2I toward = (Direction)summary.ThreatDirection switch
        {
            Direction.North => new Vector2I(0, -5),
            Direction.East => new Vector2I(5, 0),
            Direction.South => new Vector2I(0, 5),
            _ => new Vector2I(-5, 0),
        };
        BuildingType type;
        Vector2I center;
        if (towers >= 1 && !_walledDirections.Contains(summary.ThreatDirection) &&
            summary.GetResource(ResourceKind.Stone) >= 40 && summary.GetResource(ResourceKind.Wood) >= 10)
        {
            BuildWall(summary, keep, toward);
            return;
        }
        if (towers < 1 + summary.Population / 8)
        {
            type = BuildingType.Tower;
            center = keep + toward;
        }
        else if (barracks == 0 && summary.Population >= 8)
        {
            type = BuildingType.Barracks;
            center = keep + toward / 2;
        }
        else
        {
            return;
        }
        if (TryFindSpot(type, center, out Vector2I spot) &&
            CoreBridge.Instance.PlaceBuilding(type, spot, out int id, out PlaceResult reason).Ok && reason == PlaceResult.Ok)
        {
            _placed.Add(id);
            GD.Print($"autopilot: {type} at {spot} toward {(Direction)summary.ThreatDirection} (day {summary.Day})");
        }
    }

    /// <summary>
    /// 위협 쪽에 성벽 한 줄 (가운데에 성문). 적이 돌아가거나 부수고 지나가는 길을 시험합니다.
    /// 성채에서 9~14칸 사이에서 성문 자리가 비어 있고 놓을 수 있는 칸이 가장 많은 줄을 고릅니다.
    /// </summary>
    private void BuildWall(in CoreSummaryView summary, Vector2I keep, Vector2I toward)
    {
        _walledDirections.Add(summary.ThreatDirection);
        Vector2I unit = toward / 5;
        Vector2I side = unit.X != 0 ? new Vector2I(0, 1) : new Vector2I(1, 0);
        Vector2I bestMiddle = default;
        int bestCells = -1;
        for (int distance = 9; distance <= 14; distance++)
        {
            Vector2I middle = keep + unit * distance;
            if (CoreBridge.Instance.CanPlace(BuildingType.Gate, middle).Result != (int)PlaceResult.Ok)
            {
                continue;
            }
            int cells = 0;
            for (int offset = -5; offset <= 5; offset++)
            {
                if (offset != 0 && CoreBridge.Instance.CanPlace(BuildingType.Wall, middle + side * offset).Result == (int)PlaceResult.Ok)
                {
                    cells++;
                }
            }
            if (cells > bestCells)
            {
                bestCells = cells;
                bestMiddle = middle;
            }
        }
        if (bestCells < 0)
        {
            GD.Print($"autopilot: no room for a wall toward {(Direction)summary.ThreatDirection}");
            return;
        }
        CoreBridge.Instance.PlaceBuilding(BuildingType.Gate, bestMiddle, out _, out PlaceResult gate);
        CoreBridge.Instance.PlaceLine(BuildingType.Wall, bestMiddle - side * 5, bestMiddle - side, out int left, out _);
        CoreBridge.Instance.PlaceLine(BuildingType.Wall, bestMiddle + side, bestMiddle + side * 5, out int right, out _);
        GD.Print($"autopilot: wall {left}+{right} cells, gate {gate} at {bestMiddle} toward {(Direction)summary.ThreatDirection} (day {summary.Day})");
    }

    /// <summary>
    /// 기본 계획(농장·집·벌목장·채집·채석)을 지은 뒤 이틀에 한 번: 겨울(농장이 쉬는 철)이 오기 전에 먹을 것이 모자라면 농장, 집이 꽉 차면 집,
    /// 나무가 바닥이면 벌목장, 마을이 되면 마정석 광산 하나. 오래 돌리는 스모크 테스트(60일 등)가 겨울을 넘기게 합니다.
    /// </summary>
    private void Grow(in CoreSummaryView summary)
    {
        if (summary.RaidId != 0)
        {
            return;
        }
        int farms = CountOf(BuildingType.Farm);
        int lumbers = CountOf(BuildingType.Lumber);
        int population = summary.Population;
        bool winter = summary.Season == CoreBridge.Instance.Config.SeasonsPerYear - 1;
        BuildingType type;
        if (!winter && summary.GetResource(ResourceKind.Food) < summary.WinterFoodNeed + population * 2 && farms < 2 + population / 3)
        {
            type = BuildingType.Farm;
        }
        else if (summary.GetResource(ResourceKind.Wood) < 20 && lumbers < 1 + population / 6)
        {
            type = BuildingType.Lumber;
        }
        else if (summary.Rank >= (int)Rank.Village && CountOf(BuildingType.ManaMine) == 0)
        {
            type = BuildingType.ManaMine;
        }
        else if (summary.Homeless > 0 || summary.HousingCapacity <= population)
        {
            type = BuildingType.House;
        }
        else
        {
            return;
        }
        Vector2I keep = new(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize / 2);
        if (TryFindSpot(type, keep, out Vector2I spot) &&
            CoreBridge.Instance.PlaceBuilding(type, spot, out int id, out PlaceResult placed).Ok && placed == PlaceResult.Ok)
        {
            _placed.Add(id);
            GD.Print($"autopilot: {type} at {spot} (day {summary.Day}, grow)");
        }
    }

    private void StaffMageTower()
    {
        foreach (CoreBuildingView tower in _main.Sync.Buildings)
        {
            if (tower.Type != (int)BuildingType.MageTower || tower.State != (int)BuildingState.Active || tower.Workers >= 1)
            {
                continue;
            }
            foreach (CoreVillagerView villager in _main.Sync.Villagers)
            {
                if (villager.Job == (int)BuildingType.Farm)
                {
                    CoreBridge.Instance.AssignWorker(villager.Id, tower.Id);
                    GD.Print($"autopilot: villager {villager.Id} moved from a farm to the mage tower (day {_main.Sync.Summary.Day})");
                    return;
                }
            }
        }
    }

    private int CountOf(BuildingType type)
    {
        int count = 0;
        foreach (CoreBuildingView building in _main.Sync.Buildings)
        {
            count += building.Type == (int)type ? 1 : 0;
        }
        return count;
    }

    /// <summary>
    /// 승급과 연구 (CLI 봇의 bot_progress 와 같은 생각): 조건이 차면 승급, 요새가 되면 마탑을 짓고
    /// 가장 싼 연구를 시작합니다. 요새가 되기 전에는 연구가 "잠김"으로 거절되는지 한 번 확인합니다.
    /// </summary>
    private void Progress(in CoreSummaryView summary)
    {
        if (summary.RaidId != 0 || summary.Outcome != (int)Outcome.None)
        {
            return;
        }
        if (summary.RankReady != 0)
        {
            CoreResult result = CoreBridge.Instance.RankUp(out DenyReason why);
            if (!result.Ok)
            {
                GD.Print($"autopilot: rank up refused ({why})");
            }
        }
        CoreBridge core = CoreBridge.Instance;
        if (!_lockedChecked && summary.Rank < (int)Rank.Fortress && core.TryGetResearch(0, out CoreResearchView first) &&
            first.RequiredRank > summary.Rank && summary.ResearchActive < 0)
        {
            _lockedChecked = true;
            core.StartResearch(0, out DenyReason locked);
            GD.Print($"autopilot: research before the fortress -> {locked}");
            if (locked != DenyReason.ResearchLocked)
            {
                Finish(false, $"research below its rank should be refused as locked, got {locked}");
            }
            return;
        }
        if (summary.Rank < (int)Rank.Fortress)
        {
            return;
        }
        if (CountOf(BuildingType.MageTower) == 0)
        {
            Vector2I keep = new(summary.KeepX + summary.KeepSize / 2, summary.KeepY + summary.KeepSize / 2);
            if (TryFindSpot(BuildingType.MageTower, keep, out Vector2I spot) &&
                core.PlaceBuilding(BuildingType.MageTower, spot, out int id, out PlaceResult placed).Ok && placed == PlaceResult.Ok)
            {
                _placed.Add(id);
                GD.Print($"autopilot: MageTower at {spot} (day {summary.Day})");
            }
            return;
        }
        // 놀고 있는 주민만 저절로 일자리를 얻으므로, 먹을 것이 넉넉하면 농부 한 명을 마탑으로 옮깁니다 (CLI 봇의 bot_staff)
        bool winter = summary.Season == core.Config.SeasonsPerYear - 1;
        if (!winter && summary.GetResource(ResourceKind.Food) > summary.Population * 8)
        {
            StaffMageTower();
        }
        if (summary.ResearchActive >= 0)
        {
            return;
        }
        int cheapest = -1;
        int cheapestCost = int.MaxValue;
        for (int index = 0; core.TryGetResearch(index, out CoreResearchView research); index++)
        {
            int cost = research.CostGold + research.CostMana * 4;
            if (research.State == (int)ResearchState.Available && cost < cheapestCost)
            {
                cheapest = index;
                cheapestCost = cost;
            }
        }
        if (cheapest >= 0 && core.TryGetResearch(cheapest, out CoreResearchView pick) &&
            summary.GetResource(ResourceKind.Gold) >= pick.CostGold + 60 && summary.GetResource(ResourceKind.Mana) >= pick.CostMana)
        {
            core.StartResearch(cheapest, out _);
        }
    }

    /// <summary>진행이 멈춘 까닭: 패배면 그 이유를, 아니면 core_tick 실패</summary>
    private string TickFailure()
    {
        CoreBridge.Instance.TryGetSummary(out CoreSummaryView summary);
        return summary.Outcome == (int)Outcome.Lost
            ? $"the colony was defeated on day {summary.Day} ({(LoseReason)summary.LoseReason})"
            : "core_tick failed";
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
            case 15:
                _main.Hud.Research.Toggle();
                _main.Hud.Ranks.Toggle();
                break;
            case 0:
                if (_main.Hud.Research.Visible)
                {
                    _main.Hud.Research.Toggle();
                    _main.Hud.Ranks.Toggle();
                }
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

    /// <summary>
    /// 진행 검사: 튜토리얼 목표가 적어도 셋(농장·집·일꾼 배치) 채워지고, 요약의 비트와 알림 수가 맞아야 합니다.
    /// 20일이면 마을로 한 번은 승급해야 합니다(인구 8, 기분 50, 방어 3, 공물 30금 5마정석).
    /// </summary>
    private bool CheckProgress(out string reason)
    {
        CoreSummaryView summary = _main.Sync.Summary;
        int bits = 0;
        for (int step = 0; step < (int)TutorialStep.Count; step++)
        {
            bits += (summary.TutorialDone >> step) & 1;
        }
        string counts = $"rank {(Rank)summary.Rank} (+{_ranksGained}), research {_researchDone}/{_researchStarted}," +
                        $" tutorial {bits}/{(int)TutorialStep.Count}, defense {summary.Defense}";
        GD.Print($"progress: {counts}");
        if (bits != _tutorialSteps)
        {
            reason = $"tutorial bits ({bits}) do not match step events ({_tutorialSteps})";
            return false;
        }
        if (LaunchOptions.Current.SmokeDays >= 6 && bits < 3)
        {
            reason = $"tutorial objectives did not progress: {counts}";
            return false;
        }
        if (LaunchOptions.Current.SmokeDays >= 20 && _ranksGained == 0)
        {
            reason = $"no rank up in {LaunchOptions.Current.SmokeDays} days: {counts}";
            return false;
        }
        if (summary.Rank != _ranksGained || summary.Outcome != (int)Outcome.None)
        {
            reason = $"rank or outcome out of step: {counts}, outcome {summary.Outcome}";
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
            // 플레이어가 창을 닫을 때와 같은 길로 끝냄 (Main 이 소리를 멈추고, CoreBridge 가 월드를 해제)
            GetTree().Root.PropagateNotification((int)NotificationWMCloseRequest);
            return;
        }
        string why = errors > 0 ? $"{errors} engine errors/warnings:\n{ErrorCounter.Summary()}" : reason;
        GD.Print($"SMOKE TEST FAILED: {why}");
        Console.Error.WriteLine($"SMOKE TEST FAILED: {why}");
        Sfx.QuitQuietly(GetTree(), 1);
    }
}
