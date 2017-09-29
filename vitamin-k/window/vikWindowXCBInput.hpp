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
    window_values =
          XCB_EVENT_MASK_EXPOSURE |
          XCB_EVENT_MASK_KEY_RELEASE |
          XCB_EVENT_MASK_KEY_PRESS |
          XCB_EVENT_MASK_STRUCTURE_NOTIFY |
          XCB_EVENT_MASK_POINTER_MOTION |
          XCB_EVENT_MASK_BUTTON_PRESS |
          XCB_EVENT_MASK_BUTTON_RELEASE;
  }

  ~WindowXCBInput() {}

  void iterate_vks(VkQueue queue, VkSemaphore semaphore) {
    poll_events();
  }

  void iterate_vkc(VkQueue queue, VkSemaphore semaphore) {
    poll_events();
    update_cb();
    swap_chain.render(queue, semaphore);
    xcb_flush(connection);
  }

  void init_swap_chain_vks(uint32_t width, uint32_t height) {
    VkResult err = create_surface(swap_chain.instance, &swap_chain.surface);
    vik_log_f_if(err != VK_SUCCESS, "Could not create surface!");
    swap_chain.set_dimension_cb(dimension_cb);
    swap_chain.select_queue();
    swap_chain.select_surface_format();
    swap_chain.set_settings(settings);
    swap_chain.create(width, height);
  }

  void init_swap_chain_vkc(uint32_t width, uint32_t height) {
    create_surface(swap_chain.instance, &swap_chain.surface);
    swap_chain.set_dimension_cb(dimension_cb);
    swap_chain.set_settings(settings);
    swap_chain.select_surface_format();
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
  }

};
}
