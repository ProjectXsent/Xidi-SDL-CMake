/***************************************************************************************************
 * Infra
 *   Common infrastructure used by other projects.
 ***************************************************************************************************
 * Authored by Samuel Grossman
 * Copyright (c) 2016-2024
 ***********************************************************************************************//**
 * @file Strings.h
 *   Declaration of types and functions for manipulating strings.
 **************************************************************************************************/

#pragma once

#include <sal.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string_view>

#include "ApiWindows.h"
#include "TemporaryBuffer.h"

namespace Infra
{
  namespace Strings
  {
    /// Compares two strings without regard for the case of each individual character.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in] strA First string in the comparison.
    /// @param [in] strB Second string in the comparison.
    /// @return Negative number if strA is "less than" strB, positive number of strA is "greater
    /// than" strB, and 0 if the two strings are equal.
    template <typename CharType> int CompareCaseInsensitive(
        std::basic_string_view<CharType> strA, std::basic_string_view<CharType> strB);

    /// Converts characters in a narrow character string to wide character format.
    /// @param [in] str Null-terminated string to convert.
    /// @return Result of the conversion, or an empty string on failure.
    TemporaryString ConvertNarrowToWide(const char* str);

    /// Converts characters in a wide character string to narrow character format.
    /// @param [in] str Null-terminated string to convert.
    /// @return Result of the conversion, or an empty string on failure.
    TemporaryBuffer<char> ConvertWideToNarrow(const wchar_t* str);

    /// Checks if one string is a suffix of another without regard for the case of each individual
    /// character.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in] str String to be checked for a possible prefix.
    /// @param [in] maybeSuffix Candidate suffix to compare with the end of the string.
    /// @return `true` if the candidate suffix is a suffix of the specified string, `false`
    /// otherwise.
    template <typename CharType> bool EndsWithCaseInsensitive(
        std::basic_string_view<CharType> str, std::basic_string_view<CharType> maybeSuffix);

    /// Checks if two strings are equal without regard for the case of each individual
    /// character.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in] strA First string in the comparison.
    /// @param [in] strB Second string in the comparison.
    /// @return `true` if the strings compare equal, `false` otherwise.
    template <typename CharType> bool EqualsCaseInsensitive(
        std::basic_string_view<CharType> strA, std::basic_string_view<CharType> strB);

    /// Formats a string and returns the result in a newly-allocated null-terminated temporary
    /// buffer.
    /// @param [in] format Format string, possibly with format specifiers which must be matched
    /// with the arguments that follow.
    /// @return Resulting string after all formatting is applied.
    TemporaryString Format(_Printf_format_string_ const wchar_t* format, ...);

    /// Generates a string representation of a system error code. The resulting string will always
    /// end in a period ('.') character.
    /// @param [in] systemErrorCode System error code for which to generate a string.
    /// @return String representation of the system error code.
    TemporaryString FromSystemErrorCode(const unsigned long systemErrorCode);

    /// Computes a hash code for the specified string, without regard to case.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in] str String for which a hash code is desired.
    /// @return Resulting hash code for the input string.
    template <typename CharType> size_t HashCaseInsensitive(std::basic_string_view<CharType> str);

    /// Safely loads the specified string resource from a module's string table. The returned view
    /// is not necessarily null-terminated and points directly to the string resource itself.
    /// @param [in] instanceHandle Instance handle of the module whose string table should be
    /// searched.
    /// @param [in] stringResourceid Identifier of the desired string resource.
    /// @return Read-only view of the resource, or an empty view if the resource could not be
    /// located.
    std::wstring_view LoadFromStringTableResource(HINSTANCE instanceHandle, UINT stringResourceId);

    /// Removes the all occurrences of specified leading character from the input string view
    /// and returns the result.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in] str String view from which to remove the leading character.
    /// @param [in] leadingChar Leading character to strip from this string.
    /// @return Resulting string view after the leading character is removed.
    template <typename CharType> inline std::basic_string_view<CharType> RemoveLeading(
        std::basic_string_view<CharType> str, CharType leadingChar)
    {
      while (str.starts_with(leadingChar))
        str.remove_prefix(1);

      return str;
    }

    /// Removes the all occurrences of specified trailing character from the input string view
    /// and returns the result.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in] str String view from which to remove the trailing character.
    /// @param [in] trailingChar Trailing character to strip from this string.
    /// @return Resulting string view after the trailing character is removed.
    template <typename CharType> inline std::basic_string_view<CharType> RemoveTrailing(
        std::basic_string_view<CharType> str, CharType trailingChar)
    {
      while (str.ends_with(trailingChar))
        str.remove_suffix(1);

      return str;
    }

    /// Splits a string using the specified delimiter string and returns a list of views, each
    /// corresponding to a part of the input string. If there are too many delimiters present
    /// such that not all of the pieces can fit into the returned container type then the
    /// returned container will be empty. Otherwise the returned container will contain at least
    /// one element.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in] stringToSplit Input string to be split.
    /// @param [in] delimiter Delimiter character sequence that identifies boundaries between
    /// pieces of the input string.
    /// @return Container that holds views referring to pieces of the input string split using
    /// the specified delimiter.
    template <typename CharType> TemporaryVector<std::basic_string_view<CharType>> Split(
        std::basic_string_view<CharType> stringToSplit, std::basic_string_view<CharType> delimiter);

    /// Splits a string using the specified delimiter strings and returns a list of views, each
    /// corresponding to a part of the input string. If there are too many delimiters present such
    /// that not all of the pieces can fit into the returned container type then the returned
    /// container will be empty. Otherwise the returned container will contain at least one element.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in] stringToSplit Input string to be split.
    /// @param [in] delimiters Delimiter character sequences each of which identifies a boundary
    /// between pieces of the input string.
    /// @return Container that holds views referring to pieces of the input string split using the
    /// specified delimiter.
    template <typename CharType> TemporaryVector<std::basic_string_view<CharType>> Split(
        std::basic_string_view<CharType> stringToSplit,
        std::initializer_list<std::basic_string_view<CharType>> delimiters);

    /// Splits a string using the specified delimiter strings and returns a list of views, each
    /// corresponding to a part of the input string. If there are too many delimiters present
    /// such that not all of the pieces can fit into the returned container type then the
    /// returned container will be empty. Otherwise the returned container will contain at least
    /// one element.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in] stringToSplit Input string to be split.
    /// @param [in] delimiters Pointer to an array of delimiter character sequences each of
    /// which identifies a boundary between pieces of the input string.
    /// @param [in] numDelimiters Number of delimiters contained in the delimiter array.
    /// @return Container that holds views referring to pieces of the input string split using
    /// the specified delimiter.
    template <typename CharType> TemporaryVector<std::basic_string_view<CharType>> Split(
        std::basic_string_view<CharType> stringToSplit,
        const std::basic_string_view<CharType>* delimiters,
        unsigned int numDelimiters);

    /// Checks if one string is a prefix of another without regard for the case of each
    /// individual character.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in] str String to be checked for a possible prefix.
    /// @param [in] maybePrefix Candidate prefix to compare with the beginning of the string.
    /// @return `true` if the candidate prefix is a prefix of the specified string, `false`
    /// otherwise.
    template <typename CharType> bool StartsWithCaseInsensitive(
        std::basic_string_view<CharType> str, std::basic_string_view<CharType> maybePrefix);

    /// Tokenizes a string, one token at a time, using the specified delimiter. Returns the next
    /// token found and updates the state variable for subsequent calls. Each invocation returns
    /// the next token found in the input string.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in, out] tokenizeState Opaque state variable that tracks tokenizing progress.
    /// Must be 0 on first invocation and preserved between invocations on the same input
    /// string.
    /// @param [in] stringToTokenize String to be tokenized.
    /// @param [in] delimiter Delimiter, which can consist of multiple characters, that
    /// separates tokens in the input string. Can additionally vary between invocations.
    /// @return Next token found in the input string, if it exists.
    template <typename CharType> std::optional<std::basic_string_view<CharType>> Tokenize(
        size_t& tokenizeState,
        std::basic_string_view<CharType> stringToTokenize,
        std::basic_string_view<CharType> delimiter);

    /// Tokenizes a string, one token at a time, using the specified delimiter strings. Returns
    /// the next token found and updates the state variable for subsequent calls. Each
    /// invocation returns the next token found in the input string.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    /// @param [in, out] tokenizeState Opaque state variable that tracks tokenizing progress.
    /// Must be 0 on first invocation and preserved between invocations on the same input
    /// string.
    /// @param [in] stringToTokenize String to be tokenized.
    /// @param [in] delimiters Pointer to an array of delimiter character sequences each of
    /// which identifies a boundary between pieces of the input string.
    /// @param [in] numDelimiters Number of delimiters contained in the delimiter array.
    /// @return Next token found in the input string, if it exists.
    template <typename CharType> std::optional<std::basic_string_view<CharType>> Tokenize(
        size_t& tokenizeState,
        std::basic_string_view<CharType> stringToTokenize,
        const std::basic_string_view<CharType>* delimiters,
        unsigned int numDelimiters);

    /// Case-insensitive hasher for various kinds of string representations. This is a
    /// type-transparent hasher for all string representations that are implicitly convertable
    /// to string views.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    template <typename CharType> struct CaseInsensitiveHasher
    {
      using is_transparent = void;

      constexpr size_t operator()(const std::basic_string_view<CharType>& key) const
      {
        return HashCaseInsensitive(key);
      }
    };

    /// Case-insensitive equality comparator for various kinds of string representations. This
    /// is a type-transparent comparator for all string representations that are implicitly
    /// convertable to string views.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    template <typename CharType> struct CaseInsensitiveEqualityComparator
    {
      using is_transparent = void;

      constexpr bool operator()(
          const std::basic_string_view<CharType>& lhs,
          const std::basic_string_view<CharType>& rhs) const
      {
        return EqualsCaseInsensitive(lhs, rhs);
      }
    };

    /// Case-insensitive greater-than comparator for various kinds of string representations.
    /// This is a type-transparent comparator for all string representations that are implicitly
    /// convertable to string views.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    template <typename CharType> struct CaseInsensitiveGreaterThanComparator
    {
      using is_transparent = void;

      constexpr bool operator()(
          const std::basic_string_view<CharType>& lhs,
          const std::basic_string_view<CharType>& rhs) const
      {
        return (CompareCaseInsensitive(lhs, rhs) > 0);
      }
    };

    /// Case-insensitive less-than comparator for various kinds of string representations. This
    /// is a type-transparent comparator for all string representations that are implicitly
    /// convertable to string views.
    /// @tparam CharType Type of character in each string, either narrow or wide.
    template <typename CharType> struct CaseInsensitiveLessThanComparator
    {
      using is_transparent = void;

      constexpr bool operator()(
          const std::basic_string_view<CharType>& lhs,
          const std::basic_string_view<CharType>& rhs) const
      {
        return (CompareCaseInsensitive(lhs, rhs) < 0);
      }
    };

    /// Captures the state of a tokenization operation and exposes it via an iterator interface.
    /// Intended to be constructed directly within a range-based loop to provide simple
    /// iteration over all the tokens in a string.
    template <typename CharType> class Tokenizer
    {
    public:

      /// Iterator type used to hold the complete state of a tokenization operator.
      /// Implements the minimum functionality needed for single-step forward iteration
      /// through the tokens of a string.
      class Iterator
      {
      public:

        /// Iterator state indicator to be used by all one-past-the-end iterators.
        static constexpr size_t kTokenizeStateEnd = std::numeric_limits<size_t>::max();

        constexpr Iterator(
            const Tokenizer& tokenizer,
            size_t tokenizeState = 0,
            std::optional<std::basic_string_view<CharType>> token = std::nullopt)
            : tokenizer(tokenizer), tokenizeState(tokenizeState), token(token)
        {}

        constexpr const std::basic_string_view<CharType>& operator*(void) const
        {
          return *token;
        }

        constexpr Iterator& operator++(void)
        {
          if (nullptr != tokenizer.multiDelimiters)
            token = Tokenize(
                tokenizeState,
                tokenizer.stringToTokenize,
                tokenizer.multiDelimiters,
                tokenizer.numDelimiters);
          else
            token = Tokenize(tokenizeState, tokenizer.stringToTokenize, tokenizer.delimiter);

          if (false == token.has_value()) tokenizeState = kTokenizeStateEnd;

          return *this;
        }

        constexpr bool operator==(const Iterator& other) const
        {
          DebugAssert(
              &tokenizer == &(other.tokenizer),
              "Iterators refer to different tokenization operations.");
          return ((tokenizeState == other.tokenizeState) && (token == other.token));
        }

      private:

        /// Read-only reference to the tokenizer object itself.
        const Tokenizer& tokenizer;

        /// Current internal state of the tokenization operation.
        size_t tokenizeState;

        /// Last token that was successfully tokenized.
        std::optional<std::basic_string_view<CharType>> token;
      };

      constexpr Tokenizer(
          std::basic_string_view<CharType> stringToTokenize,
          std::basic_string_view<CharType> delimiter)
          : stringToTokenize(stringToTokenize),
            delimiter(delimiter),
            multiDelimiters(nullptr),
            numDelimiters(1)
      {}

      constexpr Tokenizer(
          std::basic_string_view<CharType> stringToTokenize,
          const std::basic_string<CharType>& delimiter)
          : Tokenizer(stringToTokenize, std::basic_string_view<CharType>(delimiter))
      {}

      constexpr Tokenizer(
          std::basic_string_view<CharType> stringToTokenize, const CharType* delimiter)
          : Tokenizer(stringToTokenize, std::basic_string_view<CharType>(delimiter))
      {}

      constexpr Tokenizer(
          std::basic_string_view<CharType> stringToTokenize,
          const std::basic_string_view<CharType>* delimiters,
          unsigned int numDelimiters)
          : stringToTokenize(stringToTokenize),
            delimiter(),
            multiDelimiters(delimiters),
            numDelimiters(numDelimiters)
      {}

      Iterator begin(void) const
      {
        return ++Iterator(*this);
      }

      Iterator end(void) const
      {
        return Iterator(*this, Iterator::kTokenizeStateEnd);
      }

      /// Instantiates a tokenizer object configured to tokenize a special type of list of strings
      /// used throughout Windows APIs. These lists place multiple strings into a single buffer and
      /// delimit them using a single null character, with the end of the list being identified by
      /// multiple consecutive null characters.
      static constexpr Tokenizer NullDelimitedList(
          std::basic_string_view<CharType> stringToTokenize)
      {
        static constexpr CharType kNullDelimiter = static_cast<CharType>(0);

        return Tokenizer(
            RemoveTrailing(stringToTokenize, kNullDelimiter),
            std::wstring_view(&kNullDelimiter, 1));
      }

    private:

      /// String to be tokenized.
      const std::basic_string_view<CharType> stringToTokenize;

      /// Single delimiter to be used for tokenization.
      /// To be filled if this object is constructed with a single delimiter.
      const std::basic_string_view<CharType> delimiter;

      /// Pointer to an array of multiple delimiters to be used for tokenization.
      /// To be filled if this object is constructed with multiple delimiters.
      const std::basic_string_view<CharType>* const multiDelimiters;

      /// Number of delimiters.
      const unsigned int numDelimiters;
    };
  } // namespace Strings
} // namespace Infra
