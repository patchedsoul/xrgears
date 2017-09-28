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

#include <vulkan/vulkan.h>
#include <string>

#include "system/vikLog.hpp"
#include "render/vikSwapChainVKComplex.hpp"
#include "vikWindowXCB.hpp"

namespace vik {
class WindowXCBInput : public WindowXCB {

  SwapChainVkComplex swap_chain;

 public:
  explicit WindowXCBInput(Settings *s) : WindowXCB(s) {
    name = "xcb-input";
  }

  ~WindowXCBInput() {
    xcb_destroy_window(connection, window);
    xcb_disconnect(connection);
    xcb_key_symbols_free(syms);
  }

  // Set up a window using XCB and request event types
  int init(uint32_t width, uint32_t height) {
    xcb_screen_iterator_t iter;
    int scr;

    connection = xcb_connect(NULL, &scr);

    if (connection == NULL)
      return -1;

    iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
    while (scr-- > 0)
      xcb_screen_next(&iter);
    xcb_screen_t *screen = iter.data;
    root_visual = iter.data->root_visual;

    window = xcb_generate_id(connection);
    syms = xcb_key_symbols_alloc(connection);

    uint32_t window_values[] = {
      XCB_EVENT_MASK_KEY_RELEASE |
      XCB_EVENT_MASK_KEY_PRESS |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_POINTER_MOTION |
      XCB_EVENT_MASK_BUTTON_PRESS |
      XCB_EVENT_MASK_BUTTON_RELEASE
    };

    if (settings->fullscreen) {
      width = screen->width_in_pixels;
      height = screen->height_in_pixels;
      dimension_cb(width, height);
    }

    xcb_create_window(connection,
                      XCB_COPY_FROM_PARENT,
                      window, screen->root,
                      0, 0,
                      width, height,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      XCB_CW_EVENT_MASK,
                      window_values);

    /* Will send notification when window is destroyed */
    atom_wm_protocols = get_atom("WM_PROTOCOLS");
    atom_wm_delete_window = get_atom("WM_DELETE_WINDOW");

    xcb_change_property(connection,
                        XCB_PROP_MODE_REPLACE,
                        window,
                        atom_wm_protocols,
                        XCB_ATOM_ATOM,
                        32, 1,
                        &atom_wm_delete_window);

    if (settings->fullscreen) {
      xcb_atom_t atom_wm_state = get_atom("_NET_WM_STATE");
      xcb_atom_t atom_wm_fullscreen = get_atom("_NET_WM_STATE_FULLSCREEN");
      xcb_change_property(connection,
                          XCB_PROP_MODE_REPLACE,
                          window,
                          atom_wm_state,
                          XCB_ATOM_ATOM,
                          32, 1,
                          &atom_wm_fullscreen);
    }

    xcb_map_window(connection, window);

    return 0;
  }

  void iterate(VkQueue queue, VkSemaphore semaphore) {
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(connection))) {
      handle_event(event);
      free(event);
    }
  }

  void init_swap_chain(uint32_t width, uint32_t height) {
    VkResult err = create_surface(swap_chain.instance, &swap_chain.surface);
    vik_log_f_if(err != VK_SUCCESS, "Could not create surface!");
    swap_chain.set_dimension_cb(dimension_cb);
    swap_chain.select_queue_and_format();
    swap_chain.set_settings(settings);
    swap_chain.create(width, height);
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
  }

  void handle_client_message(const xcb_client_message_event_t *event) {
    if (event->data.data32[0] == atom_wm_delete_window)
      quit_cb();
  }

  void handle_event(const xcb_generic_event_t *event) {
    switch (event->response_type & 0x7f) {
      case XCB_CLIENT_MESSAGE:
        handle_client_message((const xcb_client_message_event_t *)event);
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

};
}
