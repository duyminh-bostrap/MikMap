# Convenience wrapper for Windows. Everything it does works the same as calling
# cmake by hand (see README.md); it just adds the optional src/.tools/ toolchain
# to PATH first, if that directory happens to exist on this machine.
#   .\src\build.ps1          build Release into src/build
#   .\src\build.ps1 -Run     ...and launch it
param([switch]$Run)
$r = $PSScriptRoot

# src/.tools/ is a machine-local toolchain cache (never in git). It is optional:
# without it, cmake/ninja/g++ are expected on the system PATH instead.
foreach ($sub in @('mingw64\bin', 'ninja')) {
  $p = Join-Path $r ".tools\$sub"
  if (Test-Path $p) { $env:PATH = "$p;$env:PATH" }
}
Get-ChildItem (Join-Path $r '.tools') -Directory -Filter 'cmake-*' -ErrorAction SilentlyContinue |
  Select-Object -First 1 |
  ForEach-Object { $env:PATH = "$(Join-Path $_.FullName 'bin');$env:PATH" }

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  Write-Error "cmake khong co tren PATH. Cai bang: winget install Kitware.CMake"
  exit 1
}

# Qua cmake/configure.cmake: tren may Windows khong co Visual Studio no tu chon
# Ninja thay cho NMake (mac dinh cua CMake, se hong vi khong co nmake).
$fs = $r.Replace('\', '/')
cmake "-DSRC=$fs" "-DBUILD=$fs/build" -DTYPE=Release -P "$fs/../cmake/configure.cmake"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build "$r\build" --config Release
if ($Run -and $LASTEXITCODE -eq 0) { Start-Process "$r\build\mikmap.exe" }
