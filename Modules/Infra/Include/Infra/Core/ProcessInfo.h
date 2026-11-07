/***************************************************************************************************
 * Infra
 *   Common infrastructure used by other projects.
 ***************************************************************************************************
 * Authored by Samuel Grossman
 * Copyright (c) 2016-2024
 ***********************************************************************************************//**
 * @file Globals.h
 *   Declaration of functionality related to getting information about the running process.
 **************************************************************************************************/

#pragma once

#include <string>
#include <string_view>

#include "ApiWindows.h"
#include "Strings.h"

#include "GitVersionInfo.generated.h"

/// Defines the product name using a wide-string literal. One of the product name and one of the
/// product version macros must be invoked anywhere in the global scope in a project that uses Infra
/// to ensure that the product name and version is set early at runtime.
#define INFRA_DEFINE_PRODUCT_NAME_FROM_LITERAL(name)                                               \
  namespace Infra                                                                                  \
  {                                                                                                \
    namespace ProcessInfo                                                                          \
    {                                                                                              \
      namespace _ProductInformationInternal                                                        \
      {                                                                                            \
        std::wstring GetDefinedProductNameInternal(void)                                           \
        {                                                                                          \
          return name;                                                                             \
        }                                                                                          \
      }                                                                                            \
    }                                                                                              \
  }

/// Defines the product name using a string table resource. One of the product name and one of the
/// product version macros must be invoked anywhere in the global scope in a project that uses Infra
/// to ensure that the product name and version is set early at runtime.
#define INFRA_DEFINE_PRODUCT_NAME_FROM_RESOURCE(hinstance, resourceStringId)                       \
  namespace Infra                                                                                  \
  {                                                                                                \
    namespace ProcessInfo                                                                          \
    {                                                                                              \
      namespace _ProductInformationInternal                                                        \
      {                                                                                            \
        std::wstring GetDefinedProductNameInternal(void)                                           \
        {                                                                                          \
          std::wstring_view loadedResourceString =                                                 \
              ::Infra::Strings::LoadFromStringTableResource(hinstance, resourceStringId);          \
          if (true == loadedResourceString.empty())                                                \
            return std::wstring(::Infra::ProcessInfo::GetThisModuleBaseName());                    \
          return std::wstring(loadedResourceString);                                               \
        }                                                                                          \
      }                                                                                            \
    }                                                                                              \
  }

/// Defines the product version using Git version information auto-populated for whatever project is
/// being compiled that is using Infra. One of the product name and one of the product version
/// macros must be invoked anywhere in the global scope in a project that uses Infra to ensure that
/// the product name and version is set early at runtime.
#define INFRA_DEFINE_PRODUCT_VERSION_FROM_GIT_VERSION_INFO()                                       \
  namespace Infra                                                                                  \
  {                                                                                                \
    namespace ProcessInfo                                                                          \
    {                                                                                              \
      namespace _ProductInformationInternal                                                        \
      {                                                                                            \
        SVersionInfo GetDefinedProductVersionInternal(void)                                        \
        {                                                                                          \
          return GitVersionInfoForCurrentProject();                                                \
        }                                                                                          \
      }                                                                                            \
    }                                                                                              \
  }

namespace Infra
{
  namespace ProcessInfo
  {
    /// Version information structure.
    struct SVersionInfo
    {
      /// Major version number.
      uint16_t major;

      /// Minor version number.
      uint16_t minor;

      /// Patch level.
      uint16_t patch;

      union
      {
        /// Complete view of the flags element of structured version information.
        uint16_t flags;

        // Per Microsoft documentation, bit fields are ordered from low bit to high bit.
        // See https://docs.microsoft.com/en-us/cpp/cpp/cpp-bit-fields for more information.
        struct
        {
          /// Whether or not the working directory was dirty when the binary was built.
          uint16_t isDirty : 1;

          /// Unused bits, reserved for future use.
          uint16_t reserved : 3;

          /// Number of commits since the most recent official version tag.
          uint16_t commitDistance : 12;
        };
      };

      /// String representation of the version information, including any suffixes.
      /// Guaranteed to be null-terminated.
      std::wstring_view string;
    };

    static_assert(
        sizeof(SVersionInfo) == ((4 * sizeof(uint16_t)) + sizeof(std::wstring_view)),
        "Version information structure size constraint violation.");

    /// Produces a version information structure filled with information supplied by Git for the
    /// project from which this function is invoked. The constants used in this function are
    /// auto-generated by the build system when a project is built.
    /// @return Git version information structure for the current project.
    consteval SVersionInfo GitVersionInfoForCurrentProject(void)
    {
      constexpr uint16_t kVersionStructured[] = {GIT_VERSION_STRUCT};
      static_assert(4 == _countof(kVersionStructured), "Invalid structured version information.");

      return {
          .major = kVersionStructured[0],
          .minor = kVersionStructured[1],
          .patch = kVersionStructured[2],
          .flags = kVersionStructured[3],
          .string = _CRT_WIDE(GIT_VERSION_STRING)};
    }

    /// Retrieves a pseudohandle to the current process.
    /// @return Current process pseudohandle.
    HANDLE GetCurrentProcessHandle(void);

    /// Retrieves the PID of the current process.
    /// @return Current process PID.
    DWORD GetCurrentProcessId(void);

    /// Retrieves the handle of the instance that represents the binary module that is running this
    /// code, which could be different from the executable if this code is contained in a
    /// dynamically-linked library.
    /// @return Instance handle for this code.
    HINSTANCE GetThisModuleInstanceHandle(void);

    /// Retrieves information on the current system. This includes architecture, page size, and
    /// so on.
    /// @return Reference to a read-only structure containing system information.
    const SYSTEM_INFO& GetSystemInformation(void);

    /// Retrieves and returns version information for this running instance of the Infra library.
    /// @return Version information structure for the Infra library.
    SVersionInfo GetInfraVersion(void);

    /// Retrieves and returns the name of the product that corresponds to the running binary.
    /// @return Name of the product for this running binary.
    std::wstring_view GetProductName(void);

    /// Retrieves and returns version information for this running binary.
    /// @return Version information structure for the product.
    SVersionInfo GetProductVersion(void);

    /// Obtains the complete filename for the running executable.
    /// @return Complete filename of the running executable.
    std::wstring_view GetExecutableCompleteFilename(void);

    /// Obtains the base name for the running executable, which is the file name part of the path,
    /// after the last backslash.
    /// @return Base name of the running executable.
    std::wstring_view GetExecutableBaseName(void);

    /// Obtains the directory in which the running executable is located, without a trailing
    /// backslash.
    /// @return Directory name of the running executable.
    std::wstring_view GetExecutableDirectoryName(void);

    /// Obtains the complete filename for the specific binary module that contains this code.
    /// @return Complete filename of the module that contains this code.
    std::wstring_view GetThisModuleCompleteFilename(void);

    /// Obtains the base name for the specific binary module that contains this code, which is the
    /// file name part of the path, after the last backslash.
    /// @return Base name of the module that contains this code.
    std::wstring_view GetThisModuleBaseName(void);

    /// Obtains the directory in which the specific binary module that contains this code is
    /// located, without a trailing backslash.
    /// @return Directory name of the module that contains this code.
    std::wstring_view GetThisModuleDirectoryName(void);
  } // namespace ProcessInfo
} // namespace Infra
