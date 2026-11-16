#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Archive,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$ExternalLeapCLibrary = '',
    [string]$ExpectedLeapCVersion = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows) { throw 'The portable-package smoke requires actual Windows processes and DLL loading.' }
$Archive = (Resolve-Path -LiteralPath $Archive).Path
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$runDirectory = Join-Path $OutputDirectory ('isolated-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDirectory | Out-Null
Expand-Archive -LiteralPath $Archive -DestinationPath $runDirectory
$package = Join-Path $runDirectory 'HandBridge-2.0.0-windows-x64-preview'
$manifest = Get-Content (Join-Path $package 'package-manifest.json') -Raw | ConvertFrom-Json
if (-not $manifest.leapc_adapter_compiled -or $manifest.leapc_runtime_included -or $manifest.hardware_verified) {
    throw 'Expected a genuine SDK-header adapter build with external runtime and unverified hardware.'
}
$forbidden = @(Get-ChildItem -LiteralPath $package -File -Recurse | Where-Object { $_.Name -match '(?i)^LeapC\.(dll|lib|h)$|^Qt6.*d\.dll$|\.pdb$' })
if ($forbidden.Count) { throw 'The ZIP contains prohibited SDK/debug files.' }
foreach ($file in $manifest.files) {
    $path = Join-Path $package $file.path
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $file.sha256) { throw "Extracted file checksum mismatch: $($file.path)" }
}
$records = [Collections.Generic.List[object]]::new()
$processes = [Collections.Generic.List[object]]::new()
$cleanPath = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem"
function Start-CleanProcess([string]$Name, [string[]]$Arguments) {
    $info = [Diagnostics.ProcessStartInfo]::new((Join-Path $package $Name))
    $info.WorkingDirectory = $runDirectory
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [Text.Encoding]::UTF8
    $info.StandardErrorEncoding = [Text.Encoding]::UTF8
    foreach ($key in @($info.Environment.Keys)) {
        if ($key -match '^(QT_|QML|CMAKE|VCPKG|VSINSTALLDIR|VCINSTALLDIR|LEAP_SDK_ROOT|HANDBRIDGE_LEAPC_LIBRARY)') { [void]$info.Environment.Remove($key) }
    }
    $info.Environment['PATH'] = $cleanPath
    $info.Environment['QT_QPA_PLATFORM'] = 'windows'
    $info.Environment['QT_QUICK_BACKEND'] = 'software'
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    if (-not $process.Start()) { throw "Cannot launch $Name" }
    $record = [pscustomobject]@{ Process = $process; Out = $process.StandardOutput.ReadToEndAsync(); Error = $process.StandardError.ReadToEndAsync(); Name = $Name }
    $processes.Add($record)
    return $record
}
function Wait-CleanProcess($Record, [int]$Milliseconds = 15000) {
    if (-not $Record.Process.WaitForExit($Milliseconds)) { throw "$($Record.Name) exceeded the bounded smoke deadline." }
    return [pscustomobject]@{ exit = $Record.Process.ExitCode; stdout = $Record.Out.GetAwaiter().GetResult(); stderr = $Record.Error.GetAwaiter().GetResult() }
}
function Assert-LocalModules($Record, [bool]$Gui) {
    $Record.Process.Refresh()
    $modules = @($Record.Process.Modules | ForEach-Object { [pscustomobject]@{ name = $_.ModuleName; path = $_.FileName } })
    $qt = @($modules | Where-Object { $_.name -like 'Qt6*.dll' })
    if ($qt.Count -lt 2) { throw "Expected loaded Qt DLLs in $($Record.Name)" }
    foreach ($module in $qt) {
        if (-not $module.path.StartsWith($package + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Qt escaped the extracted package: $($module.path)"
        }
    }
    if (-not $Gui -and @($qt | Where-Object { $_.name -match '^Qt6(Gui|Qml|Quick)' }).Count) { throw 'Headless engine loaded rendering libraries.' }
    $crt = @($modules | Where-Object { $_.name -match '^(vcruntime140|msvcp140).*\.dll$' })
    if (-not $crt.Count) { throw 'No loaded MSVC release runtime modules were observed.' }
    foreach ($module in $crt) {
        if (-not $module.path.StartsWith($package + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "MSVC runtime escaped the app-local package: $($module.path)" }
    }
    $platformPlugin = @($modules | Where-Object { $_.name -ieq 'qwindows.dll' })
    if ($Gui -and (-not $platformPlugin.Count -or -not $platformPlugin[0].path.StartsWith($package + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase))) {
        throw 'The extracted package did not supply the actual Windows platform plugin.'
    }
    $records.Add([ordered]@{ check = "$($Record.Name) native modules"; result = 'PASS'; qt = $qt; crt = $crt })
}
$udp = [Net.Sockets.UdpClient]::new([Net.IPEndPoint]::new([Net.IPAddress]::Loopback, 0))
$udp.Client.ReceiveTimeout = 5000
$port = ([Net.IPEndPoint]$udp.Client.LocalEndPoint).Port
function Assert-FreshOsc([string]$Stage) {
    $remote = [Net.IPEndPoint]::new([Net.IPAddress]::Any, 0)
    # Drain queued data so post-GUI checks require a fresh engine datagram.
    for ($i = 0; $i -lt 10000 -and $udp.Available -gt 0; ++$i) { [void]$udp.Receive([ref]$remote) }
    $packet = $udp.Receive([ref]$remote)
    $text = [Text.Encoding]::ASCII.GetString($packet)
    if ($packet.Length -lt 12 -or $packet[0] -ne 47 -or -not $text.Contains(',f')) { throw "Invalid legacy scalar OSC at $Stage" }
    $records.Add([ordered]@{ check = $Stage; result = 'PASS'; udp_bytes = $packet.Length; address = $text.Split([char]0)[0] })
}
function Assert-Screenshot([string]$Path) {
    $png = [IO.File]::ReadAllBytes($Path)
    if ($png.Length -lt 1000 -or $png[0] -ne 137 -or $png[1] -ne 80) { throw 'No actual rendered PNG screenshot.' }
    $width = ([int]$png[16] -shl 24) -bor ([int]$png[17] -shl 16) -bor ([int]$png[18] -shl 8) -bor [int]$png[19]
    $height = ([int]$png[20] -shl 24) -bor ([int]$png[21] -shl 16) -bor ([int]$png[22] -shl 8) -bor [int]$png[23]
    if ($width -lt 1100 -or $height -lt 700) { throw "Unexpected rendered dimensions: ${width}x$height" }
    $records.Add([ordered]@{ check = 'Native Windows GUI render'; result = 'PASS'; width = $width; height = $height; bytes = $png.Length })
}
$engine = $null
try {
    $missing = Join-Path $runDirectory 'not-installed/LeapC.dll'
    $probe = Wait-CleanProcess (Start-CleanProcess 'handbridge-engine.exe' @('--check-leapc', '--leapc-library', $missing))
    if ($probe.exit -eq 0 -or $probe.stderr -notmatch 'Cannot load LeapC.dll') { throw "Missing external runtime was not actionable: $($probe | ConvertTo-Json -Compress)" }
    $records.Add([ordered]@{ check = 'Missing external Ultraleap runtime'; result = 'PASS'; diagnostic = $probe.stderr.Trim() })
    $probe = Wait-CleanProcess (Start-CleanProcess 'handbridge-engine.exe' @('--check-leapc', '--leapc-library', 'LeapC.dll'))
    if ($probe.exit -eq 0 -or $probe.stderr -notmatch 'absolute') { throw "Relative runtime path was not rejected: $($probe.stderr)" }
    $records.Add([ordered]@{ check = 'Relative DLL search rejected'; result = 'PASS'; diagnostic = $probe.stderr.Trim() })
    $probe = Wait-CleanProcess (Start-CleanProcess 'handbridge-engine.exe' @('--check-leapc', '--leapc-library', (Join-Path $env:SystemRoot 'System32/kernel32.dll')))
    if ($probe.exit -eq 0 -or $probe.stderr -notmatch 'missing required SDK export') { throw "Non-SDK library was not rejected: $($probe.stderr)" }
    $records.Add([ordered]@{ check = 'Real non-SDK system DLL rejected'; result = 'PASS'; diagnostic = $probe.stderr.Trim() })
    $x86Library = Join-Path $env:SystemRoot 'SysWOW64/kernel32.dll'
    if (Test-Path -LiteralPath $x86Library) {
        $probe = Wait-CleanProcess (Start-CleanProcess 'handbridge-engine.exe' @('--check-leapc', '--leapc-library', $x86Library))
        if ($probe.exit -eq 0 -or $probe.stderr -notmatch 'Windows error 193') { throw "Wrong-architecture DLL was not rejected as error 193: $($probe.stderr)" }
        $records.Add([ordered]@{ check = 'Actual x86 DLL rejected by x64 loader'; result = 'PASS'; diagnostic = $probe.stderr.Trim() })
    }
    if ($ExternalLeapCLibrary) {
        if ($ExpectedLeapCVersion -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw 'External SDK probes require its exact numeric file version from the verified SDK baseline manifest.' }
        $ExternalLeapCLibrary = (Resolve-Path -LiteralPath $ExternalLeapCLibrary).Path
        $probe = Wait-CleanProcess (Start-CleanProcess 'handbridge-engine.exe' @('--check-leapc', '--leapc-library', $ExternalLeapCLibrary))
        if ($probe.exit -ne 0) { throw "Real external SDK load failed: $($probe.stderr)" }
        $sdk = $probe.stdout | ConvertFrom-Json
        if ($sdk.version -cne $ExpectedLeapCVersion -or $sdk.hardware_verified) { throw "Incorrect real SDK probe: $($probe.stdout)" }
        $records.Add([ordered]@{ check = 'External genuine LeapC runtime load/version/exports only'; result = 'PASS'; probe = $sdk })
        # This is a temporary development-only copy outside the packaged tree.
        # Only *.txt/*.png/report evidence is uploaded; no SDK DLL is published.
        $unicodeDirectory = Join-Path $runDirectory 'runtime probe – caméra'
        New-Item -ItemType Directory -Path $unicodeDirectory | Out-Null
        $unicodeLibrary = Join-Path $unicodeDirectory 'LeapC.dll'
        Copy-Item -LiteralPath $ExternalLeapCLibrary -Destination $unicodeLibrary
        $probe = Wait-CleanProcess (Start-CleanProcess 'handbridge-engine.exe' @('--check-leapc', '--leapc-library', $unicodeLibrary))
        if ($probe.exit -ne 0) { throw "Runtime path containing spaces/Unicode failed: $($probe.stderr)" }
        $sdk = $probe.stdout | ConvertFrom-Json
        if ($sdk.version -cne $ExpectedLeapCVersion -or $sdk.library -cne $unicodeLibrary -or $sdk.hardware_verified) {
            throw "UTF-8 library path/version did not round-trip: $($probe.stdout)"
        }
        $records.Add([ordered]@{ check = 'Actual SDK absolute path with spaces/Unicode'; result = 'PASS'; probe = $sdk })
        # Exercise genuine SDK Create/Open/Poll/retry/cleanup with the tracking
        # service intentionally absent. This is still not a hardware test.
        $offlineEndpoint = 'handbridge-sdk-offline-' + [guid]::NewGuid().ToString('N')
        $offline = Wait-CleanProcess (Start-CleanProcess 'handbridge-engine.exe' @('--headless',
            '--leapc-library', $ExternalLeapCLibrary, '--endpoint', $offlineEndpoint,
            '--state-dir', (Join-Path $runDirectory 'sdk-no-service-state'), '--port', "$port", '--duration-ms', '3600')) 15000
        if ($offline.exit -ne 0) { throw "Real SDK offline-host lifecycle failed: $($offline.stderr)" }
        $offlineSummary = ($offline.stdout.Trim() -split '\r?\n')[-1] | ConvertFrom-Json
        if ($offlineSummary.source -cne 'leapc' -or $offlineSummary.simulated -or $offlineSummary.hardware_verified -or
            $offlineSummary.service_connected -or $offlineSummary.tracking.library_version -cne $ExpectedLeapCVersion -or
            $offlineSummary.tracking.connection_attempts -lt 1 -or @($offlineSummary.devices.PSObject.Properties).Count -ne 0 -or
            $offlineSummary.output.packets_sent -ne 0 -or [string]::IsNullOrWhiteSpace($offlineSummary.tracking.last_error)) {
            throw "Genuine SDK offline state claimed an invalid health/frame/output result: $($offline.stdout)"
        }
        if ($udp.Available -ne 0) { throw 'Real SDK without the service unexpectedly produced OSC datagrams.' }
        $records.Add([ordered]@{ check = 'Genuine SDK offline Create/Open/Poll/recovery/cleanup without service';
            result = 'PASS'; duration_ms = 3600; deadline_ms = 15000; summary = $offlineSummary })
    }
    $endpoint = 'handbridge-package-' + [guid]::NewGuid().ToString('N')
    $engine = Start-CleanProcess 'handbridge-engine.exe' @('--simulate', '--headless', '--endpoint', $endpoint,
        '--state-dir', (Join-Path $runDirectory 'state'), '--port', "$port", '--duration-ms', '45000')
    Assert-FreshOsc 'Headless OSC before GUI'
    Assert-LocalModules $engine $false
    $screenshot = Join-Path $OutputDirectory 'native-windows-overview.png'
    if (Test-Path -LiteralPath $screenshot) { Remove-Item -LiteralPath $screenshot }
    $gui = Start-CleanProcess 'handbridge-control.exe' @('--endpoint', $endpoint, '--screenshot', $screenshot, '--exit-after-ms', '5500')
    [Threading.Thread]::Sleep(2000)
    Assert-LocalModules $gui $true
    $closed = Wait-CleanProcess $gui
    if ($closed.exit -ne 0 -or $closed.stderr -match 'failed to load|is not installed|Could not load') { throw "Packaged GUI failed: $($closed | ConvertTo-Json -Compress)" }
    Assert-Screenshot $screenshot
    Assert-FreshOsc 'OSC after normal GUI close'
    $gui = Start-CleanProcess 'handbridge-control.exe' @('--endpoint', $endpoint)
    [Threading.Thread]::Sleep(1500)
    if ($gui.Process.HasExited) { throw 'Reopened packaged GUI exited unexpectedly.' }
    $gui.Process.Kill($true)
    [void](Wait-CleanProcess $gui)
    Assert-FreshOsc 'OSC after forced GUI termination'
    $processingScreenshot = Join-Path $OutputDirectory 'native-windows-processing.png'
    if (Test-Path -LiteralPath $processingScreenshot) { Remove-Item -LiteralPath $processingScreenshot }
    $gui = Start-CleanProcess 'handbridge-control.exe' @('--endpoint', $endpoint, '--page', 'processing',
        '--screenshot', $processingScreenshot, '--exit-after-ms', '3500')
    $reopened = Wait-CleanProcess $gui
    if ($reopened.exit -ne 0) { throw "Second GUI reopen failed: $($reopened.stderr)" }
    Assert-Screenshot $processingScreenshot
    Assert-FreshOsc 'OSC after GUI reopen and second normal close'
    if ($engine.Process.HasExited) { throw 'Engine did not survive independent GUI lifecycles.' }
    $records.Add([ordered]@{ check = 'No development search paths'; result = 'PASS'; path = $cleanPath;
        working_directory = $runDirectory; platform_plugin = 'windows'; qt_quick_backend = 'software' })
    $report = [ordered]@{ result = 'PASS'; source_revision = $manifest.source_revision; hardware_verified = $false;
        actual_windows_processes = $true; extracted_zip = $Archive; checks = $records }
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'portable-smoke.json') -Encoding utf8NoBOM
    Write-Host ($report | ConvertTo-Json -Depth 12)
} catch {
    [ordered]@{ result = 'FAIL'; source_revision = $manifest.source_revision; hardware_verified = $false;
        error = $_.Exception.ToString(); checks_completed = $records } | ConvertTo-Json -Depth 12 |
        Set-Content -LiteralPath (Join-Path $OutputDirectory 'portable-smoke.json') -Encoding utf8NoBOM
    throw
} finally {
    foreach ($record in $processes) {
        if (-not $record.Process.HasExited) { $record.Process.Kill($true); [void]$record.Process.WaitForExit(5000) }
        $result = Wait-CleanProcess $record
        $safeName = "$($record.Name)-$($record.Process.Id)"
        $result.stdout | Set-Content -LiteralPath (Join-Path $OutputDirectory "$safeName.stdout.txt")
        $result.stderr | Set-Content -LiteralPath (Join-Path $OutputDirectory "$safeName.stderr.txt")
        "ExitCode=$($result.exit)" | Set-Content -LiteralPath (Join-Path $OutputDirectory "$safeName.exit.txt")
        $record.Process.Dispose()
    }
    $udp.Dispose()
}
