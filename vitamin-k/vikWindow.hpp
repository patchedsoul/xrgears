#pragma once

#include <stdio.h>
#include <string.h>

#include <functional>

#include "vikSwapChain.hpp"
#include "vikRenderer.hpp"
#include "vikInput.hpp"

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

  std::function<void(double x, double y)> pointer_motion_cb;
  std::function<void(vik::Input::MouseButton button, bool state)> pointer_button_cb;
  std::function<void(vik::Input::MouseScrollAxis axis, double value)> pointer_axis_cb;
  std::function<void(vik::Input::Key key, bool state)> keyboard_key_cb;

  std::function<void(uint32_t width, uint32_t height)> configure_cb;
  std::function<void(uint32_t width, uint32_t height)> dimension_cb;

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

  void set_pointer_motion_cb(std::function<void(double x, double y)> cb) {
    pointer_motion_cb = cb;
  }

  void set_pointer_button_cb(std::function<void(vik::Input::MouseButton button,
                                                bool state)> cb) {
    pointer_button_cb = cb;
  }

  void set_pointer_axis_cb(std::function<void(
                             vik::Input::MouseScrollAxis axis,
                             double value)> cb) {
    pointer_axis_cb = cb;
  }

  void set_keyboard_key_cb(std::function<void(
                             vik::Input::Key key, bool state)> cb) {
    keyboard_key_cb = cb;
  }

  void set_configure_cb(std::function<void(uint32_t width, uint32_t height)> cb) {
    configure_cb = cb;
  }

  void set_dimension_cb(std::function<void(uint32_t width, uint32_t height)> cb) {
    dimension_cb = cb;
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

  virtual int init(vik::Renderer *r) = 0;
  virtual void iterate(vik::Renderer *r) = 0;
  virtual const std::vector<const char*> required_extensions() = 0;
  virtual void init_swap_chain(Renderer *r) = 0;
  virtual void update_window_title(const std::string& title) = 0;
  virtual VkBool32 check_support(VkPhysicalDevice physical_device) = 0;
};
}
