param(
    [string]$DriveLetter = "E",
    [Parameter(Mandatory = $true)]
    [string]$WifiSsid,
    [Parameter(Mandatory = $true)]
    [string]$WifiPassword,
    [Parameter(Mandatory = $true)]
    [string]$PiPassword,
    [string]$Hostname = "seilwinde-logger",
    [string]$PiUser = "seilwinde",
    [string]$Timezone = "Europe/Berlin",
    [string]$SshPublicKeyPath = ""
)

$ErrorActionPreference = "Stop"

function Get-LatestRpiOsLiteImageUrl {
    $baseUrl = "https://downloads.raspberrypi.com/raspios_lite_arm64/images/"
    $folderPattern = 'href="(raspios_lite_arm64-\d{4}-\d{2}-\d{2}/)"'
    $imagePattern = 'href="([^"]+\.img\.xz)"'

    $indexHtml = (Invoke-WebRequest -Uri $baseUrl -UseBasicParsing).Content
    $folders = [regex]::Matches($indexHtml, $folderPattern) | ForEach-Object { $_.Groups[1].Value } | Sort-Object
    if (-not $folders) {
        throw "Kein Raspberry-Pi-OS-Lite-Arm64-Ordner gefunden."
    }

    $latestFolder = $folders[-1]
    $folderUrl = "$baseUrl$latestFolder"
    $folderHtml = (Invoke-WebRequest -Uri $folderUrl -UseBasicParsing).Content
    $images = [regex]::Matches($folderHtml, $imagePattern) | ForEach-Object { $_.Groups[1].Value } | Sort-Object
    if (-not $images) {
        throw "Kein .img.xz-Image im Ordner $latestFolder gefunden."
    }

    return "$folderUrl$($images[-1])"
}

function Get-PhysicalDrivePath {
    param([string]$DriveLetter)

    $partition = Get-Partition -DriveLetter $DriveLetter
    if (-not $partition) {
        throw "Laufwerk $DriveLetter`: nicht gefunden."
    }

    $disk = Get-Disk -Number $partition.DiskNumber
    if (-not $disk) {
        throw "Kein Datentraeger zu Laufwerk $DriveLetter`: gefunden."
    }

    return "\\.\PhysicalDrive$($disk.Number)"
}

function Get-OptionalSshKey {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "SSH-Public-Key nicht gefunden: $Path"
    }

    return (Get-Content -LiteralPath $Path -Raw).Trim()
}

function New-CloudInitFiles {
    param(
        [string]$TargetDirectory,
        [string]$Hostname,
        [string]$PiUser,
        [string]$PiPassword,
        [string]$Timezone,
        [string]$WifiSsid,
        [string]$WifiPassword,
        [string]$SshPublicKey
    )

    New-Item -ItemType Directory -Path $TargetDirectory -Force | Out-Null

    $sshLines = ""
    $sshPwauth = "true"
    if (-not [string]::IsNullOrWhiteSpace($SshPublicKey)) {
        $sshLines = @"
    ssh_authorized_keys:
      - $SshPublicKey
"@
        $sshPwauth = "false"
    }

    $userData = @"
#cloud-config
hostname: $Hostname
manage_etc_hosts: true
timezone: $Timezone
ssh_pwauth: $sshPwauth
users:
  - name: $PiUser
    gecos: Seilwinde Logger
    groups: [adm, dialout, cdrom, sudo, audio, video, plugdev, users, netdev]
    shell: /bin/bash
    lock_passwd: false
$sshLines
chpasswd:
  expire: false
  list:
    - ${PiUser}:$PiPassword
"@

    $networkConfig = @"
version: 2
wifis:
  wlan0:
    dhcp4: true
    optional: true
    access-points:
      "$WifiSsid":
        password: "$WifiPassword"
"@

    $userDataPath = Join-Path $TargetDirectory "user-data"
    $networkConfigPath = Join-Path $TargetDirectory "network-config"
    Set-Content -LiteralPath $userDataPath -Value $userData -NoNewline
    Set-Content -LiteralPath $networkConfigPath -Value $networkConfig -NoNewline

    return @{
        UserDataPath = $userDataPath
        NetworkConfigPath = $networkConfigPath
    }
}

$imagerCli = "C:\Program Files\Raspberry Pi Imager\rpi-imager-cli.cmd"
if (-not (Test-Path -LiteralPath $imagerCli)) {
    throw "Raspberry Pi Imager CLI nicht gefunden: $imagerCli"
}

$drivePath = Get-PhysicalDrivePath -DriveLetter $DriveLetter
$imageUrl = Get-LatestRpiOsLiteImageUrl
$sshPublicKey = Get-OptionalSshKey -Path $SshPublicKeyPath
$tempDir = Join-Path $env:TEMP "seilwinde-pi-sd"
$cloudInitFiles = New-CloudInitFiles `
    -TargetDirectory $tempDir `
    -Hostname $Hostname `
    -PiUser $PiUser `
    -PiPassword $PiPassword `
    -Timezone $Timezone `
    -WifiSsid $WifiSsid `
    -WifiPassword $WifiPassword `
    -SshPublicKey $sshPublicKey

Write-Host "Image URL: $imageUrl"
Write-Host "Zielgeraet: $drivePath"
Write-Host "Cloud-Init-Verzeichnis: $tempDir"

& $imagerCli --disable-verify --cloudinit-userdata $cloudInitFiles.UserDataPath --cloudinit-networkconfig $cloudInitFiles.NetworkConfigPath $imageUrl $drivePath
