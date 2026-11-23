// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace vrowser {

// Evaluated by the renderer in the hinted frame. Returns a controller object
// that only the renderer holds; nothing is added to `window`, so page script
// cannot reach the element list. The labels live in a closed shadow root.
//
// The page runs in the same JavaScript world and can replace the DOM methods
// used here, so it can hide its own elements from hints or move their labels.
// It cannot make a key reach the page or run code in the browser process:
// keys are consumed by the browser process, and only element positions come
// back.
//
//   rects        [x, y, width, height, ...] in CSS pixels of the viewport
//   show(labels) draw one label per rect
//   filter(text) hide labels that do not start with `text`
//   activate(i)  remove the labels, then focus or click element `i`
//   clear()      remove the labels
inline constexpr char kHintControllerScript[] = R"JS((() => {
  'use strict';
  const SELECTOR = [
    'a[href]', 'area[href]', 'button', 'input:not([type="hidden"])',
    'select', 'textarea', 'summary', '[role="button"]', '[role="link"]',
    '[role="checkbox"]', '[role="radio"]', '[role="switch"]', '[role="tab"]',
    '[role="menuitem"]', '[role="option"]', '[onclick]',
    '[contenteditable=""]', '[contenteditable="true"]',
    '[tabindex]:not([tabindex="-1"])',
  ].join(',');
  const MAX_HINTS = 1000;
  const doc = document;
  const width = doc.documentElement.clientWidth || window.innerWidth;
  const height = doc.documentElement.clientHeight || window.innerHeight;

  const visibleBox = (el) => {
    for (const r of el.getClientRects()) {
      if (r.width >= 1 && r.height >= 1 && r.bottom > 0 && r.right > 0 &&
          r.top < height && r.left < width) {
        return r;
      }
    }
    return null;
  };
  // The element, or something inside it, is what a click there would hit.
  const onTop = (el, box) => {
    const x = Math.min(Math.max(box.left + box.width / 2, 0), width - 1);
    const y = Math.min(Math.max(box.top + box.height / 2, 0), height - 1);
    const hit = doc.elementFromPoint(x, y);
    return !hit || hit === el || el.contains(hit) || hit.contains(el);
  };

  const found = [];
  for (const el of doc.querySelectorAll(SELECTOR)) {
    if (found.length >= MAX_HINTS) break;
    if (el.disabled || el.closest('[inert]')) continue;
    const box = visibleBox(el);
    if (!box) continue;
    const style = getComputedStyle(el);
    if (style.visibility !== 'visible' || style.pointerEvents === 'none') {
      continue;
    }
    if (!onTop(el, box)) continue;
    found.push({el, box});
  }

  let host = null;
  let marks = [];
  const clear = () => {
    if (host) host.remove();
    host = null;
    marks = [];
  };
  const isEditable = (el) =>
      el.isContentEditable || el.localName === 'textarea' ||
      el.localName === 'select' ||
      (el.localName === 'input' &&
       !/^(button|submit|reset|checkbox|radio|file|image|color|range)$/i
            .test(el.type));

  return {
    rects: found.flatMap(({box}) => [box.left, box.top, box.width, box.height]),
    show(labels) {
      clear();
      host = doc.createElement('vrowser-hints');
      host.style.cssText = 'all: initial !important';
      const root = host.attachShadow({mode: 'closed'});
      const style = doc.createElement('style');
      style.textContent = `
        div { position: fixed; z-index: 2147483647; pointer-events: none;
              font: bold 11px/1.2 ui-monospace, Menlo, monospace;
              padding: 0 3px; border: 1px solid #8a6d00; border-radius: 3px;
              background: #ffd54a; color: #1a1a1a; text-transform: uppercase;
              box-shadow: 0 1px 2px rgba(0,0,0,.35); white-space: nowrap; }
        div[hidden] { display: none; }
        span { opacity: .45; }`;
      root.append(style);
      marks = found.map(({box}, i) => {
        const mark = doc.createElement('div');
        mark.style.left = `${Math.max(0, box.left)}px`;
        mark.style.top = `${Math.max(0, box.top)}px`;
        mark.dataset.label = String(labels[i] ?? '');
        mark.textContent = mark.dataset.label;
        root.append(mark);
        return mark;
      });
      (doc.documentElement || doc.body).append(host);
    },
    filter(typed) {
      for (const mark of marks) {
        const label = mark.dataset.label;
        mark.hidden = !label.startsWith(typed);
        mark.textContent = '';
        if (typed && !mark.hidden) {
          const done = doc.createElement('span');
          done.textContent = typed;
          mark.append(done, label.slice(typed.length));
        } else {
          mark.textContent = label;
        }
      }
    },
    activate(i) {
      clear();
      const target = found[i] && found[i].el;
      if (!target || !target.isConnected) return false;
      if (isEditable(target)) {
        target.focus();
      } else {
        target.click();
      }
      return true;
    },
    clear,
  };
})())JS";

}  // namespace vrowser
