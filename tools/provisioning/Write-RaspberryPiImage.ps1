[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $ImagePath,

    [Parameter(Mandatory)]
    [string] $FirstBootScript,

    [int] $DiskNumber = 2,

    [Parameter(Mandatory)]
    [string] $ExpectedSerialNumber,

    [Parameter(Mandatory)]
    [double] $ExpectedSizeGB
)

$ErrorActionPreference = 'Stop'

$principal = [Security.Principal.WindowsPrincipal]::new(
    [Security.Principal.WindowsIdentity]::GetCurrent()
)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'This script must run as Administrator.'
}

$image = (Resolve-Path -LiteralPath $ImagePath).Path
$firstBoot = (Resolve-Path -LiteralPath $FirstBootScript).Path
$imager = 'C:\Program Files\Raspberry Pi Ltd\Imager\rpi-imager.exe'
if (-not (Test-Path -LiteralPath $imager)) {
    throw "Raspberry Pi Imager CLI was not found at $imager"
}

$disk = Get-Disk -Number $DiskNumber
$actualSizeGB = [math]::Round($disk.Size / 1GB, 2)
if ([math]::Abs($actualSizeGB - $ExpectedSizeGB) -gt 0.1) {
    throw "Safety check failed: disk $DiskNumber is $actualSizeGB GB, expected $ExpectedSizeGB GB."
}

$actualSerialNumber = $disk.SerialNumber.Trim()
if ($actualSerialNumber -ne $ExpectedSerialNumber) {
    throw "Safety check failed: disk $DiskNumber has serial '$actualSerialNumber'."
}

$protectedPartition = Get-Partition -DriveLetter G -ErrorAction SilentlyContinue
if ($protectedPartition -and $protectedPartition.DiskNumber -eq $DiskNumber) {
    throw 'Safety check failed: G: unexpectedly belongs to the target disk.'
}

$target = "\\.\PhysicalDrive$DiskNumber"
$outputDirectory = Split-Path -Parent $PSCommandPath
$logPath = Join-Path $outputDirectory 'imager-write.log'
$errorLogPath = Join-Path $outputDirectory 'imager-write-error.log'
$statusPath = Join-Path $outputDirectory 'imager-write-status.json'

Remove-Item -LiteralPath $logPath, $errorLogPath, $statusPath -Force -ErrorAction SilentlyContinue

$startedAt = Get-Date
$arguments = @(
    '--cli'
    '--enable-writing-system-drives'
    '--first-run-script'
    "`"$firstBoot`""
    '--disable-eject'
    "`"$image`""
    $target
)
$imagerProcess = Start-Process `
    -FilePath $imager `
    -ArgumentList $arguments `
    -WindowStyle Hidden `
    -RedirectStandardOutput $logPath `
    -RedirectStandardError $errorLogPath `
    -Wait `
    -PassThru
$exitCode = $imagerProcess.ExitCode

$status = [ordered]@{
    exitCode = $exitCode
    diskNumber = $DiskNumber
    target = $target
    image = $image
    startedAt = $startedAt.ToString('o')
    finishedAt = (Get-Date).ToString('o')
}
$status | ConvertTo-Json | Set-Content -LiteralPath $statusPath -Encoding UTF8

if ($exitCode -ne 0) {
    throw "Raspberry Pi Imager failed with exit code $exitCode. See $errorLogPath"
}
