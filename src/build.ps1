param([switch]$Run)
$r = $PSScriptRoot
$env:PATH = "$r\.tools\mingw64\bin;$r\.tools\cmake-4.4.3-windows-x86_64\bin;$r\.tools\ninja;$env:PATH"
if (-not (Test-Path "$r\build\build.ninja")) { cmake -S $r -B "$r\build" -G Ninja -DCMAKE_BUILD_TYPE=Release }
cmake --build "$r\build"
if ($Run -and $LASTEXITCODE -eq 0) { Start-Process "$r\build\mikmap.exe" }
