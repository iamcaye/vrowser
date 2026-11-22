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

// Held-key scrolling. While a line or half-page key is held, the page scrolls
// at a steady speed from one requestAnimationFrame loop:
//
//   HeldScrollStartScript    starts the loop (replacing any running one).
//   HeldScrollKeepAliveScript pushes the loop's deadline out again.
//   HeldScrollStopScript     ends the loop.
//
// The loop also ends on its own kHeldScrollTimeoutMs after the last start or
// keep-alive, so a lost key-up or a window that loses focus cannot leave the
// page scrolling. Its state lives in one page-visible property; a page that
// changes it only affects its own scrolling.

// Line motions held down, in CSS pixels per second.
inline constexpr int kHeldScrollLineSpeed = 900;
// Half-page motions held down, in viewport heights per second.
inline constexpr int kHeldScrollPageSpeed = 2;
inline constexpr int kHeldScrollTimeoutMs = 600;

// True for the motions that keep going while held: line and half page.
bool IsContinuousMotion(ScrollMotion motion);
// Empty for a motion that is not continuous.
std::string HeldScrollStartScript(ScrollMotion motion);
std::string HeldScrollKeepAliveScript();
std::string HeldScrollStopScript();

}  // namespace vrowser
