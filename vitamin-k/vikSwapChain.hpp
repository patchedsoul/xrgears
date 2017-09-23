#pragma once

#include <vulkan/vulkan.h>

#include "../vks/vksLog.hpp"

#define MAX_NUM_IMAGES 4

namespace vik {

struct RenderBuffer {
  VkImage image;
  VkImageView view;
  VkFramebuffer framebuffer;
};

class SwapChain {
public:
  RenderBuffer buffers[MAX_NUM_IMAGES];

  SwapChain() {}
  ~SwapChain() {}

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

  void create_frame_buffer(const VkDevice &device,
                           const VkRenderPass &render_pass, VkImageView *view,
                           uint32_t width, uint32_t height,
                           VkFramebuffer *frame_buffer) {
    VkFramebufferCreateInfo framebufferinfo = {};
    framebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferinfo.renderPass = render_pass;
    framebufferinfo.attachmentCount = 1;
    framebufferinfo.pAttachments = view;
    framebufferinfo.width = width;
    framebufferinfo.height = height;
    framebufferinfo.layers = 1;

    vkCreateFramebuffer(device,
                        &framebufferinfo,
                        NULL,
                        frame_buffer);
  }

};
}