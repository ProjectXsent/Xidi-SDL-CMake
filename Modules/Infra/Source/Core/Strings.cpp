/***************************************************************************************************
 * Infra
 *   Common infrastructure used by other projects.
 ***************************************************************************************************
 * Authored by Samuel Grossman
 * Copyright (c) 2016-2024
 ***********************************************************************************************//**
 * @file Strings.cpp
 *   Implementation of functions for manipulating strings.
 **************************************************************************************************/

#include "Core/Strings.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <mutex>
#include <string>
#include <string_view>

#include "ApiWindows.h"

#include "Core/DebugAssert.h"
#include "Core/ProcessInfo.h"
#include "Core/TemporaryBuffer.h"

namespace Infra
{
  namespace Strings
  {
    /// Converts a single character to lowercase.
    /// Default implementation does nothing useful.
    /// @tparam CharType Character type.
    /// @param [in] c Character to convert.
    /// @return Null character, as the default implementation does nothing useful.
    template <typename CharType> static inline CharType ToLowercase(CharType c)
    {
      return L'\0';
    }

    /// Converts a single narrow character to lowercase.
    /// @tparam CharType Character type.
    /// @param [in] c Character to convert.
    /// @return Lowercase version of the input, if a conversion is possible, or the same
    /// character as the input otherwise.
    template <> char static inline ToLowercase(char c)
    {
      return std::tolower(c);
    }

    /// Converts a single wide character to lowercase.
    /// Default implementation does nothing useful.
    /// @tparam CharType Character type.
    /// @param [in] c Character to convert.
    /// @return Lowercase version of the input, if a conversion is possible, or the same
    /// character as the input otherwise.
    template <> wchar_t static inline ToLowercase(wchar_t c)
    {
      return std::towlower(c);
    }

    template <typename CharType> int CompareCaseInsensitive(
        std::basic_string_view<CharType> strA, std::basic_string_view<CharType> strB)
    {
      for (size_t i = 0; i < std::min(strA.length(), strB.length()); ++i)
      {
        const wchar_t charA = ToLowercase(strA[i]);
        const wchar_t charB = ToLowercase(strB[i]);

        if (charA != charB) return (static_cast<int>(charA) - static_cast<int>(charB));
      }

      return (static_cast<int>(strA.length()) - static_cast<int>(strB.length()));
    }

    template int CompareCaseInsensitive<char>(std::string_view, std::string_view);
    template int CompareCaseInsensitive<wchar_t>(std::wstring_view, std::wstring_view);

    TemporaryString ConvertNarrowToWide(const char* str)
    {
      return TemporaryString(str);
    }

    TemporaryBuffer<char> ConvertWideToNarrow(const wchar_t* str)
    {
      TemporaryBuffer<char> convertedStr;
      size_t numCharsConverted = 0;

      if (0 !=
          wcstombs_s(
              &numCharsConverted,
              convertedStr.Data(),
              convertedStr.Capacity(),
              str,
              static_cast<size_t>(convertedStr.Capacity()) - 1))
        convertedStr[0] = '\0';

      return convertedStr;
    }

    template <typename CharType> bool EqualsCaseInsensitive(
        std::basic_string_view<CharType> strA, std::basic_string_view<CharType> strB)
    {
      if (strA.length() != strB.length()) return false;

      for (size_t i = 0; i < strA.length(); ++i)
      {
        if (ToLowercase(strA[i]) != ToLowercase(strB[i])) return false;
      }

      return true;
    }

    template bool EqualsCaseInsensitive<char>(std::string_view, std::string_view);
    template bool EqualsCaseInsensitive<wchar_t>(std::wstring_view, std::wstring_view);

    template <typename CharType> bool EndsWithCaseInsensitive(
        std::basic_string_view<CharType> str, std::basic_string_view<CharType> maybeSuffix)
    {
      if (str.length() < maybeSuffix.length()) return false;

      str.remove_prefix(str.length() - maybeSuffix.length());
      return EqualsCaseInsensitive(str, maybeSuffix);
    }

    template bool EndsWithCaseInsensitive<char>(std::string_view, std::string_view);
    template bool EndsWithCaseInsensitive<wchar_t>(std::wstring_view, std::wstring_view);

    TemporaryString Format(_Printf_format_string_ const wchar_t* format, ...)
    {
      TemporaryString buf;

      va_list args;
      va_start(args, format);

      buf.UnsafeSetSize(static_cast<size_t>(vswprintf_s(buf.Data(), buf.Capacity(), format, args)));

      va_end(args);

      return buf;
    }

    TemporaryString FromSystemErrorCode(const unsigned long systemErrorCode)
    {
      TemporaryString systemErrorString;
      DWORD systemErrorLength = FormatMessage(
          FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
          nullptr,
          systemErrorCode,
          0,
          systemErrorString.Data(),
          systemErrorString.Capacity(),
          nullptr);

      if (0 == systemErrorLength)
      {
        systemErrorString = Format(L"System error %u.", static_cast<unsigned int>(systemErrorCode));
      }
      else
      {
        systemErrorString.UnsafeSetSize(systemErrorLength - 1);
        while (L'\0' == systemErrorString.Back() || iswspace(systemErrorString.Back()))
          systemErrorString.RemoveSuffix(1);
        if (L'.' != systemErrorString.Back()) systemErrorString += L'.';
      }

      return systemErrorString;
    }

    template <typename CharType> size_t HashCaseInsensitive(std::basic_string_view<CharType> str)
    {
      // Implements the FNV-1a hash algorithm. References:
      // https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
      // https://softwareengineering.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed/145633#145633

#ifdef _WIN64
      constexpr uint64_t hashPrime = 1099511628211ull;
      uint64_t hash = 14695981039346656037ull;
#else
      constexpr uint32_t hashPrime = 16777619u;
      uint32_t hash = 2166136261u;
#endif
      static_assert(sizeof(size_t) == sizeof(hash), "Hash size mismatch.");

      for (size_t charIndex = 0; charIndex < str.length(); ++charIndex)
      {
        const CharType currentChar = Strings::ToLowercase(str[charIndex]);
        const uint8_t* const charByteBase = reinterpret_cast<const uint8_t*>(&currentChar);
        const size_t charByteCount = sizeof(currentChar);

        for (size_t charByteIndex = 0; charByteIndex < charByteCount; ++charByteIndex)
        {
          const decltype(hash) currentByte =
              static_cast<decltype(hash)>(charByteBase[charByteIndex]);
          hash = hash ^ currentByte;
          hash = hash * hashPrime;
        }
      }

      return hash;
    }

    template size_t HashCaseInsensitive<char>(std::string_view);
    template size_t HashCaseInsensitive<wchar_t>(std::wstring_view);

    std::wstring_view LoadFromStringTableResource(HINSTANCE instanceHandle, UINT stringResourceId)
    {
      const wchar_t* stringStart = nullptr;
      int stringLength = LoadStringW(
          Infra::ProcessInfo::GetThisModuleInstanceHandle(), 201, (wchar_t*)&stringStart, 0);
      std::wstring_view loadedResourceString(stringStart, stringLength);
      while (loadedResourceString.ends_with(L'\0'))
        loadedResourceString.remove_suffix(1);
      return loadedResourceString;
    }
    
    template <typename CharType> TemporaryVector<std::basic_string_view<CharType>> Split(
        std::basic_string_view<CharType> stringToSplit, std::basic_string_view<CharType> delimiter)
    {
      return Split(stringToSplit, &delimiter, 1);
    }

    template TemporaryVector<std::string_view> Split<char>(std::string_view, std::string_view);
    template TemporaryVector<std::wstring_view> Split<wchar_t>(
        std::wstring_view, std::wstring_view);

    template <typename CharType> TemporaryVector<std::basic_string_view<CharType>> Split(
        std::basic_string_view<CharType> stringToSplit,
        std::initializer_list<std::basic_string_view<CharType>> delimiters)
    {
      return Split(stringToSplit, delimiters.begin(), static_cast<unsigned int>(delimiters.size()));
    }

    template TemporaryVector<std::string_view> Split<char>(
        std::string_view, std::initializer_list<std::string_view>);
    template TemporaryVector<std::wstring_view> Split<wchar_t>(
        std::wstring_view, std::initializer_list<std::wstring_view>);

    template <typename CharType> TemporaryVector<std::basic_string_view<CharType>> Split(
        std::basic_string_view<CharType> stringToSplit,
        const std::basic_string_view<CharType>* delimiters,
        unsigned int numDelimiters)
    {
      TemporaryVector<std::basic_string_view<CharType>> stringPieces;

      auto beginIter = stringToSplit.cbegin();
      auto endIter = beginIter;

      while ((stringPieces.Size() < stringPieces.Capacity()) && (stringToSplit.cend() != endIter))
      {
        bool delimiterFound = false;
        std::basic_string_view<CharType> remainingStringToSplit(endIter, stringToSplit.cend());

        for (unsigned int i = 0; i < numDelimiters; ++i)
        {
          auto delimiter = delimiters[i];

          if (true == delimiter.empty()) continue;

          if (true == remainingStringToSplit.starts_with(delimiter))
          {
            stringPieces.EmplaceBack(beginIter, endIter);
            endIter += delimiter.length();
            beginIter = endIter;
            delimiterFound = true;
            break;
          }
        }

        if (false == delimiterFound) endIter += 1;
      }

      if (stringPieces.Size() < stringPieces.Capacity())
        stringPieces.EmplaceBack(beginIter, endIter);
      else
        stringPieces.Clear();

      return stringPieces;
    }

    template TemporaryVector<std::string_view> Split<char>(
        std::string_view, const std::string_view*, unsigned int);
    template TemporaryVector<std::wstring_view> Split<wchar_t>(
        std::wstring_view, const std::wstring_view*, unsigned int);

    template <typename CharType> bool StartsWithCaseInsensitive(
        std::basic_string_view<CharType> str, std::basic_string_view<CharType> maybePrefix)
    {
      if (str.length() < maybePrefix.length()) return false;

      str.remove_suffix(str.length() - maybePrefix.length());
      return EqualsCaseInsensitive(str, maybePrefix);
    }

    template bool StartsWithCaseInsensitive<char>(std::string_view, std::string_view);
    template bool StartsWithCaseInsensitive<wchar_t>(std::wstring_view, std::wstring_view);

    template <typename CharType> std::optional<std::basic_string_view<CharType>> Tokenize(
        size_t& tokenizeState,
        std::basic_string_view<CharType> stringToTokenize,
        std::basic_string_view<CharType> delimiter)
    {
      return Tokenize(tokenizeState, stringToTokenize, &delimiter, 1);
    }

    template std::optional<std::string_view> Tokenize<char>(
        size_t&, std::string_view, std::string_view);
    template std::optional<std::wstring_view> Tokenize<wchar_t>(
        size_t&, std::wstring_view, std::wstring_view);

    template <typename CharType> std::optional<std::basic_string_view<CharType>> Tokenize(
        size_t& tokenizeState,
        std::basic_string_view<CharType> stringToTokenize,
        const std::basic_string_view<CharType>* delimiters,
        unsigned int numDelimiters)
    {
      if (stringToTokenize.length() < tokenizeState) return std::nullopt;

      auto beginIter = stringToTokenize.cbegin() + tokenizeState;
      auto endIter = beginIter;

      while (stringToTokenize.cend() != endIter)
      {
        std::basic_string_view<CharType> remainingStringToTokenize(
            endIter, stringToTokenize.cend());

        for (unsigned int i = 0; i < numDelimiters; ++i)
        {
          auto delimiter = delimiters[i];

          if (true == delimiter.empty()) continue;

          if (true == remainingStringToTokenize.starts_with(delimiter))
          {
            tokenizeState += delimiter.length();
            return std::basic_string_view<CharType>(beginIter, endIter);
          }
        }

        tokenizeState += 1;
        endIter += 1;
      }

      tokenizeState = (1 + stringToTokenize.length());
      return std::basic_string_view<CharType>(beginIter, endIter);
    }

    template std::optional<std::string_view> Tokenize<char>(
        size_t&, std::string_view, const std::string_view*, unsigned int);
    template std::optional<std::wstring_view> Tokenize<wchar_t>(
        size_t&, std::wstring_view, const std::wstring_view*, unsigned int);
  } // namespace Strings
} // namespace Infra
