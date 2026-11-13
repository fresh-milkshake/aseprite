// Aseprite fork: actual native dialog interaction on the Windows runner.
#pragma once
#include "app/app.h"
#include "app/commands/commands.h"
#include "app/context.h"
#include "app/doc.h"
#include "app/i18n/strings.h"
#include "app/script/engine.h"
#include "base/convert_to.h"
#include "base/sha1.h"
#include "doc/layer.h"
#include "doc/sprite.h"
#include "ui/combobox.h"
#include "ui/entry.h"
#include "ui/manager.h"
#include "ui/message.h"
#include "ui/slider.h"
#include "ui/timer.h"
#include "ui/window.h"
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

inline std::string minecraft_title(const char* source)
{
  const auto hash = base::convert_to<std::string>(base::Sha1::calculateFromString(source));
  return app::Strings::Translate(("minecraft.t_" + hash.substr(0, 12)).c_str());
}

inline void check_minecraft_dialog(const char* command,
                                   const char* buttonId,
                                   const std::function<void(ui::Window*)>& configure,
                                   const std::function<void(const char*)>& screenshot,
                                   const char* picture,
                                   const char* completionTitle = nullptr)
{
  auto* application = app::App::instance();
  const bool generator = std::string(command) == "MinecraftNewTexture" ||
                         std::string(command) == "MinecraftColorVariants" ||
                         std::string(command) == "MinecraftOutline" ||
                         std::string(command) == "MinecraftTransform" ||
                         std::string(command) == "MinecraftAlphaCleanup" ||
                         std::string(command) == "MinecraftNormalMap" ||
                         std::string(command) == "MinecraftMaterialRamp" ||
                         std::string(command) == "MinecraftOffset";
  if (generator && !application->scriptEngine()->evalCode(
                     "previewProbe={count=#app.sprites,sprite=app.sprite,cel=app.cel}; "
                     "if app.sprite then previewProbe.layers=#app.sprite.layers end; "
                     "if app.cel then previewProbe.image=Image(app.cel.image) end"))
    throw std::runtime_error("Cannot snapshot preview source");
  ui::Timer driver(350);
  std::exception_ptr failure;
  bool interacted = false;
  bool configured = false;
  bool submitted = false, completed = false, unexpected = false;
  driver.Tick.connect([&] {
    driver.stop();
    try {
      auto* manager = ui::Manager::getDefault();
      if (submitted) {
        auto* response = manager->getTopWindow();
        if (completionTitle && response && response->text() == minecraft_title(completionTitle))
          completed = true;
        else
          unexpected = true;
        if (response && !response->isDesktop())
          response->closeWindow(nullptr);
        if (!completed)
          driver.start();
        return;
      }
      auto* window = manager->getForegroundWindow();
      auto* button = window ? window->findChild(buttonId) : nullptr;
      if (!button) {
        screenshot("unexpected-dialog.png");
        if (window)
          window->closeWindow(nullptr);
        return;
      }
      if (!configured) {
        configure(window);
        // Emit the same changes as native user interaction after setting values.
        if (generator) {
          std::function<void(ui::Widget*)> changed = [&](ui::Widget* widget) {
            if (auto* combo = dynamic_cast<ui::ComboBox*>(widget))
              combo->Change();
            else if (auto* slider = dynamic_cast<ui::Slider*>(widget))
              slider->Change();
            for (auto* child : widget->children())
              changed(child);
          };
          changed(window);
        }
        configured = true;
        driver.start(); // Let the real dialog paint the selected values first.
        return;
      }
      if (generator) {
        if (!window->findChild("result_preview"))
          throw std::runtime_error("Generating dialog has no preview canvas");
        if (!application->scriptEngine()->evalCode(
              "assert(#app.sprites==previewProbe.count and app.sprite==previewProbe.sprite); "
              "if app.sprite then assert(#app.sprite.layers==previewProbe.layers) end; "
              "if previewProbe.cel then assert(previewProbe.cel.image:isEqual(previewProbe.image)) end"))
          throw std::runtime_error("Preview modified the source before confirmation");
      }
      screenshot(picture);
      button->requestFocus();
      manager->dispatchMessages();
      ui::KeyMessage down(ui::kKeyDownMessage, ui::kKeyEnter, ui::kKeyNoneModifier, 0, 0);
      ui::KeyMessage up(ui::kKeyUpMessage, ui::kKeyEnter, ui::kKeyNoneModifier, 0, 0);
      interacted = true;
      submitted = true;
      driver.start(); // Also close/report an unexpected native error dialog.
      button->sendMessage(&down);
      button->sendMessage(&up);
    }
    catch (...) {
      failure = std::current_exception();
      driver.stop();
      screenshot("failed-dialog.png");
      auto* window = ui::Manager::getDefault()->getForegroundWindow();
      if (window && !window->isDesktop())
        window->closeWindow(nullptr);
    }
  });
  driver.start();
  auto* app = app::App::instance();
  auto* action = app::Commands::instance()->byId(command);
  if (!action)
    throw std::runtime_error(std::string("Missing Minecraft command: ") + command);
  app->context()->executeCommand(action);
  driver.stop();
  if (failure)
    std::rethrow_exception(failure);
  if (!interacted || unexpected || (completionTitle && !completed)) {
    screenshot("failed-dialog.png");
    auto* window = ui::Manager::getDefault()->getTopWindow();
    std::function<void(ui::Widget*)> describe = [&](ui::Widget* widget) {
      if (!widget->text().empty())
        std::cerr << "GUI widget: " << widget->id() << " = " << widget->text() << std::endl;
      for (auto* child : widget->children())
        describe(child);
    };
    if (window)
      describe(window);
    throw std::runtime_error(std::string("Dialog interaction failed: ") + command);
  }
}

inline void check_minecraft_templates(const std::function<void(const char*)>& screenshot)
{
  auto* app = app::App::instance();
  for (const int kind : { 3, 4 }) {
    check_minecraft_dialog(
      "MinecraftNewTexture",
      "create",
      [kind](ui::Window* window) {
        auto* box = window->findChildT<ui::ComboBox>("kind");
        if (!box)
          throw std::runtime_error("Missing template selector");
        box->setSelectedItemIndex(kind);
      },
      screenshot,
      kind == 3 ? "armor-upper-dialog.png" : "armor-leggings-dialog.png");
    auto* document = app->context()->activeDocument();
    if (!document || document->sprite()->width() != 64 || document->sprite()->height() != 32)
      throw std::runtime_error("Armor template did not create a 64 x 32 atlas");
    bool guide = false;
    for (const auto* layer : document->sprite()->root()->layers())
      guide = guide || (layer->isReference() && !layer->isEditable());
    if (!guide)
      throw std::runtime_error("Template must include a locked reference guide");
    document->setFilename(kind == 3 ? "gui-upper.aseprite" : "gui-leggings.aseprite");
  }
  if (!app->scriptEngine()->evalCode(
        "for i,s in ipairs(app.sprites) do "
        " if s.width==64 and s.height==32 then "
        "  local l=s.layers[#s.layers]; for _,candidate in ipairs(s.layers) do "
        "   if not candidate.isReference then l=candidate end end; "
        "  l:cel(1).image:clear(Color{r=100+i*20,g=170,b=110}.rgbaPixel) "
        " end end"))
    throw std::runtime_error("Cannot paint armor GUI fixtures");
  check_minecraft_dialog(
    "MinecraftArmorPreview",
    "confirm",
    [](ui::Window*) {},
    screenshot,
    "armor-preview-dialog.png");
}

inline void check_minecraft_sheet_tools(const std::function<void(const char*)>& screenshot,
                                        const std::string& exportRoot)
{
  auto* app = app::App::instance();
  if (!app->scriptEngine()->evalCode(
        "guiSheet=Sprite(64,64); guiSheet.filename='gui-sheet.aseprite'; "
        "guiSheet.selection:select(Rectangle(16,0,16,16)); "
        "guiSheet.cels[1].image:putPixel(16,0,Color{r=240,g=70,b=40,a=120}.rgbaPixel)"))
    throw std::runtime_error("Cannot create sheet fixture");
  for (const auto& test : {
         std::pair<const char*, const char*>{ "MinecraftTransform",     "apply"  },
         { "MinecraftAlphaCleanup",  "apply"  },
         { "MinecraftOutline",       "create" },
         { "MinecraftNormalMap",     "create" },
         { "MinecraftMaterialRamp",  "create" },
         { "MinecraftColorVariants", "create" }
  }) {
    const auto name = std::string(test.first) + ".png";
    check_minecraft_dialog(test.first, test.second, [](ui::Window*) {}, screenshot, name.c_str());
    if (std::string(test.first) == "MinecraftColorVariants") {
      if (!app->scriptEngine()->evalCode(
            "assert(app.sprite.width==96 and app.sprite.height==16); app.sprite:close(); app.sprite=guiSheet"))
        throw std::runtime_error("Color variants did not produce a grid row");
    }
  }
  for (int pass = 0; pass < 2; ++pass) {
    check_minecraft_dialog(
      "MinecraftNormalMap",
      "create",
      [pass](ui::Window* window) {
        auto* strength = window->findChildT<ui::Slider>("strength");
        if (pass == 0) {
          strength->setValue(7);
          window->findChild("flip_green")->setSelected(true);
        }
        else if (strength->getValue() != 7 || !window->findChild("flip_green")->isSelected())
          throw std::runtime_error("Normal map options must persist across dialogs");
      },
      screenshot,
      "normal-options.png");
  }
  check_minecraft_dialog(
    "MinecraftNewTexture",
    "create",
    [](ui::Window* window) {
      window->findChildT<ui::ComboBox>("kind")->setSelectedItemIndex(0);
      window->findChild("insert")->setSelected(true);
    },
    screenshot,
    "sheet-template-guide.png");
  if (!app->scriptEngine()->evalCode(
        "assert(app.sprite==guiSheet); local guide=app.sprite.layers[#app.sprite.layers]; "
        "local found=false; for _,l in ipairs(app.sprite.layers) do if l.isReference and not l.isEditable then found=true end end; assert(found)"))
    throw std::runtime_error("Template guide must be inserted without creating another document");
  for (const char* command : { "MinecraftColorVariants",
                               "MinecraftOutline",
                               "MinecraftTransform",
                               "MinecraftAlphaCleanup",
                               "MinecraftNormalMap",
                               "MinecraftMaterialRamp",
                               "MinecraftOffset",
                               "MinecraftNewTexture" }) {
    check_minecraft_dialog(
      command,
      "cancel_preview",
      [](ui::Window*) {},
      screenshot,
      "preview-cancel.png");
    if (!app->scriptEngine()->evalCode(
          "assert(#app.sprites==previewProbe.count and app.sprite==previewProbe.sprite); "
          "if previewProbe.cel then assert(previewProbe.cel.image:isEqual(previewProbe.image)) end; "
          "if app.sprite then assert(#app.sprite.layers==previewProbe.layers) end"))
      throw std::runtime_error("Cancel changed the preview source");
  }
  if (!app->scriptEngine()->evalCode(
        "guiVariantProbe=Sprite(32,16); guiVariantProbe.selection:select(Rectangle(0,0,16,16)); "
        "guiVariantProbe.cels[1].image:putPixel(0,0,Color{r=220,g=30,b=20}.rgbaPixel)"))
    throw std::runtime_error("Cannot create selected-variant fixture");
  check_minecraft_dialog(
    "MinecraftColorVariants",
    "create",
    [](ui::Window* window) {
      window->findChildT<ui::ComboBox>("which")->setSelectedItemIndex(1);
      auto* canvas = window->findChild("result_preview");
      const auto b = canvas->bounds();
      const auto pos = gfx::Point(b.x + b.w * 5 / 12, b.y + b.h / 2);
      ui::MouseMessage down(ui::kMouseDownMessage,
                            ui::PointerType::Unknown,
                            ui::kButtonLeft,
                            ui::kKeyNoneModifier,
                            pos);
      ui::MouseMessage up(ui::kMouseUpMessage,
                          ui::PointerType::Unknown,
                          ui::kButtonLeft,
                          ui::kKeyNoneModifier,
                          pos);
      down.setRecipient(canvas);
      up.setRecipient(canvas);
      canvas->sendMessage(&down);
      canvas->sendMessage(&up);
    },
    screenshot,
    "selected-variant-preview.png");
  if (!app->scriptEngine()->evalCode(
        "assert(app.sprite.width==16 and app.sprite.height==16); "
        "assert(app.pixelColor.rgbaG(app.sprite.cels[1].image:getPixel(0,0))>180); "
        "app.sprite:close();guiVariantProbe:close();app.sprite=guiSheet"))
    throw std::runtime_error("Selected variant does not match the chosen preview");
  // Two native confirmation dialogs are intentionally exercised, not bypassed.
  ui::Timer driver(350);
  int stage = 0;
  bool completed = false, unexpected = false;
  driver.Tick.connect([&] {
    driver.stop();
    auto* manager = ui::Manager::getDefault();
    auto* window = manager->getTopWindow();
    const std::string title = window ? window->text() : "";
    if (stage == 0 && title == minecraft_title("Export Minecraft Sheet Cells")) {
      window->findChild("root")->setText(exportRoot);
      window->findChild("namespace")->setText("gui_test");
      stage = 1;
      driver.start();
      return;
    }
    if (stage == 1 && title == minecraft_title("Export Minecraft Sheet Cells")) {
      screenshot("sheet-export-dialog.png");
      auto* button = window->findChild("export");
      button->requestFocus();
      manager->dispatchMessages();
      stage = 2;
      driver.start();
      ui::KeyMessage down(ui::kKeyDownMessage, ui::kKeyEnter, ui::kKeyNoneModifier, 0, 0);
      ui::KeyMessage up(ui::kKeyUpMessage, ui::kKeyEnter, ui::kKeyNoneModifier, 0, 0);
      button->sendMessage(&down);
      button->sendMessage(&up);
      return;
    }
    if (stage == 2 && title == minecraft_title("Confirm Sheet Export")) {
      stage = 3;
      driver.start();
      ui::KeyMessage down(ui::kKeyDownMessage, ui::kKeyEnter, ui::kKeyNoneModifier, 0, 0);
      ui::KeyMessage up(ui::kKeyUpMessage, ui::kKeyEnter, ui::kKeyNoneModifier, 0, 0);
      manager->getFocus()->sendMessage(&down);
      manager->getFocus()->sendMessage(&up);
      return;
    }
    completed = stage == 3 && title == minecraft_title("Minecraft Sheet Exported");
    unexpected = !completed;
    if (window && !window->isDesktop())
      window->closeWindow(nullptr);
  });
  driver.start();
  app->context()->executeCommand(app::Commands::instance()->byId("MinecraftBatchExport"));
  driver.stop();
  if (!completed || unexpected)
    throw std::runtime_error("Sheet batch export dialog flow failed");
  if (!app->scriptEngine()->evalCode("app.command.MinecraftCellNavigator()"))
    throw std::runtime_error("Cell navigator command failed");
}
