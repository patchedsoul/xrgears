/*
* Vulkan device class
*
* Encapsulates a physical Vulkan device and it's logical representation
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <assert.h>
#include <vulkan/vulkan.h>

#include <exception>
#include <algorithm>
#include <vector>
#include <string>

#include "vikTools.hpp"
#include "vikBuffer.hpp"

namespace vik {
class Device {
 public:
  /** @brief Physical device representation */
  VkPhysicalDevice physicalDevice;
  /** @brief Logical device representation (application's view of the device) */
  VkDevice logicalDevice;
  /** @brief Properties of the physical device including limits that the application can check against */
  VkPhysicalDeviceProperties properties;
  /** @brief Features of the physical device that an application can use to check if a feature is supported */
  VkPhysicalDeviceFeatures features;
  /** @brief Memory types and heaps of the physical device */
  VkPhysicalDeviceMemoryProperties memoryProperties;
  /** @brief Queue family properties of the physical device */
  std::vector<VkQueueFamilyProperties> queueFamilyProperties;
  /** @brief List of extensions supported by the device */
  std::vector<std::string> supported_extensions;

  /** @brief Default command pool for the graphics queue family index */
  VkCommandPool commandPool = VK_NULL_HANDLE;

  /** @brief Set to true when the debug marker extension is detected */
  bool enable_debug_markers = false;

  /** @brief Contains queue family indices */
  struct {
    uint32_t graphics;
    uint32_t compute;
    uint32_t transfer;
  } queueFamilyIndices;

  /**  @brief Typecast to VkDevice */
  operator VkDevice() { return logicalDevice; }


  /**
    * Default constructor
    *
    * @param physicalDevice Physical device that is to be used
    */
  explicit Device(VkPhysicalDevice physicalDevice) {
    assert(physicalDevice);
    this->physicalDevice = physicalDevice;

    // Store Properties features, limits and properties of the physical device for later use
    // Device properties also contain limits and sparse properties
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    // Features should be checked by the examples before using them
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    // Memory properties are used regularly for creating all kinds of buffers
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    // Queue family properties, used for setting up requested queues upon device creation
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    // Get list of supported extensions
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
    if (extCount > 0) {
      std::vector<VkExtensionProperties> extensions(extCount);
      if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
        for (auto ext : extensions)
          supported_extensions.push_back(ext.extensionName);
    }
  }

  /**
    * Default destructor
    *
    * @note Frees the logical device
    */
  ~Device() {
    if (commandPool)
      vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
    if (logicalDevice)
      vkDestroyDevice(logicalDevice, nullptr);
  }

  /**
    * Get the index of a memory type that has all the requested property bits set
    *
    * @param typeBits Bitmask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
    * @param properties Bitmask of properties for the memory type to request
    * @param (Optional) memTypeFound Pointer to a bool that is set to true if a matching memory type has been found
    *
    * @return Index of the requested memory type
    *
    * @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
    */
  uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr) {
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
      if ((typeBits & 1) == 1) {
        if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
          if (memTypeFound)
            *memTypeFound = true;
          return i;
        }
      }
      typeBits >>= 1;
    }

    if (memTypeFound) {
      *memTypeFound = false;
      return 0;
    } else {
      throw std::runtime_error("Could not find a matching memory type");
    }
  }

  /**
    * Get the index of a queue family that supports the requested queue flags
    *
    * @param queueFlags Queue flags to find a queue family index for
    *
    * @return Index of the queue family index that matches the flags
    *
    * @throw Throws an exception if no queue family index could be found that supports the requested flags
    */
  uint32_t getQueueFamilyIndex(VkQueueFlagBits queueFlags) {
    // Dedicated queue for compute
    // Try to find a queue family index that supports compute but not graphics
    if (queueFlags & VK_QUEUE_COMPUTE_BIT) {
      for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
        if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
          return i;
        }
    }

    // Dedicated queue for transfer
    // Try to find a queue family index that supports transfer but not graphics and compute
    if (queueFlags & VK_QUEUE_TRANSFER_BIT) {
      for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
        if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) {
          return i;
        }
      }
    }

    // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
      if (queueFamilyProperties[i].queueFlags & queueFlags) {
        return i;
      }
    }

    throw std::runtime_error("Could not find a matching queue family index");
  }

  /**
    * Create the logical device based on the assigned physical device, also gets default queue family indices
    *
    * @param enabledFeatures Can be used to enable certain features upon device creation
    * @param useSwapChain Set to false for headless rendering to omit the swapchain device extensions
    * @param requestedQueueTypes Bit flags specifying the queue types to be requested from the device
    *
    * @return VkResult of the device creation call
    */
  VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures,
                               const std::vector<const char*> &window_extensions,
                               bool useSwapChain = true,
                               VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT) {
    // Desired queues need to be requested upon logical device creation
    // Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
    // requests different queue types

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

    // Get queue family indices for the requested queue family types
    // Note that the indices may overlap depending on the implementation

    const float defaultQueuePriority(0.0f);

    // Graphics queue
    if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT) {
      queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
      VkDeviceQueueCreateInfo queueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndices.graphics,
        .queueCount = 1,
        .pQueuePriorities = &defaultQueuePriority
      };

      queueCreateInfos.push_back(queueInfo);
    } else {
      queueFamilyIndices.graphics = VK_NULL_HANDLE;
    }

    // Dedicated compute queue
    if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT) {
      queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
      if (queueFamilyIndices.compute != queueFamilyIndices.graphics) {
        // If compute family index differs, we need an additional queue create info for the compute queue
        VkDeviceQueueCreateInfo queueInfo = {
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = queueFamilyIndices.compute,
          .queueCount = 1,
          .pQueuePriorities = &defaultQueuePriority
        };

        queueCreateInfos.push_back(queueInfo);
      }
    } else {
      // Else we use the same queue
      queueFamilyIndices.compute = queueFamilyIndices.graphics;
    }

    // Dedicated transfer queue
    if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT) {
      queueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
      if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute)) {
        // If compute family index differs, we need an additional queue create info for the compute queue
        VkDeviceQueueCreateInfo queueInfo = {
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = queueFamilyIndices.transfer,
          .queueCount = 1,
          .pQueuePriorities = &defaultQueuePriority
        };

        queueCreateInfos.push_back(queueInfo);
      }
    } else {
      // Else we use the same queue
      queueFamilyIndices.transfer = queueFamilyIndices.graphics;
    }

    // Create the logical device representation
    std::vector<const char*> deviceExtensions;
    if (useSwapChain) {
      // If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
      deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    enable_if_supported(&deviceExtensions, VK_KHR_MULTIVIEW_EXTENSION_NAME);
    enable_if_supported(&deviceExtensions, VK_NVX_MULTIVIEW_PER_VIEW_ATTRIBUTES_EXTENSION_NAME);
    enable_if_supported(&deviceExtensions, VK_NV_VIEWPORT_ARRAY2_EXTENSION_NAME);

    for (auto window_ext : window_extensions)
      enable_if_supported(&deviceExtensions, window_ext);

    VkDeviceCreateInfo deviceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .pEnabledFeatures = &enabledFeatures
    };

    // Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
    // enableDebugMarkers = enableIfSupported(&deviceExtensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);

    if (deviceExtensions.size() > 0) {
      deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
      deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }

    VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo,
                                     nullptr, &logicalDevice);

    if (result == VK_SUCCESS)
      // Create a default command pool for graphics command buffers
      commandPool = createCommandPool(queueFamilyIndices.graphics);

    return result;
  }

  /**
    * Create a buffer on the device
    *
    * @param usageFlags Usage flag bitmask for the buffer (i.e. index, vertex, uniform buffer)
    * @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
    * @param size Size of the buffer in byes
    * @param buffer Pointer to the buffer handle acquired by the function
    * @param memory Pointer to the memory handle acquired by the function
    * @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
    *
    * @return VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
    */
  VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr) {
    // Create the buffer handle
    VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usageFlags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    vik_log_check(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, buffer));

    // Create the memory backing up the buffer handle
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memReqs);

    VkMemoryAllocateInfo memAlloc {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      // Find a memory type index that fits the properties of the buffer
      .memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags)
    };

    vik_log_check(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, memory));

    // If a pointer to the buffer data has been passed, map the buffer and copy over the data
    if (data != nullptr) {
      void *mapped;
      vik_log_check(vkMapMemory(logicalDevice, *memory, 0, size, 0, &mapped));
      memcpy(mapped, data, size);
      // If host coherency hasn't been requested, do a manual flush to make writes visible
      if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {

        VkMappedMemoryRange mappedRange = {
          .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
          .memory = *memory,
          .offset = 0,
          .size = size
        };

        vkFlushMappedMemoryRanges(logicalDevice, 1, &mappedRange);
      }
      vkUnmapMemory(logicalDevice, *memory);
    }

    // Attach the memory to the buffer object
    vik_log_check(vkBindBufferMemory(logicalDevice, *buffer, *memory, 0));

    return VK_SUCCESS;
  }

  void create_and_map(Buffer *buffer, VkDeviceSize size) {
    vik_log_check(
          createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            buffer, size));

    // Map persistent
    vik_log_check(buffer->map());
  }

  /**
    * Create a buffer on the device
    *
    * @param usageFlags Usage flag bitmask for the buffer (i.e. index, vertex, uniform buffer)
    * @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
    * @param buffer Pointer to a vk::Vulkan buffer object
    * @param size Size of the buffer in byes
    * @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
    *
    * @return VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
    */
  VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Buffer *buffer, VkDeviceSize size, void *data = nullptr) {
    buffer->device = logicalDevice;

    // Create the buffer handle
    VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usageFlags
    };
    vik_log_check(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &buffer->buffer));

    // Create the memory backing up the buffer handle
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(logicalDevice, buffer->buffer, &memReqs);

    VkMemoryAllocateInfo memAlloc {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      // Find a memory type index that fits the properties of the buffer
      .memoryTypeIndex = getMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags)
    };

    vik_log_check(vkAllocateMemory(logicalDevice, &memAlloc,
                                   nullptr, &buffer->memory));

    buffer->alignment = memReqs.alignment;
    buffer->size = memAlloc.allocationSize;
    buffer->usageFlags = usageFlags;
    buffer->memoryPropertyFlags = memoryPropertyFlags;

    // If a pointer to the buffer data has been passed, map the buffer and copy over the data
    if (data != nullptr) {
      vik_log_check(buffer->map());
      memcpy(buffer->mapped, data, size);
      buffer->unmap();
    }

    // Initialize a default descriptor that covers the whole buffer size
    buffer->setupDescriptor();

    // Attach the memory to the buffer object
    return buffer->bind();
  }

  /**
    * Copy buffer data from src to dst using VkCmdCopyBuffer
    *
    * @param src Pointer to the source buffer to copy from
    * @param dst Pointer to the destination buffer to copy tp
    * @param queue Pointer
    * @param copyRegion (Optional) Pointer to a copy region, if NULL, the whole buffer is copied
    *
    * @note Source and destionation pointers must have the approriate transfer usage flags set (TRANSFER_SRC / TRANSFER_DST)
    */
  void copyBuffer(Buffer *src, Buffer *dst, VkQueue queue, VkBufferCopy *copyRegion = nullptr) {
    assert(dst->size <= src->size);
    assert(src->buffer && src->buffer);
    VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy bufferCopy{};
    if (copyRegion == nullptr)
      bufferCopy.size = src->size;
    else
      bufferCopy = *copyRegion;

    vkCmdCopyBuffer(copyCmd, src->buffer, dst->buffer, 1, &bufferCopy);

    flushCommandBuffer(copyCmd, queue);
  }

  /**
    * Create a command pool for allocation command buffers from
    *
    * @param queueFamilyIndex Family index of the queue to create the command pool for
    * @param createFlags (Optional) Command pool creation flags (Defaults to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
    *
    * @note Command buffers allocated from the created pool can only be submitted to a queue with the same family index
    *
    * @return A handle to the created command buffer
    */
  VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) {
    VkCommandPoolCreateInfo cmdPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = createFlags,
      .queueFamilyIndex = queueFamilyIndex
    };

    VkCommandPool cmdPool;
    vik_log_check(vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
    return cmdPool;
  }

  /**
    * Allocate a command buffer from the command pool
    *
    * @param level Level of the new command buffer (primary or secondary)
    * @param (Optional) begin If true, recording on the new command buffer will be started (vkBeginCommandBuffer) (Defaults to false)
    *
    * @return A handle to the allocated command buffer
    */
  VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false) {
    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .level = level,
      .commandBufferCount = 1
    };

    VkCommandBuffer cmdBuffer;
    vik_log_check(vkAllocateCommandBuffers(logicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

    // If requested, also start recording for the new command buffer
    if (begin) {
      VkCommandBufferBeginInfo cmdBufInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
      };
      vik_log_check(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
    }

    return cmdBuffer;
  }

  /**
    * Finish command buffer recording and submit it to a queue
    *
    * @param commandBuffer Command buffer to flush
    * @param queue Queue to submit the command buffer to
    * @param free (Optional) Free the command buffer once it has been submitted (Defaults to true)
    *
    * @note The queue that the command buffer is submitted to must be from the same family index as the pool it was allocated from
    * @note Uses a fence to ensure command buffer has finished executing
    */
  void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true) {
    if (commandBuffer == VK_NULL_HANDLE)
      return;

    vik_log_check(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer
    };

    // Create fence to ensure that the command buffer has finished executing
    VkFenceCreateInfo fenceInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
    };
    VkFence fence;
    vik_log_check(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence));

    // Submit to the queue
    vik_log_check(vkQueueSubmit(queue, 1, &submitInfo, fence));
    // Wait for the fence to signal that command buffer has finished executing
    vik_log_check(vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

    vkDestroyFence(logicalDevice, fence, nullptr);

    if (free)
      vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
  }

  bool enable_if_supported(std::vector<const char*> *extensions,
                           const char* name) {
    if (is_extension_supported(name)) {
      vik_log_d("device: Enabling supported %s.", name);
      extensions->push_back(name);
      return true;
    } else {
      vik_log_w("device: %s not supported.", name);
      return false;
    }
  }

  void print_supported_extensions() {
    vik_log_i("Supported device extensions");
    for (auto extension : supported_extensions) {
      vik_log_i("%s", extension.c_str());
    }
  }

  bool is_extension_supported(std::string extension) {
    return std::find(supported_extensions.begin(),
                     supported_extensions.end(),
                     extension) != supported_extensions.end();
  }

  void print_multiview_properties(VkInstance instance) {
    PFN_vkGetPhysicalDeviceFeatures2KHR fpGetPhysicalDeviceFeatures2KHR;
    PFN_vkGetPhysicalDeviceProperties2KHR fpGetPhysicalDeviceProperties2KHR;

    GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceFeatures2KHR);
    // also retrievable from device
    // GET_DEVICE_PROC_ADDR(logicalDevice, GetPhysicalDeviceFeatures2KHR);
    GET_INSTANCE_PROC_ADDR(instance, GetPhysicalDeviceProperties2KHR);

    if (fpGetPhysicalDeviceFeatures2KHR) {
      VkPhysicalDeviceMultiviewFeaturesKHR multi_view_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR
      };

      VkPhysicalDeviceFeatures2KHR device_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
        .pNext = &multi_view_features
      };

      fpGetPhysicalDeviceFeatures2KHR(physicalDevice, &device_features);

      vik_log_i("multiview %d", multi_view_features.multiview);
      vik_log_i("multiviewGeometryShader %d",
                multi_view_features.multiviewGeometryShader);
      vik_log_i("multiviewTessellationShader %d",
                multi_view_features.multiviewTessellationShader);
    } else {
      vik_log_w("vkGetPhysicalDeviceFeatures2KHR extension not found.");
    }

    if (fpGetPhysicalDeviceProperties2KHR) {
      VkPhysicalDeviceMultiviewPropertiesKHR multi_view_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR
      };

      VkPhysicalDeviceProperties2KHR device_props = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
        .pNext = &multi_view_props
      };

      fpGetPhysicalDeviceProperties2KHR(physicalDevice, &device_props);

      vik_log_i("maxMultiviewViewCount %d",
                multi_view_props.maxMultiviewViewCount);
      vik_log_i("maxMultiviewInstanceIndex %d",
                multi_view_props.maxMultiviewInstanceIndex);

      VkPhysicalDeviceMultiviewPerViewAttributesPropertiesNVX multi_view_props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX
      };

      VkPhysicalDeviceProperties2KHR device_props2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
        .pNext = &multi_view_props2
      };
      fpGetPhysicalDeviceProperties2KHR(physicalDevice, &device_props2);

      vik_log_i("perViewPositionAllComponents %d",
                multi_view_props2.perViewPositionAllComponents);
    } else {
      vik_log_w("vkGetPhysicalDeviceProperties2KHR extension not found.");
    }
  }
};
}  // namespace vik
