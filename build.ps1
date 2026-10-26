param(
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

if (-not $env:VCPKG_ROOT) {
    $env:VCPKG_ROOT = "E:\vcpkg"
}

if (-not (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) {
    throw "vcpkg not found at '$env:VCPKG_ROOT' - set VCPKG_ROOT or install vcpkg."
}

if (-not (Test-Path "$root/build/CMakeCache.txt")) {
    cmake -B build -S . -G "Visual Studio 18 2026" -A x64 `
        -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
        -DVCPKG_TARGET_TRIPLET=x64-windows-static-md `
        -DVCPKG_HOST_TRIPLET=x64-windows-static-md
}

cmake --build build --config $Config

$out = Join-Path $root "build/$Config/BossSense.dll"
if (Test-Path $out) {
    $info = Get-Item $out
    Write-Host ("built {0} ({1:N0} bytes, {2})" -f $info.Name, $info.Length, $info.LastWriteTime)
}
