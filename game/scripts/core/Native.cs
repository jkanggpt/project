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
}

public enum Terrain
{
    Grass = 0,
    Forest = 1,
    Rock = 2,
    Water = 3,
    Mana = 4,
}

public enum ResourceKind
{
    Gold = 0,
    Mana = 1,
    Food = 2,
    Wood = 3,
    Stone = 4,
    Herbs = 5,
}

public enum Difficulty
{
    Peaceful = 0,
    Normal = 1,
    Harsh = 2,
}

public enum CoreEventType
{
    None = 0,
    DayStarted = 1,
    SeasonChanged = 2,
    NightFell = 3,
    Dawn = 4,
    Warning = 5,
}

public enum CoreWarning
{
    None = 0,
    EventsDropped = 1,
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
    public fixed int Resources[ResourceCount];
    /// <summary>자원별 저장 한도. 0 이면 한도 없음</summary>
    public fixed int ResourceCapacity[ResourceCount];
    public fixed byte CastleName[CoreNewGameParams.NameBytes];

    public readonly int GetResource(ResourceKind resource)
    {
        int index = (int)resource;
        return index >= 0 && index < ResourceCount ? Resources[index] : 0;
    }

    public readonly int GetCapacity(ResourceKind resource)
    {
        int index = (int)resource;
        return index >= 0 && index < ResourceCount ? ResourceCapacity[index] : 0;
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
public struct CoreEventView
{
    public int Type;
    public int A;
    public int B;
    public int C;
    public int D;
}

[StructLayout(LayoutKind.Sequential)]
public struct CoreStructSizes
{
    public int NewGameParams;
    public int ConfigView;
    public int SummaryView;
    public int EventView;
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

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_abi_version();

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_get_struct_sizes(out CoreStructSizes sizes, int sizesSize);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_get_config(out CoreConfigView config);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr core_result_name(int code);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_create(in CoreNewGameParams parameters, out IntPtr world);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void core_destroy(IntPtr world);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_tick(IntPtr world, int ticks);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_get_summary(IntPtr world, out CoreSummaryView summary);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_get_map_size(IntPtr world, out int width, out int height);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_copy_terrain(IntPtr world, [Out] byte[] buffer, int capacity);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_copy_cost_grid(IntPtr world, [Out] byte[] buffer, int capacity, out int revision);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_poll_event(IntPtr world, out CoreEventView coreEvent);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_save(IntPtr world, byte[] pathUtf8);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_load(byte[] pathUtf8, out IntPtr world, out int errorLine);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_validate(IntPtr world);

    [DllImport(LibraryName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int core_state_hash(IntPtr world, out ulong hash);
}
