#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <functional>

#include "system/vikLog.hpp"

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

  std::function<void(uint32_t index)> render_cb;

  SwapChain() {}
  ~SwapChain() {}

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
    VkImageViewCreateInfo view_create_info = {};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.pNext = NULL;
    view_create_info.format = format;
    view_create_info.components = {
      VK_COMPONENT_SWIZZLE_R,
      VK_COMPONENT_SWIZZLE_G,
      VK_COMPONENT_SWIZZLE_B,
      VK_COMPONENT_SWIZZLE_A
    };
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.flags = 0;
    view_create_info.image = image;

    vik_log_check(vkCreateImageView(device, &view_create_info, nullptr, view));
  }

};
}
