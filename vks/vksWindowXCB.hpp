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

#include "vksWindow.hpp"
#include "vksApplication.hpp"

#include "vksLog.hpp"

#define XCB_KEY_ESCAPE 0x9
#define XCB_KEY_F1 0x43
#define XCB_KEY_W 0x19
#define XCB_KEY_A 0x26
#define XCB_KEY_S 0x27
#define XCB_KEY_D 0x28
#define XCB_KEY_P 0x21
/*
#define KEY_F2 0x44
#define KEY_F3 0x45
#define KEY_F4 0x46
#define KEY_SPACE 0x41

#define KEY_KPADD 0x56
#define KEY_KPSUB 0x52

#define KEY_B 0x38
#define KEY_F 0x29
#define KEY_L 0x2E
#define KEY_N 0x39
#define KEY_O 0x20
#define KEY_T 0x1C
*/

namespace vks {
class WindowXCB : public Window {
  xcb_connection_t *connection;
  xcb_screen_t *screen;
  xcb_window_t window;
  xcb_intern_atom_reply_t *atom_wm_delete_window;

 public:
  explicit WindowXCB() {
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
  }

  ~WindowXCB() {
    xcb_destroy_window(connection, window);
    xcb_disconnect(connection);
  }

  void init_swap_chain(Renderer *r) {
    VkResult err = VK_SUCCESS;

    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.connection = connection;
    surfaceCreateInfo.window = window;
    err = vkCreateXcbSurfaceKHR(r->instance, &surfaceCreateInfo, nullptr, &r->swapChain.surface);

    vik_log_f_if(err != VK_SUCCESS, "Could not create surface!");

    r->swapChain.initSurfaceCommon();
  }

  void iterate(vks::Application *app) {
    xcb_generic_event_t *event;
    //xcb_flush(connection);
    while ((event = xcb_poll_for_event(connection))) {
      handleEvent(app, event);
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
  int init(vks::Application *app) {
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

    if (app->settings.fullscreen) {
      app->renderer->width = app->renderer->destWidth = screen->width_in_pixels;
      app->renderer->height = app->renderer->destHeight = screen->height_in_pixels;
    }

    xcb_create_window(connection,
                      XCB_COPY_FROM_PARENT,
                      window, screen->root,
                      0, 0, app->renderer->width, app->renderer->height, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      value_mask, value_list);

    /* Magic code that will send notification when window is destroyed */
    xcb_intern_atom_reply_t* reply = intern_atom_helper(connection, true, "WM_PROTOCOLS");
    atom_wm_delete_window = intern_atom_helper(connection, false, "WM_DELETE_WINDOW");

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                        window, (*reply).atom, 4, 32, 1,
                        &(*atom_wm_delete_window).atom);

    std::string windowTitle = app->renderer->make_title_string(app->title);
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                        window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        app->title.size(), windowTitle.c_str());

    free(reply);

    if (app->settings.fullscreen) {
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

  void handleEvent(vks::Application *app, const xcb_generic_event_t *event) {
    switch (event->response_type & 0x7f) {
      case XCB_CLIENT_MESSAGE:
        if ((*(xcb_client_message_event_t*)event).data.data32[0] ==
            (*atom_wm_delete_window).atom) {
          app->quit = true;
        }
        break;
      case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;
        if (app->mouseButtons.left) {
          app->rotation.x += (app->mousePos.y - (float)motion->event_y) * 1.25f;
          app->rotation.y -= (app->mousePos.x - (float)motion->event_x) * 1.25f;
          app->camera.rotate(
                glm::vec3((
                            app->mousePos.y - (float)motion->event_y) * app->camera.rotationSpeed,
                          -(app->mousePos.x - (float)motion->event_x) * app->camera.rotationSpeed,
                          0.0f));
          app->viewUpdated = true;
        }
        if (app->mouseButtons.right) {
          app->zoom += (app->mousePos.y - (float)motion->event_y) * .005f;
          app->camera.translate(glm::vec3(-0.0f, 0.0f, (app->mousePos.y - (float)motion->event_y) * .005f * app->zoomSpeed));
          app->viewUpdated = true;
        }
        if (app->mouseButtons.middle) {
          app->cameraPos.x -= (app->mousePos.x - (float)motion->event_x) * 0.01f;
          app->cameraPos.y -= (app->mousePos.y - (float)motion->event_y) * 0.01f;
          app->camera.translate(
                glm::vec3(
                  -(app->mousePos.x - (float)(float)motion->event_x) * 0.01f,
                  -(app->mousePos.y - (float)motion->event_y) * 0.01f,
                  0.0f));
          app->viewUpdated = true;
          app->mousePos.x = (float)motion->event_x;
          app->mousePos.y = (float)motion->event_y;
        }
        app->mousePos = glm::vec2((float)motion->event_x, (float)motion->event_y);
      }
        break;
      case XCB_BUTTON_PRESS: {
        xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
        if (press->detail == XCB_BUTTON_INDEX_1)
          app->mouseButtons.left = true;
        if (press->detail == XCB_BUTTON_INDEX_2)
          app->mouseButtons.middle = true;
        if (press->detail == XCB_BUTTON_INDEX_3)
          app->mouseButtons.right = true;
      }
        break;
      case XCB_BUTTON_RELEASE: {
        xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
        if (press->detail == XCB_BUTTON_INDEX_1)
          app->mouseButtons.left = false;
        if (press->detail == XCB_BUTTON_INDEX_2)
          app->mouseButtons.middle = false;
        if (press->detail == XCB_BUTTON_INDEX_3)
          app->mouseButtons.right = false;
      }
        break;
      case XCB_KEY_PRESS: {
        const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
        switch (keyEvent->detail) {
          case XCB_KEY_W:
            app->camera.keys.up = true;
            break;
          case XCB_KEY_S:
            app->camera.keys.down = true;
            break;
          case XCB_KEY_A:
            app->camera.keys.left = true;
            break;
          case XCB_KEY_D:
            app->camera.keys.right = true;
            break;
          case XCB_KEY_P:
            app->renderer->timer.toggle_animation_pause();
            break;
          case XCB_KEY_F1:
            if (app->renderer->enableTextOverlay)
              app->renderer->textOverlay->visible = !app->renderer->textOverlay->visible;
            break;
        }
      }
        break;
      case XCB_KEY_RELEASE: {
        const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
        switch (keyEvent->detail) {
          case XCB_KEY_W:
            app->camera.keys.up = false;
            break;
          case XCB_KEY_S:
            app->camera.keys.down = false;
            break;
          case XCB_KEY_A:
            app->camera.keys.left = false;
            break;
          case XCB_KEY_D:
            app->camera.keys.right = false;
            break;
          case XCB_KEY_ESCAPE:
            app->quit = true;
            break;
        }
        app->keyPressed(keyEvent->detail);
      }
        break;
      case XCB_DESTROY_NOTIFY:
        app->quit = true;
        break;
      case XCB_CONFIGURE_NOTIFY: {
        const xcb_configure_notify_event_t *cfgEvent = (const xcb_configure_notify_event_t *)event;
        if ((app->prepared) && ((cfgEvent->width != app->renderer->width) || (cfgEvent->height != app->renderer->height))) {
          app->renderer->destWidth = cfgEvent->width;
          app->renderer->destHeight = cfgEvent->height;
          if ((app->renderer->destWidth > 0) && (app->renderer->destHeight > 0))
            app->windowResize();
        }
      }
        break;
      default:
        break;
    }
  }

  const std::vector<const char*> required_extensions() {
    return {VK_KHR_XCB_SURFACE_EXTENSION_NAME };
  }
};
}
