#pragma once

#include <string.h>
#include <stdio.h>

#include "render/vikRendererVkc.hpp"
#include "vikWindowXCB.hpp"
#include "system/vikLog.hpp"
#include "render/vikSwapChainVKComplex.hpp"

namespace vik {
class WindowXCBSimple : public WindowXCB {

  SwapChainVkComplex swap_chain;

  bool ready = false;

public:
  WindowXCBSimple(Settings *s) : WindowXCB(s) {
    name = "xcb-simple";
    window_values =
          XCB_EVENT_MASK_EXPOSURE |
          XCB_EVENT_MASK_KEY_RELEASE |
          XCB_EVENT_MASK_KEY_PRESS |
          XCB_EVENT_MASK_STRUCTURE_NOTIFY |
          XCB_EVENT_MASK_POINTER_MOTION |
          XCB_EVENT_MASK_BUTTON_PRESS |
          XCB_EVENT_MASK_BUTTON_RELEASE;
  }

  ~WindowXCBSimple() {}

  void iterate_vkc(VkQueue queue, VkSemaphore semaphore) {
    poll_events();
    if (ready) {
      update_cb();

      swap_chain.render(queue, semaphore);
      //schedule_repaint();
    }
    xcb_flush(connection);
  }

  void iterate_vks(VkQueue queue, VkSemaphore semaphore) {
    poll_events();
  }

  /*
  void init_swap_chain(uint32_t width, uint32_t height) {
    create_surface(swap_chain.instance, &swap_chain.surface);
    swap_chain.select_surface_format();
  }*/


  void init_swap_chain_vkc(uint32_t width, uint32_t height) {
    create_surface(swap_chain.instance, &swap_chain.surface);
    swap_chain.set_dimension_cb(dimension_cb);
    swap_chain.set_settings(settings);
    swap_chain.select_surface_format();
  }

  void init_swap_chain_vks(uint32_t width, uint32_t height) {
    vik_log_e("init_swap_chain_vks");
    VkResult err = create_surface(swap_chain.instance, &swap_chain.surface);
    vik_log_f_if(err != VK_SUCCESS, "Could not create surface!");
    swap_chain.set_dimension_cb(dimension_cb);
    swap_chain.select_queue();
    swap_chain.select_surface_format();
    swap_chain.set_settings(settings);
    //swap_chain.create(width, height);
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
  }

  void schedule_repaint() {
    xcb_client_message_event_t client_message;
    client_message.response_type = XCB_CLIENT_MESSAGE;
    client_message.format = 32;
    client_message.window = window;
    client_message.type = XCB_ATOM_NOTICE;
    xcb_send_event(connection, 0, window, 0, (char *) &client_message);
  }

  void handle_client_message(const xcb_client_message_event_t *event) {
    WindowXCB::handle_client_message(event);

    if (event->type == XCB_ATOM_NOTICE)
      ready = true;
  }


  void handle_expose(const xcb_expose_event_t *event) {
    vik_log_e("handle_expose");
    vik_log_d("XCB_EXPOSE %dx%d", event->width, event->height);
    //swap_chain.recreate(event->width, event->height);
    swap_chain.create(event->width, event->height);
    create_buffers_cb(swap_chain.image_count);
    WindowXCB::handle_expose(event);
    schedule_repaint();
  }


};
}
