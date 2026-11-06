// CoreBridge.cs — Godot 과 C 코어 사이의 유일한 다리 (자동 로드 이름: Core)
//
// 규칙 (docs/SPEC.md §3.1)
// - Godot 은 게임 상태를 직접 바꾸지 않습니다. 모든 변경은 여기의 메서드 → C 함수로만 갑니다.
// - 월드는 불투명 포인터(IntPtr)로만 들고 있고, C 메모리를 가리키는 포인터는 절대 보관하지 않습니다.
// - 모든 네이티브 호출은 이 클래스 안에서만 하고, 오류 코드는 CoreResult 로 바꿔 돌려줍니다.
// - 메인 스레드에서만 부릅니다. 에디터([Tool])에서는 DLL 을 로드하지 않습니다 (DLL 파일이 잠기지 않게).
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using Godot;
using MagicCastle.UI;

namespace MagicCastle.Core;

/// <summary>네이티브 호출 결과</summary>
public readonly record struct CoreResult(CoreResultCode Code)
{
    public bool Ok => Code == CoreResultCode.Ok;

    public static CoreResult FromCode(int code) => new((CoreResultCode)code);

    /// <summary>로그용 영어 이름 (예: CORE_ERR_FORMAT)</summary>
    public string Name => CoreBridge.ResultName((int)Code);

    /// <summary>플레이어에게 보여 줄 한국어 설명</summary>
    public string Message => Strings.ResultMessage(Code);
}

public enum CoreStatus
{
    NotLoaded,
    Ready,
    LibraryMissing,
    AbiMismatch,
}

public partial class CoreBridge : Node
{
    /// <summary>C 의 CORE_ABI_VERSION 과 같아야 합니다.</summary>
    public const int ExpectedAbiVersion = 5;

    /// <summary>정의 파일 (순서는 DataFile)</summary>
    private static readonly string[] DataFiles = { "res://data/buildings.def", "res://data/traits.def", "res://data/names.txt", "res://data/enemies.def",
        "res://data/research.def", "res://data/ranks.def" };

    private static bool _resolverInstalled;
    private static string _loadError = "";

    private IntPtr _world = IntPtr.Zero;
    private IntPtr _defs = IntPtr.Zero;
    private readonly CoreMoodTermView[] _moodBuffer = new CoreMoodTermView[(int)MoodTerm.Count];
    private readonly int[] _repairCost = new int[CoreSummaryView.ResourceCount];

    public static CoreBridge Instance { get; private set; } = null!;

    public CoreStatus Status { get; private set; } = CoreStatus.NotLoaded;

    /// <summary>시작에 실패했을 때 플레이어에게 보여 줄 설명 (한국어)</summary>
    public string StatusMessage { get; private set; } = "";

    /// <summary>시작에 실패했을 때의 자세한 원인 (로그용)</summary>
    public string StatusDetail { get; private set; } = "";

    public CoreConfigView Config { get; private set; }

    public bool HasWorld => _world != IntPtr.Zero;

    /// <summary>정의 파일을 읽다가 생긴 문제 (시작 알림으로 보여 줌)</summary>
    public List<string> DataWarnings { get; } = new();

    /// <summary>예상하지 못한 오류가 났을 때 (알림으로 보여 주기 위해)</summary>
    [Signal]
    public delegate void CoreErrorEventHandler(string message);

    /// <summary>플레이어 명령이 상태를 바꿈 (일시정지 중에도 화면을 바로 맞추기 위해)</summary>
    [Signal]
    public delegate void CommandAppliedEventHandler();

    public override void _EnterTree()
    {
        Instance = this;
        ProcessMode = ProcessModeEnum.Always;
        // 스모크 테스트는 가장 먼저 로드되는 자동 로드에서 오류 기록기를 붙여야 시작 중 오류도 잡습니다
        Debugging.LaunchOptions.Parse(OS.GetCmdlineUserArgs());
        if (Debugging.LaunchOptions.Current.CountErrors)
        {
            Debugging.ErrorCounter.Install();
        }
        if (Engine.IsEditorHint())
        {
            return; // 에디터는 DLL 을 로드하지 않습니다
        }
        Initialize();
    }

    public override void _Notification(int what)
    {
        if (what == NotificationPredelete || what == NotificationWMCloseRequest)
        {
            DestroyWorld();
        }
    }

    public override void _ExitTree()
    {
        DestroyWorld();
        if (_defs != IntPtr.Zero)
        {
            Native.core_defs_destroy(_defs);
            _defs = IntPtr.Zero;
        }
    }

    // ---- 라이브러리 로드와 ABI 검사 ----------------------------------------

    private void Initialize()
    {
        try
        {
            InstallResolver();
            int abi = Native.core_abi_version();
            if (abi != ExpectedAbiVersion)
            {
                Fail(CoreStatus.AbiMismatch, Strings.AbiMismatch,
                    $"ABI version mismatch: C reports {abi}, C# expects {ExpectedAbiVersion}");
                return;
            }
            if (!CheckStructSizes(out string detail))
            {
                Fail(CoreStatus.AbiMismatch, Strings.AbiMismatch, detail);
                return;
            }
            Native.core_get_config(out CoreConfigView config);
            Config = config;
            LoadDefinitions();
            Status = CoreStatus.Ready;
            GD.Print($"castle_core loaded (ABI {abi}, {Config.TicksPerDay} ticks/day)");
        }
        catch (DllNotFoundException exception)
        {
            Fail(CoreStatus.LibraryMissing, Strings.LibraryMissing, $"{exception.Message} {_loadError}");
        }
        catch (EntryPointNotFoundException exception)
        {
            Fail(CoreStatus.AbiMismatch, Strings.AbiMismatch, exception.Message);
        }
        catch (BadImageFormatException exception)
        {
            Fail(CoreStatus.LibraryMissing, Strings.LibraryWrongArchitecture, exception.Message);
        }
    }

    /// <summary>
    /// res://data/ 의 정의 파일을 읽어 C 의 정의 묶음에 넣습니다. 파일을 Godot 이 읽어서 넘기므로
    /// 내보낸 게임(.pck 안의 파일)에서도 같은 방법으로 동작합니다. 없거나 망가진 파일은 기본값을 씁니다.
    /// </summary>
    private void LoadDefinitions()
    {
        if (Native.core_defs_create(out _defs) != 0)
        {
            _defs = IntPtr.Zero;
            return;
        }
        for (int file = 0; file < DataFiles.Length; file++)
        {
            byte[] bytes = Godot.FileAccess.GetFileAsBytes(DataFiles[file]);
            if (bytes.Length == 0)
            {
                DataWarnings.Add(Strings.DataFileMissing(DataFiles[file]));
                continue;
            }
            int result = Native.core_defs_load_text(_defs, file, bytes, bytes.Length, out int line);
            if (result != 0)
            {
                // 경고 이벤트는 새 월드를 만들 때 C 가 다시 알려 주므로 여기서는 로그만 남깁니다
                GD.Print($"definition file {DataFiles[file]} rejected at line {line}: {ResultName(result)}");
            }
        }
    }

    private void Fail(CoreStatus status, string message, string detail)
    {
        Status = status;
        StatusMessage = message;
        StatusDetail = detail;
        GD.PushError($"castle_core: {detail}");
    }

    private static bool CheckStructSizes(out string detail)
    {
        int result = Native.core_get_struct_sizes(out CoreStructSizes sizes, Marshal.SizeOf<CoreStructSizes>());
        if (result != 0)
        {
            detail = $"core_get_struct_sizes failed: {ResultName(result)}";
            return false;
        }
        var problems = new List<string>();
        Compare("CoreNewGameParams", sizes.NewGameParams, Marshal.SizeOf<CoreNewGameParams>(), problems);
        Compare("CoreConfigView", sizes.ConfigView, Marshal.SizeOf<CoreConfigView>(), problems);
        Compare("CoreSummaryView", sizes.SummaryView, Marshal.SizeOf<CoreSummaryView>(), problems);
        Compare("CoreEventView", sizes.EventView, Marshal.SizeOf<CoreEventView>(), problems);
        Compare("CoreVillagerView", sizes.VillagerView, Marshal.SizeOf<CoreVillagerView>(), problems);
        Compare("CoreBuildingView", sizes.BuildingView, Marshal.SizeOf<CoreBuildingView>(), problems);
        Compare("CoreBuildingDefView", sizes.BuildingDefView, Marshal.SizeOf<CoreBuildingDefView>(), problems);
        Compare("CoreTraitDefView", sizes.TraitDefView, Marshal.SizeOf<CoreTraitDefView>(), problems);
        Compare("CorePlacementView", sizes.PlacementView, Marshal.SizeOf<CorePlacementView>(), problems);
        Compare("CoreMoodTermView", sizes.MoodTermView, Marshal.SizeOf<CoreMoodTermView>(), problems);
        Compare("CoreLedgerView", sizes.LedgerView, Marshal.SizeOf<CoreLedgerView>(), problems);
        Compare("CoreMarketView", sizes.MarketView, Marshal.SizeOf<CoreMarketView>(), problems);
        Compare("CoreMerchantView", sizes.MerchantView, Marshal.SizeOf<CoreMerchantView>(), problems);
        Compare("CoreEnemyDefView", sizes.EnemyDefView, Marshal.SizeOf<CoreEnemyDefView>(), problems);
        Compare("CoreThreatView", sizes.ThreatView, Marshal.SizeOf<CoreThreatView>(), problems);
        Compare("CoreRaidView", sizes.RaidView, Marshal.SizeOf<CoreRaidView>(), problems);
        Compare("CoreEnemyView", sizes.EnemyView, Marshal.SizeOf<CoreEnemyView>(), problems);
        Compare("CoreResearchView", sizes.ResearchView, Marshal.SizeOf<CoreResearchView>(), problems);
        Compare("CoreRankView", sizes.RankView, Marshal.SizeOf<CoreRankView>(), problems);
        Compare("CoreRunStatsView", sizes.RunStatsView, Marshal.SizeOf<CoreRunStatsView>(), problems);
        Compare("CoreDifficultyView", sizes.DifficultyView, Marshal.SizeOf<CoreDifficultyView>(), problems);
        detail = string.Join("; ", problems);
        return problems.Count == 0;
    }

    private static void Compare(string name, int nativeSize, int managedSize, List<string> problems)
    {
        if (nativeSize != managedSize)
        {
            problems.Add($"{name}: C {nativeSize} bytes, C# {managedSize} bytes");
        }
    }

    /// <summary>
    /// "castle_core" 를 찾는 위치를 정합니다.
    /// 에디터에서 실행: res://bin/&lt;플랫폼&gt;/,  내보낸 게임: 실행 파일 옆 폴더.
    /// </summary>
    private static void InstallResolver()
    {
        if (_resolverInstalled)
        {
            return;
        }
        NativeLibrary.SetDllImportResolver(typeof(CoreBridge).Assembly, Resolve);
        _resolverInstalled = true;
    }

    private static IntPtr Resolve(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (libraryName != Native.LibraryName)
        {
            return IntPtr.Zero;
        }
        foreach (string candidate in CandidatePaths())
        {
            if (!File.Exists(candidate))
            {
                continue;
            }
            try
            {
                return NativeLibrary.Load(candidate);
            }
            catch (Exception exception) when (exception is DllNotFoundException or BadImageFormatException)
            {
                _loadError = $"({candidate}: {exception.Message})";
            }
        }
        if (_loadError.Length == 0)
        {
            _loadError = $"(looked in: {string.Join(", ", CandidatePaths())})";
        }
        return IntPtr.Zero; // 기본 검색으로 넘어가고, 거기서도 없으면 DllNotFoundException
    }

    private static IEnumerable<string> CandidatePaths()
    {
        string fileName;
        string platformDir;
        if (OperatingSystem.IsWindows())
        {
            fileName = "castle_core.dll";
            platformDir = "windows-x64";
        }
        else if (OperatingSystem.IsMacOS())
        {
            fileName = "libcastle_core.dylib";
            platformDir = "macos";
        }
        else
        {
            fileName = "libcastle_core.so";
            platformDir = "linux-x64";
        }
        string editorPath = ProjectSettings.GlobalizePath($"res://bin/{platformDir}/{fileName}");
        string exportPath = Path.Combine(Path.GetDirectoryName(OS.GetExecutablePath()) ?? ".", fileName);
        if (OS.HasFeature("editor"))
        {
            yield return editorPath;
            yield return exportPath;
        }
        else
        {
            yield return exportPath;
            yield return editorPath;
        }
    }

    public static string ResultName(int code)
    {
        try
        {
            IntPtr name = Native.core_result_name(code);
            return Marshal.PtrToStringUTF8(name) ?? code.ToString();
        }
        catch (Exception exception) when (exception is DllNotFoundException or EntryPointNotFoundException)
        {
            return code.ToString();
        }
    }

    // ---- 월드 수명 -------------------------------------------------------

    /// <summary>새 게임을 만듭니다. 이미 있던 월드는 해제합니다.</summary>
    public CoreResult NewGame(string castleName, int seed, Difficulty difficulty)
    {
        if (Status != CoreStatus.Ready)
        {
            return new CoreResult(CoreResultCode.InvalidArg);
        }
        var parameters = new CoreNewGameParams { Seed = seed, Difficulty = (int)difficulty };
        parameters.SetCastleName(castleName);
        CoreResult result = CoreResult.FromCode(Native.core_create(in parameters, _defs, out IntPtr world));
        if (!result.Ok)
        {
            Report("core_create", result);
            return result;
        }
        DestroyWorld();
        _world = world;
        return result;
    }

    public void DestroyWorld()
    {
        if (_world != IntPtr.Zero)
        {
            Native.core_destroy(_world);
            _world = IntPtr.Zero;
        }
    }

    // ---- 시간 ------------------------------------------------------------

    public CoreResult Tick(int ticks)
    {
        if (!HasWorld)
        {
            return new CoreResult(CoreResultCode.NullArg);
        }
        CoreResult result = CoreResult.FromCode(Native.core_tick(_world, ticks));
        // 패배한 뒤에는 C 가 시간을 멈춥니다(REJECTED): 오류가 아니라 게임이 끝난 것이므로 알리지 않음
        if (!result.Ok && result.Code != CoreResultCode.Rejected)
        {
            Report("core_tick", result);
        }
        return result;
    }

    // ---- 읽기 ------------------------------------------------------------

    public bool TryGetSummary(out CoreSummaryView summary)
    {
        summary = default;
        if (!HasWorld)
        {
            return false;
        }
        CoreResult result = CoreResult.FromCode(Native.core_get_summary(_world, out summary));
        if (!result.Ok)
        {
            Report("core_get_summary", result);
        }
        return result.Ok;
    }

    /// <summary>지형 격자를 새 배열로 복사합니다 (지도를 만들 때 한 번 씀).</summary>
    public byte[]? CopyTerrain(out int width, out int height)
    {
        width = 0;
        height = 0;
        if (!HasWorld || Native.core_get_map_size(_world, out width, out height) != 0)
        {
            return null;
        }
        byte[] buffer = new byte[width * height];
        CoreResult result = CoreResult.FromCode(Native.core_copy_terrain(_world, buffer, buffer.Length));
        if (!result.Ok)
        {
            Report("core_copy_terrain", result);
            return null;
        }
        return buffer;
    }

    /// <summary>이동 비용 격자를 buffer 에 복사합니다 (길찾기용, 마일스톤 2).</summary>
    public CoreResult CopyCostGrid(byte[] buffer, out int revision)
    {
        revision = 0;
        if (!HasWorld)
        {
            return new CoreResult(CoreResultCode.NullArg);
        }
        CoreResult result = CoreResult.FromCode(Native.core_copy_cost_grid(_world, buffer, buffer.Length, out revision));
        if (!result.Ok)
        {
            Report("core_copy_cost_grid", result);
        }
        return result;
    }

    /// <summary>쌓인 피드백 이벤트를 모두 꺼내 into 에 넣고 개수를 돌려줍니다.</summary>
    public int PollEvents(List<CoreEventView> into)
    {
        int count = 0;
        if (!HasWorld)
        {
            return 0;
        }
        while (Native.core_poll_event(_world, out CoreEventView coreEvent) == 1)
        {
            into.Add(coreEvent);
            count++;
        }
        return count;
    }

    /// <summary>도로 격자를 buffer 에 복사합니다 (1 = 도로).</summary>
    public bool CopyRoads(byte[] buffer)
    {
        return HasWorld && Native.core_copy_roads(_world, buffer, buffer.Length) == 0;
    }

    /// <summary>모든 주민 뷰를 buffer 에 복사합니다. 모자라면 buffer 를 키웁니다. 개수를 돌려줌</summary>
    public int CopyVillagers(ref CoreVillagerView[] buffer)
    {
        if (!HasWorld)
        {
            return 0;
        }
        int rc = Native.core_copy_villagers(_world, buffer, buffer.Length, out int count);
        if (rc == (int)CoreResultCode.BufferTooSmall)
        {
            buffer = new CoreVillagerView[count + 32];
            rc = Native.core_copy_villagers(_world, buffer, buffer.Length, out count);
        }
        if (rc != 0)
        {
            Report("core_copy_villagers", CoreResult.FromCode(rc));
            return 0;
        }
        return count;
    }

    public int CopyBuildings(ref CoreBuildingView[] buffer)
    {
        if (!HasWorld)
        {
            return 0;
        }
        int rc = Native.core_copy_buildings(_world, buffer, buffer.Length, out int count);
        if (rc == (int)CoreResultCode.BufferTooSmall)
        {
            buffer = new CoreBuildingView[count + 64];
            rc = Native.core_copy_buildings(_world, buffer, buffer.Length, out count);
        }
        if (rc != 0)
        {
            Report("core_copy_buildings", CoreResult.FromCode(rc));
            return 0;
        }
        return count;
    }

    public bool TryGetBuildingDef(BuildingType type, out CoreBuildingDefView def)
    {
        def = default;
        return HasWorld && Native.core_get_building_def(_world, (int)type, out def) == 0;
    }

    public bool TryGetTraitDef(int index, out CoreTraitDefView def)
    {
        def = default;
        return HasWorld && index >= 0 && Native.core_get_trait_def(_world, index, out def) == 0;
    }

    /// <summary>주민 기분 항목. 주민이 없으면 빈 범위</summary>
    public bool TryGetLedger(out CoreLedgerView ledger)
    {
        ledger = default;
        return HasWorld && Native.core_get_ledger(_world, out ledger) == 0;
    }

    public bool TryGetMarket(out CoreMarketView market)
    {
        market = default;
        return HasWorld && Native.core_get_market(_world, out market) == 0;
    }

    public bool TryGetMerchant(out CoreMerchantView merchant)
    {
        merchant = default;
        return HasWorld && Native.core_get_merchant(_world, out merchant) == 0;
    }

    public bool TryGetEnemyDef(int type, out CoreEnemyDefView def)
    {
        def = default;
        return HasWorld && type >= 0 && Native.core_get_enemy_def(_world, type, out def) == 0;
    }

    /// <summary>예고된 위협들 (도착이 빠른 순). 개수를 돌려줌</summary>
    public int CopyThreats(CoreThreatView[] buffer)
    {
        if (!HasWorld || Native.core_copy_threats(_world, buffer, buffer.Length, out int count) != 0)
        {
            return 0;
        }
        return count;
    }

    /// <summary>진행 중인 습격 (없으면 raid.Id == 0)</summary>
    public bool TryGetRaid(out CoreRaidView raid)
    {
        raid = default;
        return HasWorld && Native.core_get_raid(_world, out raid) == 0;
    }

    /// <summary>진행 중인 습격의 적 전체. 모자라면 buffer 를 키웁니다. 개수를 돌려줌</summary>
    public int CopyEnemies(ref CoreEnemyView[] buffer)
    {
        if (!HasWorld)
        {
            return 0;
        }
        int rc = Native.core_copy_enemies(_world, buffer, buffer.Length, out int count);
        if (rc == (int)CoreResultCode.BufferTooSmall)
        {
            buffer = new CoreEnemyView[count + 16];
            rc = Native.core_copy_enemies(_world, buffer, buffer.Length, out count);
        }
        if (rc != 0)
        {
            Report("core_copy_enemies", CoreResult.FromCode(rc));
            return 0;
        }
        return count;
    }

    /// <summary>연구 하나. 범위 밖이면 false (0 부터 차례로 읽으면 개수를 알 수 있음)</summary>
    public bool TryGetResearch(int index, out CoreResearchView research)
    {
        research = default;
        return HasWorld && index >= 0 && Native.core_get_research(_world, index, out research) == 0;
    }

    public bool TryGetRank(out CoreRankView rank)
    {
        rank = default;
        return HasWorld && Native.core_get_rank(_world, out rank) == 0;
    }

    public bool TryGetRunStats(out CoreRunStatsView stats)
    {
        stats = default;
        return HasWorld && Native.core_get_run_stats(_world, out stats) == 0;
    }

    /// <summary>난이도 설정 (월드가 없어도 됨)</summary>
    public bool TryGetDifficulty(Difficulty difficulty, out CoreDifficultyView view)
    {
        view = default;
        return Status == CoreStatus.Ready && Native.core_get_difficulty((int)difficulty, out view) == 0;
    }

    /// <summary>수리 비용 (자원 6개, 고칠 곳이 없으면 모두 0). 호출할 때마다 새 배열을 만들지 않게 버퍼를 돌려 씁니다</summary>
    public int[] GetRepairCost(int buildingId)
    {
        System.Array.Clear(_repairCost);
        if (HasWorld)
        {
            _ = Native.core_get_repair_cost(_world, buildingId, _repairCost, _repairCost.Length);
        }
        return _repairCost;
    }

    public ReadOnlySpan<CoreMoodTermView> GetMoodTerms(int villagerId)
    {
        if (!HasWorld || Native.core_get_mood_terms(_world, villagerId, _moodBuffer, _moodBuffer.Length, out int count) != 0)
        {
            return ReadOnlySpan<CoreMoodTermView>.Empty;
        }
        return new ReadOnlySpan<CoreMoodTermView>(_moodBuffer, 0, count);
    }

    // ---- 명령 (모든 상태 변경은 여기를 거쳐 C 로) ----------------------------

    public CorePlacementView CanPlace(BuildingType type, Vector2I tile)
    {
        CorePlacementView view = default;
        view.Result = (int)PlaceResult.BadType;
        if (HasWorld)
        {
            Native.core_can_place(_world, (int)type, tile.X, tile.Y, out view);
        }
        return view;
    }

    public CoreResult PlaceBuilding(BuildingType type, Vector2I tile, out int id, out PlaceResult reason)
    {
        id = 0;
        reason = PlaceResult.BadType;
        if (!HasWorld)
        {
            return new CoreResult(CoreResultCode.NullArg);
        }
        int rc = Native.core_place_building(_world, (int)type, tile.X, tile.Y, out id, out int why);
        reason = (PlaceResult)why;
        return Checked("core_place_building", rc);
    }

    public CoreResult PlaceLine(BuildingType type, Vector2I from, Vector2I to, out int placed, out PlaceResult reason)
    {
        placed = 0;
        reason = PlaceResult.BadType;
        if (!HasWorld)
        {
            return new CoreResult(CoreResultCode.NullArg);
        }
        int rc = Native.core_place_line(_world, (int)type, from.X, from.Y, to.X, to.Y, out placed, out int why);
        reason = (PlaceResult)why;
        return Checked("core_place_line", rc);
    }

    public CoreResult Demolish(int buildingId) =>
        HasWorld ? Checked("core_demolish", Native.core_demolish(_world, buildingId)) : new CoreResult(CoreResultCode.NullArg);

    public CoreResult RemoveRoad(Vector2I tile) =>
        HasWorld ? Checked("core_remove_road", Native.core_remove_road(_world, tile.X, tile.Y)) : new CoreResult(CoreResultCode.NullArg);

    public CoreResult AssignWorker(int villagerId, int buildingId) =>
        HasWorld ? Checked("core_assign_worker", Native.core_assign_worker(_world, villagerId, buildingId)) : new CoreResult(CoreResultCode.NullArg);

    public CoreResult UnassignWorker(int villagerId) =>
        HasWorld ? Checked("core_unassign_worker", Native.core_unassign_worker(_world, villagerId)) : new CoreResult(CoreResultCode.NullArg);

    public CoreResult ReleaseWorker(int villagerId) =>
        HasWorld ? Checked("core_release_worker", Native.core_release_worker(_world, villagerId)) : new CoreResult(CoreResultCode.NullArg);

    public CoreResult SetAutoAssign(int buildingId, bool enabled) =>
        HasWorld ? Checked("core_set_auto_assign", Native.core_set_auto_assign(_world, buildingId, enabled ? 1 : 0)) : new CoreResult(CoreResultCode.NullArg);

    /// <summary>시장 거래: lots &gt; 0 이면 사고, &lt; 0 이면 팝니다. gold = 금화 변화</summary>
    public CoreResult Trade(ResourceKind resource, int lots, out int gold, out DenyReason reason)
    {
        gold = 0;
        reason = DenyReason.None;
        if (!HasWorld)
        {
            return new CoreResult(CoreResultCode.NullArg);
        }
        int rc = Native.core_trade(_world, (int)resource, lots, out gold, out int why);
        reason = (DenyReason)why;
        return Checked("core_trade", rc);
    }

    public CoreResult AcceptDeal(int index, out DenyReason reason)
    {
        reason = DenyReason.None;
        if (!HasWorld)
        {
            return new CoreResult(CoreResultCode.NullArg);
        }
        int rc = Native.core_merchant_accept(_world, index, out int why);
        reason = (DenyReason)why;
        return Checked("core_merchant_accept", rc);
    }

    public CoreResult Repair(int buildingId, out DenyReason reason)
    {
        reason = DenyReason.None;
        if (!HasWorld)
        {
            return new CoreResult(CoreResultCode.NullArg);
        }
        int rc = Native.core_repair(_world, buildingId, out int why);
        reason = (DenyReason)why;
        return Checked("core_repair", rc);
    }

    /// <summary>연구 시작 (금화·마정석을 냄)</summary>
    public CoreResult StartResearch(int index, out DenyReason reason)
    {
        reason = DenyReason.None;
        if (!HasWorld)
        {
            return new CoreResult(CoreResultCode.NullArg);
        }
        int rc = Native.core_start_research(_world, index, out int why);
        reason = (DenyReason)why;
        return Checked("core_start_research", rc);
    }

    /// <summary>승급 (조건을 채우고 공물을 냄)</summary>
    public CoreResult RankUp(out DenyReason reason)
    {
        reason = DenyReason.None;
        if (!HasWorld)
        {
            return new CoreResult(CoreResultCode.NullArg);
        }
        int rc = Native.core_rank_up(_world, out int why);
        reason = (DenyReason)why;
        return Checked("core_rank_up", rc);
    }

    // ---- 보고 (Godot 이 본 물리적 사실 → C) -----------------------------------

    public CoreResult VillagerArrived(int villagerId, int intentRev) =>
        HasWorld ? Checked("core_villager_arrived", Native.core_villager_arrived(_world, villagerId, intentRev)) : new CoreResult(CoreResultCode.NullArg);

    public CoreResult VillagerUnreachable(int villagerId, int intentRev) =>
        HasWorld ? Checked("core_villager_unreachable", Native.core_villager_unreachable(_world, villagerId, intentRev)) : new CoreResult(CoreResultCode.NullArg);

    // 습격 보고: 전투는 Godot 이 하고, 결과(피해, 처치, 약탈, 달아남, 끝)만 C 에 알립니다.
    // C 가 규칙대로 적용합니다. 습격이 이미 끝났으면 IGNORED 가 조용히 돌아옵니다.

    public CoreResult ReportBuildingDamage(int raidId, int buildingId, int damage) =>
        HasWorld ? Checked("core_report_building_damage", Native.core_report_building_damage(_world, raidId, buildingId, damage)) : new CoreResult(CoreResultCode.NullArg);

    public CoreResult ReportVillagerDamage(int raidId, int villagerId, int damage) =>
        HasWorld ? Checked("core_report_villager_damage", Native.core_report_villager_damage(_world, raidId, villagerId, damage)) : new CoreResult(CoreResultCode.NullArg);

    public CoreResult ReportEnemyKilled(int raidId, int enemyId) =>
        HasWorld ? Checked("core_report_enemy_killed", Native.core_report_enemy_killed(_world, raidId, enemyId)) : new CoreResult(CoreResultCode.NullArg);

    public CoreResult ReportLoot(int raidId, int enemyId, int buildingId, out int amount)
    {
        amount = 0;
        return HasWorld ? Checked("core_report_loot", Native.core_report_loot(_world, raidId, enemyId, buildingId, out amount)) : new CoreResult(CoreResultCode.NullArg);
    }

    public CoreResult ReportEnemyEscaped(int raidId, int enemyId) =>
        HasWorld ? Checked("core_report_enemy_escaped", Native.core_report_enemy_escaped(_world, raidId, enemyId)) : new CoreResult(CoreResultCode.NullArg);

    public CoreResult ReportRaidEnded(int raidId) =>
        HasWorld ? Checked("core_report_raid_ended", Native.core_report_raid_ended(_world, raidId)) : new CoreResult(CoreResultCode.NullArg);

    /// <summary>
    /// 결과 코드를 CoreResult 로 바꿉니다. 규칙상 거절(REJECTED), 오래된 보고(IGNORED), 이미 사라진 대상(NOT_FOUND)은
    /// 게임 중에 늘 있는 일이라 조용히 돌려주고, 나머지 오류만 버그로 기록합니다.
    /// </summary>
    private CoreResult Checked(string call, int code)
    {
        CoreResult result = CoreResult.FromCode(code);
        if (!result.Ok && result.Code is not (CoreResultCode.Rejected or CoreResultCode.Ignored or CoreResultCode.NotFound))
        {
            Report(call, result);
        }
        // 보고는 플레이어 명령이 아니므로 화면을 바로 맞추지 않습니다 (다음 틱에 맞춤)
        if (result.Ok && !call.StartsWith("core_villager_", System.StringComparison.Ordinal) &&
            !call.StartsWith("core_report_", System.StringComparison.Ordinal))
        {
            EmitSignal(SignalName.CommandApplied);
        }
        return result;
    }

    // ---- 저장, 불러오기 (메뉴는 마일스톤 6) ------------------------------

    public CoreResult Save(string path)
    {
        if (!HasWorld)
        {
            return new CoreResult(CoreResultCode.NullArg);
        }
        string absolute = ProjectSettings.GlobalizePath(path);
        return CoreResult.FromCode(Native.core_save(_world, Utf8Text.ToNullTerminated(absolute)));
    }

    /// <summary>불러오기에 성공하면 지금 월드를 바꿉니다. 실패하면 지금 월드는 그대로입니다.</summary>
    public CoreResult Load(string path, out int errorLine)
    {
        errorLine = 0;
        if (Status != CoreStatus.Ready)
        {
            return new CoreResult(CoreResultCode.InvalidArg);
        }
        string absolute = ProjectSettings.GlobalizePath(path);
        CoreResult result = CoreResult.FromCode(
            Native.core_load(Utf8Text.ToNullTerminated(absolute), _defs, out IntPtr loaded, out errorLine));
        if (result.Ok)
        {
            DestroyWorld();
            _world = loaded;
        }
        return result;
    }

    // ---- 디버그 ----------------------------------------------------------

    /// <summary>불변 조건 위반 개수 (0 이면 정상)</summary>
    public int Validate()
    {
        return HasWorld ? Native.core_validate(_world) : -1;
    }

    public ulong StateHash()
    {
        if (!HasWorld || Native.core_state_hash(_world, out ulong hash) != 0)
        {
            return 0;
        }
        return hash;
    }

    private void Report(string call, CoreResult result)
    {
        // 인자 오류, 메모리 부족은 버그이므로 로그에 남기고 알림으로도 보여 줍니다
        GD.PushError($"{call} failed: {result.Name}");
        EmitSignal(SignalName.CoreError, result.Message);
    }
}
