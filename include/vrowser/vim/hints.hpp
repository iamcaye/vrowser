// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vrowser {

// Home-row keys, most comfortable first.
inline constexpr std::string_view kDefaultHintAlphabet = "asdfghjkl";

// A hintable element's box in CSS pixels relative to the frame's viewport, as
// the renderer reported it. Untrusted: the page can lie about its layout.
struct HintRect {
  double x = 0;
  double y = 0;
  double width = 0;
  double height = 0;

  bool operator==(const HintRect&) const = default;
};

// One label per rect, in the order of `rects`. No label is a prefix of
// another, so typing a whole label is never ambiguous, and labels are as
// short as the count allows. Shorter labels go to rects earlier in reading
// order (top to bottom, then left to right). `alphabet` needs two or more
// distinct lowercase characters; otherwise every label is empty.
std::vector<std::string> AssignHintLabels(
    const std::vector<HintRect>& rects,
    std::string_view alphabet = kDefaultHintAlphabet);

enum class HintStatus {
  kPending,  // The typed text is a prefix of one or more labels.
  kMatched,  // The typed text is a whole label; see `index`.
  kNoMatch,  // No label starts with the text. The last key was dropped.
};

struct HintStep {
  HintStatus status = HintStatus::kPending;
  // Set for kMatched.
  std::optional<size_t> index;
};

// Labels on screen and what has been typed toward one.
class HintSession {
 public:
  explicit HintSession(std::vector<std::string> labels)
      : labels_(std::move(labels)) {}

  // A key typed in Hint mode: one character of the alphabet, or "Backspace".
  // Letters are matched without case. Other keys change nothing and return
  // kNoMatch.
  HintStep Type(std::string_view key);

  [[nodiscard]] const std::string& typed() const { return typed_; }
  [[nodiscard]] const std::vector<std::string>& labels() const {
    return labels_;
  }

 private:
  std::vector<std::string> labels_;
  std::string typed_;
};

}  // namespace vrowser
