//--------------------------------------------------------------------------------------------------
// Copyright (c) 2019 Marcus Geelnard
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

#include <base/file_utils.hpp>

#include <doctest/doctest.h>

#include <string>

// Workaround for macOS build errors.
// See: https://github.com/onqtam/doctest/issues/126
#include <iostream>

using namespace bcache;

TEST_CASE("tmp_file_t produces expected results") {
  SUBCASE("A full path is constructed properly") {
    const std::string base_path = file::append_path("hello", "world");
    const std::string ext = ".myext";

    const auto result = file::tmp_file_t(base_path, ext);

    // The base path and the extension are part of the final file name (and in the right places).
    CHECK_EQ(result.path().find(base_path), 0);
    CHECK_EQ(result.path().find(ext), result.path().size() - ext.size());

    // The file name contains some temporary string part.
    const auto min_expected_size = base_path.size() + ext.size() + 6;
    CHECK_GT(result.path().size(), min_expected_size);
  }

  SUBCASE("Two files are created and deleted") {
    const std::string base_path = file::get_temp_dir();
    const std::string ext = ".foo";

    std::string tmp1_path;
    std::string tmp2_path;
    {
      const auto tmp1 = file::tmp_file_t(base_path, ext);
      const auto tmp2 = file::tmp_file_t(base_path, ext);
      tmp1_path = tmp1.path();
      tmp2_path = tmp2.path();

      // Create the first file.
      file::write("Hello world!", tmp1.path());

      // The first file, but not the second file, should exist.
      CHECK_EQ(file::file_exists(tmp1_path), true);
      CHECK_EQ(file::file_exists(tmp2_path), false);

      // Create the first file.
      file::write("Hello world!", tmp2.path());

      // Both files should exist.
      CHECK_EQ(file::file_exists(tmp1_path), true);
      CHECK_EQ(file::file_exists(tmp2_path), true);
    }

    // After the tmp_file_t objects go out of scope, both files should be deleted.
    CHECK_EQ(file::file_exists(tmp1_path), false);
    CHECK_EQ(file::file_exists(tmp2_path), false);
  }

  SUBCASE("A directory is created and completely removed") {
    const std::string base_path = file::get_temp_dir();
    const std::string ext;

    std::string tmp_dir_path;
    std::string tmp_file_path;
    {
      const auto tmp = file::tmp_file_t(base_path, ext);
      tmp_dir_path = tmp.path();
      tmp_file_path = file::append_path(tmp_dir_path, "hello.foo");

      // Create the directory and a file in the dir.
      file::create_dir(tmp_dir_path);
      file::write("Hello world!", tmp_file_path);

      // The dir and the file should exist.
      CHECK_EQ(file::dir_exists(tmp_dir_path), true);
      CHECK_EQ(file::file_exists(tmp_file_path), true);
    }

    // After the tmp_file_t object goes out of scope, the file and the dir should be deleted.
    CHECK_EQ(file::dir_exists(tmp_dir_path), false);
    CHECK_EQ(file::file_exists(tmp_file_path), false);
  }
}

TEST_CASE("filter_t: ALL filter produces expected results") {
  SUBCASE("ALL filter works") {
    const file::filter_t filter = file::filter_t();

    CHECK_EQ(filter.keep("Hello!"), true);
    CHECK_EQ(filter.keep(""), true);
    CHECK_EQ(filter.keep("/foo/bar.lock"), true);
    CHECK_EQ(filter.keep("/foo/bar.locker"), true);
  }
}

TEST_CASE("filter_t: Inclusion filters produces expected results") {
  SUBCASE("INCLUDE EXTENSION filter works") {
    const file::filter_t filter = file::filter_t::include_extension(".lock");

    CHECK_EQ(filter.keep("Hello!"), false);
    CHECK_EQ(filter.keep(""), false);
    CHECK_EQ(filter.keep("/foo/bar.lock"), true);
    CHECK_EQ(filter.keep("/foo/bar.locker"), false);
  }

  SUBCASE("INCLUDE SUBSTRING filter works") {
    const file::filter_t filter = file::filter_t::include_substring(".lock");

    CHECK_EQ(filter.keep("Hello!"), false);
    CHECK_EQ(filter.keep(""), false);
    CHECK_EQ(filter.keep("/foo/bar.lock"), true);
    CHECK_EQ(filter.keep("/foo/bar.locker"), true);
  }
}

TEST_CASE("filter_t: Exclusion filters produces expected results") {
  SUBCASE("EXCLUDE EXTENSION filter works") {
    const file::filter_t filter = file::filter_t::exclude_extension(".lock");

    CHECK_EQ(filter.keep("Hello!"), true);
    CHECK_EQ(filter.keep(""), true);
    CHECK_EQ(filter.keep("/foo/bar.lock"), false);
    CHECK_EQ(filter.keep("/foo/bar.locker"), true);
  }

  SUBCASE("EXCLUDE SUBSTRING filter works") {
    const file::filter_t filter = file::filter_t::exclude_substring(".lock");

    CHECK_EQ(filter.keep("Hello!"), true);
    CHECK_EQ(filter.keep(""), true);
    CHECK_EQ(filter.keep("/foo/bar.lock"), false);
    CHECK_EQ(filter.keep("/foo/bar.locker"), false);
  }
}

TEST_CASE("append_path produces expected results") {
  SUBCASE("A full path is constructed properly") {
    const std::string part_1 = "hello";
    const std::string part_2 = "world";
    const std::string result = file::append_path(part_1, part_2);

    // The file name contains some temporary string part.
    const auto expected_size = part_1.size() + part_2.size() + 1;
    CHECK_EQ(result.size(), expected_size);
  }

  SUBCASE("An empty dir part results in the file part alone") {
    const std::string part_1;
    const std::string part_2 = "world";
    const std::string result = file::append_path(part_1, part_2);
    CHECK_EQ(result, part_2);
  }

  SUBCASE("An empty file part results in the dir part alone") {
    const std::string part_1 = "hello";
    const std::string part_2;
    const std::string result = file::append_path(part_1, part_2);
    CHECK_EQ(result, part_1);
  }
}

TEST_CASE("get_dir_part produces expected results") {
  SUBCASE("The dir is extracted when it exists") {
    const std::string part_1 = "hello";
    const std::string part_2 = "world";
    const std::string path = file::append_path(part_1, part_2);
    const std::string result = file::get_dir_part(path);
    CHECK_EQ(result, part_1);
  }

  SUBCASE("An empty string is returned when no dir part exists") {
    const std::string path = "world";
    const std::string result = file::get_dir_part(path);
    CHECK_EQ(result.size(), 0);
  }
}

#ifdef _WIN32
TEST_CASE("dir_exists produces expected results") {
  SUBCASE("A Windows drive letter is assumed to exist.") {
    CHECK_EQ(file::dir_exists("c:"), true);
  }
}
#endif

TEST_CASE("get_file_part produces expected results") {
  SUBCASE("The file is extracted when it exists") {
    const std::string part_1 = "hello";
    const std::string part_2 = "world";
    const std::string path = file::append_path(part_1, part_2);
    const std::string result = file::get_file_part(path);
    CHECK_EQ(result, part_2);
  }

  SUBCASE("The entire string is returned when no dir part exists") {
    const std::string path = "world";
    const std::string result = file::get_file_part(path);
    CHECK_EQ(result, path);
  }
}

TEST_CASE("get_extension produces expected results") {
  SUBCASE("Simple extension") {
    const std::string ext = ".ext";
    const std::string path = file::append_path("hello", "world") + ext;
    const std::string result = file::get_extension(path);
    CHECK_EQ(result, ext);
  }

  SUBCASE("Only the last of multiple extensions is returned") {
    const std::string ext = ".ext";
    const std::string path = file::append_path("hello", "world") + ".some.other.parts" + ext;
    const std::string result = file::get_extension(path);
    CHECK_EQ(result, ext);
  }
}

TEST_CASE("get_unique_id produces expected results") {
  SUBCASE("No repetition") {
    // Generate some unique ID:s.
    const int NUM_IDS = 1000;
    std::vector<std::string> ids(NUM_IDS);
    for (auto& id : ids) {
      id = file::get_unique_id();
    }

    // Make sure that none of them are the same.
    for (int i = 0; i < NUM_IDS; ++i) {
      for (int j = i + 1; j < NUM_IDS; ++j) {
        CHECK_NE(ids[i], ids[j]);
      }
    }
  }
}

TEST_CASE("Canonicalizing paths works as expected") {
#if defined(_WIN32)
  SUBCASE("Absolute path 1") {
    const auto path = file::canonicalize_path(R"(C:\foo\.\.\bar\.)");
    CHECK_EQ(path, R"(C:\foo\bar)");
  }

  SUBCASE("Absolute path 2") {
    const auto path = file::canonicalize_path(R"(C:\foo\.\..\bar\.)");
    CHECK_EQ(path, R"(C:\bar)");
  }

  SUBCASE("Absolute path 3") {
    const auto path = file::canonicalize_path(R"(C:\foo\.\\\..\bar\..)");
    CHECK_EQ(path, R"(C:\)");
  }

  SUBCASE("Absolute path 4") {
    const auto path = file::canonicalize_path(R"(c:\foo/bar\)");
    CHECK_EQ(path, R"(C:\foo\bar)");
  }
#else
  SUBCASE("Absolute path 1") {
    const auto path = file::canonicalize_path("/foo/././bar/.");
    CHECK_EQ(path, "/foo/bar");
  }

  SUBCASE("Absolute path 2") {
    const auto path = file::canonicalize_path("/foo/./../bar/.");
    CHECK_EQ(path, "/bar");
  }

  SUBCASE("Absolute path 3") {
    const auto path = file::canonicalize_path("/foo/.///../bar/..");
    CHECK_EQ(path, "/");
  }

  SUBCASE("Absolute path 4") {
    const auto path = file::canonicalize_path("/foo/bar/");
    CHECK_EQ(path, "/foo/bar");
  }
#endif
}

TEST_CASE("Making relative paths works as expected") {
#if defined(_WIN32)
  SUBCASE("Relative path 1") {
    const auto path = file::relative_path(R"(C:\)", R"(C:\foo\.\.\bar\.)");
    CHECK_EQ(path, R"(foo\bar)");
  }

  SUBCASE("Relative path 2") {
    const auto path = file::relative_path(R"(C:\foo)", R"(C:\foo\.\.\bar\.)");
    CHECK_EQ(path, R"(bar)");
  }

  SUBCASE("Relative path 3") {
    const auto path = file::relative_path(R"(foo2)", R"(foo\.\.\bar\.)");
    CHECK_EQ(path, R"(..\foo\bar)");
  }

  SUBCASE("Relative path 4") {
    const auto path = file::relative_path(R"(C:\)", R"(C:\hello\world)");
    CHECK_EQ(path, R"(hello\world)");
  }

  SUBCASE("Relative path 5") {
    const auto path = file::relative_path(R"(C:\hello\..\world)", R"(C:\hello\world)");
    CHECK_EQ(path, R"(..\hello\world)");
  }

  SUBCASE("Relative path 6") {
    const auto path = file::relative_path(R"(X:\)", R"(C:\foo\.\.\bar\.)");
    CHECK_EQ(path, R"(C:\foo\bar)");
  }
#else
  SUBCASE("Relative path 1") {
    const auto path = file::relative_path("/", "/foo/././bar/.");
    CHECK_EQ(path, "foo/bar");
  }

  SUBCASE("Relative path 2") {
    const auto path = file::relative_path("/foo", "/foo/././bar/.");
    CHECK_EQ(path, "bar");
  }

  SUBCASE("Relative path 3") {
    const auto path = file::relative_path("foo2", "foo/././bar/.");
    CHECK_EQ(path, "../foo/bar");
  }

  SUBCASE("Relative path 4") {
    const auto path = file::relative_path("/", "/hello/world");
    CHECK_EQ(path, "hello/world");
  }

  SUBCASE("Relative path 5") {
    const auto path = file::relative_path("/hello/../world", "/hello/world");
    CHECK_EQ(path, "../hello/world");
  }
#endif
}

TEST_CASE("Set and get current working directory") {
  // Remember the current working directory.
  const auto old_cwd = file::get_cwd();
  try {
    // Change to a new working directory.
    file::set_cwd(file::get_temp_dir());

    // Check that we have changed the working directory.
    CHECK_NE(old_cwd, file::get_cwd());

    // Change back - we should now be in the old CWD.
    file::set_cwd(old_cwd);
    CHECK_EQ(old_cwd, file::get_cwd());
  } catch (...) {
    // ...just in case things go wrong.
    file::set_cwd(old_cwd);
    FAIL("Something went wrong");
  }
}

TEST_CASE("Scoped working directory") {
  // Remember the current working directory.
  const auto old_cwd = file::get_cwd();

  {
    // Change to a new working dir using scoped_work_dir_t.
    file::scoped_work_dir_t scoped_work_dir(file::get_temp_dir());

    // Check that we have changed the working directory.
    CHECK_NE(old_cwd, file::get_cwd());
  }

  // We should now be in the old CWD.
  CHECK_EQ(old_cwd, file::get_cwd());
}
