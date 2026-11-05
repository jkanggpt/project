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

    private static readonly string[] DataFileNames = { "buildings.def", "traits.def", "names.txt" };

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

    private static readonly string[] MoodTermNames = { "기본", "배고픔", "피로", "안전", "집", "성격", "임금", "슬픔" };

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

    private static string Pick(string[] names, int index) =>
        index >= 0 && index < names.Length ? names[index] : "?";

    // ---- HUD ----
    public static string Date(in CoreSummaryView summary) =>
        $"{summary.Year}년 {SeasonName(summary.Season)} {summary.DayOfSeason}일";

    public static string DayCount(int day) => $"{day}일째";

    public static string Clock(int minuteOfDay) => $"{minuteOfDay / 60:00}:{minuteOfDay % 60:00}";

    public static string DateTooltip(in CoreSummaryView summary, in CoreConfigView config) =>
        $"{summary.Year}년 {SeasonName(summary.Season)} {summary.DayOfSeason}일, 게임을 시작하고 {summary.Day}일째입니다.\n" +
        $"한 계절은 {config.DaysPerSeason}일, 한 해는 {config.DaysPerSeason * config.SeasonsPerYear}일입니다.";

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
        if (disabledReason != null)
        {
            text += $"\n\n지금 지을 수 없음: {disabledReason}";
        }
        return text;
    }

    public static string RankRequired(int rank) => $"등급 {rank} 이 필요합니다 (승급은 마일스톤 5 에서 생깁니다)";

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
                return $"{Subject(name)} {(coreEvent.B == 1 ? "굶주림으로 " : "")}세상을 떠났습니다.";
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
            default:
                return null;
        }
    }

    private static string? StoryText(string name, Story story, int value) => story switch
    {
        Story.FirstProduce => $"{Subject(name)} 처음으로 {Object(ResourceName(value))} 거뒀습니다.",
        Story.MovedIn => $"{Subject(name)} 새 집으로 이사했습니다.",
        Story.NewJob => $"{Subject(name)} {BuildingName(value)}에서 일하게 되었습니다.",
        _ => null,
    };

    public static string Produced(int resource, int amount) => $"+{amount} {ResourceName(resource)}";

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
