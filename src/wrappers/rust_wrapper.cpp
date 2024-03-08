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

#include <wrappers/rust_wrapper.hpp>

#include <base/env_utils.hpp>
#include <base/file_utils.hpp>
#include <base/hasher.hpp>
#include <base/string_list.hpp>
#include <base/unicode_utils.hpp>
#include <config/configuration.hpp>
#include <sys/sys_utils.hpp>

#include <algorithm>
#include <regex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>

namespace bcache {
namespace {
// Tick this to a new number if the format has changed in a non-backwards-compatible way.
const std::string HASH_VERSION = "1";

// Categories of options that can be passed to rustc and how we handle them.
enum class option_type_t {
  UNSUPPORTED,
  UNHANDLED,
  IGNORED,
  LIBRARY_PATH,
  LIBRARY,
  CRATE_TYPE,
  CRATE_NAME,
  EMIT,
  CODE_GEN,
  OUT_DIR,
  TARGET,
  EXTERN,
  RESPONSE_FILE,
  PATH,
};

// Does the option require an argument, either on the form of --option value, --option=value, -opt
// value and sometimes -optvalue.
enum class has_argument_t {
  NO,
  YES,
};

// The result of a parsed option. In case of failure, m_success is false and m_option contains
// the option we tried to parse;
struct option_info_t {
  bool m_success = false;
  option_type_t m_type;
  has_argument_t m_has_argument;
  std::string m_option;
  std::string m_argument;
};

// Figure out which category an option belongs to, and if it requires an argument.
void get_option_type(const std::string& argument, option_info_t& option_info) {
  const auto& NO = has_argument_t::NO;
  const auto& YES = has_argument_t::YES;
  const auto& UNSUPPORTED = option_type_t::UNSUPPORTED;
  const auto& UNHANDLED = option_type_t::UNHANDLED;
  const auto& IGNORED = option_type_t::IGNORED;
  const auto& CODE_GEN = option_type_t::CODE_GEN;

  static std::unordered_map<std::string, std::tuple<option_type_t, has_argument_t>>
      option_specification = {
          {"-", {UNSUPPORTED, NO}},
          {"-h", {UNHANDLED, NO}},
          {"--help", {UNHANDLED, NO}},
          {"--cfg", {IGNORED, YES}},
          {"-L", {option_type_t::LIBRARY_PATH, YES}},
          {"-l", {option_type_t::LIBRARY, YES}},
          {"--crate-type", {option_type_t::CRATE_TYPE, YES}},
          {"--crate-name", {option_type_t::CRATE_NAME, YES}},
          {"--edition", {IGNORED, YES}},
          {"--emit", {option_type_t::EMIT, YES}},
          {"--print", {UNHANDLED, YES}},
          {"-g", {CODE_GEN, NO}},
          {"-O", {CODE_GEN, NO}},
          {"-o", {UNSUPPORTED, YES}},
          {"--out-dir", {option_type_t::OUT_DIR, YES}},
          {"--explain", {UNHANDLED, YES}},
          {"--test", {UNHANDLED, NO}},
          {"--target", {option_type_t::TARGET, YES}},
          {"-A", {IGNORED, YES}},
          {"--allow", {IGNORED, YES}},
          {"-W", {IGNORED, YES}},
          {"--warn", {IGNORED, YES}},
          {"--force-warn", {IGNORED, YES}},
          {"-D", {IGNORED, YES}},
          {"--deny", {IGNORED, YES}},
          {"-F", {IGNORED, YES}},
          {"--forbid", {IGNORED, YES}},
          {"--cap-lints", {IGNORED, YES}},
          {"-C", {CODE_GEN, YES}},
          {"--codegen", {CODE_GEN, YES}},
          {"-V", {UNHANDLED, NO}},
          {"--version", {UNHANDLED, NO}},
          {"-v", {IGNORED, NO}},
          {"--verbose", {IGNORED, NO}},
          {"--extern", {option_type_t::EXTERN, YES}},
          {"--sysroot", {UNSUPPORTED, YES}},
          {"--error-format", {IGNORED, YES}},
          {"--json", {IGNORED, YES}},
          {"--color", {IGNORED, YES}},
          {"--diagnostic-width", {IGNORED, YES}},
          {"--remap-path-prefix", {UNSUPPORTED, YES}},
          {"@", {option_type_t::RESPONSE_FILE, NO}},
      };

  const auto& result = option_specification.find(argument);
  std::tie(option_info.m_type, option_info.m_has_argument) =
      (result != option_specification.end()) ? result->second
                                             : std::make_tuple(option_type_t::PATH, NO);
}

// Check if an option needs an argument.
bool needs_argument(const option_info_t& option_info) {
  return option_info.m_has_argument == has_argument_t::YES && option_info.m_argument.empty();
}

// Parse an option according to the specification in `get_option_type`. This should correspond to
// all options available to an invocation of rustc.
option_info_t parse_argument(const std::string& argument) {
  option_info_t result;

  // This parses all possible options:
  // 1) starting with "--" and either having " " or "=" as delimiter.
  // 2) starting with "-" and then one single character out of [hLlgOoAWDFCVv]
  // 3) the single character "-"
  // 4) starting with "@" followed by a string of non-whitespace characters
  // 5) a string of non-whitespace characters
  static std::regex argument_parser(
      R"(^(?:(--[^\s=]*)=(\S*))|(?:(-[hLlgOoAWDFCVv])(\S*))|(-)|(?:(@)(\S+))|(\S+)$)");
  std::smatch match;
  if (!std::regex_match(argument, match, argument_parser)) {
    result.m_option = argument;
    return result;
  }

  if (match[1].length() != 0) {
    result.m_option = match[1];
    result.m_argument = match[2];
  } else if (match[3].length() != 0) {
    result.m_option = match[3];
    result.m_argument = match[4];
  } else if (match[5].length() != 0) {
    result.m_option = "-";
  } else if (match[6].length() != 0) {
    result.m_option = match[6];
    result.m_argument = match[7];
  } else if (match[8].length() != 0) {
    result.m_option = match[8];
  } else {
    result.m_option = argument;
    return result;
  }

  result.m_success = true;

  get_option_type(result.m_option, result);

  return result;
}

// Helper for running rustc with a set of environment variables turned off.
sys::run_result_t run_rustc(const string_list_t& args, const bool quiet) {
  static const auto& unset = {
      "LD_PRELOAD",
      "RUNNING_UNDER_RR",
      "HOSTNAME",
      "PWD",
      "HOST",
      "RPM_BUILD_ROOT",
      "SOURCE_DATE_EPOCH",
      "RPM_PACKAGE_RELEASE",
      "MINICOM",
      "RPM_PACKAGE_VERSION",
  };
  auto scoped_excluded_env = std::vector<scoped_unset_env_t>(unset.begin(), unset.end());
  return sys::run(args, quiet);
}

// Get all shared libraries available to rustc. This has platorm specific details, but is also
// rustspecific at the same time, so we keep it here.
string_list_t get_compiler_shared_libraries(const std::string& sysroot) {
#if defined(_WIN32)
  const std::string lib_dir = file::append_path(sysroot, "bin");
  const std::string dll_ext = ".dll";
#else
  const std::string lib_dir = file::append_path(sysroot, "lib");
  const std::string dll_ext = ".so";
#endif

  string_list_t compiler_shared_libraries;
  for (const auto& file_info :
       file::walk_directory(lib_dir, file::filter_t::include_extension(dll_ext))) {
    if (file_info.is_dir()) {
      continue;
    }

    compiler_shared_libraries += file_info.path();
  }

  // Sort shared libraries to keep it consistent.
  std::sort(compiler_shared_libraries.begin(), compiler_shared_libraries.end());

  return compiler_shared_libraries;
}

}  // namespace

// A wrapper for rustc as invoked by cargo. This implementation is inspired heavily by the rules
// that sccache follows, which means that the same caveats applies here.
// See: https://github.com/mozilla/sccache/tree/main?tab=readme-ov-file#known-caveats
// and https://github.com/mozilla/sccache/blob/main/docs/Rust.md
rust_wrapper_t::rust_wrapper_t(const file::exe_path_t& exe_path, const string_list_t& args)
    : program_wrapper_t(exe_path, args) {
}

// Check if we can handle caching the current command.
bool rust_wrapper_t::can_handle_command() {
  // Is this the right compiler?
  const auto cmd = lower_case(file::get_file_part(m_exe_path.real_path(), false));
  // TODO(farre): We should really handle rustup proxying here.
  return (cmd == "rustc");
}

void rust_wrapper_t::resolve_args() {
  m_args = parse_options(m_unresolved_args);
}

string_list_t rust_wrapper_t::get_capabilities() {
  // force_direct_mode - We require direct mode, because of how rustc is invoked.
  // hard_links  - We can use hard links since rustc will never overwrite already existing files.
  // The cached files are usually quite large though, so we will most often compress contents in the
  // cache, hence hard_links will be off because of that. But we do support it though.
  return string_list_t{"force_direct_mode", "hard_links"};
}

// There are three artifacts built by rustc: the .rlib, .rmeta and .d files. Which files that do get
// built is controlled by the "--emit" option passed to rustc.
std::map<std::string, expected_file_t> rust_wrapper_t::get_build_files() {
  std::map<std::string, expected_file_t> build_files;

  string_list_t files;

  // We get the path to the library file by calling `rustc ... --print file-names. We allways expect
  // to build this file. The fact that we can't know (without platform dependent checks) the
  // extension of the library is the reason for calling `rustc` at all.
  // TODO(farre): This has potential for optimization. If we can figure out the filename of the
  // library, then the metadata filename follows, and things would be faster!
  const auto& args = m_args + string_list_t{"--print", "file-names"};
  const auto& result = run_rustc(args, true);
  if (result.return_code != 0) {
    panic("Failed to call " + args.join(""));
  }

  files += string_list_t(result.std_out, "\n");

  // Check if we've built metadata.
  if (std::find(m_emit.begin(), m_emit.end(), "metadata") != m_emit.end()) {
    string_list_t metadata;
    for (const auto& file : files) {
      // Unfortunately we can't query which metadata files that we emit, but they'll have the same
      // name as the .rlib files, so we get them that way.
      if (file::get_extension(file) == ".rlib") {
        const auto& rmeta = file::change_extension(file, ".rmeta");
        // We try to be defensive about it though, so if rustc suddenly start to emit this, we avoid
        // it.
        if (std::find(metadata.begin(), metadata.end(), rmeta) == metadata.end()) {
          metadata += file::change_extension(file, ".rmeta");
        }
      }
    }

    files += metadata;
  }

  // Add the dep-info file.
  if (std::find(m_emit.begin(), m_emit.end(), "dep-info") != m_emit.end()) {
    files += m_dep_info;
  }

  // Add all expected files.
  for (const auto& file : files) {
    build_files[file] = {file::join(m_output_dir, file), true};
  }

  return build_files;
}

std::string rust_wrapper_t::get_program_id() {
  // We're going to stick quite a lot of information into the program_id, so we'll hash it
  // ourselves, and return the result as a hexstring of the hash.
  hasher_t hasher;

  // Prepend the hash format version.
  hasher.update(HASH_VERSION);

  // Get the version string for the compiler.
  auto result = run_rustc({m_args[0], "-vV"}, true);
  if (result.return_code != 0) {
    panic("Unable to get the compiler version information string.");
  }

  hasher.update(result.std_out);

  // Get the sysroot of the crate.
  result = run_rustc({m_args[0], "--print=sysroot"}, true);
  if (result.return_code != 0) {
    panic("Unable to get the compiler sysroot.");
  }

  std::string sysroot = strip(result.std_out);
  std::string cwd = file::get_cwd();

  // Add cwd to the hash.
  hasher.update(cwd);

  // Hash all the compiler shared libraries.
  for (const auto& shared_library : get_compiler_shared_libraries(sysroot)) {
    hasher.update_from_file_deterministic(shared_library);
  }

  // Hash all static files by name and contents, ignoring ar specific stuff.
  for (const auto& static_lib : m_static_libraries) {
    hasher.update(static_lib);
    hasher.update_from_file_deterministic(static_lib);
  }

  return hasher.final().as_string();
}

// This data has been pre-computed by `rust_wrapper_t::parse_arguments` called by
// `rust_wrapper_t::resolve_args.
string_list_t rust_wrapper_t::get_relevant_arguments() {
  return m_relevant_args;
}

// Getting the relevant environment variables requires parsing .d files, which also will contribute
// to the implicit input files. So we compute them together in
// `rust_wrapper_t::get_implicit_input_files_and_relevant_env_vars`, which also makes sure that the
// result isn't computed twice.
std::map<std::string, std::string> rust_wrapper_t::get_relevant_env_vars() {
  process_implicit_input_files_and_relevant_env_vars();

  return m_relevant_env_vars;
}

// This data has been pre-computed by `rust_wrapper_t::parse_arguments` called by
// `rust_wrapper_t::resolve_args.
string_list_t rust_wrapper_t::get_input_files() {
  // Hash all extern libs named on the command line along with the single input source file.
  return string_list_t{m_input} + m_externs;
}

// Getting the implicit input files requires parsing .d files, which also will contribute
// to the relevant environment variables. So we compute them together in
// `rust_wrapper_t::get_implicit_input_files_and_relevant_env_vars`, which also makes sure that the
// result isn't computed twice.
string_list_t rust_wrapper_t::get_implicit_input_files() {
  process_implicit_input_files_and_relevant_env_vars();

  return m_implicit_input_files;
}

// Parse and verify all options passed to the invocation of rustc, and when possible collect data
// for other calls to the `rust_wrapper_t` implementation of the `program_wrapper_t` interface.
string_list_t rust_wrapper_t::parse_options(const string_list_t& unresolved_arguments) {
  string_list_t parsed_args;

  // The data we're going to collect. We don't collect directly into the object, so that things go
  // out of scope sooner.
  string_list_t relevant_args;
  string_list_t static_library_paths;
  string_list_t static_library_names;
  string_list_t static_libraries;
  bool crate_type_rlib = false;
  bool crate_type_static_lib = false;
  std::string crate_name;
  string_list_t emit;
  std::string extra_filename;
  std::string output_dir;
  string_list_t externs;
  std::string input;
  std::string dep_info;

  // We try to be comprehensive in the errors that we get from an invocation, so instead of bailing
  // on the first bad option, we continue and collect all encountered errors here.
  string_list_t errors;

  parsed_args += unresolved_arguments[0];

  const std::string cwd = file::get_cwd();

  for (unsigned i = 1; i < unresolved_arguments.size(); ++i) {
    const auto& option = parse_argument(unresolved_arguments[i]);
    if (!option.m_success) {
      errors += option.m_option;
      continue;
    }
    bool check_for_argument = needs_argument(option);
    const auto& arg_1 = option.m_option;
    const auto& arg_2 = check_for_argument && i + 1 < unresolved_arguments.size()
                            ? unresolved_arguments[++i]
                            : option.m_argument;
    if (check_for_argument && arg_2.empty()) {
      errors += ("Can't parse arguments, missing argument for " + option.m_option);
      continue;
    }

    parsed_args += arg_1;
    if (!arg_2.empty()) {
      parsed_args += arg_2;
    }

    switch (option.m_type) {
      case option_type_t::UNSUPPORTED:
        errors += ("Unsupported compiler argument " + option.m_option);
        continue;
      case option_type_t::UNHANDLED:
        errors += ("Unhandled compiler argument " + option.m_option);
        continue;
      case option_type_t::IGNORED:
        continue;
      case option_type_t::LIBRARY_PATH: {
        string_list_t argument(arg_2, "=");
        const auto& kind = argument.size() == 2 ? argument[1] : "";
        if (kind.empty() || kind == "native" || kind == "all") {
          static_library_paths += argument[argument.size() - 1];
        }
        // The paths to where we find the static libraries isn't a relevant argument, since the
        // contents of the static libraries are used to create the program id.
        continue;
      }
      case option_type_t::LIBRARY: {
        string_list_t argument(arg_2, "=");
        const auto& kind = argument.size() == 2 ? argument[1] : "";
        if (kind == "static") {
          static_library_names += argument[argument.size() - 1];
        }
        break;
      }
      case option_type_t::CRATE_TYPE: {
        if (crate_type_rlib && crate_type_static_lib) {
          break;
        }

        string_list_t crate_types(arg_2, ",");

        // We assume that lib implies rlib. We also only support lib, rlib or staticlib.
        crate_type_rlib =
            crate_type_rlib ||
            std::find(crate_types.begin(), crate_types.end(), "lib") != crate_types.end() ||
            std::find(crate_types.begin(), crate_types.end(), "rlib") != crate_types.end();
        crate_type_static_lib =
            crate_type_static_lib ||
            std::find(crate_types.begin(), crate_types.end(), "staticlib") != crate_types.end();
        break;
      }
      case option_type_t::CRATE_NAME:
        crate_name = arg_2;
        break;
      case option_type_t::EMIT:
        if (emit.size() != 0U) {
          errors += ("Cannot handle more than one --emit");
          continue;
        }

        emit += string_list_t(arg_2, ",");
        std::sort(emit.begin(), emit.end());

        break;
      case option_type_t::CODE_GEN: {
        string_list_t argument(arg_2, "=");
        const auto& option = argument.size() == 2 ? argument[1] : "";
        if (argument[0] == "extra-filename") {
          extra_filename = argument[argument.size() - 1];
          if (extra_filename.empty()) {
            errors += "Can't cache extra-filename";
            continue;
          }
        }
        if (option == "incremental") {
          errors += "Can't cache incremental builds";
          continue;
        }
        break;
      }
      case option_type_t::OUT_DIR:
        output_dir = arg_2;
        // Where we actually store the result isn't relevant to the hash.
        continue;
      case option_type_t::TARGET:
        if (file::get_extension(arg_2) == "json" || file::file_exists(arg_2 + ".json")) {
          errors += ("Can't cache target " + arg_2);
          continue;
        }

        break;
      case option_type_t::EXTERN: {
        string_list_t argument(arg_2, "=");
        const auto& extern_lib = argument.size() == 2 ? argument[1] : "";
        if (!extern_lib.empty()) {
          // If the extern isn't specified with a absolute path, assume that it's relative to the
          // working directory.
          externs += file::join(cwd, extern_lib);
        }
        // The extern and where it's located aren't relevant arguments, since the
        // contents of the externs libraries are used to create the program id.
        continue;
      }
      case option_type_t::RESPONSE_FILE:
        // TODO(farre): Handle response files.
        // https://github.com/mozilla/sccache/blob/754242bdb33266ccb0cd91c861f117564644ebb4/docs/ResponseFiles.md
        // and `gcc_wrapper_t::parse_response_file`.
        errors += ("Cannot handle response file " + option.m_option);
        continue;
      case option_type_t::PATH:
        if (!input.empty()) {
          errors += ("Cannot handle multiple inputs " + arg_1);
          continue;
        }

        input = arg_1;
        break;
    }

    relevant_args += arg_1;
    if (!arg_2.empty()) {
      relevant_args += arg_2;
    }
  }

  // We've now parsed all options so now we start verification.
  if (errors.size() > 0) {
    panic(errors.join("\n"));
  }

  // We need to have exactly one input file.
  if (input.empty()) {
    panic("input file required to cache cargo/rustc compilation");
  }

  // We only allow --emit with arguments of link, metadata and dep-info, and require "link" and
  // "metadata".
  static const auto& required_emit = string_list_t{"link", "metadata"};
  static const auto& allowed_emit = string_list_t{"dep-info", "link", "metadata"};
  if ((emit.size() == 0U) ||
      !std::includes(emit.begin(), emit.end(), required_emit.begin(), required_emit.end()) ||
      !std::includes(allowed_emit.begin(), allowed_emit.end(), emit.begin(), emit.end())) {
    panic("--emit required to cache cargo/rustc compilation");
  }

  // We need to know the output directory to perform caching.
  if (output_dir.empty()) {
    panic("--output-dir required to cache cargo/rustc compilation");
  }

  // We need to know the crate name to figure out where the depinfo goes, but we also save it to be
  // used ing `rust_wrapper_t::panic`.
  if (crate_name.empty()) {
    panic("--crate-name required to cache cargo/rustc compilation");
  }

  // We can't cache if none of the supported crate types have been seen.
  if (!crate_type_rlib && !crate_type_static_lib) {
    panic("--crate-type required to cache cargo/rustc compilation");
  }

  // Prepare a list of all known static libraries.
  for (const auto& name : static_library_names) {
    for (const auto& path : static_library_paths) {
      for (const auto& lib_path_format : {string_list_t{"lib", path, ".a"},
                                          string_list_t{path, ".lib"},
                                          string_list_t{path, ".a"}}) {
        const auto& lib_path = lib_path_format.join("");
        if (file::file_exists(lib_path)) {
          static_libraries += lib_path;
        }
      }
    }
  }

  // If dep_info is to be emitted, figure out the name of the output file.
  if (std::find(emit.begin(), emit.end(), "dep-info") != emit.end()) {
    dep_info = string_list_t{crate_name, extra_filename, ".d"}.join("");
  }

  // Cargo doesn't guarantee the order of externs, so we'll sort them now.
  std::sort(externs.begin(), externs.end());

  // Sort static_libraries as well for good measure.
  std::sort(static_libraries.begin(), static_libraries.end());

  // Move over all collected data to the object.
  m_output_dir = std::move(output_dir);
  m_externs = std::move(externs);
  m_static_libraries = std::move(static_libraries);
  m_crate_name = std::move(crate_name);
  m_dep_info = std::move(dep_info);
  m_emit = std::move(emit);
  m_input = std::move(input);
  m_relevant_args = std::move(relevant_args);

  return parsed_args;
}

// The entirety of implicit input files and partially the relevant environment variables are
// collected from ".d files", which is why we do it together here.
void rust_wrapper_t::process_implicit_input_files_and_relevant_env_vars() {
  // Make sure to only call this once. If we didn't actually find anything when we did this the
  // first time around we'll try again, and we'll find nothing. Here it would've been nice with
  // `std::optional`.
  if (!m_relevant_env_vars.empty() || m_implicit_input_files.size() > 0) {
    return;
  }

  string_list_t implicit_input_files;
  std::map<std::string, std::string> relevant_env_vars;

  // We need a temporary directoy where we can emit all needed dependency information.
  file::tmp_file_t tmp_file(sys::get_local_temp_folder(), ".d");

  // When calling rustc with "--emit=dep-info" we need to remove existing "--emit" along with
  // "--out-dir" and all "-C" options.
  string_list_t filtered_args;
  bool to_remove = false;
  std::for_each(m_args.begin(), m_args.end(), [&to_remove, &filtered_args](const std::string& arg) {
    if (to_remove || arg == "--emit" || arg == "--out-dir" || arg == "-C") {
      to_remove = !to_remove;
      return;
    }

    filtered_args += arg;
  });

  // Create the arguemnts list.
  filtered_args += {"-o", tmp_file.path(), "--emit=dep-info"};

  // And then call rustc.
  auto result = run_rustc(filtered_args, false);

  if (result.return_code != 0) {
    panic("Failed to call " + m_args.join(""));
  }

  string_list_t lines = string_list_t(file::read(tmp_file.path()), "\n");
  if (lines.size() == 0) {
    return;
  }

  // The first line lists all source dependencies.
  implicit_input_files = string_list_t(lines[0], " ");
  auto last = std::move(std::next(implicit_input_files.begin()),
                        implicit_input_files.end(),
                        implicit_input_files.begin());
  while (last != implicit_input_files.end()) {
    implicit_input_files.pop_back();
  }

  // Next look for environment variables. They start with "#env-dep:".
  for (auto line_iterator = std::next(lines.begin()); line_iterator != lines.end();
       ++line_iterator) {
    string_list_t parts(*line_iterator, ":");
    if (parts.size() == 0) {
      continue;
    }
    for (auto parts_iterator = std::next(parts.begin()); parts_iterator != parts.end();
         ++parts_iterator) {
      if (*parts_iterator == "# env-dep") {
        string_list_t env(*(parts_iterator + 1), "=");
        // Filter out RUSTC_COLOR, it's controlled from the command line.
        if (env[0] == "RUSTC_COLOR" || env[0] == "CARGO_MAKEFLAGS") {
          continue;
        }
        relevant_env_vars[env[0]] = env.size() > 1 ? env[1] : "";
        continue;
      }
    }
  }

  // Include all environment variables that begin with CARGO_.
  for (const auto& env_var : get_env()) {
    if (env_var.find("CARGO_") == std::string::npos) {
      continue;
    }

    string_list_t env(env_var, "=");
    // CARGO_MAKEFLAGS isn't cacheable.
    if (env[0] == "CARGO_MAKEFLAGS") {
      continue;
    }
    relevant_env_vars[env[0]] = env.size() > 1 ? env[1] : "";
  }

  // We don't trust Cargo keeping the source files sorted.
  std::sort(implicit_input_files.begin(), implicit_input_files.end());

  m_implicit_input_files = std::move(implicit_input_files);
  m_relevant_env_vars = std::move(relevant_env_vars);
}

// Utility function that throws a `runtime_error` with the name of the crate prepended to the
// message.
void rust_wrapper_t::panic(const std::string& message) {
  const auto& header = m_crate_name.empty() ? "<unknown crate>" : m_crate_name;
  throw std::runtime_error(string_list_t{header, message}.join(": "));
}

}  // namespace bcache
