// Aseprite fork GUI integration checks. Run on Windows, never as a batch test.
// Uses the real application, SkinTheme, event loop, buttons and preview workers.
#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif
#include "app/app.h"
#include "app/app_menus.h"
#include "app/app_mod.h"
#include "app/cli/app_options.h"
#include "app/commands/commands.h"
#include "app/context.h"
#include "app/doc.h"
#include "app/file/file.h"
#include "app/file_system.h"
#include "app/i18n/strings.h"
#include "app/ini_file.h"
#include "app/recent_files.h"
#include "app/script/engine.h"
#include "app/script/luacpp.h"
#include "app/ui/app_menuitem.h"
#include "app/ui/filename_field.h"
#include "app/ui/keyboard_shortcuts.h"
#include "app/ui/main_menu_bar.h"
#include "app/ui/main_window.h"
#include "app/ui/preview_editor.h"
#include "app/ui/recent_files_grid.h"
#include "app/ui/skin/skin_theme.h"
#include "base/convert_to.h"
#include "base/fs.h"
#include "base/log.h"
#include "base/sha1.h"
#include "doc/cel.h"
#include "doc/image.h"
#include "doc/layer.h"
#include "doc/sprite.h"
#include "os/surface.h"
#include "os/system.h"
#include "os/window.h"
#include "ui/button.h"
#include "ui/display.h"
#include "ui/listbox.h"
#include "ui/manager.h"
#include "ui/menu.h"
#include "ui/message.h"
#include "ui/slider.h"
#include "ui/system.h"
#include "ui/theme.h"
#include "ui/timer.h"
#include "ui/view.h"
#include <cstdlib>
#if LAF_WINDOWS
  #include "base/win/coinit.h"
#endif
#include "minecraft_dialog_checks.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace fs = std::filesystem;

void require(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}

class GuiCheck : public app::AppMod {
public:
  int result = 1;
  fs::path output;

  void modMainWindow(app::MainWindow* window) override
  {
    main = window;
    base::set_log_filename((output / "native.log").string().c_str());
    base::set_log_level(LogLevel::VERBOSE);
    timer = std::make_unique<ui::Timer>(200);
    timer->Tick.connect([this] {
      timer->stop(); // Native Open File can pump a nested event loop.
      try {
        tick();
        if (result != 0)
          timer->start();
      }
      catch (const std::exception& error) {
        std::cerr << "GUI CHECK FAILED: " << error.what() << std::endl;
        timer->stop();
        main->closeWindow(nullptr);
      }
    });
    timer->start();
  }

private:
  app::MainWindow* main = nullptr;
  std::unique_ptr<ui::Timer> timer;
  int phase = 0, tries = 0, previewFrames = 0;
  app::RecentFilesGrid* grid = nullptr;
  std::string pinned;

  void key(ui::Widget* widget, ui::KeyScancode code, ui::MessageType type = ui::kKeyDownMessage)
  {
    ui::KeyMessage message(type, code, ui::kKeyNoneModifier, 0, 0);
    widget->sendMessage(&message);
  }

  void selectTab(ui::Widget* tab)
  {
    require(tab != nullptr, "Missing native dialog tab");
    tab->requestFocus();
    ui::Manager::getDefault()->dispatchMessages();
    key(tab, ui::kKeySpace);
    key(tab, ui::kKeySpace, ui::kKeyUpMessage);
  }

  void screenshot(const char* name)
  {
    auto* surface = ui::Manager::getDefault()->display()->nativeWindow()->surface();
    auto sprite = doc::Sprite::MakeStdSprite(
      doc::ImageSpec(doc::ColorMode::RGB, surface->width(), surface->height()),
      256);
    auto* image = sprite->firstLayer()->cel(0)->image();
    for (int y = 0; y < image->height(); ++y)
      for (int x = 0; x < image->width(); ++x) {
        const auto color = surface->getPixel(x, y);
        image->putPixel(x, y, doc::rgba(gfx::getr(color), gfx::getg(color), gfx::getb(color), 255));
      }
    app::Doc document(sprite);
    document.setFilename((output / name).string());
    require(app::save_document(app::App::instance()->context(), &document) == 0,
            "Cannot save GUI screenshot");
  }

  void closePreviewWindows()
  {
    auto* manager = ui::Manager::getDefault();
    while (manager->getTopWindow() != main)
      manager->getTopWindow()->closeWindow(nullptr);
  }
  void closePreview()
  {
    closePreviewWindows();
    app::App::instance()->scriptEngine()->evalCode("if app.sprite then app.sprite:close() end");
  }

  void grantExporterAccess()
  {
    auto* state = app::App::instance()->scriptEngine()->luaState();
    const int top = lua_gettop(state);
    lua_getglobal(state, "package");
    lua_getfield(state, -1, "loaded");
    lua_getfield(state, -1, "minecraft-tools/minecraft_tools_export");
    std::string source;
    if (lua_istable(state, -1)) {
      lua_getfield(state, -1, "write");
      lua_Debug info{};
      if (lua_isfunction(state, -1) && lua_getinfo(state, ">S", &info) && info.source &&
          info.source[0] == '@')
        source = info.source + 1;
    }
    lua_settop(state, top);
    require(!source.empty(), "Cannot locate exporter script for isolated GUI test permissions");
    const auto key = base::convert_to<std::string>(base::Sha1::calculateFromString(source));
    app::set_config_int("script_access", key.c_str(), 6); // Read/write in this isolated test
                                                          // profile only.
  }

  void checkResourceExports()
  {
    grantExporterAccess();
    auto* application = app::App::instance();
    require(application->scriptEngine()->evalCode(
              "local s=Sprite(16,16); s.cels[1].image:clear(Color{r=180,g=120,b=55}.rgbaPixel); "
              "s.filename='gui_block.aseprite'"),
            "Cannot create export fixture");
    check_minecraft_dialog(
      "MinecraftExport",
      "export",
      [this](ui::Window* window) {
        window->findChild("root")->setText((output / "java-resources").generic_string());
        window->findChild("namespace")->setText("gui_test");
        window->findChild("path")->setText("gui_block");
      },
      [this](const char* picture) { screenshot(picture); },
      "java-export-dialog.png",
      "Java Resources Exported");
    require(
      fs::is_regular_file(output / "java-resources/assets/gui_test/textures/block/gui_block.png"),
      "GUI block texture was not exported");
    require(fs::is_regular_file(output / "java-resources/assets/gui_test/items/gui_block.json"),
            "GUI item definition was not exported");
    application->scriptEngine()->evalCode("app.sprite:close()");
    check_minecraft_dialog(
      "MinecraftExportArmor",
      "confirm",
      [this](ui::Window* window) {
        window->findChild("root")->setText((output / "java-resources").generic_string());
        window->findChild("namespace")->setText("gui_test");
        window->findChild("path")->setText("gui_armor");
      },
      [this](const char* picture) { screenshot(picture); },
      "java-armor-export-dialog.png",
      "Java Armor Exported");
    require(fs::is_regular_file(output / "java-resources/assets/gui_test/equipment/gui_armor.json"),
            "GUI equipment definition was not exported");
  }

  void checkQuickPngExport()
  {
    auto* application = app::App::instance();
    auto* engine = application->scriptEngine();
    const auto shortcut = app::KeyboardShortcuts::instance()->command("MinecraftQuickExport");
    require(shortcut && !shortcut->shortcuts().empty(), "Quick PNG export needs a shortcut");
    require(engine->evalCode(
              "pngSheet=Sprite(32,16); pngSheet.selection:select(Rectangle(0,0,16,16)); "
              "pngSheet.cels[1].image:clear(Color{r=255,a=255}.rgbaPixel)"),
            "Cannot seed quick PNG export");
    app::Doc* source = application->context()->activeDocument();
    const auto sourcePath = output / "quick-sheet.aseprite";
    source->setFilename(sourcePath.string());
    require(app::save_document(application->context(), source) == 0,
            "Cannot save the PNG export source sheet");
    source->markAsSaved();

    const auto first = output / "quick-first.png";
    const auto second = output / "quick-second.png";
    const auto choose = [&](const fs::path& path) {
      check_minecraft_dialog(
        "MinecraftQuickExport",
        "export",
        [&](ui::Window* window) {
          auto* field = window->findChildT<app::FilenameField>("file");
          require(field != nullptr, "Missing PNG destination field");
          field->setFilename(path.generic_string());
        },
        [](const char*) {},
        "quick-export.png");
    };
    choose(first);
    require(engine->evalCode("pngSheet.selection:select(Rectangle(16,0,16,16))"),
            "Cannot select second PNG region");
    choose(second);
    require(fs::is_regular_file(first) && fs::is_regular_file(second),
            "Each selected region must export to its own file");

    const auto read = [&](const fs::path& path) {
      std::unique_ptr<app::Doc> document(app::load_document(application->context(), path.string()));
      require(document && document->sprite()->width() == 16 && document->sprite()->height() == 16,
              "Quick export must crop the PNG to the selection");
      return document->sprite()->firstLayer()->cel(0)->image()->getPixel(0, 0);
    };
    require(read(first) == doc::rgba(255, 0, 0, 255), "First export must capture the source pixels");
    require(engine->evalCode("pngSheet:close()"), "Cannot close the source before reopening it");
    app::Params open;
    open.set("filename", sourcePath.string().c_str());
    application->context()->executeCommand(app::Commands::instance()->byId("OpenFile"), open);
    require(engine->evalCode(
              "pngSheet=assert(app.sprite); assert(pngSheet.width==32 and pngSheet.height==16); "
              "pngSheet.selection:select(Rectangle(0,0,16,16)); "
              "pngSheet.cels[1].image:putPixel(0,0,Color{r=0,g=0,b=255,a=255}.rgbaPixel); "
              "app.command.MinecraftQuickExport()"),
            "Cannot repeat PNG export");
    require(read(first) == doc::rgba(0, 0, 255, 255), "Repeat export must update the original PNG");
    require(read(second) == doc::rgba(255, 0, 0, 255), "Repeat export changed the other PNG");
    require(engine->evalCode("pngSheet:close()"), "Cannot close PNG export fixture");
  }

  void checkPreviewControls()
  {
    auto* window = ui::Manager::getDefault()->getTopWindow();
    auto* surface = ui::Manager::getDefault()->display()->nativeWindow()->surface();
    const gfx::Rect viewport(0, 0, surface->width(), surface->height());
    for (const auto* id : { "refresh_preview", "close_preview" }) {
      auto* button = window->findChild(id);
      if (!button || !viewport.contains(button->bounds())) {
        screenshot("preview-layout-failure.png");
        throw std::runtime_error(std::string("Preview control outside the narrow window: ") + id);
      }
    }
  }

  void checkContextRemoval()
  {
    main->showHome();
    auto* recent = app::App::instance()->recentFiles();
    const auto path = recent->pinnedFiles().front();
    bool removed = false;
    ui::Timer driver(350);
    driver.Tick.connect([&] {
      driver.stop();
      auto* window = ui::Manager::getDefault()->getTopWindow();
      auto* item = window ? window->findChildT<ui::MenuItem>("recent_file_remove") : nullptr;
      require(item != nullptr, "Missing recent context-menu action");
      screenshot("recent-context-menu.png");
      item->executeClick();
      removed = true;
      window->closeWindow(nullptr);
    });
    driver.start();
    dynamic_cast<ui::Button*>(grid->at(0))->RightClick();
    driver.stop();
    require(removed && !recent->isFilePinned(path), "Context removal must forget a pinned file");
    require(base::is_file(path), "Context removal must retain the artwork file");
  }

  void checkLanguages()
  {
    auto* strings = app::Strings::instance();
    const auto original = strings->currentLanguage();
    const auto capture = [this](const char* name) { screenshot(name); };
    for (const auto& target :
         { original == "ru" ? std::string("en") : std::string("ru"), original }) {
      check_minecraft_dialog(
        "Options",
        "button_ok",
        [target](ui::Window* window) {
          auto* combo = window->findChildT<ui::ComboBox>("language");
          require(combo != nullptr, "Preferences must expose the language selector");
          auto* variants = window->findChild("theme_variants");
          std::function<bool(ui::Widget*, const std::string&)> contains =
            [&](ui::Widget* widget, const std::string& text) {
              if (widget->text() == text)
                return true;
              for (auto* child : widget->children())
                if (contains(child, text))
                  return true;
              return false;
            };
          require(variants && contains(variants, app::Strings::options_theme_light()) &&
                    contains(variants, app::Strings::options_theme_dark()),
                  "Builtin theme variants must follow the interface language");
          int index = -1;
          const auto suffix = " (" + target + ")";
          for (int i = 0; i < combo->getItemCount(); ++i) {
            const auto& text = combo->getItemText(i);
            if (text.size() >= suffix.size() &&
                text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0)
              index = i;
          }
          require(index >= 0, "Bundled English and Russian must appear in Preferences");
          combo->setSelectedItemIndex(index);
          combo->Change();
        },
        capture,
        strings->currentLanguage() == "ru" ? "preferences-russian.png" : "preferences-english.png");
      require(strings->currentLanguage() == target, "Preferences must apply the chosen language");
      require(main->findChild("new_file")->text() == strings->translate("home_view.new_file"),
              "Home links must update without restarting");
      auto* root = app::AppMenus::instance()->getRootMenu();
      std::function<app::AppMenuItem*(ui::Menu*)> findCommand =
        [&](ui::Menu* menu) -> app::AppMenuItem* {
        for (auto* child : menu->children()) {
          auto* item = dynamic_cast<app::AppMenuItem*>(child);
          if (!item)
            continue;
          if (item->getCommandId() == "MinecraftNewTexture")
            return item;
          if (item->getSubmenu()) {
            if (auto* found = findCommand(item->getSubmenu()))
              return found;
          }
        }
        return nullptr;
      };
      auto* command = findCommand(root);
      require(command && command->text() == minecraft_title("New Texture / Sheet..."),
              "Persistent plugin commands must follow the UI language");
    }
  }

  void tick()
  {
    auto* application = app::App::instance();
    auto* recent = application->recentFiles();
    auto* context = application->context();
    if (phase == 0) {
      main->showHome();
      checkLanguages();
      auto* root = app::AppMenus::instance()->getRootMenu();
      auto* help = root->findChild("help_menu");
      require(help && help->nextSibling() && help->nextSibling()->id() == "minecraft_menu",
              "Minecraft must immediately follow Help");
      const auto seed = base::get_absolute_path("tests/gui/seed.lua");
      const auto key = base::convert_to<std::string>(base::Sha1::calculateFromString(seed));
      app::set_config_int("script_access", key.c_str(), 6); // Read/write for this test fixture
                                                            // only.
      require(application->scriptEngine()->evalFile(seed), "Cannot seed GUI fixtures");
      main->getMenuBar()->reload();
      root = app::AppMenus::instance()->getRootMenu();
      help = root->findChild("help_menu");
      require(help && help->nextSibling() && help->nextSibling()->id() == "minecraft_menu",
              "Minecraft menu must survive a native menu reload");
      auto* minecraftItem = dynamic_cast<ui::MenuItem*>(help->nextSibling());
      require(minecraftItem && minecraftItem->mnemonic() == 'M',
              "Minecraft must expose a native menu mnemonic");
      auto* minecraft = minecraftItem->getSubmenu();
      auto* quickExport = dynamic_cast<app::AppMenuItem*>(minecraft->firstChild());
      require(quickExport && quickExport->getCommandId() == "MinecraftQuickExport",
              "Quick PNG export must be the first Minecraft action");
      bool hasItemPreview = false;
      for (auto* child : minecraft->children()) {
        if (auto* item = dynamic_cast<app::AppMenuItem*>(child)) {
          const auto id = item->getCommandId();
          hasItemPreview |= id == "MinecraftItemPreview";
          require(id != "MinecraftOutline" && id != "MinecraftTransform" && id != "MinecraftExport",
                  "Minecraft top-level menu must focus on everyday sprite work");
        }
      }
      require(hasItemPreview, "Item preview must be directly accessible");
      auto* manager = ui::Manager::getDefault();
      auto* editItem = dynamic_cast<ui::MenuItem*>(root->findItemById("edit_menu"));
      require(editItem && editItem->getSubmenu(), "Missing native Edit menu");
      editItem->openSubmenu();
      manager->dispatchMessages();
      manager->flushRedraw();
      manager->dispatchMessages();
      screenshot("edit-menu.png");
      const int nativeRowHeight = editItem->getSubmenu()->firstChild()->bounds().h;
      main->getMenuBar()->cancelMenuLoop();
      manager->dispatchMessages();
      app::Command* showMenu = nullptr;
      app::Params menuParams;
      ui::KeyMessage altM(ui::kKeyDownMessage, ui::kKeyM, ui::kKeyAltModifier, 'm', 0);
      require(app::KeyboardShortcuts::instance()->getCommandFromKeyMessage(
                &altM, &showMenu, &menuParams) &&
                showMenu == app::Commands::instance()->byId("ShowMenu") &&
                menuParams.get("menu") == "minecraft_menu",
              "Alt+M must resolve to the Minecraft menu");
      context->executeCommand(showMenu, menuParams);
      manager->dispatchMessages();
      manager->flushRedraw();
      manager->dispatchMessages();
      require(minecraftItem->hasSubmenuOpened(), "ShowMenu must open Minecraft by id");
      screenshot("minecraft-menu.png");
      for (auto* child : minecraft->children()) {
        if (dynamic_cast<app::AppMenuItem*>(child))
          require(child->bounds().h == nativeRowHeight, "Minecraft rows must match native Edit rows");
      }
      main->getMenuBar()->cancelMenuLoop();
      manager->dispatchMessages();
      auto* placeholder = root->findItemById("recent_files_placeholder");
      auto* recentMenu = placeholder ? dynamic_cast<ui::Menu*>(placeholder->parent()) : nullptr;
      require(recentMenu && std::count_if(recentMenu->children().begin(),
                                          recentMenu->children().end(),
                                          [](ui::Widget* widget) {
                                            auto* item = dynamic_cast<app::AppMenuItem*>(widget);
                                            return item && item->isRecentFileItem();
                                          }) == 16,
              "File > Open Recent must rebuild all persisted entries on menu reload");
      main->showHome();
      grid = main->findChildT<app::RecentFilesGrid>("recent_files_grid");
      require(grid && grid->children().size() == 16,
              "Recent files must contain every opened fixture");
      require(grid->at(0)->text() == "texture-16.aseprite",
              "Recent order must be most-recent first");
      phase = 1;
    }
    else if (phase == 1) {
      // Wait for real asynchronous document decoding and UI rendering.
      auto* item = app::FileSystemModule::instance()->getFileItemFromPath(
        recent->recentFiles().front());
      if (!item || !item->getThumbnail()) {
        require(++tries < 100, "Visible recent preview never loaded");
        return;
      }
      require(item->getThumbnail()->width() == 512, "Sheet thumbnail must retain 512px of detail");
      if (++previewFrames < 3)
        return; // Allow the card to paint its decoded image.
      auto* card = grid->at(0);
      auto* fileList = main->findChildT<ui::ListBox>("recent_files_list");
      require(fileList && fileList->getItemsCount() == 16, "Home must list all recent files");
      require(recent->lastOpened(recent->recentFiles().front()) > 0,
              "A successful Open File must record its time");
      require(!card->findChild("recent_file_preview"), "Preview should zoom in place");
      screenshot("recent-grid.png");
      card->requestFocus();
      ui::Manager::getDefault()->dispatchMessages();
      const auto position = card->bounds().center();
      auto* recentView = ui::View::getView(grid);
      ui::MouseMessage scroll(ui::kMouseWheelMessage, ui::PointerType::Unknown,
                              ui::kButtonNone, ui::kKeyNoneModifier, position, gfx::Point(0, 3));
      require(card->sendMessage(&scroll) && recentView->viewScroll().y > 0,
              "Wheel over a preview must scroll the recent grid");
      recentView->setViewScroll(gfx::Point());
      ui::MouseMessage wheel(ui::kMouseWheelMessage,
                             ui::PointerType::Unknown,
                             ui::kButtonNone,
                             ui::kKeyCtrlModifier,
                             position,
                             gfx::Point(0, -4));
      require(card->sendMessage(&wheel), "Ctrl+wheel over the preview must zoom it");
      key(card, ui::kKeyEquals);
      ui::MouseMessage down(ui::kMouseDownMessage, ui::PointerType::Unknown, ui::kButtonLeft,
                            ui::kKeyNoneModifier, position);
      ui::MouseMessage move(ui::kMouseMoveMessage, ui::PointerType::Unknown, ui::kButtonLeft,
                            ui::kKeyNoneModifier, position + gfx::Point(24, 0));
      ui::MouseMessage up(ui::kMouseUpMessage, ui::PointerType::Unknown, ui::kButtonLeft,
                          ui::kKeyNoneModifier, position + gfx::Point(24, 0));
      require(card->sendMessage(&down) && card->sendMessage(&move) && card->sendMessage(&up),
              "Dragging must pan within a zoomed card");
      auto* manager = ui::Manager::getDefault();
      manager->flushRedraw();
      manager->dispatchMessages();
      screenshot("recent-inline-zoom.png");
      key(card, ui::kKey0);
      require(context->activeDocument() == nullptr, "Zoom must not open the document");
      const auto firstPath = recent->recentFiles().front();
      const auto oldMouse = ui::get_mouse_position();
      const auto pinPosition = card->findChild("recent_file_pin")->bounds().center();
      ui::set_mouse_position(pinPosition, manager->display());
      manager->_updateMouseWidgets();
      for (const bool expected : { true, false }) {
        manager->dispatchMessages();
        auto* button = grid->at(0)->findChild("recent_file_pin");
        require(manager->getMouse() == button, "The stationary cursor must target the Pin button");
        for (const auto type : { ui::kMouseDownMessage, ui::kMouseUpMessage }) {
          ui::MouseMessage click(type, ui::PointerType::Unknown, ui::kButtonLeft,
                                 ui::kKeyNoneModifier, pinPosition);
          button->sendMessage(&click);
        }
        require(recent->isFilePinned(firstPath) == expected, "Repeated clicks must toggle Pin");
      }
      ui::set_mouse_position(oldMouse, nullptr);
      manager->_updateMouseWidgets();
      key(grid->at(0), ui::kKeyEnd);
      require(grid->at(15)->hasFocus(), "End must focus the final card");
      require(ui::View::getView(grid)->viewScroll().y > 0, "End must reveal the final card");
      ui::View::getView(grid)->setViewScroll(gfx::Point(0, 0));
      grid->at(0)->requestFocus();
      auto* pin = grid->at(15)->findChild("recent_file_pin");
      pin->requestFocus();
      ui::Manager::getDefault()->dispatchMessages();
      require(ui::View::getView(grid)->viewScroll().y > 0, "Tab focus on Pin must reveal its card");
      pinned = recent->recentFiles().back();
      key(pin, ui::kKeyEnter);
      key(pin, ui::kKeyEnter, ui::kKeyUpMessage);
      require(context->activeDocument() == nullptr, "Pin must not open the document");
      require(recent->isFilePinned(pinned), "Pin button did not persist its model state");
      require(grid->at(0)->text() == "texture-01.aseprite", "Pinned card must move to the front");
      require(grid->at(0)->findChild("recent_file_pin")->hasFocus(),
              "Pin focus must survive reorder");
      phase = 2;
    }
    else if (phase == 2) {
      screenshot("recent-pinned.png");
      auto* native = ui::Manager::getDefault()->display()->nativeWindow();
      native->setFrame(gfx::Rect(20, 20, 700, 600));
      phase = 3;
    }
    else if (phase == 3) {
      screenshot("recent-narrow.png");
      for (auto* card : grid->children()) {
        auto* pin = card->findChild("recent_file_pin");
        require(card->bounds().contains(pin->bounds()), "Pin must fit within a narrow card");
      }
      grid->at(0)->requestFocus();
      ui::Manager::getDefault()->dispatchMessages();
      key(grid->at(0), ui::kKeyEnter);
      key(grid->at(0), ui::kKeyEnter, ui::kKeyUpMessage);
      require(context->activeDocument() != nullptr, "Enter must open the selected recent file");
      require(context->activeDocument()->filename() == pinned, "Card opened the wrong file");
      auto* preview = main->getPreviewEditor();
      require(!preview->isVisible(), "Opening a file must not auto-open the mini editor");
      context->executeCommand(app::Commands::instance()->byId("TogglePreview"));
      require(preview->isVisible(), "The mini editor must still open when requested");
      context->executeCommand(app::Commands::instance()->byId("TogglePreview"));
      context->executeCommand(app::Commands::instance()->byId("CloseFile"));
      main->showHome();
      require(application->scriptEngine()->evalCode(
                "local s=Sprite(64,32); s.cels[1].image:clear(Color{r=90,g=155,b=105}.rgbaPixel); "
                "s.selection:select(Rectangle(16,0,16,16)); app.command.MinecraftBlockPreview()"),
              "Minecraft preview command failed");
      phase = 4;
    }
    else if (phase == 4) {
      checkPreviewControls();
      screenshot("minecraft-block-preview.png");
      auto* cube = ui::Manager::getDefault()->getTopWindow();
      auto* yaw = cube->findChildT<ui::Slider>("yaw");
      auto* pitch = cube->findChildT<ui::Slider>("pitch");
      auto* canvas = cube->findChild("preview");
      require(yaw && pitch && canvas, "Missing native cube rotation controls");
      const auto origin = canvas->bounds().center();
      const int oldYaw = yaw->getValue();
      const int oldPitch = pitch->getValue();
      for (const auto type : { ui::kMouseDownMessage, ui::kMouseMoveMessage, ui::kMouseUpMessage }) {
        ui::MouseMessage event(type, ui::PointerType::Unknown, ui::kButtonLeft,
                               ui::kKeyNoneModifier,
                               type == ui::kMouseDownMessage ? origin : origin + gfx::Point(12, 12));
        event.setRecipient(canvas);
        event.setDisplay(canvas->display());
        canvas->sendMessage(&event);
      }
      require(yaw->getValue() > oldYaw && pitch->getValue() < oldPitch,
              "A right/down drag must increase yaw and decrease pitch");
      selectTab(cube->findChild("scene_tab"));
      auto* environment = cube->findChildT<ui::ComboBox>("environment");
      auto* lighting = cube->findChildT<ui::ComboBox>("light");
      require(environment && lighting, "Missing block environment and lighting controls");
      for (int i : { 1, 2 }) {
        environment->setSelectedItemIndex(i);
        environment->Change();
        lighting->setSelectedItemIndex(i);
        lighting->Change();
        auto* manager = ui::Manager::getDefault();
        manager->flushRedraw();
        manager->dispatchMessages();
        screenshot(i == 1 ? "minecraft-block-grass.png" : "minecraft-block-cave.png");
      }
      selectTab(cube->findChild("view_tab"));
      yaw->setValue(120);
      yaw->Change();
      require(
        application->scriptEngine()->evalCode("app.sprite.selection:select(Rectangle(32,0,16,16))"),
        "Cannot select a second cube-face cell");
      selectTab(cube->findChild("faces_tab"));
      dynamic_cast<ui::Button*>(cube->findChild("assign_face"))->Click();
      dynamic_cast<ui::Button*>(cube->findChild("assign_all"))->Click();
      selectTab(cube->findChild("view_tab"));
      dynamic_cast<ui::Button*>(cube->findChild("reset_view"))->Click();
      require(yaw->getValue() == 35, "Reset View must restore default orientation");
      closePreviewWindows();
      require(application->scriptEngine()->evalCode("app.command.MinecraftTilePreview()"),
              "Tile preview failed");
      phase = 5;
    }
    else if (phase == 5) {
      checkPreviewControls();
      auto* tiling = ui::Manager::getDefault()->getTopWindow();
      require(tiling->findChild("light_background") && tiling->findChild("mark_center"),
              "Missing tiling preview options");
      auto* repeat = tiling->findChildT<ui::ComboBox>("tile_repeat");
      require(repeat && repeat->getItemCount() == 3, "Tiling must support three repeat sizes");
      repeat->setSelectedItemIndex(1);
      repeat->Change();
      tiling->findChild("light_background")->setSelected(true);
      dynamic_cast<ui::ButtonBase*>(tiling->findChild("light_background"))->Click();
      screenshot("minecraft-tiling-preview.png");
      closePreview();
      require(
        application->scriptEngine()->evalCode(
          "local s=Sprite(128,32); s.selection:select(Rectangle(64,0,16,16)); s.cels[1].image:clear(Color{r=160,g=95,b=190,a=160}.rgbaPixel); "
          "app.command.MinecraftItemPreview()"),
        "Inventory preview failed");
      phase = 6;
    }
    else if (phase == 6) {
      checkPreviewControls();
      screenshot("minecraft-inventory-preview.png");
      require(application->scriptEngine()->evalCode("app.sprite.selection:deselect()"),
              "Cannot change the inventory preview source selection");
      phase = 60;
    }
    else if (phase == 60) {
      auto* preview = ui::Manager::getDefault()->getTopWindow();
      auto* status = preview->findChild("item_preview_status");
      require(status && status->text() == minecraft_title("Select a cell or use Minecraft > Cell Navigator"),
              "Item preview must refresh after changing the selection");
      closePreview();
      phase = 7;
    }
    else if (phase == 7) {
      check_minecraft_templates([this](const char* name) { screenshot(name); });
      phase = 8;
    }
    else if (phase == 8) {
      checkPreviewControls();
      auto* armor = ui::Manager::getDefault()->getTopWindow();
      auto* angle = armor->findChildT<ui::ComboBox>("armor_view");
      require(angle && armor->findChild("armor_upper") && armor->findChild("armor_leggings"),
              "Armor preview must provide view and layer controls");
      screenshot("minecraft-armor-preview.png");
      timer->stop();
      auto* manager = ui::Manager::getDefault();
      while (manager->getTopWindow() != main)
        manager->getTopWindow()->closeWindow(nullptr);
      checkResourceExports();
      checkQuickPngExport();
      check_minecraft_sheet_tools([this](const char* name) { screenshot(name); },
                                  (output / "sheet-resources").generic_string());
      phase = 9;
    }
    else if (phase == 9) {
      screenshot("sheet-navigator.png");
      auto* navigator = ui::Manager::getDefault()->getTopWindow();
      selectTab(navigator->findChild("cells_tab"));
      auto* select = navigator->findChild("select");
      require(select != nullptr, "Missing navigator selection control");
      key(select, ui::kKeyEnter);
      key(select, ui::kKeyEnter, ui::kKeyUpMessage);
      require(application->scriptEngine()->evalCode(
                "assert(guiSheet.selection.bounds==Rectangle(16,0,16,16))"),
              "Navigator must select the requested grid cell");
      dynamic_cast<ui::Button*>(navigator->findChild("previous"))->Click();
      require(application->scriptEngine()->evalCode("assert(guiSheet.selection.bounds.x==0)"),
              "Prev must select the previous cell");
      dynamic_cast<ui::Button*>(navigator->findChild("next_cell"))->Click();
      selectTab(navigator->findChild("name_tab"));
      navigator->findChild("name")->setText("gui_named_cell");
      dynamic_cast<ui::Button*>(navigator->findChild("name_cell"))->Click();
      require(application->scriptEngine()->evalCode(
                "assert(#guiSheet.slices==1 and guiSheet.slices[1].name=='gui_named_cell')"),
              "Navigator must create native slice metadata");
      selectTab(navigator->findChild("cells_tab"));
      navigator->findChild("width")->setText("4");
      navigator->findChild("height")->setText("2");
      navigator->findChild("column")->setText("1");
      navigator->findChild("row")->setText("2");
      dynamic_cast<ui::Button*>(select)->Click();
      require(application->scriptEngine()->evalCode(
                "assert(guiSheet.selection.bounds==Rectangle(0,16,64,32))"),
              "Navigator must support armor-sized regions");
      navigator->closeWindow(nullptr);
      require(application->scriptEngine()->evalCode("app.command.MinecraftCellNavigator()"),
              "Navigator reopen failed");
      navigator = ui::Manager::getDefault()->getTopWindow();
      selectTab(navigator->findChild("cells_tab"));
      require(navigator->findChild("width")->text() == "4" &&
                navigator->findChild("height")->text() == "2",
              "Navigator must preserve an existing multi-cell region");
      screenshot("navigator-region.png");
      navigator->closeWindow(nullptr);
      application->scriptEngine()->evalCode("while #app.sprites>0 do app.sprites[1]:close() end");
      checkContextRemoval();
      result = 0;
      std::cout
        << "GUI CHECK PASSED: menus, rendered previews, pins, context actions, keyboard focus, Open File and armor dialogs"
        << std::endl;
      main->closeWindow(nullptr);
    }
  }
};

int app_main(int argc, char* argv[])
{
#if LAF_WINDOWS
  base::CoInit com;
#endif
  try {
    GuiCheck check;
    check.output = fs::absolute(std::getenv("ASEPRITE_GUI_LANGUAGE") ? "gui-results-ru" :
                                                                       "gui-results");
    fs::create_directories(check.output);
    auto system = os::System::make();
    app::App application(&check);
    app::AppOptions options(argc, const_cast<const char**>(argv));
    application.initialize(options);
    application.run(true);
    return check.result;
  }
  catch (const std::exception& error) {
    std::cerr << error.what() << std::endl;
    return 1;
  }
}

#if LAF_WINDOWS
int main(int argc, char* argv[])
{
  return app_main(argc, argv);
}
#endif
