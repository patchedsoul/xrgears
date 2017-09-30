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

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

#include "vikWindow.hpp"
#include "../render/vikSwapChainVKComplex.hpp"

namespace vik {
class WindowKhrDisplay  : public Window {
  SwapChainVkComplex swap_chain;

 public:
  explicit WindowKhrDisplay(Settings *s) : Window(s) {}
  ~WindowKhrDisplay() {}

  const std::vector<const char*> required_extensions() {
    return { VK_KHR_DISPLAY_EXTENSION_NAME };
  }

  void init_swap_chain(uint32_t width, uint32_t height) {
    uint32_t displayPropertyCount;

    VkPhysicalDevice physical_device = swap_chain.physical_device;

    // Get display property
    vkGetPhysicalDeviceDisplayPropertiesKHR(physical_device, &displayPropertyCount, NULL);
    VkDisplayPropertiesKHR* pDisplayProperties = new VkDisplayPropertiesKHR[displayPropertyCount];
    vkGetPhysicalDeviceDisplayPropertiesKHR(physical_device, &displayPropertyCount, pDisplayProperties);

    // Get plane property
    uint32_t planePropertyCount;
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical_device, &planePropertyCount, NULL);
    VkDisplayPlanePropertiesKHR* pPlaneProperties = new VkDisplayPlanePropertiesKHR[planePropertyCount];
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical_device, &planePropertyCount, pPlaneProperties);

    VkDisplayKHR display = VK_NULL_HANDLE;
    VkDisplayModeKHR displayMode;
    VkDisplayModePropertiesKHR* pModeProperties;
    bool foundMode = false;

    for (uint32_t i = 0; i < displayPropertyCount; ++i) {
      display = pDisplayProperties[i].display;
      uint32_t modeCount;
      vkGetDisplayModePropertiesKHR(physical_device, display, &modeCount, NULL);
      pModeProperties = new VkDisplayModePropertiesKHR[modeCount];
      vkGetDisplayModePropertiesKHR(physical_device, display, &modeCount, pModeProperties);

      for (uint32_t j = 0; j < modeCount; ++j) {
        const VkDisplayModePropertiesKHR* mode = &pModeProperties[j];

        if (mode->parameters.visibleRegion.width == width && mode->parameters.visibleRegion.height == height) {
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
      vkGetDisplayPlaneSupportedDisplaysKHR(physical_device, planeIndex, &displayCount, NULL);
      if (pDisplays)
        delete [] pDisplays;
      pDisplays = new VkDisplayKHR[displayCount];
      vkGetDisplayPlaneSupportedDisplaysKHR(physical_device, planeIndex, &displayCount, pDisplays);

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
    vkGetDisplayPlaneCapabilitiesKHR(physical_device, displayMode, bestPlaneIndex, &planeCap);
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
    surfaceInfo.imageExtent.width = width;
    surfaceInfo.imageExtent.height = height;

    VkResult result = vkCreateDisplayPlaneSurfaceKHR(swap_chain.instance, &surfaceInfo, NULL, &swap_chain.surface);
    vik_log_f_if(result !=VK_SUCCESS, "Failed to create surface!");

    delete[] pDisplays;
    delete[] pModeProperties;
    delete[] pDisplayProperties;
    delete[] pPlaneProperties;

    swap_chain.select_queue();
    swap_chain.select_surface_format();
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
  }

  void update_window_title(const std::string& title) {}

  int init(uint32_t width, uint32_t height) {
    return 0;
  }

  void iterate() {}

  VkBool32 check_support(VkPhysicalDevice physical_device) {
    return true;
  }
};
}  // namespace vik
