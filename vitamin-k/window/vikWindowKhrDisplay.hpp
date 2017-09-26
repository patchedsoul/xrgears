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

#include <vulkan/vulkan.h>

#include "vikWindow.hpp"
#include "render/vikSwapChainVKComplex.hpp"

namespace vik {
class WindowKhrDisplay  : public Window {
 public:
  explicit WindowKhrDisplay() {}
  ~WindowKhrDisplay() {}

  const std::vector<const char*> required_extensions() {
    return { VK_KHR_DISPLAY_EXTENSION_NAME };
  }

  void init_swap_chain(Renderer *r) {

    r->swap_chain = new SwapChainVkComplex();
    SwapChainVkComplex *sc = (SwapChainVkComplex*) r->swap_chain;
    sc->set_context(r->instance, r->physical_device, r->device);

    uint32_t displayPropertyCount;

    // Get display property
    vkGetPhysicalDeviceDisplayPropertiesKHR(r->physical_device, &displayPropertyCount, NULL);
    VkDisplayPropertiesKHR* pDisplayProperties = new VkDisplayPropertiesKHR[displayPropertyCount];
    vkGetPhysicalDeviceDisplayPropertiesKHR(r->physical_device, &displayPropertyCount, pDisplayProperties);

    // Get plane property
    uint32_t planePropertyCount;
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(r->physical_device, &planePropertyCount, NULL);
    VkDisplayPlanePropertiesKHR* pPlaneProperties = new VkDisplayPlanePropertiesKHR[planePropertyCount];
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(r->physical_device, &planePropertyCount, pPlaneProperties);

    VkDisplayKHR display = VK_NULL_HANDLE;
    VkDisplayModeKHR displayMode;
    VkDisplayModePropertiesKHR* pModeProperties;
    bool foundMode = false;

    for (uint32_t i = 0; i < displayPropertyCount; ++i) {
      display = pDisplayProperties[i].display;
      uint32_t modeCount;
      vkGetDisplayModePropertiesKHR(r->physical_device, display, &modeCount, NULL);
      pModeProperties = new VkDisplayModePropertiesKHR[modeCount];
      vkGetDisplayModePropertiesKHR(r->physical_device, display, &modeCount, pModeProperties);

      for (uint32_t j = 0; j < modeCount; ++j) {
        const VkDisplayModePropertiesKHR* mode = &pModeProperties[j];

        if (mode->parameters.visibleRegion.width == r->width && mode->parameters.visibleRegion.height == r->height) {
          displayMode = mode->displayMode;
          foundMode = true;
          break;
        }
      }
      if (foundMode)
        break;
      delete [] pModeProperties;
    }

    vik_log_f_if(!foundMode, "Can't find a display and a display mode!");

    // Search for a best plane we can use
    uint32_t bestPlaneIndex = UINT32_MAX;
    VkDisplayKHR* pDisplays = NULL;
    for (uint32_t i = 0; i < planePropertyCount; i++) {
      uint32_t planeIndex = i;
      uint32_t displayCount;
      vkGetDisplayPlaneSupportedDisplaysKHR(r->physical_device, planeIndex, &displayCount, NULL);
      if (pDisplays)
        delete [] pDisplays;
      pDisplays = new VkDisplayKHR[displayCount];
      vkGetDisplayPlaneSupportedDisplaysKHR(r->physical_device, planeIndex, &displayCount, pDisplays);

      // Find a display that matches the current plane
      bestPlaneIndex = UINT32_MAX;
      for (uint32_t j = 0; j < displayCount; j++)
        if (display == pDisplays[j]) {
          bestPlaneIndex = i;
          break;
        }
      if (bestPlaneIndex != UINT32_MAX)
        break;
    }

    vik_log_f_if(bestPlaneIndex == UINT32_MAX, "Can't find a plane for displaying!");

    VkDisplayPlaneCapabilitiesKHR planeCap;
    vkGetDisplayPlaneCapabilitiesKHR(r->physical_device, displayMode, bestPlaneIndex, &planeCap);
    VkDisplayPlaneAlphaFlagBitsKHR alphaMode;

    if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR)
      alphaMode = VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR;
    else if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR)
      alphaMode = VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR;
    else
      alphaMode = VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR;

    VkDisplaySurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.pNext = NULL;
    surfaceInfo.flags = 0;
    surfaceInfo.displayMode = displayMode;
    surfaceInfo.planeIndex = bestPlaneIndex;
    surfaceInfo.planeStackIndex = pPlaneProperties[bestPlaneIndex].currentStackIndex;
    surfaceInfo.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    surfaceInfo.globalAlpha = 1.0;
    surfaceInfo.alphaMode = alphaMode;
    surfaceInfo.imageExtent.width = r->width;
    surfaceInfo.imageExtent.height = r->height;

    VkResult result = vkCreateDisplayPlaneSurfaceKHR(r->instance, &surfaceInfo, NULL, &sc->surface);
    vik_log_f_if(result !=VK_SUCCESS, "Failed to create surface!");

    delete[] pDisplays;
    delete[] pModeProperties;
    delete[] pDisplayProperties;
    delete[] pPlaneProperties;

    sc->select_queue_and_format();
  }

  void update_window_title(const std::string& title) {}

  int init(Renderer *r) {}
  void iterate(Renderer *r) {}

  VkBool32 check_support(VkPhysicalDevice physical_device) {
    return true;
  }
};
}
