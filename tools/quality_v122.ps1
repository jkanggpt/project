# v1.2.2 장기 검사. 완료한 시드를 보존하며 중단 후 같은 명령으로 재개합니다.
param([int]$Days=200,[int]$Seeds=48,[int]$Parallel=6,[string]$Output='artifacts/v122/quality_final_v2')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outputRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot $Output))
if(-not $outputRoot.StartsWith($projectRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Output must stay in project'}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$source=Join-Path $projectRoot 'build/msvc-check/bin/Release/castle_expansion_balance.exe'
$binary=Join-Path $outputRoot 'castle_expansion_balance.exe'
$sha=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$manifestPath=Join-Path $outputRoot 'manifest.json'
if(Test-Path -LiteralPath $manifestPath) {
    $prior=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if($prior.sha256 -ne $sha -or $prior.days -ne $Days -or $prior.seeds -ne $Seeds){throw 'Different binary or options: use a new output folder'}
    $parent=Get-CimInstance Win32_Process -Filter ('ProcessId='+$prior.parent_pid) -ErrorAction SilentlyContinue
    if($parent -and $parent.CreationDate.ToUniversalTime().ToString('o') -eq $prior.parent_created){throw 'This evaluation is already running'}
    # 이전 도구 세션 종료 후 남은 자체 검사만 정리합니다.
    Get-CimInstance Win32_Process | Where-Object {$_.ExecutablePath -eq $binary -and $_.CommandLine.Contains($outputRoot)} | ForEach-Object {Stop-Process -Id $_.ProcessId -ErrorAction SilentlyContinue}
} else {Copy-Item -LiteralPath $source -Destination $binary}
$parent=Get-CimInstance Win32_Process -Filter ('ProcessId='+$PID)
[PSCustomObject]@{days=$Days;seeds=$Seeds;sha256=$sha;parent_pid=$PID;parent_created=$parent.CreationDate.ToUniversalTime().ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath $manifestPath -Encoding utf8
$data=Join-Path $projectRoot 'game/data'
$pending=[Collections.Generic.Queue[object]]::new()
$done=0;$failures=0;$running=@();$results=@()
for($difficulty=0;$difficulty -lt 3;$difficulty++){for($seed=1;$seed -le $Seeds;$seed++) {
    $stdout=Join-Path $outputRoot ('d{0}_seed{1}.summary.log' -f $difficulty,$seed)
    $stderr=Join-Path $outputRoot ('d{0}_seed{1}.errors.log' -f $difficulty,$seed)
    $ok=(Test-Path -LiteralPath $stdout) -and ((Get-Content -LiteralPath $stdout -Raw) -match 'errors=0 save_skipped=0 hash_match=1')
    if($ok){$done++}else{$pending.Enqueue([PSCustomObject]@{difficulty=$difficulty;seed=$seed;stdout=$stdout;stderr=$stderr})}
}}
Write-Output ('resuming completed={0}/{1}' -f $done,($Seeds*3))
while($pending.Count -or $running.Count) {
    while($pending.Count -and $running.Count -lt $Parallel) {
        $job=$pending.Dequeue()
        $arguments=@('--days',$Days,'--seeds',1,'--seed',$job.seed,'--difficulty',$job.difficulty,'--data',('"'+$data+'"'),'--out',('"'+$outputRoot+'"'))
        $process=Start-Process -FilePath $binary -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $job.stdout -RedirectStandardError $job.stderr
        $running+=[PSCustomObject]@{job=$job;process=$process}
        $running | ForEach-Object {[PSCustomObject]@{pid=$_.process.Id;difficulty=$_.job.difficulty;seed=$_.job.seed}} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputRoot 'running.json') -Encoding utf8
    }
    $active=@()
    foreach($item in $running) {
        if($item.process.HasExited) {
            $item.process.WaitForExit();$exitCode=$item.process.ExitCode;$done++
            if($exitCode -ne 0){$failures++}
            $results+=[PSCustomObject]@{difficulty=$item.job.difficulty;seed=$item.job.seed;exit_code=$exitCode}
            Write-Output ('completed={0}/{1} difficulty={2} seed={3} exit={4}' -f $done,($Seeds*3),$item.job.difficulty,$item.job.seed,$exitCode)
        } else {$active+=$item}
    }
    $running=$active
    if($running.Count){Start-Sleep -Milliseconds 500}
}
[PSCustomObject]@{completed=$done;failures=$failures;sha256=$sha;results=$results} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputRoot 'completed.json') -Encoding utf8
if($failures){exit 1}
