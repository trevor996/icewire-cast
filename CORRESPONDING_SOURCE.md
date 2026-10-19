# ICEWIRE CAST 1.2.0 corresponding source

This archive contains the source code used to build ICEWIRE CAST 1.2.0 for
Windows and macOS, including ICEWIRE modifications to OBS Studio, build
configuration, packaging scripts, and the source snapshots used for bundled
components.

ICEWIRE CAST is based on OBS Studio 32.1.2 and is distributed under the GNU
General Public License, version 2 or (at your option) any later version. See
`COPYING` and `UPSTREAM.md`.

## Rebuilding on Windows

1. Install Visual Studio 2022 with Desktop development with C++, CMake 3.28 or
   newer, Git, and Inno Setup 6.
2. From an x64 Native Tools command prompt, configure the Windows x64 build
   with the `windows-x64` CMake preset in `CMakePresets.json`.
3. Build the `RelWithDebInfo` configuration.
4. Build the installer with `installer/IWCCast.iss`. The installer expects the
   staged application under `build_iwc_x64/rundir/RelWithDebInfo`.

OBS build dependencies are resolved by the included CMake configuration. The
three `vendor-*.zip` files are source snapshots for bundled OBS components.
Generated build directories, compiler caches, previously built installers, and
downloaded binary dependency caches are intentionally not included because
they are not source code.

## Rebuilding on macOS

1. Install Xcode 16 or newer, CMake 3.28 or newer, Git, and Homebrew.
2. Configure with `cmake --preset macos -DIWC_APP_VERSION=1.2.0` on either an
   Apple Silicon or Intel Mac.
3. Build with `cmake --build build_macos --config RelWithDebInfo`.
4. Package with `.github/scripts/package-macos --target macos-arm64 --config
   RelWithDebInfo --package` (use `macos-x86_64` for Intel).

Public distribution requires an Apple Developer ID Application certificate and
Apple notarization. Development builds can be produced without those secrets,
but macOS will show additional Gatekeeper warnings.

For the upstream project and general build documentation, see:

- https://github.com/obsproject/obs-studio
- https://github.com/obsproject/obs-studio/wiki/Build-Instructions-For-Windows
