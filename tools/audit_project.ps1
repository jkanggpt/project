# 현재 소스·자산·패키지의 정본과 금지된 분기를 검사합니다.
param([string]$Output='build/reports/audit', [string]$Package='')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function File-Sha256([string]$FilePath) {
    $stream=[IO.File]::OpenRead([IO.Path]::GetFullPath($FilePath))
    $algorithm=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $algorithm.Dispose(); $stream.Dispose() }
}
Push-Location $projectRoot
try {
    $outputRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot $Output))
    if (-not $outputRoot.StartsWith($projectRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw '보고서는 프로젝트 내부에 생성해야 합니다.' }
    [IO.Directory]::CreateDirectory($outputRoot) | Out-Null
    $manifest=Get-Content -LiteralPath 'game/assets/assets_manifest.json' -Raw -Encoding UTF8 | ConvertFrom-Json
    $art=Get-Content -LiteralPath 'art/art_manifest.json' -Raw -Encoding UTF8 | ConvertFrom-Json
    $expectedAssets=@{}
    $expectedInputs=@{}
    $errors=[Collections.Generic.List[string]]::new()
    foreach($a in $manifest.assets) {
        $p='game/assets/'+$a.path
        if ($expectedAssets.ContainsKey($p)) { $errors.Add('중복 자산: '+$p) }
        $expectedAssets[$p]=$a
        if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { $errors.Add('자산 누락: '+$p); continue }
        if ((File-Sha256 $p) -ne $a.sha256) { $errors.Add('자산 SHA 불일치: '+$p) }
    }
    foreach($a in $art.source_files) {
        $expectedInputs[$a.path]=$a
        if (-not (Test-Path -LiteralPath $a.path -PathType Leaf)) { $errors.Add('원본 누락: '+$a.path); continue }
        if ((File-Sha256 $a.path) -ne $a.sha256) { $errors.Add('원본 SHA 불일치: '+$a.path) }
    }
    # 시작 원장에 기록한 대체 정본도 실제로 존재해야 합니다.
    $ledger=Get-Content -LiteralPath 'docs/audit/files.json' -Raw -Encoding UTF8 | ConvertFrom-Json
    $checkedTargets=@{}
    $externalInputs=[Collections.Generic.List[string]]::new()
    foreach($record in $ledger.records) {
        if ($record.kind -eq 'user_input') {
            $externalInputs.Add($record.current_path)
            if ((Test-Path -LiteralPath $record.current_path -PathType Leaf) -and (File-Sha256 $record.current_path) -ne $record.sha256) { $errors.Add('사용자 원본 SHA 불일치: '+$record.current_path) }
            continue
        }
        if (-not $record.target_path) { continue }
        foreach($target in $record.target_path.Split(',')) {
            if ($checkedTargets.ContainsKey($target)) { continue }
            $checkedTargets[$target]=$true
            if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { $errors.Add('이관 정본 누락: '+$target) }
        }
    }
    # Git 무시 규칙과 무관하게 소스 영역을 조사하고 재생성 출력·사용자 자료는 구별합니다.
    $files=@(& rg --files --hidden --no-ignore-vcs -g '!.git/**' -g '!build/**' -g '!artifacts/**' -g '!out/**' -g '!.vs/**' -g '!.vscode/**' -g '!node_modules/**')
    if ($LASTEXITCODE -ne 0) { throw 'rg 파일 조사 실패' }
    $files=@($files | ForEach-Object { $_.Replace('\','/') } | Sort-Object -Unique)
    $records=[Collections.Generic.List[object]]::new()
    $historical='v[0-9]{3,}|patch[0-9]+|saves_v[0-9]+|legacy'
    foreach($p in $files) {
        $item=Get-Item -LiteralPath $p
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { $errors.Add('외부 링크 확인 필요: '+$p); continue }
        $kind='maintained'; $producer='사람이 관리하는 정본'; $consumers=@('프로젝트 관리'); $dynamic=$false
        if ($p -in @('AGENTS.md','docs.zip','tools.zip')) { $kind='user_input'; $producer='사용자가 제공한 원본'; $consumers=@('작업 지침·참고 자료: 소스 설치 제외') }
        elseif ($p -eq 'game/assets/assets_manifest.json') { $kind='asset_registry'; $consumers=@('cmake/Assets.cmake','castle_asset_index','castle_assets_check','Runtime 설치') }
        elseif ($p.StartsWith('.github/')) { $kind='ci'; $consumers=@('GitHub Actions') }
        elseif ($expectedAssets.ContainsKey($p)) { $kind=$expectedAssets[$p].kind; $producer='castle_art_pipeline'; $consumers=@('assets_manifest','game/src/assets/loader.c','Runtime 설치'); $dynamic=$true }
        elseif ($expectedInputs.ContainsKey($p)) { $kind='art_input'; $producer=$expectedInputs[$p].producer; $consumers=@('castle_art_pipeline') }
        elseif ($p.StartsWith('game/assets/') -and $p -ne 'game/assets/assets_manifest.json') { $errors.Add('미등록 런타임 파일: '+$p) }
        elseif ($p.StartsWith('art/sources/')) { $errors.Add('미등록 제작 입력: '+$p) }
        elseif ($p.StartsWith('art/')) { $kind='art_metadata'; $consumers=@('tools/audit_project.ps1','제작·권리 추적') }
        elseif ($p.StartsWith('core/')) { $kind='core'; $consumers=@('castle_core_static','castle_core','castle_core_testing') }
        elseif ($p.StartsWith('game/src/')) { $kind='frontend'; $consumers=@('castle_frontend','castle_game') }
        elseif ($p.StartsWith('cli/')) { $kind='console'; $consumers=@('castle_cli','castle_cli_support') }
        elseif ($p.StartsWith('tests/')) { $kind='test'; $consumers=@('CTest') }
        elseif ($p.StartsWith('tools/')) { $kind='tool'; $consumers=@('tools/CMakeLists.txt','개발 검증') }
        elseif ($p.StartsWith('game/data/')) { $kind='definition'; $consumers=@('core_defs_load_file','game_session_init','castle_cli','Runtime 설치') }
        elseif ($p.StartsWith('docs/') -or $p.EndsWith('.md')) { $kind='document'; $consumers=@('개발자·플레이어') }
        elseif ($p.StartsWith('cmake/') -or $p.Contains('CMake')) { $kind='build'; $consumers=@('CMake') }
        $isEvidence=($p -in @('docs/audit/files.json','docs/audit/symbols.json','docs/audit/tests.json')) -or $p -eq 'docs/CLEANUP_AUDIT.md' -or $p -eq 'art/recipes/provenance.json' -or $p -eq 'tools/audit_project.ps1'
        if (-not $isEvidence -and $p -match $historical) { $errors.Add('역사적 기능 경로: '+$p) }
        if (-not $isEvidence -and $p -match '\.(c|h|cmake|ps1|json|txt)$') {
            $text=Get-Content -LiteralPath $p -Raw -Encoding UTF8
            if ($text -match $historical) { $errors.Add('역사적 기능 식별자: '+$p) }
            if ($p -match '\.(c|h)$' -and ($text -match '#\s*include\s*[<"][^>"\r\n]+\.c[>"]' -or $text -match '#\s*define\s+main\b')) { $errors.Add('소스 포함·main 치환: '+$p) }
        }
        if ($p.EndsWith('.c')) { $kind='c_source' } elseif ($p.EndsWith('.h')) { $kind='c_header' }
        $records.Add([ordered]@{current_path=$p;kind=$kind;size_bytes=$item.Length;sha256=(File-Sha256 $p);producer=$producer;consumers=$consumers;dynamic_reference=$dynamic;status='KEEP';target_path=$p;target_symbol=$null;reason='현재 역할별 정본';evidence=@('CLEANUP_AUDIT.md');build_verified=$false;runtime_verified=$false;package_verified=($Package -ne '');delete_approved=$false})
    }
    if (Test-Path -LiteralPath 'game/src/castle_core.h') { $errors.Add('공개 헤더 복사본') }
    if (Test-Path -LiteralPath 'game/src/castle_expansion.h') { $errors.Add('확장 헤더 복사본') }
    if ($Package) {
        $packageRoot=[IO.Path]::GetFullPath($Package)
        $expected=@('castle_game.exe','CREDITS.md','README.md','licenses/raylib.txt','licenses/GLFW.txt','assets/assets_manifest.json')
        $expected+=@($manifest.assets | ForEach-Object {'assets/'+$_.path})
        $expected+=@('buildings.def','traits.def','names.txt','enemies.def','research.def','ranks.def' | ForEach-Object {'data/'+$_})
        $actual=@(Get-ChildItem -LiteralPath $packageRoot -Recurse -File | ForEach-Object {$_.FullName.Substring($packageRoot.Length+1).Replace('\','/')})
        foreach($d in (Compare-Object -ReferenceObject ($expected|Sort-Object) -DifferenceObject ($actual|Sort-Object))) { $errors.Add('패키지 파일 목록 차이: '+$d.InputObject+' '+$d.SideIndicator) }
        foreach($a in $manifest.assets) { $p=Join-Path $packageRoot ('assets/'+$a.path); if ((Test-Path -LiteralPath $p) -and (File-Sha256 $p) -ne $a.sha256) { $errors.Add('설치 자산 SHA 불일치: '+$a.path) } }
    }
    $records | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'current_inventory.json') -Encoding UTF8
    [ordered]@{files=$records.Count;runtime_assets=$manifest.assets.Count;art_inputs=$art.source_files.Count;ledger_targets=$checkedTargets.Count;external_inputs=@($externalInputs.ToArray());errors=@($errors.ToArray());package=$Package} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'result.json') -Encoding UTF8
    Write-Host ('정본 조사: '+$records.Count+'파일, 오류 '+$errors.Count)
    if ($errors.Count) { $errors | ForEach-Object {Write-Host $_}; exit 1 }
} finally { Pop-Location }
