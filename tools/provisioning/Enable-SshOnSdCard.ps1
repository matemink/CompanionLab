[CmdletBinding()]
param(
    [int] $DiskNumber = 2,

    [int] $PartitionNumber = 1,

    [char] $DriveLetter = 'R',

    [Parameter(Mandatory)]
    [string] $ExpectedSerialNumber,

    [double] $MinimumDiskSizeGB = 55,

    [double] $MaximumDiskSizeGB = 60
)

$ErrorActionPreference = 'Stop'

$logPath = Join-Path $PSScriptRoot 'enable-ssh-status.log'
trap {
    $_ | Out-String | Set-Content -LiteralPath $logPath
    exit 1
}

$disk = Get-Disk -Number $DiskNumber
if ($disk.BusType -ne 'USB' -or
    $disk.SerialNumber.Trim() -ne $ExpectedSerialNumber -or
    $disk.Size -lt ($MinimumDiskSizeGB * 1GB) -or
    $disk.Size -gt ($MaximumDiskSizeGB * 1GB)) {
    throw "The expected USB SD card was not found as Disk $DiskNumber."
}

$partition = Get-Partition -DiskNumber $DiskNumber -PartitionNumber $PartitionNumber
if ($partition.Size -lt 500MB -or $partition.Size -gt 600MB) {
    throw 'The expected Raspberry Pi boot partition was not found.'
}

if (Get-PSDrive -Name $DriveLetter -ErrorAction SilentlyContinue) {
    throw "Drive letter $DriveLetter`: is already in use."
}

Set-Partition `
    -DiskNumber $DiskNumber `
    -PartitionNumber $PartitionNumber `
    -NewDriveLetter $DriveLetter

$bootRoot = "$DriveLetter`:\"
if (-not (Test-Path (Join-Path $bootRoot 'config.txt')) -or
    -not (Test-Path (Join-Path $bootRoot 'cmdline.txt'))) {
    throw 'The mounted partition is not a Raspberry Pi boot partition.'
}

New-Item -ItemType File -Path (Join-Path $bootRoot 'ssh') -Force | Out-Null
"SSH marker created at $bootRoot`ssh" | Set-Content -LiteralPath $logPath
