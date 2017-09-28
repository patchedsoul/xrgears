#pragma once

#include <string>
#include <vector>
#include <functional>

#include <vulkan/vulkan.h>

#include "vikRenderer.hpp"

#include "vikDevice.hpp"
#include "vikSwapChainVK.hpp"
#include "vikTimer.hpp"
#include "vikTextOverlay.hpp"
#include "system/vikSettings.hpp"
#include "vikShader.hpp"

namespace vik {

class RendererVks : public Renderer {
public:
  Timer timer;
  Device *vksDevice;

  VkPhysicalDeviceProperties deviceProperties;
  VkPhysicalDeviceFeatures deviceFeatures;
  VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
  VkPhysicalDeviceFeatures enabledFeatures{};

  VkFormat depthFormat;
  VkCommandPool cmdPool;
  VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  //VkSubmitInfo submitInfo;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  VkPipelineCache pipelineCache;

  VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

  struct {
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
  } depthStencil;

  struct {
    VkSemaphore present_complete;
    VkSemaphore render_complete;
    VkSemaphore text_overlay_complete;
  } semaphores;

  std::vector<VkCommandBuffer> drawCmdBuffers;


  std::vector<const char*> enabledExtensions;

  uint32_t destWidth;
  uint32_t destHeight;
  uint32_t currentBuffer = 0;

  std::function<void()> window_resize_cb;
  std::function<void()> enabled_features_cb;

  void set_window_resize_cb(std::function<void()> cb) {
    window_resize_cb = cb;
  }

  void set_enabled_features_cb(std::function<void()> cb) {
    enabled_features_cb = cb;
  }

  RendererVks(Settings *s, Window *w) : Renderer(s, w) {
    width = s->width;
    height = s->height;

    auto dimension_cb = [this](uint32_t w, uint32_t h) {
      width = destWidth = w;
      height = destHeight = h;
    };
    window->set_dimension_cb(dimension_cb);

    window->set_recreate_frame_buffers_cb([this]() { create_frame_buffers(); });
  }

  ~RendererVks() {
    window->get_swap_chain()->cleanup();
    if (descriptorPool != VK_NULL_HANDLE)
      vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    destroy_command_buffers();
    vkDestroyRenderPass(device, render_pass, nullptr);
    for (uint32_t i = 0; i < frame_buffers.size(); i++)
      vkDestroyFramebuffer(device, frame_buffers[i], nullptr);


    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.mem, nullptr);

    vkDestroyPipelineCache(device, pipelineCache, nullptr);

    vkDestroyCommandPool(device, cmdPool, nullptr);

    vkDestroySemaphore(device, semaphores.present_complete, nullptr);
    vkDestroySemaphore(device, semaphores.render_complete, nullptr);
    vkDestroySemaphore(device, semaphores.text_overlay_complete, nullptr);

    delete vksDevice;

    if (settings->validation)
      debug::freeDebugCallback(instance);

    vkDestroyInstance(instance, nullptr);
  }

  void init(const std::string &name, const std::string &title) {
    init_vulkan(name, window->required_extensions());
    window->init(width, height);

    window->update_window_title(make_title_string(title));
    window->get_swap_chain()->set_context(instance, physical_device, device);
    window->init_swap_chain(width, height);

    if (vksDevice->enableDebugMarkers)
      debugmarker::setup(device);

    create_command_pool();
    create_command_buffers();
    init_depth_stencil();
    create_render_pass();
    create_pipeline_cache();
    create_frame_buffers();

    destWidth = width;
    destHeight = height;
  }

  void wait_idle() {
    // Flush device to make sure all resources can be freed
    vkDeviceWaitIdle(device);
  }

  bool check_command_buffers() {
    for (auto& cmdBuffer : drawCmdBuffers)
      if (cmdBuffer == VK_NULL_HANDLE)
        return false;
    return true;
  }

  void create_command_buffers() {
    // Create one command buffer for each swap chain image and reuse for rendering

    vik_log_d("Swapchain image count %d", window->get_swap_chain()->image_count);

    drawCmdBuffers.resize(window->get_swap_chain()->image_count);

    VkCommandBufferAllocateInfo cmdBufAllocateInfo =
        initializers::commandBufferAllocateInfo(
          cmdPool,
          VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          static_cast<uint32_t>(drawCmdBuffers.size()));

    vik_log_check(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));

    vik_log_d("created %ld command buffers", drawCmdBuffers.size());
  }

  void destroy_command_buffers() {
    vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
  }

  VkCommandBuffer create_command_buffer(VkCommandBufferLevel level, bool begin) {
    VkCommandBuffer cmdBuffer;

    VkCommandBufferAllocateInfo cmdBufAllocateInfo =
        initializers::commandBufferAllocateInfo(
          cmdPool,
          level,
          1);

    vik_log_check(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

    // If requested, also start the new command buffer
    if (begin) {
      VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();
      vik_log_check(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
    }

    return cmdBuffer;
  }

  void flush_command_buffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free) {
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

  void create_pipeline_cache() {
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vik_log_check(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
  }

  virtual void check_tick_finnished(const std::string& title) {
    if (timer.tick_finnished()) {
      timer.update_fps();
      timer.reset();
    }
  }

  void prepare_frame() {
    // Acquire the next image from the swap chain
    SwapChainVK *sc = (SwapChainVK*) window->get_swap_chain();
    VkResult err = sc->acquire_next_image(semaphores.present_complete, &currentBuffer);
    // Recreate the swapchain if it's no longer compatible with the surface
    // (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
    if ((err == VK_ERROR_OUT_OF_DATE_KHR) || (err == VK_SUBOPTIMAL_KHR))
      window_resize_cb();
    else
      vik_log_check(err);
  }

  virtual void submit_frame() {
    SwapChainVK *sc = (SwapChainVK*) window->get_swap_chain();
    vik_log_check(sc->present(queue, currentBuffer, semaphores.render_complete));
    vik_log_check(vkQueueWaitIdle(queue));
  }

  void init_physical_device() {
    VkResult err;

    // Physical device
    uint32_t gpuCount = 0;
    // Get number of available physical devices
    vik_log_check(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
    assert(gpuCount > 0);
    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    err = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());

    vik_log_f_if(err, "Could not enumerate physical devices: %s", Log::result_string(err).c_str());

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

    physical_device = physicalDevices[selectedDevice];
  }

  void list_gpus() {
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
        vik_log_i(" Type: %s", tools::physicalDeviceTypeString(deviceProperties.deviceType).c_str());
        vik_log_i(" API: %d.%d.%d", deviceProperties.apiVersion >> 22, (deviceProperties.apiVersion >> 12) & 0x3ff, deviceProperties.apiVersion & 0xfff);
      }
    }
  }

  void get_physical_device_properties() {
    // Store properties (including limits), features and memory properties
    // of the phyiscal device (so that examples can check against them)
    vkGetPhysicalDeviceProperties(physical_device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(physical_device, &deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &deviceMemoryProperties);
  }

  void init_debugging() {
    // The report flags determine what type of messages for the layers will be displayed
    // For validating (debugging) an appplication the error and warning bits should suffice
    VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    // Additional flags include performance info, loader and layer debug messages, etc.
    debug::setupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
  }

  void init_vulkan(const std::string &name, const std::vector<const char*> &extensions) {
    VkResult err;

    // Vulkan instance
    err = create_instance(name, extensions);

    vik_log_f_if(err, "Could not create Vulkan instance: %s",
                 Log::result_string(err).c_str());

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
    vksDevice = new Device(physical_device);

    /*
    enabledExtensions.push_back(VK_KHX_MULTIVIEW_EXTENSION_NAME);
    enabledExtensions.push_back(VK_NVX_MULTIVIEW_PER_VIEW_ATTRIBUTES_EXTENSION_NAME);
    enabledExtensions.push_back(VK_NV_VIEWPORT_ARRAY2_EXTENSION_NAME);
    enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    */

    VkResult res = vksDevice->createLogicalDevice(enabledFeatures, enabledExtensions);
    vik_log_f_if(res != VK_SUCCESS, "Could not create Vulkan device: %s", Log::result_string(res).c_str());

    device = vksDevice->logicalDevice;

    //vksDevice->printMultiviewProperties();

    // Get a graphics queue from the device
    vkGetDeviceQueue(device, vksDevice->queueFamilyIndices.graphics, 0, &queue);

    // Find a suitable depth format
    VkBool32 validDepthFormat = tools::getSupportedDepthFormat(physical_device, &depthFormat);
    assert(validDepthFormat);

    init_semaphores();

  }

  void init_semaphores() {
    // Create synchronization objects
    VkSemaphoreCreateInfo semaphoreCreateInfo = initializers::semaphoreCreateInfo();
    // Create a semaphore used to synchronize image presentation
    // Ensures that the image is displayed before we start submitting new commands to the queu
    vik_log_check(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.present_complete));
    // Create a semaphore used to synchronize command submission
    // Ensures that the image is not presented until all commands have been sumbitted and executed
    vik_log_check(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.render_complete));
    // Create a semaphore used to synchronize command submission
    // Ensures that the image is not presented until all commands for the text overlay have been sumbitted and executed
    // Will be inserted after the render complete semaphore if the text overlay is enabled
    vik_log_check(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.text_overlay_complete));
  }

  void create_command_pool() {
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = window->get_swap_chain()->get_queue_index();
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vik_log_check(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
  }

  std::string make_title_string(const std::string& title) {
    std::string device_str(deviceProperties.deviceName);
    std::string windowTitle = title + " - " + device_str;
    if (!settings->enable_text_overlay)
      windowTitle += " - " + std::to_string(timer.frames_since_tick) + " fps";
    return windowTitle;
  }

  float get_aspect_ratio() {
    return (float)width / (float)height;
  }

  void resize() {
    // Ensure all operations on the device have been finished before destroying resources
    vkDeviceWaitIdle(device);

    // Recreate swap chain
    width = destWidth;
    height = destHeight;

    window->get_swap_chain()->create(width, height);
    // Recreate the frame buffers

    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.mem, nullptr);
    init_depth_stencil();

    for (uint32_t i = 0; i < frame_buffers.size(); i++)
      vkDestroyFramebuffer(device, frame_buffers[i], nullptr);
    create_frame_buffers();

    // Command buffers need to be recreated as they may store
    // references to the recreated frame buffer
    destroy_command_buffers();
    create_command_buffers();
  }

  VkSubmitInfo init_render_submit_info() {
    VkSubmitInfo submit_info = initializers::submitInfo();
    submit_info.pWaitDstStageMask = &submitPipelineStages;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &semaphores.present_complete;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &semaphores.render_complete;
    submit_info.commandBufferCount = 1;
    return submit_info;
  }

  VkCommandBuffer* get_current_command_buffer() {
    return &drawCmdBuffers[currentBuffer];
  }

  void create_frame_buffers() {
    uint32_t count = window->get_swap_chain()->image_count;

    // Create frame buffers for every swap chain image
    frame_buffers.resize(count);

    std::vector<VkImageView> attachments = std::vector<VkImageView>(2);
    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = depthStencil.view;

    for (uint32_t i = 0; i < count; i++) {
      attachments[0] = window->get_swap_chain()->buffers[i].view;
      create_frame_buffer(&frame_buffers[i], attachments);
    }
  }

  void create_render_pass() {
    std::array<VkAttachmentDescription, 2> attachments = {};
    // Color attachment
    attachments[0].format = window->get_swap_chain()->surface_format.format;
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

    vik_log_check(vkCreateRenderPass(device, &renderPassInfo, nullptr, &render_pass));
    vik_log_d("renderpass setup complete");
  }

  void init_depth_stencil() {
    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
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
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkImageViewCreateInfo depthStencilView = {};
    depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
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
};
}
