// Aseprite
// Copyright (C) 2018-2019  Igara Studio S.A.
// Copyright (C) 2001-2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_RECENT_FILES_H_INCLUDED
#define APP_RECENT_FILES_H_INCLUDED
#pragma once

#include "base/paths.h"
#include "obs/signal.h"
#include <functional>
#include <ctime>
#include <map>

#include <string>

namespace app {

class RecentFiles {
  enum { kPinnedFiles, kRecentFiles, kPinnedFolders, kRecentFolders, kPinnedFonts, kCollections };

public:
  const base::paths& pinnedFiles() const { return m_paths[kPinnedFiles]; }
  const base::paths& recentFiles() const { return m_paths[kRecentFiles]; }
  const base::paths& pinnedFolders() const { return m_paths[kPinnedFolders]; }
  const base::paths& recentFolders() const { return m_paths[kRecentFolders]; }
  // TODO probably this collection should be in another kind of class.
  base::paths& pinnedFonts() { return m_paths[kPinnedFonts]; }

  RecentFiles(const int limit);
  ~RecentFiles();

  void addRecentFile(const std::string& filename);
  void recordOpenedFile(const std::string& filename);
  std::time_t lastOpened(const std::string& filename) const;
  bool isFilePinned(const std::string& filename) const;
  void setFilePinned(const std::string& filename, bool pinned);
  void removeRecentFile(const std::string& filename);
  void forgetFile(const std::string& filename);
  void removeRecentFolder(const std::string& dir);
  void setLimit(const int newLimit);
  void clear();
  bool refresh();

  void setFiles(const base::paths& pinnedFiles, const base::paths& recentFiles);
  void setFolders(const base::paths& pinnedFolders, const base::paths& recentFolders);

  obs::signal<void()> Changed;

private:
  void addRecentFile(const std::string& filename, bool opened);
  void pruneOpenTimes();
  static std::string normalizePath(const std::string& filename);
  void addItem(base::paths& list, const std::string& filename);
  void removeItem(base::paths& list, const std::string& filename);
  void load();
  void save();
  void mutate(const std::function<void()>& operation, bool notify = true);
  bool readStore();
  void writeStore();

  base::paths m_paths[kCollections];
  std::map<std::string, std::time_t> m_openTimes;
  std::string m_storageFilename;
  int m_limit;
};

} // namespace app

#endif
