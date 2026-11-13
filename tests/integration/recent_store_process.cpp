// Aseprite fork: production RecentFiles exercised across real Windows processes.
#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif
#include "app/app.h"
#include "app/cli/app_options.h"
#include "app/ini_file.h"
#include "app/recent_files.h"
#include "base/file_handle.h"
#include "base/fs.h"
#include "os/system.h"
#include <memory>
#if LAF_WINDOWS
  #include "base/win/coinit.h"
#endif
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

int app_main(int argc, char* argv[])
{
#if LAF_WINDOWS
  base::CoInit com;
#endif
  try {
    if (argc < 2)
      throw std::runtime_error("Missing probe operation");
    auto system = os::System::make();
    app::App application;
    const char* args[] = { argv[0], "--batch" };
    const app::AppOptions options(2, args);
    application.initialize(options);
    auto* recent = application.recentFiles();
    const std::string mode = argv[1];
    const auto pinned = base::get_absolute_path("shared-pinned.aseprite");
    if (mode == "worker" && argc == 3) {
      recent->setLimit(100);
      recent->setFilePinned(pinned, true);
      for (int i = 0; i < 50; ++i) {
        recent->addRecentFile(std::string(argv[2]) + "-" + std::to_string(i) + ".aseprite");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    }
    else if (mode == "verify-full") {
      if (recent->recentFiles().size() != 100 || recent->pinnedFiles().size() != 1 ||
          !recent->isFilePinned(pinned))
        throw std::runtime_error("Concurrent history or pin was lost");
    }
    else if (mode == "clear")
      recent->clear();
    else if (mode == "verify-empty") {
      if (!recent->recentFiles().empty() || !recent->isFilePinned(pinned))
        throw std::runtime_error("Stale process resurrected history or Clear dropped a pin");
    }
    else if (mode == "hold" && argc == 4) {
      if (recent->recentFiles().size() != 100)
        throw std::runtime_error("Cannot take stale snapshot");
      auto stale = std::make_unique<app::RecentFiles>(100);
      auto ready = base::open_file(argv[2], "wb");
      if (!ready || std::fputs("ready", ready.get()) < 0 || std::fflush(ready.get()) != 0)
        throw std::runtime_error("Cannot signal stale snapshot readiness");
      ready.reset();
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
      while (!base::is_file(argv[3])) {
        if (std::chrono::steady_clock::now() > deadline)
          throw std::runtime_error("Missing exit gate");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      // Serialize the old snapshot and flush the legacy ini, as GUI shutdown does.
      // The authoritative companion must remain cleared after this physical write.
      stale.reset();
      app::flush_config_file();
    }
    else
      throw std::runtime_error("Invalid probe operation");
    std::cout << "PASS " << mode << std::endl;
    return 0;
  }
  catch (const std::exception& error) {
    std::cerr << "RECENT PROCESS CHECK FAILED: " << error.what() << std::endl;
    return 1;
  }
}
#if LAF_WINDOWS
int main(int argc, char* argv[])
{
  return app_main(argc, argv);
}
#endif
