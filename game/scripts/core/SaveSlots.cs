// SaveSlots.cs — 세이브 슬롯: user://saves/slot1~3.sav 와 autosave.sav (SPEC §7)
//
// 파일을 쓰고 읽는 일과 검사는 모두 C 가 합니다 (core_save: 임시 파일 → 바꿔치기 + .bak, core_peek_save, core_load).
// 여기서는 슬롯 번호를 경로로 바꾸고, 목록에 보여 줄 요약과 "가장 최근 세이브"(이어하기)를 고를 뿐입니다.
// 자동 검사는 학생의 세이브를 건드리지 않도록 다른 폴더(user://saves_automated)를 씁니다.
using System;
using Godot;
using MagicCastle.Debugging;

namespace MagicCastle.Core;

/// <summary>슬롯 하나의 상태 (목록 한 줄)</summary>
public readonly record struct SlotInfo(int Slot, bool Exists, CoreSaveInfoView Info, ulong ModifiedUnix,
    bool BackupOk, CoreSaveInfoView Backup)
{
    /// <summary>본 파일을 불러올 수 있음</summary>
    public bool Ok => Exists && Info.Ok;

    /// <summary>본 파일은 망가졌지만 예비 파일(.bak)은 불러올 수 있음</summary>
    public bool OnlyBackup => Exists && !Info.Ok && BackupOk;
}

public static class SaveSlots
{
    /// <summary>0 = 자동 저장, 1~3 = 슬롯</summary>
    public const int AutosaveSlot = 0;
    public const int SlotCount = 3;

    public static string Directory => LaunchOptions.Current.Automated ? "user://saves_automated" : "user://saves";

    public static string PathOf(int slot) =>
        slot == AutosaveSlot ? $"{Directory}/autosave.sav" : $"{Directory}/slot{slot}.sav";

    public static string BackupPathOf(int slot) => PathOf(slot) + ".bak";

    /// <summary>슬롯 요약. 본 파일이 망가졌으면 예비 파일도 확인합니다.</summary>
    public static SlotInfo Peek(int slot)
    {
        string path = PathOf(slot);
        if (!FileAccess.FileExists(path))
        {
            return new SlotInfo(slot, false, default, 0, false, default);
        }
        CoreSaveInfoView info = CoreBridge.Instance.PeekSave(path);
        ulong modified = FileAccess.GetModifiedTime(path);
        CoreSaveInfoView backup = default;
        bool backupOk = false;
        if (!info.Ok && FileAccess.FileExists(BackupPathOf(slot)))
        {
            backup = CoreBridge.Instance.PeekSave(BackupPathOf(slot));
            backupOk = backup.Ok;
        }
        return new SlotInfo(slot, true, info, modified, backupOk, backup);
    }

    /// <summary>
    /// 이어하기: 가장 최근에 저장한 슬롯(자동 저장 포함). 파일 시각이 같으면(1초 단위) 더 나중 틱.
    /// 망가졌어도 예비 파일이 있으면 후보 (불러올 때 예비 파일을 제안). 없으면 -1.
    /// </summary>
    public static int Latest(out SlotInfo latest)
    {
        latest = default;
        int best = -1;
        for (int slot = AutosaveSlot; slot <= SlotCount; slot++)
        {
            SlotInfo info = Peek(slot);
            if (!info.Ok && !info.OnlyBackup)
            {
                continue;
            }
            int tick = info.Ok ? info.Info.Tick : info.Backup.Tick;
            int bestTick = latest.Ok ? latest.Info.Tick : latest.Backup.Tick;
            if (best < 0 || info.ModifiedUnix > latest.ModifiedUnix ||
                (info.ModifiedUnix == latest.ModifiedUnix && tick > bestTick))
            {
                best = slot;
                latest = info;
            }
        }
        return best;
    }

    public static CoreResult Save(int slot)
    {
        Error error = DirAccess.MakeDirRecursiveAbsolute(ProjectSettings.GlobalizePath(Directory));
        if (error != Error.Ok && error != Error.AlreadyExists)
        {
            return new CoreResult(CoreResultCode.FileIo);
        }
        return CoreBridge.Instance.Save(PathOf(slot));
    }

    /// <summary>슬롯을 불러옵니다 (backup 이면 예비 파일). 실패하면 지금 월드는 그대로.</summary>
    public static CoreResult Load(int slot, bool backup, out int errorLine) =>
        CoreBridge.Instance.Load(backup ? BackupPathOf(slot) : PathOf(slot), out errorLine);

    /// <summary>자동 검사용: 자동 검사 폴더의 세이브를 모두 지웁니다 (학생의 세이브 폴더는 절대 건드리지 않음)</summary>
    public static void ClearAutomatedSaves()
    {
        if (!LaunchOptions.Current.Automated)
        {
            return;
        }
        for (int slot = AutosaveSlot; slot <= SlotCount; slot++)
        {
            foreach (string path in new[] { PathOf(slot), BackupPathOf(slot), PathOf(slot) + ".tmp" })
            {
                if (FileAccess.FileExists(path))
                {
                    DirAccess.RemoveAbsolute(ProjectSettings.GlobalizePath(path));
                }
            }
        }
    }

    /// <summary>파일 시각(유닉스 초)을 이 컴퓨터의 현지 시각 문자열로</summary>
    public static string LocalTime(ulong unix)
    {
        if (unix == 0)
        {
            return "";
        }
        DateTime local = DateTimeOffset.FromUnixTimeSeconds((long)unix).LocalDateTime;
        return local.ToString("yyyy-MM-dd HH:mm", System.Globalization.CultureInfo.InvariantCulture);
    }
}
