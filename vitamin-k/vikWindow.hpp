#pragma once

#include <stdio.h>
#include <string.h>

#include <functional>

#include "vikSwapChain.hpp"
#include "vikRenderer.hpp"

namespace vik {
class Window {
public:

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

  std::string name;

  std::function<void()> init_cb;
  std::function<void()> update_cb;
  std::function<void()> quit_cb;

  Window() {}
  ~Window() {}

  void set_init_cb(std::function<void()> cb) {
    init_cb = cb;
  }

  void set_update_cb(std::function<void()> cb) {
    update_cb = cb;
  }

  void set_quit_cb(std::function<void()> cb) {
    quit_cb = cb;
  }

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

  virtual const std::vector<const char*> required_extensions() = 0;
  virtual void init_swap_chain(Renderer *r) = 0;
  virtual void update_window_title(const std::string& title) = 0;

};
}
