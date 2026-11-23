// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/download_path.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <set>
#include <string>

namespace vrowser {
namespace {

namespace fs = std::filesystem;

TEST(SanitizeDownloadNameTest, KeepsAPlainName) {
  EXPECT_EQ(SanitizeDownloadName("report.pdf"), "report.pdf");
  EXPECT_EQ(SanitizeDownloadName("my file (2).tar.gz"), "my file (2).tar.gz");
  EXPECT_EQ(SanitizeDownloadName("r\xC3\xA9sum\xC3\xA9.txt"),
            "r\xC3\xA9sum\xC3\xA9.txt");
}

TEST(SanitizeDownloadNameTest, DropsDirectories) {
  EXPECT_EQ(SanitizeDownloadName("../../etc/passwd"), "passwd");
  EXPECT_EQ(SanitizeDownloadName("..\\..\\Windows\\evil.dll"), "evil.dll");
  EXPECT_EQ(SanitizeDownloadName("/tmp/x.sh"), "x.sh");
  EXPECT_EQ(SanitizeDownloadName("a/b\\c.txt"), "c.txt");
}

TEST(SanitizeDownloadNameTest, NeverNamesADirectory) {
  EXPECT_EQ(SanitizeDownloadName(".."), "download");
  EXPECT_EQ(SanitizeDownloadName("."), "download");
  EXPECT_EQ(SanitizeDownloadName("../"), "download");
  EXPECT_EQ(SanitizeDownloadName("foo/.."), "download");
  EXPECT_EQ(SanitizeDownloadName(""), "download");
  EXPECT_EQ(SanitizeDownloadName("   "), "download");
}

TEST(SanitizeDownloadNameTest, HiddenNamesAreMadeVisible) {
  EXPECT_EQ(SanitizeDownloadName(".bashrc"), "_bashrc");
  EXPECT_EQ(SanitizeDownloadName("...txt"), "_..txt");
}

TEST(SanitizeDownloadNameTest, ReplacesControlAndReservedCharacters) {
  EXPECT_EQ(SanitizeDownloadName("a\nb\tc\x7f.txt"), "a_b_c_.txt");
  EXPECT_EQ(SanitizeDownloadName("a<b>c:d\"e|f?g*.txt"), "a_b_c_d_e_f_g_.txt");
  EXPECT_EQ(SanitizeDownloadName(std::string("a\0b.txt", 7)), "a_b.txt");
}

TEST(SanitizeDownloadNameTest, TrimsDotsAndSpaces) {
  EXPECT_EQ(SanitizeDownloadName("  name.txt. . "), "name.txt");
}

TEST(SanitizeDownloadNameTest, PrefixesWindowsDeviceNames) {
  EXPECT_EQ(SanitizeDownloadName("CON"), "_CON");
  EXPECT_EQ(SanitizeDownloadName("nul.txt"), "_nul.txt");
  EXPECT_EQ(SanitizeDownloadName("com1.log"), "_com1.log");
  EXPECT_EQ(SanitizeDownloadName("LPT9"), "_LPT9");
  EXPECT_EQ(SanitizeDownloadName("console.txt"), "console.txt");
  EXPECT_EQ(SanitizeDownloadName("COM10"), "COM10");
}

TEST(SanitizeDownloadNameTest, CutsLongNamesAndKeepsTheExtension) {
  const std::string name = SanitizeDownloadName(std::string(300, 'a') + ".pdf");
  EXPECT_EQ(name.size(), 200U);
  EXPECT_TRUE(name.ends_with(".pdf"));
}

TEST(SanitizeDownloadNameTest, CutsOnAUtf8Boundary) {
  // 150 two-byte characters: 300 bytes.
  std::string text;
  for (int i = 0; i < 150; ++i) {
    text += "\xC3\xA9";
  }
  const std::string name = SanitizeDownloadName(text);
  EXPECT_LE(name.size(), 200U);
  EXPECT_EQ(name.size() % 2, 0U);
  EXPECT_EQ(name, text.substr(0, name.size()));
}

TEST(UniqueDownloadPathTest, UsesTheNameWhenFree) {
  const fs::path dir = fs::path("profile") / "Downloads";
  const std::optional<fs::path> path =
      UniqueDownloadPath(dir, "a.txt", [](const fs::path&) { return false; });
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ(*path, dir / "a.txt");
}

TEST(UniqueDownloadPathTest, NumbersATakenName) {
  const fs::path dir = fs::path("profile") / "Downloads";
  const std::set<fs::path> taken = {dir / "a.txt", dir / "a (1).txt"};
  const auto is_taken = [&](const fs::path& p) { return taken.contains(p); };

  EXPECT_EQ(UniqueDownloadPath(dir, "a.txt", is_taken), dir / "a (2).txt");
  EXPECT_EQ(UniqueDownloadPath(dir, "b", is_taken), dir / "b");
}

TEST(UniqueDownloadPathTest, NumbersANameWithoutExtension) {
  const fs::path dir = "Downloads";
  const auto is_taken = [&](const fs::path& p) { return p == dir / "notes"; };

  EXPECT_EQ(UniqueDownloadPath(dir, "notes", is_taken), dir / "notes (1)");
}

TEST(UniqueDownloadPathTest, GivesUpWhenEveryNameIsTaken) {
  EXPECT_FALSE(UniqueDownloadPath("Downloads", "a.txt", [](const fs::path&) {
                 return true;
               }).has_value());
}

TEST(UniqueDownloadPathTest, RefusesNamesThatLeaveTheDirectory) {
  const auto free = [](const fs::path&) { return false; };
  EXPECT_FALSE(UniqueDownloadPath("Downloads", "..", free).has_value());
  EXPECT_FALSE(UniqueDownloadPath("Downloads", "a/b", free).has_value());
  EXPECT_FALSE(UniqueDownloadPath("Downloads", "..\\b", free).has_value());
  EXPECT_FALSE(UniqueDownloadPath("Downloads", "", free).has_value());
}

TEST(UniqueDownloadPathTest, SanitizedNamesStayInTheDirectory) {
  const fs::path dir = fs::path("profile") / "Downloads";
  for (const char* suggested :
       {"../../x", "..", "/abs/path", "..\\..\\y", "a/../../b", "....//"}) {
    const std::optional<fs::path> path =
        UniqueDownloadPath(dir, SanitizeDownloadName(suggested),
                           [](const fs::path&) { return false; });
    ASSERT_TRUE(path.has_value()) << suggested;
    EXPECT_EQ(path->parent_path(), dir) << suggested;
    EXPECT_NE(path->filename(), "..") << suggested;
  }
}

TEST(IsInsideDirectoryTest, AcceptsEntriesBelowTheDirectory) {
  const fs::path dir = fs::path("/profile") / "Downloads";
  EXPECT_TRUE(IsInsideDirectory(dir, dir / "a.txt"));
  EXPECT_TRUE(IsInsideDirectory(dir, dir / "a.txt.crdownload"));
  EXPECT_TRUE(IsInsideDirectory(dir, dir / "sub" / "a.txt"));
}

TEST(IsInsideDirectoryTest, RejectsPathsOutsideTheDirectory) {
  const fs::path dir = fs::path("/profile") / "Downloads";
  EXPECT_FALSE(IsInsideDirectory(dir, dir));
  EXPECT_FALSE(IsInsideDirectory(dir, dir / ".." / "a.txt"));
  EXPECT_FALSE(IsInsideDirectory(dir, fs::path("/profile") / "a.txt"));
  EXPECT_FALSE(IsInsideDirectory(dir, fs::path("/profile") / "Downloads2"));
  EXPECT_FALSE(IsInsideDirectory(dir, fs::path("/tmp/a.txt")));
  EXPECT_FALSE(IsInsideDirectory(dir, fs::path()));
}

}  // namespace
}  // namespace vrowser
