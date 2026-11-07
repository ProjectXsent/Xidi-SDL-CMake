cmake -S . -B build/Win32 -A Win32   -DXIDI_SDL3_ROOT="./ThirdParty/SDL3-VC" -DXIDI_FALLBACK_VERSION="4.3.1"
cmake --build build/Win32 --config Release
pause