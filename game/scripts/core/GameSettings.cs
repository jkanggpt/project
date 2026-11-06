// GameSettings.cs — 설정 (user://settings.cfg): 음량 셋, UI 크기, 전체 화면, 가장자리 스크롤
//
// 설정은 게임 상태가 아니므로 C 가 아니라 Godot 이 가집니다 (SPEC §3.2: Godot 이 저장하는 것은 설정 파일뿐).
// 자동 검사(스모크 테스트·스크린숏)는 학생의 설정 파일을 읽지도 쓰지도 않고 늘 기본값을 씁니다 → 결과가 늘 같음.
using System;
using Godot;
using MagicCastle.Debugging;

namespace MagicCastle.Core;

public sealed class GameSettings
{
    private const string Path = "user://settings.cfg";
    private const string Section = "settings";

    public static GameSettings Current { get; private set; } = new();

    /// <summary>0..1 (0 이면 소리 끔)</summary>
    public float MasterVolume { get; set; } = 0.8f;

    public float MusicVolume { get; set; } = 0.7f;

    /// <summary>효과음과 UI 소리 (SFX·UI 버스)</summary>
    public float SfxVolume { get; set; } = 0.8f;

    /// <summary>0 = 자동 (창 크기에 맞춤), 1~4 = 그 배율</summary>
    public int UiScale { get; set; }

    public bool Fullscreen { get; set; }

    public bool EdgeScroll { get; set; } = true;

    /// <summary>설정 파일을 읽습니다. 없거나 망가졌으면 기본값 (오류 없이)</summary>
    public static void Load()
    {
        var settings = new GameSettings();
        if (!LaunchOptions.Current.Automated)
        {
            var file = new ConfigFile();
            if (file.Load(Path) == Error.Ok)
            {
                settings.MasterVolume = ReadVolume(file, "master_volume", settings.MasterVolume);
                settings.MusicVolume = ReadVolume(file, "music_volume", settings.MusicVolume);
                settings.SfxVolume = ReadVolume(file, "sfx_volume", settings.SfxVolume);
                settings.UiScale = Math.Clamp(ReadInt(file, "ui_scale", 0), 0, 4);
                settings.Fullscreen = ReadInt(file, "fullscreen", 0) != 0;
                settings.EdgeScroll = ReadInt(file, "edge_scroll", 1) != 0;
            }
        }
        Current = settings;
    }

    /// <summary>설정 파일에 씁니다 (자동 검사 중에는 쓰지 않음)</summary>
    public void Save()
    {
        if (LaunchOptions.Current.Automated)
        {
            return;
        }
        var file = new ConfigFile();
        file.SetValue(Section, "master_volume", MasterVolume);
        file.SetValue(Section, "music_volume", MusicVolume);
        file.SetValue(Section, "sfx_volume", SfxVolume);
        file.SetValue(Section, "ui_scale", UiScale);
        file.SetValue(Section, "fullscreen", Fullscreen ? 1 : 0);
        file.SetValue(Section, "edge_scroll", EdgeScroll ? 1 : 0);
        Error error = file.Save(Path);
        if (error != Error.Ok)
        {
            GD.Print($"settings not saved: {error}"); // 설정을 못 써도 게임은 계속 (경고로 남기지 않음)
        }
    }

    /// <summary>음량을 오디오 버스에 바로 적용합니다</summary>
    public void ApplyAudio()
    {
        SetBus("Master", MasterVolume);
        SetBus("Music", MusicVolume);
        SetBus("SFX", SfxVolume);
        SetBus("UI", SfxVolume);
    }

    /// <summary>전체 화면 (화면이 없는 헤드리스 실행에서는 아무것도 하지 않음)</summary>
    public void ApplyWindow()
    {
        if (DisplayServer.GetName() == "headless" || LaunchOptions.Current.Automated)
        {
            return;
        }
        DisplayServer.WindowMode wanted = Fullscreen ? DisplayServer.WindowMode.Fullscreen : DisplayServer.WindowMode.Windowed;
        if (DisplayServer.WindowGetMode() != wanted)
        {
            DisplayServer.WindowSetMode(wanted);
        }
    }

    /// <summary>쓸 UI 배율: 설정이 자동이면 창 크기에 맞춘 값</summary>
    public int ResolveUiScale(Vector2I windowSize) => UiScale > 0 ? UiScale : UI.UiScale.DefaultFor(windowSize);

    private static void SetBus(string name, float volume)
    {
        int index = AudioServer.GetBusIndex(name);
        if (index < 0)
        {
            return;
        }
        AudioServer.SetBusMute(index, volume <= 0.001f);
        AudioServer.SetBusVolumeDb(index, Mathf.LinearToDb(Math.Max(volume, 0.001f)));
    }

    private static float ReadVolume(ConfigFile file, string key, float fallback)
    {
        Variant value = file.GetValue(Section, key, fallback);
        return value.VariantType is Variant.Type.Float or Variant.Type.Int ? Math.Clamp((float)value.AsDouble(), 0f, 1f) : fallback;
    }

    private static int ReadInt(ConfigFile file, string key, int fallback)
    {
        Variant value = file.GetValue(Section, key, fallback);
        return value.VariantType == Variant.Type.Int ? value.AsInt32() : fallback;
    }
}
