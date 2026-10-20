<#
.SYNOPSIS
  Build the animview DLL and publish a release via the forge's REST API — no vendor
  CLI (gh/glab). Just HTTP, so it ports across forges by editing the settings block.

.DESCRIPTION
  Creates (or reuses) the tag's release on the repo and uploads animview.dll plus the
  libwebp notice. The token is read from $env:RELEASE_TOKEN so it never lives on disk.

.EXAMPLE
  # token from 1Password (fine-grained PAT, Contents: write on this repo)
  $env:RELEASE_TOKEN = op read "op://Private/dopus-tools release/token"
  ./release.ps1 -Tag v0.1.0 -Notes "Animated WebP + GIF viewer for Directory Opus."

.NOTES
  -SkipBuild uses the DLL already under out/build/x64-release. Needs Visual Studio to build.
#>
param(
  [Parameter(Mandatory)][string]$Tag,
  [string]$Name,
  [string]$Notes = "",
  [string]$NotesFile,
  [switch]$Draft,
  [switch]$Prerelease,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12  # PS 5.1 needs this for TLS 1.2

# ============================================================================
#  Forge settings — THIS is the only block that changes to target another forge.
# ============================================================================
$ApiBase = 'https://api.github.com'
$Owner   = 'annoyances-studio'
$Repo    = 'dopus-tools'
$Headers = @{
  Authorization          = "Bearer $env:RELEASE_TOKEN"
  Accept                 = 'application/vnd.github+json'
  'X-GitHub-Api-Version' = '2022-11-28'
}
# GitLab:   $ApiBase='https://gitlab.com/api/v4'; address the project by URL-encoded
#           'group%2Frepo'; auth header is  'PRIVATE-TOKEN' = <PAT>  (not Bearer); and
#           assets are uploaded to /uploads first, then linked on the release (a bit
#           more work than GitHub's single upload_url).
# Forgejo/Gitea: $ApiBase='https://your.forge/api/v1'; auth 'Authorization' = "token <PAT>";
#           release create + asset upload endpoints mirror GitHub closely.
# ============================================================================

if (-not $env:RELEASE_TOKEN) {
  throw "Set `$env:RELEASE_TOKEN first (fine-grained PAT with Contents: write). " +
        "e.g.  `$env:RELEASE_TOKEN = op read 'op://Private/dopus-tools release/token'"
}

$root      = $PSScriptRoot
$pluginDir = Join-Path $root 'plugins\animview'
$dll       = Join-Path $pluginDir 'out\build\x64-release\opus\animview.dll'
$notice    = Join-Path $pluginDir 'THIRD-PARTY.md'

# ---- Build (x64 Release) ----------------------------------------------------
if (-not $SkipBuild) {
  Write-Host "Building x64 Release..." -ForegroundColor Cyan
  $vsw = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vsw)) { throw "vswhere not found — is Visual Studio installed?" }
  $vsPath = & $vsw -latest -property installationPath
  Import-Module (Join-Path $vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
  Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation `
    -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
  Push-Location $pluginDir
  try {
    cmake --preset x64-release | Out-Null
    cmake --build --preset x64-release
  } finally { Pop-Location }
}
if (-not (Test-Path $dll)) { throw "DLL not found at $dll — build it, or drop -SkipBuild." }

# ---- Notes / name -----------------------------------------------------------
if ($NotesFile) { $Notes = Get-Content -Raw $NotesFile }
if (-not $Name) { $Name = "animview $Tag" }

# ---- Create the release (creates the tag at main if it doesn't exist) -------
Write-Host "Creating release $Tag on $Owner/$Repo..." -ForegroundColor Cyan
$payload = @{
  tag_name         = $Tag
  target_commitish = 'main'
  name             = $Name
  body             = $Notes
  draft            = [bool]$Draft
  prerelease       = [bool]$Prerelease
} | ConvertTo-Json
$release = Invoke-RestMethod -Method Post -Uri "$ApiBase/repos/$Owner/$Repo/releases" `
  -Headers $Headers -Body $payload -ContentType 'application/json'
Write-Host "  $($release.html_url)"

# ---- Upload assets ----------------------------------------------------------
# GitHub returns upload_url as a URI template ".../assets{?name,label}"; strip the template.
$uploadBase = $release.upload_url -replace '\{\?.*\}$', ''
function Publish-Asset([string]$Path, [string]$ContentType) {
  $fname = Split-Path -Leaf $Path
  Write-Host "Uploading $fname..." -ForegroundColor Cyan
  Invoke-RestMethod -Method Post -Uri "$uploadBase?name=$fname" -Headers $Headers `
    -InFile $Path -ContentType $ContentType | Out-Null
}
Publish-Asset $dll 'application/octet-stream'
if (Test-Path $notice) { Publish-Asset $notice 'text/markdown' }

Write-Host "Done: $($release.html_url)" -ForegroundColor Green
