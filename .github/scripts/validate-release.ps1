$ErrorActionPreference = 'Stop'

if ($env:PR_HEAD_BRANCH -ne 'dev' -or $env:PR_HEAD_REPO -ne $env:GITHUB_REPOSITORY) {
  throw 'Release PRs into main must come from this repository dev branch'
}
git fetch --no-tags --depth=1 origin $env:PR_BASE_SHA
if ($LASTEXITCODE -ne 0) { throw 'Cannot fetch the release baseline' }
function Get-ReleaseVersion([string]$text) {
  $versionMatch = [regex]::Match($text, 'set\(VERSION "([0-9]+\.[0-9]+\.[0-9]+)"\)')
  if (-not $versionMatch.Success) { throw 'Expected a stable numeric release version' }
  return [version]$versionMatch.Groups[1].Value
}
$baseText = (git show "$($env:PR_BASE_SHA):src/ver/CMakeLists.txt") -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Cannot read the release baseline' }
$baseVersion = Get-ReleaseVersion $baseText
$releaseVersion = Get-ReleaseVersion (Get-Content 'src/ver/CMakeLists.txt' -Raw)
if ($releaseVersion -le $baseVersion) { throw 'A release must increase the version in src/ver/CMakeLists.txt' }
