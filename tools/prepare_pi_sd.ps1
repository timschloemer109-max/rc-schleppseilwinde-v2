param(
    [string]$DriveLetter = "E",
    [int]$DiskNumber = -1,
    [Parameter(Mandatory = $true)]
    [string]$WifiSsid,
    [Parameter(Mandatory = $true)]
    [string]$WifiPassword,
    [string]$SecondaryWifiSsid = "",
    [string]$SecondaryWifiPassword = "",
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
    $folders = @([regex]::Matches($indexHtml, $folderPattern) | ForEach-Object { $_.Groups[1].Value } | Sort-Object)
    if (-not $folders) {
        throw "Kein Raspberry-Pi-OS-Lite-Arm64-Ordner gefunden."
    }

    $latestFolder = $folders[-1]
    $folderUrl = "$baseUrl$latestFolder"
    $folderHtml = (Invoke-WebRequest -Uri $folderUrl -UseBasicParsing).Content
    $images = @([regex]::Matches($folderHtml, $imagePattern) | ForEach-Object { $_.Groups[1].Value } | Sort-Object)
    if (-not $images) {
        throw "Kein .img.xz-Image im Ordner $latestFolder gefunden."
    }

    return "$folderUrl$($images[-1])"
}

function Get-PhysicalDrivePath {
    param(
        [string]$DriveLetter,
        [int]$DiskNumber
    )

    if ($DiskNumber -ge 0) {
        $disk = Get-Disk -Number $DiskNumber
    }
    else {
        $partition = Get-Partition -DriveLetter $DriveLetter
        if (-not $partition) {
            throw "Laufwerk $DriveLetter`: nicht gefunden."
        }

        $disk = Get-Disk -Number $partition.DiskNumber
    }

    if (-not $disk) {
        throw "Zieldatentraeger nicht gefunden."
    }
    if ($disk.IsBoot -or $disk.IsSystem) {
        throw "Zieldatentraeger $($disk.Number) ist ein Boot- oder Systemdatentraeger."
    }
    if ($disk.BusType -ne "USB") {
        throw "Zieldatentraeger $($disk.Number) ist kein USB-Datentraeger."
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
        [string]$SecondaryWifiSsid,
        [string]$SecondaryWifiPassword,
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
enable_ssh: true
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

    $secondaryWifi = ""
    if (-not [string]::IsNullOrWhiteSpace($SecondaryWifiSsid)) {
        if ([string]::IsNullOrWhiteSpace($SecondaryWifiPassword)) {
            throw "Fuer das zweite WLAN fehlt das Passwort."
        }

        $secondaryWifi = @"
      "$SecondaryWifiSsid":
        password: "$SecondaryWifiPassword"
"@
    }

    $networkConfig = @"
version: 2
wifis:
  wlan0:
    dhcp4: true
    optional: true
    access-points:
      "$WifiSsid":
        password: "$WifiPassword"
$secondaryWifi
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

$imagerExe = "C:\Program Files\Raspberry Pi Imager\rpi-imager.exe"
if (-not (Test-Path -LiteralPath $imagerExe)) {
    throw "Raspberry Pi Imager nicht gefunden: $imagerExe"
}

$drivePath = Get-PhysicalDrivePath -DriveLetter $DriveLetter -DiskNumber $DiskNumber
$imageUrl = Get-LatestRpiOsLiteImageUrl
$sshPublicKey = Get-OptionalSshKey -Path $SshPublicKeyPath
$tempDir = Join-Path $env:TEMP "seilwinde-pi-sd"
$imagePath = Join-Path $tempDir ([System.IO.Path]::GetFileName($imageUrl))
$imageDownloadPath = "$imagePath.download"
$cloudInitFiles = New-CloudInitFiles `
    -TargetDirectory $tempDir `
    -Hostname $Hostname `
    -PiUser $PiUser `
    -PiPassword $PiPassword `
    -Timezone $Timezone `
    -WifiSsid $WifiSsid `
    -WifiPassword $WifiPassword `
    -SecondaryWifiSsid $SecondaryWifiSsid `
    -SecondaryWifiPassword $SecondaryWifiPassword `
    -SshPublicKey $sshPublicKey

Write-Host "Image URL: $imageUrl"
Write-Host "Lokales Image: $imagePath"
Write-Host "Zielgeraet: $drivePath"
Write-Host "Cloud-Init-Verzeichnis: $tempDir"

if (-not (Test-Path -LiteralPath $imagePath)) {
    Remove-Item -LiteralPath $imageDownloadPath -Force -ErrorAction SilentlyContinue
    Write-Host "Raspberry Pi OS Lite wird heruntergeladen ..."
    Invoke-WebRequest -Uri $imageUrl -OutFile $imageDownloadPath -UseBasicParsing
    Move-Item -LiteralPath $imageDownloadPath -Destination $imagePath -Force
}
else {
    Write-Host "Vorhandenes lokales Image wird verwendet."
}

$hashResponse = Invoke-WebRequest -Uri "$imageUrl.sha256" -UseBasicParsing
$hashText = if ($hashResponse.Content -is [byte[]]) {
    [System.Text.Encoding]::UTF8.GetString($hashResponse.Content)
}
else {
    [string]$hashResponse.Content
}
$expectedHash = ([regex]::Match($hashText, '^[0-9a-fA-F]{64}')).Value
$actualHash = (Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash
if ([string]::IsNullOrWhiteSpace($expectedHash) -or $actualHash -ne $expectedHash) {
    throw "SHA-256-Pruefung des Raspberry-Pi-OS-Images fehlgeschlagen."
}
Write-Host "SHA-256-Pruefung erfolgreich."

$imagerArgs = @(
    "--cli"
    "--disable-verify"
    "--cloudinit-userdata"
    $cloudInitFiles.UserDataPath
    "--cloudinit-networkconfig"
    $cloudInitFiles.NetworkConfigPath
    $imagePath
    $drivePath
)
$imagerStdout = Join-Path $env:TEMP "seilwinde-rpi-imager.stdout.log"
$imagerStderr = Join-Path $env:TEMP "seilwinde-rpi-imager.stderr.log"
Remove-Item -LiteralPath $imagerStdout, $imagerStderr -Force -ErrorAction SilentlyContinue
$imagerProcess = Start-Process `
    -FilePath $imagerExe `
    -ArgumentList $imagerArgs `
    -Wait `
    -PassThru `
    -NoNewWindow `
    -RedirectStandardOutput $imagerStdout `
    -RedirectStandardError $imagerStderr
if ($imagerProcess.ExitCode -ne 0) {
    $imagerOutput = @(
        Get-Content -LiteralPath $imagerStdout -ErrorAction SilentlyContinue
        Get-Content -LiteralPath $imagerStderr -ErrorAction SilentlyContinue
    ) -join [Environment]::NewLine
    throw "Raspberry Pi Imager wurde mit Exitcode $($imagerProcess.ExitCode) beendet.$([Environment]::NewLine)$imagerOutput"
}
