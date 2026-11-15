#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Repository,
    [Parameter(Mandatory)][string]$BinaryDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$QtRoot,
    [Parameter(Mandatory)][string]$Configuration,
    [Parameter(Mandatory)][string]$Revision,
    [string]$QtVersion = '6.8.3',
    [ValidateSet('0', '1')][string]$LeapCCompiled = '0',
    [string]$LeapCHeaderSha256 = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'Windows deployment requires Windows x64.' }
if ($Configuration -cne 'Release') { throw 'Portable packages require Release; debug CRT/Qt binaries are not distributable.' }
if ($LeapCCompiled -eq '1' -and $LeapCHeaderSha256 -notmatch '^[a-fA-F0-9]{64}$') { throw 'An SDK-backed package must record the actual compile header SHA256.' }
$Repository = (Resolve-Path -LiteralPath $Repository).Path
$actualRevision = & git -C $Repository rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actualRevision -cne $Revision) {
    throw 'Source revision changed after configuration. Reconfigure and rebuild before packaging.'
}
$sourceDirty = -not [string]::IsNullOrWhiteSpace((& git -C $Repository status --porcelain | Out-String))
$BinaryDirectory = (Resolve-Path -LiteralPath $BinaryDirectory).Path
$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$sourceManifest = Get-Content (Join-Path $Repository 'packaging/qt-sources.json') -Raw | ConvertFrom-Json
if ($QtVersion -cne $sourceManifest.version) { throw "Qt $QtVersion has no matching corresponding-source manifest." }
$stage = Join-Path $OutputDirectory 'HandBridge-2.0.0-windows-x64-preview'
# Delete only our fixed staging child; never a caller-specified arbitrary tree.
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
foreach ($name in @('handbridge-engine.exe', 'handbridge-control.exe')) {
    Copy-Item -LiteralPath (Join-Path $BinaryDirectory $name) -Destination $stage
}
$deploy = Join-Path $QtRoot 'bin/windeployqt.exe'
if (-not (Test-Path -LiteralPath $deploy)) { throw "Missing official Qt deployment tool: $deploy" }
& $deploy --release --no-translations --no-compiler-runtime --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw --no-ffmpeg `
    --dir $stage --libdir $stage --plugindir (Join-Path $stage 'plugins') `
    --qml-deploy-dir (Join-Path $stage 'qml') --qmldir (Join-Path $Repository 'src/control/qml') `
    (Join-Path $stage 'handbridge-control.exe') (Join-Path $stage 'handbridge-engine.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit $LASTEXITCODE" }
@'
[Paths]
Prefix=.
Plugins=plugins
QmlImports=qml
'@ | Set-Content -LiteralPath (Join-Path $stage 'qt.conf') -Encoding utf8NoBOM

# windeployqt --compiler-runtime distributes a VC installer in Release. Use only
# VS's documented redistributable files for an actually app-local x64 CRT.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Cannot locate vswhere to select the licensed VC143 x64 redistributables.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $vs) { throw 'A Visual Studio 2022 C++ tools installation is required for packaging.' }
$crt = Get-ChildItem -LiteralPath (Join-Path $vs 'VC/Redist/MSVC') -Directory |
    Sort-Object Name -Descending | ForEach-Object {
        $candidate = Join-Path $_.FullName 'x64/Microsoft.VC143.CRT'
        if (Test-Path -LiteralPath $candidate) { Get-Item -LiteralPath $candidate }
    } | Select-Object -First 1
if (-not $crt) { throw 'No permitted x64/Microsoft.VC143.CRT directory was found; debug/non-redist files must not be substituted.' }
Copy-Item -Path (Join-Path $crt.FullName '*.dll') -Destination $stage
foreach ($required in @('Qt6Core.dll', 'Qt6Network.dll', 'Qt6Gui.dll', 'Qt6Qml.dll', 'Qt6Quick.dll',
    'vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll', 'plugins/platforms/qwindows.dll',
    'qml/QtQuick/qmldir', 'qml/QtQuick/Controls/Basic/qmldir')) {
    if (-not (Test-Path -LiteralPath (Join-Path $stage $required))) { throw "Deployment is incomplete: missing $required" }
}
$forbidden = @(Get-ChildItem -LiteralPath $stage -File -Recurse | Where-Object {
    $_.Name -match '(?i)^LeapC\.(dll|lib|h)$|^Qt6.*d\.dll$|^(msvcp|vcruntime|ucrtbase).*d\.dll$|\.pdb$|^vc_redist.*\.exe$'
})
if ($forbidden.Count) { throw "Forbidden SDK/debug/installer content: $($forbidden.FullName -join ', ')" }
# The source offer is deliberately limited to the deployed Qt modules. An extra
# module must receive matching source/license coverage before this can succeed.
foreach ($dll in Get-ChildItem -LiteralPath $stage -Filter 'Qt6*.dll' -File) {
    if ($dll.Name -notmatch '^Qt6(Core|Concurrent|Gui|Network|OpenGL|Widgets|PrintSupport|Sql|Xml|Qml.*|Quick.*|Labs.*)\.dll$') {
        throw "No corresponding-source mapping for $($dll.Name). Update packaging/qt-sources.json and notices first."
    }
}
Copy-Item -LiteralPath (Join-Path $Repository 'packaging/PORTABLE_README.txt') -Destination (Join-Path $stage 'README.txt')
Copy-Item -LiteralPath (Join-Path $Repository 'packaging/THIRD_PARTY_NOTICES.txt') -Destination $stage
Copy-Item -LiteralPath (Join-Path $Repository 'packaging/licenses') -Destination $stage -Recurse
New-Item -ItemType Directory -Path (Join-Path $stage 'examples') | Out-Null
Copy-Item -LiteralPath (Join-Path $Repository 'configs/studio-default.example.json') -Destination (Join-Path $stage 'examples')
$sourceCache = Join-Path $OutputDirectory 'qt-source-cache'
$sources = Join-Path $stage 'corresponding-source'
New-Item -ItemType Directory -Force -Path $sourceCache, $sources | Out-Null
foreach ($archive in $sourceManifest.archives) {
    $cached = Join-Path $sourceCache $archive.name
    if (-not (Test-Path -LiteralPath $cached) -or (Get-FileHash -LiteralPath $cached -Algorithm SHA256).Hash -ine $archive.sha256) {
        $partial = "$cached.partial"
        Invoke-WebRequest -Uri $archive.url -OutFile $partial -MaximumRetryCount 2
        if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ine $archive.sha256) { throw "Qt source checksum mismatch: $($archive.name)" }
        Move-Item -LiteralPath $partial -Destination $cached -Force
    }
    Copy-Item -LiteralPath $cached -Destination $sources
}
Copy-Item -LiteralPath (Join-Path $Repository 'packaging/qt-sources.json') -Destination $sources
$files = @(Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{ path = [IO.Path]::GetRelativePath($stage, $_.FullName).Replace('\', '/');
        bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$manifest = [ordered]@{
    product = 'Hand Bridge'; version = '2.0.0-preview'; source_revision = $Revision; source_dirty = $sourceDirty;
    platform = 'Windows x64'; configuration = 'Release'; qt_version = $QtVersion;
    leapc_adapter_compiled = ($LeapCCompiled -eq '1'); leapc_runtime_included = $false;
    leapc_compile_header_sha256 = $LeapCHeaderSha256;
    hardware_verified = $false; runtime_requirement = 'Windows 10/11 x64; real tracking additionally requires installed Ultraleap runtime/service';
    crt_source = 'Visual Studio 2022 Redist/MSVC/x64/Microsoft.VC143.CRT';
    crt_version = (Get-Item -LiteralPath (Join-Path $stage 'vcruntime140.dll')).VersionInfo.FileVersion;
    qt_corresponding_source = $sourceManifest; files = $files
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $stage 'package-manifest.json') -Encoding utf8NoBOM
$zip = Join-Path $OutputDirectory 'HandBridge-2.0.0-windows-x64-preview.zip'
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -LiteralPath $stage -DestinationPath $zip -CompressionLevel Optimal
"$((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($zip))" |
    Set-Content -LiteralPath "$zip.sha256" -Encoding ascii
Write-Host "Portable developer preview: $zip"
Write-Host 'SDK runtime excluded; physical cameras/Hyperion/USB/TouchDesigner remain UNTESTED ON HARDWARE.'
