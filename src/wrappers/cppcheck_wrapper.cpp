//--------------------------------------------------------------------------------------------------
// Copyright (c) 2024 Marcus Geelnard
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

#include <wrappers/cppcheck_wrapper.hpp>

#include <base/debug_utils.hpp>
#include <base/file_utils.hpp>
#include <base/unicode_utils.hpp>

#include <set>
#include <stdexcept>

namespace bcache {
namespace {
// Tick this to a new number if the format has changed in a non-backwards-compatible way.
const std::string HASH_VERSION = "1";

bool is_source_file(const std::string& arg) {
  static const std::set<std::string> source_exts = {
      ".cpp", ".cxx", ".cc", ".c++", ".c", ".ipp", ".ixx", ".tpp", ".txx"};
  const auto ext = lower_case(file::get_extension(arg));
  return source_exts.find(ext) != source_exts.end();
}

bool is_two_part_arg(const std::string& arg) {
  static const std::set<std::string> first_of_two = {"-D", "-U", "-I", "-i", "-j", "-l"};
  return first_of_two.find(arg) != first_of_two.end();
}

bool is_supported_arg(const std::string& arg) {
  static const std::set<std::string> supported = {
      // "--addon",
      // "--addon-python",
      // "--cppcheck-build-dir",
      // "--check-config,
      "--check-level",
      "--check-library",
      // "--checkers-report",
      // "--clang",
      // "--config-exclude".
      // "--config-excludes-file",
      "--disable",
      // "--dump",
      "-D",
      // "-E",
      "--enable",
      "--error-exitcode",
      // "--errorlist",
      "--exitcode-suppressions",
      "--file-filter",
      // "--file-list",
      "-f",
      "--force",
      "--fsigned-char",
      "--funsigned-char",
      // "-h",
      // "--help",
      "-I",
      // "--includes-file",
      // "--include",
      "-i",
      "--inconclusive",
      "--inline-suppr",
      // "-j",
      // "-l",
      "--language",
      // "--library",
      "--max-configs",
      "--max-ctu-depth",
      "--output-file",
      "--platform",
      // "--plist-output",
      "--premium",
      // "--project",
      // "--project-configuration",
      "-q",
      "--quiet",
      "-rp",
      "--relative-paths",
      // "--report-progress",
      "--rule",
      // "--rule-file",
      "--showtime",
      "--std",
      "--suppress",
      // "--suppressions-list",
      // "--suppress-xml",
      "--template",
      "--template-location",
      "-U",
      "-v",
      "--verbose",
      // "--version",
      "--xml",
  };
  return (supported.find(arg) != supported.end()) || is_source_file(arg);
}
}  // Namespace

string_list_t cppcheck_wrapper_t::arg_pair_t::get() const {
  string_list_t result;
  if (equal_separator) {
    result += (arg + "=" + opt);
  } else {
    result += arg;
    if (!opt.empty()) {
      result += opt;
    }
  }
  return result;
}

cppcheck_wrapper_t::cppcheck_wrapper_t(const file::exe_path_t& exe_path, const string_list_t& args)
    : program_wrapper_t(exe_path, args) {
}

void cppcheck_wrapper_t::parse_arguments() {
  m_arg_pairs.clear();

  // Note: We always skip the first "arg" since it is the program name.
  for (unsigned i = 1; i < m_args.size(); ++i) {
    const auto& arg = m_args[i];
    if (is_two_part_arg(arg) && (i + 1) < m_args.size()) {
      m_arg_pairs.emplace_back(arg_pair_t{arg, m_args[i + 1], false});
      ++i;
    } else {
      // Can this argument be split into a pair (enforce consistent arguments)?
      const auto first_two_chars = arg.substr(0, 2);
      if (is_two_part_arg(first_two_chars)) {
        m_arg_pairs.emplace_back(arg_pair_t{first_two_chars, arg.substr(2), false});
      } else {
        // Is this an argument with an equals sign in it that we can split?
        const auto eq_pos = arg.find('=');
        if (eq_pos != std::string::npos) {
          m_arg_pairs.emplace_back(arg_pair_t{arg.substr(0, eq_pos), arg.substr(eq_pos + 1), true});
        } else {
          // This is a plain single argument.
          m_arg_pairs.emplace_back(arg_pair_t{arg, std::string(), false});
        }
      }
    }
  }

  // Check that we only have supported arguments.
  for (const auto& arg_pair : m_arg_pairs) {
    if (!is_supported_arg(arg_pair.arg)) {
      throw std::runtime_error("Unsupported argument: " + arg_pair.get().join(" "));
    }
  }
}

string_list_t cppcheck_wrapper_t::make_preprocessor_cmd() {
  string_list_t preprocess_args;

  // Start with the program.
  preprocess_args += m_args[0];

  // Drop arguments that we do not want/need.
  for (const auto& arg_pair : m_arg_pairs) {
    bool drop_this_arg = false;
    if (arg_pair.arg == "--output-file") {
      drop_this_arg = true;
    }
    if (!drop_this_arg) {
      preprocess_args += arg_pair.get();
    }
  }

  // Append the required arguments for producing preprocessed output.
  preprocess_args += std::string("-E");

  return preprocess_args;
}

void cppcheck_wrapper_t::resolve_args() {
  // Use the default resolver.
  program_wrapper_t::resolve_args();

  // Parse the arguments into a more intelligble form to be used internally.
  parse_arguments();
}

bool cppcheck_wrapper_t::can_handle_command() {
  // Is Cppcheck being invoked?
  const auto cmd = lower_case(file::get_file_part(m_exe_path.real_path(), false));
  return cmd.find("cppcheck") != std::string::npos;
}

std::map<std::string, expected_file_t> cppcheck_wrapper_t::get_build_files() {
  std::map<std::string, expected_file_t> files;
  auto found_output_file = false;
  for (const auto& arg_pair : m_arg_pairs) {
    if (arg_pair.arg == "--output-file") {
      if (found_output_file) {
        throw std::runtime_error("Only a single output file can be specified.");
      }
      files["output_file"] = {arg_pair.opt, true};
    }
  }
  return files;
}

std::string cppcheck_wrapper_t::get_program_id() {
  // Get the version string for the compiler.
  string_list_t version_args;
  version_args += m_args[0];
  version_args += "--version";
  const auto result = sys::run(version_args);
  if (result.return_code != 0) {
    throw std::runtime_error("Unable to get the Cppcheck version information string.");
  }

  // Prepend the hash format version.
  auto id = HASH_VERSION + result.std_out;

  return id;
}

string_list_t cppcheck_wrapper_t::get_relevant_arguments() {
  string_list_t filtered_args;

  // The first argument is the compiler binary without the path.
  filtered_args += file::get_file_part(m_args[0]);

  for (const auto& arg_pair : m_arg_pairs) {
    // Generally unwanted argument (things that will not change how we go from preprocessed code
    // to analysis result)?
    // Note: We deliberately include the source file path, as it is printed either as a relative
    // path or an absolute path in the output as part of error messages, depending on how it is
    // given on the command line.
    // TODO(m): Derive the source file path in the same way as Cppcheck does it, and use that as
    // part of the hash instead of the path passed on the command line.
    const bool is_unwanted =
        (arg_pair.arg == "-I") || (arg_pair.arg == "-D") || (arg_pair.arg == "-U");
    if (!is_unwanted) {
      if (arg_pair.arg == "--output-file") {
        // Special case: We want to know that we used --output-file, as it affects the program
        // output, but we are NOT interested in the output file name at this stage.
        filtered_args += arg_pair.arg;
      } else {
        filtered_args += arg_pair.get();
      }
    }
  }

  debug::log(debug::DEBUG) << "Filtered arguments: " << filtered_args.join(" ", true);

  return filtered_args;
}

std::map<std::string, std::string> cppcheck_wrapper_t::get_relevant_env_vars() {
  // TODO(m): What environment variables can affect the analysis result?
  std::map<std::string, std::string> env_vars;
  return env_vars;
}

std::string cppcheck_wrapper_t::preprocess_source() {
  // Run the preprocessor step.
  const auto preprocessor_args = make_preprocessor_cmd();
  auto result = sys::run(preprocessor_args);
  if (result.return_code != 0) {
    throw std::runtime_error("Preprocessing command was unsuccessful.");
  }

  // Return the preprocessed output.
  return result.std_out;
}
}  // namespace bcache
