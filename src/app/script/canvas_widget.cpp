// Aseprite
// Copyright (c) 2022-2025  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#include "app/script/canvas_widget.h"

#include "app/color_spaces.h"
#include "app/script/graphics_context.h"
#include "app/ui/skin/skin_theme.h"
#include "os/system.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/resize_event.h"
#include "ui/size_hint_event.h"
#include "ui/system.h"

namespace app { namespace script {

// static
ui::WidgetType Canvas::Type()
{
  static ui::WidgetType type = ui::kGenericWidget;
  if (type == ui::kGenericWidget)
    type = ui::register_widget_type();
  return type;
}

// static
bool Canvas::s_stopKeyEventPropagation = false;

// static
void Canvas::stopKeyEventPropagation()
{
  s_stopKeyEventPropagation = true;
}

Canvas::Canvas() : ui::Widget(Type())
{
}

int Canvas::frameInset() const
{
  return m_framed ? 3 * ui::guiscale() : 0;
}

void Canvas::callPaint()
{
  if (!m_surface)
    return;

  os::Paint p;
  p.color(bgColor(), m_surface->colorSpace().get());
  m_surface->drawRect(m_surface->bounds(), p);

  // Draw only on resize (onPaint we draw the cached m_surface)
  GraphicsContext gc(m_surface, m_autoScaling ? ui::guiscale() : 1);
  if (m_autoScaling) {
    auto theme = skin::SkinTheme::get(this);
    gc.font(theme->getUnscaledFont(font()));
  }
  else
    gc.font(font());

  Paint(gc);
}

void Canvas::onInitTheme(ui::InitThemeEvent& ev)
{
  Widget::onInitTheme(ev);

  gfx::Color bg;
  if (auto theme = skin::SkinTheme::get(this))
    bg = theme->colors.windowFace();
  else
    bg = gfx::rgba(0, 0, 0);
  setBgColor(bg);
}

template<typename T>
static T canvasMessage(T* msg, const Canvas& canvas)
{
  static_assert(std::is_base_of<ui::Message, T>::value,
                "type parameter must derive from ui::Message");
  const auto origin = canvas.bounds().origin();
  const int inset = canvas.frameInset();
  const int scale = canvas.isAutoScaling() ? ui::guiscale() : 1;
  auto result = T(msg->type(), *msg, origin + (msg->position() - origin - gfx::Point(inset, inset)) / scale);
  result.setRecipient(static_cast<ui::Message*>(msg)->recipient());
  result.setDisplay(static_cast<ui::Message*>(msg)->display());
  return result;
}

bool Canvas::onProcessMessage(ui::Message* msg)
{
  switch (msg->type()) {
    case ui::kKeyDownMessage:
      if (hasFocus()) {
        s_stopKeyEventPropagation = false;
        auto keyMsg = static_cast<ui::KeyMessage*>(msg);
        KeyDown(keyMsg);
        if (s_stopKeyEventPropagation)
          return true;
      }
      break;

    case ui::kKeyUpMessage:
      if (hasFocus()) {
        s_stopKeyEventPropagation = false;
        auto keyMsg = static_cast<ui::KeyMessage*>(msg);
        KeyUp(keyMsg);
        if (s_stopKeyEventPropagation)
          return true;
      }
      break;

    case ui::kSetCursorMessage: ui::set_mouse_cursor(m_cursorType); return true;

    case ui::kMouseMoveMessage: {
      auto mouseMsg = canvasMessage(static_cast<ui::MouseMessage*>(msg), *this);
      MouseMove(&mouseMsg);
      break;
    }

    case ui::kMouseDownMessage: {
      if (!hasCapture())
        captureMouse();

      if (isFocusStop() && !hasFocus())
        requestFocus();

      auto mouseMsg = canvasMessage(static_cast<ui::MouseMessage*>(msg), *this);
      MouseDown(&mouseMsg);
      break;
    }

    case ui::kMouseUpMessage: {
      auto mouseMsg = canvasMessage(static_cast<ui::MouseMessage*>(msg), *this);
      MouseUp(&mouseMsg);

      if (hasCapture())
        releaseMouse();
      break;
    }

    case ui::kDoubleClickMessage: {
      auto mouseMsg = canvasMessage(static_cast<ui::MouseMessage*>(msg), *this);
      DoubleClick(&mouseMsg);
      break;
    }

    case ui::kMouseWheelMessage:
      if (Wheel) {
        auto mouseMsg = canvasMessage(static_cast<ui::MouseMessage*>(msg), *this);
        Wheel(&mouseMsg);
        return true;
      }
      break;

    case ui::kTouchMagnifyMessage: {
      auto touchMsg = canvasMessage(static_cast<ui::TouchMessage*>(msg), *this);
      TouchMagnify(&touchMsg);
      break;
    }
  }
  return ui::Widget::onProcessMessage(msg);
}

void Canvas::onResize(ui::ResizeEvent& ev)
{
  Widget::onResize(ev);

  const os::SystemRef system = os::System::instance();
  auto inner = clientBounds();
  inner.shrink(frameInset());
  if (system && !inner.isEmpty()) {
    int w = inner.w;
    int h = inner.h;

    if (m_autoScaling) {
      w = w / ui::guiscale() + (w % ui::guiscale());
      h = h / ui::guiscale() + (h % ui::guiscale());
    }

    if (!m_surface || m_surface->width() != w || m_surface->height() != h) {
      ui::Display* display = this->display();
      os::ColorSpaceRef cs = (display ? display->colorSpace() : nullptr);
      m_surface = system->makeSurface(w, h, cs);
      callPaint();
    }
  }
  else
    m_surface.reset();
}

void Canvas::onPaint(ui::PaintEvent& ev)
{
  auto g = ev.graphics();
  gfx::Rect rc = clientBounds();
  if (m_framed) {
    auto* theme = skin::SkinTheme::get(this);
    theme->paintWidgetPart(g, theme->styles.editorView(), rc, ui::PaintWidgetPartInfo(this));
    rc.shrink(frameInset());
  }
  if (m_surface) {
    ui::IntersectClip clip(g, rc);
    if (!clip)
      return;
    if (m_autoScaling) {
      rc.w += (rc.w % ui::guiscale());
      rc.h += (rc.h % ui::guiscale());
    }
    g->drawSurface(m_surface.get(), m_surface->bounds(), rc, os::Sampling(), nullptr);
  }
}

}} // namespace app::script
