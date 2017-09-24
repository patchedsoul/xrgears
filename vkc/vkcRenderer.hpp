#pragma once

#include "vikSwapChainVK.hpp"
#include "vikSwapChainDRM.hpp"
#include "vikRenderer.hpp"

namespace vkc {

class Renderer : public vik::Renderer {
public:

  vik::SwapChain *swap_chain;

  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  VkDeviceMemory mem;
  VkBuffer buffer;
  VkDescriptorSet descriptor_set;
  VkSemaphore semaphore;
  VkFence fence;
  VkCommandPool cmd_pool;

  timeval start_tv;

  VkCommandBuffer cmd_buffer;

  uint32_t vertex_offset, colors_offset, normals_offset;

  Renderer(uint32_t w, uint32_t h) ;
  ~Renderer() ;

  void init_vk(const char *extension);
  void init_render_pass(VkFormat format);
  void init_vk_objects();
  void submit_queue();

  void wait_and_reset_fences();

  uint64_t get_animation_time();

  void build_command_buffer(VkFramebuffer frame_buffer);

  void render(uint32_t index);

  void render_swapchain_vk(vik::SwapChainVK *swap_chain);

  void create_frame_buffer(VkImageView *view, VkFramebuffer *frame_buffer) {
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

  void create_frame_buffers(vik::SwapChain *swap_chain) {
    frame_buffers.resize(swap_chain->image_count);
    for (uint32_t i = 0; i < swap_chain->image_count; i++) {
      create_frame_buffer(&swap_chain->buffers[i].view, &frame_buffers[i]);
    }
  }

};
}
