// LaunchOptions.cs — 명령줄 옵션 (godot --path game -- <옵션들>)
//   --smoke-test [--days N]    화면 없이 N일 진행하며 오류를 검사하고 종료 (실패하면 종료 코드 1)
//   --screenshots <폴더>       주요 장면을 PNG 로 저장하고 종료 (Xvfb 에서 사용)
//   --seed N                   지도 시드 고정
using System;
using System.Globalization;

namespace MagicCastle.Debugging;

public sealed class LaunchOptions
{
    public static LaunchOptions Current { get; private set; } = new();

    public bool SmokeTest { get; private set; }
    public int SmokeDays { get; private set; } = 20;
    public string? ScreenshotDir { get; private set; }
    public int? Seed { get; private set; }

    /// <summary>오류 기록기를 붙여야 하는 모드인지</summary>
    public bool CountErrors => SmokeTest || ScreenshotDir != null;

    /// <summary>사람 대신 프로그램이 조종하는 모드인지 (가장자리 스크롤 등을 끔)</summary>
    public bool Automated => SmokeTest || ScreenshotDir != null;

    public static void Parse(string[] args)
    {
        var options = new LaunchOptions();
        for (int i = 0; i < args.Length; i++)
        {
            string next = i + 1 < args.Length ? args[i + 1] : "";
            switch (args[i])
            {
                case "--smoke-test":
                    options.SmokeTest = true;
                    break;
                case "--days" when int.TryParse(next, NumberStyles.Integer, CultureInfo.InvariantCulture, out int days):
                    options.SmokeDays = Math.Clamp(days, 1, 1000);
                    i++;
                    break;
                case "--screenshots" when next.Length > 0:
                    options.ScreenshotDir = next;
                    i++;
                    break;
                case "--seed" when int.TryParse(next, NumberStyles.Integer, CultureInfo.InvariantCulture, out int seed) && seed >= 0:
                    options.Seed = seed;
                    i++;
                    break;
            }
        }
        Current = options;
    }
}
