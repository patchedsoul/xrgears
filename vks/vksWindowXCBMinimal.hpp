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

#define KEY_ESCAPE 0x9

class VikWindowXCBMinimal : public VikWindow {
  xcb_connection_t *connection;
  xcb_screen_t *screen;
  xcb_window_t window;
  xcb_intern_atom_reply_t *atom_wm_delete_window;

 public:
  explicit VikWindowXCBMinimal() {
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

  ~VikWindowXCBMinimal() {
    xcb_destroy_window(connection, window);
    xcb_disconnect(connection);
  }

  void initSwapChain(const VkInstance &instance, VulkanSwapChain* swapChain) {
    VkResult err = VK_SUCCESS;

    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.connection = connection;
    surfaceCreateInfo.window = window;
    err = vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &swapChain->surface);

    if (err != VK_SUCCESS)
      vks::tools::exitFatal("Could not create surface!", "Fatal error");
    else
      swapChain->initSurfaceCommon();
  }

  void renderLoop(Application *app) {
    xcb_flush(connection);
    while (!app->quit) {
      auto tStart = std::chrono::high_resolution_clock::now();
      if (app->viewUpdated) {
        app->viewUpdated = false;
        app->viewChanged();
      }
      xcb_generic_event_t *event;
      while ((event = xcb_poll_for_event(connection))) {
        handleEvent(app, event);
        free(event);
      }
      app->render();
      app->frameCounter++;
      auto tEnd = std::chrono::high_resolution_clock::now();
      auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
      app->frameTimer = tDiff / 1000.0f;
      app->camera.update(app->frameTimer);
      if (app->camera.moving())
        app->viewUpdated = true;
      // Convert to clamped timer value
      if (!app->paused) {
        app->timer += app->timerSpeed * app->frameTimer;
        if (app->timer > 1.0)
          app->timer -= 1.0f;
      }
      app->fpsTimer += (float)tDiff;
      if (app->fpsTimer > 1000.0f) {
        if (!app->enableTextOverlay) {
          std::string windowTitle = app->getWindowTitle();
          xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                              window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                              windowTitle.size(), windowTitle.c_str());
        }
        app->lastFPS = app->frameCounter;
        app->updateTextOverlay();
        app->fpsTimer = 0.0f;
        app->frameCounter = 0;
      }
    }
  }

  static inline xcb_intern_atom_reply_t* intern_atom_helper(xcb_connection_t *conn, bool only_if_exists, const char *str) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, only_if_exists, strlen(str), str);
    return xcb_intern_atom_reply(conn, cookie, NULL);
  }

  // Set up a window using XCB and request event types
  int setupWindow(Application *app) {
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
      app->width = app->destWidth = screen->width_in_pixels;
      app->height = app->destHeight = screen->height_in_pixels;
    }

    xcb_create_window(connection,
                      XCB_COPY_FROM_PARENT,
                      window, screen->root,
                      0, 0, app->width, app->height, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      value_mask, value_list);

    /* Magic code that will send notification when window is destroyed */
    xcb_intern_atom_reply_t* reply = intern_atom_helper(connection, true, "WM_PROTOCOLS");
    atom_wm_delete_window = intern_atom_helper(connection, false, "WM_DELETE_WINDOW");

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                        window, (*reply).atom, 4, 32, 1,
                        &(*atom_wm_delete_window).atom);

    std::string windowTitle = app->getWindowTitle();
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
                        window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        app->title.size(), windowTitle.c_str());

    free(reply);


    xcb_map_window(connection, window);

    return 0;
  }

  void handleEvent(Application *app, const xcb_generic_event_t *event) {
    switch (event->response_type & 0x7f) {
      case XCB_CLIENT_MESSAGE:
        if ((*(xcb_client_message_event_t*)event).data.data32[0] ==
            (*atom_wm_delete_window).atom) {
          app->quit = true;
        }
        break;

      case XCB_KEY_RELEASE: {
        const xcb_key_release_event_t *keyEvent = (const xcb_key_release_event_t *)event;
        switch (keyEvent->detail) {
          case KEY_ESCAPE:
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
        if ((app->prepared) && ((cfgEvent->width != app->width) || (cfgEvent->height != app->height))) {
          app->destWidth = cfgEvent->width;
          app->destHeight = cfgEvent->height;
          if ((app->destWidth > 0) && (app->destHeight > 0))
            app->windowResize();
        }
      }
        break;
      default:
        break;
    }
  }

  const char* requiredExtensionName() {
    return VK_KHR_XCB_SURFACE_EXTENSION_NAME;
  }
};
