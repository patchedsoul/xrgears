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

#include <vulkan/vulkan.h>

#include <vector>
#include <map>
#include <utility>

#include "vikSwapChain.hpp"

namespace vik {
class SwapChainVK : public SwapChain {
 public:
  /** @brief Handle to the current swap chain, required for recreation */
  VkSwapchainKHR swap_chain = VK_NULL_HANDLE;

  VkSurfaceKHR surface;

  SwapChainVK() {}
  ~SwapChainVK() {}

  std::function<void(uint32_t width, uint32_t height)> dimension_cb;
  void set_dimension_cb(std::function<void(uint32_t width, uint32_t height)> cb) {
    dimension_cb = cb;
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

    VkBool32 supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, 0, surface, &supported);
    assert(supported);

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

    create_image_views();
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

  void recreate(uint32_t width, uint32_t height) {
    destroy();
    create(width, height);
    create_image_views();
  }

  void destroy() {
    if (image_count > 0) {
      vkDestroySwapchainKHR(device, swap_chain, NULL);
      image_count = 0;
    }
  }

  void destroy_old(const VkSwapchainKHR &sc) {
    for (uint32_t i = 0; i < image_count; i++)
      vkDestroyImageView(device, buffers[i].view, nullptr);
    vkDestroySwapchainKHR(device, sc, nullptr);
  }

  /**
  * Acquires the next image in the swap chain
  *
  * @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is ready for use
  * @param imageIndex Pointer to the image index that will be increased if the next image could be acquired
  *
  * @note The function will always wait until the next image has been acquired by setting timeout to UINT64_MAX
  *
  * @return VkResult of the image acquisition
  */
  VkResult acquire_next_image(VkSemaphore semaphore, uint32_t *index) {
    // By setting timeout to UINT64_MAX we will always
    // wait until the next image has been acquired or an actual error is thrown
    // With that we don't have to handle VK_NOT_READY
    return vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX,
                                 semaphore, VK_NULL_HANDLE, index);
  }

  /**
  * Queue an image for presentation
  *
  * @param queue Presentation queue for presenting the image
  * @param imageIndex Index of the swapchain image to queue for presentation
  * @param waitSemaphore (Optional) Semaphore that is waited on before the image is presented (only used if != VK_NULL_HANDLE)
  *
  * @return VkResult of the queue presentation
  */
  VkResult present(VkQueue queue, uint32_t index, VkSemaphore semaphore = VK_NULL_HANDLE) {
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swap_chain;
    presentInfo.pImageIndices = &index;
    // Check if a wait semaphore has been specified to wait for before presenting the image
    if (semaphore != VK_NULL_HANDLE) {
      presentInfo.pWaitSemaphores = &semaphore;
      presentInfo.waitSemaphoreCount = 1;
    }
    return vkQueuePresentKHR(queue, &presentInfo);
  }

  void print_available_formats() {
    std::vector<VkSurfaceFormatKHR> formats
        = get_surface_formats(physical_device, surface);

    vik_log_i_short("Available formats:");
    for (VkSurfaceFormatKHR format : formats)
      vik_log_i_short("%s (%s)",
                      Log::color_format_string(format.format).c_str(),
                      Log::color_space_string(format.colorSpace).c_str());
  }

  static std::vector<VkSurfaceFormatKHR>
  get_surface_formats(VkPhysicalDevice d, VkSurfaceKHR s) {
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(d, s, &count, NULL);
    assert(count > 0);

    std::vector<VkSurfaceFormatKHR> formats(count);
    vik_log_check(vkGetPhysicalDeviceSurfaceFormatsKHR(
                    d, s, &count, formats.data()));
    return formats;
  }

  virtual void select_surface_format() {
    if (settings->list_formats_and_exit) {
      print_available_formats();
      exit(0);
    }

    std::vector<VkSurfaceFormatKHR> formats
        = get_surface_formats(physical_device, surface);

    std::map<VkFormat, VkSurfaceFormatKHR> format_map;
    for (auto format : formats)
      format_map.insert(
            std::pair<VkFormat, VkSurfaceFormatKHR>(format.format, format));

    auto search = format_map.find(settings->color_format);
    if (search != format_map.end()) {
        surface_format = search->second;
        vik_log_i("Using color format %s (%s)",
                  Log::color_format_string(surface_format.format).c_str(),
                  Log::color_space_string(surface_format.colorSpace).c_str());
    } else {
        vik_log_w("Selected format %s not found, falling back to default.",
                  Log::color_format_string(settings->color_format).c_str());
        auto search2 = format_map.find(VK_FORMAT_B8G8R8A8_UNORM);
        if(search2 != format_map.end()) {
            surface_format = search2->second;
        } else {
            vik_log_e("VK_FORMAT_B8G8R8A8_UNORM format not found.");
            print_available_formats();
            vik_log_f("No usable format set.");
        }
    }

    assert(surface_format.format != VK_FORMAT_UNDEFINED);
  }

  void create_image_views() {
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, NULL);
    assert(image_count > 0);
    vik_log_d("Creating %d image views.", image_count);

    std::vector<VkImage> images(image_count);
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, images.data());

    buffers.resize(image_count);

    for (uint32_t i = 0; i < image_count; i++) {
      buffers[i].image = images[i];
      create_image_view(device, buffers[i].image,
                        surface_format.format, &buffers[i].view);
    }
  }


  void print_available_present_modes() {
    std::vector<VkPresentModeKHR> present_modes =
        get_present_modes(physical_device, surface);

    vik_log_i_short("Available present modes:");
    for (auto mode : present_modes)
        vik_log_i_short("%s", Log::present_mode_string(mode).c_str());
  }

  static std::vector<VkPresentModeKHR>
  get_present_modes(VkPhysicalDevice d, VkSurfaceKHR s) {
    uint32_t count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(d, s, &count, NULL);
    assert(count > 0);

    std::vector<VkPresentModeKHR> present_modes(count);

    vkGetPhysicalDeviceSurfacePresentModesKHR(
          d, s, &count, present_modes.data());
    return present_modes;
  }

  VkPresentModeKHR select_present_mode() {
    if (settings->list_present_modes_and_exit) {
      print_available_present_modes();
      exit(0);
    }

    std::vector<VkPresentModeKHR> present_modes =
        get_present_modes(physical_device, surface);

    if(std::find(present_modes.begin(),
                 present_modes.end(),
                 settings->present_mode) != present_modes.end()) {
      vik_log_i("Using present mode %s",
                Log::present_mode_string(settings->present_mode).c_str());
      return settings->present_mode;
    } else {
      vik_log_w("Present mode %s not available",
                Log::present_mode_string(settings->present_mode).c_str());
      print_available_present_modes();
      vik_log_w("Using %s", Log::present_mode_string(present_modes[0]).c_str());
      return present_modes[0];
    }
  }

  /**
  * Destroy and free Vulkan resources used for the swapchain
  */
  void cleanup() {
    if (swap_chain != VK_NULL_HANDLE) {
      for (uint32_t i = 0; i < image_count; i++)
        vkDestroyImageView(device, buffers[i].view, nullptr);
    }

    if (surface != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(device, swap_chain, nullptr);
      vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    surface = VK_NULL_HANDLE;
    swap_chain = VK_NULL_HANDLE;
  }
};
}  // namespace vik
