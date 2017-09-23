#pragma once

#include <vulkan/vulkan.h>

#include "vikSwapchain.hpp"

#define MAX_NUM_IMAGES 4

namespace vkc {

struct RenderBuffer {
  VkImage image;
  VkImageView view;
  VkFramebuffer framebuffer;
};

class SwapChain : public vik::SwapChain {
public:
  RenderBuffer buffers[MAX_NUM_IMAGES];

  SwapChain() {}
  ~SwapChain() {}

  void init_buffer(VkDevice device, VkFormat image_format, VkRenderPass render_pass,
                   uint32_t width, uint32_t height, RenderBuffer *b) {

    VkImageSubresourceRange subresource_range;
    subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_range.baseMipLevel = 0;
    subresource_range.levelCount = 1;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount = 1;

    VkImageViewCreateInfo imageviewinfo;
    imageviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageviewinfo.image = b->image;
    imageviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageviewinfo.format = image_format;
    imageviewinfo.components = {
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A
    };
    imageviewinfo.subresourceRange = subresource_range;

    vkCreateImageView(device,
                      &imageviewinfo,
                      NULL,
                      &b->view);

    VkFramebufferCreateInfo framebufferinfo;
    framebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferinfo.renderPass = render_pass;
    framebufferinfo.attachmentCount = 1;
    framebufferinfo.pAttachments = &b->view;
    framebufferinfo.width = width;
    framebufferinfo.height = height;
    framebufferinfo.layers = 1;

    vkCreateFramebuffer(device,
                        &framebufferinfo,
                        NULL,
                        &b->framebuffer);
  }
};
}
