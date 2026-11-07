# Infra: common infrastructure used by Xidi. Ported from CoreInfra.vcxproj, TestInfra.vcxproj and
# CoreInfraTest.vcxproj.
#
# Expects the including project to provide the XidiGitVersionInfo target and to have added
# ${XIDI_GENERATED_DIR} to the include path where needed (see the top-level CMakeLists.txt).

set(INFRA_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/Include
  ${CMAKE_CURRENT_SOURCE_DIR}/Include/Infra
  ${CMAKE_CURRENT_SOURCE_DIR}/Include/Infra/Internal)

function(infra_configure tgt)
  target_include_directories(${tgt} PUBLIC ${INFRA_INCLUDE_DIRS})
  target_include_directories(${tgt} PRIVATE ${XIDI_GENERATED_DIR})
  target_compile_definitions(${tgt} PRIVATE _CONSOLE UNICODE _UNICODE $<$<CONFIG:Debug>:_DEBUG>)
  add_dependencies(${tgt} XidiGitVersionInfo)
  xidi_apply_common_settings(${tgt})
endfunction()

add_library(CoreInfra STATIC
  Source/Core/Configuration.cpp
  Source/Core/Message.cpp
  Source/Core/ProcessInfo.cpp
  Source/Core/Strings.cpp
  Source/Core/TemporaryBuffer.cpp)
infra_configure(CoreInfra)

add_library(TestInfra STATIC
  Source/Test/Harness.cpp
  Source/Test/TestCase.cpp
  Source/Test/Utilities.cpp)
target_link_libraries(TestInfra PUBLIC CoreInfra)
infra_configure(TestInfra)

if(XIDI_BUILD_TESTS)
  add_executable(CoreInfraTest
    Source/Test/ProductInfo.cpp
    Source/Test/TestMain.cpp
    Source/Test/Case/StringsTest.cpp
    Source/Test/Case/TemporaryBufferTest.cpp)
  target_link_libraries(CoreInfraTest PRIVATE TestInfra CoreInfra)
  infra_configure(CoreInfraTest)
  set_target_properties(CoreInfraTest PROPERTIES WIN32_EXECUTABLE FALSE)
  add_test(NAME CoreInfraTest COMMAND CoreInfraTest)
endif()
