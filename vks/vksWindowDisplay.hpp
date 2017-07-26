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

#include "vksApplication.hpp"

class ApplicationDisplay  : public Application {
public:
  explicit ApplicationDisplay(bool enableValidation) : Application(enableValidation) {}
  ~ApplicationDisplay() {}

  const char* requiredExtensionName() {
    return VK_KHR_DISPLAY_EXTENSION_NAME;
  }

  void initSwapChain() {
    createDirect2DisplaySurface(width, height);
    swapChain.initSurfaceCommon();
  }

  void renderLoop() {
    while (!quit) {
      auto tStart = std::chrono::high_resolution_clock::now();
      if (viewUpdated) {
        viewUpdated = false;
        viewChanged();
      }
      render();
      frameCounter++;
      auto tEnd = std::chrono::high_resolution_clock::now();
      auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
      frameTimer = tDiff / 1000.0f;
      camera.update(frameTimer);
      if (camera.moving())
        viewUpdated = true;
      // Convert to clamped timer value
      if (!paused) {
        timer += timerSpeed * frameTimer;
        if (timer > 1.0)
          timer -= 1.0f;
      }
      fpsTimer += (float)tDiff;
      if (fpsTimer > 1000.0f) {
        lastFPS = frameCounter;
        updateTextOverlay();
        fpsTimer = 0.0f;
        frameCounter = 0;
      }
    }
  }

  void createDirect2DisplaySurface(uint32_t width, uint32_t height) {
    uint32_t displayPropertyCount;

    // Get display property
    vkGetPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &displayPropertyCount, NULL);
    VkDisplayPropertiesKHR* pDisplayProperties = new VkDisplayPropertiesKHR[displayPropertyCount];
    vkGetPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &displayPropertyCount, pDisplayProperties);

    // Get plane property
    uint32_t planePropertyCount;
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, &planePropertyCount, NULL);
    VkDisplayPlanePropertiesKHR* pPlaneProperties = new VkDisplayPlanePropertiesKHR[planePropertyCount];
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, &planePropertyCount, pPlaneProperties);

    VkDisplayKHR display = VK_NULL_HANDLE;
    VkDisplayModeKHR displayMode;
    VkDisplayModePropertiesKHR* pModeProperties;
    bool foundMode = false;

    for(uint32_t i = 0; i < displayPropertyCount;++i) {
      display = pDisplayProperties[i].display;
      uint32_t modeCount;
      vkGetDisplayModePropertiesKHR(physicalDevice, display, &modeCount, NULL);
      pModeProperties = new VkDisplayModePropertiesKHR[modeCount];
      vkGetDisplayModePropertiesKHR(physicalDevice, display, &modeCount, pModeProperties);

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

    if (!foundMode) {
      vks::tools::exitFatal("Can't find a display and a display mode!", "Fatal error");
      return;
    }

    // Search for a best plane we can use
    uint32_t bestPlaneIndex = UINT32_MAX;
    VkDisplayKHR* pDisplays = NULL;
    for (uint32_t i = 0; i < planePropertyCount; i++) {
      uint32_t planeIndex=i;
      uint32_t displayCount;
      vkGetDisplayPlaneSupportedDisplaysKHR(physicalDevice, planeIndex, &displayCount, NULL);
      if (pDisplays)
        delete [] pDisplays;
      pDisplays = new VkDisplayKHR[displayCount];
      vkGetDisplayPlaneSupportedDisplaysKHR(physicalDevice, planeIndex, &displayCount, pDisplays);

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

    if (bestPlaneIndex == UINT32_MAX) {
      vks::tools::exitFatal("Can't find a plane for displaying!", "Fatal error");
      return;
    }

    VkDisplayPlaneCapabilitiesKHR planeCap;
    vkGetDisplayPlaneCapabilitiesKHR(physicalDevice, displayMode, bestPlaneIndex, &planeCap);
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

    VkResult result = vkCreateDisplayPlaneSurfaceKHR(instance, &surfaceInfo, NULL, &swapChain.surface);
    if (result !=VK_SUCCESS)
      vks::tools::exitFatal("Failed to create surface!", "Fatal error");

    delete[] pDisplays;
    delete[] pModeProperties;
    delete[] pDisplayProperties;
    delete[] pPlaneProperties;
  }
};
