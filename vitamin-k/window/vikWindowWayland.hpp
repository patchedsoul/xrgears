/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <linux/input.h>
#include <wayland-client.h>

#include <vector>
#include <string>
#include <map>
#include <utility>

#include "vikWindow.hpp"

#include "../render/vikSwapChainVK.hpp"

namespace vik {
class WindowWayland : public Window {
 protected:
  wl_display *display = nullptr;
  wl_compositor *compositor = nullptr;
  wl_keyboard *keyboard = nullptr;
  wl_pointer *pointer = nullptr;
  wl_seat *seat = nullptr;
  wl_surface *surface = nullptr;

  SwapChainVK swap_chain;

  explicit WindowWayland(Settings *s) : Window(s) {}

  struct Mode {
    std::pair<int, int> size;
    int refresh;
  };

  struct Display {
    wl_output* output;
    std::string make;
    std::string model;
    std::vector<Mode> modes;
    std::pair<int, int> physical_size_mm;
    std::pair<int, int> position;
  };

  std::vector<Display> displays;

  ~WindowWayland() {
    if (surface)
      wl_surface_destroy(surface);
    if (keyboard)
      wl_keyboard_destroy(keyboard);
    if (pointer)
      wl_pointer_destroy(pointer);
    if (seat)
      wl_seat_destroy(seat);
    if (compositor)
      wl_compositor_destroy(compositor);
    if (display)
      wl_display_disconnect(display);
  }

  void iterate() {
    flush();
    render_frame_cb();
  }

  void init_swap_chain(uint32_t width, uint32_t height) {
    VkResult err = create_surface(swap_chain.instance, &swap_chain.surface);
    vik_log_f_if(err != VK_SUCCESS, "Could not create surface!");
    swap_chain.set_settings(settings);
    swap_chain.select_surface_format();
    swap_chain.create(width, height);
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
  }

  static Input::Key wayland_to_vik_key(uint32_t key) {
    switch (key) {
      case KEY_W:
        return Input::Key::W;
      case KEY_S:
        return Input::Key::S;
      case KEY_A:
        return Input::Key::A;
      case KEY_D:
        return Input::Key::D;
      case KEY_P:
        return Input::Key::P;
      case KEY_F1:
        return Input::Key::F1;
      case KEY_ESC:
        return Input::Key::ESCAPE;
      case KEY_SPACE:
        return Input::Key::SPACE;
      case KEY_KPPLUS:
        return Input::Key::KPPLUS;
      case KEY_KPMINUS:
        return Input::Key::KPMINUS;
      default:
        return Input::Key::UNKNOWN;
    }
  }

  static Input::MouseScrollAxis wayland_to_vik_axis(uint32_t axis) {
    switch (axis) {
      case REL_X:
        return Input::MouseScrollAxis::X;
      case REL_Y:
        return Input::MouseScrollAxis::Y;
      default:
        return Input::MouseScrollAxis::X;
    }
  }

  static Input::MouseButton wayland_to_vik_button(uint32_t button) {
    switch (button) {
      case BTN_LEFT:
        return Input::MouseButton::Left;
      case BTN_MIDDLE:
        return Input::MouseButton::Middle;
      case BTN_RIGHT:
        return Input::MouseButton::Right;
      default:
        return Input::MouseButton::Left;
    }
  }

  const std::vector<const char*> required_extensions() {
    return {VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME };
  }

  const std::vector<const char*> required_device_extensions() {
    return {};
  }

  // needs initialized vulkan
  VkBool32 check_support(VkPhysicalDevice physical_device) {
    return vkGetPhysicalDeviceWaylandPresentationSupportKHR(
          physical_device, 0, display);
  }

  VkResult create_surface(VkInstance instance, VkSurfaceKHR *vk_surface) {
    VkWaylandSurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    surface_info.display = display;
    surface_info.surface = surface;
    return vkCreateWaylandSurfaceKHR(instance, &surface_info, NULL, vk_surface);
  }

  void seat_capabilities(wl_seat *seat, uint32_t caps) {
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !pointer) {
      pointer = wl_seat_get_pointer(seat);
      wl_pointer_add_listener(pointer, &pointer_listener, this);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && pointer) {
      wl_pointer_destroy(pointer);
      pointer = nullptr;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
      keyboard = wl_seat_get_keyboard(seat);
      wl_keyboard_add_listener(keyboard, &keyboard_listener, this);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard) {
      wl_keyboard_destroy(keyboard);
      keyboard = nullptr;
    }
  }

  void flush() {
    while (wl_display_prepare_read(display) != 0)
      wl_display_dispatch_pending(display);
    if (wl_display_flush(display) < 0 && errno != EAGAIN) {
      wl_display_cancel_read(display);
      return;
    }

    pollfd fds[] = {{ wl_display_get_fd(display), POLLIN }, };
    if (poll(fds, 1, 0) > 0) {
      wl_display_read_events(display);
      wl_display_dispatch_pending(display);
    } else {
      wl_display_cancel_read(display);
    }
  }

  void output_mode(wl_output *output, unsigned int flags,
                   int w, int h, int refresh) {
    Mode m = {};
    m.size = {w, h};
    m.refresh = refresh;

    Display *d = get_display_from_output(output);
    vik_log_f_if(d == nullptr, "Output mode callback before geomentry!");

    d->modes.push_back(m);
  }

  virtual void registry_global(wl_registry *registry, uint32_t name,
                               const char *interface) = 0;

  // listeners
  const wl_registry_listener registry_listener = {
    _registry_global_cb,
    _registry_global_remove_cb
  };

  const wl_seat_listener seat_listener = {
    _seat_capabilities_cb,
    _seat_name_cb
  };

  const wl_output_listener output_listener = {
    _output_geometry_cb,
    _output_mode_cb,
    _output_done_cb,
    _output_scale_cb
  };

  const wl_pointer_listener pointer_listener = {
    _pointer_enter_cb,
    _pointer_leave_cb,
    _pointer_motion_cb,
    _pointer_button_cb,
    _pointer_axis_cb,
  };

  const struct wl_keyboard_listener keyboard_listener = {
    .keymap = _keyboard_keymap_cb,
    .enter = _keyboard_enter_cb,
    .leave = _keyboard_leave_cb,
    .key = _keyboard_key_cb,
    .modifiers = _keyboard_modifiers_cb,
    .repeat_info = _keyboard_repeat_cb,
  };

  // callback wrappers
  static void _keyboard_key_cb(void *data, wl_keyboard *keyboard,
                               uint32_t serial, uint32_t time, uint32_t key,
                               uint32_t state) {
    Window *self = reinterpret_cast<Window *>(data);
    self->keyboard_key_cb(wayland_to_vik_key(key), state);
  }

  static void _pointer_motion_cb(void *data, wl_pointer *pointer, uint32_t time,
                                 wl_fixed_t x, wl_fixed_t y) {
    Window *self = reinterpret_cast<Window *>(data);
    self->pointer_motion_cb(wl_fixed_to_double(x), wl_fixed_to_double(y));
  }

  static void _pointer_button_cb(void *data, wl_pointer *pointer,
                                 uint32_t serial, uint32_t time,
                                 uint32_t button, uint32_t state) {
    Window *self = reinterpret_cast<Window*>(data);
    self->pointer_button_cb(wayland_to_vik_button(button), state);
  }

  static void _pointer_axis_cb(void *data, wl_pointer *pointer, uint32_t time,
                               uint32_t axis, wl_fixed_t value) {
    Window *self = reinterpret_cast<Window*>(data);
    self->pointer_axis_cb(wayland_to_vik_axis(axis), wl_fixed_to_double(value));
  }

  static void _registry_global_cb(void *data, wl_registry *registry,
                                  uint32_t name, const char *interface,
                                  uint32_t version) {
    WindowWayland *self = reinterpret_cast<WindowWayland *>(data);
    // vik_log_d("Interface: %s Version %d", interface, version);
    self->registry_global(registry, name, interface);
  }

  static void _seat_capabilities_cb(void *data, wl_seat *seat, uint32_t caps) {
    WindowWayland *self = reinterpret_cast<WindowWayland *>(data);
    self->seat_capabilities(seat, caps);
  }

  static void _output_mode_cb(void *data, wl_output *output,
                              unsigned int flags, int w, int h, int refresh) {
    WindowWayland *self = reinterpret_cast<WindowWayland *>(data);
    self->output_mode(output, flags, w, h, refresh);
  }

  static void _output_geometry_cb(void *data, wl_output *output,
                                  int x, int y, int w, int h,
                                  int subpixel,
                                  const char *make, const char *model,
                                  int transform) {
    Display d = {};
    d.output = output;
    d.make = std::string(make);
    d.model = std::string(model);
    d.physical_size_mm = {w, h};
    d.position = {x, y};

    WindowWayland *self = reinterpret_cast<WindowWayland *>(data);
    self->displays.push_back(d);
  }

  Display* get_display_from_output(wl_output* output) {
    for (int i = 0; i < (int) displays.size(); i++) {
      if (displays[i].output == output)
        return &displays[i];
    }
    return nullptr;
  }

  void print_displays() {
    int i_d = 0;
    vik_log_i_short("Available displays:");
    for (auto d : displays) {
      vik_log_i_short("%d: %s %s [%d, %d] %dx%dmm (%d Modes)",
                i_d,
                d.make.c_str(),
                d.model.c_str(),
                d.position.first, d.position.second,
                d.physical_size_mm.first, d.physical_size_mm.second,
                d.modes.size());

      int i_m = 0;
      for (auto m : d.modes) {
        vik_log_i_short("\t%d: %s", i_m, mode_to_string(m).c_str());
        i_m++;
      }
      i_d++;
    }
  }

  Display* current_display() {
    return &displays[settings->display];
  }

  Mode* current_mode() {
    return &current_display()->modes[settings->mode];
  }

  static std::string mode_to_string(const Mode& m) {
    auto size = std::snprintf(nullptr, 0, "%d x %d @ %.2fHz",
                              m.size.first,
                              m.size.second,
                              (float) m.refresh/1000.0);
    std::string output(size + 1, '\0');
    std::snprintf(&output[0], size, "%d x %d @ %.2fHz",
        m.size.first,
        m.size.second,
        (float) m.refresh/1000.0);
    return std::string(output);
  }

  virtual void fullscreen() = 0;

  bool is_configured = false;

  void validate_display() {
    Display *d;
    if (settings->display > (int) displays.size()) {
      vik_log_e("Requested display %d, but only %d displays are available.",
                settings->display, displays.size());

      settings->display = 0;
      d = current_display();
      vik_log_e("Selecting '%s %s' instead.",
                d->make.c_str(),
                d->model.c_str());
    }
  }

  void validate_mode() {
    Display* d = current_display();

    if (settings->mode > (int) d->modes.size()) {
      vik_log_e("Requested mode %d, but only %d modes"
                " are available on display %d.",
                settings->mode,
                d->modes.size(),
                settings->display);
      settings->mode = 0;
      vik_log_e("Selecting '%s' instead",
                mode_to_string(*current_mode()).c_str());
    }
  }

  bool first_configure = true;
  bool fullscreen_requested = false;

  void configure(int32_t width, int32_t height) {
    if (settings->list_screens_and_exit) {
      print_displays();
      quit_cb();
      return;
    }

    // vik_log_d("configure: %dx%d", width, height);

    if (first_configure) {
      validate_display();
      validate_mode();
      first_configure = false;
    }

    Mode *m = current_mode();
    if (fullscreen_requested &&
        (m->size.first != width || m->size.second != height)) {
      vik_log_w("Received mode %dx%d does not match requested Mode %dx%d. "
                "Compositor bug? Requesting again.",
                width, height,
                m->size.first, m->size.second);
      fullscreen_requested = false;
    }

    m = current_mode();
    if (settings->fullscreen && !fullscreen_requested) {
      vik_log_i("Setting full screen on Display %d Mode %s",
                settings->display, mode_to_string(*m).c_str());
      fullscreen();
      fullscreen_requested = true;
      dimension_cb(m->size.first, m->size.second);
    }
  }

  // Unused callbacks

  static void _output_done_cb(void *data, wl_output *output) {}

  static void _output_scale_cb(void *data, wl_output *output, int scale) {}

  static void _registry_global_remove_cb(void *data, wl_registry *registry,
                                         uint32_t name) {}
  static void _keyboard_keymap_cb(void *data, wl_keyboard *keyboard,
                                  uint32_t format, int fd, uint32_t size) {}
  static void _keyboard_modifiers_cb(void *data, wl_keyboard *keyboard,
                                     uint32_t serial, uint32_t mods_depressed,
                                     uint32_t mods_latched,
                                     uint32_t mods_locked, uint32_t group) {}
  static void _keyboard_repeat_cb(void *data, wl_keyboard *keyboard,
                                  int32_t rate, int32_t delay) {}

  static void _keyboard_enter_cb(void *data,
                                 wl_keyboard *keyboard, uint32_t serial,
                                 wl_surface *surface, wl_array *keys) {}
  static void _keyboard_leave_cb(void *data,
                                 wl_keyboard *keyboard, uint32_t serial,
                                 wl_surface *surface) {}
  static void _pointer_enter_cb(void *data, wl_pointer *pointer,
                                uint32_t serial, wl_surface *surface,
                                wl_fixed_t sx, wl_fixed_t sy) {}
  static void _pointer_leave_cb(void *data, wl_pointer *pointer,
                                uint32_t serial, wl_surface *surface) {}
  static void _seat_name_cb (void *data, wl_seat *wl_seat, const char *name) {}
};
}  // namespace vik
