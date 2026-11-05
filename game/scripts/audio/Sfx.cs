// Sfx.cs — 효과음 (자동 로드가 아니라 Main 의 자식)
//
// 버스: Master, Music, SFX, UI (game/default_bus_layout.tres). 볼륨 설정 화면은 마일스톤 6 입니다.
// 같은 소리가 한꺼번에 수십 번 나지 않게 소리마다 최소 간격을 두고, 재생기는 몇 개만 만들어 돌려 씁니다.
// 소리 파일은 Kenney 의 CC0 팩(assets/third_party/kenney/audio-*)에서 고릅니다. 파일이 없으면 조용히 건너뜁니다.
using System.Collections.Generic;
using Godot;

namespace MagicCastle.Audio;

public enum Sound
{
    Alarm,       // 습격 경보 (종)
    ArrowShot,   // 감시탑이 쏨
    ArrowHit,    // 화살이 맞음
    Strike,      // 적·경비병의 근접 공격
    WallHit,     // 성벽·건물을 때림
    Collapse,    // 건물이 무너짐
    EnemyDown,   // 적이 쓰러짐
    Loot,        // 금화를 빼앗김
    Victory,     // 습격이 끝남
}

public partial class Sfx : Node
{
    private const int PoolSize = 10;
    private const string Impact = "res://assets/third_party/kenney/audio-impact/Audio/";
    private const string Rpg = "res://assets/third_party/kenney/audio-rpg/Audio/";
    private const string Interface = "res://assets/third_party/kenney/audio-interface/Audio/";

    // 소리마다: 파일 후보들(번갈아 씀), 볼륨(dB), 최소 간격(초)
    private static readonly (string[] Files, float VolumeDb, double MinGap)[] Table =
    {
        (new[] { Impact + "impactBell_heavy_000.ogg", Impact + "impactBell_heavy_001.ogg" }, -4f, 1.5),
        (new[] { Rpg + "drawKnife1.ogg", Rpg + "drawKnife2.ogg", Rpg + "drawKnife3.ogg" }, -18f, 0.08),
        (new[] { Impact + "impactSoft_medium_000.ogg", Impact + "impactSoft_medium_001.ogg", Impact + "impactSoft_medium_002.ogg" }, -14f, 0.06),
        (new[] { Rpg + "knifeSlice.ogg", Rpg + "knifeSlice2.ogg" }, -14f, 0.1),
        (new[] { Impact + "impactWood_medium_000.ogg", Impact + "impactWood_medium_001.ogg", Impact + "impactWood_medium_002.ogg" }, -12f, 0.12),
        (new[] { Impact + "impactPlank_medium_000.ogg", Impact + "impactWood_heavy_000.ogg" }, -4f, 0.3),
        (new[] { Impact + "impactPunch_heavy_000.ogg", Impact + "impactPunch_heavy_001.ogg" }, -10f, 0.1),
        (new[] { Rpg + "handleCoins.ogg", Rpg + "handleCoins2.ogg" }, -6f, 0.3),
        (new[] { Interface + "confirmation_002.ogg" }, -6f, 1.0),
    };

    private readonly List<AudioStreamPlayer> _players = new();
    private readonly Dictionary<string, AudioStream?> _streams = new();
    private readonly double[] _lastPlayed = new double[Table.Length];
    private readonly int[] _nextFile = new int[Table.Length];
    private int _nextPlayer;

    public static Sfx? Instance { get; private set; }

    public override void _EnterTree()
    {
        Instance = this;
    }

    public override void _ExitTree()
    {
        if (Instance == this)
        {
            Instance = null;
        }
        // 기억해 둔 소리를 직접 놓아 줍니다. C# 쪽 참조가 GC 를 기다리면 종료 때 "자원이 아직 쓰이는 중" 경고가 납니다
        foreach (AudioStreamPlayer player in _players)
        {
            player.Stop();
            player.Stream = null;
        }
        foreach (AudioStream? stream in _streams.Values)
        {
            stream?.Dispose();
        }
        _streams.Clear();
    }

    public override void _Ready()
    {
        ProcessMode = ProcessModeEnum.Always;
        StringName bus = AudioServer.GetBusIndex("SFX") >= 0 ? new StringName("SFX") : new StringName("Master");
        for (int i = 0; i < PoolSize; i++)
        {
            var player = new AudioStreamPlayer { Bus = bus };
            AddChild(player);
            _players.Add(player);
        }
        for (int i = 0; i < _lastPlayed.Length; i++)
        {
            _lastPlayed[i] = -1000;
        }
    }

    /// <summary>모든 소리를 멈춥니다. 오디오 서버는 멈춘 재생을 조금 뒤에 놓아 주므로,
    /// 끝내기 직전이라면 한두 프레임 기다린 뒤에 끝내야 종료 때 "자원이 아직 쓰이는 중" 경고가 나지 않습니다.</summary>
    public void StopAll()
    {
        foreach (AudioStreamPlayer player in _players)
        {
            player.Stop();
        }
    }

    /// <summary>소리를 모두 멈추고 잠깐 기다렸다가 게임을 끝냅니다 (자동 검사, 스크린숏)</summary>
    public static void QuitQuietly(SceneTree tree, int exitCode)
    {
        Instance?.StopAll();
        SceneTreeTimer timer = tree.CreateTimer(0.2, processAlways: true);
        timer.Timeout += () => tree.Quit(exitCode);
    }

    public void Play(Sound sound)
    {
        int index = (int)sound;
        if (index < 0 || index >= Table.Length || _players.Count == 0)
        {
            return;
        }
        double now = Time.GetTicksMsec() / 1000.0;
        if (now - _lastPlayed[index] < Table[index].MinGap)
        {
            return;
        }
        string[] files = Table[index].Files;
        string path = files[_nextFile[index] % files.Length];
        _nextFile[index]++;
        AudioStream? stream = Load(path);
        if (stream == null)
        {
            return;
        }
        _lastPlayed[index] = now;
        AudioStreamPlayer player = _players[_nextPlayer];
        _nextPlayer = (_nextPlayer + 1) % _players.Count;
        player.Stream = stream;
        player.VolumeDb = Table[index].VolumeDb;
        player.PitchScale = (float)GD.RandRange(0.92, 1.08);
        player.Play();
    }

    private AudioStream? Load(string path)
    {
        if (!_streams.TryGetValue(path, out AudioStream? stream))
        {
            stream = ResourceLoader.Exists(path) ? GD.Load<AudioStream>(path) : null;
            if (stream == null)
            {
                GD.Print($"sound missing: {path}");
            }
            _streams[path] = stream;
        }
        return stream;
    }
}
