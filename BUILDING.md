# Building Xidi with CMake

This repository builds with CMake only. The original Visual Studio solution and project files are
still available in the first commit of the Git history (`Xidi.sln`, `*.vcxproj`, `Properties/`).
`Modules/Infra` keeps its own Visual Studio files.

## Requirements

- Windows, with Visual Studio 2022 (MSVC, C++23 / `/std:c++latest`) or clang-cl
- CMake 3.21 or newer
- SDL3 development files (see below)

## SDL3

SDL3 is not vendored as a library (only its headers are, in `ThirdParty/SDL3`). Give CMake one of:

| Option | Use when |
| --- | --- |
| `-DXIDI_SDL3_ROOT=C:/path/SDL3-3.x.y` | You extracted an `SDL3-devel-*-VC.zip` from the SDL releases page. Expects `include/` and `lib/x64`, `lib/x86`. |
| `-DCMAKE_PREFIX_PATH=...` or `-DSDL3_DIR=...` | You have SDL3 installed (for example `vcpkg install sdl3:x64-windows`). |
| `-DXIDI_SDL3_LIBRARY=C:/path/SDL3.lib` | You only have an import library; the headers in `ThirdParty/SDL3` are used. |
| `-DXIDI_FETCH_SDL3=ON` | Download SDL3 and link it statically, so no `SDL3.dll` needs shipping. |

When a shared SDL3 is used, `SDL3.dll` is copied next to the built DLLs if it can be found.

## Build

Each architecture is a separate build tree, because the proxy DLLs must match the game's bitness.

    cmake -S . -B build/x64 -A x64   -DXIDI_SDL3_ROOT=C:/SDL3-3.4.0
    cmake --build build/x64 --config Release

    cmake -S . -B build/x86 -A Win32 -DXIDI_SDL3_ROOT=C:/SDL3-3.4.0
    cmake --build build/x86 --config Release

`CMakePresets.json` has `vs2022-x64` and `vs2022-x86` presets that set the generator and
architecture for you (`cmake --preset vs2022-x64`).

Outputs land in `build/<arch>/bin/<Config>/`:

- `dinput.dll`, `dinput8.dll`, `winmm.dll`
- `Xidi.HookModule.32.dll` / `Xidi.HookModule.64.dll`
- `XidiTest.exe`, `CoreInfraTest.exe`

## Options

- `XIDI_BUILD_TESTS` (ON): build the test executables and register them with CTest
  (`ctest --test-dir build/x64 -C Release`)
- `XIDI_BUILD_HOOKMODULE` (ON): build the Hookshot hook module
- `XIDI_FALLBACK_VERSION` (`4.3.1`): version to report when the Git history has no `vX.Y.Z` tag

## Version information

`GitVersionInfo.generated.h` (used by `Globals.cpp` and the `.rc` files) is generated at build time
from Git by `cmake/GitVersionInfo.cmake`, replacing `Modules/Infra/Build/Scripts/GitVersionInfo.bat`.
Tag releases as `vMAJOR.MINOR.PATCH` to get clean version strings.
