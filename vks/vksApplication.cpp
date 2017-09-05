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
}

Application::~Application() {
  // Clean up Vulkan resources
  swapChain.cleanup();
  if (descriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
  }
  destroyCommandBuffers();
  vkDestroyRenderPass(device, renderPass, nullptr);
  for (uint32_t i = 0; i < frameBuffers.size(); i++) {
    vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
  }

  for (auto& shaderModule : shaderModules) {
    vkDestroyShaderModule(device, shaderModule, nullptr);
  }
  vkDestroyImageView(device, depthStencil.view, nullptr);
  vkDestroyImage(device, depthStencil.image, nullptr);
  vkFreeMemory(device, depthStencil.mem, nullptr);

  vkDestroyPipelineCache(device, pipelineCache, nullptr);

  vkDestroyCommandPool(device, cmdPool, nullptr);

  vkDestroySemaphore(device, semaphores.presentComplete, nullptr);
  vkDestroySemaphore(device, semaphores.renderComplete, nullptr);
  vkDestroySemaphore(device, semaphores.textOverlayComplete, nullptr);

  if (enableTextOverlay)
    delete textOverlay;

  delete vksDevice;

  if (settings.validation)
    vks::debug::freeDebugCallback(renderer->instance);

  vkDestroyInstance(renderer->instance, nullptr);
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
      if (endptr != args[i + 1]) width = w;
    }
    if ((args[i] == std::string("-g")) || (args[i] == std::string("-gpu"))) {
      char* endptr;
      uint32_t gpu_index = strtol(args[i + 1], &endptr, 10);
      if (endptr != args[i + 1]) settings.gpu_index = gpu_index;
    }
    if ((args[i] == std::string("-h")) || (args[i] == std::string("-height"))) {
      char* endptr;
      uint32_t h = strtol(args[i + 1], &endptr, 10);
      if (endptr != args[i + 1]) height = h;
    }
    // List available GPUs
    if (args[i] == std::string("-listgpus"))
      settings.list_gpus_and_exit = true;
  }
}

std::string Application::getWindowTitle() {
  std::string device(deviceProperties.deviceName);
  std::string windowTitle = title + " - " + device;
  if (!enableTextOverlay)
    windowTitle += " - " + std::to_string(timer.frames_since_tick) + " fps";
  return windowTitle;
}

void Application::check_view_update() {
  if (viewUpdated) {
    viewUpdated = false;
    viewChanged();
  }
}

bool Application::checkCommandBuffers() {
  for (auto& cmdBuffer : drawCmdBuffers)
    if (cmdBuffer == VK_NULL_HANDLE)
      return false;
  return true;
}

void Application::createCommandBuffers() {
  // Create one command buffer for each swap chain image and reuse for rendering

  vik_log_d("Swapchain image count %d", swapChain.imageCount);

  drawCmdBuffers.resize(swapChain.imageCount);

  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(
        cmdPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        static_cast<uint32_t>(drawCmdBuffers.size()));

  vik_log_check(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));

  vik_log_d("created %ld command buffers", drawCmdBuffers.size());
}

void Application::destroyCommandBuffers() {
  vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

VkCommandBuffer Application::createCommandBuffer(VkCommandBufferLevel level, bool begin) {
  VkCommandBuffer cmdBuffer;

  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(
        cmdPool,
        level,
        1);

  vik_log_check(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

  // If requested, also start the new command buffer
  if (begin) {
    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
    vik_log_check(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
  }

  return cmdBuffer;
}

void Application::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free) {
  if (commandBuffer == VK_NULL_HANDLE)
    return;

  vik_log_check(vkEndCommandBuffer(commandBuffer));

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vik_log_check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
  vik_log_check(vkQueueWaitIdle(queue));

  if (free)
    vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
}

void Application::createPipelineCache() {
  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  vik_log_check(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void Application::prepare() {
  if (vksDevice->enableDebugMarkers)
    vks::debugmarker::setup(device);
  createCommandPool();
  // TODO: create DRM swapchain here

  swapChain.create(&width, &height, settings.vsync);

  createCommandBuffers();
  setupDepthStencil();
  setupRenderPass();
  createPipelineCache();
  setupFrameBuffer();

  if (enableTextOverlay) {
    // Load the text rendering shaders
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(VikShader::load(device, "base/textoverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
    shaderStages.push_back(VikShader::load(device, "base/textoverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

    textOverlay = new TextOverlay(
          vksDevice,
          queue,
          &frameBuffers,
          swapChain.colorFormat,
          depthFormat,
          &width,
          &height,
          shaderStages);
    updateTextOverlay();
  }

  vik_log_d("prepare done");
}


void Application::loop(Window *window) {
  destWidth = width;
  destHeight = height;

  while (!quit) {
    timer.start();
    check_view_update();

    window->flush(this);

    render();
    timer.increment();
    float frame_time = timer.update_frame_time();
    update_camera(frame_time);
    timer.update_animation_timer();
    check_tick_finnished(window);
  }

  // Flush device to make sure all resources can be freed
  vkDeviceWaitIdle(device);
}

void Application::update_camera(float frame_time) {
  camera.update(frame_time);
  if (camera.moving())
    viewUpdated = true;
}

void Application::check_tick_finnished(Window *window) {
  if (timer.tick_finnished()) {
    timer.update_fps();
    if (!enableTextOverlay)
      window->update_window_title(getWindowTitle());
    else
      updateTextOverlay();
    timer.reset();
  }
}

void Application::updateTextOverlay() {
  if (!enableTextOverlay)
    return;

  std::stringstream ss;
  ss << std::fixed
     << std::setprecision(3)
     << (timer.frame_time_seconds * 1000.0f)
     << "ms (" << timer.frames_per_second
     << " fps)";
  std::string deviceName(deviceProperties.deviceName);

  textOverlay->update(title, ss.str(), deviceName);
}


void Application::prepareFrame() {
  // Acquire the next image from the swap chain
  VkResult err = swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer);
  // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
  if ((err == VK_ERROR_OUT_OF_DATE_KHR) || (err == VK_SUBOPTIMAL_KHR))
    windowResize();
  else
    vik_log_check(err);
}

void Application::submit_text_overlay() {
  // Wait for color attachment output to finish before rendering the text overlay
  VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submitInfo.pWaitDstStageMask = &stageFlags;

  // Set semaphores
  // Wait for render complete semaphore
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &semaphores.renderComplete;
  // Signal ready with text overlay complete semaphpre
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &semaphores.textOverlayComplete;

  // Submit current text overlay command buffer
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &textOverlay->cmdBuffers[currentBuffer];
  vik_log_check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

  // Reset stage mask
  submitInfo.pWaitDstStageMask = &submitPipelineStages;
  // Reset wait and signal semaphores for rendering next frame
  // Wait for swap chain presentation to finish
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &semaphores.presentComplete;
  // Signal ready with offscreen semaphore
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &semaphores.renderComplete;
}

void Application::submitFrame() {
  VkSemaphore waitSemaphore;
  if (enableTextOverlay && textOverlay->visible) {
    submit_text_overlay();
    waitSemaphore = semaphores.textOverlayComplete;
  } else {
    waitSemaphore = semaphores.renderComplete;
  }

  vik_log_check(swapChain.queuePresent(queue, currentBuffer, waitSemaphore));
  vik_log_check(vkQueueWaitIdle(queue));
}

void Application::init_physical_device() {
  VkResult err;

  // Physical device
  uint32_t gpuCount = 0;
  // Get number of available physical devices
  vik_log_check(vkEnumeratePhysicalDevices(renderer->instance, &gpuCount, nullptr));
  assert(gpuCount > 0);
  // Enumerate devices
  std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
  err = vkEnumeratePhysicalDevices(renderer->instance, &gpuCount, physicalDevices.data());

  vik_log_f_if(err, "Could not enumerate physical devices: %s", vks::tools::errorString(err).c_str());

  // GPU selection
  if (settings.list_gpus_and_exit) {
    list_gpus();
    exit(0);
  }

  // Select physical device to be used for the Vulkan example
  // Defaults to the first device unless specified by command line
  uint32_t selectedDevice = 0;
  if (settings.gpu_index > gpuCount - 1) {
    std::cerr << "Selected device index " << settings.gpu_index
              << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)"
              << std::endl;
  } else if (settings.gpu_index != 0) {
    vik_log_i("Selected Vulkan device %d", settings.gpu_index);
    selectedDevice = settings.gpu_index;
  }

  physicalDevice = physicalDevices[selectedDevice];
}

void Application::list_gpus() {
  uint32_t gpuCount = 0;
  vik_log_check(vkEnumeratePhysicalDevices(renderer->instance, &gpuCount, nullptr));
  if (gpuCount == 0) {
    vik_log_e("No Vulkan devices found!");
  } else {
    // Enumerate devices
    vik_log_i("Available Vulkan devices");
    std::vector<VkPhysicalDevice> devices(gpuCount);
    vik_log_check(vkEnumeratePhysicalDevices(renderer->instance, &gpuCount, devices.data()));
    for (uint32_t i = 0; i < gpuCount; i++) {
      VkPhysicalDeviceProperties deviceProperties;
      vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
      vik_log_i("Device [%d] : %s", i, deviceProperties.deviceName);
      vik_log_i(" Type: %s", vks::tools::physicalDeviceTypeString(deviceProperties.deviceType).c_str());
      vik_log_i(" API: %d.%d.%d", deviceProperties.apiVersion >> 22, (deviceProperties.apiVersion >> 12) & 0x3ff, deviceProperties.apiVersion & 0xfff);
    }
  }
}

void Application::get_physical_device_properties() {
  // Store properties (including limits), features and memory properties
  // of the phyiscal device (so that examples can check against them)
  vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
  vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
}

void Application::init_debugging() {
  // The report flags determine what type of messages for the layers will be displayed
  // For validating (debugging) an appplication the error and warning bits should suffice
  VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
  // Additional flags include performance info, loader and layer debug messages, etc.
  vks::debug::setupDebugging(renderer->instance, debugReportFlags, VK_NULL_HANDLE);
}

void Application::initVulkan(Window *window) {
  VkResult err;

  // Vulkan instance
  err = renderer->createInstance(&settings, window, name);

  vik_log_f_if(err, "Could not create Vulkan instance: %s", vks::tools::errorString(err).c_str());

  // If requested, we enable the default validation layers for debugging
  if (settings.validation)
    init_debugging();

  init_physical_device();

  get_physical_device_properties();

  // Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
  getEnabledFeatures();

  // Vulkan device creation
  // This is handled by a separate class that gets a logical device representation
  // and encapsulates functions related to a device
  vksDevice = new vks::Device(physicalDevice);

  /*
  enabledExtensions.push_back(VK_KHX_MULTIVIEW_EXTENSION_NAME);
  enabledExtensions.push_back(VK_NVX_MULTIVIEW_PER_VIEW_ATTRIBUTES_EXTENSION_NAME);
  enabledExtensions.push_back(VK_NV_VIEWPORT_ARRAY2_EXTENSION_NAME);
  enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  */

  VkResult res = vksDevice->createLogicalDevice(enabledFeatures, enabledExtensions);
  vik_log_f_if(res != VK_SUCCESS, "Could not create Vulkan device: %s", vks::tools::errorString(res).c_str());

  device = vksDevice->logicalDevice;

  //vksDevice->printMultiviewProperties();

  // Get a graphics queue from the device
  vkGetDeviceQueue(device, vksDevice->queueFamilyIndices.graphics, 0, &queue);

  // Find a suitable depth format
  VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
  assert(validDepthFormat);

  swapChain.connect(renderer->instance, physicalDevice, device);

  init_semaphores();

}

void Application::init_semaphores() {
  // Create synchronization objects
  VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
  // Create a semaphore used to synchronize image presentation
  // Ensures that the image is displayed before we start submitting new commands to the queu
  vik_log_check(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
  // Create a semaphore used to synchronize command submission
  // Ensures that the image is not presented until all commands have been sumbitted and executed
  vik_log_check(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));
  // Create a semaphore used to synchronize command submission
  // Ensures that the image is not presented until all commands for the text overlay have been sumbitted and executed
  // Will be inserted after the render complete semaphore if the text overlay is enabled
  vik_log_check(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.textOverlayComplete));

  // Set up submit info structure
  // Semaphores will stay the same during application lifetime
  // Command buffer submission info is set by each example
  submitInfo = vks::initializers::submitInfo();
  submitInfo.pWaitDstStageMask = &submitPipelineStages;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &semaphores.presentComplete;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &semaphores.renderComplete;
}

void Application::viewChanged() {}

void Application::keyPressed(uint32_t) {}

void Application::buildCommandBuffers() {}

void Application::createCommandPool() {
  VkCommandPoolCreateInfo cmdPoolInfo = {};
  cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
  cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  vik_log_check(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
}

void Application::setupDepthStencil() {
  VkImageCreateInfo image = {};
  image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image.pNext = NULL;
  image.imageType = VK_IMAGE_TYPE_2D;
  image.format = depthFormat;
  image.extent = { width, height, 1 };
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
  depthStencilView.format = depthFormat;
  depthStencilView.flags = 0;
  depthStencilView.subresourceRange = {};
  depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  depthStencilView.subresourceRange.baseMipLevel = 0;
  depthStencilView.subresourceRange.levelCount = 1;
  depthStencilView.subresourceRange.baseArrayLayer = 0;
  depthStencilView.subresourceRange.layerCount = 1;

  VkMemoryRequirements memReqs;

  vik_log_check(vkCreateImage(device, &image, nullptr, &depthStencil.image));
  vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);
  mem_alloc.allocationSize = memReqs.size;
  mem_alloc.memoryTypeIndex = vksDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vik_log_check(vkAllocateMemory(device, &mem_alloc, nullptr, &depthStencil.mem));
  vik_log_check(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

  depthStencilView.image = depthStencil.image;
  vik_log_check(vkCreateImageView(device, &depthStencilView, nullptr, &depthStencil.view));
}

void Application::setupFrameBuffer() {
  vik_log_d("setupFrameBuffer");


  VkImageView attachments[2];

  // Depth/Stencil attachment is the same for all frame buffers
  attachments[1] = depthStencil.view;

  VkFramebufferCreateInfo frameBufferCreateInfo = {};
  frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  frameBufferCreateInfo.pNext = NULL;
  frameBufferCreateInfo.renderPass = renderPass;
  frameBufferCreateInfo.attachmentCount = 2;
  frameBufferCreateInfo.pAttachments = attachments;
  frameBufferCreateInfo.width = width;
  frameBufferCreateInfo.height = height;
  frameBufferCreateInfo.layers = 1;

  // Create frame buffers for every swap chain image
  frameBuffers.resize(swapChain.imageCount);
  for (uint32_t i = 0; i < frameBuffers.size(); i++) {
    attachments[0] = swapChain.buffers[i].view;
    vik_log_check(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
  }
}

void Application::setupRenderPass() {
  std::array<VkAttachmentDescription, 2> attachments = {};
  // Color attachment
  attachments[0].format = swapChain.colorFormat;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  // Depth attachment
  attachments[1].format = depthFormat;
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
  // VK_KHX_multiview

  vik_log_check(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
  vik_log_d("renderpass setup complete");
}

void Application::getEnabledFeatures() {}

void Application::windowResize() {
  if (!prepared)
    return;
  prepared = false;

  // Ensure all operations on the device have been finished before destroying resources
  vkDeviceWaitIdle(device);

  // Recreate swap chain
  width = destWidth;
  height = destHeight;
  // TODO: Create kms swapchain here.

  swapChain.create(&width, &height, settings.vsync);
  // Recreate the frame buffers

  vkDestroyImageView(device, depthStencil.view, nullptr);
  vkDestroyImage(device, depthStencil.image, nullptr);
  vkFreeMemory(device, depthStencil.mem, nullptr);
  setupDepthStencil();

  for (uint32_t i = 0; i < frameBuffers.size(); i++)
    vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
  setupFrameBuffer();

  // Command buffers need to be recreated as they may store
  // references to the recreated frame buffer
  destroyCommandBuffers();
  createCommandBuffers();
  buildCommandBuffers();

  vkDeviceWaitIdle(device);

  if (enableTextOverlay) {
    textOverlay->reallocateCommandBuffers();
    updateTextOverlay();
  }

  camera.updateAspectRatio((float)width / (float)height);

  // Notify derived class
  //windowResized();
  viewChanged();

  prepared = true;
}

}
