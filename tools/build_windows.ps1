# 한글 경로를 지원하지 않는 MinGW/Ninja에도 같은 C 빌드를 제공합니다.
param([switch]$NoTests, [switch]$CoreOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$mappedDrive = $null
try {
    # 기존 드라이브는 건드리지 않고 비어 있는 문자만 잠시 사용합니다.
    foreach ($letter in @('R','S','T','U','V','W','X','Y','Z')) {
        if (-not (Test-Path -LiteralPath ($letter + ':\'))) {
            & subst ($letter + ':') $projectRoot
            if ($LASTEXITCODE -eq 0) { $mappedDrive = $letter; break }
        }
    }
    if (-not $mappedDrive) { throw '임시 빌드 드라이브 문자가 없습니다. Visual Studio 프리셋을 사용하세요.' }
    $sourcePath = $mappedDrive + ':/'
    $buildPath = $sourcePath + 'build/windows-native-' + $mappedDrive
    $gameOption = if ($CoreOnly) { 'OFF' } else { 'ON' }
    & cmake -S $sourcePath -B $buildPath -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASTLE_WERROR=ON "-DCASTLE_BUILD_GAME=$gameOption"
    if ($LASTEXITCODE -ne 0) { throw 'CMake 구성 실패' }
    & cmake --build $buildPath --parallel
    if ($LASTEXITCODE -ne 0) { throw 'C 빌드 실패' }
    if (-not $NoTests) {
        & ctest --test-dir $buildPath --output-on-failure --parallel 8
        if ($LASTEXITCODE -ne 0) { throw '테스트 실패' }
    }
    Write-Host ('빌드 완료: ' + (Join-Path $projectRoot ('build/windows-native-' + $mappedDrive + '/bin')))
} finally {
    if ($mappedDrive) { & subst ($mappedDrive + ':') /D }
}
