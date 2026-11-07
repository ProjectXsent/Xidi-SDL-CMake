cmake -S . -B build/x64 -A x64   -DXIDI_SDL3_ROOT="./ThirdParty/SDL3-VC" -DXIDI_FALLBACK_VERSION="4.3.1"
cmake --build build/x64 --config Release
pause