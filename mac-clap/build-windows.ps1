$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "build\windows"

cmake -S $Root -B $BuildDir -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed (exit $LASTEXITCODE)." }
cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed (exit $LASTEXITCODE)." }

$TestExe = Join-Path $BuildDir "Release\host_test.exe"
$Clap = Join-Path $BuildDir "clap\Release\Reverie.clap"
if (-not (Test-Path $Clap)) {
    $Clap = Join-Path $BuildDir "clap\Reverie.clap"
}

& $TestExe $Clap
if ($LASTEXITCODE -ne 0) { throw "Reverie host test failed (exit $LASTEXITCODE)." }
Write-Host "Built Reverie.clap! Copy it to: C:\Program Files\Common Files\CLAP\"
