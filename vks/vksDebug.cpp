/*
* Vulkan examples debug wrapper
* 
* Appendix for VK_EXT_Debug_Report can be found at https://github.com/KhronosGroup/Vulkan-Docs/blob/1.0-VK_EXT_debug_report/doc/specs/vulkan/appendices/debug_report.txt
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <iostream>
#include <string>

#include "vksDebug.hpp"

namespace vks {
namespace debug {

void setupDebugging(VkInstance instance, VkDebugReportFlagsEXT flags, VkDebugReportCallbackEXT callBack) {
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

void freeDebugCallback(VkInstance instance) {
  if (msgCallback != VK_NULL_HANDLE)
    DestroyDebugReportCallback(instance, msgCallback, nullptr);
}
}  // namespace debug

namespace debugmarker {
bool active = false;

PFN_vkDebugMarkerSetObjectTagEXT pfnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
PFN_vkDebugMarkerSetObjectNameEXT pfnDebugMarkerSetObjectName = VK_NULL_HANDLE;
PFN_vkCmdDebugMarkerBeginEXT pfnCmdDebugMarkerBegin = VK_NULL_HANDLE;
PFN_vkCmdDebugMarkerEndEXT pfnCmdDebugMarkerEnd = VK_NULL_HANDLE;
PFN_vkCmdDebugMarkerInsertEXT pfnCmdDebugMarkerInsert = VK_NULL_HANDLE;

void setup(VkDevice device) {
  pfnDebugMarkerSetObjectTag = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT"));
  pfnDebugMarkerSetObjectName = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT"));
  pfnCmdDebugMarkerBegin = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
  pfnCmdDebugMarkerEnd = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
  pfnCmdDebugMarkerInsert = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));

  // Set flag if at least one function pointer is present
  active = (pfnDebugMarkerSetObjectName != VK_NULL_HANDLE);
}

void setObjectName(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, const char *name) {
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

void setObjectTag(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag) {
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

void beginRegion(VkCommandBuffer cmdbuffer, const char* pMarkerName, glm::vec4 color) {
  // Check for valid function pointer (may not be present if not running in a debugging application)
  if (pfnCmdDebugMarkerBegin) {
    VkDebugMarkerMarkerInfoEXT markerInfo = {};
    markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
    markerInfo.pMarkerName = pMarkerName;
    pfnCmdDebugMarkerBegin(cmdbuffer, &markerInfo);
  }
}

void insert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color) {
  // Check for valid function pointer (may not be present if not running in a debugging application)
  if (pfnCmdDebugMarkerInsert) {
    VkDebugMarkerMarkerInfoEXT markerInfo = {};
    markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
    markerInfo.pMarkerName = markerName.c_str();
    pfnCmdDebugMarkerInsert(cmdbuffer, &markerInfo);
  }
}

void endRegion(VkCommandBuffer cmdBuffer) {
  // Check for valid function (may not be present if not runnin in a debugging application)
  if (pfnCmdDebugMarkerEnd)
    pfnCmdDebugMarkerEnd(cmdBuffer);
}


}  // namespace debugmarker
}  // namespace vks

