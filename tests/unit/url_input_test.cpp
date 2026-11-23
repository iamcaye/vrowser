// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/url_input.hpp"

#include <gtest/gtest.h>

#include <string>

namespace vrowser {
namespace {

void ExpectUrl(const std::string& text, const std::string& url) {
  const UrlInput result = ClassifyUrlInput(text);
  EXPECT_EQ(result.kind, UrlInputKind::kUrl) << text;
  EXPECT_EQ(result.url, url) << text;
  EXPECT_TRUE(result.error.empty()) << text;
}

void ExpectSearch(const std::string& text, const std::string& url) {
  const UrlInput result = ClassifyUrlInput(text);
  EXPECT_EQ(result.kind, UrlInputKind::kSearch) << text;
  EXPECT_EQ(result.url, url) << text;
  EXPECT_TRUE(result.error.empty()) << text;
}

void ExpectRejected(const std::string& text) {
  const UrlInput result = ClassifyUrlInput(text);
  EXPECT_EQ(result.kind, UrlInputKind::kRejected) << text;
  EXPECT_TRUE(result.url.empty()) << text;
  EXPECT_FALSE(result.error.empty()) << text;
}

TEST(UrlInputTest, HttpAndHttpsAreUsedAsTyped) {
  ExpectUrl("https://example.com", "https://example.com");
  ExpectUrl("http://example.com/a?b=c#d", "http://example.com/a?b=c#d");
  ExpectUrl("HTTPS://Example.com/", "HTTPS://Example.com/");
  ExpectUrl("  https://neovim.io/  ", "https://neovim.io/");
}

TEST(UrlInputTest, HostsGetHttps) {
  ExpectUrl("example.com", "https://example.com");
  ExpectUrl("www.example.co.uk/path?q=1", "https://www.example.co.uk/path?q=1");
  ExpectUrl("neovim.io#help", "https://neovim.io#help");
  ExpectUrl("Example.COM", "https://Example.COM");
  ExpectUrl("xn--mnchen-3ya.de", "https://xn--mnchen-3ya.de");
  ExpectUrl("münchen.de", "https://münchen.de");
}

TEST(UrlInputTest, HostsWithPortsGetHttps) {
  ExpectUrl("localhost", "https://localhost");
  ExpectUrl("localhost:8080", "https://localhost:8080");
  ExpectUrl("localhost:8080/api", "https://localhost:8080/api");
  ExpectUrl("example.com:443", "https://example.com:443");
  ExpectUrl("devbox:3000", "https://devbox:3000");
}

TEST(UrlInputTest, IpAddressesGetHttps) {
  ExpectUrl("127.0.0.1", "https://127.0.0.1");
  ExpectUrl("192.168.1.10:8000/x", "https://192.168.1.10:8000/x");
  ExpectUrl("[::1]", "https://[::1]");
  ExpectUrl("[2001:db8::1]:8443", "https://[2001:db8::1]:8443");
}

TEST(UrlInputTest, TextWithSpacesIsASearch) {
  ExpectSearch("vim motions", "https://duckduckgo.com/?q=vim+motions");
  ExpectSearch("  what is  example.com ",
               "https://duckduckgo.com/?q=what+is++example.com");
  ExpectSearch("javascript: the good parts",
               "https://duckduckgo.com/?q=javascript%3A+the+good+parts");
}

TEST(UrlInputTest, WordsAndNonHostsAreSearches) {
  ExpectSearch("neovim", "https://duckduckgo.com/?q=neovim");
  ExpectSearch("c++", "https://duckduckgo.com/?q=c%2B%2B");
  ExpectSearch("1.5", "https://duckduckgo.com/?q=1.5");
  ExpectSearch("256.1.1.1", "https://duckduckgo.com/?q=256.1.1.1");
  ExpectSearch("example..com", "https://duckduckgo.com/?q=example..com");
  ExpectSearch("-bad.com", "https://duckduckgo.com/?q=-bad.com");
  ExpectSearch("localhost:http", "https://duckduckgo.com/?q=localhost%3Ahttp");
  ExpectSearch("localhost:123456",
               "https://duckduckgo.com/?q=localhost%3A123456");
  ExpectSearch("user@example.com",
               "https://duckduckgo.com/?q=user%40example.com");
  ExpectSearch("a&b=c#d", "https://duckduckgo.com/?q=a%26b%3Dc%23d");
}

TEST(UrlInputTest, DangerousSchemesAreRejected) {
  ExpectRejected("javascript:alert(1)");
  ExpectRejected("JavaScript:alert(1)");
  ExpectRejected("  javascript:alert(1)");
  ExpectRejected("data:text/html,<script>alert(1)</script>");
  ExpectRejected("file:///etc/passwd");
  ExpectRejected("chrome://settings");
  ExpectRejected("chrome-extension://abc/page.html");
  ExpectRejected("devtools://devtools/bundled/inspector.html");
  ExpectRejected("view-source:https://example.com");
  ExpectRejected("about:config");
  ExpectRejected("blob:https://example.com/uuid");
  ExpectRejected("vbscript:msgbox");
  ExpectRejected("ftp://example.com/");
  ExpectRejected("ms-settings://x");
}

TEST(UrlInputTest, OtherSchemeLikeWordsAreSearchesNotLoads) {
  ExpectSearch("mailto:someone", "https://duckduckgo.com/?q=mailto%3Asomeone");
  ExpectSearch("define:modal", "https://duckduckgo.com/?q=define%3Amodal");
  ExpectSearch("http:example.com",
               "https://duckduckgo.com/?q=http%3Aexample.com");
}

TEST(UrlInputTest, HttpUrlsWithSpacesAreStillUrls) {
  ExpectUrl("https://example.com/a b", "https://example.com/a b");
}

TEST(UrlInputTest, RejectionNamesTheScheme) {
  EXPECT_EQ(ClassifyUrlInput("javascript:alert(1)").error,
            "Cannot open javascript: URLs");
  EXPECT_EQ(ClassifyUrlInput("FILE:///tmp").error, "Cannot open file: URLs");
}

TEST(UrlInputTest, AboutBlankIsAllowed) {
  ExpectUrl("about:blank", "about:blank");
}

TEST(UrlInputTest, EmptyInputIsEmpty) {
  EXPECT_EQ(ClassifyUrlInput("").kind, UrlInputKind::kEmpty);
  EXPECT_EQ(ClassifyUrlInput(" \t\n").kind, UrlInputKind::kEmpty);
  EXPECT_TRUE(ClassifyUrlInput("").url.empty());
}

TEST(UrlInputTest, SearchUrlEncodesTheQuery) {
  EXPECT_EQ(SearchUrl("vim motions"), "https://duckduckgo.com/?q=vim+motions");
  EXPECT_EQ(SearchUrl("a-b_c.d~e"), "https://duckduckgo.com/?q=a-b_c.d~e");
  EXPECT_EQ(SearchUrl("ñ"), "https://duckduckgo.com/?q=%C3%B1");
}

}  // namespace
}  // namespace vrowser
