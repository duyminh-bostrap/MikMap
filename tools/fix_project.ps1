# ========================================================================
#  tools/fix_project.ps1 - va file .vcxproj do oF Project Generator sinh
#
#  PHAI chay lai sau MOI lan chay Project Generator, vi PG ghi de file.
#
#  Luu y: script nay co y viet KHONG DAU. PowerShell 5.1 doc file .ps1
#  theo ANSI khi khong co BOM, nen dau tieng Viet lam hong cu phap.
#
#  Hai viec can sua:
#
#  1. PG luon chen hai stub cua template - src\main.cpp va src\ofApp.cpp -
#     du du an nay khong dung chung (diem vao that la src\app\main.cpp).
#     De nguyen thi build bao "Cannot open source file".
#
#  2. PG them TUNG thu muc con vao include path (src\core\math, src\ui...)
#     nhung KHONG them chinh `src`. Code cua du an include theo duong dan
#     day du tu goc src - #include "core/math/Vec2.h" - nen thieu `src`
#     la hong toan bo.
#
#  Script nay idempotent: chay nhieu lan khong gay hai.
# ========================================================================

$ErrorActionPreference = 'Stop'
$proj = (Resolve-Path (Join-Path $PSScriptRoot '..\HexMapping.vcxproj')).Path

$text = Get-Content $proj -Raw
$original = $text

# -- 1. Bo stub cua template --------------------------------------------
$before = $text
$text = $text -replace '(?m)^\s*<ClCompile Include="src\\main\.cpp"\s*/>\r?\n', ''
$text = $text -replace '(?m)^\s*<ClCompile Include="src\\ofApp\.cpp"\s*/>\r?\n', ''
$text = $text -replace '(?ms)\s*<ClCompile Include="src\\main\.cpp">.*?</ClCompile>\r?\n', "`n"
$text = $text -replace '(?ms)\s*<ClCompile Include="src\\ofApp\.cpp">.*?</ClCompile>\r?\n', "`n"
$text = $text -replace '(?m)^\s*<ClInclude Include="src\\ofApp\.h"\s*/>\r?\n', ''

if ($text -ne $before) { Write-Output "  [OK] Da bo stub template" }
else                   { Write-Output "  [--] Khong con stub template" }

# -- 2. Them `src` vao include path -------------------------------------
if ($text -match '%\(AdditionalIncludeDirectories\);src;') {
    Write-Output "  [--] 'src' da co trong include path"
} else {
    $text = $text -replace '%\(AdditionalIncludeDirectories\);', '%(AdditionalIncludeDirectories);src;'
    Write-Output "  [OK] Da them 'src' vao include path"
}

# -- 3. Them /utf-8 -----------------------------------------------------
#
# KHONG PHAI tuy chon cho dep. Khong co no, MSVC doc file .cpp theo
# codepage ANSI cua may (1252 o day, 1258 tren Windows tieng Viet, 932
# tren Windows Nhat). Bang chuoi trong src/ui/Localization.cpp la UTF-8,
# nen thieu co nay thi giao dien hien sai dau -- va sai KHAC NHAU tuy may,
# tuc la may nay chay dung con may dong nghiep thi hong.
#
# CMakeLists.txt da co /utf-8 san; day la ban tuong duong cho MSBuild.
if ($text -match '/utf-8') {
    Write-Output "  [--] '/utf-8' da co trong AdditionalOptions"
} else {
    $text = $text -replace '<AdditionalOptions>/Zc:__cplusplus', '<AdditionalOptions>/utf-8 /Zc:__cplusplus'
    Write-Output "  [OK] Da them '/utf-8' vao AdditionalOptions"
}

if ($text -ne $original) {
    Set-Content -Path $proj -Value $text -Encoding UTF8 -NoNewline
    Write-Output "  Da ghi: $proj"
} else {
    Write-Output "  Khong co gi thay doi."
}
