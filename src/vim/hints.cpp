// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/hints.hpp"

#include <algorithm>
#include <cctype>
#include <numeric>
#include <utility>

namespace vrowser {
namespace {

// `count` prefix-free labels, shortest first. Splits the first (shortest)
// label into one label per character until there are enough, as a k-ary tree
// is filled breadth-first.
std::vector<std::string> PrefixFreeLabels(size_t count,
                                          std::string_view alphabet) {
  std::vector<std::string> labels = {""};
  size_t next = 0;
  while (labels.size() - next < count || labels.size() == 1) {
    const std::string prefix = labels[next++];
    for (char c : alphabet) {
      labels.push_back(prefix + c);
    }
  }
  labels.erase(labels.begin(), labels.begin() + static_cast<long>(next));
  labels.resize(count);
  // Breadth-first order is already shortest first. Within one length, keep
  // the alphabet's order, which puts the home keys first.
  return labels;
}

bool HasDistinctCharacters(std::string_view alphabet) {
  if (alphabet.size() < 2) {
    return false;
  }
  std::string sorted(alphabet);
  std::sort(sorted.begin(), sorted.end());
  return std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end();
}

}  // namespace

std::vector<std::string> AssignHintLabels(const std::vector<HintRect>& rects,
                                          std::string_view alphabet) {
  std::vector<std::string> result(rects.size());
  if (rects.empty() || !HasDistinctCharacters(alphabet)) {
    return result;
  }
  std::vector<size_t> order(rects.size());
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&rects](size_t a, size_t b) {
    if (rects[a].y != rects[b].y) {
      return rects[a].y < rects[b].y;
    }
    return rects[a].x < rects[b].x;
  });
  std::vector<std::string> labels = PrefixFreeLabels(rects.size(), alphabet);
  for (size_t i = 0; i < order.size(); ++i) {
    result[order[i]] = std::move(labels[i]);
  }
  return result;
}

HintStep HintSession::Type(std::string_view key) {
  if (key == "Backspace") {
    if (!typed_.empty()) {
      typed_.pop_back();
    }
    return {};
  }
  if (key.size() != 1) {
    return {.status = HintStatus::kNoMatch, .index = std::nullopt};
  }
  const std::string candidate =
      typed_ +
      static_cast<char>(std::tolower(static_cast<unsigned char>(key[0])));
  bool prefix = false;
  for (size_t i = 0; i < labels_.size(); ++i) {
    if (labels_[i] == candidate) {
      typed_ = candidate;
      return {.status = HintStatus::kMatched, .index = i};
    }
    prefix = prefix || labels_[i].starts_with(candidate);
  }
  if (!prefix) {
    return {.status = HintStatus::kNoMatch, .index = std::nullopt};
  }
  typed_ = candidate;
  return {};
}

}  // namespace vrowser
