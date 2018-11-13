/*
 * vitamin-k
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <functional>

#include "../system/vikLog.hpp"
#include "../system/vikSettings.hpp"

namespace vik {

struct SwapChainBuffer {
  VkImage image;
  VkImageView view;
};

class SwapChain {
 public:
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkInstance instance;
  std::vector<SwapChainBuffer> buffers;

  uint32_t image_count = 0;
  VkSurfaceFormatKHR surface_format;

  Settings *settings;

  std::function<void(uint32_t index)> render_cb;

  SwapChain() {}
  virtual ~SwapChain() {}

  virtual void cleanup() = 0;

  virtual uint32_t get_queue_index() {
    return 0;
  }

  virtual void create(uint32_t width, uint32_t height) = 0;

  void set_settings(Settings *s) {
    settings = s;
  }

  void set_render_cb(std::function<void(uint32_t index)> cb) {
    render_cb = cb;
  }

  /**
  * Set instance, physical and logical device to use for the swapchain and get all required function pointers
  *
  * @param instance Vulkan instance to use
  * @param physicalDevice Physical device used to query properties and formats relevant to the swapchain
  * @param device Logical representation of the device to create the swapchain for
  *
  */
  void set_context(VkInstance i, VkPhysicalDevice p, VkDevice d) {
    instance = i;
    physical_device = p;
    device = d;
  }

  void create_image_view(const VkDevice &device, const VkImage& image,
                         const VkFormat &format, VkImageView *view) {
    VkImageViewCreateInfo view_create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_R,
        .g = VK_COMPONENT_SWIZZLE_G,
        .b = VK_COMPONENT_SWIZZLE_B,
        .a = VK_COMPONENT_SWIZZLE_A
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      }
    };

    vik_log_check(vkCreateImageView(device, &view_create_info, nullptr, view));
  }
};
}  // namespace vik
