//--------------------------------------------------------------------------------------------------
// Copyright (c) 2024 Andreas Farre, Marcus Geelnard
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

#ifndef BUILDCACHE_RUST_WRAPPER_HPP_
#define BUILDCACHE_RUST_WRAPPER_HPP_

#include <base/env_utils.hpp>
#include <base/string_list.hpp>
#include <wrappers/program_wrapper.hpp>

#include <string>

namespace bcache {
/// @brief A program wrapper for cargo-invoked rustc.
class rust_wrapper_t final : public program_wrapper_t {
public:
  rust_wrapper_t(const file::exe_path_t& exe_path, const string_list_t& args);

  bool can_handle_command() override;

private:
  void resolve_args() override;
  string_list_t get_capabilities() override;
  std::map<std::string, expected_file_t> get_build_files() override;
  std::string get_program_id() override;
  string_list_t get_relevant_arguments() override;
  std::map<std::string, std::string> get_relevant_env_vars() override;
  string_list_t get_input_files() override;
  string_list_t get_implicit_input_files() override;

  string_list_t parse_options(const string_list_t& unresolved_arguments);
  void process_implicit_input_files_and_relevant_env_vars();
  void panic(const std::string& message);

  string_list_t m_relevant_args;
  std::map<std::string, std::string> m_relevant_env_vars;
  string_list_t m_implicit_input_files;

  std::string m_output_dir;
  string_list_t m_externs;
  string_list_t m_static_libraries;
  std::string m_crate_name;
  std::string m_dep_info;
  string_list_t m_emit;
  std::string m_input;
};
}  // namespace bcache
#endif  // BUILDCACHE_RUST_WRAPPER_HPP_
