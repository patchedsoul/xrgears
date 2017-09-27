#pragma once

#include "vikWindow.hpp"

#include <xcb/xcb.h>
#include <X11/keysym.h>
#include <xcb/xcb_keysyms.h>

#include <string>

namespace vik {
class WindowXCB : public Window {
protected:
  xcb_connection_t *connection;
  xcb_window_t window = XCB_NONE;
  xcb_visualid_t root_visual;
  xcb_key_symbols_t *syms;

  WindowXCB(Settings *s) : Window(s) {
  }

  ~WindowXCB() {
  }

  static Input::MouseButton xcb_to_vik_button(xcb_button_t button) {
    switch (button) {
      case XCB_BUTTON_INDEX_1:
        return Input::MouseButton::Left;
      case XCB_BUTTON_INDEX_2:
        return Input::MouseButton::Middle;
      case XCB_BUTTON_INDEX_3:
        return Input::MouseButton::Right;
    }
  }

  Input::Key xcb_to_vik_key(xcb_keycode_t key) {

    xcb_keysym_t xcb_keyp = xcb_key_symbols_get_keysym(syms, key, 0);
    switch (xcb_keyp) {
      case XK_w:
        return Input::Key::W;
      case XK_s:
        return Input::Key::S;
      case XK_a:
        return Input::Key::A;
      case XK_d:
        return Input::Key::D;
      case XK_p:
        return Input::Key::P;
      case XK_F1:
        return Input::Key::F1;
      case XK_Escape:
        return Input::Key::ESCAPE;
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

  void update_window_title(const std::string& title) {
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                        window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        title.size(), title.c_str());
  }

};
}
