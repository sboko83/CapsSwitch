param(
    [ValidateSet('build', 'test', 'package')]
    [string]$Target = 'build',
    [ValidateSet('x64', 'arm64')]
    [string[]]$Arch = @('x64', 'arm64')
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$obj = Join-Path $root 'obj'
$bin = Join-Path $root 'bin'
$sources = 'main.c', 'tray.c', 'hook.c', 'keystate.c', 'glyph.c', 'strings.c'
$libs = 'kernel32.lib', 'user32.lib', 'gdi32.lib', 'shell32.lib', 'advapi32.lib'
$baseEnv = [Environment]::GetEnvironmentVariables('Process')

# Imports the MSVC environment (cl, link, rc, INCLUDE, LIB) for a target
# architecture into this process, starting from the original environment
# so builds for several architectures do not mix their paths.
function Import-Msvc([string]$TargetArch) {
    foreach ($name in [Environment]::GetEnvironmentVariables('Process').Keys) {
        if (-not $baseEnv.Contains($name)) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    }
    foreach ($entry in $baseEnv.GetEnumerator()) { [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, 'Process') }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe not found. Install Visual Studio Build Tools with the C++ workload.' }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'MSVC not found. Install Visual Studio Build Tools with the C++ workload.' }
    $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
    $env:PATH = (Split-Path $vswhere) + ';' + $env:PATH # vcvarsall calls vswhere too
    $hostArch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'arm64' } else { 'x64' }
    $vcArch = if ($hostArch -eq $TargetArch) { $TargetArch } else { "${hostArch}_$TargetArch" }
    $lines = & cmd.exe /d /s /c "`"$vcvars`" $vcArch >nul && set"
    if ($LASTEXITCODE -ne 0) { throw "vcvarsall $vcArch failed." }
    foreach ($line in $lines) {
        $i = $line.IndexOf('=')
        if ($i -gt 0) { [Environment]::SetEnvironmentVariable($line.Substring(0, $i), $line.Substring($i + 1), 'Process') }
    }
}

function Invoke-Tool([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE." }
}

# Release EXE without the C runtime: own entry point, no security cookie,
# unreferenced code and data dropped.
function Invoke-Build([string]$TargetArch) {
    Import-Msvc $TargetArch
    $out = Join-Path $obj $TargetArch
    $exeDir = Join-Path $bin $TargetArch
    New-Item -ItemType Directory -Path $out, $exeDir -Force | Out-Null
    $res = Join-Path $out 'capsswitch.res'
    Push-Location $root
    try { Invoke-Tool 'rc.exe' @('/nologo', '/I', 'src', "/fo$res", 'src\capsswitch.rc') }
    finally { Pop-Location }
    Invoke-Tool 'cl.exe' (@('/nologo', '/c', '/std:c17', '/O1', '/W4', '/WX', '/GS-', '/Gy', '/Gw', '/Zl',
        '/utf-8', '/DUNICODE', '/D_UNICODE', "/Fo$out\") + @($sources | ForEach-Object { Join-Path $root "src\$_" }))
    $objects = $sources | ForEach-Object { Join-Path $out ([IO.Path]::ChangeExtension($_, '.obj')) }
    $exe = Join-Path $exeDir 'CapsSwitch.exe'
    $linkArgs = @('/NOLOGO', '/NODEFAULTLIB', '/ENTRY:entry', '/SUBSYSTEM:WINDOWS', '/OPT:REF', '/OPT:ICF',
        '/MANIFEST:NO', '/DYNAMICBASE', '/NXCOMPAT', "/OUT:$exe") + $objects + $res + $libs
    if ($TargetArch -eq 'x64') { $linkArgs += '/CETCOMPAT' }
    Invoke-Tool 'link.exe' $linkArgs
    Write-Output ("Built: {0} ({1:N0} bytes)" -f $exe, (Get-Item -LiteralPath $exe).Length)
}

function Invoke-Tests {
    Import-Msvc 'x64'
    $out = Join-Path $obj 'tests'
    New-Item -ItemType Directory -Path $out -Force | Out-Null
    $exe = Join-Path $out 'tests.exe'
    $res = Join-Path $out 'capsswitch.res'
    Push-Location $root
    try { Invoke-Tool 'rc.exe' @('/nologo', '/I', 'src', "/fo$res", 'src\capsswitch.rc') }
    finally { Pop-Location }
    Invoke-Tool 'cl.exe' (@('/nologo', '/std:c17', '/W4', '/WX', '/Zi', '/utf-8', '/DUNICODE', '/D_UNICODE',
        "/Fo$out\", "/Fd$out\", "/Fe$exe", (Join-Path $root 'tests\tests.c')) +
        @('keystate.c', 'glyph.c', 'hook.c' | ForEach-Object { Join-Path $root "src\$_" }) +
        @('/link', $res, 'user32.lib', 'gdi32.lib'))
    Invoke-Tool $exe @()
}

function Write-Hash([string]$Path) {
    $name = Split-Path -Leaf $Path
    $hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath "$Path.sha256" -Value "$hash  $name" -Encoding ASCII
    Write-Output "Package: $Path"
    Write-Output "SHA-256: $hash"
}

# Inno Setup 6 compiler: PATH first, then a machine or per-user install.
function Find-Iscc {
    $command = Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($hive in 'HKLM:', 'HKCU:') {
        foreach ($view in 'SOFTWARE', 'SOFTWARE\WOW6432Node') {
            $key = "$hive\$view\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1"
            $dir = (Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue).InstallLocation
            if ($dir -and (Test-Path -LiteralPath (Join-Path $dir 'ISCC.exe'))) { return Join-Path $dir 'ISCC.exe' }
        }
    }
    throw 'Inno Setup 6 not found. Install it: winget install JRSoftware.InnoSetup'
}

function Invoke-Package {
    $header = Get-Content -LiteralPath (Join-Path $root 'src\version.h') -Raw
    $parts = foreach ($part in 'MAJOR', 'MINOR', 'PATCH') {
        $match = [regex]::Match($header, "(?m)^#define VERSION_$part (\d+)\s*$")
        if (-not $match.Success) { throw "VERSION_$part not found in src/version.h." }
        $match.Groups[1].Value
    }
    $version = $parts -join '.'
    [xml]$manifest = Get-Content -LiteralPath (Join-Path $root 'app.manifest') -Raw
    if ($manifest.assembly.assemblyIdentity.version -ne "$version.0") {
        throw 'app.manifest version does not match src/version.h.'
    }

    Invoke-Tests
    $dist = Join-Path $root 'dist'
    New-Item -ItemType Directory -Path $dist -Force | Out-Null
    foreach ($a in $Arch) {
        Invoke-Build $a
        $zipName = "CapsSwitch-v$version-windows-$a.zip"
        $zipPath = Join-Path $dist $zipName
        $files = @((Join-Path $bin "$a\CapsSwitch.exe"),
            (Join-Path $root 'QUICKSTART.txt'), (Join-Path $root 'LICENSE'))
        Compress-Archive -LiteralPath $files -DestinationPath $zipPath -Force
        Write-Hash $zipPath
    }

    # The setup carries both EXEs, so it is built only with both architectures.
    if ($Arch -contains 'x64' -and $Arch -contains 'arm64') {
        $iscc = Find-Iscc
        Invoke-Tool $iscc @('/Q', "/DAppVersion=$version", (Join-Path $root 'installer\CapsSwitch.iss'))
        Write-Hash (Join-Path $dist "CapsSwitch-v$version-windows-setup.exe")
    } else {
        Write-Output 'Setup skipped: it needs both x64 and arm64.'
    }
}

switch ($Target) {
    'build' { foreach ($a in $Arch) { Invoke-Build $a } }
    'test' { Invoke-Tests }
    'package' { Invoke-Package }
}
