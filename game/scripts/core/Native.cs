// Native.cs — C 코어(castle_core)와 주고받는 구조체와 함수 선언
//
// 이 파일의 구조체는 core/include/castle_core.h 의 뷰 구조체와 필드 순서·크기가 똑같아야 합니다.
// 모두 int 와 고정 크기 byte 배열로만 되어 있어서(blittable) 복사 없이 그대로 C 에 넘어갑니다.
// 게임 시작 때 CoreBridge 가 C 가 알려 준 크기와 Marshal.SizeOf 를 비교해서, 다르면 실행을 멈춥니다.
// 이 클래스는 CoreBridge 만 씁니다. 다른 코드는 CoreBridge 를 거쳐야 합니다.
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace MagicCastle.Core;

/// <summary>C 의 CoreResult 와 같은 값</summary>
public enum CoreResultCode
{
    Ignored = 1,
    Ok = 0,
    NullArg = -1,
    InvalidArg = -2,
    OutOfMemory = -3,
    BufferTooSmall = -4,
    FileOpen = -5,
    FileIo = -6,
    Format = -7,
    Version = -8,
    StructSize = -9,
    Rejected = -10,
    NotFound = -11,
}

public enum Terrain { Grass = 0, Forest = 1, Rock = 2, Water = 3, Mana = 4 }

public enum ResourceKind { Gold = 0, Mana = 1, Food = 2, Wood = 3, Stone = 4, Herbs = 5 }

public enum Difficulty { Peaceful = 0, Normal = 1, Harsh = 2 }

public enum BuildingType
{
    Keep = 0, House = 1, Farm = 2, Gatherer = 3, Lumber = 4, Quarry = 5, ManaMine = 6, Warehouse = 7,
    Market = 8, Barracks = 9, Tower = 10, Wall = 11, Gate = 12, Healer = 13, MageTower = 14, Road = 15, Count = 16,
}

public enum BuildingState { Constructing = 0, Active = 1, Ruined = 2 }

public enum BuildCategory { Special = 0, Housing = 1, Resource = 2, Storage = 3, Defense = 4, Service = 5, Road = 6 }

public enum PlaceResult
{
    Ok = 0, BadType = 1, OutOfBounds = 2, TooCloseEdge = 3, BadTerrain = 4, Occupied = 5, RoadInWay = 6,
    BlocksDoor = 7, NoDoor = 8, NeedsForest = 9, NeedsRock = 10, NeedsMana = 11, Locked = 12, CannotAfford = 13,
    AlreadyThere = 14, BlocksPath = 15, Unreachable = 16,
}

public enum VillagerState
{
    Idle = 0, Sleep = 1, GoWork = 2, Working = 3, Eat = 4, SeekHealing = 5, Flee = 6, GuardPost = 7, Leaving = 8,
}

[Flags]
public enum IntentFlags { None = 0, Hold = 1, SpawnEdge = 2 }

[Flags]
public enum VillagerFlags { None = 0, Manual = 1, Unpaid = 2, Starving = 4, Homeless = 8, Unhappy = 16, Sick = 32 }

public enum MoodTerm
{
    Base = 0, Hunger = 1, Rest = 2, Safety = 3, Housing = 4, Traits = 5, Wages = 6, Mourning = 7, Cold = 8, Sickness = 9,
    Count = 10,
}

[Flags]
public enum BuildingFlags { None = 0, Cold = 1 }

public enum Weather { Clear = 0, Rain = 1, Snow = 2, Storm = 3, Count = 4 }

public enum NatureEvent { None = 0, Drought = 1, Plague = 2, Storm = 3, ColdSnap = 4, Bumper = 5, Count = 6 }

public enum LedgerCategory
{
    Production = 0, Consumption = 1, Wages = 2, Taxes = 3, Construction = 4, Refund = 5, Trade = 6, Heating = 7,
    Raid = 8, Other = 9, Healing = 10, Repair = 11, Count = 12,
}

public enum DenyReason
{
    None = 0, NoMarket = 1, NoTrader = 2, NotTradable = 3, NoGold = 4, NoGoods = 5, StorageFull = 6, NoMerchant = 7,
    DealTaken = 8, NotDamaged = 9, UnderConstruction = 10, BadAmount = 11, NoResources = 12, Count = 13,
}

public enum DeathCause { Starvation = 1, Illness = 2, Raid = 3 }

public enum DataFile { Buildings = 0, Traits = 1, Names = 2, Enemies = 3, Count = 4 }

public enum ThreatType { None = 0, Bandits = 1, Monsters = 2, Army = 3, Count = 4 }

public enum Direction { North = 0, East = 1, South = 2, West = 3, Count = 4 }

public enum EnemyBehavior { Thief = 0, Raider = 1, Monster = 2 }

public enum EnemyState { Alive = 0, Killed = 1, Escaped = 2 }

public enum CoreEventType
{
    None = 0, DayStarted = 1, SeasonChanged = 2, NightFell = 3, Dawn = 4, Warning = 5, BuildingPlaced = 6,
    BuildingCompleted = 7, BuildingRemoved = 8, Produced = 9, Immigrant = 10, VillagerLeaving = 11,
    VillagerLeft = 12, VillagerDied = 13, PathBlocked = 14, StorageFull = 15, WagesUnpaid = 16, Story = 17,
    Starving = 18, DefWarning = 19, WeatherChanged = 20, NatureForecast = 21, NatureStarted = 22, NatureEnded = 23,
    BuildingDamaged = 24, BuildingRuined = 25, BuildingRepaired = 26, VillagerSick = 27, VillagerCured = 28,
    MerchantArrived = 29, MerchantLeft = 30, WinterWarning = 31, HousesCold = 32, ThreatScheduled = 33,
    RaidStarted = 34, RaidEnded = 35, EnemyKilled = 36, LootStolen = 37, ThreatNear = 38,
}

public enum CoreWarning { None = 0, EventsDropped = 1 }

public enum Story { FirstProduce = 1, MovedIn = 2, NewJob = 3 }

/// <summary>이동 비용 격자의 특별한 값 (castle_core.h 의 CORE_COST_*)</summary>
public static class CostCodes
{
    public const byte Blocked = 0;
    public const byte Normal = 10;
    public const byte Gate = 253;
    public const byte Wall = 254;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct CoreNewGameParams
{
    public const int NameBytes = 48;
    public fixed byte CastleName[NameBytes];
    public int Seed;
    public int Difficulty;

    /// <summary>이름을 UTF-8 로 넣습니다. 너무 길면 글자 중간을 자르지 않도록 글자 단위로 줄입니다.</summary>
    public void SetCastleName(string name)
    {
        fixed (byte* buffer = CastleName)
        {
            Utf8Text.Write(name, new Span<byte>(buffer, NameBytes));
        }
    }
}

[StructLayout(LayoutKind.Sequential)]
public struct CoreConfigView
{
    public int AbiVersion;
    public int MinutesPerTick;
    public int TicksPerDay;
    public int DaysPerSeason;
    public int SeasonsPerYear;
    public int RealMsPerTick;
    public int MaxSpeed;
    public int MaxTicksPerCall;
    public int NightStartMinute;
    public int NightEndMinute;
    public int ResourceCount;
    public int TerrainCount;
    public int BuildingTypeCount;
    public int VillagerStateCount;
    public int WalkMilliTilesPerTick;
    public int WorkStartMinute;
    public int WorkEndMinute;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct CoreSummaryView
{
    public const int ResourceCount = 6;

    public int Tick;
    public int Day;
    public int MinuteOfDay;
    public int Season;
    public int DayOfSeason;
    public int Year;
    public int IsNight;
    public int Difficulty;
    public int Seed;
    public int MapWidth;
    public int MapHeight;
    public int KeepX;
    public int KeepY;
    public int KeepSize;
    public int CostGridRev;
    public int Population;
    public int HousingCapacity;
    public int Homeless;
    public int Employed;
    public int JobSlots;
    public int AverageMood;
    public int BuildingCount;
    public int VillagerCountHint;
    public int BuildingCountHint;
    public int Rank;
    public int Weather;
    public int NatureEvent;
    public int NatureDaysLeft;
    public int ForecastEvent;
    public int MerchantDaysLeft;
    public int MarketOpen;
    public int SickCount;
    public int ColdHouses;
    public int HeatingPerHouse;
    public int FarmPercent;
    public int WinterFoodNeed;
    public int WinterWoodNeed;
    public int ThreatType;
    public int ThreatDirection;
    public int ThreatTicksLeft;
    public int ThreatStrength;
    public int RaidId;
    public int RaidType;
    public int RaidEnemiesLeft;
    public int Guards;
    public fixed int Resources[ResourceCount];
    /// <summary>자원별 저장 한도. 0 이면 한도 없음</summary>
    public fixed int ResourceCapacity[ResourceCount];
    /// <summary>어제 하루 동안의 순변화</summary>
    public fixed int ResourceDelta[ResourceCount];
    public fixed byte CastleName[CoreNewGameParams.NameBytes];

    public readonly int GetResource(ResourceKind resource) => Pick(resource, 0);
    public readonly int GetCapacity(ResourceKind resource) => Pick(resource, 1);
    public readonly int GetDelta(ResourceKind resource) => Pick(resource, 2);

    private readonly int Pick(ResourceKind resource, int which)
    {
        int index = (int)resource;
        if (index < 0 || index >= ResourceCount)
        {
            return 0;
        }
        return which switch
        {
            0 => Resources[index],
            1 => ResourceCapacity[index],
            _ => ResourceDelta[index],
        };
    }

    public readonly string GetCastleName()
    {
        fixed (byte* buffer = CastleName)
        {
            return Utf8Text.Read(new ReadOnlySpan<byte>(buffer, CoreNewGameParams.NameBytes));
        }
    }
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct CoreEventView
{
    public int Type;
    public int A;
    public int B;
    public int C;
    public int D;
    public fixed byte Text[CoreNewGameParams.NameBytes];

    public readonly string GetText()
    {
        fixed (byte* buffer = Text)
        {
            return Utf8Text.Read(new ReadOnlySpan<byte>(buffer, CoreNewGameParams.NameBytes));
        }
    }
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct CoreVillagerView
{
    public int Id;
    public int State;
    public int IntentRev;
    public int IntentFlags;
    public int TargetBuilding;
    public int TargetX;
    public int TargetY;
    public int AtTarget;
    public int LocX;
    public int LocY;
    public int HomeId;
    public int WorkplaceId;
    public int Job;
    public int Mood;
    public int MoodTarget;
    public int Health;
    public int Hunger;
    public int Rest;
    public int Safety;
    public int Housing;
    public int Strength;
    public int Intellect;
    public int Dexterity;
    public int Vitality;
    public int TraitA;
    public int TraitB;
    public int Age;
    public int Flags;
    public int WalkSpeed;
    public fixed byte Name[CoreNewGameParams.NameBytes];

    public readonly string GetName()
    {
        fixed (byte* buffer = Name)
        {
            return Utf8Text.Read(new ReadOnlySpan<byte>(buffer, CoreNewGameParams.NameBytes));
        }
    }

    public readonly bool Has(VillagerFlags flag) => (Flags & (int)flag) != 0;
    public readonly bool Holding => (IntentFlags & (int)MagicCastle.Core.IntentFlags.Hold) != 0;
}

[StructLayout(LayoutKind.Sequential)]
public struct CoreBuildingView
{
    public int Id;
    public int Type;
    public int X;
    public int Y;
    public int W;
    public int H;
    public int State;
    public int Hp;
    public int MaxHp;
    public int BuildProgress;
    public int BuildTotal;
    public int Workers;
    public int WorkerSlots;
    public int Residents;
    public int ResidentSlots;
    public int WorkingNow;
    public int AutoAssign;
    public int DoorX;
    public int DoorY;
    public int Productivity;
    public int Flags;
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct CoreBuildingDefView
{
    public const int KeyBytes = 24;
    public int Type;
    public int W;
    public int H;
    public fixed int Cost[CoreSummaryView.ResourceCount];
    public int BuildTicks;
    public int MaxHp;
    public int WorkerSlots;
    public int ResidentSlots;
    public int Storage;
    public int Produces;
    public int Rate;
    public int Secondary;
    public int SecondaryRate;
    public int WorkRadius;
    public int Placement;
    public int MinTerrain;
    public int IdealTerrain;
    public int Wage;
    public int RequiredRank;
    public int Category;
    public int Line;
    public int Available;
    /// <summary>한 번에 주는 피해 (감시탑, 성채 망루, 병영의 경비병)</summary>
    public int Attack;
    public int AttackRange;
    /// <summary>공격 사이 (게임 속 분)</summary>
    public int AttackCooldown;
    public fixed byte Key[KeyBytes];

    public readonly int GetCost(ResourceKind resource)
    {
        int index = (int)resource;
        return index >= 0 && index < CoreSummaryView.ResourceCount ? Cost[index] : 0;
    }

    public readonly string GetKey()
    {
        fixed (byte* buffer = Key)
        {
            return Utf8Text.Read(new ReadOnlySpan<byte>(buffer, KeyBytes));
        }
    }
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct CoreTraitDefView
{
    public int Index;
    public int WorkPct;
    public int FoodPct;
    public int Mood;
    public int Bravery;
    public int MagicPct;
    public fixed byte Key[CoreBuildingDefView.KeyBytes];

    public readonly string GetKey()
    {
        fixed (byte* buffer = Key)
        {
            return Utf8Text.Read(new ReadOnlySpan<byte>(buffer, CoreBuildingDefView.KeyBytes));
        }
    }
}

[StructLayout(LayoutKind.Sequential)]
public unsafe struct CorePlacementView
{
    public int Result;
    public int DoorX;
    public int DoorY;
    public int TerrainCount;
    public int Productivity;
    public fixed int Cost[CoreSummaryView.ResourceCount];
}

[StructLayout(LayoutKind.Sequential)]
public struct CoreMoodTermView
{
    public int Term;
    public int Value;
}

[StructLayout(LayoutKind.Sequential)]
public struct CoreStructSizes
{
    public int NewGameParams;
    public int ConfigView;
    public int SummaryView;
    public int EventView;
    public int VillagerView;
    public int BuildingView;
    public int BuildingDefView;
    public int TraitDefView;
    public int PlacementView;
    public int MoodTermView;
    public int LedgerView;
    public int MarketView;
    public int MerchantView;
    public int EnemyDefView;
    public int ThreatView;
    public int RaidView;
    public int EnemyView;
}

/// <summary>적 정의 (data/enemies.def)</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct CoreEnemyDefView
{
    public int Type;
    public int Hp;
    public int Damage;
    /// <summary>공격 사이 (게임 속 분)</summary>
    public int Cooldown;
    /// <summary>한 틱에 걷는 칸 × 1000</summary>
    public int Speed;
    public int Range;
    public int Behavior;
    public int LootResource;
    public int LootAmount;
    public int ManaReward;
    public fixed byte Key[CoreBuildingDefView.KeyBytes];

    public readonly string GetKey()
    {
        fixed (byte* buffer = Key)
        {
            return Utf8Text.Read(new ReadOnlySpan<byte>(buffer, CoreBuildingDefView.KeyBytes));
        }
    }
}

[StructLayout(LayoutKind.Sequential)]
public struct CoreThreatView
{
    public int Type;
    public int Direction;
    public int EtaTick;
    public int TicksLeft;
    public int Strength;
}

[StructLayout(LayoutKind.Sequential)]
public struct CoreRaidView
{
    public int Id;
    public int Type;
    public int Direction;
    public int SpawnX;
    public int SpawnY;
    public int EnemyCount;
    public int Alive;
    public int StartedTick;
    public int Stolen;
}

[StructLayout(LayoutKind.Sequential)]
public struct CoreEnemyView
{
    public int Id;
    public int Type;
    public int State;
}

/// <summary>장부: [자원 × 항목] (오늘 쌓는 중, 어제 하루)</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct CoreLedgerView
{
    public const int Cells = CoreSummaryView.ResourceCount * (int)LedgerCategory.Count;
    public fixed int Today[Cells];
    public fixed int Yesterday[Cells];

    public readonly int Get(ResourceKind resource, LedgerCategory category, bool yesterday)
    {
        int index = (int)resource * (int)LedgerCategory.Count + (int)category;
        if (index < 0 || index >= Cells)
        {
            return 0;
        }
        return yesterday ? Yesterday[index] : Today[index];
    }
}

/// <summary>시장 가격 (한 묶음 = Lot 개, 금화)</summary>
[StructLayout(LayoutKind.Sequential)]
public unsafe struct CoreMarketView
{
    public const int Lot = 10;
    public const int MaxLots = 10;
    public int Open;
    public int ClosedReason;
    public fixed int BuyPrice[CoreSummaryView.ResourceCount];
    public fixed int SellPrice[CoreSummaryView.ResourceCount];
    public fixed int BasePrice[CoreSummaryView.ResourceCount];
    public fixed int Trend[CoreSummaryView.ResourceCount];

    public readonly bool Tradable(ResourceKind r) => Index(r) >= 0 && BasePrice[Index(r)] > 0;
    public readonly int Buy(ResourceKind r) => Index(r) >= 0 ? BuyPrice[Index(r)] : 0;
    public readonly int Sell(ResourceKind r) => Index(r) >= 0 ? SellPrice[Index(r)] : 0;
    public readonly int Base(ResourceKind r) => Index(r) >= 0 ? BasePrice[Index(r)] : 0;
    public readonly int TrendPercent(ResourceKind r) => Index(r) >= 0 ? Trend[Index(r)] : 0;

    private static int Index(ResourceKind r) => (int)r >= 0 && (int)r < CoreSummaryView.ResourceCount ? (int)r : -1;
}

/// <summary>떠돌이 상인의 제안 하나: Give 를 내고 Get 을 받음</summary>
[StructLayout(LayoutKind.Sequential)]
public struct CoreDealView
{
    public int GiveResource;
    public int GiveAmount;
    public int GetResource;
    public int GetAmount;
    public int Taken;
}

[StructLayout(LayoutKind.Sequential)]
public struct CoreMerchantView
{
    public const int MaxDeals = 4;
    public int DaysLeft;
    public int DealCount;
    public CoreDealView Deal0;
    public CoreDealView Deal1;
    public CoreDealView Deal2;
    public CoreDealView Deal3;

    public readonly CoreDealView GetDeal(int index) => index switch
    {
        0 => Deal0,
        1 => Deal1,
        2 => Deal2,
        _ => Deal3,
    };
}

/// <summary>고정 크기 UTF-8 버퍼 읽기/쓰기</summary>
public static class Utf8Text
{
    public static string Read(ReadOnlySpan<byte> buffer)
    {
        int length = buffer.IndexOf((byte)0);
        if (length < 0)
        {
            length = buffer.Length;
        }
        return Encoding.UTF8.GetString(buffer[..length]);
    }

    /// <summary>buffer 에 '\0' 으로 끝나게 씁니다. 넘치면 마지막 글자부터 버립니다.</summary>
    public static void Write(string text, Span<byte> buffer)
    {
        buffer.Clear();
        string value = text;
        while (Encoding.UTF8.GetByteCount(value) > buffer.Length - 1 && value.Length > 0)
        {
            // 서로게이트 쌍(이모지 등)을 반으로 자르지 않게 글자 단위로 줄입니다
            int cut = char.IsLowSurrogate(value[^1]) && value.Length > 1 ? 2 : 1;
            value = value[..^cut];
        }
        Encoding.UTF8.GetBytes(value, buffer);
    }

    /// <summary>C 에 넘길 '\0' 으로 끝나는 UTF-8 바이트 배열</summary>
    public static byte[] ToNullTerminated(string text)
    {
        byte[] bytes = new byte[Encoding.UTF8.GetByteCount(text) + 1];
        Encoding.UTF8.GetBytes(text, bytes);
        return bytes;
    }
}

/// <summary>castle_core 의 함수들. 반드시 CoreBridge 를 통해서만 부릅니다.</summary>
internal static class Native
{
    public const string LibraryName = "castle_core";
    private const CallingConvention Cdecl = CallingConvention.Cdecl;

    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_abi_version();
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_struct_sizes(out CoreStructSizes sizes, int sizesSize);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_config(out CoreConfigView config);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern IntPtr core_result_name(int code);

    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_defs_create(out IntPtr defs);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern void core_defs_destroy(IntPtr defs);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_defs_load_text(IntPtr defs, int fileId, byte[] text, int length, out int line);

    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_create(in CoreNewGameParams parameters, IntPtr defs, out IntPtr world);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern void core_destroy(IntPtr world);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_tick(IntPtr world, int ticks);

    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_summary(IntPtr world, out CoreSummaryView summary);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_map_size(IntPtr world, out int width, out int height);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_copy_terrain(IntPtr world, [Out] byte[] buffer, int capacity);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_copy_roads(IntPtr world, [Out] byte[] buffer, int capacity);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_copy_cost_grid(IntPtr world, [Out] byte[] buffer, int capacity, out int revision);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_copy_villagers(IntPtr world, [Out] CoreVillagerView[] buffer, int capacity, out int count);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_copy_buildings(IntPtr world, [Out] CoreBuildingView[] buffer, int capacity, out int count);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_building_def(IntPtr world, int type, out CoreBuildingDefView def);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_trait_def(IntPtr world, int index, out CoreTraitDefView def);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_mood_terms(IntPtr world, int villagerId, [Out] CoreMoodTermView[] buffer, int capacity, out int count);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_ledger(IntPtr world, out CoreLedgerView ledger);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_enemy_def(IntPtr world, int type, out CoreEnemyDefView def);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_copy_threats(IntPtr world, [Out] CoreThreatView[] buffer, int capacity, out int count);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_raid(IntPtr world, out CoreRaidView raid);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_copy_enemies(IntPtr world, [Out] CoreEnemyView[] buffer, int capacity, out int count);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_market(IntPtr world, out CoreMarketView market);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_merchant(IntPtr world, out CoreMerchantView merchant);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_get_repair_cost(IntPtr world, int buildingId, [Out] int[] cost, int count);

    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_can_place(IntPtr world, int type, int x, int y, out CorePlacementView view);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_place_building(IntPtr world, int type, int x, int y, out int id, out int reason);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_place_line(IntPtr world, int type, int x0, int y0, int x1, int y1, out int placed, out int reason);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_demolish(IntPtr world, int buildingId);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_remove_road(IntPtr world, int x, int y);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_assign_worker(IntPtr world, int villagerId, int buildingId);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_unassign_worker(IntPtr world, int villagerId);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_release_worker(IntPtr world, int villagerId);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_set_auto_assign(IntPtr world, int buildingId, int enabled);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_trade(IntPtr world, int resource, int lots, out int gold, out int reason);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_merchant_accept(IntPtr world, int index, out int reason);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_repair(IntPtr world, int buildingId, out int reason);

    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_villager_arrived(IntPtr world, int villagerId, int intentRev);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_villager_unreachable(IntPtr world, int villagerId, int intentRev);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_report_building_damage(IntPtr world, int raidId, int buildingId, int damage);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_report_villager_damage(IntPtr world, int raidId, int villagerId, int damage);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_report_enemy_killed(IntPtr world, int raidId, int enemyId);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_report_loot(IntPtr world, int raidId, int enemyId, int buildingId, out int amount);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_report_enemy_escaped(IntPtr world, int raidId, int enemyId);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_report_raid_ended(IntPtr world, int raidId);

    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_poll_event(IntPtr world, out CoreEventView coreEvent);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_save(IntPtr world, byte[] pathUtf8);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_load(byte[] pathUtf8, IntPtr defs, out IntPtr world, out int errorLine);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_validate(IntPtr world);
    [DllImport(LibraryName, CallingConvention = Cdecl)] public static extern int core_state_hash(IntPtr world, out ulong hash);
}
