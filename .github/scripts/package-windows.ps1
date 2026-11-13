$ErrorActionPreference = 'Stop'

$commit = (git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine build commit' }
$versionHeader = Get-Content 'build/src/ver/generated_version.h' -Raw
$versionMatch = [regex]::Match($versionHeader, '#define VERSION "([0-9]+\.[0-9]+\.[0-9]+)"')
if (-not $versionMatch.Success) { throw 'Expected a stable numeric release version' }
$version = $versionMatch.Groups[1].Value
if ($env:GITHUB_OUTPUT) { "version=$version" | Add-Content $env:GITHUB_OUTPUT }
$staging = Join-Path $env:RUNNER_TEMP 'aseprite-package'
$package = Join-Path $staging 'aseprite'
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force $package, 'dist' | Out-Null
Copy-Item 'build/bin/aseprite.exe' $package
Copy-Item 'build/bin/icudtl.dat' $package
Copy-Item 'build/bin/data' $package -Recurse
# Native extension preferences live next to the extension, outside the isolated
# GUI profile. Never ship settings written by tests or a previous build.
Get-ChildItem (Join-Path $package 'data/extensions') -Filter '__pref.lua' -File -Recurse | Remove-Item -Force
# The editable translation catalogue is used by source validation/generation only.
Remove-Item (Join-Path $package 'data/extensions/minecraft-tools/translations.json')
# Remove obsolete resource copies left by an incremental build.
foreach ($obsolete in @('AUTHORS.md', 'strings/README.md', 'strings/README.ru.md')) {
  Remove-Item (Join-Path (Join-Path $package 'data') $obsolete) -ErrorAction SilentlyContinue
}
Get-ChildItem 'build/bin' -Filter '*.dll' -File | Copy-Item -Destination $package
# License notices and credits are already included in data by copy_data.
@"
Source: https://github.com/fresh-milkshake/aseprite
Commit: $commit
Version: $version
Skia: $((Get-Content 'laf/misc/skia-tag.txt' -Raw).Trim())
Build: Release, Windows x64, MSVC, Lua enabled
"@ | Set-Content (Join-Path $package 'BUILD-INFO.txt') -Encoding utf8

$archive = Join-Path $PWD "dist/aseprite-v$version-windows-x64.zip"
Compress-Archive -Path $package -DestinationPath $archive -Force
$hash = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $(Split-Path $archive -Leaf)" | Set-Content "$archive.sha256" -Encoding ascii

# Run the extracted application, so missing resources or runtime DLLs
# fail the build before the artifact is uploaded.
$smoke = Join-Path $env:RUNNER_TEMP 'aseprite-package-smoke'
Expand-Archive $archive -DestinationPath $smoke -Force
if (Get-ChildItem (Join-Path $smoke 'aseprite/data/extensions') -Filter '__pref.lua' -File -Recurse) {
  throw 'Packaged extensions contain user/test preferences'
}
$env:ASEPRITE_USER_FOLDER = Join-Path $env:RUNNER_TEMP 'aseprite-fresh-settings'
New-Item -ItemType Directory -Force $env:ASEPRITE_USER_FOLDER | Out-Null
& "$smoke/aseprite/aseprite.exe" --batch --script-param "expected-version=$version" --script tests/scripts/fork_defaults.lua
if ($LASTEXITCODE -ne 0) { throw 'Packaged application smoke test failed' }

& "$smoke/aseprite/aseprite.exe" --batch --script tests/scripts/language.lua
if ($LASTEXITCODE -ne 0) { throw 'Packaged Russian language smoke test failed' }
