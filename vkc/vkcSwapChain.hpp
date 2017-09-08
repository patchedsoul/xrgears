#pragma once

#include <vulkan/vulkan.h>

#define MAX_NUM_IMAGES 4

namespace vkc {

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

    void init_buffer(VkDevice device, VkFormat image_format, VkRenderPass render_pass,
                     uint32_t width, uint32_t height, RenderBuffer *b) {
  VkImageViewCreateInfo imageviewinfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = b->image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = image_format,
      .components = {
          .r = VK_COMPONENT_SWIZZLE_R,
          .g = VK_COMPONENT_SWIZZLE_G,
          .b = VK_COMPONENT_SWIZZLE_B,
          .a = VK_COMPONENT_SWIZZLE_A,
      },
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
  };

  vkCreateImageView(device,
                    &imageviewinfo,
                    NULL,
                    &b->view);


  VkFramebufferCreateInfo framebufferinfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = render_pass,
      .attachmentCount = 1,
      .pAttachments = &b->view,
      .width = width,
      .height = height,
      .layers = 1
  };

  vkCreateFramebuffer(device,
                      &framebufferinfo,
                      NULL,
                      &b->framebuffer);
    }


};
}
