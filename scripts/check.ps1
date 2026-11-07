param([switch]$Capture)
$ErrorActionPreference='Stop'
$sceneRoot=Join-Path (Split-Path $PSScriptRoot -Parent) '场景工程_美术重制'
$testExe=Join-Path $sceneRoot 'bin/MeshTests.exe'
if(-not(Test-Path -LiteralPath $testExe)){throw '请先运行 scripts/build.ps1。'}
$assetRoot=Join-Path $sceneRoot 'assets'
$count=@(Get-ChildItem -LiteralPath $assetRoot -Recurse -Filter '*.obj' -File).Count
if($count -lt 10){throw '运行素材不完整：预期至少10个OBJ。'}
& $testExe $assetRoot $count
if($LASTEXITCODE -ne 0){throw '模型与数学检查失败。'}
if($Capture){
    & (Join-Path $sceneRoot 'bin/LuoshenScene.exe') --capture
    if($LASTEXITCODE -ne 0){throw '实际图形渲染失败。'}
}
