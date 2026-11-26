# 현재 실행 파일의 실제 화면을 사용자 저장과 분리해 촬영합니다.
param(
 [Parameter(Mandatory=$true)][string]$Game,
 [string]$Output='build/reports/screens',
 [string[]]$Sizes=@('800x600','1024x768','1440x900','1920x1080'),
 [string[]]$Scenes=@('title','merchant','clinic','lighthouse','smallboat','windmill','irrigated','fishing','largeboat','household','forecast','roads','facilities','rank_requirements','alerts','cards_high','gates')
)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outputRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot $Output))
if(-not $outputRoot.StartsWith($projectRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw '출력은 작업공간 안이어야 합니다.'}
$binary=(Resolve-Path -LiteralPath $Game).Path
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
foreach($size in $Sizes){
 if($size -notmatch '^(\d+)x(\d+)$'){throw '잘못된 해상도'}
 $width=$Matches[1];$height=$Matches[2]
 foreach($scene in $Scenes){
  if($scene -notmatch '^[a-z_]+$'){throw '잘못된 장면 이름'}
  $prefix=Join-Path $outputRoot ($scene+'_'+$size)
  $arguments=@('--screenshot',('"'+$prefix+'.png"'),'--screen',$scene,'--width',$width,'--height',$height,'--no-audio','--save-dir',('"'+(Join-Path $outputRoot 'saves')+'"'))
  $process=Start-Process -FilePath $binary -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput ($prefix+'.log') -RedirectStandardError ($prefix+'.err')
  if($process.ExitCode -ne 0){throw ('화면 검사 실패: '+$scene+' '+$size)}
  Write-Output ($scene+' '+$size+' 완료')
 }
}
