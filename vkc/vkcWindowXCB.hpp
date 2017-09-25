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
  xcb_visualid_t root_visual;

  bool repaint = false;

public:
  WindowXCB() {
    window = XCB_NONE;
    name = "xcb";
  }

  ~WindowXCB() {}

  // Return -1 on failure.
  int init(Renderer* r) {
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
                      r->width,
                      r->height,
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

    root_visual = iter.data->root_visual;

    return 0;
  }

  const std::vector<const char*> required_extensions() {
    return { VK_KHR_XCB_SURFACE_EXTENSION_NAME };
  }

  void create_surface(VkInstance instance, VkSurfaceKHR *surface) {
    VkXcbSurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surface_info.connection = conn;
    surface_info.window = window;
    vkCreateXcbSurfaceKHR(instance, &surface_info, NULL, surface);
  }

  VkBool32 check_support(VkPhysicalDevice physical_device) {
    return vkGetPhysicalDeviceXcbPresentationSupportKHR(
          physical_device, 0, conn, root_visual);
  }

  void init_swap_chain(vik::Renderer *r) {
    r->swap_chain = new vik::SwapChainVK();
    vik::SwapChainVK *sc = (vik::SwapChainVK*) r->swap_chain;
    sc->set_context(r->instance, r->physical_device, r->device);

    create_surface(r->instance, &sc->surface);

    sc->choose_surface_format();
    init_cb();
    sc->create_simple(r->width, r->height);
    sc->update_images();
  }

  void update_window_title(const std::string& title) {}

  void schedule_repaint() {
    xcb_client_message_event_t client_message;
    client_message.response_type = XCB_CLIENT_MESSAGE;
    client_message.format = 32;
    client_message.window = window;
    client_message.type = XCB_ATOM_NOTICE;
    xcb_send_event(conn, 0, window, 0, (char *) &client_message);
  }


  void poll_events(Renderer *r) {
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
          if (r->width != configure->width ||
              r->height != configure->height) {

            vik_log_d("XCB_CONFIGURE_NOTIFY %dx%d", r->width, r->height);

            vik::SwapChainVK *sc = (vik::SwapChainVK*) r->swap_chain;

            if (sc != nullptr)
              sc->destroy();

            r->width = configure->width;
            r->height = configure->height;
          }
          break;
        case XCB_EXPOSE:
          {
            vik_log_d("XCB_EXPOSE");
            vik::SwapChainVK *sc = (vik::SwapChainVK*) r->swap_chain;
            sc->create_simple(r->width, r->height);
            sc->update_images();
            r->create_frame_buffers(r->swap_chain);
            schedule_repaint();
          }
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

  void iterate(Renderer *r) {
    poll_events(r);

    if (repaint) {

      update_cb();
      r->render_swapchain_vk((vik::SwapChainVK*) r->swap_chain);
      schedule_repaint();
    }
    xcb_flush(conn);
  }
};
}
