cmake_minimum_required(VERSION 3.21)

project(Xidi
  VERSION 4.3.1
  DESCRIPTION "DirectInput interface for game controllers (SDL3 backend)"
  LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Xidi targets the Windows API and must be built for Windows "
                      "(MSVC or clang-cl; select a Windows toolchain).")
endif()
enable_language(RC)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(XIDI_BUILD_TESTS "Build the XidiTest and Infra test executables" ON)
option(XIDI_BUILD_HOOKMODULE "Build the Hookshot hook module (Xidi.HookModule.32/64.dll)" ON)
set(XIDI_FALLBACK_VERSION "${PROJECT_VERSION}" CACHE STRING
    "Version used when the Git history has no vMAJOR.MINOR.PATCH tag")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Match the MSVC project settings: dynamic CRT (/MT).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# One flat output folder per configuration; the DLLs are named after what they replace.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(XIDI_ARCH_BITS 64)
else()
  set(XIDI_ARCH_BITS 32)
endif()

# ---- Version information generated from Git -----------------------------------------------------

set(XIDI_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(XIDI_VERSION_HEADER "${XIDI_GENERATED_DIR}/GitVersionInfo.generated.h")

# Run once at configure time so that the header exists for IDE indexing...
execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DSRC_DIR=${CMAKE_SOURCE_DIR}" "-DOUT_FILE=${XIDI_VERSION_HEADER}"
          "-DFALLBACK_VERSION=${XIDI_FALLBACK_VERSION}" -P "${CMAKE_SOURCE_DIR}/cmake/GitVersionInfo.cmake")

# ...and again on every build so that it tracks commits (the file is only rewritten on change).
add_custom_target(XidiGitVersionInfo
  COMMAND "${CMAKE_COMMAND}" "-DSRC_DIR=${CMAKE_SOURCE_DIR}" "-DOUT_FILE=${XIDI_VERSION_HEADER}"
          "-DFALLBACK_VERSION=${XIDI_FALLBACK_VERSION}" "-DEXTRA=$<$<CONFIG:Debug>:debug>"
          -P "${CMAKE_SOURCE_DIR}/cmake/GitVersionInfo.cmake"
  BYPRODUCTS "${XIDI_VERSION_HEADER}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Generating Git version information"
  VERBATIM)

# ---- Common compile settings ---------------------------------------------------------------------

function(xidi_apply_common_settings tgt)
  target_compile_definitions(${tgt} PRIVATE UNICODE _UNICODE)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /W3 /permissive- /MP)
  endif()
  set_target_properties(${tgt} PROPERTIES
    MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
endfunction()

if(XIDI_BUILD_TESTS)
  enable_testing()
endif()

# ---- Dependencies --------------------------------------------------------------------------------

include(XidiSDL3)

add_library(Xidi_Boost INTERFACE)
target_include_directories(Xidi_Boost SYSTEM INTERFACE "${CMAKE_SOURCE_DIR}/ThirdParty/Boost/Files")

add_library(Xidi_XstdBitSet INTERFACE)
target_include_directories(Xidi_XstdBitSet SYSTEM INTERFACE
  "${CMAKE_SOURCE_DIR}/ThirdParty/XstdBitSet/Files/include")

add_library(Xidi_Hookshot INTERFACE)
target_include_directories(Xidi_Hookshot SYSTEM INTERFACE
  "${CMAKE_SOURCE_DIR}/ThirdParty/Hookshot/Files/Include")

add_subdirectory(Modules/Infra)

# ---- Xidi sources --------------------------------------------------------------------------------

# Sources compiled into every target except HookModule (proxy DLLs and unit tests).
set(XIDI_CORE_SOURCES
  Source/ApiDirectInput.cpp
  Source/ApiGUID.cpp
  Source/ApiXidi.cpp
  Source/ControllerIdentification.cpp
  Source/ControllerMath.cpp
  Source/DataFormat.cpp
  Source/ElementMapper.cpp
  Source/ForceFeedbackDevice.cpp
  Source/ForceFeedbackEffect.cpp
  Source/ForceFeedbackParameters.cpp
  Source/Globals.cpp
  Source/ImportApiWinMM.cpp
  Source/ImportApiXInput.cpp
  Source/Mapper.cpp
  Source/MapperBuilder.cpp
  Source/MapperDefinitions.cpp
  Source/MapperParser.cpp
  Source/StateChangeEventBuffer.cpp
  Source/Strings.cpp
  Source/VirtualController.cpp)

# Additionally needed by the real DLLs (not by the unit tests, which use mocks instead).
set(XIDI_DLL_SOURCES
  Source/DllMain.cpp
  Source/ImportApiDirectInput.cpp
  Source/Keyboard.cpp
  Source/Mouse.cpp
  Source/PhysicalController.cpp
  Source/XidiConfigReader.cpp)

# The virtual DirectInput device layer: DirectInput front ends and unit tests, but not WinMM.
set(XIDI_DIRECTINPUT_LAYER_SOURCES
  Source/VirtualDirectInputDevice.cpp
  Source/VirtualDirectInputEffect.cpp
  Source/WrapperIDirectInput.cpp)

function(xidi_configure tgt)
  target_include_directories(${tgt} PRIVATE
    "${CMAKE_SOURCE_DIR}/Include/Xidi"
    "${CMAKE_SOURCE_DIR}/Include/Xidi/Internal"
    "${XIDI_GENERATED_DIR}")
  # NOTE: the Resources folder must NOT be on the include path. It contains DInput.h and DInput8.h,
  # and Windows is case-insensitive, so <dinput.h> / <dinput8.h> would resolve to those files
  # instead of the Windows SDK headers. The original projects instead force-include the resource
  # ID headers by absolute path (see Modules/Infra/Build/Properties/NativeBuild.props).
  target_compile_options(${tgt} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_SOURCE_DIR}/Resources/Xidi.h>")
  if(EXISTS "${CMAKE_SOURCE_DIR}/Resources/${tgt}.h")
    target_compile_options(${tgt} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_SOURCE_DIR}/Resources/${tgt}.h>")
  endif()
  target_compile_definitions(${tgt} PRIVATE $<$<CONFIG:Debug>:_DEBUG>)
  add_dependencies(${tgt} XidiGitVersionInfo)
  xidi_apply_common_settings(${tgt})
endfunction()

# Copies SDL3.dll next to a target when a shared SDL3 is in use.
function(xidi_copy_sdl3_dll tgt)
  if(XIDI_SDL3_DLL)
    add_custom_command(TARGET ${tgt} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${XIDI_SDL3_DLL}" "$<TARGET_FILE_DIR:${tgt}>"
      VERBATIM)
  endif()
endfunction()

# Defines one of the proxy DLLs (dinput, dinput8, winmm). Extra sources are passed last.
function(xidi_add_proxy_dll tgt out_name rc_file def_file di_version)
  add_library(${tgt} SHARED ${XIDI_CORE_SOURCES} ${XIDI_DLL_SOURCES}
    Resources/Xidi.rc "${rc_file}" "${def_file}" ${ARGN})
  target_compile_definitions(${tgt} PRIVATE DIRECTINPUT_VERSION=${di_version} _USRDLL)
  target_link_libraries(${tgt} PRIVATE CoreInfra Xidi_SDL3 Xidi_Boost Xidi_XstdBitSet)
  set_target_properties(${tgt} PROPERTIES OUTPUT_NAME "${out_name}" PREFIX "")
  xidi_configure(${tgt})
  xidi_copy_sdl3_dll(${tgt})
endfunction()

set(XIDI_DIRECTINPUT_FRONTEND_SOURCES
  ${XIDI_DIRECTINPUT_LAYER_SOURCES}
  Source/DirectInputClassFactory.cpp
  Source/ExportApiDirectInput.cpp)

# dinput.dll: DirectInput 1-7.  dinput8.dll: DirectInput 8.  winmm.dll: joystick API.
xidi_add_proxy_dll(DInput  dinput  "Resources/DInput.rc"  "${CMAKE_SOURCE_DIR}/dinput.def"  0x0700
  ${XIDI_DIRECTINPUT_FRONTEND_SOURCES})
xidi_add_proxy_dll(DInput8 dinput8 "Resources/DInput8.rc" "${CMAKE_SOURCE_DIR}/dinput8.def" 0x0800
  ${XIDI_DIRECTINPUT_FRONTEND_SOURCES})
xidi_add_proxy_dll(WinMM   winmm   "Resources/WinMM.rc"   "${CMAKE_SOURCE_DIR}/winmm.def"   0x0800
  Source/ExportApiWinMM.cpp Source/WrapperJoyWinMM.cpp)

# ---- Hook module (Hookshot) ----------------------------------------------------------------------

if(XIDI_BUILD_HOOKMODULE)
  add_library(HookModule SHARED
    Source/ApiDirectInput.cpp
    Source/ApiGUID.cpp
    Source/ApiXidi.cpp
    Source/DllMain.cpp
    Source/Globals.cpp
    Source/HookModuleMain.cpp
    Source/Hooks/CoCreateInstance.cpp
    Source/SetHooks.cpp
    Source/SetHooksDirectInput.cpp
    Source/SetHooksWinMM.cpp
    Source/Strings.cpp
    Source/XidiConfigReader.cpp
    Resources/HookModule.rc
    Resources/Xidi.rc)
  target_compile_definitions(HookModule PRIVATE XIDI_SKIP_MAPPERS DIRECTINPUT_VERSION=0x0800)
  target_link_libraries(HookModule PRIVATE CoreInfra Xidi_SDL3_Headers Xidi_Boost Xidi_XstdBitSet
                                           Xidi_Hookshot shlwapi)
  set_target_properties(HookModule PROPERTIES
    OUTPUT_NAME "Xidi.HookModule.${XIDI_ARCH_BITS}" PREFIX "")
  xidi_configure(HookModule)
endif()

# ---- Unit tests ----------------------------------------------------------------------------------

if(XIDI_BUILD_TESTS)
  file(GLOB XIDI_TEST_CASES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Source/Test/Case/*.cpp")

  # The test executable uses mock physical controllers, keyboard and mouse instead of the real
  # ones, and has no DLL entry points or configuration file reader.
  set(XIDI_TEST_SOURCES ${XIDI_CORE_SOURCES} ${XIDI_DIRECTINPUT_LAYER_SOURCES}
    ${XIDI_TEST_CASES}
    Source/Test/MockDirectInput.cpp
    Source/Test/MockDirectInputDevice.cpp
    Source/Test/MockKeyboard.cpp
    Source/Test/MockMouse.cpp
    Source/Test/MockPhysicalController.cpp
    Source/Test/TestMain.cpp)

  add_executable(XidiTest ${XIDI_TEST_SOURCES} Resources/Xidi.rc)
  target_compile_definitions(XidiTest PRIVATE XIDI_SKIP_CONFIG DIRECTINPUT_VERSION=0x0800 _CONSOLE)
  target_include_directories(XidiTest PRIVATE "${CMAKE_SOURCE_DIR}/Include/Xidi/Test")
  target_link_libraries(XidiTest PRIVATE TestInfra CoreInfra Xidi_SDL3 Xidi_Boost Xidi_XstdBitSet)
  set_target_properties(XidiTest PROPERTIES WIN32_EXECUTABLE FALSE)
  xidi_configure(XidiTest)
  xidi_copy_sdl3_dll(XidiTest)
  add_test(NAME XidiTest COMMAND XidiTest)
endif()
