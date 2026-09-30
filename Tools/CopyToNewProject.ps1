# Copies the ActionGame code, docs and required config into a fresh Unreal project.
#
# Usage: run CopyToNewProject.bat from the repository root (it asks for the folder),
#        or: powershell -ExecutionPolicy Bypass -File Tools\CopyToNewProject.ps1 -NewProjectDir "D:\UnrealProjects\ActionGame"
#
# The new project must be a C++ project named exactly "ActionGame" so the module name matches.
# Safe to run again after every "git pull" to bring new code into the new project.

param(
    [Parameter(Mandatory = $true)]
    [string]$NewProjectDir
)

$ErrorActionPreference = 'Stop'

function Fail([string]$Message) {
    Write-Host ""
    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit 1
}

$RepoDir = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$NewProjectDir = $NewProjectDir.Trim().Trim('"').TrimEnd('\')

if (-not (Test-Path $NewProjectDir)) {
    Fail "Folder not found: $NewProjectDir"
}
$NewProjectDir = (Resolve-Path $NewProjectDir).Path

if ($NewProjectDir -eq $RepoDir) {
    Fail "That is this repository folder. Pick the NEW project's folder instead."
}
if (-not (Test-Path (Join-Path $NewProjectDir 'ActionGame.uproject'))) {
    Fail "No ActionGame.uproject in $NewProjectDir. Create the new project as a C++ project named exactly 'ActionGame'."
}

Write-Host "Copying from: $RepoDir"
Write-Host "Copying into: $NewProjectDir"
Write-Host ""

# 1) Game code: replace the template's module with ours.
$SourceTarget = Join-Path $NewProjectDir 'Source\ActionGame'
# First migration = the project still has the template's code, not ours.
$FirstMigration = -not (Test-Path (Join-Path $SourceTarget 'ActionGameTags.h'))
if (Test-Path $SourceTarget) {
    Remove-Item $SourceTarget -Recurse -Force
}
Copy-Item (Join-Path $RepoDir 'Source\ActionGame') $SourceTarget -Recurse
Write-Host "[1/4] Game code copied to Source\ActionGame"

# 2) Docs. (Remove first: Copy-Item would otherwise nest Docs\Docs on a second run.)
$DocsTarget = Join-Path $NewProjectDir 'Docs'
if (Test-Path $DocsTarget) {
    Remove-Item $DocsTarget -Recurse -Force
}
Copy-Item (Join-Path $RepoDir 'Docs') $DocsTarget -Recurse
Copy-Item (Join-Path $RepoDir 'README.md') (Join-Path $NewProjectDir 'README.md') -Force
Write-Host "[2/4] Docs copied"

# 3) Config: point the game at our game mode and add the combat-specific settings.
$IniPath = Join-Path $NewProjectDir 'Config\DefaultEngine.ini'
$Ini = if (Test-Path $IniPath) { Get-Content $IniPath -Raw } else { '' }
$GameModeLine = 'GlobalDefaultGameMode=/Script/ActionGame.ActionGameMode'

if ($Ini -match '(?m)^GlobalDefaultGameMode=') {
    $Ini = $Ini -replace '(?m)^GlobalDefaultGameMode=[^\r\n]*', $GameModeLine
}
elseif ($Ini -match '(?m)^\[/Script/EngineSettings\.GameMapsSettings\]') {
    $Ini = $Ini -replace '(?m)^(\[/Script/EngineSettings\.GameMapsSettings\])', "`$1`r`n$GameModeLine"
}
else {
    $Ini += "`r`n[/Script/EngineSettings.GameMapsSettings]`r`n$GameModeLine`r`n"
}

if ($Ini -notmatch 'ActionGame additions') {
    $Ini += @"

; ---- ActionGame additions ----
[/Script/Engine.PhysicsSettings]
bSubstepping=True
MaxSubstepDeltaTime=0.016667
MaxSubsteps=6

[/Script/NavigationSystem.RecastNavMesh]
RuntimeGeneration=Dynamic
"@
    if ($Ini -notmatch 'ECC_GameTraceChannel1') {
        $Ini += "`r`n" + @"

[/Script/Engine.CollisionProfile]
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Block,bTraceType=True,bStaticObject=False,Name="Weapon")
"@
    }
}
Set-Content -Path $IniPath -Value $Ini -Encoding UTF8
Write-Host "[3/4] Config\DefaultEngine.ini updated (game mode, physics, navigation, Weapon trace channel)"

# 4) Remove stale build output so Unreal recompiles with the new code
#    (otherwise it would load the template's old DLL and our classes would be missing).
#    Only needed the first time; updates keep the build output so Unreal rebuilds incrementally.
if ($FirstMigration) {
    foreach ($Folder in @('Binaries', 'Intermediate')) {
        $Path = Join-Path $NewProjectDir $Folder
        if (Test-Path $Path) {
            Remove-Item $Path -Recurse -Force
        }
    }
    Write-Host "[4/4] Template build output removed"
}
else {
    Write-Host "[4/4] Update: existing build output kept"
}

Write-Host ""
Write-Host "Done! Now open the new project from the Epic Games Launcher (Library > My Projects)" -ForegroundColor Green
Write-Host "and click 'Yes' when Unreal asks to rebuild the ActionGame module." -ForegroundColor Green
