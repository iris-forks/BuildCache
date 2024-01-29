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

#ifndef BUILDCACHE_LOCAL_CACHE_HPP_
#define BUILDCACHE_LOCAL_CACHE_HPP_

#include <base/file_lock.hpp>
#include <base/file_utils.hpp>
#include <cache/cache_entry.hpp>
#include <cache/cache_stats.hpp>
#include <cache/direct_mode_manifest.hpp>
#include <cache/expected_file.hpp>

#include <map>
#include <string>
#include <utility>

namespace bcache {

class local_cache_t {
public:
  /// @brief Initialize the cache object.
  /// @throws runtime_error if the cache could not be initialized.
  local_cache_t();

  /// @brief De-initialzie the cache object.
  ~local_cache_t() = default;

  /// @brief Clear all entries in the cache.
  void clear();

  /// @brief Perform housekeeping (prune old entries etc).
  void perform_housekeeping();

  /// @brief Show cache statistics (print to standard out).
  void show_stats();

  /// @brief Clear the cache statistics.
  void zero_stats();

  /// @brief Add a new direct mode entry to the cache.
  /// @param direct_hash The hash of the direct mode cache entry.
  /// @param manifest The direct mode manifest.
  void add_direct(const std::string& direct_hash, const direct_mode_manifest_t& manifest);

  /// @brief Check if a direct mode entry exists in the cache.
  /// @param direct_hash The hash of the direct mode cache entry.
  /// @returns A a direct mode manifest. If there was no cache hit the manifest will be empty.
  direct_mode_manifest_t lookup_direct(const std::string& direct_hash);

  /// @brief Add a set of files to the cache.
  /// @param hash The cache entry identifier.
  /// @param entry The cache entry data (files, stdout, etc).
  /// @param expected_files Paths to the actual files in the local file system (map from file ID to
  /// an expected file descriptor).
  /// @param allow_hard_links Whether or not to allow hard links to be used when caching files.
  void add(const std::string& hash,
           const cache_entry_t& entry,
           const std::map<std::string, expected_file_t>& expected_files,
           const bool allow_hard_links);

  /// @brief Check if an entry exists in the cache.
  /// @param hash The cache entry identifier.
  /// @param do_update_stats Whether or not to update cache statistics.
  /// @returns A pair of a cache entry struct and a file lock object. If there was no cache hit,
  /// the entry will be empty, and the file lock object will not hold any lock.
  std::pair<cache_entry_t, file_lock_t> lookup(const std::string& hash,
                                               const bool do_update_stats = true);

  /// @brief Copy a cached file to the local file system.
  /// @param hash The cache entry identifier.
  /// @param source_id The ID of the cached file to copy.
  /// @param target_path The path to the local file.
  /// @param is_compressed True if the cached data is compressed.
  /// @param allow_hard_links True if hard links are allowed.
  void get_file(const std::string& hash,
                const std::string& source_id,
                const std::string& target_path,
                const bool is_compressed,
                const bool allow_hard_links);

  /// @brief Update statistics associated with the given entry.
  /// @param hash The hash of the entry to which the stats belong.
  /// @param delta The incremental stats delta.
  /// @returns true if the stats were successfully updated, otherwise false.
  /// @note The hash argument is only used for selecting where in the local cache structure to store
  /// the stats file.
  bool update_stats(const std::string& hash, const cache_stats_t& delta) const noexcept;

  /// @brief Inspect (display) a cache entry.
  ///
  /// If only a hash is given, the meta data of the cache entry is printed (including stdout and
  /// stderr).
  ///
  /// If a file ID is also given, only the file contents is printed.
  /// @param hash The hash of the cache entry.
  /// @param file If non-empty, specifies a file ID.
  /// @returns true if the entry was found in the cache, otherwise false.
  bool inspect(const std::string& hash, const std::string& file);

private:
  std::string hash_to_cache_entry_path(const std::string& hash) const;
  std::string get_cache_files_folder() const;
};

}  // namespace bcache

#endif  // BUILDCACHE_LOCAL_CACHE_HPP_
