/***************************************************************************************************
 * Infra
 *   Common infrastructure used by other projects.
 ***************************************************************************************************
 * Authored by Samuel Grossman
 * Copyright (c) 2016-2024
 ***********************************************************************************************//**
 * @file TestCase.cpp
 *   Implementation of the test case interface.
 **************************************************************************************************/

#include "Test/TestCase.h"

#include <string_view>

#include "Test/Harness.h"

namespace Infra
{
  namespace Test
  {
    ITestCase::ITestCase(std::wstring_view name)
    {
      Harness::RegisterTestCase(this, name);
    }
  } // namespace Test
} // namespace Infra
