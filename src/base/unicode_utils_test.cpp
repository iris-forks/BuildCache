//--------------------------------------------------------------------------------------------------
// Copyright (c) 2021 Marcus Geelnard
//
// This software is provided 'as-is', without any express or implied warranty. In no event will the
// authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose, including commercial
// applications, and to alter it and redistribute it freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not claim that you wrote
//     the original software. If you use this software in a product, an acknowledgment in the
//     product documentation would be appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be misrepresented as
//     being the original software.
//
//  3. This notice may not be removed or altered from any source distribution.
//--------------------------------------------------------------------------------------------------

#include <base/unicode_utils.hpp>

#include <doctest/doctest.h>

#include <string>

// Workaround for macOS build errors.
// See: https://github.com/onqtam/doctest/issues/126
#include <iostream>

using namespace bcache;

TEST_CASE("ucs2_to_utf8() produces expected results") {
  SUBCASE("Case 1 - Hello world") {
    const std::wstring str = L"Hello world";
    const auto result = ucs2_to_utf8(str);
    CHECK_EQ(result, "Hello world");
  }

  SUBCASE("Case 2 - Привет мир") {
    const std::wstring str{1055, 1088, 1080, 1074, 1077, 1090, 32, 1084, 1080, 1088};
    const auto result = ucs2_to_utf8(str);
    CHECK_EQ(result, "Привет мир");
  }
}

TEST_CASE("ucs2_to_utf8() with range produces expected results") {
  SUBCASE("Case 1 - Sub-range") {
    const std::wstring str = L"Hello world";
    const auto result = ucs2_to_utf8(&str[6], &str[8]);
    CHECK_EQ(result, "wo");
  }

  SUBCASE("Case 2 - Negative range") {
    const std::wstring str = L"Hello world";
    const auto result = ucs2_to_utf8(&str[8], &str[6]);
    CHECK_EQ(result, "");
  }
}

TEST_CASE("utf8_to_ucs2() produces expected results") {
  SUBCASE("Case 1 - Hello world") {
    const std::string str = "Hello world";
    const auto result = utf8_to_ucs2(str);
    CHECK_EQ(result, L"Hello world");
  }

  SUBCASE("Case 2 - Привет мир") {
    const std::string str = "Привет мир";
    const auto result = utf8_to_ucs2(str);
    const std::wstring expected{1055, 1088, 1080, 1074, 1077, 1090, 32, 1084, 1080, 1088};
    CHECK_EQ(result, expected);
  }
}

TEST_CASE("lower_case() produces expected results") {
  SUBCASE("Case 1 - Character") {
    const char c = 'X';
    const auto result = lower_case(c);
    CHECK_EQ(result, 'x');
  }

  SUBCASE("Case 2 - String") {
    const std::string str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234";
    const auto result = lower_case(str);
    CHECK_EQ(result, "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz01234");
  }
}

TEST_CASE("upper_case() produces expected results") {
  SUBCASE("Case 1 - Character") {
    const char c = 'x';
    const auto result = upper_case(c);
    CHECK_EQ(result, 'X');
  }

  SUBCASE("Case 2 - String") {
    const std::string str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01234";
    const auto result = upper_case(str);
    CHECK_EQ(result, "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ01234");
  }
}

TEST_CASE("lstrip() produces expected results") {
  SUBCASE("Case 1 - space") {
    const std::string str = "  Hello world  ";
    const auto result = lstrip(str);
    CHECK_EQ(result, "Hello world  ");
  }

  SUBCASE("Case 2 - tab") {
    const std::string str = "\t Hello world \t";
    const auto result = lstrip(str);
    CHECK_EQ(result, "Hello world \t");
  }

  SUBCASE("Case 3 - newline") {
    const std::string str = "\n Hello world \n";
    const auto result = lstrip(str);
    CHECK_EQ(result, "Hello world \n");
  }

  SUBCASE("Case 4 - carriage return") {
    const std::string str = "\r Hello world \r";
    const auto result = lstrip(str);
    CHECK_EQ(result, "Hello world \r");
  }
}

TEST_CASE("rstrip() produces expected results") {
  SUBCASE("Case 1 - space") {
    const std::string str = "  Hello world  ";
    const auto result = rstrip(str);
    CHECK_EQ(result, "  Hello world");
  }

  SUBCASE("Case 2 - tab") {
    const std::string str = "\t Hello world \t";
    const auto result = rstrip(str);
    CHECK_EQ(result, "\t Hello world");
  }

  SUBCASE("Case 3 - newline") {
    const std::string str = "\n Hello world \n";
    const auto result = rstrip(str);
    CHECK_EQ(result, "\n Hello world");
  }

  SUBCASE("Case 4 - carriage return") {
    const std::string str = "\r Hello world \r";
    const auto result = rstrip(str);
    CHECK_EQ(result, "\r Hello world");
  }
}

TEST_CASE("strip() produces expected results") {
  SUBCASE("Case 1 - space") {
    const std::string str = "  Hello world  ";
    const auto result = strip(str);
    CHECK_EQ(result, "Hello world");
  }

  SUBCASE("Case 2 - tab") {
    const std::string str = "\t Hello world \t";
    const auto result = strip(str);
    CHECK_EQ(result, "Hello world");
  }

  SUBCASE("Case 3 - newline") {
    const std::string str = "\n Hello world \n";
    const auto result = strip(str);
    CHECK_EQ(result, "Hello world");
  }

  SUBCASE("Case 4 - carriage return") {
    const std::string str = "\r Hello world \r";
    const auto result = strip(str);
    CHECK_EQ(result, "Hello world");
  }
}

TEST_CASE("starts_with() produces expected results") {
  SUBCASE("Case 1 - short match") {
    const std::string str = "Here is a string";
    const auto result = starts_with(str, "He");
    CHECK_EQ(result, true);
  }

  SUBCASE("Case 2 - full match") {
    const std::string str = "Here is a string";
    const auto result = starts_with(str, "Here is a string");
    CHECK_EQ(result, true);
  }

  SUBCASE("Case 3 - too long substring") {
    const std::string str = "Here is a string";
    const auto result = starts_with(str, "Here is a string with more words");
    CHECK_EQ(result, false);
  }

  SUBCASE("Case 4 - no match") {
    const std::string str = "Here is a string";
    const auto result = starts_with(str, "ere is a");
    CHECK_EQ(result, false);
  }
}

TEST_CASE("ends_with() produces expected results") {
  SUBCASE("Case 1 - short match") {
    const std::string str = "Here is a string";
    const auto result = ends_with(str, "ing");
    CHECK_EQ(result, true);
  }

  SUBCASE("Case 2 - full match") {
    const std::string str = "Here is a string";
    const auto result = ends_with(str, "Here is a string");
    CHECK_EQ(result, true);
  }

  SUBCASE("Case 3 - too long substring") {
    const std::string str = "Here is a string";
    const auto result = ends_with(str, "Aaand Here is a string");
    CHECK_EQ(result, false);
  }

  SUBCASE("Case 4 - no match") {
    const std::string str = "Here is a string";
    const auto result = ends_with(str, "is a strin");
    CHECK_EQ(result, false);
  }
}
