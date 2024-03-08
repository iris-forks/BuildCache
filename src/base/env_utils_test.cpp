//--------------------------------------------------------------------------------------------------
// Copyright (c) 2020 Marcus Geelnard
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

#include <base/env_utils.hpp>
#include <base/string_list.hpp>
#include <base/unicode_utils.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

// Workaround for macOS build errors.
// See: https://github.com/onqtam/doctest/issues/126
#include <iostream>

using namespace bcache;

TEST_CASE("Environment variable manipulation") {
  SUBCASE("Define, read and undefine a variable") {
    // Define the variable.
    const std::string name("MyTestVariable");
    const std::string value("abcd");
    set_env(name, value);

    // The variable is defined.
    CHECK_EQ(env_defined(name), true);

    // The variable has the correct value.
    CHECK_EQ(get_env(name), value);

    // Undefine the variable.
    unset_env(name);

    // The variable is no longer defined.
    CHECK_EQ(env_defined(name), false);
  }

  SUBCASE("Unicode names and values work") {
    // Define the variable.
    const std::string name(reinterpret_cast<const char*>(u8"БуилдЦаче"));
    const std::string value(reinterpret_cast<const char*>(u8"είναι υπέροχο"));
    set_env(name, value);

    // The variable is defined.
    CHECK_EQ(env_defined(name), true);

    // The variable has the correct value.
    CHECK_EQ(get_env(name), value);

    // Undefine the variable.
    unset_env(name);

    // The variable is no longer defined.
    CHECK_EQ(env_defined(name), false);
  }

  SUBCASE("Accessing environ/_wenviron should work") {
    // Define the variables.
    static const auto& name_values = {
        std::make_pair("MyTestVariable", "abcd"),
        std::make_pair(reinterpret_cast<const char*>(u8"БуилдЦаче"),
                       reinterpret_cast<const char*>(u8"είναι υπέροχο"))};

    for (const auto& entry : name_values) {
      set_env(entry.first, entry.second);
    }

    std::vector<string_list_t> results;
    for (const auto& entry : get_env()) {
      results.push_back(string_list_t(entry, "="));
    }

    for (const auto& entry : name_values) {
      const auto& it =
          std::find_if(results.begin(), results.end(), [&entry](const string_list_t& env_entry) {
            return env_entry[0] == entry.first;
          });
      // All defined variables should be found.
      CHECK_NE(it, results.end());
      // All defined variables should have a value.
      CHECK_EQ(it->size(), 2);
      // All defined variables should have the correct value.
      CHECK_EQ((*it)[1], entry.second);
    }

    for (const auto& entry : name_values) {
      unset_env(entry.first);
    }

    // The variables are no longer defined.
    for (const auto& entry : get_env()) {
      string_list_t env_entry(entry, "=");
      CHECK_EQ(std::find_if(name_values.begin(),
                            name_values.end(),
                            [&env_entry](const std::pair<const char*, const char*>& name_value) {
                              return name_value.first == env_entry[0];
                            }),
               name_values.end());
    }
  }
}

TEST_CASE("Parse environment variables") {
  SUBCASE("String parsing") {
    const std::string name("A_STRING_VARIABLE");

    // Simple ASCII string with spaces.
    {
      set_env(name, "Hello world!");
      env_var_t var(name);
      CHECK_EQ(var.as_string(), std::string("Hello world!"));
    }

    // An undefined variable.
    {
      unset_env(name);
      env_var_t var(name);
      CHECK_EQ(var.as_string(), std::string());
    }

    // Cleanup.
    unset_env(name);
  }

  SUBCASE("Integer parsing") {
    const std::string name("AN_INTEGER_VARIABLE");

    // Positive, large integer.
    {
      set_env(name, "6542667823978");
      env_var_t var(name);
      CHECK_EQ(var.as_int64(), 6542667823978LL);
    }

    // Negative, large integer.
    {
      set_env(name, "-1234567894561324");
      env_var_t var(name);
      CHECK_EQ(var.as_int64(), -1234567894561324LL);
    }

    // Cleanup.
    unset_env(name);
  }

  SUBCASE("Boolean parsing") {
    const std::string name("A_BOOLEAN_VARIABLE");

    // Truthy value: "TRUE".
    {
      set_env(name, "TRUe");
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), true);
    }

    // Truthy value: "ON".
    {
      set_env(name, "On");
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), true);
    }

    // Truthy value: "YES".
    {
      set_env(name, "yES");
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), true);
    }

    // Truthy value: "1".
    {
      set_env(name, "1");
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), true);
    }

    // Truthy value: Some random unrecognized string.
    {
      set_env(name, "Hello world!");
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), true);
    }

    // Falsy value: "FALSE".
    {
      set_env(name, "FaLSe");
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), false);
    }

    // Falsy value: "OFF".
    {
      set_env(name, "OfF");
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), false);
    }

    // Falsy value: "NO".
    {
      set_env(name, "No");
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), false);
    }

    // Falsy value: "0".
    {
      set_env(name, "0");
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), false);
    }

    // Falsy value: An empty string.
    {
      set_env(name, "");
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), false);
    }

    // Falsy value: An undefined environment variable.
    {
      unset_env(name);
      env_var_t var(name);
      CHECK_EQ(var.as_bool(), false);
    }

    // Cleanup.
    unset_env(name);
  }
}

TEST_CASE("env_var_t boolean operator behaves as expected") {
  const std::string name("A_VARIABLE");

  // A defined variable is defined.
  {
    set_env(name, "Something");
    env_var_t var(name);
    CHECK_EQ(var, true);
  }

  // An undefined variable is undefined.
  {
    unset_env(name);
    env_var_t var(name);
    CHECK_EQ(var, false);
  }

  // Cleanup.
  unset_env(name);
}

TEST_CASE("Scoped set_env") {
  const std::string name("A_VARIABLE");
  const std::string old_value("Lorem ipsum");
  const std::string value("Hello world!");

  // A non-existing variable is unset.
  unset_env(name);
  {
    scoped_set_env_t scoped_env(name, value);
    CHECK_EQ(env_defined(name), true);
    CHECK_EQ(get_env(name), value);
  }
  CHECK_EQ(env_defined(name), false);

  // Just in case...
  unset_env(name);

  // An existing variable is restored.
  set_env(name, old_value);
  {
    scoped_set_env_t scoped_env(name, value);
    CHECK_EQ(env_defined(name), true);
    CHECK_EQ(get_env(name), value);
  }
  CHECK_EQ(env_defined(name), true);
  CHECK_EQ(get_env(name), old_value);

  // Cleanup.
  unset_env(name);
}
