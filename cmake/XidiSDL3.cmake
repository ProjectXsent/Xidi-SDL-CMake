# Provides the imported target Xidi::SDL3 (headers + import library).
#
# Resolution order:
#   1. XIDI_FETCH_SDL3=ON : build SDL3 from source with FetchContent and link it statically.
#   2. XIDI_SDL3_ROOT     : an extracted SDL3 "VC" development package (include/, lib/x64, lib/x86).
#                           Headers come from the package so they match the library.
#   3. find_package(SDL3) : vcpkg, an installed SDL3, or SDL3_DIR pointing at SDL3Config.cmake.
#
# If none of these succeed, configuration stops with instructions. The headers bundled in
# ThirdParty/SDL3/Files/include are used only for the case where just an import library is given
# via XIDI_SDL3_LIBRARY.

set(XIDI_SDL3_ROOT "" CACHE PATH "Extracted SDL3 VC development package (contains include/ and lib/<arch>/)")
set(XIDI_SDL3_LIBRARY "" CACHE FILEPATH "Path to SDL3.lib (import library) matching the target architecture")
option(XIDI_FETCH_SDL3 "Download and statically link SDL3 instead of using a prebuilt SDL3.lib" OFF)
set(XIDI_SDL3_FETCH_TAG "release-3.4.0" CACHE STRING "SDL git tag used when XIDI_FETCH_SDL3 is ON")

set(_xidi_bundled_sdl3_include "${CMAKE_SOURCE_DIR}/ThirdParty/SDL3/Files/include")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_xidi_sdl3_arch x64)
else()
  set(_xidi_sdl3_arch x86)
endif()

add_library(Xidi_SDL3 INTERFACE)
add_library(Xidi::SDL3 ALIAS Xidi_SDL3)

if(XIDI_FETCH_SDL3)
  include(FetchContent)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG ${XIDI_SDL3_FETCH_TAG}
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
  target_link_libraries(Xidi_SDL3 INTERFACE SDL3::SDL3-static)
  set(XIDI_SDL3_DESCRIPTION "built from source (static), tag ${XIDI_SDL3_FETCH_TAG}")

elseif(XIDI_SDL3_ROOT)
  find_library(XIDI_SDL3_LIBRARY_FOUND SDL3
    HINTS "${XIDI_SDL3_ROOT}/lib/${_xidi_sdl3_arch}" "${XIDI_SDL3_ROOT}/lib"
    NO_DEFAULT_PATH)
  if(NOT XIDI_SDL3_LIBRARY_FOUND)
    message(FATAL_ERROR "SDL3.lib not found under ${XIDI_SDL3_ROOT}/lib/${_xidi_sdl3_arch}. "
                        "Is XIDI_SDL3_ROOT an SDL3 VC development package, and does it match the target architecture (${_xidi_sdl3_arch})?")
  endif()
  target_include_directories(Xidi_SDL3 INTERFACE "${XIDI_SDL3_ROOT}/include")
  target_link_libraries(Xidi_SDL3 INTERFACE "${XIDI_SDL3_LIBRARY_FOUND}")
  find_file(XIDI_SDL3_DLL SDL3.dll HINTS "${XIDI_SDL3_ROOT}/lib/${_xidi_sdl3_arch}" NO_DEFAULT_PATH)
  set(XIDI_SDL3_DESCRIPTION "${XIDI_SDL3_ROOT} (${XIDI_SDL3_LIBRARY_FOUND})")

elseif(XIDI_SDL3_LIBRARY)
  target_include_directories(Xidi_SDL3 INTERFACE "${_xidi_bundled_sdl3_include}")
  target_link_libraries(Xidi_SDL3 INTERFACE "${XIDI_SDL3_LIBRARY}")
  get_filename_component(_xidi_sdl3_lib_dir "${XIDI_SDL3_LIBRARY}" DIRECTORY)
  find_file(XIDI_SDL3_DLL SDL3.dll HINTS "${_xidi_sdl3_lib_dir}" NO_DEFAULT_PATH)
  set(XIDI_SDL3_DESCRIPTION "${XIDI_SDL3_LIBRARY} with bundled headers")

else()
  find_package(SDL3 CONFIG QUIET)
  if(TARGET SDL3::SDL3)
    target_link_libraries(Xidi_SDL3 INTERFACE SDL3::SDL3)
    set(XIDI_SDL3_DESCRIPTION "find_package(SDL3) ${SDL3_VERSION}")
  else()
    message(FATAL_ERROR
      "SDL3 was not found. Do one of the following:\n"
      "  -DXIDI_SDL3_ROOT=<path to extracted SDL3-devel-*-VC package>\n"
      "  -DSDL3_DIR=<dir containing SDL3Config.cmake>  (or use vcpkg: vcpkg install sdl3)\n"
      "  -DXIDI_SDL3_LIBRARY=<path to SDL3.lib>        (uses the headers in ThirdParty/SDL3)\n"
      "  -DXIDI_FETCH_SDL3=ON                          (download and build SDL3 statically)")
  endif()
endif()

message(STATUS "SDL3: ${XIDI_SDL3_DESCRIPTION}")

# Include-only variant for targets that need SDL headers but do not call into SDL (HookModule).
add_library(Xidi_SDL3_Headers INTERFACE)
get_target_property(_xidi_sdl3_incs Xidi_SDL3 INTERFACE_INCLUDE_DIRECTORIES)
if(NOT _xidi_sdl3_incs)
  if(TARGET SDL3::SDL3)
    get_target_property(_xidi_sdl3_incs SDL3::SDL3 INTERFACE_INCLUDE_DIRECTORIES)
  elseif(TARGET SDL3::SDL3-static)
    get_target_property(_xidi_sdl3_incs SDL3::SDL3-static INTERFACE_INCLUDE_DIRECTORIES)
  endif()
endif()
if(_xidi_sdl3_incs)
  target_include_directories(Xidi_SDL3_Headers INTERFACE ${_xidi_sdl3_incs})
else()
  target_include_directories(Xidi_SDL3_Headers INTERFACE "${_xidi_bundled_sdl3_include}")
endif()
