cmake -S . -B build/win32-vs2019 -G "Visual Studio 16 2019" -A Win32 -DXIDI_SDL3_ROOT="./ThirdParty/SDL3-VC" -DXIDI_FALLBACK_VERSION="4.3.1"
cmake --build build/win32-vs2019 --config Release
pause