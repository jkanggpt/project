// ErrorCounter.cs — 엔진의 오류·경고를 세는 로거 (스모크 테스트와 스크린숏 모드에서만 붙음)
// "시작할 때와 플레이 중에 Output 패널에 오류나 경고가 하나도 없어야 한다"(SPEC §8)를 자동으로 확인합니다.
using System.Collections.Generic;
using System.Threading;
using Godot;

namespace MagicCastle.Debugging;

public partial class ErrorCounter : Logger
{
    private static ErrorCounter? _instance;
    private static int _errors;
    private static readonly List<string> FirstMessages = new();
    private static readonly object Lock = new();

    public static int Count => Volatile.Read(ref _errors);

    public static void Install()
    {
        if (_instance != null)
        {
            return;
        }
        _instance = new ErrorCounter();
        OS.AddLogger(_instance);
    }

    public static string Summary()
    {
        lock (Lock)
        {
            return string.Join("\n", FirstMessages);
        }
    }

    // 로거는 다른 스레드에서 불릴 수 있으므로 공유 값은 잠금이나 Interlocked 로만 바꿉니다
    public override void _LogError(string function, string file, int line, string code, string rationale,
        bool editorNotify, int errorType, Godot.Collections.Array<ScriptBacktrace> scriptBacktraces)
    {
        Interlocked.Increment(ref _errors);
        lock (Lock)
        {
            if (FirstMessages.Count < 10)
            {
                string text = rationale.Length > 0 ? rationale : code;
                FirstMessages.Add($"{(errorType == 1 ? "WARNING" : "ERROR")}: {text} ({file}:{line} {function})");
            }
        }
    }

    public override void _LogMessage(string message, bool error)
    {
        if (!error)
        {
            return;
        }
        Interlocked.Increment(ref _errors);
        lock (Lock)
        {
            if (FirstMessages.Count < 10)
            {
                FirstMessages.Add($"STDERR: {message.TrimEnd()}");
            }
        }
    }
}
