/*
 * XRGears
 *
 * Copyright (C) 2016 Sascha Willems - www.saschawillems.de
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Based on Vulkan Examples written by Sascha Willems
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

namespace vks {
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
void setupDebugging(
    VkInstance instance,
    VkDebugReportFlagsEXT flags,
    VkDebugReportCallbackEXT callBack);
// Clear debug callback
void freeDebugCallback(VkInstance instance);
}  // namespace debug

// Setup and functions for the VK_EXT_debug_marker_extension
// Extension spec can be found at https://github.com/KhronosGroup/Vulkan-Docs/blob/1.0-VK_EXT_debug_marker/doc/specs/vulkan/appendices/VK_EXT_debug_marker.txt
// Note that the extension will only be present if run from an offline debugging application
// The actual check for extension presence and enabling it on the device is done in the example base class
// See VulkanExampleBase::createInstance and VulkanExampleBase::createDevice (base/vulkanexamplebase.cpp)
namespace debugmarker {
// Set to true if function pointer for the debug marker are available
extern bool active;

// Get function pointers for the debug report extensions from the device
void setup(VkDevice device);

// Sets the debug name of an object
// All Objects in Vulkan are represented by their 64-bit handles which are passed into this function
// along with the object type
void setObjectName(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, const char *name);

// Set the tag for an object
void setObjectTag(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag);

// Start a new debug marker region
void beginRegion(VkCommandBuffer cmdbuffer, const char* pMarkerName, glm::vec4 color);

// Insert a new debug marker into the command buffer
void insert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color);

// End the current debug marker region
void endRegion(VkCommandBuffer cmdBuffer);

// Object specific naming functions
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
}  // namespace debugmarker
}  // namespace vks
