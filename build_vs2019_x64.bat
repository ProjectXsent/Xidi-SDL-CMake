cmake -S . -B build/x64-vs2019 -G "Visual Studio 16 2019" -A x64 -DXIDI_SDL3_ROOT="./ThirdParty/SDL3-VC" -DXIDI_FALLBACK_VERSION="4.3.1"
cmake --build build/x64-vs2019 --config Release
pause