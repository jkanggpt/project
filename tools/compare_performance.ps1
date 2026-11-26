# 같은 하드웨어에서 두 실행 파일을 번갈아 단독 계측합니다.
param([Parameter(Mandatory=$true)][string]$Before,[Parameter(Mandatory=$true)][string]$After,[string]$Output='build/reports/performance',[int]$Runs=3,[int]$SettleSeconds=5)
$ErrorActionPreference='Stop'
$beforeExe=[IO.Path]::GetFullPath($Before)
$afterExe=[IO.Path]::GetFullPath($After)
$outputRoot=[IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory($outputRoot)|Out-Null
$results=[Collections.Generic.List[object]]::new()
foreach($round in 1..$Runs) {
    $order=if($round%2){@('before','after')}else{@('after','before')}
    foreach($kind in $order) {
        if($SettleSeconds -gt 0){Start-Sleep -Seconds ([Math]::Min($SettleSeconds,30))}
        $exe=if($kind -eq 'before'){$beforeExe}else{$afterExe}
        $prefix=Join-Path $outputRoot ($kind+'_'+$round)
        $arguments='--stress-probe --no-audio --width 1440 --height 900 --save-dir "'+$prefix+'_saves"'
        $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -PassThru -RedirectStandardOutput ($prefix+'.log') -RedirectStandardError ($prefix+'.err')
        $null=$process.Handle
        $peakWorking=0L; $peakPrivate=0L
        while(-not $process.HasExited) {
            $process.Refresh()
            $peakWorking=[Math]::Max($peakWorking,$process.PeakWorkingSet64)
            $peakPrivate=[Math]::Max($peakPrivate,$process.PrivateMemorySize64)
            $process.WaitForExit(100)|Out-Null
        }
        $process.WaitForExit()
        $line=Get-Content -LiteralPath ($prefix+'.log') -Encoding UTF8 | Where-Object {$_ -like 'STRESS:*'} | Select-Object -Last 1
        if(-not $line){throw ($kind+' STRESS 계측 누락: 종료 '+$process.ExitCode)}
        # 예산 미달은 결과로 보존합니다. 충돌·조건 미충족은 유효 표본이 아닙니다.
        if($process.ExitCode -notin @(0,1) -or $line -notmatch 'conditions=1' -or $line -notmatch 'frames=600'){throw ($kind+' 계측 조건 실패: '+$line)}
        $metrics=[ordered]@{round=$round;kind=$kind;settle_seconds=$SettleSeconds;exit_code=$process.ExitCode;peak_working_bytes=$peakWorking;peak_private_bytes=$peakPrivate;executable_sha256=(Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant()}
        foreach($match in [regex]::Matches($line,'(?<key>[a-zA-Z0-9_]+)=(?<value>[0-9.]+)')){$metrics[$match.Groups['key'].Value]=[double]::Parse($match.Groups['value'].Value,[Globalization.CultureInfo]::InvariantCulture)}
        $results.Add($metrics)
        $results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputRoot 'samples.json') -Encoding UTF8
        Write-Host $line
    }
}
Write-Host ('성능 비교 '+$results.Count+'회 완료: '+$outputRoot)
