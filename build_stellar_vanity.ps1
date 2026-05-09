$ErrorActionPreference = "Stop"

$libsodiumVersion = "1.0.21-stable"
$libsodiumUrl = "https://download.libsodium.org/libsodium/releases/libsodium-$libsodiumVersion-msvc.zip"
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$archive = Join-Path $repoRoot "libsodium-msvc.zip"
$vendorDir = Join-Path $repoRoot "vendor"
$libsodiumDir = Join-Path $vendorDir "libsodium"
$includeDir = Join-Path $libsodiumDir "include"
$libDir = Join-Path $libsodiumDir "x64\Release\v143\static"
$vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if (!(Test-Path $vcvars)) {
  throw "Visual Studio 2022 Build Tools with C++ tools were not found."
}

if (!(Test-Path (Join-Path $includeDir "sodium.h")) -or !(Test-Path (Join-Path $libDir "libsodium.lib"))) {
  New-Item -ItemType Directory -Force -Path $vendorDir | Out-Null

  if (!(Test-Path $archive)) {
    Write-Host "Downloading libsodium $libsodiumVersion..."
    curl.exe -L $libsodiumUrl -o $archive
  }

  if (Test-Path $libsodiumDir) {
    Remove-Item -Recurse -Force $libsodiumDir
  }

  tar.exe -xf $archive -C $vendorDir
}

$buildCommand = @(
  "call `"$vcvars`" >nul",
  "cl /nologo /O2 /D SODIUM_STATIC /I `"$includeDir`" stellar_vanity.c /Fe:stellar_vanity.exe /link /LIBPATH:`"$libDir`" libsodium.lib advapi32.lib ws2_32.lib user32.lib"
) -join " && "

cmd.exe /d /c $buildCommand
