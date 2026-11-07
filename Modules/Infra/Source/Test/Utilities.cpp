/***************************************************************************************************
 * Infra
 *   Common infrastructure used by other projects.
 ***************************************************************************************************
 * Authored by Samuel Grossman
 * Copyright (c) 2016-2024
 ***********************************************************************************************//**
 * @file Utilities.cpp
 *   Implementation of test utility functions.
 **************************************************************************************************/

#include "Test/Utilities.h"

#include <sal.h>

#include <cstdarg>
#include <cstdio>

#include "Internal/ApiWindows.h"

namespace Infra
{
  namespace Test
  {
    void Print(const wchar_t* const str)
    {
      if (IsDebuggerPresent())
      {
        OutputDebugString(str);
        OutputDebugString(L"\n");
      }
      else
      {
        _putws(str);
      }
    }

    void PrintFormatted(_Printf_format_string_ const wchar_t* const format, ...)
    {
      wchar_t formattedStringBuffer[1024];

      va_list args;
      va_start(args, format);
      vswprintf_s(formattedStringBuffer, _countof(formattedStringBuffer), format, args);
      va_end(args);

      Print(formattedStringBuffer);
    }
  } // namespace Test
} // namespace Infra
