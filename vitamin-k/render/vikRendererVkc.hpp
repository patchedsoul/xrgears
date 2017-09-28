#pragma once

#include <assert.h>
#include <sys/time.h>

#include <vulkan/vulkan.h>

#include <array>

#include "vikRenderer.hpp"

namespace vik {

class RendererVkc : public Renderer {
public:
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  VkDeviceMemory mem;
  VkBuffer buffer;
  VkDescriptorSet descriptor_set;
  VkSemaphore semaphore;
  VkFence fence;

  timeval start_tv;

  uint32_t vertex_offset, colors_offset, normals_offset;

  std::function<void()> init_cb;

  void set_init_cb(std::function<void()> cb) {
    init_cb = cb;
  }

  RendererVkc(Settings *s, Window *w) : Renderer(s, w) {
    width = s->width;
    height = s->height;
    gettimeofday(&start_tv, NULL);

    window->set_recreate_frame_buffers_cb([this]() {
      create_frame_buffers();
      allocate_command_buffers();
      for (int i = 0; i < window->get_swap_chain()->image_count; i++)
        build_command_buffer(cmd_buffers[i], frame_buffers[i]);
    });

    auto dimension_cb = [this](uint32_t w, uint32_t h) {
      width = w;
      height = h;
    };

    window->set_dimension_cb(dimension_cb);

    auto expose_cb = [this](uint32_t w, uint32_t h) {
      width = w;
      height = h;
      create_frame_buffers();
      allocate_command_buffers();
      for (int i = 0; i < window->get_swap_chain()->image_count; i++)
        build_command_buffer(cmd_buffers[i], frame_buffers[i]);
    };

    window->set_expose_cb(expose_cb);

  }

  ~RendererVkc() {
  }

  void init(const std::string& name) {
    init_vulkan(name, window->required_extensions());
    init_vk_objects();
    window->init(width, height);
    window->update_window_title(name);
    if (!window->check_support(physical_device))
      vik_log_f("Vulkan not supported on given surface");
    window->get_swap_chain()->set_context(instance, physical_device, device);
    window->init_swap_chain(width, height);

    auto render_cb = [this](uint32_t index) { render(index); };
    window->get_swap_chain()->set_render_cb(render_cb);

    init_render_pass(window->get_swap_chain()->surface_format.format);

    init_cb();

    create_frame_buffers();
    allocate_command_buffers();
    for (int i = 0; i < window->get_swap_chain()->image_count; i++)
      build_command_buffer(cmd_buffers[i], frame_buffers[i]);
  }

  void init_vulkan(const std::string& name,
                   const std::vector<const char*> &window_extensions) {

    create_instance(name, window_extensions);
    uint32_t count;
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    VkPhysicalDevice pd[count];
    vkEnumeratePhysicalDevices(instance, &count, pd);
    physical_device = pd[0];
    vik_log_d("%d physical devices", count);

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    vik_log_i("vendor id %04x, device name %s",
           properties.vendorID, properties.deviceName);

    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, NULL);
    assert(count > 0);
    VkQueueFamilyProperties props[count];
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, props);
    assert(props[0].queueFlags & VK_QUEUE_GRAPHICS_BIT);

    float prois[] = { 1.0f };

    VkDeviceQueueCreateInfo queueinfo = {};
    queueinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueinfo.queueFamilyIndex = 0;
    queueinfo.queueCount = 1;
    queueinfo.pQueuePriorities = prois;

    const char * const extnames[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueinfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = extnames;

    vkCreateDevice(physical_device,
                   &deviceInfo,
                   NULL,
                   &device);

    vkGetDeviceQueue(device, 0, 0, &queue);
  }

  void init_render_pass(VkFormat format) {

    std::array<VkAttachmentDescription, 1> attachments = {};
    attachments[0].format = format;
    attachments[0].samples = (VkSampleCountFlagBits) 1;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    std::array<VkAttachmentReference, 1> color_attachments = {};
    color_attachments[0].attachment = 0;
    color_attachments[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentReference, 1> resolve_attachments = {};
    resolve_attachments[0].attachment = VK_ATTACHMENT_UNUSED;
    resolve_attachments[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    uint32_t presereve_attachments[] = { 0 };

    std::array<VkSubpassDescription, 1> sub_pass_desc = {};
    sub_pass_desc[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub_pass_desc[0].inputAttachmentCount = 0;
    sub_pass_desc[0].colorAttachmentCount = 1;
    sub_pass_desc[0].pColorAttachments = color_attachments.data();
    sub_pass_desc[0].pResolveAttachments = resolve_attachments.data();
    sub_pass_desc[0].pDepthStencilAttachment = NULL;
    sub_pass_desc[0].preserveAttachmentCount = 1;
    sub_pass_desc[0].pPreserveAttachments = presereve_attachments;

    VkRenderPassCreateInfo render_pass_create_info = {};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments = attachments.data();
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = sub_pass_desc.data();
    render_pass_create_info.dependencyCount = 0;

    vkCreateRenderPass(device,
                       &render_pass_create_info,
                       NULL,
                       &render_pass);
  }

  void init_vk_objects() {
    VkFenceCreateInfo fenceinfo = {};
    fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceinfo.flags = 0;

    vkCreateFence(device,
                  &fenceinfo,
                  NULL,
                  &fence);

    VkCommandPoolCreateInfo commandpoolinfo = {};
    commandpoolinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandpoolinfo.queueFamilyIndex = 0;
    commandpoolinfo.flags = 0;

    vkCreateCommandPool(device,
                        &commandpoolinfo,
                        NULL,
                        &cmd_pool);

    vik_log_e("Creating command pool.");

    VkSemaphoreCreateInfo semaphoreinfo = {};
    semaphoreinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    vkCreateSemaphore(device,
                      &semaphoreinfo,
                      NULL,
                      &semaphore);
  }

  void submit_queue(VkCommandBuffer cmd_buffer) {
    VkPipelineStageFlags stageflags[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &semaphore;
    submitInfo.pWaitDstStageMask = stageflags;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd_buffer;

    vkQueueSubmit(queue, 1, &submitInfo, fence);
  }

  static uint64_t get_ms_from_tv(const timeval& tv) {
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
  }

  uint64_t get_animation_time() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return (get_ms_from_tv(tv) - get_ms_from_tv(start_tv)) / 5;
  }

  void build_command_buffer(VkCommandBuffer cmd_buffer,
                            VkFramebuffer frame_buffer) {
    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.flags = 0;

    VkResult r = vkBeginCommandBuffer(cmd_buffer, &cmdBufferBeginInfo);
    vik_log_e_if(r != VK_SUCCESS, "vkBeginCommandBuffer: %s",
                 Log::result_string(r).c_str());

    std::array<VkClearValue,1> clearValues = {};
    clearValues[0].color = { { 0.2f, 0.2f, 0.2f, 1.0f } };

    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = render_pass;
    passBeginInfo.framebuffer = frame_buffer;
    passBeginInfo.renderArea = { { 0, 0 }, { width, height } };
    passBeginInfo.clearValueCount = 1;
    passBeginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd_buffer,
                         &passBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    VkBuffer buffers[] = {
      buffer,
      buffer,
      buffer
    };

    VkDeviceSize offsets[] = {
      vertex_offset,
      colors_offset,
      normals_offset
    };

    vkCmdBindVertexBuffers(cmd_buffer, 0, 3,
                           buffers,
                           offsets);

    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdBindDescriptorSets(cmd_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout,
                            0, 1,
                            &descriptor_set, 0, NULL);

    const VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = (float)width,
      .height = (float)height,
      .minDepth = 0,
      .maxDepth = 1,
    };
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

    const VkRect2D scissor = {
      .offset = { 0, 0 },
      .extent = { width, height },
    };
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    vkCmdDraw(cmd_buffer, 4, 1, 0, 0);
    vkCmdDraw(cmd_buffer, 4, 1, 4, 0);
    vkCmdDraw(cmd_buffer, 4, 1, 8, 0);
    vkCmdDraw(cmd_buffer, 4, 1, 12, 0);
    vkCmdDraw(cmd_buffer, 4, 1, 16, 0);
    vkCmdDraw(cmd_buffer, 4, 1, 20, 0);

    vkCmdEndRenderPass(cmd_buffer);

    r = vkEndCommandBuffer(cmd_buffer);
    vik_log_e_if(r != VK_SUCCESS,
                 "vkEndCommandBuffer: %s", Log::result_string(r).c_str());
  }

  void iterate() {
    window->iterate(queue, semaphore);
  }

  void render(uint32_t index) {
    submit_queue(cmd_buffers[index]);
    VkFence fences[] = { fence };
    vkWaitForFences(device, 1, fences, VK_TRUE, INT64_MAX);
    vkResetFences(device, 1, &fence);
  }

  void create_frame_buffers() {
    vik_log_e("create_frame_buffers: %d", window->get_swap_chain()->image_count);

    uint32_t count = window->get_swap_chain()->image_count;
    frame_buffers.resize(count);
    for (uint32_t i = 0; i < count; i++) {
      std::vector<VkImageView> attachments = {
        window->get_swap_chain()->buffers[i].view
      };
      create_frame_buffer(&frame_buffers[i], attachments);
    }
  }
};
}
