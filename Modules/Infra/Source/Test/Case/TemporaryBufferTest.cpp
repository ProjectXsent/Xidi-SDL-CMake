/***************************************************************************************************
 * Infra
 *   Common infrastructure used by other projects.
 ***************************************************************************************************
 * Authored by Samuel Grossman
 * Copyright (c) 2016-2024
 ***********************************************************************************************//**
 * @file StringsTest.cpp
 *   Unit tests for functions that manipulate strings.
 **************************************************************************************************/

#include "Core/TemporaryBuffer.h"

#include "Test/TestCase.h"
#include "Test/Utilities.h"

namespace CoreInfraTest
{
  using namespace ::Infra;

  // Verifies that temporary buffers correctly report their capacities.
  TEST_CASE(TemporaryBuffer_Capacity)
  {
    TemporaryBuffer<int> testBuf;
    TEST_ASSERT(testBuf.CapacityBytes() == TemporaryBufferBase::kBytesPerBuffer);
    TEST_ASSERT(testBuf.Capacity() == TemporaryBufferBase::kBytesPerBuffer / sizeof(int));
  }

  // Verifies that temporary buffers correctly return pointers to their underlying data arrays.
  TEST_CASE(TemporaryBuffer_Data)
  {
    TemporaryBuffer<int> testBuf;
    TEST_ASSERT(testBuf.Data() == &testBuf[0]);
  }

  // Verifies that temporary buffers can be moved and that doing so produces the right result.
  TEST_CASE(TemporaryBuffer_Move)
  {
    TemporaryBuffer<int> testBuf;
    testBuf[0] = 55;
    testBuf[testBuf.Capacity() - 1] = 5555;

    TemporaryBuffer<int> testBuf2(std::move(testBuf));
    TEST_ASSERT(testBuf2[0] == 55);
    TEST_ASSERT(testBuf2[testBuf2.Capacity() - 1] == 5555);
    TEST_ASSERT(testBuf.Data() != testBuf2.Data());
  }

  // Verifies that newly-created temporary vectors correctly report that they are empty.
  TEST_CASE(TestVector_Empty)
  {
    TemporaryVector<int> testVector;
    TEST_ASSERT(true == testVector.Empty());
    TEST_ASSERT(0 == testVector.Size());
  }

  // Initializes a newly-created temporary vector and verifies that iteration works both by
  // explicitly-indexed and range-based loops.
  TEST_CASE(TemporaryVector_InitializeAndIterate)
  {
    TemporaryVector<int> testVector = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    TEST_ASSERT(testVector.Size() == 9);

    for (unsigned int i = 0; i < testVector.Size(); ++i)
      TEST_ASSERT(i == testVector[i]);

    unsigned int i = 0;
    for (const auto& element : testVector)
    {
      TEST_ASSERT(i == element);
      i += 1;
    }
  }

  // Verifies that simple push and pop operations execute correctly.
  TEST_CASE(TemporaryVector_PushAndPop)
  {
    TemporaryVector<int> testVector;

    testVector.PushBack(11);
    TEST_ASSERT(false == testVector.Empty());
    TEST_ASSERT(1 == testVector.Size());
    TEST_ASSERT(11 == testVector.Front());
    TEST_ASSERT(11 == testVector.Back());

    testVector.PushBack(22);
    TEST_ASSERT(false == testVector.Empty());
    TEST_ASSERT(2 == testVector.Size());
    TEST_ASSERT(11 == testVector.Front());
    TEST_ASSERT(22 == testVector.Back());

    testVector.PopBack();
    TEST_ASSERT(false == testVector.Empty());
    TEST_ASSERT(1 == testVector.Size());
    TEST_ASSERT(11 == testVector.Front());
    TEST_ASSERT(11 == testVector.Back());

    testVector.PopBack();
    TEST_ASSERT(true == testVector.Empty());
    TEST_ASSERT(0 == testVector.Size());
  }

  // Verifies that a vector can be copied and that the result is all stored elements are also
  // copied.
  TEST_CASE(TemporaryVector_Copy)
  {
    TemporaryVector<int> testVector1 = {11, 22, 33, 44, 55};
    TemporaryVector<int> testVector2 = testVector1;

    TEST_ASSERT(testVector2 == testVector1);
    TEST_ASSERT(testVector2.Data() != testVector1.Data());
  }

  // Verifies that a vector can be the source of a move operation and that ownership of its backing
  // buffer pointer is correctly transferred.
  TEST_CASE(TemporaryVector_Move)
  {
    TemporaryVector<int> testVector1 = {11, 22, 33, 44, 55, 66, 77, 88, 99};
    int* const backingBuffer = testVector1.Data();

    TemporaryVector<int> testVector2 = std::move(testVector1);
    TEST_ASSERT(testVector1.Data() != backingBuffer);
    TEST_ASSERT(testVector2.Data() == backingBuffer);
    TEST_ASSERT(true == testVector1.Empty());
    TEST_ASSERT(9 == testVector2.Size());

    for (unsigned int i = 0; i < testVector2.Size(); ++i)
      TEST_ASSERT((11 * (i + 1)) == testVector2[i]);
  }

  // Verifies that a temporary string can have multiple different data types correctly appended.
  TEST_CASE(TemporaryString_ConcatenateMultipleTypes)
  {
    TemporaryString testString;
    testString << L"Wide-character string " << "Narrow-character string " << 1234 << false << -5678
               << true;
    TEST_ASSERT(testString == L"Wide-character string Narrow-character string 1234false-5678true");
  }

  // Verifies that unsafely setting the size of a string ensures proper null termination.
  TEST_CASE(TemporaryString_UnsafeSetSizeAddsNull)
  {
    TemporaryString testString = L"123456789";
    TEST_ASSERT(testString[5] == L'6');

    testString.UnsafeSetSize(5);
    TEST_ASSERT(testString == L"12345");
    TEST_ASSERT(testString[5] == L'\0');
  }

  // Verifies that characters outside the 0-128 ANSI range are correctly converted when appended as
  // a narrow-character string.
  TEST_CASE(TemporaryString_NarrowCharacterConversion)
  {
    const char narrowCharStringToConvert[] = "Æ±×÷µ¼½¾";
    wchar_t expectedConvertedStr[_countof(narrowCharStringToConvert)];
    size_t expectedConvertedStrLength = 0;
    mbstowcs_s(
        &expectedConvertedStrLength,
        expectedConvertedStr,
        narrowCharStringToConvert,
        _countof(expectedConvertedStr) - 1);

    TemporaryString actualConvertedStr = narrowCharStringToConvert;
    TEST_ASSERT(actualConvertedStr == expectedConvertedStr);
  }

  // Verifies that suffix replacement works correctly when the replacement suffix is shorter than
  // the existing string.
  TEST_CASE(TemporaryString_ReplaceSuffix_ShorterThanExisting)
  {
    TemporaryString testString = L"This is a test object";
    testString.ReplaceSuffix(L"string");
    TEST_ASSERT(L"This is a test string" == testString);
  }

  // Verifies that suffix replacement works correctly when the replacement suffix is longer than the
  // existing string.
  TEST_CASE(TemporaryString_ReplaceSuffix_LongerThanExisting)
  {
    TemporaryString testString = L"somestring";
    testString.ReplaceSuffix(L"This is a test string");
    TEST_ASSERT(L"This is a test string" == testString);
  }

  // Verifies that suffix removal works correctly and that the string is shortened by the specified
  // number of characters.
  TEST_CASE(TemporaryString_RemoveSuffix)
  {
    TemporaryString testString = L"This is a test string with a suffix.";
    testString.RemoveSuffix(15);
    TEST_ASSERT(L"This is a test string" == testString);
  }

  // Verifies that trailing character removal works correctly and removes all instances of the
  // specified character.
  TEST_CASE(TemporaryString_RemoveTrailing_CharacterMatches)
  {
    TemporaryString testString = L"This is a test string...........";
    testString.RemoveTrailing(L'.');
    TEST_ASSERT(L"This is a test string" == testString);
  }

  // Verifies that trailing character removal works correctly and does not edit the string if the
  // specified character is not actually the last character of the string and hence is not trailing.
  TEST_CASE(TemporaryString_RemoveTrailing_CharacterDoesNotMatch)
  {
    TemporaryString testString = L"This is a test string";
    testString.RemoveTrailing(L'.');
    TEST_ASSERT(L"This is a test string" == testString);
  }

} // namespace CoreInfraTest
