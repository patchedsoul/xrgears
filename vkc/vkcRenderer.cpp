#include <assert.h>
#include <sys/time.h>

#include "vkcRenderer.hpp"

namespace vkc {

Renderer::Renderer(uint32_t w, uint32_t h) : width(w), height(h) {
  gettimeofday(&start_tv, NULL);
}

Renderer::~Renderer() {
}

void Renderer::init_vk(const char *extension) {
  VkApplicationInfo appinfo = {};
  appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appinfo.pApplicationName = "vkcube";
  appinfo.apiVersion = VK_MAKE_VERSION(1, 0, 2);

  const char * names[2] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    extension,
  };

  VkInstanceCreateInfo instanceinfo = {};
  instanceinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceinfo.pApplicationInfo = &appinfo;
  instanceinfo.enabledExtensionCount = extension ? (uint32_t) 2 : 0;
  instanceinfo.ppEnabledExtensionNames = names;

  vkCreateInstance(&instanceinfo,
                   NULL,
                   &instance);

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

VkFormat Renderer::choose_surface_format() {
  uint32_t num_formats = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                       &num_formats, NULL);
  assert(num_formats > 0);

  VkSurfaceFormatKHR formats[num_formats];

  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                       &num_formats, formats);

  VkFormat format = VK_FORMAT_UNDEFINED;
  for (int i = 0; i < num_formats; i++) {
    switch (formats[i].format) {
      case VK_FORMAT_R8G8B8A8_SRGB:
      case VK_FORMAT_B8G8R8A8_SRGB:
        /* These formats are all fine */
        format = formats[i].format;
        break;
      case VK_FORMAT_R8G8B8_SRGB:
      case VK_FORMAT_B8G8R8_SRGB:
      case VK_FORMAT_R5G6B5_UNORM_PACK16:
      case VK_FORMAT_B5G6R5_UNORM_PACK16:
        /* We would like to support these but they don't seem to work. */
      default:
        continue;
    }
  }

  assert(format != VK_FORMAT_UNDEFINED);

  return format;
}

void Renderer::init_render_pass() {
  VkAttachmentDescription attachement_desc[] = {
    {
      .format = image_format,
      .samples = (VkSampleCountFlagBits) 1,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    }
  };

  VkAttachmentReference color_attachments[] = {
    {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    }
  };

  VkAttachmentReference resolve_attachments[] = {
    {
      .attachment = VK_ATTACHMENT_UNUSED,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    }
  };

  uint32_t presereve_attachments[] = { 0 };

  VkSubpassDescription sub_pass_desc[] = {
    {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .colorAttachmentCount = 1,
      .pColorAttachments = color_attachments,
      .pResolveAttachments = resolve_attachments,
      .pDepthStencilAttachment = NULL,
      .preserveAttachmentCount = 1,
      .pPreserveAttachments = presereve_attachments,
    }
  };

  VkRenderPassCreateInfo render_pass_create_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = attachement_desc,
    .subpassCount = 1,
    .pSubpasses = sub_pass_desc,
    .dependencyCount = 0
  };

  vkCreateRenderPass(device,
                     &render_pass_create_info,
                     NULL,
                     &render_pass);
}

void Renderer::init_vk_objects() {
  VkFenceCreateInfo fenceinfo = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = 0
  };

  vkCreateFence(device,
                &fenceinfo,
                NULL,
                &fence);


  const VkCommandPoolCreateInfo commandpoolinfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = 0,
    .flags = 0
  };

  vkCreateCommandPool(device,
                      &commandpoolinfo,
                      NULL,
                      &cmd_pool);

  VkSemaphoreCreateInfo semaphoreinfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  vkCreateSemaphore(device,
                    &semaphoreinfo,
                    NULL,
                    &semaphore);
}

void Renderer::submit_queue() {
  VkPipelineStageFlags stageflags[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
  };

  VkSubmitInfo submitInfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &semaphore,
    .pWaitDstStageMask = stageflags,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd_buffer,
  };

  vkQueueSubmit(queue, 1, &submitInfo, fence);
}

VkResult Renderer::aquire_next_image(uint32_t *index) {
  SwapChainVK *sc = (SwapChainVK*) swap_chain_obj;
  return vkAcquireNextImageKHR(device, sc->swap_chain, 60,
                                 semaphore, VK_NULL_HANDLE, index);
}

void Renderer::create_swapchain_if_needed() {
  if (swap_chain_obj == nullptr) {
    swap_chain_obj = new SwapChainVK();
    //SwapChainVK *sc = (SwapChainVK*) swap_chain_obj;
    SwapChainVK* sc = (SwapChainVK*) swap_chain_obj;
    sc->init(device, physical_device, surface,
             image_format, width, height, render_pass);
  }
}

static uint64_t get_ms_from_tv(const timeval& tv) {
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

uint64_t Renderer::get_animation_time() {
  timeval tv;
  gettimeofday(&tv, NULL);
  return (get_ms_from_tv(tv) - get_ms_from_tv(start_tv)) / 5;
}

void Renderer::wait_and_reset_fences() {
  VkFence fences[] = { fence };
  vkWaitForFences(device, 1, fences, VK_TRUE, INT64_MAX);
  vkResetFences(device, 1, &fence);
  vkResetCommandPool(device, cmd_pool, 0);
}

void Renderer::build_command_buffer(VkFramebuffer frame_buffer) {
  VkCommandBufferAllocateInfo cmdBufferAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = cmd_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  VkResult r = vkAllocateCommandBuffers(device,
                                        &cmdBufferAllocateInfo,
                                        &cmd_buffer);
  vik_log_e_if(r != VK_SUCCESS, "vkAllocateCommandBuffers: %s",
               vks::Log::result_string(r).c_str());

  VkCommandBufferBeginInfo cmdBufferBeginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = 0
  };

  r = vkBeginCommandBuffer(cmd_buffer, &cmdBufferBeginInfo);
  vik_log_e_if(r != VK_SUCCESS, "vkBeginCommandBuffer: %s",
               vks::Log::result_string(r).c_str());

  VkClearValue clearValues[] = {
    { .color = { .float32 = { 0.2f, 0.2f, 0.2f, 1.0f } } }
  };

  VkRenderPassBeginInfo passBeginInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = render_pass,
    .framebuffer = frame_buffer,
    .renderArea = { { 0, 0 }, { width, height } },
    .clearValueCount = 1,
    .pClearValues = clearValues
  };

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
               "vkEndCommandBuffer: %s", vks::Log::result_string(r).c_str());
}

void Renderer::render(uint32_t index) {
  RenderBuffer *b = &swap_chain_obj->buffers[index];
  build_command_buffer(b->framebuffer);
  submit_queue();
  wait_and_reset_fences();
}

void Renderer::render_swapchain_vk() {
  uint32_t index;
  VkResult result = aquire_next_image(&index);

  switch (result) {
    case VK_SUCCESS: {
      render(index);
      SwapChainVK *sc = (SwapChainVK *) swap_chain_obj;
      sc->present(queue, index);
      break;
    }
    case VK_TIMEOUT:
      // TODO: XCB times out
      break;
    default:
      vik_log_e("vkAcquireNextImageKHR failed: %s",
                 vks::Log::result_string(result).c_str());
      break;
  }
}

void Renderer::render_swapchain_drm() {
  render(current & 1);
}

void Renderer::init_buffer(RenderBuffer *b) {
  swap_chain_obj->init_buffer(device, image_format, render_pass,
                              width, height, b);
}
}
