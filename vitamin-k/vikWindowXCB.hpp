#pragma once

#include "vikWindow.hpp"

#include <xcb/xcb.h>

#define XCB_KEY_ESCAPE 0x9
#define XCB_KEY_F1 0x43
#define XCB_KEY_W 0x19
#define XCB_KEY_A 0x26
#define XCB_KEY_S 0x27
#define XCB_KEY_D 0x28
#define XCB_KEY_P 0x21

namespace vik {
class WindowXCB : public Window {
protected:
  xcb_connection_t *connection;
  xcb_window_t window = XCB_NONE;
  xcb_visualid_t root_visual;

  static vik::Input::MouseButton xcb_to_vik_button(xcb_button_t button) {
    switch (button) {
      case XCB_BUTTON_INDEX_1:
        return vik::Input::MouseButton::Left;
      case XCB_BUTTON_INDEX_2:
        return vik::Input::MouseButton::Middle;
      case XCB_BUTTON_INDEX_3:
        return vik::Input::MouseButton::Right;
    }
  }

  static vik::Input::Key xcb_to_vik_key(xcb_keycode_t key) {
    switch (key) {
      case XCB_KEY_W:
        return vik::Input::Key::W;
      case XCB_KEY_S:
        return vik::Input::Key::S;
      case XCB_KEY_A:
        return vik::Input::Key::A;
      case XCB_KEY_D:
        return vik::Input::Key::D;
      case XCB_KEY_P:
        return vik::Input::Key::P;
      case XCB_KEY_F1:
        return vik::Input::Key::F1;
      case XCB_KEY_ESCAPE:
        return vik::Input::Key::ESCAPE;
    }
  }

  VkResult create_surface(VkInstance instance, VkSurfaceKHR *surface) {
    VkXcbSurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surface_info.connection = connection;
    surface_info.window = window;
    return vkCreateXcbSurfaceKHR(instance, &surface_info, NULL, surface);
  }

public:
  const std::vector<const char*> required_extensions() {
    return { VK_KHR_XCB_SURFACE_EXTENSION_NAME };
  }

  VkBool32 check_support(VkPhysicalDevice physical_device) {
    return vkGetPhysicalDeviceXcbPresentationSupportKHR(
          physical_device, 0, connection, root_visual);
  }

};
}
