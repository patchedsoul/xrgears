#pragma once

#include "vikWindow.hpp"

#include <linux/input.h>
#include <wayland-client.h>

namespace vik {
class WindowWayland : public Window {
 protected:
  wl_display *display = nullptr;
  wl_compositor *compositor = nullptr;
  wl_keyboard *keyboard = nullptr;
  wl_pointer *pointer = nullptr;
  wl_seat *seat = nullptr;
  wl_surface *surface = nullptr;

  int hmd_refresh = 0;

  wl_output *hmd_output = nullptr;

  WindowWayland(Settings *s) : Window(s) {
  }

  ~WindowWayland() {
    wl_surface_destroy(surface);
    if (keyboard)
      wl_keyboard_destroy(keyboard);
    if (pointer)
      wl_pointer_destroy(pointer);
    wl_seat_destroy(seat);

    wl_compositor_destroy(compositor);
    wl_display_disconnect(display);
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
    }
  }

  static Input::MouseScrollAxis wayland_to_vik_axis(uint32_t axis) {
    switch (axis) {
      case REL_X:
        return Input::MouseScrollAxis::X;
      case REL_Y:
        return Input::MouseScrollAxis::Y;
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
    }
  }

  const std::vector<const char*> required_extensions() {
    return {VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME };
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

    pollfd fds[] = {{ wl_display_get_fd(display), POLLIN },};
    if (poll(fds, 1, 0) > 0) {
      wl_display_read_events(display);
      wl_display_dispatch_pending(display);
    } else {
      wl_display_cancel_read(display);
    }
  }

  virtual void output_mode(wl_output *wl_output, unsigned int flags,
                           int w, int h, int refresh) = 0;

  virtual void registry_global(wl_registry *registry, uint32_t name,
                               const char *interface) = 0;

  // listeners
  const wl_registry_listener registry_listener = {
    _registry_global_cb,
    _registry_global_remove_cb
  };

  const wl_seat_listener seat_listener = {
    _seat_capabilities_cb,
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

  static void _registry_global_cb(void *data, wl_registry *registry, uint32_t name,
                                  const char *interface, uint32_t version) {
    WindowWayland *self = reinterpret_cast<WindowWayland *>(data);
    self->registry_global(registry, name, interface);
  }

  static void _seat_capabilities_cb(void *data, wl_seat *seat, uint32_t caps) {
    WindowWayland *self = reinterpret_cast<WindowWayland *>(data);
    self->seat_capabilities(seat, caps);
  }

  static void _output_mode_cb(void *data, wl_output *wl_output,
                              unsigned int flags, int w, int h, int refresh) {
    WindowWayland *self = reinterpret_cast<WindowWayland *>(data);
    self->output_mode(wl_output, flags, w, h, refresh);
  }

  // debug callbacks
  static void _output_done_cb(void *data, wl_output *output) {
    vik_log_d("output done %p", output);
  }

  static void _output_scale_cb(void *data, wl_output *output, int scale) {
    vik_log_d("output scale: %d", scale);
  }

  static void _output_geometry_cb(void *data, wl_output *wl_output, int x,
                                  int y, int w, int h, int subpixel,
                                  const char *make, const char *model,
                                  int transform) {
    vik_log_i("%s: %s [%d, %d] %dx%d", make, model, x, y, w, h);
  }

  // Unused callbacks
  static void _registry_global_remove_cb(void *data, wl_registry *registry,
                                         uint32_t name) {}
  static void _keyboard_keymap_cb(void *data, wl_keyboard *keyboard,
                                  uint32_t format, int fd, uint32_t size) {}
  static void _keyboard_modifiers_cb(void *data, wl_keyboard *keyboard,
                                     uint32_t serial, uint32_t mods_depressed,
                                     uint32_t mods_latched, uint32_t mods_locked,
                                     uint32_t group) {}
  static void _keyboard_repeat_cb(void *data, wl_keyboard *wl_keyboard,
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
};
}
