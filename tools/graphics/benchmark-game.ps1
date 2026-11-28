# 실제 게임 조명 검수 장면(밤 240명 군중·낮)을 네 해상도에서 측정하고 RTX 4060 결과만 받아들입니다.
param(
    [string]$Executable = (Join-Path $PSScriptRoot 'castle_game.exe'),
    [ValidateRange(120, 100000)][int]$Frames = 600,
    [switch]$AllowOtherGpu
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Executable)) { throw 'castle_game.exe 경로를 확인하세요.' }
$Executable = [IO.Path]::GetFullPath($Executable)
$resultRoot = Join-Path (Split-Path $Executable) ('game-benchmark-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $resultRoot | Out-Null
$sizes = @(@(800,600), @(1024,768), @(1440,900), @(1920,1080))
$phases = @(@{name='night'; phase=0}, @{name='day'; phase=2})
$results = @()
foreach ($size in $sizes) {
    foreach ($p in $phases) {
        $name = '{0}x{1}-{2}' -f $size[0], $size[1], $p.name
        $log = Join-Path $resultRoot ($name + '.log')
        & $Executable --review-benchmark $Frames --review-phase $p.phase --no-audio --width $size[0] --height $size[1] 2>&1 |
            Out-File -LiteralPath $log -Encoding utf8
        if ($LASTEXITCODE -ne 0) { throw ('측정 실패: ' + $log) }
        $line = Select-String -LiteralPath $log -Pattern '^REVIEW BENCHMARK: (.*)$' | Select-Object -Last 1
        if (-not $line) { throw ('결과 줄이 없습니다: ' + $log) }
        $data = $line.Matches[0].Groups[1].Value | ConvertFrom-Json
        if ($data.software) { throw ('소프트웨어 렌더러입니다. 하드웨어 측정이 아닙니다: ' + $data.renderer) }
        if (-not $AllowOtherGpu -and $data.renderer -notmatch 'RTX 4060') {
            throw ('RTX 4060이 선택되지 않았습니다: ' + $data.renderer +
                   '. Windows 설정의 디스플레이 > 그래픽에서 castle_game.exe를 고성능으로 지정하세요.')
        }
        if ($data.gpu_samples -lt 20) { throw ('GPU 시간 표본이 부족합니다: ' + $name) }
        $data | Add-Member -NotePropertyName name -NotePropertyValue $name
        Write-Host ('{0}: 프레임 p95 {1} ms, GPU p95 {2} ms, 광원 {3}' -f $name, $data.frame_p95_ms, $data.gpu_p95_ms, $data.lights)
        $results += $data
    }
}
$results | ConvertTo-Json -Depth 4 | Out-File -LiteralPath (Join-Path $resultRoot 'summary.json') -Encoding utf8
Write-Host ('결과: ' + $resultRoot)
