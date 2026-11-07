param([string]$MSBuildPath)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot -Parent
if(-not $MSBuildPath){
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if(Test-Path -LiteralPath $vswhere){$MSBuildPath= & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1}
}
if(-not $MSBuildPath){$cmd=Get-Command MSBuild.exe -ErrorAction SilentlyContinue;if($cmd){$MSBuildPath=$cmd.Source}}
if(-not $MSBuildPath -or -not(Test-Path -LiteralPath $MSBuildPath)){throw '未找到MSBuild，请安装Visual Studio C++桌面开发或传入 -MSBuildPath。'}
$msbuildVersion= & $MSBuildPath -version -nologo | Select-Object -Last 1
$toolset=if(([version]$msbuildVersion).Major -ge 18){'v145'}else{'v143'}
foreach($project in @('LuoshenScene','MeshTests')){
    & $MSBuildPath (Join-Path $repoRoot "场景工程_美术重制/$project.vcxproj") /nologo /m /v:minimal /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$toolset"
    if($LASTEXITCODE -ne 0){throw "$project 构建失败：$LASTEXITCODE"}
}
