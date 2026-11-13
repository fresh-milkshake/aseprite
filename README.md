# Aseprite Java Edition

Windows x64 fork with Minecraft Java tools, live previews, a recent-file grid,
linear Undo/Redo, a dark default theme and a bundled Russian interface.
Minecraft tools are in **Minecraft**, immediately after **Help**.

[Download Windows releases](https://github.com/fresh-milkshake/aseprite/releases).
Extract the ZIP and keep `aseprite.exe` beside its `data` folder.
Select Russian in **Edit > Preferences > General > Language**.

## Development

Develop in `dev` (`git switch dev`). Release PRs into `main` require a higher
stable version and passing Windows checks; merge them with squash.

## Build

Clone with submodules: `git clone --recursive https://github.com/fresh-milkshake/aseprite.git`.
On Windows, install Visual Studio 2022 C++ tools, Git for Windows, CMake and Ninja,
then run `build.cmd --auto --norun`.

The [Windows workflow](https://github.com/fresh-milkshake/aseprite/actions/workflows/windows-release.yml)
contains the release configuration, pinned Skia dependency and validation steps.
Stable versions use `UPDATE_VERSION_WITH_GIT=OFF`.
The Skia backend is required and supplies FreeType, HarfBuzz and WebP.
Upstream news, update checks and Steam integration are omitted.

## Notices

Aseprite was created by David Capello and is maintained by Igara Studio and
[contributors](docs/LICENSES.md). This fork is maintained by fresh-milkshake.
See [EULA.txt](EULA.txt), [third-party notices](docs/LICENSES.md), and the license
files in individual modules and assets. Translation credits are
included in the notices document and available through **About > Translators**.
