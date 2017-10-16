/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <xcb/xcb.h>
#include <X11/keysym.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/randr.h>

#include <string>
#include <vector>
#include <utility>

#include "vikWindow.hpp"

#include "../render/vikSwapChainVK.hpp"

namespace vik {
class WindowXCB : public Window {
  xcb_connection_t *connection;
  xcb_window_t window = XCB_NONE;
  xcb_key_symbols_t *syms;
  xcb_screen_t *screen;

  uint32_t window_values =
      XCB_EVENT_MASK_EXPOSURE |
      XCB_EVENT_MASK_KEY_RELEASE |
      XCB_EVENT_MASK_KEY_PRESS |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_POINTER_MOTION |
      XCB_EVENT_MASK_BUTTON_PRESS |
      XCB_EVENT_MASK_BUTTON_RELEASE;

  xcb_atom_t atom_wm_protocols;
  xcb_atom_t atom_wm_delete_window;

  SwapChainVK swap_chain;

  struct Display {
    std::string name;
    std::pair<int16_t, int16_t> position;
    std::pair<uint16_t, uint16_t> size;
  };

  std::vector<Display> displays;

 public:
  explicit WindowXCB(Settings *s) : Window(s) {
    name = "xcb";
  }

  ~WindowXCB() {
    xcb_destroy_window(connection, window);
    xcb_disconnect(connection);
    xcb_key_symbols_free(syms);
  }

  int init() {
    if (!connect())
      return -1;

    xcb_screen_iterator_t iter =
        xcb_setup_roots_iterator(xcb_get_setup(connection));

    screen = iter.data;

    if (settings->list_screens_and_exit) {
      vik_log_i("Screen 0 %dx%d",
                iter.data->width_in_pixels,
                iter.data->height_in_pixels);
      get_randr_outputs();

      int display_i = 0;
      for (Display d : displays) {
        vik_log_i("%d: %s %dx%d [%d, %d]",
                  display_i,
                  d.name.c_str(),
                  d.size.first, d.size.second,
                  d.position.first, d.position.second);
        display_i++;
      }
      exit(0);
    }

    syms = xcb_key_symbols_alloc(connection);

    if (settings->fullscreen) {
      get_randr_outputs();

      if (settings->display > displays.size()) {
        vik_log_e("Requested display %d, but only %d displays are available.",
                  settings->display, displays.size());

        settings->display = 0;
        Display *d = current_display();
        vik_log_e("Selecting '%s' instead.",
                  d->name.c_str());
      }

      Display *d = current_display();
      settings->size.first = d->size.first;
      settings->size.second = d->size.second;
      size_only_cb(settings->size.first, settings->size.second);
    }

    create_window(settings->size.first, settings->size.second, &window_values);

    connect_delete_event();

    if (settings->fullscreen)
      set_full_screen();

    xcb_map_window(connection, window);

    return 0;
  }

  Display* current_display() {
    return &displays[settings->display];
  }

  void iterate() {
    poll_events();
    render_frame_cb();
  }

  void init_swap_chain(uint32_t width, uint32_t height) {
    VkResult err = create_surface(swap_chain.instance, &swap_chain.surface);
    vik_log_f_if(err != VK_SUCCESS, "Could not create surface!");
    swap_chain.set_dimension_cb(size_only_cb);
    swap_chain.set_settings(settings);
    swap_chain.select_surface_format();
    swap_chain.create(width, height);
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
  }

  int connect() {
    connection = xcb_connect(0, 0);
    return !xcb_connection_has_error(connection);
  }

  void create_window(uint32_t width, uint32_t height,
                     uint32_t *window_values) {
    window = xcb_generate_id(connection);

    int x = 0;
    int y = 0;

    if (settings->fullscreen) {
      x = current_display()->position.first;
      y = current_display()->position.second;
    }

    xcb_create_window(connection,
                      XCB_COPY_FROM_PARENT,
                      window,
                      screen->root,
                      x, y,
                      width, height,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      XCB_CW_EVENT_MASK,
                      window_values);
  }

  void get_randr_outputs() {
    xcb_randr_get_screen_resources_cookie_t resources_cookie =
        xcb_randr_get_screen_resources(connection, screen->root);
    xcb_randr_get_screen_resources_reply_t *resources_reply =
        xcb_randr_get_screen_resources_reply(connection, resources_cookie, nullptr);
    xcb_randr_output_t *xcb_outputs =
        xcb_randr_get_screen_resources_outputs(resources_reply);

    int count = xcb_randr_get_screen_resources_outputs_length(resources_reply);
    if (count < 1)
      vik_log_f("failed to retrieve randr outputs");

    for (int i = 0; i < count; i++) {
      xcb_randr_get_output_info_cookie_t output_cookie =
          xcb_randr_get_output_info(connection, xcb_outputs[i], XCB_CURRENT_TIME);
      xcb_randr_get_output_info_reply_t *output_reply =
          xcb_randr_get_output_info_reply(connection, output_cookie, nullptr);

      if (output_reply->connection != XCB_RANDR_CONNECTION_CONNECTED ||
          output_reply->crtc == XCB_NONE)
        continue;

      xcb_randr_get_crtc_info_cookie_t crtc_cookie =
          xcb_randr_get_crtc_info(connection, output_reply->crtc, XCB_CURRENT_TIME);
      xcb_randr_get_crtc_info_reply_t *crtc_reply =
          xcb_randr_get_crtc_info_reply(connection, crtc_cookie, nullptr);

      uint8_t *name = xcb_randr_get_output_info_name(output_reply);
      int name_len = xcb_randr_get_output_info_name_length(output_reply);

      char* name_str = (char*) malloc(name_len + 1);
      memcpy(name_str, name, name_len);
      name_str[name_len] = '\0';

      Display d = {};
      d.name = std::string(name_str);
      d.position = {crtc_reply->x, crtc_reply->y};
      d.size = {crtc_reply->width, crtc_reply->height};
      displays.push_back(d);

      free(name_str);
    }
  }

  void connect_delete_event() {
    atom_wm_protocols = get_atom("WM_PROTOCOLS");
    atom_wm_delete_window = get_atom("WM_DELETE_WINDOW");
    xcb_change_property(connection,
                        XCB_PROP_MODE_REPLACE,
                        window,
                        atom_wm_protocols,
                        XCB_ATOM_ATOM,
                        32,
                        1,
                        &atom_wm_delete_window);
  }

  void set_full_screen() {
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

  xcb_atom_t get_atom(const char *name) {
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom;

    cookie = xcb_intern_atom(connection, 0, strlen(name), name);
    reply = xcb_intern_atom_reply(connection, cookie, NULL);
    if (reply)
      atom = reply->atom;
    else
      atom = XCB_NONE;

    free(reply);
    return atom;
  }

  static Input::MouseButton xcb_to_vik_button(xcb_button_t button) {
    switch (button) {
      case XCB_BUTTON_INDEX_1:
        return Input::MouseButton::Left;
      case XCB_BUTTON_INDEX_2:
        return Input::MouseButton::Middle;
      case XCB_BUTTON_INDEX_3:
        return Input::MouseButton::Right;
      default:
        return Input::MouseButton::Left;
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
      case XK_space:
        return Input::Key::SPACE;
      default:
        return Input::Key::UNKNOWN;
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
          physical_device, 0, connection, screen->root_visual);
  }

  void update_window_title(const std::string& title) {
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                        window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        title.size(), title.c_str());
  }

  virtual void handle_client_message(const xcb_client_message_event_t *event) {
    if (event->type == atom_wm_protocols &&
        event->data.data32[0] == atom_wm_delete_window)
      quit_cb();
  }

  virtual void handle_expose(const xcb_expose_event_t *event) {
    dimension_cb(event->width, event->height);
  }

  void poll_events() {
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(connection))) {
      handle_event(event);
      free(event);
    }
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

        if (press->detail == XCB_BUTTON_INDEX_4)
          pointer_axis_cb(Input::MouseScrollAxis::X, -10.0f);

        if (press->detail == XCB_BUTTON_INDEX_5)
          pointer_axis_cb(Input::MouseScrollAxis::X, 10.0f);
      }
        break;
      case XCB_BUTTON_RELEASE: {
        xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
        pointer_button_cb(xcb_to_vik_button(press->detail), false);
      }
        break;
      case XCB_KEY_PRESS: {
        const xcb_key_press_event_t *keyEvent = (const xcb_key_press_event_t *)event;
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
      case XCB_EXPOSE: {
        handle_expose((const xcb_expose_event_t *)event);
      }
        break;
      default:
        break;
    }
  }
};
}  // namespace vik
