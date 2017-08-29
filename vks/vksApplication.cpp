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

std::vector<const char*> Application::args;

VkResult Application::createInstance(bool enableValidation, VikWindow *window) {
  this->settings.validation = enableValidation;

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

  /*
  instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  enabledExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  enabledExtensions.push_back(VK_KHX_MULTIVIEW_EXTENSION_NAME);
  enabledExtensions.push_back(VK_NVX_MULTIVIEW_PER_VIEW_ATTRIBUTES_EXTENSION_NAME);
  enabledExtensions.push_back(VK_NV_VIEWPORT_ARRAY2_EXTENSION_NAME);
  enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  */

  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pNext = NULL;
  instanceCreateInfo.pApplicationInfo = &appInfo;
  if (instanceExtensions.size() > 0) {
    if (settings.validation) {
      printf("Enabling VK_EXT_DEBUG_REPORT_EXTENSION_NAME\n");
      instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    } else {
      printf("!NOT! Enabling VK_EXT_DEBUG_REPORT_EXTENSION_NAME\n");
    }
    instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
  }
  if (settings.validation) {
    instanceCreateInfo.enabledLayerCount = vks::debug::validationLayerCount;
    instanceCreateInfo.ppEnabledLayerNames = vks::debug::validationLayerNames;
  }
  return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}

std::string Application::getWindowTitle() {
  std::string device(deviceProperties.deviceName);
  std::string windowTitle;
  windowTitle = title + " - " + device;
  if (!enableTextOverlay) {
    windowTitle += " - " + std::to_string(frameCounter) + " fps";
  }
  return windowTitle;
}

const std::string Application::getAssetPath() {
  return "./data/";
}

bool Application::checkCommandBuffers() {
  for (auto& cmdBuffer : drawCmdBuffers)
    if (cmdBuffer == VK_NULL_HANDLE)
      return false;
  return true;
}

void Application::createCommandBuffers() {
  // Create one command buffer for each swap chain image and reuse for rendering

  printf("Swapchain image count %d\n", swapChain.imageCount);

  drawCmdBuffers.resize(swapChain.imageCount);

  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(
        cmdPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        static_cast<uint32_t>(drawCmdBuffers.size()));

  VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
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

  VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

  // If requested, also start the new command buffer
  if (begin) {
    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
  }

  return cmdBuffer;
}

void Application::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free) {
  if (commandBuffer == VK_NULL_HANDLE)
    return;

  VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
  VK_CHECK_RESULT(vkQueueWaitIdle(queue));

  if (free)
    vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
}

void Application::createPipelineCache() {
  VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
  pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void Application::prepare() {
  if (vulkanDevice->enableDebugMarkers)
    vks::debugmarker::setup(device);
  createCommandPool();
  // TODO: create DRM swapchain here

  //setupSwapChain();
  fprintf(stderr, "prepare: not creating swapchain.\n");


  createCommandBuffers();
  setupDepthStencil();
  setupRenderPass();
  createPipelineCache();
  setupFrameBuffer();

  if (enableTextOverlay) {
    // Load the text rendering shaders
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(loadShader(getAssetPath() + "shaders/base/textoverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
    shaderStages.push_back(loadShader(getAssetPath() + "shaders/base/textoverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
    textOverlay = new VulkanTextOverlay(
          vulkanDevice,
          queue,
          &frameBuffers,
          swapChain.colorFormat,
          depthFormat,
          &width,
          &height,
          shaderStages);
    updateTextOverlay();
  }

  fprintf(stderr, "prepare: done.\n");
}

VkPipelineShaderStageCreateInfo Application::loadShader(std::string fileName, VkShaderStageFlagBits stage) {
  VkPipelineShaderStageCreateInfo shaderStage = {};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = stage;
  shaderStage.module = vks::tools::loadShader(fileName.c_str(), device);
  shaderStage.pName = "main";
  assert(shaderStage.module != VK_NULL_HANDLE);
  shaderModules.push_back(shaderStage.module);
  return shaderStage;
}


void Application::renderLoopWrap(VikWindow *window) {
  destWidth = width;
  destHeight = height;

  window->renderLoop(this);

  // Flush device to make sure all resources can be freed
  vkDeviceWaitIdle(device);
}

void Application::updateTextOverlay() {
  if (!enableTextOverlay)
    return;

  textOverlay->beginTextUpdate();

  textOverlay->addText(title, 5.0f, 5.0f, VulkanTextOverlay::alignLeft);

  std::stringstream ss;
  ss << std::fixed << std::setprecision(3) << (frameTimer * 1000.0f) << "ms (" << lastFPS << " fps)";
  textOverlay->addText(ss.str(), 5.0f, 25.0f, VulkanTextOverlay::alignLeft);

  std::string deviceName(deviceProperties.deviceName);
  textOverlay->addText(deviceName, 5.0f, 45.0f, VulkanTextOverlay::alignLeft);

  getOverlayText(textOverlay);

  textOverlay->endTextUpdate();
}

void Application::getOverlayText(VulkanTextOverlay*) {}

void Application::prepareFrame() {
  // Acquire the next image from the swap chain
  VkResult err = swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer);
  // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
  if ((err == VK_ERROR_OUT_OF_DATE_KHR) || (err == VK_SUBOPTIMAL_KHR))
    windowResize();
  else
    VK_CHECK_RESULT(err);
}

void Application::submitFrame() {
  bool submitTextOverlay = enableTextOverlay && textOverlay->visible;

  if (submitTextOverlay) {
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
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

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

  VK_CHECK_RESULT(swapChain.queuePresent(queue, currentBuffer, submitTextOverlay ? semaphores.textOverlayComplete : semaphores.renderComplete));

  VK_CHECK_RESULT(vkQueueWaitIdle(queue));
}

Application::Application(bool enableValidation) {
  // Check for a valid asset path
  struct stat info;
  if (stat(getAssetPath().c_str(), &info) != 0) {
    std::cerr << "Error: Could not find asset path in " << getAssetPath() << std::endl;
    exit(-1);
  }

  settings.validation = enableValidation;

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
    if ((args[i] == std::string("-h")) || (args[i] == std::string("-height"))) {
      char* endptr;
      uint32_t h = strtol(args[i + 1], &endptr, 10);
      if (endptr != args[i + 1]) height = h;
    }
  }
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

  delete vulkanDevice;

  if (settings.validation)
    vks::debug::freeDebugCallback(instance);

  vkDestroyInstance(instance, nullptr);
}

void Application::printMultiviewProperties(VkDevice logicalDevice, VkPhysicalDevice physicalDevice) {
  GET_DEVICE_PROC_ADDR(logicalDevice, GetPhysicalDeviceProperties2KHR);
  GET_DEVICE_PROC_ADDR(logicalDevice, GetPhysicalDeviceFeatures2KHR);

  if (fpGetPhysicalDeviceFeatures2KHR) {
    VkPhysicalDeviceFeatures2KHR deviceFeatures2{};
    VkPhysicalDeviceMultiviewFeaturesKHX multiViewFeatures{};
    multiViewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHX;
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    deviceFeatures2.pNext = &multiViewFeatures;
    fpGetPhysicalDeviceFeatures2KHR(physicalDevice, &deviceFeatures2);

    // vkGetPhysicalDeviceFeatures2KHR(physicalDevice, &deviceFeatures2);

    printf("multiview %d\n", multiViewFeatures.multiview);
    printf("multiviewGeometryShader %d\n", multiViewFeatures.multiviewGeometryShader);
    printf("multiviewTessellationShader %d\n", multiViewFeatures.multiviewTessellationShader);
  }

  if (fpGetPhysicalDeviceProperties2KHR) {
    VkPhysicalDeviceProperties2KHR deviceProps2{};

    VkPhysicalDeviceMultiviewPropertiesKHX extProps{};
    extProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHX;
    deviceProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    deviceProps2.pNext = &extProps;
    fpGetPhysicalDeviceProperties2KHR(physicalDevice, &deviceProps2);

    printf("maxMultiviewViewCount %d\n", extProps.maxMultiviewViewCount);
    printf("maxMultiviewInstanceIndex %d\n", extProps.maxMultiviewInstanceIndex);

    VkPhysicalDeviceProperties2KHR deviceProps22{};
    VkPhysicalDeviceMultiviewPerViewAttributesPropertiesNVX extProps2{};
    extProps2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX;
    deviceProps22.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    deviceProps22.pNext = &extProps2;
    fpGetPhysicalDeviceProperties2KHR(physicalDevice, &deviceProps22);

    printf("perViewPositionAllComponents %d\n", extProps2.perViewPositionAllComponents);
  }
}


void Application::initVulkan(VikWindow *window) {
  VkResult err;

  // Vulkan instance
  err = createInstance(settings.validation, window);
  if (err)
    vks::tools::exitFatal("Could not create Vulkan instance : \n" + vks::tools::errorString(err), "Fatal error");

  // If requested, we enable the default validation layers for debugging
  if (settings.validation) {
    // The report flags determine what type of messages for the layers will be displayed
    // For validating (debugging) an appplication the error and warning bits should suffice
    VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    // Additional flags include performance info, loader and layer debug messages, etc.
    vks::debug::setupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
  }

  // Physical device
  uint32_t gpuCount = 0;
  // Get number of available physical devices
  VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
  assert(gpuCount > 0);
  // Enumerate devices
  std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
  err = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());
  if (err)
    vks::tools::exitFatal("Could not enumerate physical devices : \n" + vks::tools::errorString(err), "Fatal error");

  // GPU selection

  // Select physical device to be used for the Vulkan example
  // Defaults to the first device unless specified by command line
  uint32_t selectedDevice = 0;

  // GPU selection via command line argument
  for (size_t i = 0; i < args.size(); i++) {
    // Select GPU
    if ((args[i] == std::string("-g")) || (args[i] == std::string("-gpu"))) {
      char* endptr;
      uint32_t index = strtol(args[i + 1], &endptr, 10);
      if (endptr != args[i + 1]) {
        if (index > gpuCount - 1) {
          std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listgpus to show available Vulkan devices)" << std::endl;
        } else {
          std::cout << "Selected Vulkan device " << index << std::endl;
          selectedDevice = index;
        }
      }
      break;
    }
    // List available GPUs
    if (args[i] == std::string("-listgpus")) {
      uint32_t gpuCount = 0;
      VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
      if (gpuCount == 0) {
        std::cerr << "No Vulkan devices found!" << std::endl;
      } else {
        // Enumerate devices
        std::cout << "Available Vulkan devices" << std::endl;
        std::vector<VkPhysicalDevice> devices(gpuCount);
        VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, devices.data()));
        for (uint32_t i = 0; i < gpuCount; i++) {
          VkPhysicalDeviceProperties deviceProperties;
          vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
          std::cout << "Device [" << i << "] : " << deviceProperties.deviceName << std::endl;
          std::cout << " Type: " << vks::tools::physicalDeviceTypeString(deviceProperties.deviceType) << std::endl;
          std::cout << " API: " << (deviceProperties.apiVersion >> 22) << "." << ((deviceProperties.apiVersion >> 12) & 0x3ff) << "." << (deviceProperties.apiVersion & 0xfff) << std::endl;
        }
      }
    }
  }

  physicalDevice = physicalDevices[selectedDevice];

  // Store properties (including limits), features and memory properties of the phyiscal device (so that examples can check against them)
  vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
  vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

  // Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
  getEnabledFeatures();

  // Vulkan device creation
  // This is handled by a separate class that gets a logical device representation
  // and encapsulates functions related to a device
  vulkanDevice = new vks::VulkanDevice(physicalDevice);
  VkResult res = vulkanDevice->createLogicalDevice(enabledFeatures, enabledExtensions);
  if (res != VK_SUCCESS)
    vks::tools::exitFatal("Could not create Vulkan device: \n" + vks::tools::errorString(res), "Fatal error");

  device = vulkanDevice->logicalDevice;

  // printMultiviewProperties(device, physicalDevice);

  // Get a graphics queue from the device
  vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

  // Find a suitable depth format
  VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
  assert(validDepthFormat);

  swapChain.connect(instance, physicalDevice, device);

  // Create synchronization objects
  VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
  // Create a semaphore used to synchronize image presentation
  // Ensures that the image is displayed before we start submitting new commands to the queu
  VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
  // Create a semaphore used to synchronize command submission
  // Ensures that the image is not presented until all commands have been sumbitted and executed
  VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));
  // Create a semaphore used to synchronize command submission
  // Ensures that the image is not presented until all commands for the text overlay have been sumbitted and executed
  // Will be inserted after the render complete semaphore if the text overlay is enabled
  VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.textOverlayComplete));

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
  VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
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

  VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &depthStencil.image));
  vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);
  mem_alloc.allocationSize = memReqs.size;
  mem_alloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(device, &mem_alloc, nullptr, &depthStencil.mem));
  VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

  depthStencilView.image = depthStencil.image;
  VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &depthStencil.view));
}

void Application::setupFrameBuffer() {
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
    VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
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

  VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
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

  //setupSwapChain();
  fprintf(stderr, "resize: not creating swapchain.\n");
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
  windowResized();
  viewChanged();

  prepared = true;
}

void Application::windowResized() {
  // Can be overriden in derived class
}

void Application::setupSwapChain() {
  swapChain.create(&width, &height, settings.vsync);
}
