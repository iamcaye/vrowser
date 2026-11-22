// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/scroll.hpp"

#include <algorithm>
#include <string>

namespace vrowser {
namespace {

std::string ScrollBy(const std::string& top) {
  return "window.scrollBy({top: " + top + ", behavior: 'instant'});";
}

std::string ScrollTo(const std::string& top) {
  return "window.scrollTo({top: " + top + ", behavior: 'instant'});";
}

}  // namespace

std::string ScrollScript(ScrollMotion motion, int count) {
  const int steps = std::max(1, count);
  switch (motion) {
    case ScrollMotion::kLineDown:
      return ScrollBy(std::to_string(steps * kScrollLineStep));
    case ScrollMotion::kLineUp:
      return ScrollBy(std::to_string(-steps * kScrollLineStep));
    case ScrollMotion::kHalfPageDown:
      return ScrollBy("window.innerHeight / 2 * " + std::to_string(steps));
    case ScrollMotion::kHalfPageUp:
      return ScrollBy("-window.innerHeight / 2 * " + std::to_string(steps));
    case ScrollMotion::kTop:
      return ScrollTo("0");
    case ScrollMotion::kBottom:
      return ScrollTo(
          "(document.scrollingElement || document.documentElement)"
          ".scrollHeight");
  }
  return std::string();
}

}  // namespace vrowser
