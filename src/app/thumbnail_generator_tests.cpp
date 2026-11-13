// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#define TEST_APP
#include "tests/app_test.h"

#include "app/file_system.h"
#include "app/thumbnail_generator.h"
#include "base/fs.h"

#include <chrono>
#include <thread>

using namespace app;

TEST(ThumbnailGenerator, CancelledPreviewsCanBeRetried)
{
  const auto path = base::get_absolute_path(
    base::join_path(base::get_file_path(base::get_app_path()),
                    "../../tests/sprites/abcd.aseprite"));
  ASSERT_TRUE(base::is_file(path));
  auto* item = FileSystemModule::instance()->getFileItemFromPath(path);
  ASSERT_NE(nullptr, item);
  auto* generator = ThumbnailGenerator::instance();
  auto wait = [&] {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (generator->checkWorkers()) {
      if (std::chrono::steady_clock::now() >= deadline)
        return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
  };

  generator->generateThumbnail(item);
  generator->stopAllWorkers();
  ASSERT_TRUE(wait());
  EXPECT_TRUE(item->getThumbnailProgress() == 0.0 || item->getThumbnailProgress() == 1.0);

  generator->generateThumbnail(item);
  ASSERT_TRUE(wait());
  const auto preview = item->getThumbnail();
  ASSERT_TRUE(preview);
  EXPECT_GT(preview->width(), 0);
  EXPECT_GT(preview->height(), 0);
  EXPECT_LE(preview->width(), 128);
  EXPECT_LE(preview->height(), 128);
}
