// Aseprite
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_RECENT_FILES_GRID_H_INCLUDED
#define APP_UI_RECENT_FILES_GRID_H_INCLUDED
#pragma once

#include "gfx/rect.h"
#include "gfx/size.h"
#include "obs/connection.h"
#include "ui/timer.h"
#include "ui/tooltips.h"
#include "ui/widget.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace app {

// Shared by sizing, placement, and keyboard navigation.
struct RecentGridLayout {
  int columns;
  int rows;
  int cardWidth;
  int cardHeight;
  int gap;
  int height;

  gfx::Rect itemBounds(int index) const;
};

RecentGridLayout recent_grid_layout(int width, int count, int scale);

gfx::Rect fit_recent_preview(const gfx::Size& image, const gfx::Rect& bounds);

gfx::Rect zoom_recent_preview(const gfx::Rect& image, const gfx::Rect& viewport, double scale,
                              const gfx::Point& anchor);

class IFileItem;
class RecentFileCard;

class RecentFilesGrid : public ui::Widget {
public:
  RecentFilesGrid();
  ~RecentFilesGrid() override;

  void openFile(const std::string& path);
  bool navigate(RecentFileCard* card, ui::Message* message);

protected:
  bool onProcessMessage(ui::Message* msg) override;
  void onResize(ui::ResizeEvent& ev) override;
  void onSizeHint(ui::SizeHintEvent& ev) override;
  void onPaint(ui::PaintEvent& ev) override;

private:
  friend class RecentFileCard;

  void rebuild();
  void pollPreviews();
  void revealCard(ui::Widget* card);

  ui::Timer m_previewTimer;
  ui::TooltipManager m_tooltips;
  obs::scoped_connection m_languageConnection;
  obs::scoped_connection m_recentConnection;
  obs::scoped_connection m_removedConnection;
  std::set<IFileItem*> m_pendingPreviews;
  std::map<std::string, std::filesystem::file_time_type> m_previewVersions;
  bool m_startedPreviews = false;
  int m_refreshTicks = 0;
};

} // namespace app
#endif
