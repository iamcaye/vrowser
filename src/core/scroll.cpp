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

// Shared by animated steps and the held loop. `id` names the one loop allowed
// to run; `animating` and `target` describe a step animation in flight.
constexpr char kState[] = "window.__vrowserScroll";

std::string StateObject() {
  const std::string state = kState;
  return "const s = " + state + " || (" + state + " = {id: 0});";
}

// `target` is a JavaScript expression that may use `base` (where the page is,
// or where a running step animation is headed) and `max` (the bottom).
std::string AnimatedStep(const std::string& target, int duration_ms) {
  return "(() => { " + StateObject() +
         " const el = document.scrollingElement || document.documentElement;"
         " const max = Math.max(0, el.scrollHeight - window.innerHeight);"
         " const base = s.animating ? s.target : window.scrollY;"
         " const target = Math.min(max, Math.max(0, " +
         target +
         ")); const id = ++s.id;"
         " if (window.matchMedia('(prefers-reduced-motion: reduce)')"
         ".matches) { s.animating = false; window.scrollTo({top: target, "
         "behavior: 'instant'}); return; }"
         " s.target = target; s.animating = true;"
         " const from = window.scrollY; const start = performance.now();"
         " const step = (t) => { if (s.id !== id) { return; }"
         " const p = Math.min(1, Math.max(0, (t - start) / " +
         std::to_string(duration_ms) +
         ")); const e = 1 - Math.pow(1 - p, 3);"
         " window.scrollTo({top: from + (target - from) * e, behavior: "
         "'instant'}); if (p < 1) { requestAnimationFrame(step); } else { "
         "s.animating = false; } }; requestAnimationFrame(step); })();";
}

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
  // Each start takes a new id; a loop whose id is stale ends, including a
  // step animation. A frame gap is capped at 50 ms so a stalled tab does not
  // jump when it resumes.
  return "(() => { " + StateObject() +
         " const id = ++s.id; s.animating = false; s.until = " + Deadline() +
         "; const v = " + speed +
         "; let last = 0; const step = (t) => { if (s.id !== id || t > "
         "s.until) { return; } if (last) { window.scrollBy({top: v * "
         "Math.min(t - last, 50) / 1000, behavior: 'instant'}); } last = t; "
         "requestAnimationFrame(step); }; requestAnimationFrame(step); })();";
}

std::string HeldScrollKeepAliveScript() {
  return std::string("(() => { const s = ") + kState +
         "; if (s) { s.until = " + Deadline() + "; } })();";
}

std::string HeldScrollStopScript() {
  return std::string("(() => { const s = ") + kState +
         "; if (s) { ++s.id; s.animating = false; } })();";
}

std::string ScrollScript(ScrollMotion motion, int count, int duration_ms) {
  const int steps = std::max(1, count);
  if (duration_ms > 0) {
    const std::string n = std::to_string(steps);
    switch (motion) {
      case ScrollMotion::kLineDown:
        return AnimatedStep("base + " + std::to_string(steps * kScrollLineStep),
                            duration_ms);
      case ScrollMotion::kLineUp:
        return AnimatedStep("base - " + std::to_string(steps * kScrollLineStep),
                            duration_ms);
      case ScrollMotion::kHalfPageDown:
        return AnimatedStep("base + window.innerHeight / 2 * " + n,
                            duration_ms);
      case ScrollMotion::kHalfPageUp:
        return AnimatedStep("base - window.innerHeight / 2 * " + n,
                            duration_ms);
      case ScrollMotion::kTop:
        return AnimatedStep("0", duration_ms);
      case ScrollMotion::kBottom:
        return AnimatedStep("max", duration_ms);
    }
    return std::string();
  }
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
