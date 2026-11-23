param(
 [Parameter(Mandatory=$true)][string]$Compiler,
 [Parameter(Mandatory=$true)][string]$MinHookInclude,
 [Parameter(Mandatory=$true)][string]$MinHookLibrary,
 [switch]$Test
)
$ErrorActionPreference = 'Stop'
$buildArgs = @('-std=c++17','-O2','-static','-static-libgcc','-static-libstdc++','-DWINVER=0x0601','-D_WIN32_WINNT=0x0601',"-I$MinHookInclude")
Push-Location $PSScriptRoot
try {
 if ($Test) {
  & $Compiler @buildArgs 'tests.cpp' $MinHookLibrary '-luser32' '-o' 'tests.exe'
  if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
  & './tests.exe'
  if ($LASTEXITCODE -ne 0) { throw 'Regression test failed' }
 } else {
  & $Compiler @buildArgs '-shared' 'large_maps.cpp' $MinHookLibrary '-luser32' '-o' 'TS2-LargeMaps.asi'
  if ($LASTEXITCODE -ne 0) { throw 'ASI compilation failed' }
 }
} finally { Pop-Location }
