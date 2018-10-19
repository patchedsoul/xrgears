 /*
 * vitamin-k
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <string>
#include <cstring>
#include <fstream>
#include <vector>
#include <sstream>

namespace vik {
namespace debug {
// Default validation layers
// On desktop the LunarG loaders exposes a meta layer that contains all layers
static int32_t validationLayerCount = 1;
static const char *validationLayerNames[] = {
  "VK_LAYER_LUNARG_standard_validation"
};

static PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = VK_NULL_HANDLE;
static PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback = VK_NULL_HANDLE;
static PFN_vkDebugReportMessageEXT dbgBreakCallback = VK_NULL_HANDLE;

static VkDebugReportCallbackEXT msgCallback;

// Default debug callback
static VkBool32 messageCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t srcObject,
    size_t location,
    int32_t msgCode,
    const char* pLayerPrefix,
    const char* pMsg,
    void* pUserData) {
  // Select prefix depending on flags passed to the callback
  // Note that multiple flags may be set for a single validation message
  std::string prefix("");

  // Error that may result in undefined behaviour
  if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    prefix += "ERROR:";
  // Warnings may hint at unexpected / non-spec API usage
  if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    prefix += "WARNING:";
  // May indicate sub-optimal usage of the API
  if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    prefix += "PERFORMANCE:";
  // Informal messages that may become handy during debugging
  if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    prefix += "INFO:";
  // Diagnostic info from the Vulkan loader and layers
  // Usually not helpful in terms of API usage, but may help to debug layer and loader problems
  if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    prefix += "DEBUG:";

  // Display message to default output (console/logcat)
  std::stringstream debugMessage;
  debugMessage << prefix << " [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg;

  if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    std::cerr << debugMessage.str() << "\n";
  else
    std::cout << debugMessage.str() << "\n";

  fflush(stdout);

  // The return value of this callback controls wether the Vulkan call that caused
  // the validation message will be aborted or not
  // We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message
  // (and return a VkResult) to abort
  // If you instead want to have calls abort, pass in VK_TRUE and the function will
  // return VK_ERROR_VALIDATION_FAILED_EXT
  return VK_FALSE;
}

// Load debug function pointers and set debug callback
// if callBack is NULL, default message callback will be used
static void setupDebugging(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportCallbackEXT callBack) {
  CreateDebugReportCallback = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
  DestroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
  dbgBreakCallback = reinterpret_cast<PFN_vkDebugReportMessageEXT>(vkGetInstanceProcAddr(instance, "vkDebugReportMessageEXT"));

  VkDebugReportCallbackCreateInfoEXT dbgCreateInfo = {};
  dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
  dbgCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)messageCallback;
  dbgCreateInfo.flags = flags;

  VkResult err = CreateDebugReportCallback(
        instance,
        &dbgCreateInfo,
        nullptr,
        (callBack != VK_NULL_HANDLE) ? &callBack : &msgCallback);
  assert(!err);
}

// Clear debug callback
static void freeDebugCallback(VkInstance instance) {
  if (msgCallback != VK_NULL_HANDLE)
    DestroyDebugReportCallback(instance, msgCallback, nullptr);
}
}  // namespace debug

// Setup and functions for the VK_EXT_debug_marker_extension
// Extension spec can be found at https://github.com/KhronosGroup/Vulkan-Docs/blob/1.0-VK_EXT_debug_marker/doc/specs/vulkan/appendices/VK_EXT_debug_marker.txt
// Note that the extension will only be present if run from an offline debugging application
// The actual check for extension presence and enabling it on the device is done in the example base class
// See VulkanExampleBase::createInstance and VulkanExampleBase::createDevice (base/vulkanexamplebase.cpp)
namespace debugmarker {
// Set to true if function pointer for the debug marker are available
static bool active = false;

static PFN_vkDebugMarkerSetObjectTagEXT pfnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
static PFN_vkDebugMarkerSetObjectNameEXT pfnDebugMarkerSetObjectName = VK_NULL_HANDLE;
static PFN_vkCmdDebugMarkerBeginEXT pfnCmdDebugMarkerBegin = VK_NULL_HANDLE;
static PFN_vkCmdDebugMarkerEndEXT pfnCmdDebugMarkerEnd = VK_NULL_HANDLE;
static PFN_vkCmdDebugMarkerInsertEXT pfnCmdDebugMarkerInsert = VK_NULL_HANDLE;

// Get function pointers for the debug report extensions from the device
static void setup(VkDevice device) {
  pfnDebugMarkerSetObjectTag = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT"));
  pfnDebugMarkerSetObjectName = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT"));
  pfnCmdDebugMarkerBegin = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
  pfnCmdDebugMarkerEnd = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
  pfnCmdDebugMarkerInsert = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));

  // Set flag if at least one function pointer is present
  active = (pfnDebugMarkerSetObjectName != VK_NULL_HANDLE);
}

// Sets the debug name of an object
// All Objects in Vulkan are represented by their 64-bit handles which are passed into this function
// along with the object type
/*
static void setObjectName(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, const char *name) {
  // Check for valid function pointer (may not be present if not running in a debugging application)
  if (pfnDebugMarkerSetObjectName) {
    VkDebugMarkerObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.object = object;
    nameInfo.pObjectName = name;
    pfnDebugMarkerSetObjectName(device, &nameInfo);
  }
}
// Set the tag for an object
static void setObjectTag(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag) {
  // Check for valid function pointer (may not be present if not running in a debugging application)
  if (pfnDebugMarkerSetObjectTag) {
    VkDebugMarkerObjectTagInfoEXT tagInfo = {};
    tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
    tagInfo.objectType = objectType;
    tagInfo.object = object;
    tagInfo.tagName = name;
    tagInfo.tagSize = tagSize;
    tagInfo.pTag = tag;
    pfnDebugMarkerSetObjectTag(device, &tagInfo);
  }
}
*/

// Start a new debug marker region
static void beginRegion(VkCommandBuffer cmdbuffer, const char* pMarkerName, glm::vec4 color) {
  // Check for valid function pointer (may not be present if not running in a debugging application)
  if (pfnCmdDebugMarkerBegin) {
    VkDebugMarkerMarkerInfoEXT markerInfo = {};
    markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
    markerInfo.pMarkerName = pMarkerName;
    pfnCmdDebugMarkerBegin(cmdbuffer, &markerInfo);
  }
}
/*
// Insert a new debug marker into the command buffer
static void insert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color) {
  // Check for valid function pointer (may not be present if not running in a debugging application)
  if (pfnCmdDebugMarkerInsert) {
    VkDebugMarkerMarkerInfoEXT markerInfo = {};
    markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
    markerInfo.pMarkerName = markerName.c_str();
    pfnCmdDebugMarkerInsert(cmdbuffer, &markerInfo);
  }
}
*/
// End the current debug marker region
static void endRegion(VkCommandBuffer cmdBuffer) {
  // Check for valid function (may not be present if not runnin in a debugging application)
  if (pfnCmdDebugMarkerEnd)
    pfnCmdDebugMarkerEnd(cmdBuffer);
}

// Object specific naming functions
/*
static void setCommandBufferName(VkDevice device, VkCommandBuffer cmdBuffer, const char * name) {
  setObjectName(device, (uint64_t)cmdBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, name);
}

static void setQueueName(VkDevice device, VkQueue queue, const char * name) {
  setObjectName(device, (uint64_t)queue, VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, name);
}

static void setImageName(VkDevice device, VkImage image, const char * name) {
  setObjectName(device, (uint64_t)image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, name);
}

static void setSamplerName(VkDevice device, VkSampler sampler, const char * name) {
  setObjectName(device, (uint64_t)sampler, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, name);
}

static void setBufferName(VkDevice device, VkBuffer buffer, const char * name) {
  setObjectName(device, (uint64_t)buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name);
}

static void setDeviceMemoryName(VkDevice device, VkDeviceMemory memory, const char * name) {
  setObjectName(device, (uint64_t)memory, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, name);
}

static void setShaderModuleName(VkDevice device, VkShaderModule shaderModule, const char * name) {
  setObjectName(device, (uint64_t)shaderModule, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, name);
}

static void setPipelineName(VkDevice device, VkPipeline pipeline, const char * name) {
  setObjectName(device, (uint64_t)pipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, name);
}

static void setPipelineLayoutName(VkDevice device, VkPipelineLayout pipelineLayout, const char * name) {
  setObjectName(device, (uint64_t)pipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, name);
}

static void setRenderPassName(VkDevice device, VkRenderPass renderPass, const char * name) {
  setObjectName(device, (uint64_t)renderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, name);
}

static void setFramebufferName(VkDevice device, VkFramebuffer framebuffer, const char * name) {
  setObjectName(device, (uint64_t)framebuffer, VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, name);
}

static void setDescriptorSetLayoutName(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const char * name) {
  setObjectName(device, (uint64_t)descriptorSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, name);
}

static void setDescriptorSetName(VkDevice device, VkDescriptorSet descriptorSet, const char * name) {
  setObjectName(device, (uint64_t)descriptorSet, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, name);
}

static void setSemaphoreName(VkDevice device, VkSemaphore semaphore, const char * name) {
  setObjectName(device, (uint64_t)semaphore, VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, name);
}

static void setFenceName(VkDevice device, VkFence fence, const char * name) {
  setObjectName(device, (uint64_t)fence, VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, name);
}

static void setEventName(VkDevice device, VkEvent _event, const char * name) {
  setObjectName(device, (uint64_t)_event, VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT, name);
}
*/
}  // namespace debugmarker
}  // namespace vik
