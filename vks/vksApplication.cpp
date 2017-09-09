/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <vector>
#include <string>

#include "vksApplication.hpp"

#include "vksWindowXCB.hpp"
#include "../vitamin-k/VikShader.hpp"

namespace vks {

Application::Application() {
  // Check for a valid asset path
  struct stat info;
  if (stat(VikAssets::getAssetPath().c_str(), &info) != 0)
    vik_log_f("Error: Could not find asset path in %s", VikAssets::getAssetPath().c_str());

  renderer = new Renderer();

  std::function<void()> set_window_resize_cb = [this]() { windowResize(); };
  std::function<void()> enabled_features_cb = [this]() { getEnabledFeatures(); };

  renderer->set_enabled_features_cb(enabled_features_cb);
  renderer->set_window_resize_cb(set_window_resize_cb);
}

Application::~Application() {
  delete renderer;
}

void Application::parse_arguments(const int argc, const char *argv[]) {
  std::vector<const char*> args;
  for (size_t i = 0; i < argc; i++) { args.push_back(argv[i]); };

  // Parse command line arguments
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == std::string("-validation"))
      settings.validation = true;
    if (args[i] == std::string("-vsync"))
      settings.vsync = true;
    if (args[i] == std::string("-fullscreen"))
      settings.fullscreen = true;
    if ((args[i] == std::string("-w")) || (args[i] == std::string("-width"))) {
      char* endptr;
      uint32_t w = strtol(args[i + 1], &endptr, 10);
      if (endptr != args[i + 1]) renderer->width = w;
    }
    if ((args[i] == std::string("-g")) || (args[i] == std::string("-gpu"))) {
      char* endptr;
      uint32_t gpu_index = strtol(args[i + 1], &endptr, 10);
      if (endptr != args[i + 1]) settings.gpu_index = gpu_index;
    }
    if ((args[i] == std::string("-h")) || (args[i] == std::string("-height"))) {
      char* endptr;
      uint32_t h = strtol(args[i + 1], &endptr, 10);
      if (endptr != args[i + 1]) renderer->height = h;
    }
    // List available GPUs
    if (args[i] == std::string("-listgpus"))
      settings.list_gpus_and_exit = true;
  }
}

void Application::check_view_update() {
  if (viewUpdated) {
    viewUpdated = false;
    viewChanged();
  }
}

void Application::prepare() {
  if (renderer->vksDevice->enableDebugMarkers)
    vks::debugmarker::setup(renderer->device);
  renderer->createCommandPool();
  // TODO: create DRM swapchain here

  renderer->swapChain.create(&renderer->width, &renderer->height, settings.vsync);

  renderer->createCommandBuffers();
  setupDepthStencil();
  setupRenderPass();
  renderer->createPipelineCache();
  setupFrameBuffer();

  renderer->init_text_overlay(title);

  vik_log_d("prepare done");
}

void Application::loop(Window *window) {
  renderer->destWidth = renderer->width;
  renderer->destHeight = renderer->height;

  while (!quit) {
    renderer->timer.start();
    check_view_update();

    window->flush(this);

    render();
    renderer->timer.increment();
    float frame_time = renderer->timer.update_frame_time();
    update_camera(frame_time);
    renderer->timer.update_animation_timer();
    renderer->check_tick_finnished(window, title);
  }

  // Flush device to make sure all resources can be freed
  vkDeviceWaitIdle(renderer->device);
}

void Application::update_camera(float frame_time) {
  camera.update(frame_time);
  if (camera.moving())
    viewUpdated = true;
}

void Application::viewChanged() {}
void Application::keyPressed(uint32_t) {}
void Application::buildCommandBuffers() {}

void Application::setupDepthStencil() {
  VkImageCreateInfo image = {};
  image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image.pNext = NULL;
  image.imageType = VK_IMAGE_TYPE_2D;
  image.format = renderer->depthFormat;
  image.extent = { renderer->width, renderer->height, 1 };
  image.mipLevels = 1;
  image.arrayLayers = 1;
  image.samples = VK_SAMPLE_COUNT_1_BIT;
  image.tiling = VK_IMAGE_TILING_OPTIMAL;
  image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  image.flags = 0;

  VkMemoryAllocateInfo mem_alloc = {};
  mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc.pNext = NULL;
  mem_alloc.allocationSize = 0;
  mem_alloc.memoryTypeIndex = 0;

  VkImageViewCreateInfo depthStencilView = {};
  depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  depthStencilView.pNext = NULL;
  depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depthStencilView.format = renderer->depthFormat;
  depthStencilView.flags = 0;
  depthStencilView.subresourceRange = {};
  depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  depthStencilView.subresourceRange.baseMipLevel = 0;
  depthStencilView.subresourceRange.levelCount = 1;
  depthStencilView.subresourceRange.baseArrayLayer = 0;
  depthStencilView.subresourceRange.layerCount = 1;

  VkMemoryRequirements memReqs;

  vik_log_check(vkCreateImage(renderer->device, &image, nullptr, &renderer->depthStencil.image));
  vkGetImageMemoryRequirements(renderer->device, renderer->depthStencil.image, &memReqs);
  mem_alloc.allocationSize = memReqs.size;
  mem_alloc.memoryTypeIndex = renderer->vksDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vik_log_check(vkAllocateMemory(renderer->device, &mem_alloc, nullptr, &renderer->depthStencil.mem));
  vik_log_check(vkBindImageMemory(renderer->device, renderer->depthStencil.image, renderer->depthStencil.mem, 0));

  depthStencilView.image = renderer->depthStencil.image;
  vik_log_check(vkCreateImageView(renderer->device, &depthStencilView, nullptr, &renderer->depthStencil.view));
}

void Application::getEnabledFeatures() {}

void Application::windowResize() {
  if (!prepared)
    return;
  prepared = false;

  // Ensure all operations on the device have been finished before destroying resources
  vkDeviceWaitIdle(renderer->device);

  // Recreate swap chain
  renderer->width = renderer->destWidth;
  renderer->height = renderer->destHeight;
  // TODO: Create kms swapchain here.

  renderer->swapChain.create(&renderer->width, &renderer->height, settings.vsync);
  // Recreate the frame buffers

  vkDestroyImageView(renderer->device, renderer->depthStencil.view, nullptr);
  vkDestroyImage(renderer->device, renderer->depthStencil.image, nullptr);
  vkFreeMemory(renderer->device, renderer->depthStencil.mem, nullptr);
  setupDepthStencil();

  for (uint32_t i = 0; i < renderer->frameBuffers.size(); i++)
    vkDestroyFramebuffer(renderer->device, renderer->frameBuffers[i], nullptr);
  setupFrameBuffer();

  // Command buffers need to be recreated as they may store
  // references to the recreated frame buffer
  renderer->destroyCommandBuffers();
  renderer->createCommandBuffers();
  buildCommandBuffers();

  vkDeviceWaitIdle(renderer->device);

  if (renderer->enableTextOverlay) {
    renderer->textOverlay->reallocateCommandBuffers();
    renderer->updateTextOverlay(title);
  }

  camera.updateAspectRatio((float)renderer->width / (float)renderer->height);

  // Notify derived class
  //windowResized();
  viewChanged();

  prepared = true;
}

void Application::setupFrameBuffer() {
  vik_log_d("setupFrameBuffer");


  VkImageView attachments[2];

  // Depth/Stencil attachment is the same for all frame buffers
  attachments[1] = renderer->depthStencil.view;

  VkFramebufferCreateInfo frameBufferCreateInfo = {};
  frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  frameBufferCreateInfo.pNext = NULL;
  frameBufferCreateInfo.renderPass = renderer->renderPass;
  frameBufferCreateInfo.attachmentCount = 2;
  frameBufferCreateInfo.pAttachments = attachments;
  frameBufferCreateInfo.width = renderer->width;
  frameBufferCreateInfo.height = renderer->height;
  frameBufferCreateInfo.layers = 1;

  // Create frame buffers for every swap chain image
  renderer->frameBuffers.resize(renderer->swapChain.imageCount);
  for (uint32_t i = 0; i < renderer->frameBuffers.size(); i++) {
    attachments[0] = renderer->swapChain.buffers[i].view;
    vik_log_check(vkCreateFramebuffer(renderer->device, &frameBufferCreateInfo, nullptr, &renderer->frameBuffers[i]));
  }
}

void Application::setupRenderPass() {
  std::array<VkAttachmentDescription, 2> attachments = {};
  // Color attachment
  attachments[0].format = renderer->swapChain.colorFormat;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  // Depth attachment
  attachments[1].format = renderer->depthFormat;
  attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference colorReference = {};
  colorReference.attachment = 0;
  colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthReference = {};
  depthReference.attachment = 1;
  depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpassDescription = {};
  subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpassDescription.colorAttachmentCount = 1;
  subpassDescription.pColorAttachments = &colorReference;
  subpassDescription.pDepthStencilAttachment = &depthReference;
  subpassDescription.inputAttachmentCount = 0;
  subpassDescription.pInputAttachments = nullptr;
  subpassDescription.preserveAttachmentCount = 0;
  subpassDescription.pPreserveAttachments = nullptr;
  subpassDescription.pResolveAttachments = nullptr;

  // Subpass dependencies for layout transitions
  std::array<VkSubpassDependency, 2> dependencies;

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpassDescription;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();


  // VK_KHX_multiview
  /*
  VkRenderPassMultiviewCreateInfoKHX renderPassMvInfo = {};
  renderPassMvInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO_KHX;
  renderPassMvInfo.subpassCount = 1;
  renderPassMvInfo.dependencyCount = 1;
  renderPassMvInfo.correlationMaskCount = 1;
  renderPassMvInfo.pNext = NULL;

  uint32_t correlationMasks[] = { 2 };
  renderPassMvInfo.pCorrelationMasks = correlationMasks;

  uint32_t viewMasks[] = { 1 };
  renderPassMvInfo.pViewMasks = viewMasks;

  int32_t viewOffsets[] = { 2 };
  renderPassMvInfo.pViewOffsets = viewOffsets;

  renderPassInfo.pNext = &renderPassMvInfo;
  renderPassInfo.pNext = NULL;
  */
  // VK_KHX_multiview

  vik_log_check(vkCreateRenderPass(renderer->device, &renderPassInfo, nullptr, &renderer->renderPass));
  vik_log_d("renderpass setup complete");
}

}
