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

#include "vikSwapChain.hpp"

namespace vik {
class SwapChainVK : public SwapChain {
 public:
  /** @brief Handle to the current swap chain, required for recreation */
  VkSwapchainKHR swap_chain = VK_NULL_HANDLE;

  VkSurfaceKHR surface;

  SwapChainVK() {}
  ~SwapChainVK() {}

  void create(uint32_t width, uint32_t height) {
    VkSurfaceCapabilitiesKHR surface_caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                              &surface_caps);
    assert(surface_caps.supportedCompositeAlpha &
           VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);

    VkBool32 supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, 0, surface, &supported);
    assert(supported);

    VkSwapchainCreateInfoKHR swap_chain_info = {};
    swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_info.surface = surface;
    swap_chain_info.minImageCount = 2;
    swap_chain_info.imageFormat = surface_format.format;
    swap_chain_info.imageColorSpace = surface_format.colorSpace;
    swap_chain_info.imageExtent = { width, height };
    swap_chain_info.imageArrayLayers = 1;
    swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swap_chain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_chain_info.queueFamilyIndexCount = 1;
    swap_chain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swap_chain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_info.presentMode = select_present_mode();

    vkCreateSwapchainKHR(device, &swap_chain_info, NULL, &swap_chain);

    update_images();
  }

  void recreate(uint32_t width, uint32_t height) {
    destroy();
    create(width, height);
    update_images();
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

  void print_available_formats(const std::vector<VkSurfaceFormatKHR>& formats) {
    vik_log_d("Available formats:");
    for (VkSurfaceFormatKHR format : formats)
        vik_log_d("(%s, %s)",
                  Log::color_format_string(format.format).c_str(),
                  Log::color_space_string(format.colorSpace).c_str());
  }

  virtual void select_surface_format() {
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, NULL);
    assert(count > 0);

    std::vector<VkSurfaceFormatKHR> formats(count);
    vik_log_check(vkGetPhysicalDeviceSurfaceFormatsKHR(
                    physical_device, surface, &count, formats.data()));

    print_available_formats(formats);

    for (uint32_t i = 0; i < count; i++) {
      switch (formats[i].format) {
        case VK_FORMAT_B8G8R8A8_UNORM:
          surface_format = formats[i];
          vik_log_d("Using color format %s, %s",
                    Log::color_format_string(surface_format.format).c_str(),
                    Log::color_space_string(surface_format.colorSpace).c_str());
          break;
        default:
          continue;
      }
    }

    assert(surface_format.format != VK_FORMAT_UNDEFINED);
  }

  void update_images() {
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, NULL);
    assert(image_count > 0);
    vik_log_d("Creating swap chain with %d images.", image_count);

    std::vector<VkImage> images(image_count);
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, images.data());

    buffers.resize(image_count);

    for (uint32_t i = 0; i < image_count; i++) {
      buffers[i].image = images[i];
      create_image_view(device, buffers[i].image,
                        surface_format.format, &buffers[i].view);
    }
  }

  VkPresentModeKHR select_present_mode() {
    // Select a present mode for the swapchain

    // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
    // This mode waits for the vertical blank ("v-sync")


    // If v-sync is not requested, try to find a mailbox mode
    // It's the lowest latency non-tearing present mode available

    // Get available present modes
    uint32_t count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                              &count, NULL);
    assert(count > 0);

    std::vector<VkPresentModeKHR> present_modes(count);

    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                              &count, present_modes.data());

    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;

    for (size_t i = 0; i < count; i++) {
      if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        return VK_PRESENT_MODE_MAILBOX_KHR;
      else if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    return mode;
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
