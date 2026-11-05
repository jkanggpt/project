// Strings.cs — 플레이어에게 보이는 모든 한국어 문장
//
// 스크립트 곳곳에 한국어를 흩어 두지 않고 여기 한곳에 모읍니다 (SPEC §7).
// C 코어는 숫자 코드만 내보내고, 그 코드를 문장으로 바꾸는 일도 여기서 합니다.
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

    // ---- 이름표 ----
    private static readonly string[] SeasonNames = { "봄", "여름", "가을", "겨울" };
    private static readonly string[] DifficultyNames = { "평화", "보통", "혹독" };
    private static readonly string[] ResourceNames = { "금화", "마정석", "식량", "목재", "석재", "약초" };

    private static readonly string[] ResourceDescriptions =
    {
        "오늘의 경제를 돌리는 돈입니다. 건설, 임금, 거래에 씁니다.",
        "미래를 위한 귀한 힘입니다. 연구, 마법 건물, 승급에 씁니다.",
        "주민이 먹습니다. 겨울에는 농장이 멈추니 미리 모아 두세요.",
        "건설과 겨울 난방에 씁니다.",
        "튼튼한 건물과 성벽에 씁니다.",
        "치료소에서 병을 고치는 데 씁니다.",
    };

    private static readonly string[] TerrainNames = { "풀밭", "숲", "바위 지대", "물", "마정석 광맥" };

    public static string SeasonName(int season) => Pick(SeasonNames, season);
    public static string DifficultyName(int difficulty) => Pick(DifficultyNames, difficulty);
    public static string ResourceName(ResourceKind resource) => Pick(ResourceNames, (int)resource);
    public static string TerrainName(int terrain) => Pick(TerrainNames, terrain);

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
        $"{Clock(config.NightStartMinute)}부터 {Clock(config.NightEndMinute)}까지는 밤입니다.";

    /// <summary>capacity 가 0 이면 저장 한도가 없는 자원</summary>
    public static string ResourceTooltip(ResourceKind resource, int amount, int capacity)
    {
        string text = $"{ResourceName(resource)}: {amount}\n{Pick(ResourceDescriptions, (int)resource)}";
        if (capacity > 0)
        {
            text += $"\n저장 한도 {capacity} (성채와 창고가 정합니다)";
        }
        else
        {
            text += "\n화폐라서 저장 한도가 없습니다.";
        }
        return text;
    }

    public const string SpeedPause = "||";
    public const string SpeedPauseTooltip = "일시정지 (스페이스)\n멈춘 동안에도 화면을 둘러볼 수 있습니다.";

    public static string SpeedLabel(int speed) => $"{speed}×";

    public static string SpeedTooltip(int speed) => $"{speed}배속 (숫자 키 {speed})";

    public const string PausedBanner = "일시정지 — 스페이스 키로 계속합니다";
    public const string ScreenshotSampleToast = "성채 주변을 둘러봅니다.";

    // ---- 이벤트 → 알림 문장 ----
    public static string? EventText(in CoreEventView coreEvent)
    {
        switch ((CoreEventType)coreEvent.Type)
        {
            case CoreEventType.DayStarted:
                return null; // 매일 자정 알림은 너무 잦아서 보여 주지 않습니다 (새벽 알림이 대신함)
            case CoreEventType.Dawn:
                return $"{coreEvent.A}일째 아침이 밝아 옵니다.";
            case CoreEventType.NightFell:
                return "밤이 되었습니다.";
            case CoreEventType.SeasonChanged:
                // 봄, 여름, 가을, 겨울 모두 받침으로 끝나므로 조사는 늘 '이'
                return coreEvent.A == 0 && coreEvent.B > 1
                    ? $"{coreEvent.B}년째 봄이 왔습니다."
                    : $"{SeasonName(coreEvent.A)}이 왔습니다.";
            case CoreEventType.Warning:
                return coreEvent.A == (int)CoreWarning.EventsDropped
                    ? $"알림 {coreEvent.B}개를 표시하지 못했습니다."
                    : null;
            default:
                return null;
        }
    }

    // ---- 오류 코드 → 문장 ----
    public static string ResultMessage(CoreResultCode code) => code switch
    {
        CoreResultCode.Ok => "성공했습니다.",
        CoreResultCode.NullArg or CoreResultCode.InvalidArg => "잘못된 요청입니다.",
        CoreResultCode.OutOfMemory => "메모리가 부족합니다.",
        CoreResultCode.BufferTooSmall => "내부 버퍼가 작습니다.",
        CoreResultCode.FileOpen => "파일을 열 수 없습니다.",
        CoreResultCode.FileIo => "파일을 읽거나 쓰는 중 문제가 생겼습니다.",
        CoreResultCode.Format => "세이브 파일이 손상되었습니다.",
        CoreResultCode.Version => "다른 버전의 세이브 파일입니다.",
        CoreResultCode.StructSize => "게임 코어 버전이 맞지 않습니다.",
        _ => "알 수 없는 오류입니다.",
    };
}
