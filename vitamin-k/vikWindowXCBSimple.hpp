#pragma once

#include <string.h>
#include <stdio.h>

#include "../vkc/vkcRenderer.hpp"
#include "vikWindowXCB.hpp"
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

namespace vik {
class WindowXCBSimple : public WindowXCB {

  xcb_atom_t atom_wm_protocols;
  xcb_atom_t atom_wm_delete_window;

  bool repaint = false;

public:
  WindowXCBSimple() {
    name = "xcb-simple";
  }

  ~WindowXCBSimple() {}

  // Return -1 on failure.
  int init(Renderer* r) {
    xcb_screen_iterator_t iter;
    static const char title[] = "Vulkan Cube";

    connection = xcb_connect(0, 0);
    if (xcb_connection_has_error(connection))
      return -1;

    window = xcb_generate_id(connection);

    uint32_t window_values[] = {
      XCB_EVENT_MASK_EXPOSURE |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_KEY_PRESS
    };

    iter = xcb_setup_roots_iterator(xcb_get_setup(connection));

    xcb_create_window(connection,
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

    atom_wm_protocols = get_atom(connection, "WM_PROTOCOLS");
    atom_wm_delete_window = get_atom(connection, "WM_DELETE_WINDOW");
    xcb_change_property(connection,
                        XCB_PROP_MODE_REPLACE,
                        window,
                        atom_wm_protocols,
                        XCB_ATOM_ATOM,
                        32,
                        1, &atom_wm_delete_window);

    xcb_change_property(connection,
                        XCB_PROP_MODE_REPLACE,
                        window,
                        get_atom(connection, "_NET_WM_NAME"),
                        get_atom(connection, "UTF8_STRING"),
                        8, // sizeof(char),
                        strlen(title), title);

    xcb_map_window(connection, window);

    xcb_flush(connection);

    root_visual = iter.data->root_visual;

    return 0;
  }

  void iterate(Renderer *r) {
    poll_events(r);

    if (repaint) {
      update_cb();

      SwapChainVK* sc = (SwapChainVK*) r->swap_chain;
      vkc::Renderer* vkc_renderer = (vkc::Renderer*) r;
      sc->render(r->queue, vkc_renderer->semaphore);
      schedule_repaint();
    }
    xcb_flush(connection);
  }

  void init_swap_chain(Renderer *r) {
    r->swap_chain = new SwapChainVK();
    SwapChainVK *sc = (SwapChainVK*) r->swap_chain;
    sc->set_context(r->instance, r->physical_device, r->device);

    create_surface(r->instance, &sc->surface);

    sc->choose_surface_format();
    sc->create_simple(r->width, r->height);
    sc->update_images();
  }

  void schedule_repaint() {
    xcb_client_message_event_t client_message;
    client_message.response_type = XCB_CLIENT_MESSAGE;
    client_message.format = 32;
    client_message.window = window;
    client_message.type = XCB_ATOM_NOTICE;
    xcb_send_event(connection, 0, window, 0, (char *) &client_message);
  }

  void poll_events(Renderer *r) {
    xcb_generic_event_t *event;
    event = xcb_wait_for_event(connection);
    while (event) {
      handle_event(event, r);
      free(event);
      event = xcb_poll_for_event(connection);
    }
  }

  void handle_event(const xcb_generic_event_t *event, Renderer *r) {
    xcb_key_press_event_t *key_press;
    xcb_client_message_event_t *client_message;
    xcb_configure_notify_event_t *configure;

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

          SwapChainVK *sc = (SwapChainVK*) r->swap_chain;

          if (sc != nullptr)
            sc->destroy();

          r->width = configure->width;
          r->height = configure->height;
        }
        break;
      case XCB_EXPOSE:
        {
          vik_log_d("XCB_EXPOSE");
          SwapChainVK *sc = (SwapChainVK*) r->swap_chain;
          sc->create_simple(r->width, r->height);
          sc->update_images();
          vkc::Renderer* vkc_renderer = (vkc::Renderer*) r;
          vkc_renderer->create_frame_buffers(r->swap_chain);
          schedule_repaint();
        }
        break;
      case XCB_KEY_PRESS:
        key_press = (xcb_key_press_event_t *) event;
        if (key_press->detail == 9)
          exit(0);
        break;
    }
  }
};
}
