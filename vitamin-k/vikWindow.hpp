#pragma once

#include <stdio.h>
#include <string.h>

#include "vikSwapChain.hpp"

namespace vik {
class Window {
public:

  SwapChain *swap_chain;

  enum window_type {
    AUTO = 0,
    KMS,
    XCB_SIMPLE,
    XCB_MOUSE,
    WAYLAND_XDG,
    WAYLAND_LEGACY,
    KHR_DISPLAY,
    INVALID
  };

  Window() {}
  ~Window() {}

  static inline bool streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
  }

  static window_type window_type_from_string(const char *s) {
    if (streq(s, "auto"))
      return AUTO;
    else if (streq(s, "kms"))
      return KMS;
    else if (streq(s, "xcb"))
      return XCB_SIMPLE;
    else if (streq(s, "wayland"))
      return WAYLAND_XDG;
    else if (streq(s, "xcb-input"))
      return XCB_MOUSE;
    else if (streq(s, "wayland-legacy"))
      return WAYLAND_LEGACY;
    else if (streq(s, "khr-display"))
      return KHR_DISPLAY;
    else
      return INVALID;
  }

};
}
