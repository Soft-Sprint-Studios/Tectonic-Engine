$githubRepoUrl = "https://github.com/Soft-Sprint-Studios/Tectonic-Engine"
$githubToken = "ATJEOIM7VTQKQCFQCC3QZELIOPCFW"

Set-Location -Path $PSScriptRoot

$windowsRunnerDir = Join-Path $PWD "actions-runner-win"
$linuxRunnerDir = Join-Path $PWD "actions-runner-linux"
$linuxSetupScriptWinPath = Resolve-Path "./setup-linux-runner.sh"
$winPath = $linuxSetupScriptWinPath.Path
$driveLetter = $winPath.Substring(0, 1).ToLower()
$relativePath = $winPath.Substring(2) -replace "\\", "/"
$linuxSetupScript = "/mnt/$driveLetter/$relativePath"

function Setup-LinuxRunnerInWSL {
    if (!(Test-Path $linuxRunnerDir)) {
        New-Item -ItemType Directory -Path $linuxRunnerDir | Out-Null
    }

    $runnerNameLinux = "linux-runner-$env:COMPUTERNAME-$((Get-Date).ToString('yyyyMMddHHmmss'))"

    wsl bash -c "cd $linuxRunnerDir && if [ -f config.sh ]; then ./config.sh remove --token $githubToken || true; fi"
    wsl dos2unix "$linuxSetupScript"
    Start-Process "wsl.exe" -ArgumentList "bash -c '$linuxSetupScript $githubToken $githubRepoUrl $runnerNameLinux; exec bash'"
    Start-Process "wsl.exe" -ArgumentList "bash -c 'cd $linuxRunnerDir && ./run.sh'"
}

function Setup-WindowsRunner {
    if (!(Test-Path $windowsRunnerDir)) {
        New-Item -ItemType Directory -Path $windowsRunnerDir | Out-Null
    }

    Set-Location $windowsRunnerDir
    $runnerZip = Join-Path $windowsRunnerDir "actions-runner-win-x64-2.326.0.zip"

    if (!(Test-Path $runnerZip)) {
        Invoke-WebRequest -Uri "https://github.com/actions/runner/releases/download/v2.326.0/actions-runner-win-x64-2.326.0.zip" -OutFile $runnerZip
    }

    if (!(Test-Path "$windowsRunnerDir\config.cmd")) {
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::ExtractToDirectory($runnerZip, $windowsRunnerDir)
    }

    $runnerNameWindows = "windows-runner-$env:COMPUTERNAME-$((Get-Date).ToString('yyyyMMddHHmmss'))"

    try { & .\config.cmd remove --token $githubToken } catch {}
    Start-Sleep -Seconds 2

    & .\config.cmd --unattended --url $githubRepoUrl --token $githubToken --labels windows --name $runnerNameWindows
    Start-Process "cmd.exe" -ArgumentList "/k `"$windowsRunnerDir\run.cmd`""
}

Setup-LinuxRunnerInWSL
Setup-WindowsRunner

Write-Host "`n✅ Both GitHub runners started successfully!"
Write-Host "   - Linux runner is running in a visible WSL terminal"
Write-Host "   - Windows runner is running in a visible Command Prompt"
