/*
 * XRGears
 *
 * Copyright (C) 2016 Sascha Willems - www.saschawillems.de
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#include <xcb/xcb.h>
#include <vulkan/vulkan.h>

#include <string>

#include "../vks/vksLog.hpp"
#include "../vks/vksSwapChain.hpp"
#include "vikWindowXCB.hpp"

#define XCB_KEY_ESCAPE 0x9
#define XCB_KEY_F1 0x43
#define XCB_KEY_W 0x19
#define XCB_KEY_A 0x26
#define XCB_KEY_S 0x27
#define XCB_KEY_D 0x28
#define XCB_KEY_P 0x21

namespace vik {
class WindowXCBInput : public WindowXCB {
  xcb_connection_t *connection;
  xcb_screen_t *screen;
  xcb_window_t window;
  xcb_intern_atom_reply_t *atom_wm_delete_window;
  xcb_visualid_t root_visual;

 public:
  explicit WindowXCBInput() {
    const xcb_setup_t *setup;
    xcb_screen_iterator_t iter;
    int scr;

    connection = xcb_connect(NULL, &scr);
    if (connection == NULL) {
      printf("Failed to create XCB connection\n");
      fflush(stdout);
      exit(1);
    }

    setup = xcb_get_setup(connection);
    iter = xcb_setup_roots_iterator(setup);
    while (scr-- > 0)
      xcb_screen_next(&iter);
    screen = iter.data;
    root_visual = iter.data->root_visual;
  }

  ~WindowXCBInput() {
    xcb_destroy_window(connection, window);
    xcb_disconnect(connection);
  }

  void init_swap_chain(vik::Renderer *r) {
    VkResult err = VK_SUCCESS;

    r->swap_chain = new vks::SwapChain();

    vks::SwapChain *sc = (vks::SwapChain*) r->swap_chain;

    sc->set_context(r->instance, r->physical_device, r->device);

    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.connection = connection;
    surfaceCreateInfo.window = window;
    err = vkCreateXcbSurfaceKHR(r->instance, &surfaceCreateInfo, nullptr, &sc->surface);

    vik_log_f_if(err != VK_SUCCESS, "Could not create surface!");

    sc->select_queue_and_format();
  }

  void iterate(vik::Renderer *r) {
    xcb_generic_event_t *event;
    //xcb_flush(connection);
    while ((event = xcb_poll_for_event(connection))) {
      handle_event(event);
      free(event);
    }
  }

  void update_window_title(const std::string& title) {
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                        window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        title.size(), title.c_str());
  }

  static inline xcb_intern_atom_reply_t* intern_atom_helper(xcb_connection_t *conn, bool only_if_exists, const char *str) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, only_if_exists, strlen(str), str);
    return xcb_intern_atom_reply(conn, cookie, NULL);
  }

  // Set up a window using XCB and request event types
  int init(vik::Renderer *r) {
    uint32_t value_mask, value_list[32];

    if (connection == nullptr)
      printf("the connection is null!\n");

    window = xcb_generate_id(connection);

    value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    value_list[0] = screen->black_pixel;
    value_list[1] =
        XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE;

    if (r->settings->fullscreen)
      dimension_cb(screen->width_in_pixels, screen->height_in_pixels);

    xcb_create_window(connection,
                      XCB_COPY_FROM_PARENT,
                      window, screen->root,
                      0, 0, screen->width_in_pixels, screen->height_in_pixels, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      value_mask, value_list);

    /* Magic code that will send notification when window is destroyed */
    xcb_intern_atom_reply_t* reply = intern_atom_helper(connection, true, "WM_PROTOCOLS");
    atom_wm_delete_window = intern_atom_helper(connection, false, "WM_DELETE_WINDOW");

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                        window, (*reply).atom, 4, 32, 1,
                        &(*atom_wm_delete_window).atom);

    free(reply);

    if (r->settings->fullscreen) {
      xcb_intern_atom_reply_t *atom_wm_state = intern_atom_helper(connection, false, "_NET_WM_STATE");
      xcb_intern_atom_reply_t *atom_wm_fullscreen = intern_atom_helper(connection, false, "_NET_WM_STATE_FULLSCREEN");
      xcb_change_property(connection,
                          XCB_PROP_MODE_REPLACE,
                          window, atom_wm_state->atom,
                          XCB_ATOM_ATOM, 32, 1,
                          &(atom_wm_fullscreen->atom));
      free(atom_wm_fullscreen);
      free(atom_wm_state);
    }

    xcb_map_window(connection, window);

    return 0;
  }

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

  void handle_event(const xcb_generic_event_t *event) {
    switch (event->response_type & 0x7f) {
      case XCB_CLIENT_MESSAGE:
        if ((*(xcb_client_message_event_t*)event).data.data32[0] == (*atom_wm_delete_window).atom)
          quit_cb();
        break;
      case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;
        pointer_motion_cb((float)motion->event_x, (float)motion->event_y);
      }
        break;
      case XCB_BUTTON_PRESS: {
        xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
        pointer_button_cb(xcb_to_vik_button(press->detail), true);
      }
        break;
      case XCB_BUTTON_RELEASE: {
        xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
        pointer_button_cb(xcb_to_vik_button(press->detail), false);
      }
        break;
      case XCB_KEY_PRESS: {
        const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
        keyboard_key_cb(xcb_to_vik_key(keyEvent->detail), true);
      }
        break;
      case XCB_KEY_RELEASE: {
        const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
        keyboard_key_cb(xcb_to_vik_key(keyEvent->detail), false);
      }
        break;
      case XCB_DESTROY_NOTIFY:
        quit_cb();
        break;
      case XCB_CONFIGURE_NOTIFY: {
        const xcb_configure_notify_event_t *cfgEvent = (const xcb_configure_notify_event_t *)event;
        configure_cb(cfgEvent->width, cfgEvent->height);
      }
        break;
      default:
        break;
    }
  }

  VkBool32 check_support(VkPhysicalDevice physical_device) {
    return vkGetPhysicalDeviceXcbPresentationSupportKHR(
          physical_device, 0, connection, root_visual);
  }

  const std::vector<const char*> required_extensions() {
    return {VK_KHR_XCB_SURFACE_EXTENSION_NAME };
  }
};
}