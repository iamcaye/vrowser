// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "vrowser/vim/key_bindings.hpp"

namespace vrowser {

// Built-in Normal-mode bindings:
//
//   :   command-line             open the command line (Command mode)
//   r   reload
//   j   scroll-down              one line step, times the count
//   k   scroll-up                one line step, times the count
//   d   scroll-half-page-down    half the viewport, times the count
//   u   scroll-half-page-up      half the viewport, times the count
//   gg  scroll-to-top
//   G   scroll-to-bottom
//   H   history-back             one entry back in the current tab
//   L   history-forward          one entry forward in the current tab
//   gt  tab-next                 count tabs right, wrapping
//   gT  tab-previous             count tabs left, wrapping
//   t   tab-new                  a new tab with the start page
//   x   tab-close                the current tab; the last one quits
//
// Returns false if any binding could not be added, for example because
// `bindings` already binds one of these sequences.
bool AddDefaultBindings(KeyBindings& bindings);

}  // namespace vrowser
