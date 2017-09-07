#pragma once

#include <xcb/xcb.h>
#include <string.h>
#include <stdio.h>

#include "vkcWindow.hpp"
#include "vkcApplication.hpp"
#include "vkcRenderer.hpp"

#include "../vks/vksLog.hpp"

static xcb_atom_t
get_atom(struct xcb_connection_t *conn, const char *name)
{
  xcb_intern_atom_cookie_t cookie;
  xcb_intern_atom_reply_t *reply;
  xcb_atom_t atom;

  cookie = xcb_intern_atom(conn, 0, strlen(name), name);
  reply = xcb_intern_atom_reply(conn, cookie, NULL);
  if (reply)
    atom = reply->atom;
  else
    atom = XCB_NONE;

  free(reply);
  return atom;
}

namespace vkc {
class WindowXCB : public Window {

  xcb_connection_t *conn;
  xcb_window_t window;
  xcb_atom_t atom_wm_protocols;
  xcb_atom_t atom_wm_delete_window;

  bool repaint = false;

public:
  WindowXCB() {
    window = XCB_NONE;
    name = "xcb";
  }

  ~WindowXCB() {}

  // Return -1 on failure.
  int
  init(Application* app, Renderer *vc)
  {
    xcb_screen_iterator_t iter;
    static const char title[] = "Vulkan Cube";

    conn = xcb_connect(0, 0);
    if (xcb_connection_has_error(conn))
      return -1;

    window = xcb_generate_id(conn);

    uint32_t window_values[] = {
      XCB_EVENT_MASK_EXPOSURE |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_KEY_PRESS
    };

    iter = xcb_setup_roots_iterator(xcb_get_setup(conn));

    xcb_create_window(conn,
                      XCB_COPY_FROM_PARENT,
                      window,
                      iter.data->root,
                      0, 0,
                      vc->width,
                      vc->height,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      iter.data->root_visual,
                      XCB_CW_EVENT_MASK, window_values);

    atom_wm_protocols = get_atom(conn, "WM_PROTOCOLS");
    atom_wm_delete_window = get_atom(conn, "WM_DELETE_WINDOW");
    xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        window,
                        atom_wm_protocols,
                        XCB_ATOM_ATOM,
                        32,
                        1, &atom_wm_delete_window);

    xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        window,
                        get_atom(conn, "_NET_WM_NAME"),
                        get_atom(conn, "UTF8_STRING"),
                        8, // sizeof(char),
                        strlen(title), title);

    xcb_map_window(conn, window);

    xcb_flush(conn);

    vc->init_vk(VK_KHR_XCB_SURFACE_EXTENSION_NAME);

    VkBool32 ret = vkGetPhysicalDeviceXcbPresentationSupportKHR(
          vc->physical_device, 0, conn, iter.data->root_visual);
    vik_log_f_if(!ret, "Vulkan not supported on given X window");

    init_surface(vc);

    //vc->init_vk_objects(app);
    vc->init_render_pass();
    app->init();
    vc->init_vk_objects();

    vc->image_count = 0;

    return 0;
  }

  void init_surface(Renderer *vc) {
    VkXcbSurfaceCreateInfoKHR surfaceInfo = {};

    surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.connection = conn;
    surfaceInfo.window = window;

    vkCreateXcbSurfaceKHR(vc->instance, &surfaceInfo, NULL, &vc->surface);

    vc->image_format = vc->choose_surface_format();
  }

  void schedule_repaint()
  {
    xcb_client_message_event_t client_message;
    client_message.response_type = XCB_CLIENT_MESSAGE;
    client_message.format = 32;
    client_message.window = window;
    client_message.type = XCB_ATOM_NOTICE;
    xcb_send_event(conn, 0, window, 0, (char *) &client_message);
  }


  void poll_events(Renderer *vc) {
    xcb_generic_event_t *event;
    xcb_key_press_event_t *key_press;
    xcb_client_message_event_t *client_message;
    xcb_configure_notify_event_t *configure;

    event = xcb_wait_for_event(conn);
    while (event) {
      switch (event->response_type & 0x7f) {
        case XCB_CLIENT_MESSAGE:
          client_message = (xcb_client_message_event_t *) event;
          if (client_message->window != window)
            break;

          if (client_message->type == atom_wm_protocols &&
              client_message->data.data32[0] == atom_wm_delete_window) {
            exit(0);
          }

          if (client_message->type == XCB_ATOM_NOTICE)
            repaint = true;
          break;

        case XCB_CONFIGURE_NOTIFY:
          configure = (xcb_configure_notify_event_t *) event;
          if (vc->width != configure->width ||
              vc->height != configure->height) {
            if (vc->image_count > 0) {
              vkDestroySwapchainKHR(vc->device, vc->swap_chain, NULL);
              vc->image_count = 0;
            }

            vc->width = configure->width;
            vc->height = configure->height;
          }
          break;

        case XCB_EXPOSE:
          schedule_repaint();
          break;

        case XCB_KEY_PRESS:
          key_press = (xcb_key_press_event_t *) event;

          if (key_press->detail == 9)
            exit(0);

          break;
      }
      free(event);

      event = xcb_poll_for_event(conn);
    }
  }

  void present(Renderer *vc, uint32_t index) {
    VkSwapchainKHR swapChains[] = { vc->swap_chain, };
    uint32_t indices[] = { index, };

    VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .swapchainCount = 1,
      .pSwapchains = swapChains,
      .pImageIndices = indices,
      //.pResults = &result,
    };

    VkResult result = vkQueuePresentKHR(vc->queue, &presentInfo);
    vik_log_f_if(result != VK_SUCCESS, "vkQueuePresentKHR failed.");
  }

  void loop(Application* app, Renderer *vc) {
    while (1) {

      poll_events(vc);

      if (repaint) {
        if (vc->image_count == 0)
          vc->create_swapchain();

        uint32_t index;
        vkAcquireNextImageKHR(vc->device, vc->swap_chain, 60,
                              vc->semaphore, VK_NULL_HANDLE, &index);

        app->render(&vc->buffers[index]);
        present(vc, index);
        schedule_repaint();
      }
      xcb_flush(conn);

    }
  }
};
}