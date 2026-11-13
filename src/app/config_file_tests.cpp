// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#include "cfg/cfg.h"
#include "tests/app_test.h"
#include <chrono>
#include <filesystem>
#ifdef _WIN32
  #include <windows.h>
#endif

namespace fs = std::filesystem;
class ConfigFileTest : public ::testing::Test {
protected:
  fs::path folder;
  void SetUp() override
  {
    folder = fs::temp_directory_path() /
             ("aseprite-settings-" +
              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(folder);
  }
  void TearDown() override
  {
    for (const auto& file : fs::directory_iterator(folder))
      fs::permissions(file.path(), fs::perms::owner_all, fs::perm_options::add);
    fs::remove_all(folder);
  }
};

TEST_F(ConfigFileTest, AtomicallyReplacesAndReloadsSettings)
{
  const auto path = (folder / "aseprite.ini").u8string();
  cfg::CfgFile writer;
  ASSERT_TRUE(writer.load(path));
  writer.setValue("RecentFiles", "0000", "C:/art/first.aseprite");
  ASSERT_TRUE(writer.trySave());
  writer.setValue("RecentFiles", "0000", "C:/art/second.aseprite");
  ASSERT_TRUE(writer.trySave());
  cfg::CfgFile reader;
  ASSERT_TRUE(reader.load(path));
  EXPECT_STREQ("C:/art/second.aseprite", reader.getValue("RecentFiles", "0000", ""));
  EXPECT_EQ(1, std::distance(fs::directory_iterator(folder), fs::directory_iterator()));
}

#ifdef _WIN32
TEST_F(ConfigFileTest, FailedReplacePreservesPreviousSettings)
{
  const auto path = folder / "aseprite.ini";
  cfg::CfgFile writer;
  ASSERT_TRUE(writer.load(path.u8string()));
  writer.setValue("RecentFiles", "0000", "previous.aseprite");
  ASSERT_TRUE(writer.trySave());
  const auto attributes = GetFileAttributesW(path.c_str());
  ASSERT_NE(INVALID_FILE_ATTRIBUTES, attributes);
  ASSERT_TRUE(SetFileAttributesW(path.c_str(), attributes | FILE_ATTRIBUTE_READONLY));
  writer.setValue("RecentFiles", "0000", "new.aseprite");
  EXPECT_FALSE(writer.trySave());
  cfg::CfgFile reader;
  ASSERT_TRUE(reader.load(path.u8string()));
  EXPECT_STREQ("previous.aseprite", reader.getValue("RecentFiles", "0000", ""));
  EXPECT_TRUE(SetFileAttributesW(path.c_str(), attributes));
  EXPECT_EQ(1, std::distance(fs::directory_iterator(folder), fs::directory_iterator()));
}
#endif
