param(
    [string]$Compiler = 'g++',
    [switch]$Sanitize,
    [string]$SenderRoot = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $repoRoot '.pio/native-tests'
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null

# In the normal GRC26 checkout this is RRU/Embedded beside RTU. A staged or
# standalone checkout may provide the sender location explicitly.
if ([string]::IsNullOrWhiteSpace($SenderRoot)) {
    $candidateSender = [IO.Path]::GetFullPath((Join-Path $repoRoot '../../RTU'))
    if (Test-Path -LiteralPath (Join-Path $candidateSender 'include/Utils/TelemetryProtocol.h')) {
        $SenderRoot = $candidateSender
    }
}
if (-not [string]::IsNullOrWhiteSpace($SenderRoot)) {
    foreach ($schema in @('TelemetryProtocol.h')) {
        $senderSchema = Join-Path $SenderRoot "include/Utils/$schema"
        $receiverSchema = Join-Path $repoRoot "include/Utils/$schema"
        if ((Get-FileHash -LiteralPath $senderSchema -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $receiverSchema -Algorithm SHA256).Hash) {
            throw "Receiver $schema differs from the RTU sender schema."
        }
    }
    Write-Output 'Sender/receiver schema copies match.'
}
else {
    Write-Output 'Sender schema not available; use -SenderRoot to enable schema drift validation.'
}

$testExecutable = Join-Path $buildDirectory 'receiver_tests.exe'
$compilerCommand = Get-Command $Compiler -ErrorAction Stop
$compileArguments = @(
    '-std=c++11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O1', '-g',
    '-I', (Join-Path $repoRoot 'include/Utils'),
    (Join-Path $PSScriptRoot 'native/test_main.cpp'),
    (Join-Path $PSScriptRoot 'native/test_decoder.cpp'),
    (Join-Path $PSScriptRoot 'native/test_csv.cpp'),
    (Join-Path $repoRoot 'src/Utils/TelemetryReceiver.cpp'),
    (Join-Path $repoRoot 'src/Utils/TelemetryCsv.cpp'),
    '-o', $testExecutable
)
if ($Sanitize) {
    $compileArguments += @('-fsanitize=address,undefined', '-fno-omit-frame-pointer')
}
& $compilerCommand.Source @compileArguments
if ($LASTEXITCODE -ne 0) {
    throw "Receiver native test compilation failed with exit code $LASTEXITCODE"
}
& $testExecutable
if ($LASTEXITCODE -ne 0) {
    throw "Receiver native tests failed with exit code $LASTEXITCODE"
}

$integrationExecutable = Join-Path $buildDirectory 'lora_task_tests.exe'
$integrationArguments = @(
    '-std=c++11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O1', '-g',
    '-I', (Join-Path $PSScriptRoot 'integration_stubs'),
    '-I', (Join-Path $repoRoot 'include/Utils'),
    (Join-Path $PSScriptRoot 'test_lora_task.cpp'),
    (Join-Path $repoRoot 'src/Utils/TelemetryReceiver.cpp'),
    (Join-Path $repoRoot 'src/Utils/TelemetryCsv.cpp'),
    '-o', $integrationExecutable
)
if ($Sanitize) {
    $integrationArguments += @('-fsanitize=address,undefined', '-fno-omit-frame-pointer')
}
& $compilerCommand.Source @integrationArguments
if ($LASTEXITCODE -ne 0) {
    throw "LoRa task integration test compilation failed with exit code $LASTEXITCODE"
}
& $integrationExecutable
if ($LASTEXITCODE -ne 0) {
    throw "LoRa task integration tests failed with exit code $LASTEXITCODE"
}
