param(
    [string]$Compiler = 'g++',
    [switch]$Sanitize
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $repoRoot '.pio/native-tests'
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
$testExecutable = Join-Path $buildDirectory 'sender_tests.exe'
$compilerCommand = Get-Command $Compiler -ErrorAction Stop
$compileArguments = @(
    '-std=c++11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O1', '-g',
    '-I', (Join-Path $repoRoot 'include/Utils'),
    (Join-Path $PSScriptRoot 'native/test_main.cpp'),
    (Join-Path $PSScriptRoot 'native/test_decoder.cpp'),
    (Join-Path $PSScriptRoot 'native/test_sender.cpp'),
    (Join-Path $repoRoot 'src/Utils/EcuTelemetry.cpp'),
    (Join-Path $repoRoot 'src/Utils/TelemetrySender.cpp'),
    '-o', $testExecutable
)
if ($Sanitize) {
    $compileArguments += @('-fsanitize=address,undefined', '-fno-omit-frame-pointer')
}

& $compilerCommand.Source @compileArguments
if ($LASTEXITCODE -ne 0) {
    throw "Native test compilation failed with exit code $LASTEXITCODE"
}
& $testExecutable
if ($LASTEXITCODE -ne 0) {
    throw "Native tests failed with exit code $LASTEXITCODE"
}

$integrationExecutable = Join-Path $buildDirectory 'can_task_tests.exe'
$integrationArguments = @(
    '-std=c++11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O1', '-g',
    '-I', (Join-Path $PSScriptRoot 'integration_stubs'),
    '-I', (Join-Path $repoRoot 'include/Utils'),
    (Join-Path $PSScriptRoot 'test_can_task.cpp'),
    (Join-Path $repoRoot 'src/Utils/EcuTelemetry.cpp'),
    (Join-Path $repoRoot 'src/Utils/TelemetrySender.cpp'),
    '-o', $integrationExecutable
)
if ($Sanitize) {
    $integrationArguments += @('-fsanitize=address,undefined', '-fno-omit-frame-pointer')
}
& $compilerCommand.Source @integrationArguments
if ($LASTEXITCODE -ne 0) {
    throw "CAN task integration test compilation failed with exit code $LASTEXITCODE"
}
& $integrationExecutable
if ($LASTEXITCODE -ne 0) {
    throw "CAN task integration tests failed with exit code $LASTEXITCODE"
}
