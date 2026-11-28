# Windows 11 노트북의 실제 그래픽 카드와 GPU 단계별 지연을 기록합니다.
param(
    [string]$Executable = (Join-Path $PSScriptRoot 'castle_gpu_lab.exe'),
    [ValidateSet('natural-slice','reference')][string]$Scene = 'natural-slice',
    [ValidateRange(1,6)][int]$PixelScale = 3,
    [ValidateRange(80, 1000000)][int]$Frames = 600,
    [ValidateRange(4, 10000)][int]$Warmup = 60,
    [ValidateRange(0, 120)][int]$SoakMinutes = 0,
    [switch]$Extended
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Executable)) { throw 'castle_gpu_lab.exe 경로를 확인하세요.' }
if ($Frames -lt $Warmup + 20) { throw '준비 구간 뒤에 최소 20프레임이 필요합니다.' }
$Executable = [IO.Path]::GetFullPath($Executable)
$resultRoot = Join-Path (Split-Path $Executable) ('benchmark-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $resultRoot | Out-Null

function Invoke-Probe([string]$Name, [string[]]$ProbeArguments) {
    $json = Join-Path $resultRoot ($Name + '.json')
    $log = Join-Path $resultRoot ($Name + '.log')
    & $Executable @ProbeArguments --report $json 2>&1 | Out-File -LiteralPath $log -Encoding utf8
    if ($LASTEXITCODE -ne 0) { throw ('GPU 검사 실패: ' + $log) }
    $data = Get-Content -LiteralPath $json -Raw -Encoding utf8 | ConvertFrom-Json
    if ($data.software) { throw ('하드웨어 성능 검사가 아닙니다. 렌더러: ' + $data.renderer) }
    if ($data.renderer -notmatch 'RTX 4060') {
        throw ('RTX 4060이 선택되지 않았습니다: ' + $data.renderer +
               '. Windows 설정의 디스플레이 > 그래픽에서 이 실행 파일을 고성능으로 지정하세요.')
    }
    if ($data.gpu_total_ms.samples -lt 20) { throw 'GPU 시간 표본이 부족합니다.' }
    Write-Host ($Name + ': ' + $data.renderer + ', GPU p95 ' + $data.gpu_total_ms.p95 + ' ms')
    return $data
}

$summaries = @()
$sceneArguments = @('--scene',$Scene)
if ($Scene -eq 'natural-slice') { $sceneArguments += @('--pixel-scale',"$PixelScale") }
$sizes = @(@(800,600), @(1024,768), @(1440,900), @(1920,1080))
$scenes = @(
    @{name='small-day'; args=@('--lights','4')},
    @{name='dense-day'; args=@('--dense','--lights','4')},
    @{name='dense-night'; args=@('--dense','--night','--lights','4')}
)
if ($Extended) {
    $scenes += @{name='dense-night-eight'; args=@('--dense','--night','--lights','8')}
    $scenes += @{name='dense-night-sixteen'; args=@('--dense','--night','--lights','16')}
}
foreach ($size in $sizes) {
    foreach ($caseSpec in $scenes) {
        $name = '{0}-{1}x{2}-{3}' -f $Scene,$size[0],$size[1],$caseSpec.name
        $common = @('--hidden','--benchmark','--frames',"$Frames",'--warmup',"$Warmup",
                    '--width',"$($size[0])",'--height',"$($size[1])") + $sceneArguments + $caseSpec.args
        $baseline = Invoke-Probe ($name + '-baseline-before') ($common + @('--no-motion'))
        $animated = Invoke-Probe ($name + '-animated') $common
        $after = Invoke-Probe ($name + '-baseline-after') ($common + @('--no-motion'))
        $summaries += [ordered]@{
            case=$name; renderer=$animated.renderer; hardware=$true
            measurement_family=$animated.measurement_family
            scene=$animated.scene; pixel_scale=$animated.pixel_scale
            logical_width=$animated.logical_width; logical_height=$animated.logical_height
            baseline_gpu_p95_ms=$baseline.gpu_total_ms.p95
            baseline_after_gpu_p95_ms=$after.gpu_total_ms.p95
            baseline_drift_ms=[math]::Round($after.gpu_total_ms.p95-$baseline.gpu_total_ms.p95,4)
            animated_gpu_p95_ms=$animated.gpu_total_ms.p95
            estimated_added_gpu_p95_ms=[math]::Round($animated.gpu_total_ms.p95-($baseline.gpu_total_ms.p95+$after.gpu_total_ms.p95)/2,4)
            animated_wall_p95_ms=$animated.wall_ms.p95
            grass=$animated.grass_instances; smoke=$animated.smoke_particles
            tile_overflows=$animated.tile_overflows
            texture_bytes=$animated.texture_bytes; buffer_bytes=$animated.buffer_bytes
        }
    }
}
if ($SoakMinutes -gt 0) {
    $soakArguments = @('--hidden','--benchmark','--dense','--night','--lights','4',
              '--width','1920','--height','1080','--seconds',"$($SoakMinutes*60)",'--warmup',"$Warmup") + $sceneArguments
    $soak = Invoke-Probe 'dense-soak' $soakArguments
    if ($soak.elapsed_seconds -lt $SoakMinutes*60) { throw '요청한 장시간 검사 길이에 도달하지 못했습니다.' }
    $summaries += [ordered]@{case='dense-soak'; renderer=$soak.renderer; frames=$soak.frames;
        texture_bytes=$soak.texture_bytes; buffer_bytes=$soak.buffer_bytes; gpu_p95_ms=$soak.gpu_total_ms.p95}
}
[ordered]@{
    target='Windows 11 / mobile RTX 4060'
    measured_at=(Get-Date -Format o)
    note='기준·효과·기준의 세 별도 실행으로 변동도 기록합니다. p95 차이는 효과 비용의 근사이며 같은 프로세스의 차분이나 전체 게임 판정이 아닙니다. 새 논리 해상도 결과를 이전 화면 맞춤 측정과 직접 비교하지 않습니다.'
    cases=$summaries
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $resultRoot 'summary.json') -Encoding utf8
Write-Host ('완료. 결과 폴더: ' + $resultRoot)
