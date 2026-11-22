// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace vrowser {

enum class ScrollMotion {
  kLineDown,
  kLineUp,
  kHalfPageDown,
  kHalfPageUp,
  kTop,
  kBottom,
};

// One `j` or `k` step, in CSS pixels.
inline constexpr int kScrollLineStep = 60;

// JavaScript that scrolls the document of the frame it runs in. Line and
// half-page motions move `count` steps; a count below 1 counts as 1. Top and
// bottom ignore the count. The script always scrolls instantly, so a page's
// `scroll-behavior: smooth` does not delay the next step.
//
// The script is built only from constants and `count`; no page or user text
// is placed in it.
std::string ScrollScript(ScrollMotion motion, int count);

}  // namespace vrowser
