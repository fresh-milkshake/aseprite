// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.
#define TEST_APP
#include "app/i18n/strings.h"
#include "tests/app_test.h"
#include <atomic>
#include <future>
#include <thread>

TEST(Strings, BackgroundCopiesSurviveLanguageChanges)
{
  auto* strings = app::Strings::instance();
  const auto previous = strings->currentLanguage();
  strings->setCurrentLanguage("en");
  const auto english = app::Strings::TranslateCopy("home_view.new_file");
  strings->setCurrentLanguage("ru");
  const auto russian = app::Strings::TranslateCopy("home_view.new_file");
  ASSERT_NE(english, russian);
  std::atomic<bool> running{ true }, invalid{ false };
  std::promise<void> ready;
  auto started = ready.get_future();
  std::thread reader([&] {
    // Keep an owned string alive across subsequent catalogue replacements.
    const auto retained = app::Strings::TranslateCopy("home_view.new_file");
    ready.set_value();
    while (running) {
      const auto value = app::Strings::TranslateCopy("home_view.new_file");
      if (value != english && value != russian)
        invalid = true;
    }
    if (retained != russian)
      invalid = true;
  });
  started.wait();
  for (int i = 0; i < 8; ++i)
    strings->setCurrentLanguage(i % 2 ? "ru" : "en");
  running = false;
  reader.join();
  EXPECT_FALSE(invalid.load());
  strings->setCurrentLanguage(previous);
}
