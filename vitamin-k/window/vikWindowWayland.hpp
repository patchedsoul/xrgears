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
  wl_seat *seat = nullptr;
  wl_surface *surface = nullptr;

  int hmd_refresh = 0;

  wl_output *hmd_output = nullptr;

  WindowWayland(Settings *s) : Window(s) {
  }

  ~WindowWayland() {
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

  // Unused callbacks
  static void _keyboard_keymap_cb(void *data, wl_keyboard *keyboard,
                                  uint32_t format, int fd, uint32_t size) {}
  static void _keyboard_modifiers_cb(void *data, wl_keyboard *keyboard,
                                     uint32_t serial, uint32_t mods_depressed,
                                     uint32_t mods_latched, uint32_t mods_locked,
                                     uint32_t group) {}
  static void _registry_global_remove_cb(void *data, wl_registry *registry,
                                         uint32_t name) {}
  static void _keyboard_enter_cb(void *data,
                                 wl_keyboard *keyboard, uint32_t serial,
                                 wl_surface *surface, wl_array *keys) {}
  static void _keyboard_leave_cb(void *data,
                                 wl_keyboard *keyboard, uint32_t serial,
                                 wl_surface *surface) {}
};
}
