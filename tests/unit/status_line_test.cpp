// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/ui/status_line.hpp"

#include <gtest/gtest.h>

#include <string>

namespace vrowser {
namespace {

TEST(StatusLineTest, ModeLabels) {
  EXPECT_EQ(ModeLabel(Mode::kNormal), "NORMAL");
  EXPECT_EQ(ModeLabel(Mode::kInsert), "INSERT");
  EXPECT_EQ(ModeLabel(Mode::kCommand), "COMMAND");
  EXPECT_EQ(ModeLabel(Mode::kHint), "HINT");
}

TEST(StatusLineTest, PendingKeysShowCountThenKeys) {
  EXPECT_EQ(PendingKeys(KeySequenceState{}), "");
  EXPECT_EQ(PendingKeys(KeySequenceState{{}, 3}), "3");
  EXPECT_EQ(PendingKeys(KeySequenceState{{"g"}, 0}), "g");
  EXPECT_EQ(PendingKeys(KeySequenceState{{"g"}, 12}), "12g");
}

TEST(ShortenUrlTest, ShortUrlIsUnchanged) {
  EXPECT_EQ(ShortenUrl("https://example.com/", 80), "https://example.com/");
}

TEST(ShortenUrlTest, LongPathIsCutFromTheEnd) {
  EXPECT_EQ(ShortenUrl("https://example.com/a/very/long/path", 28),
            "https://example.com/a/ver...");
}

TEST(ShortenUrlTest, HostAndPortAreNeverCut) {
  const std::string url =
      "https://a-very-long-subdomain.example.com:8443/path?q=1";
  EXPECT_EQ(ShortenUrl(url, 10),
            "https://a-very-long-subdomain.example.com:8443...");
  EXPECT_EQ(ShortenUrl("https://a-very-long-subdomain.example.com", 10),
            "https://a-very-long-subdomain.example.com");
}

TEST(ShortenUrlTest, UserInfoIsDropped) {
  EXPECT_EQ(ShortenUrl("https://accounts.example@evil.test/login", 80),
            "https://evil.test/login");
  EXPECT_EQ(ShortenUrl("https://user:pass@host.test:81/", 80),
            "https://host.test:81/");
  // An '@' in the path is not user info.
  EXPECT_EQ(ShortenUrl("https://host.test/a@b", 80), "https://host.test/a@b");
}

TEST(ShortenUrlTest, PunycodeHostIsShownAsGiven) {
  EXPECT_EQ(ShortenUrl("https://xn--80ak6aa92e.com/", 80),
            "https://xn--80ak6aa92e.com/");
}

TEST(ShortenUrlTest, UrlsWithoutAuthorityKeepTheirScheme) {
  EXPECT_EQ(ShortenUrl("data:text/html;base64,PCFkb2N0eXBl", 20),
            "data:text/html;ba...");
  EXPECT_EQ(ShortenUrl("data:text/html", 2), "data:...");
  EXPECT_EQ(ShortenUrl("about:blank", 80), "about:blank");
}

TEST(FormatStatusLineTest, ShowsModeUrlAndLoading) {
  StatusLineInput input;
  input.url = "https://example.com/";
  EXPECT_EQ(FormatStatusLine(input, 80), "NORMAL  https://example.com/");

  input.mode = Mode::kInsert;
  input.loading = true;
  EXPECT_EQ(FormatStatusLine(input, 80),
            "INSERT  https://example.com/  loading");
}

TEST(FormatStatusLineTest, ShowsPendingKeys) {
  StatusLineInput input;
  input.url = "https://example.com/";
  input.pending = KeySequenceState{{"g"}, 3};
  EXPECT_EQ(FormatStatusLine(input, 80), "NORMAL  3g  https://example.com/");
}

TEST(FormatStatusLineTest, UrlTakesTheRemainingWidth) {
  StatusLineInput input;
  input.url = "https://example.com/a/very/long/path/to/a/page";
  const std::string line = FormatStatusLine(input, 40);
  EXPECT_EQ(line.size(), 40u);
  EXPECT_EQ(line, "NORMAL  https://example.com/a/very/lo...");
}

TEST(FormatStatusLineTest, NarrowWidthStillShowsTheHost) {
  StatusLineInput input;
  input.url = "https://example.com/path";
  input.loading = true;
  EXPECT_EQ(FormatStatusLine(input, 5),
            "NORMAL  https://example.com...  loading");
}

}  // namespace
}  // namespace vrowser
