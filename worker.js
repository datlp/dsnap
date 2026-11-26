const INSTALL_PS1 = `$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

Write-Host "==> Fetching latest dsnap release from GitHub..." -ForegroundColor Cyan

$apiUrl = "https://api.github.com/repos/datlp/dsnap/releases/latest"
$headers = @{ "User-Agent" = "dsnap-installer" }
$release = Invoke-RestMethod -Uri $apiUrl -Headers $headers

$tag = $release.tag_name
$asset = $release.assets | Where-Object { $_.name -match '^dsnap-v.*-setup\\.exe$' } | Select-Object -First 1
if (-not $asset) {
    throw "Could not find setup installer asset in release $tag"
}

$installerUrl = $asset.browser_download_url
$installerPath = Join-Path $env:TEMP $asset.name

Write-Host "==> Downloading $($asset.name) ($tag)..." -ForegroundColor Cyan
Invoke-WebRequest -Uri $installerUrl -OutFile $installerPath -UseBasicParsing

$shaAsset = $release.assets | Where-Object { $_.name -eq 'SHA256SUMS.txt' } | Select-Object -First 1
if ($shaAsset) {
    $sumsRaw = (Invoke-WebRequest -Uri $shaAsset.browser_download_url -UseBasicParsing).Content
    if ($sumsRaw -is [byte[]]) {
        $sumsRaw = [System.Text.Encoding]::UTF8.GetString($sumsRaw)
    }
    $expectedLine = ($sumsRaw -split "\`r?\`n") | Where-Object { $_ -match [regex]::Escape($asset.name) } | Select-Object -First 1
    if ($expectedLine) {
        $expectedHash = ($expectedLine.Trim() -split '\\s+')[0].ToUpperInvariant()
        $actualHash = (Get-FileHash -Path $installerPath -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($actualHash -ne $expectedHash) {
            Remove-Item $installerPath -Force -ErrorAction SilentlyContinue
            throw "SHA256 mismatch! Expected $expectedHash, got $actualHash"
        }
        Write-Host "==> SHA256 verified ($actualHash)" -ForegroundColor Green
    }
}

$innoArgs = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /ALLUSERS /TASKS="autostart,runasadmin"'
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

Write-Host "==> Installing dsnap $tag (Silent + Admin)..." -ForegroundColor Cyan
if ($isAdmin) {
    $proc = Start-Process -FilePath $installerPath -ArgumentList $innoArgs -Wait -PassThru
} else {
    Write-Host "    (Requesting Administrator privileges via UAC...)" -ForegroundColor Yellow
    $proc = Start-Process -FilePath $installerPath -ArgumentList $innoArgs -Verb RunAs -Wait -PassThru
}

Remove-Item $installerPath -Force -ErrorAction SilentlyContinue

if ($proc.ExitCode -ne 0) {
    throw "Installer exited with code $($proc.ExitCode)"
}

$exePath = Join-Path \${env:ProgramFiles} "dsnap\\dsnap.exe"
if (Test-Path $exePath) {
    Write-Host "==> Launching dsnap..." -ForegroundColor Green
    Start-Process -FilePath $exePath
}

Write-Host "==> dsnap $tag installed successfully! (PrtScn: Screenshot | Shift+PrtScn: Record)" -ForegroundColor Green
`;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const ua = request.headers.get("user-agent") || "";
    const accept = request.headers.get("accept") || "";

    const isCliClient = /powershell|pwsh|curl|wget/i.test(ua) && !accept.includes("text/html");

    if (url.pathname === "/install.ps1" || (url.pathname === "/" && isCliClient)) {
      return new Response(INSTALL_PS1, {
        headers: {
          "content-type": "text/plain; charset=utf-8",
          "cache-control": "no-store",
        },
      });
    }

    return env.ASSETS.fetch(request);
  },
};
