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

// How long one scroll step animates. 0 scrolls instantly. Until the config
// module can hold a setting, this constant is the switch.
inline constexpr int kScrollAnimationMs = 120;

// JavaScript that scrolls the document of the frame it runs in. Line and
// half-page motions move `count` steps; a count below 1 counts as 1. Top and
// bottom ignore the count.
//
// With `duration_ms` above 0 the step eases out over that time from one
// requestAnimationFrame loop. A step that arrives while another is still
// animating adds to that step's target, so `jjj` ends where `3j` does. The
// target is clamped to the page, so steps past an edge do not pile up. When
// the page reports `prefers-reduced-motion: reduce`, the step is instant.
// With `duration_ms` of 0 the step is one instant scrollBy or scrollTo.
// Either way scrolling is done with `behavior: 'instant'`, so a page's
// `scroll-behavior: smooth` does not add its own animation.
//
// The script is built only from constants, `count`, and `duration_ms`; no
// page or user text is placed in it.
std::string ScrollScript(ScrollMotion motion,
                         int count,
                         int duration_ms = kScrollAnimationMs);

// Held-key scrolling. While a line or half-page key is held, the page scrolls
// at a steady speed from one requestAnimationFrame loop:
//
//   HeldScrollStartScript    starts the loop (replacing any running one).
//   HeldScrollKeepAliveScript pushes the loop's deadline out again.
//   HeldScrollStopScript     ends the loop.
//
// The loop also ends on its own kHeldScrollTimeoutMs after the last start or
// keep-alive, so a lost key-up or a window that loses focus cannot leave the
// page scrolling.
//
// Animated steps and the held loop share one page-visible state object, so a
// hold cancels a running step animation and a new step cancels a hold loop. A
// page that changes that object only affects its own scrolling.

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
