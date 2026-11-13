// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#define TEST_APP
#include "tests/app_test.h"

#include "app/ini_file.h"
#include "app/recent_files.h"
#include "base/fs.h"
#include "cfg/cfg.h"

#include <chrono>
#include <cstdio>
#include <filesystem>

using namespace app;
namespace fs = std::filesystem;

class RecentFilesTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
    folder = fs::temp_directory_path() / ("aseprite-recents-" + std::to_string(id));
    fs::create_directory(folder);
    config = (folder / "aseprite.ini").string();
    push_config_state();
    set_config_file(config.c_str());
  }

  void TearDown() override
  {
    pop_config_state();
    fs::remove_all(folder);
  }

  std::string path(const char* name) const
  {
    return base::get_absolute_path((folder / name).string());
  }

  fs::path folder;
  std::string config;
};

TEST_F(RecentFilesTest, OpenPersistsBeforeShutdown)
{
  RecentFiles recent(16);
  int changes = 0;
  recent.Changed.connect([&] { ++changes; });
  recent.addRecentFile(path("drawing.aseprite"));

  ASSERT_EQ(1u, recent.recentFiles().size());
  EXPECT_EQ(path("drawing.aseprite"), recent.recentFiles().front());
  EXPECT_EQ(1, changes);

  cfg::CfgFile disk;
  ASSERT_TRUE(disk.load(config));
  EXPECT_EQ(path("drawing.aseprite"), disk.getValue("RecentFiles", "0000", ""));
  EXPECT_EQ(base::get_absolute_path(folder.string()), disk.getValue("RecentPaths", "0000", ""));
}

TEST_F(RecentFilesTest, LastOpenedSurvivesPinningAndReload)
{
  RecentFiles recent(16);
  const auto opened = path("opened.aseprite");
  const auto saved = path("saved.aseprite");
  recent.recordOpenedFile(opened);
  const auto timestamp = recent.lastOpened(opened);
  ASSERT_GT(timestamp, 0);
  recent.addRecentFile(saved);
  recent.setFilePinned(opened, true);

  RecentFiles other(16);
  EXPECT_EQ(timestamp, other.lastOpened(opened));
  EXPECT_EQ(0, other.lastOpened(saved));
  other.forgetFile(opened);
  EXPECT_TRUE(recent.refresh());
  EXPECT_EQ(0, recent.lastOpened(opened));
}

TEST_F(RecentFilesTest, ReopenMovesToFrontWithoutDuplicates)
{
  RecentFiles recent(2);
  recent.addRecentFile(path("first.aseprite"));
  recent.addRecentFile(path("second.aseprite"));
  recent.addRecentFile(path("./first.aseprite"));
  ASSERT_EQ(2u, recent.recentFiles().size());
  EXPECT_EQ(path("first.aseprite"), recent.recentFiles().front());
  recent.addRecentFile(path("third.aseprite"));
  EXPECT_EQ((base::paths{ path("third.aseprite"), path("first.aseprite") }), recent.recentFiles());
}

TEST_F(RecentFilesTest, RelativePathsBecomeAbsolute)
{
  RecentFiles recent(16);
  recent.addRecentFile("drawing.aseprite");
  ASSERT_EQ(1u, recent.recentFiles().size());
  EXPECT_TRUE(base::is_absolute_path(recent.recentFiles().front()));
  EXPECT_EQ(base::get_absolute_path("drawing.aseprite"), recent.recentFiles().front());
}

TEST_F(RecentFilesTest, ReloadKeepsUnavailableFilesAndSkipsMigrationMarker)
{
  {
    RecentFiles recent(16);
    recent.addRecentFile(path("disconnected/drawing.aseprite"));
  }
  set_config_file(config.c_str());
  RecentFiles reloaded(16);
  EXPECT_EQ((base::paths{ path("disconnected/drawing.aseprite") }), reloaded.recentFiles());
  EXPECT_EQ((base::paths{ path("disconnected") }), reloaded.recentFolders());
}

TEST_F(RecentFilesTest, PinningRemovalAndClearPersist)
{
  RecentFiles recent(16);
  recent.setFiles({ path("pinned.aseprite") }, { path("other.aseprite") });
  recent.setFolders({ path("pinned-folder") }, { path("other-folder") });
  recent.addRecentFile(path("pinned.aseprite"));
  EXPECT_EQ((base::paths{ path("other.aseprite") }), recent.recentFiles());

  recent.removeRecentFile(path("other.aseprite"));
  recent.clear();
  cfg::CfgFile disk;
  ASSERT_TRUE(disk.load(config));
  EXPECT_EQ(path("pinned.aseprite"), disk.getValue("PinnedFiles", "0000", ""));
  EXPECT_EQ(path("pinned-folder"), disk.getValue("PinnedPaths", "0000", ""));
  EXPECT_EQ(nullptr, disk.getValue("RecentFiles", "0000", nullptr));
  EXPECT_EQ(nullptr, disk.getValue("RecentPaths", "0000", nullptr));
}

TEST_F(RecentFilesTest, IgnoresEmptyFilename)
{
  RecentFiles recent(16);
  recent.addRecentFile("");
  EXPECT_TRUE(recent.recentFiles().empty());
}

TEST_F(RecentFilesTest, PinningMovesFilesAheadAndPersists)
{
  RecentFiles recent(16);
  recent.addRecentFile(path("first.aseprite"));
  recent.addRecentFile(path("second.aseprite"));
  recent.setFilePinned(path("first.aseprite"), true);
  recent.setFilePinned(path("second.aseprite"), true);
  EXPECT_EQ((base::paths{ path("second.aseprite"), path("first.aseprite") }), recent.pinnedFiles());
  EXPECT_TRUE(recent.recentFiles().empty());
  EXPECT_TRUE(recent.isFilePinned(path("./first.aseprite")));

  cfg::CfgFile disk;
  ASSERT_TRUE(disk.load(config));
  EXPECT_EQ(path("second.aseprite"), disk.getValue("PinnedFiles", "0000", ""));
  EXPECT_EQ(path("first.aseprite"), disk.getValue("PinnedFiles", "0001", ""));
}

TEST_F(RecentFilesTest, UnpinningRestoresRecentWithoutDuplicates)
{
  RecentFiles recent(2);
  recent.addRecentFile(path("first.aseprite"));
  recent.addRecentFile(path("second.aseprite"));
  recent.setFilePinned(path("first.aseprite"), true);
  recent.setFilePinned(path("first.aseprite"), false);
  EXPECT_FALSE(recent.isFilePinned(path("first.aseprite")));
  EXPECT_TRUE(recent.pinnedFiles().empty());
  EXPECT_EQ((base::paths{ path("first.aseprite"), path("second.aseprite") }), recent.recentFiles());
  recent.setFilePinned(path("first.aseprite"), true);
  recent.setFilePinned(path("first.aseprite"), true);
  EXPECT_EQ(1u, recent.pinnedFiles().size());
}

TEST_F(RecentFilesTest, RecentLimitAndClearPreserveAllPinnedFiles)
{
  RecentFiles recent(16);
  recent.setFilePinned(path("first.aseprite"), true);
  recent.setFilePinned(path("second.aseprite"), true);
  recent.addRecentFile(path("other.aseprite"));
  recent.setLimit(1);
  recent.clear();
  EXPECT_EQ(2u, recent.pinnedFiles().size());
  EXPECT_TRUE(recent.recentFiles().empty());
  cfg::CfgFile disk;
  ASSERT_TRUE(disk.load(config));
  EXPECT_EQ(path("first.aseprite"), disk.getValue("PinnedFiles", "0001", ""));
}

TEST_F(RecentFilesTest, ForgetRemovesPinnedAndRecentEntriesWithoutDeletingFiles)
{
  const auto pinned = path("pinned.aseprite");
  const auto other = path("other.aseprite");
  {
    auto file = std::fopen(pinned.c_str(), "wb");
    ASSERT_NE(nullptr, file);
    std::fclose(file);
  }
  RecentFiles recent(16);
  recent.setFilePinned(pinned, true);
  recent.addRecentFile(other);
  recent.forgetFile(pinned);
  EXPECT_FALSE(recent.isFilePinned(pinned));
  EXPECT_TRUE(base::is_file(pinned));
  EXPECT_EQ((base::paths{ other }), recent.recentFiles());
  recent.forgetFile(other);
  RecentFiles restored(16);
  EXPECT_TRUE(restored.pinnedFiles().empty());
  EXPECT_TRUE(restored.recentFiles().empty());
}

TEST_F(RecentFilesTest, InterleavedEditorsPreserveEachOthersOpensAndPins)
{
  RecentFiles first(16), second(16);
  first.addRecentFile(path("first.aseprite"));
  second.addRecentFile(path("second.aseprite"));
  second.setFilePinned(path("first.aseprite"), true);
  first.addRecentFile(path("third.aseprite"));
  EXPECT_TRUE(first.isFilePinned(path("first.aseprite")));
  EXPECT_EQ((base::paths{ path("third.aseprite"), path("second.aseprite") }), first.recentFiles());
  EXPECT_TRUE(second.refresh());
  EXPECT_EQ(first.recentFiles(), second.recentFiles());
  EXPECT_FALSE(second.refresh());
}

TEST_F(RecentFilesTest, StaleExitCannotResurrectClearedOrForgottenHistory)
{
  RecentFiles first(16);
  first.addRecentFile(path("first.aseprite"));
  {
    RecentFiles stale(16);
    first.clear();
    // Stale destructor mirrors the old ini but cannot replace the authoritative store.
  }
  RecentFiles restored(16);
  EXPECT_TRUE(restored.recentFiles().empty());
  restored.addRecentFile(path("second.aseprite"));
  first.addRecentFile(path("third.aseprite"));
  EXPECT_EQ((base::paths{ path("third.aseprite"), path("second.aseprite") }), first.recentFiles());
}

TEST_F(RecentFilesTest, FolderDragPreservesConcurrentOpenAndForget)
{
  RecentFiles first(16);
  first.setFolders({}, { path("one"), path("two") });
  RecentFiles second(16);
  second.removeRecentFolder(path("two"));
  second.addRecentFile(path("three/image.aseprite"));
  first.setFolders({ path("one") }, { path("two") });
  EXPECT_EQ((base::paths{ path("one") }), first.pinnedFolders());
  EXPECT_EQ((base::paths{ path("three") }), first.recentFolders());
}

TEST_F(RecentFilesTest, LongUnicodePathsAreRetainedWithoutTruncation)
{
  auto longPath = folder;
  for (int i = 0; i < 15; ++i)
    longPath /= "long-texture-project-directory";
  longPath /= fs::u8path("текстура.aseprite");
  const auto expected = fs::absolute(longPath).lexically_normal().u8string();
  ASSERT_GT(expected.size(), 260u);
  RecentFiles recent(16);
  recent.addRecentFile(longPath.u8string());
  ASSERT_EQ(1u, recent.recentFiles().size());
  EXPECT_EQ(expected, recent.recentFiles().front());
  RecentFiles restored(16);
  EXPECT_EQ(expected, restored.recentFiles().front());
}

TEST_F(RecentFilesTest, ReloadHonorsViewLimitWithoutDeletingAnotherEditorsHistory)
{
  RecentFiles wide(4);
  for (const auto* file : { "one.aseprite", "two.aseprite", "three.aseprite", "four.aseprite" })
    wide.addRecentFile(path(file));
  RecentFiles narrow(2);
  EXPECT_EQ(2u, narrow.recentFiles().size());
  EXPECT_FALSE(narrow.refresh());
  RecentFiles restored(4);
  EXPECT_EQ(4u, restored.recentFiles().size());
}
