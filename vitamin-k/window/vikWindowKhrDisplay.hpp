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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/extensions/Xrandr.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib_xrandr.h>

#include <string>
#include <vector>

#include "vikWindow.hpp"
#include "../render/vikSwapChainVK.hpp"

namespace vik {
class WindowKhrDisplay  : public Window {
  SwapChainVK swap_chain;

 public:
  explicit WindowKhrDisplay(Settings *s) : Window(s) {
    name = "khr-display";
  }
  ~WindowKhrDisplay() {}

  const std::vector<const char*> required_extensions() {
    return {
      VK_KHR_DISPLAY_EXTENSION_NAME,
      VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME,
      VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
      VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME
    };
  }

  const std::vector<const char*> required_device_extensions() {
    return {
      VK_KHR_DISPLAY_SWAPCHAIN_EXTENSION_NAME,
      VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME
    };
  }

  void print_displays (VkDisplayPropertiesKHR* display_properties,
                       uint32_t display_property_count) {
    /* print available */
    VkPhysicalDevice physical_device = swap_chain.physical_device;

    for (uint32_t i = 0; i < display_property_count; ++i) {
      VkDisplayKHR display = display_properties[i].display;
      uint32_t modeCount;
      vkGetDisplayModePropertiesKHR(physical_device, display, &modeCount, nullptr);
      vik_log_i("Found display %p %s (%d modes)",
                display,
                display_properties[i].displayName,
                modeCount);
      vik_log_i("physicalDimensions %dx%d",
                display_properties[i].physicalDimensions.width,
                display_properties[i].physicalDimensions.height);
      vik_log_i("physicalResolution %dx%d",
                display_properties[i].physicalResolution.width,
                display_properties[i].physicalResolution.height);

      VkDisplayPowerStateEXT power_state = get_display_power_state(display);
      vik_log_i("power state %s", Log::power_state_string(power_state).c_str());

      uint32_t mode_count;
      vkGetDisplayModePropertiesKHR(physical_device, display,
                                    &mode_count, nullptr);

      vik_log_i("Found %d VkDisplayModePropertiesKHR", mode_count);
      VkDisplayModePropertiesKHR* mode_properties
          = new VkDisplayModePropertiesKHR[mode_count];
      vkGetDisplayModePropertiesKHR(physical_device, display,
                                    &mode_count, mode_properties);

      for (uint32_t j = 0; j < mode_count; ++j) {
        const VkDisplayModePropertiesKHR* mode = &mode_properties[j];
          vik_log_i("Mode %d. %dx%d@%d",
                    j,
                    mode->parameters.visibleRegion.width,
                    mode->parameters.visibleRegion.height,
                    mode->parameters.refreshRate);
      }
    }
  }

  void init_swap_chain(uint32_t width, uint32_t height) {
    uint32_t display_property_count;

    VkPhysicalDevice physical_device = swap_chain.physical_device;

    // Get display property
    VkResult res = vkGetPhysicalDeviceDisplayPropertiesKHR(physical_device,
                                                           &display_property_count,
                                                           nullptr);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not get Physical Device Display Properties: %s",
                 Log::result_string(res).c_str());

    vik_log_i("Found %d display properites.", display_property_count);

    VkDisplayPropertiesKHR* display_properties =
      new VkDisplayPropertiesKHR[display_property_count];

    res = vkGetPhysicalDeviceDisplayPropertiesKHR(physical_device,
                                                  &display_property_count,
                                                  display_properties);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not get Physical Device Display Properties: %s",
                 Log::result_string(res).c_str());

    print_displays (display_properties, display_property_count);

    VkDisplayKHR display = nullptr;
    VkDisplayModeKHR displayMode;
    VkDisplayModePropertiesKHR* mode_properties;
    bool foundMode = false;

    for (uint32_t i = 0; i < display_property_count; ++i) {

      display = display_properties[i].display;
      uint32_t mode_count;
      vkGetDisplayModePropertiesKHR(physical_device, display,
                                    &mode_count, nullptr);

      vik_log_i("Found %d VkDisplayModePropertiesKHR", mode_count);
      mode_properties = new VkDisplayModePropertiesKHR[mode_count];
      vkGetDisplayModePropertiesKHR(physical_device, display,
                                    &mode_count, mode_properties);

      for (uint32_t j = 0; j < mode_count; ++j) {
        const VkDisplayModePropertiesKHR* mode = &mode_properties[j];

        if (mode->parameters.visibleRegion.width == width
            && mode->parameters.visibleRegion.height == height) {

          vik_log_i("Using display %s (%d modes) with mode %d",
                    display_properties[i].displayName,
                    mode_count, j);

          displayMode = mode->displayMode;
          foundMode = true;
          break;
        }
      }
      if (foundMode)
        break;
      delete [] mode_properties;
    }

    vik_log_f_if(!foundMode, "Can't find a display and a display mode!");

    aquire_xlib_display(display);

    // Get plane property
    uint32_t plane_property_count;
    res = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical_device,
                                                      &plane_property_count,
                                                       nullptr);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not vkGetPhysicalDeviceDisplayPlanePropertiesKHR: %s",
                 Log::result_string(res).c_str());

    vik_log_i("Found %d plane properites.", display_property_count);

    VkDisplayPlanePropertiesKHR* plane_properties =
        new VkDisplayPlanePropertiesKHR[plane_property_count];

    res = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical_device,
                                                      &plane_property_count,
                                                       plane_properties);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not vkGetPhysicalDeviceDisplayPlanePropertiesKHR: %s",
                 Log::result_string(res).c_str());

    // Search for a best plane we can use
    uint32_t bestPlaneIndex = UINT32_MAX;
    VkDisplayKHR* pDisplays = nullptr;
    for (uint32_t i = 0; i < plane_property_count; i++) {
      uint32_t planeIndex = i;
      uint32_t displayCount;
      vkGetDisplayPlaneSupportedDisplaysKHR(physical_device, planeIndex, &displayCount, nullptr);
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
    surfaceInfo.flags = 0;
    surfaceInfo.displayMode = displayMode;
    surfaceInfo.planeIndex = bestPlaneIndex;
    surfaceInfo.planeStackIndex = plane_properties[bestPlaneIndex].currentStackIndex;
    surfaceInfo.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    surfaceInfo.globalAlpha = 1.0;
    surfaceInfo.alphaMode = alphaMode;
    surfaceInfo.imageExtent.width = width;
    surfaceInfo.imageExtent.height = height;

    VkResult result = vkCreateDisplayPlaneSurfaceKHR(swap_chain.instance, &surfaceInfo, nullptr, &swap_chain.surface);
    vik_log_f_if(result !=VK_SUCCESS, "Failed to create surface!");

    delete[] pDisplays;
    delete[] mode_properties;
    delete[] display_properties;
    delete[] plane_properties;

    swap_chain.set_settings(settings);
    swap_chain.select_surface_format();
    swap_chain.create(width, height);

    vik_log_i("vkGetSwapchainCounterEXT: %d", get_swap_chain_counter());
  }

  uint64_t get_swap_chain_counter () {
    PFN_vkGetSwapchainCounterEXT fun =
        (PFN_vkGetSwapchainCounterEXT) vkGetDeviceProcAddr(swap_chain.device,
                                                           "vkGetSwapchainCounterEXT");
    vik_log_f_if(fun == nullptr,
                 "Could not Get Device Proc Addr vkDisplayPowerControlEXT.");

    uint64_t counter_value;
    VkResult res = fun(
        swap_chain.device,
        swap_chain.swap_chain,
        VK_SURFACE_COUNTER_VBLANK_EXT,
       &counter_value);

    vik_log_f_if(res != VK_SUCCESS,
                 "Could not vkGetSwapchainCounterEXT: %s",
                 Log::result_string(res).c_str());

    return counter_value;
  }

  VkDisplayPowerStateEXT get_display_power_state(VkDisplayKHR display) {
    VkDisplayPowerInfoEXT power_info = {
      .sType = VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT
    };

    PFN_vkDisplayPowerControlEXT fun =
        (PFN_vkDisplayPowerControlEXT) vkGetDeviceProcAddr(swap_chain.device,
                                                           "vkDisplayPowerControlEXT");
    vik_log_f_if(fun == nullptr,
                 "Could not Get Device Proc Addr vkDisplayPowerControlEXT.");

    VkResult res = fun(swap_chain.device, display, &power_info);

    vik_log_f_if(res != VK_SUCCESS,
                 "Could not vkDisplayPowerControlEXT: %s",
                 Log::result_string(res).c_str());

     return power_info.powerState;
  }

  void aquire_xlib_display(VkDisplayKHR display) {
    Display *dpy = XOpenDisplay(nullptr);
    vik_log_f_if(dpy == nullptr, "Could not open X display.");

    PFN_vkAcquireXlibDisplayEXT fun =
        (PFN_vkAcquireXlibDisplayEXT)vkGetInstanceProcAddr(swap_chain.instance,
                                                           "vkAcquireXlibDisplayEXT");
    vik_log_f_if(fun == nullptr,
                 "Could not Get Device Proc Addr vkAcquireXlibDisplayEXT.");

    VkResult res = fun(
        swap_chain.physical_device,
        dpy,
        display);

    vik_log_f_if(res != VK_SUCCESS,
                 "Could not acquire Xlib display %p: %s",
                 display,
                 Log::result_string(res).c_str());
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
  }

  void update_window_title(const std::string& title) {}

  int init() {
    return 0;
  }

  void iterate() {
    render_frame_cb();
  }

  VkBool32 check_support(VkPhysicalDevice physical_device) {
    return true;
  }
};
}  // namespace vik
