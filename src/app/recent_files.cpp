// Aseprite
// Copyright (C) 2018-2020  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/recent_files.h"

#include "app/ini_file.h"
#include "base/convert_to.h"
#include "base/file_handle.h"
#include "base/log.h"
#include "base/sha1.h"
#include "base/string.h"
#include "cfg/cfg.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>
#if LAF_WINDOWS
  #include <windows.h>
#else
  #include <sys/file.h>
#endif
#include "base/fs.h"
#include "fmt/format.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

namespace {

const char* kSectionName[] = { "PinnedFiles",
                               "RecentFiles",
                               "PinnedPaths",
                               "RecentPaths",
                               "PinnedFonts" };

// Special key used in recent sections (files/paths) to indicate that
// the section was already converted at least one time.
const char* kConversionKey = "_";
const char* kOpenedSection = "RecentFileOpened";

std::string opened_key(const std::string& path)
{
  return base::convert_to<std::string>(base::Sha1::calculateFromString(path));
}

struct compare_path {
  std::string a;
  compare_path(const std::string& a) : a(a) {}
  bool operator()(const std::string& b) const { return base::compare_filenames(a, b) == 0; }
};

// A separate authoritative store prevents an older editor instance from
// replacing new history when it saves its unrelated preferences on exit.
class RecentStoreLock {
public:
  RecentStoreLock(const std::string& path, bool wait)
  {
#if LAF_WINDOWS
    const auto name = base::from_utf8(
      "Local\\Aseprite.RecentFiles." +
      base::convert_to<std::string>(base::Sha1::calculateFromString(base::string_to_lower(path))));
    m_handle = CreateMutexW(nullptr, FALSE, name.c_str());
    if (!m_handle)
      throw std::runtime_error("Cannot lock Recent Files settings");
    const DWORD result = WaitForSingleObject(m_handle, wait ? 5000 : 0);
    m_locked = (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED);
#else
    m_file = base::open_file(path + ".lock", "ab");
    m_locked = m_file && flock(fileno(m_file.get()), LOCK_EX | (wait ? 0 : LOCK_NB)) == 0;
#endif
    if (wait && !m_locked) {
#if LAF_WINDOWS
      if (m_handle)
        CloseHandle(m_handle);
      m_handle = nullptr;
#endif
      throw std::runtime_error("Recent Files settings are busy; try again");
    }
  }
  ~RecentStoreLock()
  {
#if LAF_WINDOWS
    if (m_locked)
      ReleaseMutex(m_handle);
    if (m_handle)
      CloseHandle(m_handle);
#else
    if (m_locked)
      flock(fileno(m_file.get()), LOCK_UN);
#endif
  }
  explicit operator bool() const { return m_locked; }
  RecentStoreLock(const RecentStoreLock&) = delete;
  RecentStoreLock& operator=(const RecentStoreLock&) = delete;

private:
  bool m_locked = false;
#if LAF_WINDOWS
  HANDLE m_handle = nullptr;
#else
  base::FileHandle m_file;
#endif
};

bool contains_path(const base::paths& paths, const std::string& path)
{
  return std::find_if(paths.begin(), paths.end(), compare_path(path)) != paths.end();
}

// Legacy folder dragging submits whole lists. Preserve entries opened in another
// instance after the drag began, while honoring edits to entries the user saw.
void merge_list_edit(base::paths& pinned,
                     base::paths& recent,
                     const base::paths& beforePinned,
                     const base::paths& beforeRecent,
                     const base::paths& latestPinned,
                     const base::paths& latestRecent)
{
  const auto state = [](const base::paths& pins, const base::paths& list, const std::string& path) {
    return contains_path(pins, path) ? 1 : (contains_path(list, path) ? 2 : 0);
  };
  base::paths mergedPins, mergedRecent;
  const auto append = [&](const base::paths& requested, int requestedState) {
    for (const auto& path : requested) {
      const int before = state(beforePinned, beforeRecent, path);
      const int latest = state(latestPinned, latestRecent, path);
      const int target = before == requestedState && latest != before ? latest : requestedState;
      if (target == 0)
        continue; // Forgotten in another window; not edited locally.
      auto& destination = target == 1 ? mergedPins : mergedRecent;
      if (!contains_path(mergedPins, path) && !contains_path(mergedRecent, path))
        destination.push_back(path);
    }
  };
  append(pinned, 1);
  append(recent, 2);
  pinned = std::move(mergedPins);
  recent = std::move(mergedRecent);
  for (const auto& path : latestPinned)
    if (!contains_path(beforePinned, path) && !contains_path(beforeRecent, path) &&
        !contains_path(pinned, path) && !contains_path(recent, path))
      pinned.push_back(path);
  base::paths newlyOpened;
  for (const auto& path : latestRecent)
    if (!contains_path(beforePinned, path) && !contains_path(beforeRecent, path) &&
        !contains_path(pinned, path) && !contains_path(recent, path))
      newlyOpened.push_back(path);
  recent.insert(recent.begin(), newlyOpened.begin(), newlyOpened.end());
}

} // namespace

namespace app {

RecentFiles::RecentFiles(const int limit)
  : m_storageFilename(normalizePath(current_config_filename()) + ".recents")
  , m_limit(limit)
{
  load();
  try {
    RecentStoreLock lock(m_storageFilename, true);
    readStore();
  }
  catch (const std::exception& error) {
    LOG(ERROR, "RECENT: %s\n", error.what());
  }
  for (const int i : { kRecentFiles, kRecentFolders })
    if (m_paths[i].size() > m_limit)
      m_paths[i].resize(m_limit);
  pruneOpenTimes();
}

RecentFiles::~RecentFiles()
{
  save();
}

void RecentFiles::addRecentFile(const std::string& filename)
{
  addRecentFile(filename, false);
}

void RecentFiles::recordOpenedFile(const std::string& filename)
{
  addRecentFile(filename, true);
}

std::time_t RecentFiles::lastOpened(const std::string& filename) const
{
  const auto it = m_openTimes.find(filename);
  if (it != m_openTimes.end())
    return it->second;
  for (const auto& [path, opened] : m_openTimes)
    if (base::compare_filenames(path, filename) == 0)
      return opened;
  return 0;
}

void RecentFiles::pruneOpenTimes()
{
  for (auto it = m_openTimes.begin(); it != m_openTimes.end();) {
    if (!contains_path(pinnedFiles(), it->first) && !contains_path(recentFiles(), it->first))
      it = m_openTimes.erase(it);
    else
      ++it;
  }
}

void RecentFiles::addRecentFile(const std::string& filename, const bool opened)
{
  if (filename.empty())
    return;
  mutate([&] {
    std::string fn = normalizePath(filename);

    // If the filename is already pinned, we don't add it in the
    // collection of recent files collection.
    auto it =
      std::find_if(m_paths[kPinnedFiles].begin(), m_paths[kPinnedFiles].end(), compare_path(fn));
    if (it == m_paths[kPinnedFiles].end())
      addItem(m_paths[kRecentFiles], fn);
    const auto timestamp = opened ? std::time(nullptr) : lastOpened(fn);
    if (timestamp)
      m_openTimes[it == m_paths[kPinnedFiles].end() ? fn : *it] = timestamp;

    // Add recent folder
    std::string path = base::get_file_path(fn);
    it = std::find_if(m_paths[kPinnedFolders].begin(),
                      m_paths[kPinnedFolders].end(),
                      compare_path(path));
    if (it == m_paths[kPinnedFolders].end()) {
      addItem(m_paths[kRecentFolders], path);
    }
  });
}

bool RecentFiles::isFilePinned(const std::string& filename) const
{
  const auto fn = normalizePath(filename);
  return std::find_if(m_paths[kPinnedFiles].begin(),
                      m_paths[kPinnedFiles].end(),
                      compare_path(fn)) != m_paths[kPinnedFiles].end();
}

void RecentFiles::setFilePinned(const std::string& filename, const bool pinned)
{
  if (filename.empty())
    return;
  mutate([&] {
    const auto fn = normalizePath(filename);
    removeItem(m_paths[kPinnedFiles], fn);
    removeItem(m_paths[kRecentFiles], fn);
    if (pinned)
      m_paths[kPinnedFiles].insert(m_paths[kPinnedFiles].begin(), fn);
    else
      addItem(m_paths[kRecentFiles], fn);
  });
}

void RecentFiles::forgetFile(const std::string& filename)
{
  if (filename.empty())
    return;
  mutate([&] {
    const auto path = normalizePath(filename);
    removeItem(m_paths[kPinnedFiles], path);
    removeItem(m_paths[kRecentFiles], path);
  });
}

void RecentFiles::removeRecentFile(const std::string& filename)
{
  mutate([&] {
    std::string fn = normalizePath(filename);
    removeItem(m_paths[kRecentFiles], fn);

    std::string dir = base::get_file_path(fn);
    if (!base::is_directory(dir))
      removeItem(m_paths[kRecentFolders], dir);
  });
}

void RecentFiles::removeRecentFolder(const std::string& dir)
{
  mutate([&] {
    std::string fn = normalizePath(dir);
    removeItem(m_paths[kRecentFolders], fn);
  });
}

void RecentFiles::setLimit(const int newLimit)
{
  mutate([&] {
    ASSERT(newLimit >= 0);

    for (const int collection : { kRecentFiles, kRecentFolders }) {
      auto& list = m_paths[collection];
      if (newLimit < list.size()) {
        auto it = list.begin();
        std::advance(it, newLimit);
        list.erase(it, list.end());
      }
    }

    m_limit = newLimit;
  });
}

void RecentFiles::clear()
{
  mutate([&] {
    // Clear only recent items (not pinned items)
    m_paths[kRecentFiles].clear();
    m_paths[kRecentFolders].clear();
  });
}

void RecentFiles::setFiles(const base::paths& pinnedFiles, const base::paths& recentFiles)
{
  const auto beforePinned = m_paths[kPinnedFiles];
  const auto beforeRecent = m_paths[kRecentFiles];
  mutate(
    [&] {
      auto pinned = pinnedFiles, recent = recentFiles;
      merge_list_edit(pinned,
                      recent,
                      beforePinned,
                      beforeRecent,
                      m_paths[kPinnedFiles],
                      m_paths[kRecentFiles]);
      m_paths[kPinnedFiles] = std::move(pinned);
      m_paths[kRecentFiles] = std::move(recent);
    },
    false);
}

void RecentFiles::setFolders(const base::paths& pinnedFolders, const base::paths& recentFolders)
{
  const auto beforePinned = m_paths[kPinnedFolders];
  const auto beforeRecent = m_paths[kRecentFolders];
  mutate(
    [&] {
      auto pinned = pinnedFolders, recent = recentFolders;
      merge_list_edit(pinned,
                      recent,
                      beforePinned,
                      beforeRecent,
                      m_paths[kPinnedFolders],
                      m_paths[kRecentFolders]);
      m_paths[kPinnedFolders] = std::move(pinned);
      m_paths[kRecentFolders] = std::move(recent);
    },
    false);
}

std::string RecentFiles::normalizePath(const std::string& filename)
{
  return std::filesystem::absolute(std::filesystem::u8path(filename)).lexically_normal().u8string();
}

void RecentFiles::addItem(base::paths& list, const std::string& fn)
{
  auto it = std::find_if(list.begin(), list.end(), compare_path(fn));

  // If the item already exist in the list...
  if (it != list.end()) {
    // Move it to the first position
    list.erase(it);
    list.insert(list.begin(), fn);
    return;
  }

  if (m_limit > 0)
    list.insert(list.begin(), fn);

  while (list.size() > m_limit)
    list.erase(--list.end());
}

void RecentFiles::removeItem(base::paths& list, const std::string& fn)
{
  auto it = std::find_if(list.begin(), list.end(), compare_path(fn));
  if (it != list.end())
    list.erase(it);
}

void RecentFiles::load()
{
  for (int i = 0; i < kCollections; ++i) {
    const char* section = kSectionName[i];

    // For recent files: If there is an item called "Filename00" and no "0" key
    // For recent paths: If there is an item called "Path00" and no "0" key
    // -> We are migrating from and old version to a new one
    const bool processOldFilenames = (i == kRecentFiles &&
                                      get_config_string(section, "Filename00", nullptr) &&
                                      !get_config_bool(section, kConversionKey, false));

    const bool processOldPaths = (i == kRecentFolders &&
                                  get_config_string(section, "Path00", nullptr) &&
                                  !get_config_bool(section, kConversionKey, false));

    for (const auto& key : enum_config_keys(section)) {
      if (key == kConversionKey ||
          (!processOldFilenames && std::strncmp(key.c_str(), "Filename", 8) == 0) ||
          (!processOldPaths && std::strncmp(key.c_str(), "Path", 4) == 0)) {
        // Ignore old entries if we are going to read the new ones
        continue;
      }

      const char* fn = get_config_string(section, key.c_str(), nullptr);
      // An external drive or network share can be temporarily unavailable.
      // Keep its entry until the user explicitly removes it.
      if (fn && *fn) {
        const std::string value = (i == kPinnedFonts ? fn : normalizePath(fn));
        if (std::find_if(m_paths[i].begin(), m_paths[i].end(), compare_path(value)) ==
            m_paths[i].end())
          m_paths[i].push_back(value);
      }
    }
  }
}

void RecentFiles::save()
{
  for (int i = 0; i < kCollections; ++i) {
    const char* section = kSectionName[i];

    for (const auto& key : enum_config_keys(section)) {
      if ((i == kRecentFiles &&
           (std::strncmp(key.c_str(), "Filename", 8) == 0 || key == kConversionKey)) ||
          (i == kRecentFolders &&
           (std::strncmp(key.c_str(), "Path", 4) == 0 || key == kConversionKey))) {
        // Ignore old entries if we are going to read the new ones
        continue;
      }
      del_config_value(section, key.c_str());
    }

    for (int j = 0; j < m_paths[i].size(); ++j) {
      set_config_string(section, fmt::format("{:04d}", j).c_str(), m_paths[i][j].c_str());
    }
    // Special entry that indicates that we've already converted
    if ((i == kRecentFiles || i == kRecentFolders) &&
        !get_config_bool(section, kConversionKey, false)) {
      set_config_bool(section, kConversionKey, true);
    }
  }
}

bool RecentFiles::readStore()
{
  if (!base::is_file(m_storageFilename))
    return false;
  cfg::CfgFile store;
  if (!store.load(m_storageFilename) || store.getIntValue("RecentFilesState", "version", 0) != 1)
    throw std::runtime_error("Cannot read Recent Files settings: " + m_storageFilename);
  std::array<base::paths, kPinnedFonts> paths;
  decltype(m_openTimes) openTimes;
  for (int i = 0; i < kPinnedFonts; ++i) {
    std::vector<std::string> keys;
    store.getAllKeys(kSectionName[i], keys);
    std::sort(keys.begin(), keys.end());
    for (const auto& key : keys) {
      const auto* value = store.getValue(kSectionName[i], key.c_str(), "");
      if (!*value)
        continue;
      const auto path = normalizePath(value);
      if (!contains_path(paths[i], path)) {
        paths[i].push_back(path);
        if (i == kPinnedFiles || i == kRecentFiles) {
          const auto pathKey = opened_key(path);
          const char* value = store.getValue(kOpenedSection, pathKey.c_str(), "");
          char* end = nullptr;
          const auto timestamp = std::strtoll(value, &end, 10);
          if (end != value && !*end && timestamp > 0)
            openTimes[path] = static_cast<std::time_t>(timestamp);
        }
      }
    }
  }
  for (const auto pair : {
         std::pair<int, int>{ kPinnedFiles,   kRecentFiles   },
         { kPinnedFolders, kRecentFolders }
  })
    for (const auto& path : paths[pair.first])
      removeItem(paths[pair.second], path);
  bool changed = m_openTimes != openTimes;
  m_openTimes = std::move(openTimes);
  for (int i = 0; i < kPinnedFonts; ++i) {
    changed = changed || m_paths[i] != paths[i];
    m_paths[i] = std::move(paths[i]);
  }
  return changed;
}

void RecentFiles::writeStore()
{
  cfg::CfgFile store;
  store.load(m_storageFilename);
  store.setIntValue("RecentFilesState", "version", 1);
  store.deleteSection(kOpenedSection);
  for (int i = 0; i < kPinnedFonts; ++i) {
    store.deleteSection(kSectionName[i]);
    const bool fileList = i == kPinnedFiles || i == kRecentFiles;
    for (int j = 0; j < int(m_paths[i].size()); ++j) {
      const auto key = fmt::format("{:04d}", j);
      const auto& path = m_paths[i][j];
      store.setValue(kSectionName[i], key.c_str(), path.c_str());
      if (fileList)
        if (const auto opened = lastOpened(path))
          store.setValue(kOpenedSection, opened_key(path).c_str(), std::to_string(opened).c_str());
    }
  }
  if (!store.trySave())
    throw std::runtime_error("Cannot save Recent Files settings: " + m_storageFilename);
}

void RecentFiles::mutate(const std::function<void()>& operation, const bool notify)
{
  {
    RecentStoreLock lock(m_storageFilename, true);
    std::array<base::paths, kCollections> previous;
    std::copy(std::begin(m_paths), std::end(m_paths), previous.begin());
    const int previousLimit = m_limit;
    const auto previousOpenTimes = m_openTimes;
    try {
      readStore();
      operation();
      for (int i = 0; i < kPinnedFonts; ++i) {
        base::paths normalized;
        for (const auto& path : m_paths[i]) {
          if (path.empty())
            continue;
          const auto absolute = normalizePath(path);
          if (!contains_path(normalized, absolute))
            normalized.push_back(absolute);
        }
        m_paths[i] = std::move(normalized);
      }
      for (const auto pair : {
             std::pair<int, int>{ kPinnedFiles,   kRecentFiles   },
             { kPinnedFolders, kRecentFolders }
      })
        for (const auto& path : m_paths[pair.first])
          removeItem(m_paths[pair.second], path);
      for (const int i : { kRecentFiles, kRecentFolders })
        if (m_paths[i].size() > m_limit)
          m_paths[i].resize(m_limit);
      pruneOpenTimes();
      writeStore();
    }
    catch (...) {
      std::copy(previous.begin(), previous.end(), std::begin(m_paths));
      m_limit = previousLimit;
      m_openTimes = previousOpenTimes;
      throw;
    }
  }
  // Mirror for older versions; only the companion is authoritative now.
  save();
  flush_config_file();
  if (notify)
    Changed();
}

bool RecentFiles::refresh()
{
  bool changed = false;
  try {
    RecentStoreLock lock(m_storageFilename, false);
    if (lock) {
      std::array<base::paths, kPinnedFonts> previous;
      std::copy(std::begin(m_paths), std::begin(m_paths) + kPinnedFonts, previous.begin());
      const auto previousOpenTimes = m_openTimes;
      readStore();
      for (const int i : { kRecentFiles, kRecentFolders })
        if (m_paths[i].size() > m_limit)
          m_paths[i].resize(m_limit);
      for (int i = 0; i < kPinnedFonts; ++i)
        changed = changed || previous[i] != m_paths[i];
      pruneOpenTimes();
      changed = changed || previousOpenTimes != m_openTimes;
    }
  }
  catch (const std::exception& error) {
    LOG(ERROR, "RECENT: %s\n", error.what());
  }
  if (changed)
    Changed();
  return changed;
}

} // namespace app
