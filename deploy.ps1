param(
    [string]$ModName = "BossSense - Dev"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$game = Get-Process -Name "SkyrimSE" -ErrorAction SilentlyContinue
if ($game) {
    Write-Error "SkyrimSE.exe is running - close the game before deploying (the DLL is locked)."
}

$built = Join-Path $root "build/Release/BossSense.dll"
if (-not (Test-Path $built)) {
    Write-Error "build/Release/BossSense.dll not found - run build.ps1 first."
}

$modRoot = "D:\Mod Organizer 2\mods\$ModName"
$plugins = Join-Path $modRoot "SKSE/Plugins"
New-Item -ItemType Directory -Path $plugins -Force | Out-Null

Copy-Item $built (Join-Path $plugins "BossSense.dll") -Force
$pdb = Join-Path $root "build/Release/BossSense.pdb"
if (Test-Path $pdb) {
    Copy-Item $pdb (Join-Path $plugins "BossSense.pdb") -Force
}

# Keep the user's settings: only seed the ini on first deploy.
$iniDst = Join-Path $plugins "BossSense.ini"
if (-not (Test-Path $iniDst)) {
    Copy-Item (Join-Path $root "dist/SKSE/Plugins/BossSense.ini") $iniDst -Force
}

# Translation file: only seed on first deploy so user edits survive.
$jsonDst = Join-Path $plugins "BossSense.json"
if (-not (Test-Path $jsonDst)) {
    Copy-Item (Join-Path $root "dist/SKSE/Plugins/BossSense.json") $jsonDst -Force
}

Write-Host "deployed to $modRoot"
Write-Host "Enable '$ModName' in MO2 before launching."
