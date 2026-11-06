// Strings.cs — 플레이어에게 보이는 모든 한국어 문장
//
// 스크립트 곳곳에 한국어를 흩어 두지 않고 여기 한곳에 모읍니다 (SPEC §7).
// C 코어는 숫자 코드만 내보내고, 그 코드를 문장으로 바꾸는 일도 여기서 합니다.
using System.Collections.Generic;
using MagicCastle.Core;

namespace MagicCastle.UI;

public static class Strings
{
    // ---- 일반 ----
    public const string GameTitle = "마법성채 키우기";
    public const string DefaultCastleName = "바람언덕";

    // ---- 시작 실패 (CoreBridge) ----
    public const string LibraryMissing =
        "게임 코어(castle_core)를 찾을 수 없습니다.\nREADME 의 \"C 코어 빌드\" 순서대로 DLL 을 빌드한 뒤 다시 실행하세요.";
    public const string LibraryWrongArchitecture =
        "게임 코어(castle_core)가 이 컴퓨터와 맞지 않습니다.\n64비트(x64)로 다시 빌드하세요.";
    public const string AbiMismatch =
        "게임 코어(castle_core)의 버전이 게임과 다릅니다.\nC 코어를 다시 빌드한 뒤 Godot 에서 C# 도 다시 빌드하세요.";
    public const string NewGameFailed = "새 게임을 만들지 못했습니다";
    public const string FatalTitle = "시작할 수 없습니다";

    public static string DataFileMissing(string path) => $"정의 파일이 없어 기본값을 씁니다: {path}";

    private static readonly string[] DataFileNames =
    {
        "buildings.def", "traits.def", "names.txt", "enemies.def", "research.def", "ranks.def",
    };

    // ---- 조사 ----
    /// <summary>마지막 글자에 받침이 있으면 withFinal("이", "을"), 없으면 withoutFinal("가", "를")</summary>
    public static string Josa(string word, string withFinal, string withoutFinal)
    {
        if (word.Length == 0)
        {
            return withoutFinal;
        }
        char last = word[^1];
        if (last < '가' || last > '힣')
        {
            return withoutFinal;
        }
        // 한글 음절 = '가' + (초성×21 + 중성)×28 + 종성. 종성이 0 이면 받침 없음
        return (last - '가') % 28 != 0 ? withFinal : withoutFinal;
    }

    /// <summary>"에드윈이", "레아나가"</summary>
    public static string Subject(string name) => name + Josa(name, "이", "가");

    /// <summary>"농장을", "창고를"</summary>
    public static string Object(string word) => word + Josa(word, "을", "를");

    // ---- 이름표 ----
    private static readonly string[] SeasonNames = { "봄", "여름", "가을", "겨울" };
    private static readonly string[] DifficultyNames = { "평화", "보통", "혹독" };
    private static readonly string[] ResourceNames = { "금화", "마정석", "식량", "목재", "석재", "약초" };

    private static readonly string[] ResourceDescriptions =
    {
        "오늘의 경제를 돌리는 돈입니다. 건설, 임금, 거래에 씁니다. 집이 있는 주민이 매일 세금을 냅니다.",
        "미래를 위한 귀한 힘입니다. 연구, 마법 건물, 승급에 씁니다.",
        "주민이 먹습니다. 겨울에는 농장이 멈추니 가을에 넉넉히 모아 두세요.",
        "건설에 씁니다. 겨울에는 난방에도 씁니다.",
        "튼튼한 건물과 성벽에 씁니다.",
        "치료소에서 병을 고치는 데 씁니다.",
    };

    private static readonly string[] TerrainNames = { "풀밭", "숲", "바위 지대", "물", "마정석 광맥" };

    private static readonly string[] BuildingNames =
    {
        "성채", "주택", "농장", "채집 오두막", "벌목장", "채석장", "마정석 광산", "창고",
        "시장", "병영", "감시탑", "성벽", "성문", "치료소", "마탑", "도로",
    };

    private static readonly string[] BuildingDescriptions =
    {
        "영지의 심장. 자원을 보관합니다. 무너지면 패배합니다.",
        "주민 4명이 삽니다. 집이 있어야 기분이 좋고, 빈집이 있어야 이주민이 옵니다.",
        "식량을 기릅니다. 여름에 가장 잘 되고, 겨울에는 멈춥니다.",
        "숲 근처에서 약초와 약간의 식량을 모읍니다.",
        "주변 숲이 많을수록 목재가 많이 나옵니다.",
        "바위 지대 바로 옆에 지어야 합니다. 주변 바위가 많을수록 석재가 많이 나옵니다.",
        "마정석 광맥 위에 지어야 합니다.",
        "저장 한도를 늘립니다. 겨울을 대비해 식량을 쌓아 두려면 필요합니다.",
        "자원을 사고팝니다.",
        "경비병이 습격에 맞서 싸웁니다.",
        "다가오는 적에게 활을 쏩니다.",
        "끌어서 줄로 놓습니다. 주민은 지나가지 못하고, 적은 부숴야 지나갑니다.",
        "주민은 지나가고 적은 막는 문입니다.",
        "약초로 아픈 주민을 고칩니다.",
        "마법사가 연구를 합니다.",
        "끌어서 줄로 놓습니다. 도로 위에서는 두 배 빨리 걷습니다.",
    };

    private static readonly string[] CategoryNames = { "특별", "주거", "자원", "저장·거래", "방어", "치료·마법", "도로" };

    private static readonly string[] PlaceResults =
    {
        "지을 수 있습니다", "지을 수 없는 종류입니다", "지도 밖입니다", "지도 가장자리에 너무 가깝습니다",
        "물이나 바위 위에는 지을 수 없습니다", "다른 건물이 있습니다", "도로가 있습니다 (먼저 철거하세요)",
        "다른 건물의 문 앞을 막습니다", "문 앞 자리가 막혀 있습니다", "근처에 숲이 부족합니다",
        "바로 옆에 바위 지대가 있어야 합니다", "마정석 광맥 위에 지어야 합니다", "등급이 모자랍니다",
        "자원이 모자랍니다", "이미 있습니다", "다른 건물이나 지도 밖으로 가는 길을 막습니다",
        "성채에서 그곳까지 걸어갈 길이 없습니다",
    };

    private static readonly string[] VillagerStates =
    {
        "광장에서 쉬는 중", "자는 중", "일하러 가는 중", "일하는 중", "먹으러 가는 중", "치료받으러 가는 중",
        "피신 중", "경비 중", "마을을 떠나는 중",
    };

    private static readonly Dictionary<string, string> TraitNames = new()
    {
        ["diligent"] = "근면함", ["lazy"] = "게으름", ["glutton"] = "대식가", ["coward"] = "겁쟁이",
        ["brave"] = "용감함", ["mana_affinity"] = "마력 친화", ["optimist"] = "낙천가", ["grumbler"] = "투덜이",
        ["strong"] = "장사", ["frugal"] = "검소함",
    };

    private static readonly string[] MoodTermNames = { "기본", "배고픔", "피로", "안전", "집", "성격", "임금", "슬픔", "추위", "병" };

    private static readonly string[] WeatherNames = { "맑음", "비", "눈", "폭풍" };

    private static readonly string[] WeatherDescriptions =
    {
        "맑은 날입니다.",
        "비가 옵니다. 농장 생산 +10%.",
        "눈이 옵니다. 걷는 속도가 조금 느려집니다.",
        "폭풍이 붑니다. 건물이 부서질 수 있고 걷기가 느려집니다.",
    };

    private static readonly string[] NatureNames = { "", "가뭄", "역병", "폭풍", "혹한", "풍년" };

    private static readonly string[] NatureDescriptions =
    {
        "",
        "비가 오지 않아 농장 생산이 절반으로 줄어듭니다.",
        "병이 돕니다. 아픈 주민은 일하지 못합니다. 치료소(일꾼 + 약초)가 있으면 금방 낫습니다. 이주민도 오지 않습니다.",
        "강한 바람에 건물이 부서집니다. 검사창에서 고치세요.",
        "매서운 추위로 집마다 난방 목재가 하나 더 듭니다.",
        "풍년! 농장 생산이 1.5배가 됩니다.",
    };

    private static readonly string[] LedgerNames =
    {
        "생산", "식사", "임금", "세금", "건설", "철거 환불", "거래", "난방", "약탈", "기타", "치료", "수리", "승급 공물",
        "연구",
    };

    private static readonly string[] DenyReasons =
    {
        "", "다 지은 시장이 없습니다", "시장에 일꾼이 없습니다", "시장에서 다루지 않는 자원입니다", "금화가 모자랍니다",
        "팔 물건이 모자랍니다", "창고에 자리가 없습니다", "상인이 없습니다", "이미 한 거래입니다", "고칠 곳이 없습니다",
        "아직 짓는 중입니다", "수량이 범위 밖입니다", "수리 비용이 모자랍니다", "습격 중입니다",
        "이미 가장 높은 등급입니다", "승급 조건(인구·기분·방어·연구)이 모자랍니다", "마정석이 모자랍니다",
        "이미 다른 연구를 하는 중입니다", "이미 끝낸 연구입니다", "앞선 연구나 등급이 모자랍니다", "게임이 끝났습니다",
    };

    public static string SeasonName(int season) => Pick(SeasonNames, season);
    public static string DifficultyName(int difficulty) => Pick(DifficultyNames, difficulty);
    public static string ResourceName(ResourceKind resource) => Pick(ResourceNames, (int)resource);
    public static string ResourceName(int resource) => Pick(ResourceNames, resource);
    public static string TerrainName(int terrain) => Pick(TerrainNames, terrain);
    public static string BuildingName(int type) => Pick(BuildingNames, type);
    public static string BuildingName(BuildingType type) => Pick(BuildingNames, (int)type);
    public static string BuildingDescription(BuildingType type) => Pick(BuildingDescriptions, (int)type);
    public static string CategoryName(BuildCategory category) => Pick(CategoryNames, (int)category);
    public static string PlaceResultText(PlaceResult result) => Pick(PlaceResults, (int)result);
    public static string VillagerStateText(int state) => Pick(VillagerStates, state);
    public static string MoodTermName(int term) => Pick(MoodTermNames, term);
    public static string TraitName(string key) => TraitNames.TryGetValue(key, out string? name) ? name : key;
    public static string WeatherName(int weather) => Pick(WeatherNames, weather);
    public static string NatureName(int nature) => Pick(NatureNames, nature);
    public static string LedgerName(int category) => Pick(LedgerNames, category);
    public static string DenyText(DenyReason reason) => Pick(DenyReasons, (int)reason);

    // ---- 날씨, 자연 사건, 겨울 준비 (위쪽 막대) ----
    public static string WeatherLabel(in CoreSummaryView s) => WeatherName(s.Weather);

    public static string WeatherTooltip(in CoreSummaryView s)
    {
        string text = $"날씨: {WeatherName(s.Weather)}\n{Pick(WeatherDescriptions, s.Weather)}\n\n농장 생산 {s.FarmPercent}%";
        text += s.Season == 3 ? " (겨울에는 농장이 멈춥니다)" : " (계절 × 날씨 × 자연 사건)";
        text += s.HeatingPerHouse > 0
            ? $"\n난방: 사람이 사는 집마다 밤에 목재 {s.HeatingPerHouse}"
            : "\n난방: 여름에는 필요 없음";
        if (s.ForecastEvent != 0)
        {
            text += $"\n\n예보: 내일 {NatureName(s.ForecastEvent)} — {Pick(NatureDescriptions, s.ForecastEvent)}";
        }
        return text;
    }

    /// <summary>진행 중인 사건 (없으면 예보, 둘 다 없으면 빈 문자열)</summary>
    public static string NatureLabel(in CoreSummaryView s)
    {
        if (s.NatureEvent != 0)
        {
            return $"{NatureName(s.NatureEvent)} {s.NatureDaysLeft}일";
        }
        return s.ForecastEvent != 0 ? $"내일 {NatureName(s.ForecastEvent)}" : "";
    }

    public static string NatureTooltip(in CoreSummaryView s)
    {
        int nature = s.NatureEvent != 0 ? s.NatureEvent : s.ForecastEvent;
        string when = s.NatureEvent != 0 ? $"{s.NatureDaysLeft}일 더 이어집니다." : "내일 아침에 시작합니다. 미리 준비하세요.";
        string text = $"{NatureName(nature)}: {Pick(NatureDescriptions, nature)}\n{when}";
        if (s.SickCount > 0)
        {
            text += $"\n아픈 주민 {s.SickCount}명";
        }
        return text;
    }

    /// <summary>가을·겨울에만: "겨울 준비 식량 120/300 · 목재 40/96"</summary>
    public static string WinterLabel(in CoreSummaryView s) =>
        s.WinterFoodNeed > 0 || s.WinterWoodNeed > 0
            ? $"겨울 준비 {ResourceName(ResourceKind.Food)} {s.GetResource(ResourceKind.Food)}/{s.WinterFoodNeed} · " +
              $"{ResourceName(ResourceKind.Wood)} {s.GetResource(ResourceKind.Wood)}/{s.WinterWoodNeed}"
            : "";

    public static bool WinterShort(in CoreSummaryView s) =>
        s.GetResource(ResourceKind.Food) < s.WinterFoodNeed || s.GetResource(ResourceKind.Wood) < s.WinterWoodNeed;

    public static string WinterTooltip(in CoreSummaryView s) =>
        (s.Season == 2 ? "곧 겨울입니다. 겨울에는 농장이 멈추고 집마다 난방 목재가 두 배로 듭니다.\n"
                       : "겨울입니다. 농장이 멈췄습니다. 봄까지 버텨야 합니다.\n") +
        $"봄까지 필요한 양 (지금 주민 {s.Population}명, 사는 집 기준):\n" +
        $"  식량 {s.WinterFoodNeed} (주민 한 명이 하루에 약 1.2)\n" +
        $"  목재 {s.WinterWoodNeed} (집 한 채가 하룻밤에 2)\n" +
        "모자라면 시장이나 떠돌이 상인에게서 살 수 있습니다." +
        (s.ColdHouses > 0 ? $"\n지난밤 난방을 못 한 집: {s.ColdHouses}채" : "");

    public const string MarketButton = "시장";
    public const string MarketButtonTooltip = "시장에서 금화로 식량·목재·석재·약초를 사고팝니다.";
    public static string MerchantButton(int days) => $"떠돌이 상인 ({days}일)";
    public const string MerchantButtonTooltip = "떠돌이 상인이 왔습니다. 마정석을 살 수 있는 유일한 기회입니다.";

    private static string Pick(string[] names, int index) =>
        index >= 0 && index < names.Length ? names[index] : "?";

    // ---- HUD ----
    public static string Date(in CoreSummaryView summary) =>
        $"{summary.Year}년 {SeasonName(summary.Season)} {summary.DayOfSeason}일";

    public static string DayCount(int day) => $"{day}일째";

    public static string Clock(int minuteOfDay) => $"{minuteOfDay / 60:00}:{minuteOfDay % 60:00}";

    public static string DateTooltip(in CoreSummaryView summary, in CoreConfigView config) =>
        $"{summary.Year}년 {SeasonName(summary.Season)} {summary.DayOfSeason}일, 게임을 시작하고 {summary.Day}일째입니다.\n" +
        $"한 계절은 {config.DaysPerSeason}일, 한 해는 {config.DaysPerSeason * config.SeasonsPerYear}일입니다.\n" +
        $"난이도: {DifficultyName(summary.Difficulty)} (시드 {summary.Seed})";

    public static string ClockTooltip(in CoreConfigView config) =>
        $"게임 속 시각입니다. 1배속에서 게임 속 {config.MinutesPerTick}분이 실제 {config.RealMsPerTick / 1000.0:0.#}초입니다.\n" +
        $"{Clock(config.WorkStartMinute)}~{Clock(config.WorkEndMinute)}에 일하고, " +
        $"{Clock(config.NightStartMinute)}부터 {Clock(config.NightEndMinute)}까지는 밤이라 잠을 잡니다.";

    public static string ResourceLabel(int amount, int delta) =>
        delta == 0 ? $"{amount}" : $"{amount} ({delta:+#;-#})";

    /// <summary>capacity 가 0 이면 저장 한도가 없는 자원</summary>
    public static string ResourceTooltip(ResourceKind resource, int amount, int capacity, int delta)
    {
        string text = $"{ResourceName(resource)}: {amount}\n{Pick(ResourceDescriptions, (int)resource)}";
        text += capacity > 0 ? $"\n저장 한도 {capacity} (성채와 창고가 정합니다)" : "\n화폐라서 저장 한도가 없습니다.";
        text += $"\n어제 하루 변화: {delta:+#;-#;0}";
        return text;
    }

    /// <summary>장부 내역까지 넣은 툴팁: 어제 하루 항목별, 그리고 오늘 지금까지</summary>
    public static string ResourceTooltip(ResourceKind resource, int amount, int capacity, int delta, in CoreLedgerView ledger)
    {
        var text = new System.Text.StringBuilder(ResourceTooltip(resource, amount, capacity, delta));
        AppendLedger(text, "어제 내역", resource, ledger, true);
        AppendLedger(text, "오늘 지금까지", resource, ledger, false);
        return text.ToString();
    }

    private static void AppendLedger(System.Text.StringBuilder text, string title, ResourceKind resource,
        in CoreLedgerView ledger, bool yesterday)
    {
        bool any = false;
        for (int c = 0; c < (int)LedgerCategory.Count; c++)
        {
            int value = ledger.Get(resource, (LedgerCategory)c, yesterday);
            if (value == 0)
            {
                continue;
            }
            if (!any)
            {
                text.Append($"\n\n{title}:");
                any = true;
            }
            text.Append($"\n  {LedgerName(c)} {value:+#;-#;0}");
        }
    }

    public static string Population(in CoreSummaryView s) => $"주민 {s.Population}";

    public static string PopulationTooltip(in CoreSummaryView s) =>
        $"주민 {s.Population}명, 평균 기분 {s.AverageMood}\n" +
        $"집 자리 {s.HousingCapacity} (집 없는 주민 {s.Homeless}명)\n" +
        $"일자리 {s.JobSlots} (일하는 주민 {s.Employed}명)\n" +
        "빈집이 있고 기분이 좋고 식량이 넉넉하면 아침마다 이주민이 옵니다 (겨울 제외).";

    public const string SpeedPause = "||";
    public const string SpeedPauseTooltip = "일시정지 (스페이스)\n멈춘 동안에도 화면을 둘러보고 건물을 놓을 수 있습니다.";

    public static string SpeedLabel(int speed) => $"{speed}×";

    public static string SpeedTooltip(int speed) => $"{speed}배속 (숫자 키 {speed})";

    public const string PausedBanner = "일시정지 — 스페이스 키로 계속합니다";
    public const string ScreenshotSampleToast = "성채 주변을 둘러봅니다.";
    public const string ToastJumpTooltip = "누르면 카메라가 그곳으로 갑니다.";
    public const string VillagerListButton = "주민 목록";
    public const string VillagerListTooltip = "모든 주민을 봅니다 (V). 줄을 누르면 그 주민에게 갑니다.";
    public const string DemolishTool = "철거";
    public const string DemolishTooltip = "철거 도구 (X): 건물이나 도로를 눌러 없앱니다. 비용의 절반을 돌려받습니다.";

    // ---- 건설 ----
    public static string Cost(in CoreBuildingDefView def)
    {
        var parts = new List<string>();
        for (int i = 0; i < CoreSummaryView.ResourceCount; i++)
        {
            int amount = def.GetCost((ResourceKind)i);
            if (amount > 0)
            {
                parts.Add($"{ResourceName(i)} {amount}");
            }
        }
        return parts.Count == 0 ? "무료" : string.Join(", ", parts);
    }

    public static string BuildTooltip(in CoreBuildingDefView def, string? disabledReason)
    {
        var type = (BuildingType)def.Type;
        string text = $"{BuildingName(type)} ({def.W}×{def.H})\n{BuildingDescription(type)}\n비용: {Cost(def)}";
        if (def.BuildTicks > 0)
        {
            text += $"\n짓는 시간: {def.BuildTicks * 10 / 60}시간 {def.BuildTicks * 10 % 60}분";
        }
        if (def.WorkerSlots > 0)
        {
            text += $"\n일자리 {def.WorkerSlots}, 하루 임금 금화 {def.Wage}";
        }
        if (def.ResidentSlots > 0)
        {
            text += $"\n거주 {def.ResidentSlots}명";
        }
        if (def.Storage > 0)
        {
            text += $"\n저장 한도 +{def.Storage}";
        }
        if (def.Attack > 0)
        {
            string who = type == BuildingType.Barracks ? "경비병 한 명" : "화살";
            text += $"\n{who}: 피해 {def.Attack}, 거리 {def.AttackRange}칸, {def.AttackCooldown}분마다";
        }
        if (disabledReason != null)
        {
            text += $"\n\n지금 지을 수 없음: {disabledReason}";
        }
        return text;
    }

    public static string RankRequired(int rank) => $"{RankName(rank)} 등급부터 지을 수 있습니다 (위쪽 막대의 등급 단추로 승급)";

    public static string PlacementHint(in CorePlacementView view, BuildingType type)
    {
        var result = (PlaceResult)view.Result;
        string text = result == PlaceResult.Ok ? $"{BuildingName(type)}: 클릭해서 짓기" : PlaceResultText(result);
        if (view.TerrainCount >= 0)
        {
            text += $"\n주변 지형 {view.TerrainCount}칸 → 생산성 {view.Productivity}%";
        }
        return text;
    }

    public static string LinePlaced(BuildingType type, int placed) => $"{BuildingName(type)} {placed}칸을 놓았습니다.";

    public static string BuildRejected(PlaceResult reason) => $"지을 수 없습니다: {PlaceResultText(reason)}";

    public const string BuildModeHint = "왼쪽 클릭: 놓기 · 오른쪽 클릭 / Esc: 취소";
    public const string LineModeHint = "끌어서 줄로 놓기 · 오른쪽 클릭 / Esc: 취소";
    public const string DemolishModeHint = "철거할 건물이나 도로를 누르세요 · 오른쪽 클릭 / Esc: 취소";

    // ---- 검사창 ----
    private static readonly string[] BuildingStateNames = { "짓는 중", "운영 중", "무너짐" };

    public static string BuildingStateText(int state) => Pick(BuildingStateNames, state);

    public static string Progress(int progress, int total) =>
        total > 0 ? $"짓는 중 {progress * 100 / total}%" : "짓는 중";

    public static string Workers(int count, int slots) => $"일꾼 {count}/{slots}";
    public static string Residents(int count, int slots) => $"거주민 {count}/{slots}";
    public static string Productivity(int percent) => $"생산성 {percent}% (주변 지형)";
    public static string Hp(int hp, int max) => $"체력 {hp}/{max}";

    public const string AutoAssign = "자동 배치";
    public const string AutoAssignTooltip = "켜면 놀고 있는 주민 중 알맞은 사람을 이 건물에 자동으로 보냅니다.";
    public const string AddWorker = "일꾼 넣기";
    public const string AddWorkerTooltip = "고른 주민을 이 건물의 일꾼으로 직접 보냅니다 (자동 배치가 더는 옮기지 않음).";
    public const string RemoveWorker = "빼기";
    public const string RemoveWorkerTooltip = "이 주민을 일터에서 뺍니다. 자동 배치도 다시 넣지 않습니다.";
    public const string Demolish = "철거";
    public const string NoIdleVillagers = "(일터 없는 주민 없음)";
    public const string ReleaseWorker = "자동에 맡기기";
    public const string ReleaseWorkerTooltip = "직접 배치 표시를 지워 자동 배치가 다시 일터를 정하게 합니다.";
    public const string Unemployed = "일터 없음";
    public const string Homeless = "집 없음";
    public const string Close = "닫기";
    public const string Repair = "수리";
    public const string RepairNothing = "고칠 곳이 없습니다.";
    public const string RuinedLine = "무너짐 — 고치기 전에는 일도 잠도 못 합니다";
    public const string ColdHouseLine = "지난밤 추웠습니다 (목재 부족)";
    public const string TradeButton = "거래하기";
    public const string SickLine = "아픔 — 일하지 못합니다";

    public static string RepairTooltip(int[] cost)
    {
        var parts = new List<string>();
        for (int i = 0; i < cost.Length; i++)
        {
            if (cost[i] > 0)
            {
                parts.Add($"{ResourceName(i)} {cost[i]}");
            }
        }
        return parts.Count > 0 ? "수리 비용: " + string.Join(", ", parts) + "\n(건설 비용의 절반 × 부서진 정도)" : RepairNothing;
    }

    public const string RepairDuringRaid = "습격 중에는 고칠 수 없습니다. 습격이 끝나면 고치세요.";
    public static string RepairRejected(DenyReason reason) => $"고칠 수 없습니다: {DenyText(reason)}";
    public static string HealerLine(int herbs) => $"약초 {herbs}개 — 아픈 주민이 오면 한 시간 치료에 하나씩 씁니다";

    // ---- 시장, 떠돌이 상인 ----
    public const string MarketTitle = "시장";
    public const string MarketHelp = "한 묶음은 10개입니다. 살 때는 비싸고 팔 때는 쌉니다. 많이 사면 값이 오르고, 날마다 조금씩 제자리로 돌아갑니다. 겨울에는 식량·목재가 비쌉니다.";
    public static string MarketClosed(DenyReason reason) => $"지금은 거래할 수 없습니다: {DenyText(reason)}";
    public const string Buy = "사기";
    public const string Sell = "팔기";
    public static string PriceLine(int buy, int sell) => $"살 때 {buy} · 팔 때 {sell}";
    public static string TrendText(int percent) => percent > 110 ? "▲ 비쌈" : percent < 90 ? "▼ 쌈" : "보통";
    public static string PriceTooltip(ResourceKind resource, int basePrice, int percent) =>
        $"{ResourceName(resource)} 기준 가격 {basePrice} (한 묶음). 지금은 기준의 {percent}%입니다.";
    public static string Traded(ResourceKind resource, int lots, int gold) =>
        lots > 0
            ? $"{ResourceName(resource)} {lots * CoreMarketView.Lot}개를 금화 {-gold}에 샀습니다."
            : $"{ResourceName(resource)} {-lots * CoreMarketView.Lot}개를 금화 {gold}에 팔았습니다.";
    public static string TradeRejected(DenyReason reason) => $"거래할 수 없습니다: {DenyText(reason)}";

    public static string MerchantTitle(int days) => $"떠돌이 상인 — {days}일 더 머뭅니다";
    public const string MerchantHelp = "상인의 제안은 한 번씩만 받을 수 있습니다. 마정석은 상인에게서만 살 수 있습니다.";
    public const string MerchantGone = "떠돌이 상인이 떠났습니다.";
    public static string DealText(in CoreDealView deal) =>
        $"{ResourceName(deal.GiveResource)} {deal.GiveAmount} → {ResourceName(deal.GetResource)} {deal.GetAmount}";
    public const string Accept = "거래";
    public const string DealDone = "거래함";

    public static string WorkerLine(string name, int state) => $"{name} — {VillagerStateText(state)}";

    public static string VillagerTitle(in CoreVillagerView v) => $"{v.GetName()} ({v.Age}세)";

    public static string Stats(in CoreVillagerView v) =>
        $"힘 {v.Strength} · 지능 {v.Intellect}\n손재주 {v.Dexterity} · 체력 {v.Vitality}";

    /// <summary>필요 수치는 모두 100 이 가장 좋은 상태입니다 (배부름 0 = 굶주림)</summary>
    public static string Needs(in CoreVillagerView v) =>
        $"배부름 {v.Hunger} · 기운 {v.Rest}\n안전 {v.Safety} · 건강 {v.Health}";

    public static string MoodLine(in CoreVillagerView v) => $"기분 {v.Mood} (→ {v.MoodTarget})";

    public static string MoodTooltip(System.ReadOnlySpan<CoreMoodTermView> terms)
    {
        var lines = new List<string> { "기분은 한 시간에 2씩 목표 값으로 움직입니다. 기분이 높을수록 일이 빠릅니다." };
        int total = 0;
        foreach (CoreMoodTermView term in terms)
        {
            total += term.Value;
            if (term.Value != 0)
            {
                lines.Add($"{MoodTermName(term.Term)} {term.Value:+#;-#;0}");
            }
        }
        lines.Add($"목표 {System.Math.Clamp(total, 0, 100)}");
        lines.Add("기분 25 아래로 이틀이 지나면 마을을 떠납니다.");
        return string.Join("\n", lines);
    }

    public static string JobLine(int job) => job < 0 ? Unemployed : $"일터: {BuildingName(job)}";

    public static string HomeLine(bool hasHome) => hasHome ? "집: 있음" : Homeless;

    public static string Traits(string a, string b) =>
        b.Length > 0 ? $"성격: {TraitName(a)}, {TraitName(b)}" : $"성격: {TraitName(a)}";

    public const string ColumnName = "이름";
    public const string ColumnJob = "일터";
    public const string ColumnMood = "기분";
    public const string ColumnState = "하는 일";

    // ---- 이벤트 → 알림 문장 ----
    public static string? EventText(in CoreEventView coreEvent)
    {
        string name = coreEvent.GetText();
        switch ((CoreEventType)coreEvent.Type)
        {
            case CoreEventType.DayStarted:
            case CoreEventType.Dawn:
            case CoreEventType.NightFell:
                // 매일 오는 알림은 중요한 알림(완성, 굶주림, 이주)을 밀어내므로 보여 주지 않습니다.
                // 날짜와 시각은 위쪽 막대에, 밤은 화면 색에 이미 드러납니다.
                return null;
            case CoreEventType.SeasonChanged:
                // 봄, 여름, 가을, 겨울 모두 받침으로 끝나므로 조사는 늘 '이'
                return coreEvent.A == 0 && coreEvent.B > 1
                    ? $"{coreEvent.B}년째 봄이 왔습니다."
                    : $"{SeasonName(coreEvent.A)}이 왔습니다.";
            case CoreEventType.Warning:
                return coreEvent.A == (int)CoreWarning.EventsDropped ? $"알림 {coreEvent.B}개를 표시하지 못했습니다." : null;
            case CoreEventType.BuildingPlaced:
                return coreEvent.B == (int)BuildingType.Road || coreEvent.B == (int)BuildingType.Wall
                    ? null
                    : $"{Object(BuildingName(coreEvent.B))} 짓기 시작했습니다.";
            case CoreEventType.BuildingCompleted:
                return coreEvent.B == (int)BuildingType.Wall ? null : $"{BuildingName(coreEvent.B)} 완성!";
            case CoreEventType.BuildingRemoved:
                return coreEvent.B == (int)BuildingType.Road ? null : $"{Object(BuildingName(coreEvent.B))} 철거했습니다.";
            case CoreEventType.Produced:
                return null; // 떠오르는 숫자로 보여 줌
            case CoreEventType.Immigrant:
                return $"{Subject(name)} 마을에 이주해 왔습니다.";
            case CoreEventType.VillagerLeaving:
                return $"{Subject(name)} 마을을 떠나기로 했습니다. (기분이 너무 낮음)";
            case CoreEventType.VillagerLeft:
                return $"{Subject(name)} 마을을 떠났습니다.";
            case CoreEventType.VillagerDied:
                return coreEvent.B == (int)DeathCause.Raid
                    ? $"{Subject(name)} 습격에 맞서다 쓰러졌습니다."
                    : $"{Subject(name)} {(coreEvent.B == (int)DeathCause.Starvation ? "굶주림으로 " : coreEvent.B == (int)DeathCause.Illness ? "병으로 " : "")}세상을 떠났습니다.";
            case CoreEventType.PathBlocked:
                return $"{name}: 길이 막혔습니다.";
            case CoreEventType.StorageFull:
                return $"{ResourceName(coreEvent.A)} 창고가 가득 찼습니다. 창고를 지으세요.";
            case CoreEventType.WagesUnpaid:
                return $"금화가 모자라 {coreEvent.A}명이 임금을 받지 못했습니다.";
            case CoreEventType.Starving:
                return $"주민 {coreEvent.A}명이 굶고 있습니다!";
            case CoreEventType.Story:
                return StoryText(name, (Story)coreEvent.B, coreEvent.C);
            case CoreEventType.DefWarning:
                return $"정의 파일 {Pick(DataFileNames, coreEvent.A)} 의 {coreEvent.B}번째 줄이 잘못되어 기본값을 씁니다.";
            case CoreEventType.WeatherChanged:
                return null; // 위쪽 막대와 화면 효과로 보여 줌
            case CoreEventType.NatureForecast:
                return $"예보: 내일 {Subject(NatureName(coreEvent.A))} 옵니다. {Pick(NatureDescriptions, coreEvent.A)}";
            case CoreEventType.NatureStarted:
                return $"{Subject(NatureName(coreEvent.A))} 시작되었습니다 ({coreEvent.B}일).";
            case CoreEventType.NatureEnded:
                return $"{Subject(NatureName(coreEvent.A))} 끝났습니다.";
            case CoreEventType.BuildingDamaged:
                return $"{Subject(BuildingName(coreEvent.B))} 부서졌습니다. 검사창에서 고칠 수 있습니다.";
            case CoreEventType.BuildingRuined:
                return $"{Subject(BuildingName(coreEvent.B))} 무너졌습니다! 고치기 전에는 쓸 수 없습니다.";
            case CoreEventType.BuildingRepaired:
                return $"{Object(BuildingName(coreEvent.B))} 고쳤습니다.";
            case CoreEventType.VillagerSick:
                return $"{Subject(name)} 병에 걸렸습니다.";
            case CoreEventType.VillagerCured:
                return coreEvent.B == 1 ? $"{Subject(name)} 치료소에서 나았습니다." : $"{Subject(name)} 병이 저절로 나았습니다.";
            case CoreEventType.MerchantArrived:
                return $"떠돌이 상인이 왔습니다! {coreEvent.A}일 머뭅니다. (위쪽 막대의 상인 단추)";
            case CoreEventType.MerchantLeft:
                return "떠돌이 상인이 떠났습니다.";
            case CoreEventType.WinterWarning:
                return $"겨울 준비가 모자랍니다: 봄까지 식량 {coreEvent.A}, 목재 {coreEvent.B} 정도가 있어야 합니다.";
            case CoreEventType.HousesCold:
                return $"목재가 모자라 집 {coreEvent.A}채가 지난밤 난방을 못 했습니다.";
            case CoreEventType.ThreatScheduled:
                return $"위협 예고: {ThreatName(coreEvent.A)} {coreEvent.D}명이 {DirectionName(coreEvent.B)}쪽에서 {Countdown(coreEvent.C)} 뒤에 옵니다. {Pick(ThreatAdvice, coreEvent.A)}";
            case CoreEventType.ThreatNear:
                return $"경보! {Subject(ThreatName(coreEvent.A))} {DirectionName(coreEvent.B)}쪽에서 다가옵니다. 반나절 안에 도착합니다.";
            case CoreEventType.RaidStarted:
                return $"습격! {ThreatName(coreEvent.B)} {coreEvent.C}명이 {DirectionName(coreEvent.D)}쪽에 나타났습니다. 주민들이 성채로 피합니다.";
            case CoreEventType.RaidEnded:
                return RaidEndedText(coreEvent.A, coreEvent.B, coreEvent.C, coreEvent.D);
            case CoreEventType.EnemyKilled:
                return coreEvent.C > 0 ? $"{Object(EnemyName(coreEvent.B))} 쓰러뜨렸습니다. 마정석 +{coreEvent.C}" : null;
            case CoreEventType.LootStolen:
                return coreEvent.B > 0 ? $"적이 {ResourceName(coreEvent.A)} {coreEvent.B}개를 훔쳐 갔습니다!" : null;
            case CoreEventType.ResearchStarted:
                return $"연구 시작: {ResearchName(name)}. 마탑의 마법사들이 연구합니다.";
            case CoreEventType.ResearchDone:
                return $"연구 끝: {ResearchName(name)}! ({coreEvent.B}번째 연구)";
            case CoreEventType.RankReady:
                return $"{Subject(RankName(coreEvent.A))} 될 조건을 채웠습니다. 위쪽 막대의 등급 단추에서 공물을 바치고 승급하세요.";
            case CoreEventType.RankUp:
                return $"승급! 이제 {RankName(coreEvent.A)}입니다.";
            case CoreEventType.Victory:
                return $"승리! {coreEvent.A}일째에 왕성이 되었습니다.";
            case CoreEventType.Defeat:
                return $"패배: {LoseReasonText(coreEvent.A)}";
            case CoreEventType.TutorialStep:
                return $"목표 달성: {TutorialName(coreEvent.A)} ({coreEvent.B}/{coreEvent.C})";
            case CoreEventType.TutorialDone:
                return $"처음 목표를 모두 이루었습니다! 보상: 금화 +{coreEvent.A}, 마정석 +{coreEvent.B}";
            default:
                return null;
        }
    }

    // ---- 등급, 연구, 승패, 튜토리얼 ----
    private static readonly string[] RankNames = { "촌락", "마을", "요새", "성", "왕성" };
    private static readonly string[] BranchNames = { "경제", "방어", "마법" };
    private static readonly string[] ResearchKeys =
    {
        "crop_rotation", "better_tools", "trade_guild", "granary", "sharp_arrows", "stone_walls", "drill",
        "keep_ballista", "mana_sense", "herbal_magic", "warm_hearth", "ward",
    };
    private static readonly string[] ResearchNames =
    {
        "윤작", "좋은 연장", "상인 조합", "큰 곳간", "날카로운 화살", "돌 성벽", "경비 훈련", "성채 쇠뇌", "마력 감지",
        "약초 마법", "따뜻한 불씨", "마법 결계",
    };
    // 효과: 앞 글, 부호, 단위
    private static readonly (string Text, string Sign, string Unit)[] EffectTexts =
    {
        ("농장 생산", "+", "%"), ("벌목장·채석장 생산", "+", "%"), ("저장 한도", "+", "%"), ("시장의 사고팔 때 차이", "-", "%p"),
        ("감시탑 화살 피해", "+", ""), ("성벽·성문 체력", "+", "%"), ("경비병 피해", "+", ""), ("성채 망루 피해", "+", ""),
        ("마정석 광산 생산", "+", "%"), ("병을 앓는 시간", "-", "%"), ("집 한 채의 하룻밤 난방 목재", "-", ""),
        ("습격 때 건물이 받는 피해", "-", "%"),
    };
    private static readonly string[] ResearchStateNames = { "잠김", "시작할 수 있음", "연구 중", "끝남" };
    private static readonly string[] TutorialNames =
    {
        "농장 짓기", "주택 짓기", "주민을 일터에 직접 배치하기", "성벽 짓기", "첫 도적 습격 버티기",
    };
    private static readonly string[] TutorialHints =
    {
        "아래 건설 막대의 '자원' 탭 → 농장. 성채 가까이 지으면 걷는 시간이 줄어듭니다.",
        "'주거' 탭 → 주택. 빈 집이 있어야 이주민이 옵니다.",
        "주민을 고르고(왼쪽 클릭 또는 V 목록) 건물 검사창의 '일꾼 넣기'로 보냅니다.",
        "'방어' 탭 → 성벽을 끌어서 줄로 놓습니다. 주민이 드나들 성문도 함께.",
        "위쪽 막대의 '위협:' 카운트다운을 보고, 그쪽에 감시탑을 지어 두세요.",
    };
    private static readonly string[] LoseReasonTexts = { "", "성채가 무너졌습니다", "주민이 한 명도 남지 않았습니다" };

    public static string RankName(int rank) => Pick(RankNames, rank);
    public static string BranchName(int branch) => Pick(BranchNames, branch);
    public static string ResearchStateName(int state) => Pick(ResearchStateNames, state);
    public static string TutorialName(int step) => Pick(TutorialNames, step);
    public static string TutorialHint(int step) => Pick(TutorialHints, step);
    public static string LoseReasonText(int reason) => Pick(LoseReasonTexts, reason);

    /// <summary>연구 키 → 한국어 이름 (정의 파일에서 새로 만든 연구는 키 그대로)</summary>
    public static string ResearchName(string key)
    {
        int index = System.Array.IndexOf(ResearchKeys, key);
        return index >= 0 ? ResearchNames[index] : key;
    }

    public static string EffectText(int effect, int value)
    {
        if (effect < 0 || effect >= EffectTexts.Length)
        {
            return "?";
        }
        var (text, sign, unit) = EffectTexts[effect];
        return $"{text} {sign}{value}{unit}";
    }

    public const string RankButtonTooltip = "등급: 인구·기분·방어·연구 조건을 채우고 공물을 바치면 올라갑니다. 왕성이 되면 승리!";
    public static string RankLabel(int rank, bool ready) => ready ? $"{RankName(rank)} (승급 가능!)" : RankName(rank);

    public const string ResearchButton = "연구";
    public const string MageTowerIdle = "연구 중인 것이 없습니다 (위쪽 막대의 연구 단추)";
    public const string ResearchButtonTooltip = "연구 창: 마탑의 마법사가 점수를 모아 연구를 끝냅니다 (요새부터 마탑을 지을 수 있음).";
    public static string ResearchShort(string key, int progress, int points) => $"연구: {ResearchName(key)} {progress}/{points}";

    // 연구 창
    public const string ResearchTitle = "연구";
    public const string ResearchHelp = "연구는 시작할 때 금화·마정석을 내고, 마탑에서 일하는 마법사가 점수를 모아 끝냅니다. 한 번에 하나만 할 수 있습니다. 갈래 안에서는 위의 연구를 먼저 끝내야 합니다.";
    public static string ResearchStatus(in CoreSummaryView summary) =>
        summary.ResearchActive >= 0
            ? $"일하는 마법사 {summary.ResearchMages}명 · 끝낸 연구 {summary.ResearchDone}개"
            : summary.ResearchMages > 0 ? $"마법사 {summary.ResearchMages}명이 연구를 기다립니다 · 끝낸 연구 {summary.ResearchDone}개"
            : $"마탑에서 일하는 마법사가 없습니다 (요새부터 마탑을 지을 수 있음) · 끝낸 연구 {summary.ResearchDone}개";
    public static string ResearchNodeText(in CoreResearchView view) =>
        view.State == (int)ResearchState.Active
            ? $"{ResearchName(view.GetKey())}\n{view.Progress}/{view.Points}점"
            : $"{ResearchName(view.GetKey())}\n{(view.State == (int)ResearchState.Done ? "끝남" : $"금화 {view.CostGold} · 마정석 {view.CostMana}")}";
    public static string ResearchTooltip(in CoreResearchView view, string? requiresKey)
    {
        string text = $"{ResearchName(view.GetKey())} ({BranchName(view.Branch)})\n효과: {EffectText(view.Effect, view.Value)}\n" +
                      $"비용: 금화 {view.CostGold}, 마정석 {view.CostMana} · 연구 점수 {view.Points}\n" +
                      $"필요 등급: {RankName(view.RequiredRank)}";
        if (requiresKey != null)
        {
            text += $"\n먼저 끝낼 연구: {ResearchName(requiresKey)}";
        }
        return text + $"\n상태: {ResearchStateName(view.State)}";
    }
    public static string ResearchRejected(DenyReason reason) => $"연구를 시작할 수 없습니다: {DenyText(reason)}";

    // 승급 창
    public const string RankTitle = "등급과 승급";
    public static string RankCurrent(int rank) => $"지금 등급: {RankName(rank)}";
    public const string RankMax = "이미 가장 높은 등급(왕성)입니다.";
    public static string RankNext(int rank) => $"다음 등급: {RankName(rank)}";
    public static string Requirement(string what, int have, int need) => $"{(have >= need ? "충족" : "부족")}  {what} {have} / {need}";
    public const string ReqPopulation = "인구";
    public const string ReqMood = "평균 기분";
    public const string ReqDefense = "방어 점수";
    public const string ReqResearch = "끝낸 연구";
    public const string DefenseHelp = "방어 점수 = 다 지은 감시탑 3, 병영 2, 성문 1 + 경비병 한 명당 1";
    public static string Tribute(int gold, int mana) => $"공물: 금화 {gold}, 마정석 {mana}";
    public static string RankRewards(int storage, string buildings, int research) =>
        $"오르면: 저장 한도 +{storage}" + (buildings.Length > 0 ? $", 새 건물 {buildings}" : "") +
        (research > 0 ? $", 새 연구 {research}개" : "");
    public const string RankUpButton = "공물을 바치고 승급";
    public static string RankUpRejected(DenyReason reason) => $"승급할 수 없습니다: {DenyText(reason)}";

    // 튜토리얼
    public const string TutorialTitle = "처음 목표";
    public const string TutorialHide = "숨기기";
    public static string TutorialProgress(int done, int total) => $"처음 목표 {done}/{total}";

    // 승리·패배
    public const string VictoryTitle = "승리! 왕성이 되었습니다";
    public const string DefeatTitle = "패배";
    public const string ContinueButton = "계속 다스리기";
    public const string NewGameButton = "새 게임";
    public static string RunStatsText(in CoreRunStatsView stats) =>
        $"지난 날: {stats.Days}일 · 등급: {RankName(stats.Rank)}\n" +
        $"가장 많았던 주민: {stats.PeakPopulation}명\n" +
        $"버틴 습격: {stats.RaidsSurvived}번 · 쓰러뜨린 적: {stats.EnemiesKilled}\n" +
        $"세상을 떠난 주민: {stats.VillagersDied}명 · 마을을 떠난 주민: {stats.VillagersLeft}명\n" +
        $"지은 건물: {stats.BuildingsBuilt}채 · 끝낸 연구: {stats.ResearchDone}개";
    public static string VictoryText(in CoreRunStatsView stats) => $"{stats.VictoryDay}일째에 왕성이 되었습니다. 계속 다스릴 수도 있습니다.";

    // ---- 위협, 습격 ----
    private static readonly string[] ThreatNames = { "없음", "도적", "마물", "적대 영주 군대" };
    private static readonly string[] DirectionNames = { "북", "동", "남", "서" };
    private static readonly string[] EnemyNames = { "도적", "도적 두목", "고블린", "오우거", "병사", "궁수" };
    private static readonly string[] ThreatAdvice =
    {
        "",
        "도적은 성채에서 금화를 훔쳐 달아납니다. 감시탑과 성벽으로 막으세요.",
        "마물은 밤에 옵니다. 쓰러뜨리면 마정석을 떨어뜨립니다.",
        "군대는 건물을 부숩니다. 성벽, 감시탑, 경비병을 준비하세요.",
    };

    public static string ThreatName(int type) => Pick(ThreatNames, type);
    public static string DirectionName(int direction) => Pick(DirectionNames, direction);
    public static string EnemyName(int type) => Pick(EnemyNames, type);

    /// <summary>틱 수를 "2일 5시간" 처럼 (한 틱 = 10분)</summary>
    public static string Countdown(int ticks)
    {
        int days = ticks / 144;
        int hours = ticks % 144 / 6;
        if (days > 0)
        {
            return hours > 0 ? $"{days}일 {hours}시간" : $"{days}일";
        }
        return hours > 0 ? $"{hours}시간" : $"{ticks % 6 * 10}분";
    }

    public static string ThreatLabel(int type, int direction, int ticksLeft) =>
        $"위협: {ThreatName(type)} {Countdown(ticksLeft)}";

    public static string ThreatTooltip(int type, int direction, int ticksLeft, int strength) =>
        $"다음 위협: {ThreatName(type)} {strength}명, {DirectionName(direction)}쪽에서 {Countdown(ticksLeft)} 뒤 도착\n{Pick(ThreatAdvice, type)}\n" +
        "습격 중에는 주민이 성채로 피하고, 병영의 일꾼이 경비병으로 싸웁니다. 습격 중에는 저장할 수 없습니다.";

    public const string NoThreat = "위협 없음";

    public static string RaidBanner(int type, int alive, int total, int guards) =>
        guards > 0
            ? $"습격! {ThreatName(type)} — 남은 적 {alive}/{total} · 경비병 {guards}명"
            : $"습격! {ThreatName(type)} — 남은 적 {alive}/{total}";

    public const string RaidBannerTooltip =
        "주민들이 성채로 피했습니다. 감시탑과 성채 망루가 화살을 쏘고, 병영의 일꾼이 경비병으로 싸웁니다.\n" +
        "적이 모두 쓰러지거나 달아나면 습격이 끝납니다. 습격 중에는 저장할 수 없습니다.";

    public const string NoThreatTooltip = "지금 예고된 위협이 없습니다. 위협은 언제나 미리 예고됩니다.";

    public static string RaidEndedText(int killed, int total, int stolen, int fallen)
    {
        string text = $"습격이 끝났습니다: 적 {total}명 중 {killed}명을 쓰러뜨렸습니다.";
        if (stolen > 0)
        {
            text += $" 빼앗긴 자원 {stolen}.";
        }
        if (fallen > 0)
        {
            text += $" 쓰러진 주민 {fallen}명.";
        }
        return text;
    }

    public static string EnemyTooltip(int type, int hp, int maxHp) => $"{EnemyName(type)} — 체력 {hp}/{maxHp}";


    private static string? StoryText(string name, Story story, int value) => story switch
    {
        Story.FirstProduce => $"{Subject(name)} 처음으로 {Object(ResourceName(value))} 거뒀습니다.",
        Story.MovedIn => $"{Subject(name)} 새 집으로 이사했습니다.",
        Story.NewJob => $"{Subject(name)} {BuildingName(value)}에서 일하게 되었습니다.",
        _ => null,
    };

    public static string Produced(int resource, int amount) => $"+{amount} {ResourceName(resource)}";

    // ---- 제목 화면, 새 게임, 세이브 슬롯, 일시정지, 설정 (마일스톤 6) ----
    public const string GameSubtitle = "폐허가 된 언덕 위 성채를 왕성으로";
    public const string TitleContinue = "이어하기";
    public const string TitleNewGame = "새 게임";
    public const string TitleLoad = "불러오기";
    public const string TitleSettings = "설정";
    public const string TitleQuit = "종료";
    public const string TitleNoSave = "아직 저장한 게임이 없습니다";
    public const string TitleCredits = "그림·소리: Kenney (CC0) · 글꼴: Galmuri (OFL)";
    public static string TitleContinueInfo(in SlotInfo slot) =>
        $"{SlotName(slot.Slot)} · {SaveSummary(slot.Ok ? slot.Info : slot.Backup)}" + (slot.OnlyBackup ? " (예비 파일)" : "");

    public const string NewGameTitle = "새 게임";
    public const string NewGameName = "성채 이름";
    public const string NewGameNameInvalid = "쓸 수 없는 이름입니다 (한글 15자쯤까지, 줄바꿈 없이)";
    public const string NewGameSeed = "지도 시드";
    public const string NewGameSeedHint = "비우면 무작위";
    public const string NewGameSeedRandom = "무작위";
    public const string NewGameSeedInvalid = "시드는 0 부터 2147483647 사이의 숫자입니다";
    public const string NewGameDifficulty = "난이도";
    public const string NewGameStart = "시작";
    public const string Cancel = "취소";
    private static readonly string[] DifficultyBlurbs =
    {
        "처음 영주가 된 사람에게. 습격이 약하고 드물며 시작 자원이 넉넉합니다.",
        "기본 난이도. 계절과 습격이 고르게 압박합니다.",
        "도전하는 영주에게. 습격이 세고 잦으며 자원이 빠듯합니다.",
    };
    public static string DifficultyText(in CoreDifficultyView view) =>
        Pick(DifficultyBlurbs, view.Difficulty) + "\n" +
        $"시작 주민 {view.StartVillagers}명 · 금화 {view.GetStart(ResourceKind.Gold)} · 마정석 {view.GetStart(ResourceKind.Mana)} · " +
        $"습격 세기 {view.RaidStrengthPct}% · 습격 간격 {view.RaidIntervalPct}% · 자연 사건 {view.NaturePct}% · 공물 {view.TributePct}%";

    public const string SaveDialogTitle = "저장하기";
    public const string LoadDialogTitle = "불러오기";
    public const string AutosaveName = "자동 저장";
    public static string SlotName(int slot) => slot == SaveSlots.AutosaveSlot ? AutosaveName : $"슬롯 {slot}";
    public static string SaveSummary(in CoreSaveInfoView info) =>
        $"{info.GetCastleName()} — {info.Year}년 {SeasonName(info.Season)}, {info.Day}일째 {Clock(info.MinuteOfDay)} · " +
        $"{RankName(info.Rank)} · 주민 {info.Population}명 · {DifficultyName(info.Difficulty)}" +
        (info.Outcome == (int)Outcome.Won ? " · 승리" : info.Outcome == (int)Outcome.Lost ? " · 패배" : "");
    public static string SlotLine(in SlotInfo slot)
    {
        string name = SlotName(slot.Slot);
        if (!slot.Exists)
        {
            return $"{name}\n비어 있음";
        }
        string when = SaveSlots.LocalTime(slot.ModifiedUnix);
        if (slot.Ok)
        {
            return $"{name} · {when}\n{SaveSummary(slot.Info)}";
        }
        string problem = SaveProblem(slot.Info);
        return slot.BackupOk
            ? $"{name} · {problem}\n예비 파일: {SaveSummary(slot.Backup)}"
            : $"{name} · {problem}\n불러올 수 없습니다";
    }
    public static string SaveProblem(in CoreSaveInfoView info) => (CoreResultCode)info.Result switch
    {
        CoreResultCode.Version => $"다른 버전의 세이브 (형식 {info.Version}, 이 게임은 {CoreSaveVersion})",
        CoreResultCode.Format => info.ErrorLine > 0 ? $"망가진 세이브 ({info.ErrorLine}번째 줄)" : "망가진 세이브",
        _ => ResultMessage((CoreResultCode)info.Result),
    };
    /// <summary>C 의 CORE_SAVE_VERSION (세이브 형식). 다른 버전 안내에만 씀</summary>
    public const int CoreSaveVersion = 5;
    public static string ConfirmOverwrite(in SlotInfo slot) =>
        $"{SlotName(slot.Slot)}에 저장된 게임 위에 저장할까요?\n{(slot.Ok ? SaveSummary(slot.Info) : SaveProblem(slot.Info))}";
    public static string ConfirmBackup(in SlotInfo slot) =>
        $"{SlotName(slot.Slot)}의 세이브가 망가졌습니다 ({SaveProblem(slot.Info)}).\n" +
        $"그 전에 저장한 예비 파일(.bak)로 불러올까요?\n{SaveSummary(slot.Backup)}";
    public const string ConfirmLoseProgress = "저장하지 않은 진행은 사라집니다. 불러올까요?";
    public static string Saved(int slot) => $"{SlotName(slot)}에 저장했습니다";
    public static string SaveFailed(CoreResult result) => $"저장하지 못했습니다: {result.Message}";
    public static string Loaded(int slot, bool backup) => $"{SlotName(slot)}{(backup ? "의 예비 파일" : "")}에서 불러왔습니다";
    public static string LoadFailed(CoreResult result, int line) =>
        $"불러오지 못했습니다: {result.Message}" + (line > 0 ? $" ({line}번째 줄)" : "");
    public const string SaveBlockedRaid = "습격 중에는 저장할 수 없습니다. 습격이 끝나면 저장하세요 (자동 저장도 그때 합니다).";
    public const string Autosaved = "자동 저장";
    public const string YesButton = "예";
    public const string NoButton = "아니오";
    public const string OkButton = "확인";

    public const string PauseTitle = "일시정지";
    public const string PauseResume = "계속하기";
    public const string PauseSave = "저장하기";
    public const string PauseLoad = "불러오기";
    public const string PauseSettings = "설정";
    public const string PauseToTitle = "제목 화면으로";
    public const string PauseQuit = "게임 종료";
    public static string LastSave(int day, int minute, int slot) =>
        $"마지막 저장: {day}일째 {Clock(minute)} ({SlotName(slot)})";
    public const string NoSaveYet = "아직 저장하지 않았습니다";
    public const string QuitAutosaveNote = "제목 화면으로 가거나 끝낼 때 자동 저장합니다.";
    public const string ConfirmQuitInRaid = "습격 중이라 저장할 수 없습니다.\n마지막 저장 뒤의 진행은 사라집니다. 그래도 나갈까요?";
    public const string MenuButton = "메뉴";
    public const string MenuButtonTooltip = "일시정지 메뉴: 저장, 불러오기, 설정, 제목 화면 (Esc)";

    public const string SettingsTitle = "설정";
    public const string SettingsMaster = "전체 음량";
    public const string SettingsMusic = "음악";
    public const string SettingsSfx = "효과음";
    public const string SettingsMusicNote = "음악은 아직 없습니다 (자리만 있음)";
    public const string SettingsUiScale = "UI 크기";
    public const string SettingsUiAuto = "자동 (창 크기에 맞춤)";
    public static string SettingsUiFixed(int scale) => $"{scale}배";
    public const string SettingsFullscreen = "전체 화면";
    public const string SettingsEdgeScroll = "화면 가장자리로 카메라 이동";

    // ---- 오류 코드 → 문장 ----
    public static string ResultMessage(CoreResultCode code) => code switch
    {
        CoreResultCode.Ok => "성공했습니다.",
        CoreResultCode.Ignored => "무시되었습니다.",
        CoreResultCode.NullArg or CoreResultCode.InvalidArg => "잘못된 요청입니다.",
        CoreResultCode.OutOfMemory => "메모리가 부족합니다.",
        CoreResultCode.BufferTooSmall => "내부 버퍼가 작습니다.",
        CoreResultCode.FileOpen => "파일을 열 수 없습니다.",
        CoreResultCode.FileIo => "파일을 읽거나 쓰는 중 문제가 생겼습니다.",
        CoreResultCode.Format => "세이브 파일이 손상되었습니다.",
        CoreResultCode.Version => "다른 버전의 세이브 파일입니다.",
        CoreResultCode.StructSize => "게임 코어 버전이 맞지 않습니다.",
        CoreResultCode.Rejected => "규칙상 할 수 없습니다.",
        CoreResultCode.NotFound => "대상이 이미 없습니다.",
        _ => "알 수 없는 오류입니다.",
    };
}
