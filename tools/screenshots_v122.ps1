# 자동 화면 검사. 실제 사용자 저장과 오디오를 사용하지 않습니다.
param([string]$Executable='build/msvc-check/bin/Release/castle_game.exe',[string[]]$OnlyModes=@())
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$game=Join-Path $projectRoot $Executable
$screens=Join-Path $projectRoot 'docs/screenshots'
$logs=Join-Path $projectRoot 'artifacts/v122'
New-Item -ItemType Directory -Path $screens -Force | Out-Null
foreach($size in @('1440x900','1024x720')) {
    $width,$height=$size.Split('x')
    $modes=if($OnlyModes.Count){$OnlyModes}elseif($size -eq '1440x900'){@('title','game','villagers','uniforms','farms','rotations','connections','market','population','load')}else{@('game','market','population')}
    foreach($mode in $modes) {
        $suffix=if($size -eq '1440x900'){''}else{'_1024'}
        $png=Join-Path $screens ('v122_'+$mode+$suffix+'.png')
        $arguments=@('--screenshot',('"'+$png+'"'),'--screen',$mode,'--no-audio','--width',$width,'--height',$height)
        if($mode -eq 'market'){$arguments+=@('--difficulty','peaceful')}
        $process=Start-Process -FilePath $game -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput (Join-Path $logs ('screen_'+$mode+$suffix+'.log')) -RedirectStandardError (Join-Path $logs ('screen_'+$mode+$suffix+'.err'))
        if($process.ExitCode -ne 0){throw ('Screenshot failed: '+$mode+' '+$size)}
        Write-Output ('screenshot='+$png)
    }
}
