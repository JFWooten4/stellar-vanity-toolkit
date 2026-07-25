param(
  [switch]$Partials
)

$ErrorActionPreference = "Stop"

$libsodiumVersion = "1.0.21-stable"
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$archive = Join-Path $repoRoot "libsodium-msvc.zip"
$vendorDir = Join-Path $repoRoot "vendor"
$libsodiumDir = Join-Path $vendorDir "libsodium"
$includeDir = Join-Path $libsodiumDir "include"
$libDir = Join-Path $libsodiumDir "x64\Release\v143\static"
$vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$sourceFile = if ($Partials) { "stellar_vanity_parallel_partials.c" } else { "stellar_vanity_parallel.c" }
$outputFile = if ($Partials) { "stellar_vanity_parallel_partials.exe" } else { "stellar_vanity_parallel.exe" }

if (!(Test-Path $vcvars)) {
  throw "Visual Studio 2022 Build Tools with C++ tools were not found."
}

if (!(Test-Path (Join-Path $includeDir "sodium.h")) -or !(Test-Path (Join-Path $libDir "libsodium.lib"))) {
  if (!(Test-Path $archive)) {
    throw @"
libsodium $libsodiumVersion was not found. No download was attempted.
Before going offline, place the official Windows archive at:
  $archive
Then run this build script again.
"@
  }

  New-Item -ItemType Directory -Force -Path $vendorDir | Out-Null

  if (Test-Path $libsodiumDir) {
    Remove-Item -Recurse -Force $libsodiumDir
  }

  tar.exe -xf $archive -C $vendorDir
}

if (!(Test-Path (Join-Path $includeDir "sodium.h")) -or !(Test-Path (Join-Path $libDir "libsodium.lib"))) {
  throw "The local libsodium archive did not contain the expected x64 v143 static library."
}

$buildCommand = @(
  "call `"$vcvars`" >nul",
  "cl /nologo /O2 /D SODIUM_STATIC /I `"$includeDir`" $sourceFile /Fe:$outputFile /link /LIBPATH:`"$libDir`" libsodium.lib advapi32.lib ws2_32.lib user32.lib"
) -join " && "

cmd.exe /d /c $buildCommand
