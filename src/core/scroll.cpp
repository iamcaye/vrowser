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

constexpr char kHeldState[] = "window.__vrowserHeldScroll";

std::string Deadline() {
  return "performance.now() + " + std::to_string(kHeldScrollTimeoutMs);
}

}  // namespace

bool IsContinuousMotion(ScrollMotion motion) {
  switch (motion) {
    case ScrollMotion::kLineDown:
    case ScrollMotion::kLineUp:
    case ScrollMotion::kHalfPageDown:
    case ScrollMotion::kHalfPageUp:
      return true;
    case ScrollMotion::kTop:
    case ScrollMotion::kBottom:
      return false;
  }
  return false;
}

std::string HeldScrollStartScript(ScrollMotion motion) {
  std::string speed;
  switch (motion) {
    case ScrollMotion::kLineDown:
      speed = std::to_string(kHeldScrollLineSpeed);
      break;
    case ScrollMotion::kLineUp:
      speed = std::to_string(-kHeldScrollLineSpeed);
      break;
    case ScrollMotion::kHalfPageDown:
      speed = "window.innerHeight * " + std::to_string(kHeldScrollPageSpeed);
      break;
    case ScrollMotion::kHalfPageUp:
      speed = "-window.innerHeight * " + std::to_string(kHeldScrollPageSpeed);
      break;
    case ScrollMotion::kTop:
    case ScrollMotion::kBottom:
      return std::string();
  }
  const std::string state = kHeldState;
  // Each start takes a new id; a loop whose id is stale ends. A frame gap is
  // capped at 50 ms so a stalled tab does not jump when it resumes.
  return "(() => { const s = " + state + " || (" + state +
         " = {id: 0}); const id = ++s.id; s.until = " + Deadline() +
         "; const v = " + speed +
         "; let last = 0; const step = (t) => { if (s.id !== id || t > "
         "s.until) { return; } if (last) { window.scrollBy({top: v * "
         "Math.min(t - last, 50) / 1000, behavior: 'instant'}); } last = t; "
         "requestAnimationFrame(step); }; requestAnimationFrame(step); })();";
}

std::string HeldScrollKeepAliveScript() {
  return std::string("(() => { const s = ") + kHeldState +
         "; if (s) { s.until = " + Deadline() + "; } })();";
}

std::string HeldScrollStopScript() {
  return std::string("(() => { const s = ") + kHeldState +
         "; if (s) { ++s.id; } })();";
}

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
