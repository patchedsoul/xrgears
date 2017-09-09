#include "vksRenderer.hpp"
#include "vksWindow.hpp"
#include "VikShader.hpp"

namespace vks {

Renderer::Renderer() {

}
Renderer::~Renderer() {
  if (enableTextOverlay)
    delete textOverlay;

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

  delete vksDevice;

  if (settings->validation)
    vks::debug::freeDebugCallback(instance);

  vkDestroyInstance(instance, nullptr);
}

void Renderer::init_text_overlay(const std::string& title) {
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
    updateTextOverlay(title);
  }
}

VkResult Renderer::createInstance(Window *window, const std::string& name) {

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = name.c_str();
  appInfo.pEngineName = name.c_str();
  appInfo.apiVersion = VK_API_VERSION_1_0;

  std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

  // Enable surface extensions depending on os
  std::string windowExtension = std::string(window->requiredExtensionName());

  if (!windowExtension.empty())
    instanceExtensions.push_back(window->requiredExtensionName());

  instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pNext = NULL;
  instanceCreateInfo.pApplicationInfo = &appInfo;
  if (instanceExtensions.size() > 0) {
    if (settings->validation)
      instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
  }
  if (settings->validation) {
    instanceCreateInfo.enabledLayerCount = vks::debug::validationLayerCount;
    instanceCreateInfo.ppEnabledLayerNames = vks::debug::validationLayerNames;
  }
  return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}

bool Renderer::checkCommandBuffers() {
  for (auto& cmdBuffer : drawCmdBuffers)
    if (cmdBuffer == VK_NULL_HANDLE)
      return false;
  return true;
}

void Renderer::createCommandBuffers() {
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

void Renderer::destroyCommandBuffers() {
  vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

VkCommandBuffer Renderer::createCommandBuffer(VkCommandBufferLevel level, bool begin) {
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

void Renderer::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free) {
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

void Renderer::createPipelineCache() {
  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  vik_log_check(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void Renderer::check_tick_finnished(Window *window, const std::string& title) {
  if (timer.tick_finnished()) {
    timer.update_fps();
    if (!enableTextOverlay)
      window->update_window_title(make_title_string(title));
    else
      updateTextOverlay(title);
    timer.reset();
  }
}

void Renderer::prepareFrame() {
  // Acquire the next image from the swap chain
  VkResult err = swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer);
  // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
  if ((err == VK_ERROR_OUT_OF_DATE_KHR) || (err == VK_SUBOPTIMAL_KHR))
    window_resize_cb();
  else
    vik_log_check(err);
}

void Renderer::submitFrame() {
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

void Renderer::init_physical_device() {
  VkResult err;

  // Physical device
  uint32_t gpuCount = 0;
  // Get number of available physical devices
  vik_log_check(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
  assert(gpuCount > 0);
  // Enumerate devices
  std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
  err = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());

  vik_log_f_if(err, "Could not enumerate physical devices: %s", vks::Log::result_string(err).c_str());

  // GPU selection
  if (settings->list_gpus_and_exit) {
    list_gpus();
    exit(0);
  }

  // Select physical device to be used for the Vulkan example
  // Defaults to the first device unless specified by command line
  uint32_t selectedDevice = 0;
  if (settings->gpu_index > gpuCount - 1) {
    std::cerr << "Selected device index " << settings->gpu_index
              << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)"
              << std::endl;
  } else if (settings->gpu_index != 0) {
    vik_log_i("Selected Vulkan device %d", settings->gpu_index);
    selectedDevice = settings->gpu_index;
  }

  physicalDevice = physicalDevices[selectedDevice];
}

void Renderer::list_gpus() {
  uint32_t gpuCount = 0;
  vik_log_check(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
  if (gpuCount == 0) {
    vik_log_e("No Vulkan devices found!");
  } else {
    // Enumerate devices
    vik_log_i("Available Vulkan devices");
    std::vector<VkPhysicalDevice> devices(gpuCount);
    vik_log_check(vkEnumeratePhysicalDevices(instance, &gpuCount, devices.data()));
    for (uint32_t i = 0; i < gpuCount; i++) {
      VkPhysicalDeviceProperties deviceProperties;
      vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
      vik_log_i("Device [%d] : %s", i, deviceProperties.deviceName);
      vik_log_i(" Type: %s", vks::tools::physicalDeviceTypeString(deviceProperties.deviceType).c_str());
      vik_log_i(" API: %d.%d.%d", deviceProperties.apiVersion >> 22, (deviceProperties.apiVersion >> 12) & 0x3ff, deviceProperties.apiVersion & 0xfff);
    }
  }
}

void Renderer::get_physical_device_properties() {
  // Store properties (including limits), features and memory properties
  // of the phyiscal device (so that examples can check against them)
  vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
  vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
}

void Renderer::init_debugging() {
  // The report flags determine what type of messages for the layers will be displayed
  // For validating (debugging) an appplication the error and warning bits should suffice
  VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
  // Additional flags include performance info, loader and layer debug messages, etc.
  vks::debug::setupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
}

void Renderer::initVulkan(Settings *s, Window *window, const std::string &name) {
  VkResult err;
  settings = s;

  // Vulkan instance
  err = createInstance(window, name);

  vik_log_f_if(err, "Could not create Vulkan instance: %s",
               vks::Log::result_string(err).c_str());

  // If requested, we enable the default validation layers for debugging
  if (settings->validation)
    init_debugging();

  init_physical_device();

  get_physical_device_properties();

  // Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
  enabled_features_cb();

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
  vik_log_f_if(res != VK_SUCCESS, "Could not create Vulkan device: %s", vks::Log::result_string(res).c_str());

  device = vksDevice->logicalDevice;

  //vksDevice->printMultiviewProperties();

  // Get a graphics queue from the device
  vkGetDeviceQueue(device, vksDevice->queueFamilyIndices.graphics, 0, &queue);

  // Find a suitable depth format
  VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
  assert(validDepthFormat);

  swapChain.connect(instance, physicalDevice, device);

  init_semaphores();

}

void Renderer::init_semaphores() {
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

void Renderer::createCommandPool() {
  VkCommandPoolCreateInfo cmdPoolInfo = {};
  cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
  cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  vik_log_check(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
}

std::string Renderer::make_title_string(const std::string& title) {
  std::string device_str(deviceProperties.deviceName);
  std::string windowTitle = title + " - " + device_str;
  if (!enableTextOverlay)
    windowTitle += " - " + std::to_string(timer.frames_since_tick) + " fps";
  return windowTitle;
}

void Renderer::updateTextOverlay(const std::string& title) {
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

void Renderer::submit_text_overlay() {
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

}
