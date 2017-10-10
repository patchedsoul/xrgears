/*
* Class wrapping access to the swap chain
* 
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <wayland-client.h>
#include <vulkan/vulkan.h>

#include <string>
#include <fstream>
#include <vector>

#include "vikTools.hpp"

#include "system/vikLog.hpp"
#include "vikSwapChainVK.hpp"

namespace vik {

class SwapChainVkComplex : public vik::SwapChainVK {
 public:
  // Index of the deteced graphics and presenting device queue
  /** @brief Queue family index of the detected graphics and presenting device queue */
  uint32_t queueNodeIndex = UINT32_MAX;

  std::function<void(uint32_t width, uint32_t height)> dimension_cb;
  void set_dimension_cb(std::function<void(uint32_t width, uint32_t height)> cb) {
    dimension_cb = cb;
  }

  uint32_t get_queue_index() {
    return queueNodeIndex;
  }

  void select_queue() {
    // Get available queue family properties
    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueCount, NULL);
    assert(queueCount >= 1);

    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueCount, queueProps.data());

    // Iterate over each queue to learn whether it supports presenting:
    // Find a queue with present support
    // Will be used to present the swap chain images to the windowing system
    std::vector<VkBool32> supportsPresent(queueCount);
    for (uint32_t i = 0; i < queueCount; i++)
      vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supportsPresent[i]);

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueCount; i++) {
      if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
        if (graphicsQueueNodeIndex == UINT32_MAX)
          graphicsQueueNodeIndex = i;

        if (supportsPresent[i] == VK_TRUE) {
          graphicsQueueNodeIndex = i;
          presentQueueNodeIndex = i;
          break;
        }
      }
    }
    if (presentQueueNodeIndex == UINT32_MAX) {
      // If there's no queue that supports both present and graphics
      // try to find a separate present queue
      for (uint32_t i = 0; i < queueCount; ++i) {
        if (supportsPresent[i] == VK_TRUE) {
          presentQueueNodeIndex = i;
          break;
        }
      }
    }

    // Exit if either a graphics or a presenting queue hasn't been found
    if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
      vik_log_f("Could not find a graphics and/or presenting queue!");

    // todo : Add support for separate graphics and presenting queue
    if (graphicsQueueNodeIndex != presentQueueNodeIndex)
      vik_log_f("Separate graphics and presenting queues are not supported yet!");

    queueNodeIndex = graphicsQueueNodeIndex;
  }

  VkExtent2D select_extent(const VkSurfaceCapabilitiesKHR &caps,
                           uint32_t width, uint32_t height) {
    VkExtent2D extent = {};
    // If width (and height) equals the special value 0xFFFFFFFF,
    // the size of the surface will be set by the swapchain
    if (caps.currentExtent.width == (uint32_t)-1) {
      // If the surface size is undefined, the size is set to
      // the size of the images requested.
      extent.width = width;
      extent.height = height;
    } else {
      extent = caps.currentExtent;
      if (caps.currentExtent.width != width || caps.currentExtent.height != height) {
        vik_log_w("Swap chain extent dimensions differ from requested: %dx%d vs %dx%d",
                  caps.currentExtent.width, caps.currentExtent.height,
                  width, height);
        dimension_cb(caps.currentExtent.width, caps.currentExtent.height);
      }
    }
    return extent;
  }

  // Determine the number of swapchain images
  uint32_t select_image_count(const VkSurfaceCapabilitiesKHR &surfCaps) {
    uint32_t count = surfCaps.minImageCount + 1;
    if ((surfCaps.maxImageCount > 0) && (count > surfCaps.maxImageCount))
      count = surfCaps.maxImageCount;
    return count;
  }

  // Find the transformation of the surface
  VkSurfaceTransformFlagBitsKHR select_transform_flags(const VkSurfaceCapabilitiesKHR &surfCaps) {
    if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
      // We prefer a non-rotated transform
      return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    else
      return surfCaps.currentTransform;
  }

  // Find a supported composite alpha format (not all devices support alpha opaque)
  VkCompositeAlphaFlagBitsKHR select_composite_alpha(const VkSurfaceCapabilitiesKHR &surfCaps) {
    // Simply select the first composite alpha format available
    std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };

    for (auto& compositeAlphaFlag : compositeAlphaFlags)
      if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag)
        return compositeAlphaFlag;

    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  }

  bool is_blit_supported() {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(physical_device, surface_format.format, &formatProps);
    return formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT;
  }



  /**
  * Create the swapchain and get it's images with given width and height
  *
  * @param width Pointer to the width of the swapchain (may be adjusted to fit the requirements of the swapchain)
  * @param height Pointer to the height of the swapchain (may be adjusted to fit the requirements of the swapchain)
  * @param vsync (Optional) Can be used to force vsync'd rendering (by using VK_PRESENT_MODE_FIFO_KHR as presentation mode)
  */
  void create(uint32_t width, uint32_t height) {
    // Get physical device surface properties and formats
    VkSurfaceCapabilitiesKHR surfCaps;
    vik_log_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surfCaps));

    VkSwapchainCreateInfoKHR swap_chain_info = {};
    swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

    swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swap_chain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_chain_info.imageArrayLayers = 1;
    swap_chain_info.queueFamilyIndexCount = 0;
    // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
    swap_chain_info.clipped = VK_TRUE;

    swap_chain_info.surface = surface;
    swap_chain_info.imageFormat = surface_format.format;
    swap_chain_info.imageColorSpace = surface_format.colorSpace;

    VkSwapchainKHR oldSwapchain = swap_chain;
    swap_chain_info.oldSwapchain = oldSwapchain;

    VkExtent2D swapchainExtent = select_extent(surfCaps, width, height);
    swap_chain_info.imageExtent = { swapchainExtent.width, swapchainExtent.height };

    swap_chain_info.minImageCount = select_image_count(surfCaps);
    swap_chain_info.preTransform = select_transform_flags(surfCaps);
    swap_chain_info.presentMode = select_present_mode();
    swap_chain_info.compositeAlpha = select_composite_alpha(surfCaps);

    // Set additional usage flag for blitting from the swapchain images if supported
    if (is_blit_supported())
      swap_chain_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    vik_log_check(vkCreateSwapchainKHR(device, &swap_chain_info, nullptr, &swap_chain));

    // If an existing swap chain is re-created, destroy the old swap chain
    // This also cleans up all the presentable images
    if (oldSwapchain != VK_NULL_HANDLE)
      destroy_old(oldSwapchain);

    update_images();
  }
};
}  // namespace vik
