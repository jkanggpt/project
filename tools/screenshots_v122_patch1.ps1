# 후속 보완의 실제 렌더러 화면. 사용자 저장·오디오는 사용하지 않습니다.
param([string[]]$OnlyModes=@('game','population','farms','effects','raid','alerts','tower','assign'))
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$game=Join-Path $projectRoot 'build/msvc-check/bin/Release/castle_game.exe'
foreach($size in @('1440x900','1024x720')) {
    $width,$height=$size.Split('x')
    foreach($mode in $OnlyModes) {
        $prefix=Join-Path $projectRoot ('artifacts/v122_patch1/screen_'+$mode+'_'+$width)
        $png=Join-Path $projectRoot ('docs/screenshots/v122_patch1_'+$mode+'_'+$width+'.png')
        $process=Start-Process -FilePath $game -ArgumentList @('--screenshot',('"'+$png+'"'),'--screen',$mode,'--width',$width,'--height',$height,'--no-audio') -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput ($prefix+'.log') -RedirectStandardError ($prefix+'.err')
        if($process.ExitCode -ne 0){throw ('Screenshot failed: '+$mode)}
        Write-Output ($mode+' '+$width+' complete')
    }
}
