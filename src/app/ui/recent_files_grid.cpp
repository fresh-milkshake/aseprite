// Aseprite
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/recent_files_grid.h"

#include "app/app.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/file_system.h"
#include "app/i18n/strings.h"
#include "app/launcher.h"
#include "app/recent_files.h"
#include "app/thumbnail_generator.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/skin/skin_part.h"
#include "app/ui_context.h"
#include "base/fs.h"
#include "base/time.h"
#include "base/utf8_decode.h"
#include "fmt/chrono.h"
#include "gfx/region.h"
#include "text/draw_text.h"
#include "ui/alert.h"
#include "ui/button.h"
#include "ui/graphics.h"
#include "ui/manager.h"
#include "ui/menu.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/resize_event.h"
#include "ui/size_hint_event.h"
#include "ui/system.h"
#include "ui/view.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

namespace app {

using namespace ui;
using namespace gfx;

RecentGridLayout recent_grid_layout(const int width, const int count, const int scale)
{
  const int gap = 10 * std::max(1, scale);
  const int available = std::max(1, width);
  const int columns = std::max(1, (available + gap) / (200 * std::max(1, scale) + gap));
  const int rows = (std::max(0, count) + columns - 1) / columns;
  const int height = 164 * std::max(1, scale);
  return { columns, rows, std::max(1, (available - (columns - 1) * gap) / columns),
           height,  gap,  rows ? rows * height + (rows - 1) * gap : height };
}

Rect RecentGridLayout::itemBounds(const int index) const
{
  return Rect((index % columns) * (cardWidth + gap),
              (index / columns) * (cardHeight + gap),
              cardWidth,
              cardHeight);
}

gfx::Rect fit_recent_preview(const gfx::Size& image, const gfx::Rect& bounds)
{
  if (image.w <= 0 || image.h <= 0 || bounds.isEmpty())
    return gfx::Rect();
  int width, height;
  if (int64_t(image.w) * bounds.h > int64_t(image.h) * bounds.w) {
    width = bounds.w;
    height = std::max(1, int(int64_t(image.h) * width / image.w));
  }
  else {
    height = bounds.h;
    width = std::max(1, int(int64_t(image.w) * height / image.h));
  }
  // Integer magnification keeps low-resolution artwork crisp and evenly sized.
  if (width >= image.w && height >= image.h) {
    const int scale = std::max(1, std::min(width / image.w, height / image.h));
    width = image.w * scale;
    height = image.h * scale;
  }
  return gfx::Rect(bounds.x + (bounds.w - width) / 2,
                   bounds.y + (bounds.h - height) / 2,
                   width,
                   height);
}

gfx::Rect zoom_recent_preview(const gfx::Rect& image,
                              const gfx::Rect& viewport,
                              const double scale,
                              const gfx::Point& anchor)
{
  const int width = std::max(1, int(std::lround(image.w * scale)));
  const int height = std::max(1, int(std::lround(image.h * scale)));
  const int x = anchor.x - int(std::lround((anchor.x - image.x) * scale));
  const int y = anchor.y - int(std::lround((anchor.y - image.y) * scale));
  return Rect(width <= viewport.w ? viewport.x + (viewport.w - width) / 2 :
                                    std::clamp(x, viewport.x2() - width, viewport.x),
              height <= viewport.h ? viewport.y + (viewport.h - height) / 2 :
                                     std::clamp(y, viewport.y2() - height, viewport.y),
              width,
              height);
}

namespace {
int recent_text_width(const std::string& value, const text::FontRef& font)
{
  return text::draw_text(nullptr, get_theme()->fontMgr(), font,
                         value.c_str(), 0, 0, 0, 0, nullptr).w;
}

std::string fit_recent_text(const std::string& text, const text::FontRef& font, const int width)
{
  if (width <= 0 || recent_text_width(text, font) <= width)
    return width > 0 ? text : std::string();

  const std::string ellipsis = "...";
  if (recent_text_width(ellipsis, font) > width)
    return {};

  std::vector<size_t> offsets{ 0 };
  base::utf8_decode decode(text);
  while (decode.next())
    offsets.push_back(decode.pos() - text.begin());

  std::string result = ellipsis;
  size_t low = 1, high = offsets.size() - 1;
  while (low <= high) {
    const size_t count = low + (high - low) / 2;
    auto candidate = text.substr(0, offsets[count]) + ellipsis;
    if (recent_text_width(candidate, font) <= width) {
      result = std::move(candidate);
      low = count + 1;
    }
    else {
      high = count - 1;
    }
  }
  return result;
}

std::string recent_folder_label(const std::string& path, const text::FontRef& font, int width)
{
  const auto folder = base::get_file_path(path);
  if (recent_text_width(folder, font) <= width)
    return folder;

  const auto leaf = base::get_file_name(folder);
  const auto parent = base::get_file_name(base::get_file_path(folder));
  if (!parent.empty() && !leaf.empty()) {
    const auto pair = parent + base::path_separator + leaf;
    if (recent_text_width(pair, font) <= width)
      return pair;
  }
  return leaf.empty() ? folder : leaf;
}

Region pixel_rounded_region(const Rect& area)
{
  const int step = std::min({ guiscale(), area.w / 7, area.h / 5 });
  if (step <= 0)
    return Region(area);

  Region region(Rect(area.x, area.y + 2 * step, area.w, area.h - 4 * step));
  for (const int y : { area.y, area.y2() - step })
    region |= Region(Rect(area.x + 3 * step, y, area.w - 6 * step, step));
  for (const int y : { area.y + step, area.y2() - 2 * step })
    region |= Region(Rect(area.x + step, y, area.w - 2 * step, step));
  return region;
}

} // namespace

class RecentPinButton : public Button {
public:
  explicit RecentPinButton(bool pinned)
    : Button(pinned ? Strings::home_view_unpin_file() : Strings::home_view_pin_file())
    , m_pinned(pinned)
  {
    setTransparent(true);
  }

protected:
  void onSizeHint(SizeHintEvent& ev) override
  {
    auto* theme = skin::SkinTheme::get(this);
    auto size = theme->calcSizeHint(this, style());
    const auto& icon = m_pinned ? theme->parts.pinned() : theme->parts.unpinned();
    if (icon)
      size.w += icon->size().w + 4 * guiscale();
    ev.setSizeHint(size);
  }

  void onPaint(PaintEvent& ev) override
  {
    auto* g = ev.graphics();
    auto* theme = skin::SkinTheme::get(this);
    PaintWidgetPartInfo chrome(this);
    chrome.bgColor = ColorNone;
    chrome.text = nullptr;
    chrome.textBlob.reset();
    theme->paintWidgetPart(g, style(), clientBounds(), chrome);

    const auto& part = m_pinned ? theme->parts.pinned() : theme->parts.unpinned();
    auto* icon = part ? part->bitmap(0) : nullptr;
    const int gap = icon ? 4 * guiscale() : 0;
    const int iconWidth = icon ? icon->width() : 0;
    const int x = std::max(3 * guiscale(), (bounds().w - iconWidth - gap - textWidth()) / 2);
    const auto color = isSelected() ? theme->colors.buttonSelectedText() :
                       hasMouse() ? theme->colors.buttonHotText() : theme->colors.buttonNormalText();
    if (icon)
      g->drawColoredRgbaSurface(icon, color, x, (bounds().h - icon->height()) / 2);
    const int labelX = x + iconWidth + gap;
    const Rect label(labelX, 0, std::max(0, bounds().w - labelX - 3 * guiscale()), bounds().h);
    IntersectClip clip(g, label);
    if (clip)
      g->drawAlignedUIText(fit_recent_text(text(), font(), label.w), color, ColorNone, label,
                           LEFT | MIDDLE);
  }

private:
  bool m_pinned;
};

class RecentFileCard : public Button {
public:
  RecentFileCard(const std::string& path, const bool pinned)
    : Button(base::get_file_name(path))
    , m_path(path)
    , m_pinned(pinned)
    , m_pin(pinned)
  {
    setId("recent_file_card");
    m_pin.setId("recent_file_pin");
    setMnemonic(0, true);
    addChild(&m_pin);
    const auto timestamp = App::instance()->recentFiles()->lastOpened(path);
    if (timestamp) {
      std::tm local{};
      base::safe_localtime(timestamp, &local);
      if (local.tm_mday)
        m_opened = fmt::format("{:%d.%m.%Y %H:%M}", local);
    }
    Click.connect([this] { static_cast<RecentFilesGrid*>(parent())->openFile(m_path); });
    RightClick.connect([this] { showContextMenu(); });
    m_pin.Click.connect(
      [this] { App::instance()->recentFiles()->setFilePinned(m_path, !m_pinned); });
    m_removedConnection = FileSystemModule::instance()->ItemRemoved.connect(
      [this](IFileItem* item) {
        if (item == m_item) {
          m_item = nullptr;
          m_resolved = false;
          m_loading = false;
          m_preview.reset();
          resetZoom();
          invalidate();
        }
      });
  }

  const std::string& path() const { return m_path; }
  Button* pinButton() { return &m_pin; }
  IFileItem* fileItem() const { return m_item; }
  void revalidate() { m_resolved = false; }

  bool pollPreview(bool allowStart,
                   std::map<std::string, std::filesystem::file_time_type>& versions)
  {
    bool started = false;
    if (!m_resolved && allowStart) {
      m_resolved = true;
      m_item = FileSystemModule::instance()->getFileItemFromPath(m_path);
      if (m_item && m_item->isExistent() && !m_item->isFolder()) {
        std::error_code error;
        const auto modified = std::filesystem::last_write_time(std::filesystem::u8path(m_path),
                                                               error);
        if (error) {
          m_item = nullptr;
          invalidate();
          return false;
        }
        const auto version = versions.find(m_path);
        if ((version == versions.end() || version->second != modified) &&
            m_item->getThumbnailProgress() >= 1.0) {
          m_item->setThumbnail(nullptr);
          m_item->setThumbnailProgress(0.0);
        }
        versions[m_path] = modified;
        if (m_item->needThumbnail()) {
          ThumbnailGenerator::instance()->generateThumbnail(m_item);
          m_loading = m_item->getThumbnailProgress() < 1.0;
          started = m_loading;
        }
      }
      else {
        m_item = nullptr;
      }
      invalidate();
    }
    if (m_item) {
      auto preview = m_item->getThumbnail();
      if (preview != m_preview) {
        m_preview = preview;
        resetZoom();
        invalidate();
      }
      if (m_loading && m_item->getThumbnailProgress() == 0.0) {
        m_loading = false;
        m_resolved = false;
        invalidate();
      }
      if (m_loading && m_item->getThumbnailProgress() >= 1.0) {
        m_loading = false;
        invalidate();
      }
    }
    return started;
  }

protected:
  bool onProcessMessage(Message* msg) override
  {
    if (m_preview) {
      if (msg->type() == kMouseWheelMessage && (msg->ctrlPressed() || msg->cmdPressed())) {
        auto* mouse = static_cast<MouseMessage*>(msg);
        const auto point = mouse->position() - bounds().origin();
        const int delta = mouse->wheelDelta().y;
        if (delta && m_previewArea.contains(point)) {
          changeZoom(m_zoom + (delta < 0 ? 1 : -1), point);
          return true;
        }
      }
      if (msg->type() == kKeyDownMessage && hasFocus() && msg->modifiers() == kKeyNoneModifier) {
        switch (static_cast<KeyMessage*>(msg)->scancode()) {
          case kKeyEquals:
          case kKeyPlusPad: changeZoom(m_zoom + 1, m_previewArea.center()); return true;
          case kKeyMinus:
          case kKeyMinusPad: changeZoom(m_zoom - 1, m_previewArea.center()); return true;
          case kKey0:
          case kKey0Pad: resetZoom(); return true;
          default: break;
        }
      }
      if (msg->type() == kMouseDownMessage && m_zoom > 0) {
        auto* mouse = static_cast<MouseMessage*>(msg);
        if (mouse->left() && m_previewArea.contains(mouse->position() - bounds().origin())) {
          requestFocus();
          captureMouse();
          m_dragging = true;
          m_dragStart = mouse->position();
          return true;
        }
      }
    }
    if (msg->type() == kMouseMoveMessage && m_dragging) {
      if (!hasCapture()) {
        m_dragging = false;
        return false;
      }
      const auto position = static_cast<MouseMessage*>(msg)->position();
      m_imageArea.offset(position - m_dragStart);
      m_imageArea = zoom_recent_preview(m_imageArea, m_previewArea, 1.0, m_previewArea.center());
      m_dragStart = position;
      invalidate();
      return true;
    }
    if (msg->type() == kMouseUpMessage && m_dragging) {
      m_dragging = false;
      if (hasCapture())
        releaseMouse();
      return true;
    }
    if (msg->type() == kFocusEnterMessage && parent())
      static_cast<RecentFilesGrid*>(parent())->revealCard(this);
    if (parent() && static_cast<RecentFilesGrid*>(parent())->navigate(this, msg))
      return true;
    return Button::onProcessMessage(msg);
  }

  void onResize(ResizeEvent& ev) override
  {
    const auto previousSize = bounds().size();
    setBoundsQuietly(ev.bounds());
    const int s = guiscale();
    const Rect area = clientBounds();
    const int inset = std::min(9 * s, std::max(0, (area.w - 1) / 2));
    const int width = area.w - 2 * inset;
    const auto pinSize = m_pin.sizeHint();
    const auto otherLabel = m_pinned ? Strings::home_view_pin_file() : Strings::home_view_unpin_file();
    const int extra = recent_text_width(otherLabel, m_pin.font()) - m_pin.textWidth();
    const int pinWidth = std::min(pinSize.w + std::max(0, extra), width);
    const int height = std::max(16 * s, pinSize.h);
    m_pin.setBounds(Rect(bounds().x2() - inset - pinWidth, bounds().y + 8 * s, pinWidth, height));
    const int top = 8 * s + height + 7 * s;
    m_previewFrame = Rect(inset, top, width, std::max(1, area.h - top - 36 * s));
    m_previewArea = m_previewFrame;
    m_previewArea.shrink(std::min({ 2 * s, (width - 1) / 2, (m_previewFrame.h - 1) / 2 }));
    if (previousSize != bounds().size())
      resetZoom();
  }

  void onPaint(PaintEvent& ev) override
  {
    auto* g = ev.graphics();
    auto* skin = skin::SkinTheme::get(this);
    const int s = guiscale();
    const auto text = skin->colors.text();
    const Rect area = clientBounds();
    PaintWidgetPartInfo chrome;
    chrome.styleFlags = (hasFocus() || m_pin.hasFocus() ? Style::Layer::kFocus : 0) |
                        (hasMouse() ? Style::Layer::kMouse : 0);
    g->fillRect(skin->colors.workspace(), area);
    g->saveClip();
    g->clipRegion(pixel_rounded_region(area));
    skin->paintWidgetPart(g, skin->styles.button(), area, chrome);
    g->restoreClip();

    const auto paintText = [&](const std::string& value, const Rect& row, gfx::Color color) {
      IntersectClip clip(g, row);
      if (clip)
        g->drawAlignedUIText(fit_recent_text(value, font(), row.w),
                             color, ColorNone, row, LEFT | MIDDLE);
    };
    const int inset = m_previewFrame.x;
    auto header = m_pin.bounds();
    header.offset(-bounds().origin());
    paintText(this->text(), Rect(inset, header.y, std::max(0, header.x - inset - 6 * s), header.h),
              text);

    {
      IntersectClip clip(g, m_previewArea);
      if (clip) {
        g->clipRegion(pixel_rounded_region(m_previewArea));
        const int tile = 5 * s;
        const auto visible = g->getClipBounds() & m_previewArea;
        const int firstX = m_previewArea.x + ((visible.x - m_previewArea.x) / tile) * tile;
        const int firstY = m_previewArea.y + ((visible.y - m_previewArea.y) / tile) * tile;
        for (int y = firstY; y < visible.y2(); y += tile) {
          for (int x = firstX; x < visible.x2(); x += tile) {
            const bool alternate = ((x - m_previewArea.x) / tile + (y - m_previewArea.y) / tile) % 2;
            g->fillRect(alternate ? rgba(104, 106, 112) : rgba(92, 94, 100), Rect(x, y, tile, tile));
          }
        }
        if (m_preview) {
          const Rect source(0, 0, m_preview->width(), m_preview->height());
          const auto target = m_zoom ? m_imageArea : fit_recent_preview(source.size(), m_previewArea);
          Paint paint;
          paint.blendMode(os::BlendMode::SrcOver);
          g->drawSurface(m_preview.get(), source, target, os::Sampling(), &paint);
        }
        else {
          g->drawAlignedUIText(!m_resolved || m_loading ? Strings::home_view_loading_preview() :
                                                          Strings::home_view_preview_unavailable(),
                               rgba(230, 230, 235), ColorNone, m_previewArea,
                               CENTER | MIDDLE | WORDWRAP);
        }
      }
    }
    g->saveClip();
    g->clipRegion(pixel_rounded_region(m_previewFrame));
    skin->paintWidgetPart(g, skin->styles.colorbarView(), m_previewFrame, PaintWidgetPartInfo());
    g->restoreClip();

    const int footer = m_previewFrame.y2() + 5 * s;
    paintText(recent_folder_label(m_path, font(), m_previewFrame.w),
              Rect(inset, footer, m_previewFrame.w, 11 * s), skin->colors.workspaceText());
    if (!m_opened.empty())
      paintText(Strings::home_view_last_opened(m_opened),
                Rect(inset, footer + 12 * s, m_previewFrame.w, 11 * s),
                skin->colors.workspaceText());
  }

private:
  void resetZoom()
  {
    m_zoom = 0;
    m_imageArea = Rect();
    if (m_dragging) {
      m_dragging = false;
      if (hasCapture())
        releaseMouse();
    }
    invalidate();
  }

  void changeZoom(const int requested, const Point& anchor)
  {
    const int next = std::clamp(requested, 0, 18);
    if (next == m_zoom)
      return;
    if (next == 0) {
      resetZoom();
      return;
    }

    const auto fit = fit_recent_preview(Size(m_preview->width(), m_preview->height()), m_previewArea);
    const auto current = m_zoom ? m_imageArea : fit;
    const double ratio = double(fit.w) * std::pow(1.33, next) / current.w;
    m_imageArea = zoom_recent_preview(current, m_previewArea, ratio, anchor);
    m_zoom = next;
    invalidate();
  }

  void showContextMenu()
  {
    // Menu callbacks can rebuild the grid; keep the path by value.
    const auto path = m_path;
    auto* grid = static_cast<RecentFilesGrid*>(parent());
    Menu menu;
    MenuItem open(Strings::home_view_open_recent_file());
    MenuItem pin(m_pinned ? Strings::home_view_unpin_file() : Strings::home_view_pin_file());
    MenuItem folder(Strings::home_view_show_recent_folder());
    MenuItem copy(Strings::home_view_copy_recent_path());
    MenuItem remove(Strings::home_view_remove_recent_file());
    remove.setId("recent_file_remove");
    const bool pinTarget = !m_pinned;
    open.Click.connect([grid, path] { grid->openFile(path); });
    pin.Click.connect(
      [path, pinTarget] { App::instance()->recentFiles()->setFilePinned(path, pinTarget); });
    folder.setEnabled(base::is_directory(base::get_file_path(path)));
    folder.Click.connect([path] { launcher::open_folder(path); });
    copy.Click.connect([path] { set_clipboard_text(path); });
    remove.Click.connect([path] { App::instance()->recentFiles()->forgetFile(path); });
    menu.addChild(&open);
    menu.addChild(&pin);
    menu.addChild(new MenuSeparator);
    menu.addChild(&folder);
    menu.addChild(&copy);
    menu.addChild(new MenuSeparator);
    menu.addChild(&remove);
    menu.showPopup(mousePosInDisplay(), display());
  }

  std::string m_path;
  bool m_pinned;
  std::string m_opened;
  Rect m_previewFrame, m_previewArea, m_imageArea;
  Point m_dragStart;
  int m_zoom = 0;
  bool m_dragging = false;
  RecentPinButton m_pin;
  IFileItem* m_item = nullptr;
  bool m_resolved = false;
  bool m_loading = false;
  os::SurfaceRef m_preview;
  obs::scoped_connection m_removedConnection;
};

RecentFilesGrid::RecentFilesGrid() : Widget(kGenericWidget), m_previewTimer(100, this)
{
  setId("recent_files_grid");
  m_languageConnection = Strings::instance()->LanguageChange.connect([this] { rebuild(); });
  m_recentConnection = App::instance()->recentFiles()->Changed.connect([this] { rebuild(); });
  m_removedConnection = FileSystemModule::instance()->ItemRemoved.connect(
    [this](IFileItem* item) { m_pendingPreviews.erase(item); });
  m_previewTimer.Tick.connect([this] { pollPreviews(); });
  m_previewTimer.start();
  rebuild();
}

RecentFilesGrid::~RecentFilesGrid()
{
  m_previewTimer.stop();
  // Workers refer only to FileSystemModule items, never to these widgets.
  // Do not cancel the shared generator used by the Open File dialog.
}

void RecentFilesGrid::rebuild()
{
  std::string focused;
  bool focusPin = false;
  RecentFileCard* focusedCard = nullptr;
  int focusedIndex = -1;
  while (auto* child = lastChild()) {
    auto* card = static_cast<RecentFileCard*>(child);
    if (card->hasFocus() || card->pinButton()->hasFocus()) {
      focusedIndex = int(children().size()) - 1;
      focused = card->path();
      focusPin = card->pinButton()->hasFocus();
    }
    m_tooltips.removeTooltipFor(card);
    m_tooltips.removeTooltipFor(card->pinButton());
    removeChild(card);
    card->deferDelete();
  }
  const auto* recent = App::instance()->recentFiles();
  std::set<std::string> paths;
  auto append = [&](const base::paths& list, bool pinned) {
    for (const auto& path : list) {
      if ((!pinned && recent->isFilePinned(path)) || !paths.insert(path).second)
        continue;
      auto* card = new RecentFileCard(path, pinned);
      addChild(card);
      m_tooltips.addTooltipFor(card, path + "\n" + Strings::home_view_preview_shortcuts());
      m_tooltips.addTooltipFor(
        card->pinButton(),
        pinned ? Strings::home_view_unpin_file() : Strings::home_view_pin_file());
      if (path == focused)
        focusedCard = card;
    }
  };
  append(recent->pinnedFiles(), true);
  append(recent->recentFiles(), false);
  for (auto it = m_previewVersions.begin(); it != m_previewVersions.end();) {
    if (!paths.count(it->first))
      it = m_previewVersions.erase(it);
    else
      ++it;
  }
  if (auto* view = View::getView(this))
    view->updateView();
  if (!focusedCard && focusedIndex >= 0 && hasChildren())
    focusedCard = static_cast<RecentFileCard*>(
      at(std::min(focusedIndex, int(children().size()) - 1)));
  if (focusedCard) {
    (focusPin ? focusedCard->pinButton() : static_cast<Widget*>(focusedCard))
      ->requestFocus();
    revealCard(focusedCard);
  }
  invalidate();
  if (isVisible())
    manager()->_updateMouseWidgets();
}

void RecentFilesGrid::onResize(ResizeEvent& ev)
{
  setBoundsQuietly(ev.bounds());
  auto area = childrenBounds();
  area.shrink(std::min(6 * guiscale(), std::max(0, (area.w - 1) / 2)));
  const auto grid = recent_grid_layout(area.w, int(children().size()), guiscale());
  for (int i = 0; i < int(children().size()); ++i) {
    auto rect = grid.itemBounds(i);
    rect.offset(area.origin());
    at(i)->setBounds(rect);
  }
}

void RecentFilesGrid::onSizeHint(SizeHintEvent& ev)
{
  const auto* view = View::getView(this);
  const int width = view ? view->viewportBounds().w : std::max(1, ev.fitInWidth());
  const int inset = 6 * guiscale();
  const auto grid = recent_grid_layout(width - 2 * inset, int(children().size()), guiscale());
  ev.setSizeHint(1, grid.height + 2 * inset);
}

void RecentFilesGrid::onPaint(PaintEvent& ev)
{
  auto* g = ev.graphics();
  auto* skin = skin::SkinTheme::get(this);
  g->fillRect(skin->colors.workspace(), clientBounds());
  if (!hasChildren())
    g->drawAlignedUIText(Strings::home_view_no_recent_files(),
                         skin->colors.text(),
                         skin->colors.workspace(),
                         clientBounds(),
                         CENTER | MIDDLE | WORDWRAP);
}

bool RecentFilesGrid::onProcessMessage(Message* msg)
{
  if (msg->type() == kMouseWheelMessage && View::getView(this)) {
    View::scrollByMessage(this, msg);
    manager()->_updateMouseWidgets();
    return true;
  }
  return Widget::onProcessMessage(msg);
}

void RecentFilesGrid::pollPreviews()
{
  const bool workersActive = m_startedPreviews && ThumbnailGenerator::instance()->checkWorkers();
  if (!isVisible() || !Manager::getDefault()->isWidgetClickable(this))
    return;
  auto* view = View::getView(this);
  if (!view)
    return;
  // Recheck visible files periodically, including unavailable files that return.
  // Do not walk offscreen/network paths on every animation tick.
  if (++m_refreshTicks >= 50) {
    m_refreshTicks = 0;
    for (auto* child : children())
      if (child->bounds().intersects(view->viewportBounds()))
        static_cast<RecentFileCard*>(child)->revalidate();
  }
  for (auto it = m_pendingPreviews.begin(); it != m_pendingPreviews.end();) {
    const double progress = (*it)->getThumbnailProgress();
    if (progress <= 0.0 || progress >= 1.0) {
      it = m_pendingPreviews.erase(it);
    }
    else {
      // A worker may finish between queuing an item and starting its thread.
      if (!workersActive && progress == 0.00001)
        ThumbnailGenerator::instance()->generateThumbnail(*it);
      ++it;
    }
  }
  int loading = int(m_pendingPreviews.size());
  for (auto* child : children()) {
    auto* card = static_cast<RecentFileCard*>(child);
    card->pollPreview(false, m_previewVersions);
  }
  // Limit concurrent document decoding and only queue visible cards.
  for (auto* child : children()) {
    auto* card = static_cast<RecentFileCard*>(child);
    if (loading >= 2)
      break;
    if (card->bounds().intersects(view->viewportBounds()) &&
        card->pollPreview(true, m_previewVersions)) {
      m_pendingPreviews.insert(card->fileItem());
      m_startedPreviews = true;
      break;
    }
  }
}

void RecentFilesGrid::openFile(const std::string& path)
{
  if (!base::is_file(path)) {
    Alert::show(Strings::alerts_recent_file_doesnt_exist());
    return;
  }
  Params params;
  params.set("filename", path.c_str());
  UIContext::instance()->executeCommandFromMenuOrShortcut(
    Commands::instance()->byId(CommandId::OpenFile()),
    params);
}

bool RecentFilesGrid::navigate(RecentFileCard* card, Message* message)
{
  if (message->type() != kKeyDownMessage || message->modifiers() != kKeyNoneModifier)
    return false;
  const auto& list = children();
  const auto it = std::find(list.begin(), list.end(), card);
  if (it == list.end())
    return false;
  const int index = int(it - list.begin());
  const int count = int(list.size());
  const int columns = recent_grid_layout(childrenBounds().w - 12 * guiscale(), count, guiscale()).columns;
  int next = index;
  switch (static_cast<KeyMessage*>(message)->scancode()) {
    case kKeyLeft:  next = index - 1; break;
    case kKeyRight: next = index + 1; break;
    case kKeyUp:    next = index >= columns ? index - columns : index; break;
    case kKeyDown:
      next = index / columns < (count - 1) / columns ? std::min(index + columns, count - 1) : index;
      break;
    case kKeyHome: next = 0; break;
    case kKeyEnd:  next = count - 1; break;
    default:       return false;
  }
  next = std::clamp(next, 0, count - 1);
  at(next)->requestFocus();
  revealCard(at(next));
  return true;
}

void RecentFilesGrid::revealCard(Widget* card)
{
  if (auto* view = View::getView(this)) {
    auto scroll = view->viewScroll();
    const auto item = card->bounds();
    const auto viewport = view->viewportBounds();
    if (item.y < viewport.y)
      scroll.y -= viewport.y - item.y;
    else if (item.y2() > viewport.y2())
      scroll.y += item.y2() - viewport.y2();
    view->setViewScroll(scroll);
  }
}

} // namespace app
