/*
 * vitamin-k
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <functional>

#include "vikSwapChain.hpp"
#include "vikDebug.hpp"
#include "vikDevice.hpp"
#include "vikSwapChainVK.hpp"
#include "vikTimer.hpp"
#include "vikShader.hpp"

#include "../system/vikSettings.hpp"
#include "../window/vikWindow.hpp"

namespace vik {

class Renderer {
 public:
  VkInstance instance;
  VkDevice device;
  VkPhysicalDevice physical_device;

  VkCommandPool cmd_pool;

  std::vector<VkCommandBuffer> cmd_buffers;

  VkQueue queue;
  std::vector<VkFramebuffer> frame_buffers;
  VkRenderPass render_pass;

  uint32_t width;
  uint32_t height;

  Settings *settings;

  Window *window;

  Timer timer;
  Device *vik_device;

  VkPhysicalDeviceProperties device_properties;
  VkPhysicalDeviceFeatures device_features;
  VkPhysicalDeviceMemoryProperties device_memory_properties;
  VkPhysicalDeviceFeatures enabled_features{};

  std::vector<std::string> supported_extensions;

  VkFormat depth_format;
  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  VkPipelineCache pipeline_cache;

  VkClearColorValue default_clear_color = { { 0.025f, 0.025f, 0.025f, 1.0f } };

  struct {
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
  } depth_stencil;

  struct {
    VkSemaphore present_complete;
    VkSemaphore render_complete;
  } semaphores;

  uint32_t current_buffer = 0;

  std::function<void()> window_resize_cb;
  std::function<void()> enabled_features_cb;

  explicit Renderer(Settings *s) {
    settings = s;
    width = s->size.first;
    height = s->size.second;
  }

  virtual ~Renderer() {
    window->get_swap_chain()->cleanup();
    if (descriptor_pool != VK_NULL_HANDLE)
      vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

    destroy_command_buffers();
    vkDestroyRenderPass(device, render_pass, nullptr);
    for (uint32_t i = 0; i < frame_buffers.size(); i++)
      vkDestroyFramebuffer(device, frame_buffers[i], nullptr);

    vkDestroyImageView(device, depth_stencil.view, nullptr);
    vkDestroyImage(device, depth_stencil.image, nullptr);
    vkFreeMemory(device, depth_stencil.mem, nullptr);

    vkDestroyPipelineCache(device, pipeline_cache, nullptr);

    vkDestroyCommandPool(device, cmd_pool, nullptr);

    vkDestroySemaphore(device, semaphores.present_complete, nullptr);
    vkDestroySemaphore(device, semaphores.render_complete, nullptr);

    delete vik_device;

    if (settings->validation)
      debug::freeDebugCallback(instance);

    vkDestroyInstance(instance, nullptr);
  }

  void set_window(Window *w) {
    window = w;

    auto dimension_cb = [this](uint32_t w, uint32_t h) {
      if (((w != width) || (h != height))
          && (width > 0) && (width > 0)) {
        vik_log_e("dimension cb: requested %dx%d differs current %dx%d",
                  w, h, width, height);
        width = w;
        height = h;
        resize();
      }
    };
    window->set_dimension_cb(dimension_cb);

    auto size_only_cb = [this](uint32_t w, uint32_t h) {
      if (((w != width) || (h != height))
          && (width > 0) && (width > 0)) {
        width = w;
        height = h;
      }
    };
    window->set_size_only_cb(size_only_cb);

    window->set_render_frame_cb([this](){
      prepare_frame();
      render_cb();
      submit_frame();
    });
  }

  void create_buffers(uint32_t count) {
    create_frame_buffers(count);
    allocate_command_buffers(count);
  }

  void set_window_resize_cb(std::function<void()> cb) {
    window_resize_cb = cb;
  }

  void set_enabled_features_cb(std::function<void()> cb) {
    enabled_features_cb = cb;
  }

  std::function<void()> frame_start_cb;
  std::function<void()> render_cb;
  std::function<void(float frame_time)> frame_end_cb;

  void set_frame_start_cb(std::function<void()> cb) {
    frame_start_cb = cb;
  }

  void set_frame_end_cb(std::function<void(float frame_time)> cb) {
    frame_end_cb = cb;
  }

  void set_render_cb(std::function<void()> cb) {
    render_cb = cb;
  }
  VkResult create_instance(const std::string& name,
                           const std::vector<const char*> &window_extensions) {

    query_supported_extensions();

    VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = name.c_str(),
      .pEngineName = "vitamin-k",
      .apiVersion = VK_MAKE_VERSION(1, 0, 2)
    };

    std::vector<const char*> extensions;
    enable_if_supported(&extensions, VK_KHR_SURFACE_EXTENSION_NAME);
    enable_if_supported(&extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    // Enable surface extensions depending on window system
    for (auto window_ext : window_extensions)
      enable_if_supported(&extensions, window_ext);

    if (settings->validation)
      enable_if_supported(&extensions, VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

    VkInstanceCreateInfo instance_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = (uint32_t) extensions.size(),
      .ppEnabledExtensionNames = extensions.data()
    };

    if (settings->validation) {
      instance_info.enabledLayerCount = debug::validationLayerCount;
      instance_info.ppEnabledLayerNames = debug::validationLayerNames;
    }

    return vkCreateInstance(&instance_info, nullptr, &instance);
  }

  void create_frame_buffer(VkFramebuffer *frame_buffer,
                           const std::vector<VkImageView> &attachments) {
    VkFramebufferCreateInfo frame_buffer_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = render_pass,
      .attachmentCount = (uint32_t) attachments.size(),
      .pAttachments = attachments.data(),
      .width = width,
      .height = height,
      .layers = 1
    };

    vik_log_check(vkCreateFramebuffer(device,
                                      &frame_buffer_info,
                                      nullptr,
                                      frame_buffer));
  }

  void allocate_command_buffers(uint32_t count) {
    vik_log_f_if(count == 0, "Requested 0 command buffers.");
    vik_log_d("Allocating %d Command Buffers.", count);

    // Create one command buffer for each swap chain image and reuse for rendering
    cmd_buffers.resize(count);

    VkCommandBufferAllocateInfo cmd_buffer_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cmd_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = count
    };

    vik_log_check(vkAllocateCommandBuffers(device,
                                           &cmd_buffer_info,
                                           cmd_buffers.data()));
  }
  void init(const std::string &name) {
    init_vulkan(name, window->required_extensions());
    create_pipeline_cache();

    window->update_window_title(make_title_string(name));
    window->get_swap_chain()->set_context(instance, physical_device, device);
    window->init_swap_chain(width, height);

    // KMS render callback
    auto _render_cb = [this](uint32_t index) {
      current_buffer = index;
      render_cb();
    };
    window->get_swap_chain()->set_render_cb(_render_cb);

    if (vik_device->enable_debug_markers)
      debugmarker::setup(device);

    create_command_pool(window->get_swap_chain()->get_queue_index());

    // need format
    init_depth_stencil();
    create_render_pass();

    assert(window->get_swap_chain()->image_count > 0);
    create_buffers(window->get_swap_chain()->image_count);
  }

  void wait_idle() {
    // Flush device to make sure all resources can be freed
    vkDeviceWaitIdle(device);
  }

  bool check_command_buffers() {
    for (auto& cmd_buffer : cmd_buffers)
      if (cmd_buffer == VK_NULL_HANDLE)
        return false;
    return true;
  }

  void destroy_command_buffers() {
    vkFreeCommandBuffers(device, cmd_pool,
                         static_cast<uint32_t>(cmd_buffers.size()),
                         cmd_buffers.data());
  }

  VkCommandBuffer create_command_buffer() {
    VkCommandBuffer cmd_buffer;

    VkCommandBufferAllocateInfo cmd_buffer_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = cmd_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1
    };

    vik_log_check(vkAllocateCommandBuffers(device,
                                           &cmd_buffer_info,
                                           &cmd_buffer));

    return cmd_buffer;
  }

  void create_pipeline_cache() {
    VkPipelineCacheCreateInfo pipeline_cache_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
    };
    vik_log_check(vkCreatePipelineCache(device, &pipeline_cache_info,
                                        nullptr, &pipeline_cache));
  }

  void init_physical_device() {
    VkResult err;

    // Physical device
    uint32_t gpu_count = 0;
    // Get number of available physical devices
    vik_log_check(vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr));
    assert(gpu_count > 0);
    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpu_count);
    err = vkEnumeratePhysicalDevices(instance, &gpu_count, physicalDevices.data());

    vik_log_f_if(err, "Could not enumerate physical devices: %s",
                 Log::result_string(err).c_str());

    // GPU selection
    if (settings->list_gpus_and_exit) {
      list_gpus();
      exit(0);
    }

    // Select first device by default
    if (settings->gpu == -1)
      settings->gpu = 0;

    // Select physical device to be used for the Vulkan example
    // Defaults to the first device unless specified by command line
    uint32_t selected_device = 0;
    if (settings->gpu > (int) gpu_count - 1) {
      vik_log_f("Selected device index %d is out of range,"
                " reverting to device 0"
                " (use --list-gpus to show available Vulkan devices)",
                settings->gpu);
    } else if (settings->gpu != 0) {
      vik_log_i("Selected Vulkan device %d", settings->gpu);
      selected_device = settings->gpu;
    }

    physical_device = physicalDevices[selected_device];
  }

  void list_gpus() {
    uint32_t gpu_count = 0;
    vik_log_check(vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr));
    if (gpu_count == 0) {
      vik_log_e("No Vulkan devices found!");
    } else {
      // Enumerate devices
      vik_log_i("Available Vulkan devices");
      std::vector<VkPhysicalDevice> devices(gpu_count);
      vik_log_check(vkEnumeratePhysicalDevices(instance, &gpu_count, devices.data()));
      for (uint32_t i = 0; i < gpu_count; i++) {
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(devices[i], &device_properties);
        vik_log_i("Device [%d] : %s", i, device_properties.deviceName);
        vik_log_i(" Type: %s",
                  tools::physicalDeviceTypeString(device_properties.deviceType).c_str());
        vik_log_i(" API: %d.%d.%d",
                  device_properties.apiVersion >> 22,
                  (device_properties.apiVersion >> 12) & 0x3ff,
                  device_properties.apiVersion & 0xfff);
      }
    }
  }

  void get_physical_device_properties() {
    // Store properties (including limits), features and memory properties
    // of the phyiscal device (so that examples can check against them)
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    vkGetPhysicalDeviceFeatures(physical_device, &device_features);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &device_memory_properties);
  }

  void init_debugging() {
    // The report flags determine what type of messages for the layers
    // will be displayed For validating (debugging) an appplication the error
    // and warning bits should suffice
    VkDebugReportFlagsEXT debug_report_flags =
        VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    // Additional flags include performance info,
    // loader and layer debug messages, etc.
    debug::setupDebugging(instance, debug_report_flags, VK_NULL_HANDLE);
  }

  void init_vulkan(const std::string &name, const std::vector<const char*> &extensions) {
    VkResult err = create_instance(name, extensions);
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
    vik_device = new Device(physical_device);

    VkResult res = vik_device->createLogicalDevice(enabled_features,
                                                   window->required_device_extensions());
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not create Vulkan device: %s",
                 Log::result_string(res).c_str());

    device = vik_device->logicalDevice;

    if (is_extension_supported(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
      vik_device->print_multiview_properties(instance);

    // Get a graphics queue from the device
    vkGetDeviceQueue(device, vik_device->queueFamilyIndices.graphics, 0, &queue);

    // Find a suitable depth format
    VkBool32 validDepthFormat = tools::getSupportedDepthFormat(physical_device, &depth_format);
    assert(validDepthFormat);

    init_semaphores();
  }

  bool enable_if_supported(std::vector<const char*> *extensions,
                           const char* name) {
    if (is_extension_supported(name)) {
      vik_log_d("instance: Enabling supported %s.", name);
      extensions->push_back(name);
      return true;
    } else {
      vik_log_w("instance: %s not supported.", name);
      return false;
    }
  }

  void query_supported_extensions() {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    if (count > 0) {
      std::vector<VkExtensionProperties> extensions(count);
      if (vkEnumerateInstanceExtensionProperties(nullptr, &count, &extensions.front()) == VK_SUCCESS)
        for (auto ext : extensions)
          supported_extensions.push_back(ext.extensionName);
    }
  }

  bool is_extension_supported(std::string extension) {
    return std::find(supported_extensions.begin(),
                     supported_extensions.end(),
                     extension) != supported_extensions.end();
  }

  void print_supported_extensions() {
    vik_log_i("Supported instance extensions");
    for (auto extension : supported_extensions) {
      vik_log_i("%s", extension.c_str());
    }
  }

  virtual void init_semaphores() {
    // Create synchronization objects
    VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    // Create a semaphore used to synchronize image presentation
    // Ensures that the image is displayed before we start submitting new commands to the queu
    vik_log_check(vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphores.present_complete));
    // Create a semaphore used to synchronize command submission
    // Ensures that the image is not presented until all commands have been sumbitted and executed
    vik_log_check(vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphores.render_complete));
  }

  void create_command_pool(uint32_t index) {
    VkCommandPoolCreateInfo cmd_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = index
    };

    vik_log_check(vkCreateCommandPool(device, &cmd_pool_info,
                                      nullptr, &cmd_pool));
  }

  std::string make_title_string(const std::string& title) {
    std::string device_str(device_properties.deviceName);
    std::string window_title = title + " - " + device_str;
    if (!settings->enable_text_overlay)
      window_title += " - " + std::to_string(timer.frames_since_tick) + " fps";
    return window_title;
  }

  float get_aspect_ratio() {
    return (float)width / (float)height;
  }

  virtual void resize() {
    vik_log_d("Resize!");

    // Ensure all operations on the device have been finished
    // before destroying resources
    wait_idle();

    window->get_swap_chain()->create(width, height);
    // Recreate the frame buffers

    vkDestroyImageView(device, depth_stencil.view, nullptr);
    vkDestroyImage(device, depth_stencil.image, nullptr);
    vkFreeMemory(device, depth_stencil.mem, nullptr);
    init_depth_stencil();

    for (uint32_t i = 0; i < frame_buffers.size(); i++)
      vkDestroyFramebuffer(device, frame_buffers[i], nullptr);
    create_frame_buffers(window->get_swap_chain()->image_count);

    // Command buffers need to be recreated as they may store
    // references to the recreated frame buffer
    destroy_command_buffers();
    allocate_command_buffers(window->get_swap_chain()->image_count);

    window_resize_cb();
  }

  VkSubmitInfo init_render_submit_info() {
    // Pipeline stage at which the
    // queue submission will wait (via pWaitSemaphores)
    // TODO(lubosz): stage_flags deallocate at end of function
    // std::array<VkPipelineStageFlags,1> stage_flags = {
    //   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    // };

    // The submit info structure specifices a
    // command buffer queue submission batch
    VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      // Pointer to the list of pipeline stages that the semaphore waits will occur at
      // submit_info.pWaitDstStageMask = stage_flags.data();
      .waitSemaphoreCount = 1,
      // Semaphore(s) to wait upon before the submitted command buffer starts executing
      .pWaitSemaphores = &semaphores.present_complete,
      .commandBufferCount = 1,
      .signalSemaphoreCount = 1,
      // Semaphore(s) to be signaled when command buffers have completed
      .pSignalSemaphores = &semaphores.render_complete
    };

    return submit_info;
  }

  VkCommandBuffer* get_current_command_buffer() {
    return &cmd_buffers[current_buffer];
  }

  void create_frame_buffers(uint32_t count) {
    // Create frame buffers for every swap chain image
    frame_buffers.resize(count);

    std::vector<VkImageView> attachments = std::vector<VkImageView>(2);
    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = depth_stencil.view;

    for (uint32_t i = 0; i < count; i++) {
      attachments[0] = window->get_swap_chain()->buffers[i].view;
      create_frame_buffer(&frame_buffers[i], attachments);
    }
  }

  void create_render_pass() {
    std::array<VkAttachmentDescription, 2> attachments = {
      (VkAttachmentDescription) {
        // Color attachment
        .format = window->get_swap_chain()->surface_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      },
      (VkAttachmentDescription) {
        // Depth attachment
        .format = depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
      }
    };

    VkAttachmentReference color_reference = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depth_reference = {
      .attachment = 1,
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass_description = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_reference,
      .pDepthStencilAttachment = &depth_reference
    };

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies = {
      (VkSubpassDependency) {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
      },
      (VkSubpassDependency) {
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
      }
    };

    VkRenderPassCreateInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = static_cast<uint32_t>(attachments.size()),
      .pAttachments = attachments.data(),
      .subpassCount = 1,
      .pSubpasses = &subpass_description,
      .dependencyCount = static_cast<uint32_t>(dependencies.size()),
      .pDependencies = dependencies.data()
    };

#if 0
    // VK_KHX_multiview
    uint32_t correlationMasks[] = { 2 };
    uint32_t viewMasks[] = { 1 };
    int32_t viewOffsets[] = { 2 };

    VkRenderPassMultiviewCreateInfo renderPassMvInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
      .subpassCount = 1,
      .dependencyCount = 1,
      .correlationMaskCount = 1,
      .pCorrelationMasks = correlationMasks,
      .pViewMasks = viewMasks,
      .pViewOffsets = viewOffsets,
    };
    render_pass_info.pNext = &renderPassMvInfo;
#endif

    vik_log_check(vkCreateRenderPass(device, &render_pass_info,
                                     nullptr, &render_pass));
  }

  void init_depth_stencil() {
    VkImageCreateInfo image = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = depth_format,
      .extent = {
        .width = width,
        .height = height,
        .depth = 1
      },
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    };

    VkMemoryRequirements memReqs;

    vik_log_check(vkCreateImage(device, &image, nullptr, &depth_stencil.image));
    vkGetImageMemoryRequirements(device, depth_stencil.image, &memReqs);

    VkMemoryAllocateInfo mem_alloc = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      .memoryTypeIndex = vik_device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    vik_log_check(vkAllocateMemory(device, &mem_alloc, nullptr, &depth_stencil.mem));
    vik_log_check(vkBindImageMemory(device, depth_stencil.image, depth_stencil.mem, 0));

    VkImageViewCreateInfo depthStencilView = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = depth_stencil.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = depth_format,
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };
    vik_log_check(vkCreateImageView(device, &depthStencilView, nullptr, &depth_stencil.view));
  }

  virtual void check_tick_finnished() {
    if (timer.tick_finnished()) {
      timer.update_fps();
      timer.reset();
    }
  }

  void prepare_frame() {
    // Acquire the next image from the swap chain
    SwapChainVK *sc = (SwapChainVK*) window->get_swap_chain();
    VkResult err = sc->acquire_next_image(semaphores.present_complete, &current_buffer);
    // Recreate the swapchain if it's no longer compatible with the surface
    // (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
    if ((err == VK_ERROR_OUT_OF_DATE_KHR) || (err == VK_SUBOPTIMAL_KHR)) {
      vik_log_w("Received VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR.");
      resize();
    } else {
      vik_log_check(err);
    }
  }

  // Present the current buffer to the swap chain
  // Pass the semaphore signaled by the command buffer submission from
  // the submit info as the wait semaphore for swap chain presentation
  // This ensures that the image is not presented to the windowing system
  // until all commands have been submitted
  virtual void submit_frame() {
    SwapChainVK *sc = (SwapChainVK*) window->get_swap_chain();
    vik_log_check(sc->present(queue, current_buffer, semaphores.render_complete));
    vik_log_check(vkQueueWaitIdle(queue));
  }

  void render() {
    timer.start();
    frame_start_cb();
    window->iterate();
    timer.increment();
    float frame_time = timer.update_frame_time();
    frame_end_cb(frame_time);
    timer.update_animation_timer();
    check_tick_finnished();
  }
};
}  // namespace vik
