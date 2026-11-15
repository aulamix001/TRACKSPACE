$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    cmake -S . -B build-windows -A x64 -DBUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) { throw 'Falha na configuracao CMake.' }
    cmake --build build-windows --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Falha na compilacao.' }
    ctest --test-dir build-windows -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes.' }
    New-Item -ItemType Directory -Force dist | Out-Null
    $bundle = 'build-windows/Trackspace_artefacts/Release/VST3/TRACKSPACE.vst3'
    Copy-Item -Recurse -Force $bundle 'dist/'
    Copy-Item 'docs/Studio-One.md' 'dist/LEIA-ME.md'
    Copy-Item -Recurse -Force 'ThirdParty' 'dist/'
    Compress-Archive -Path 'dist/TRACKSPACE.vst3', 'dist/LEIA-ME.md', 'dist/ThirdParty' -DestinationPath 'dist/TRACKSPACE-Windows-x64.zip' -Force
    Write-Host 'Pronto: dist/TRACKSPACE-Windows-x64.zip'
} finally { Pop-Location }
