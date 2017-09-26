#pragma once

#include <string.h>
#include <stdio.h>

#include "render/vikRendererVkc.hpp"
#include "vikWindowXCB.hpp"
#include "system/vikLog.hpp"
#include "render/vikSwapChainVK.hpp"

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
  int init(uint32_t width, uint32_t height, bool fullscreen) {
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
                      width,
                      height,
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
      RendererVkc* vkc_renderer = (RendererVkc*) r;
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

  void handle_client_message(const xcb_client_message_event_t *event) {
    if (event->window != window)
      return;

    if (event->type == atom_wm_protocols &&
        event->data.data32[0] == atom_wm_delete_window)
      quit_cb();

    if (event->type == XCB_ATOM_NOTICE)
      repaint = true;
  }

  void handle_configure(const xcb_configure_notify_event_t *event) {
    vik_log_d("XCB_CONFIGURE_NOTIFY %dx%d", event->width, event->height);
    dimension_cb(event->width, event->height);
  }

  void handle_expose(const xcb_expose_event_t *event, Renderer *r) {
      vik_log_d("XCB_EXPOSE");
      RendererVkc* vkc_renderer = (RendererVkc*) r;
      vkc_renderer->recreate_swap_chain_vk();
      schedule_repaint();
  }

  void handle_event(const xcb_generic_event_t *event, Renderer *r) {
    switch (event->response_type & 0x7f) {
      case XCB_CLIENT_MESSAGE:
        handle_client_message((xcb_client_message_event_t*)event);
        break;
      case XCB_CONFIGURE_NOTIFY:
        handle_configure((xcb_configure_notify_event_t*) event);
        break;
      case XCB_EXPOSE:
        handle_expose((xcb_expose_event_t*) event, r);
        break;
      case XCB_KEY_PRESS:
        const xcb_key_release_event_t *key_event = (const xcb_key_release_event_t *)event;
        keyboard_key_cb(xcb_to_vik_key(key_event->detail), true);
        break;
    }
  }
};
}