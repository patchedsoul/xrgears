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

#include "vikLog.hpp"
#include "vikSwapChainVKComplex.hpp"
#include "vikWindowXCB.hpp"

namespace vik {
class WindowXCBInput : public WindowXCB {
  xcb_screen_t *screen;
  xcb_intern_atom_reply_t *atom_wm_delete_window;

 public:
  explicit WindowXCBInput() {
    name = "xcb-input";

    xcb_screen_iterator_t iter;
    int scr;

    connection = xcb_connect(NULL, &scr);

    vik_log_f_if(connection == NULL, "Failed to create XCB connection");

    iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
    while (scr-- > 0)
      xcb_screen_next(&iter);
    screen = iter.data;
    root_visual = iter.data->root_visual;
  }

  ~WindowXCBInput() {
    xcb_destroy_window(connection, window);
    xcb_disconnect(connection);
    xcb_key_symbols_free(syms);
  }

  // Set up a window using XCB and request event types
  int init(Renderer *r) {
    uint32_t value_mask, value_list[32];

    window = xcb_generate_id(connection);
    syms = xcb_key_symbols_alloc(connection);

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

  void iterate(Renderer *r) {
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(connection))) {
      handle_event(event);
      free(event);
    }
  }

  void init_swap_chain(Renderer *r) {
    r->swap_chain = new SwapChainVkComplex();
    SwapChainVkComplex *sc = (SwapChainVkComplex*) r->swap_chain;
    sc->set_context(r->instance, r->physical_device, r->device);

    VkResult err = create_surface(r->instance, &sc->surface);
    vik_log_f_if(err != VK_SUCCESS, "Could not create surface!");

    sc->select_queue_and_format();
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

  static inline xcb_intern_atom_reply_t* intern_atom_helper(xcb_connection_t *conn, bool only_if_exists, const char *str) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, only_if_exists, strlen(str), str);
    return xcb_intern_atom_reply(conn, cookie, NULL);
  }

};
}
