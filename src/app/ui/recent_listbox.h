// Aseprite
// Copyright (C) 2018  Igara Studio S.A.
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_RECENT_LISTBOX_H_INCLUDED
#define APP_UI_RECENT_LISTBOX_H_INCLUDED
#pragma once

#include "base/paths.h"
#include "obs/connection.h"
#include "ui/listbox.h"
#include "ui/view.h"
#include "ui/tooltips.h"

#include <functional>

namespace app {

class RecentFileItem;

class RecentListBox : public ui::ListBox,
                      public ui::ViewableWidget {
  friend class RecentFileItem;

public:
  RecentListBox();

  void updateRecentListFromUIItems();

protected:
  // ui::ViewableWidget impl
  virtual void onScrollRegion(ui::ScrollRegionEvent& ev);

  virtual void onRebuildList() = 0;
  virtual void onClick(const std::string& path) = 0;
  virtual void onUpdateRecentListFromUIItems(const base::paths& pinnedPaths,
                                             const base::paths& recentPaths) = 0;

private:
  void rebuildList();

  obs::scoped_connection m_recentFilesConn;
  obs::scoped_connection m_showFullPathConn;
};

class RecentFoldersListBox : public RecentListBox {
public:
  RecentFoldersListBox();

private:
  void onRebuildList() override;
  void onClick(const std::string& path) override;
  void onUpdateRecentListFromUIItems(const base::paths& pinnedPaths,
                                     const base::paths& recentPaths) override;
};

class RecentFilesListBox : public ui::ListBox {
public:
  explicit RecentFilesListBox(std::function<void(const std::string&)> open);

protected:
  bool onProcessMessage(ui::Message* msg) override;

private:
  void rebuild();
  void openSelected();

  std::function<void(const std::string&)> m_open;
  ui::TooltipManager m_tooltips;
  obs::scoped_connection m_recentConnection;
};

} // namespace app

#endif
