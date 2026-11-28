# 저장소에서 실행하는 MSVC 빌드·전체 검사입니다. GPU가 있는 Windows에서 실행합니다.
param([string]$Repository = (Join-Path $PSScriptRoot '../..'))
$ErrorActionPreference = 'Stop'
Push-Location ([IO.Path]::GetFullPath($Repository))
try {
    & cmake --preset windows-msvc -DCASTLE_WERROR=ON -DCASTLE_GPU_TESTS=ON
    if ($LASTEXITCODE -ne 0) { throw 'MSVC 구성 실패' }
    & cmake --build --preset windows-msvc
    if ($LASTEXITCODE -ne 0) { throw 'MSVC 빌드 실패' }
    & ctest --preset windows-msvc --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'MSVC 전체 검사 실패' }
} finally {
    Pop-Location
}
