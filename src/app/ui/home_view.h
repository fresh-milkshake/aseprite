// Aseprite
// Copyright (C) 2019-2024  Igara Studio S.A.
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_HOME_VIEW_H_INCLUDED
#define APP_UI_HOME_VIEW_H_INCLUDED
#pragma once

#include "app/ui/input_chain_element.h"
#include "app/ui/tabs.h"
#include "app/ui/workspace_view.h"
#include "obs/connection.h"
#include "ui/box.h"

#include "home_view.xml.h"

namespace ui {
class View;
}

namespace app {
class DataRecoveryView;
class RecentFilesGrid;
class RecentFoldersListBox;

namespace crash {
class DataRecovery;
}

class HomeView : public app::gen::HomeView,
                 public TabView,
                 public WorkspaceView,
                 public app::InputChainElement
{
public:
  HomeView();
  ~HomeView();

  // When crash::DataRecovery finish to search for sessions, this
  // function is called.
  void dataRecoverySessionsAreReady();

#if ENABLE_SENTRY
  void updateConsentCheckbox();
#endif

  // TabView implementation
  std::string getTabText() override;
  TabIcon getTabIcon() override;
  gfx::Color getTabColor() override;

  // WorkspaceView implementation
  ui::Widget* getContentWidget() override { return this; }
  bool onCloseView(Workspace* workspace, bool quitting) override;
  void onAfterRemoveView(Workspace* workspace) override;
  void onTabPopup(Workspace* workspace) override;
  void onWorkspaceViewSelected() override;
  InputChainElement* onGetInputChainElement() override { return this; }

  // InputChainElement impl
  void onNewInputPriority(InputChainElement* element, const ui::Message* msg) override;
  bool onCanCut(Context* ctx) override;
  bool onCanCopy(Context* ctx) override;
  bool onCanPaste(Context* ctx) override;
  bool onCanClear(Context* ctx) override;
  bool onCut(Context* ctx) override;
  bool onCopy(Context* ctx) override;
  bool onPaste(Context* ctx, const gfx::Point* position) override;
  bool onClear(Context* ctx) override;
  void onCancel(Context* ctx) override;

protected:
  void onResize(ui::ResizeEvent& ev) override;

private:
  void onNewFile();
  void onOpenFile();
  void onRecoverSprites();

  obs::scoped_connection m_languageConnection;
  RecentFilesGrid* m_files;
  RecentFoldersListBox* m_folders;
  crash::DataRecovery* m_dataRecovery;
  DataRecoveryView* m_dataRecoveryView;
};

} // namespace app

#endif
