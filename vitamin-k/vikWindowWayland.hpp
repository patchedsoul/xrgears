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

  static vik::Input::Key wayland_to_vik_key(uint32_t key) {
    switch (key) {
      case KEY_W:
        return vik::Input::Key::W;
      case KEY_S:
        return vik::Input::Key::S;
      case KEY_A:
        return vik::Input::Key::A;
      case KEY_D:
        return vik::Input::Key::D;
      case KEY_P:
        return vik::Input::Key::P;
      case KEY_F1:
        return vik::Input::Key::F1;
      case KEY_ESC:
        return vik::Input::Key::ESCAPE;
    }
  }

  static vik::Input::MouseScrollAxis wayland_to_vik_axis(uint32_t axis) {
    switch (axis) {
      case REL_X:
        return vik::Input::MouseScrollAxis::X;
      case REL_Y:
        return vik::Input::MouseScrollAxis::Y;
    }
  }

  static vik::Input::MouseButton wayland_to_vik_button(uint32_t button) {
    switch (button) {
      case BTN_LEFT:
        return vik::Input::MouseButton::Left;
      case BTN_MIDDLE:
        return vik::Input::MouseButton::Middle;
      case BTN_RIGHT:
        return vik::Input::MouseButton::Right;
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

};
}
