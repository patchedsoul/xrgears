#pragma once

#include <vulkan/vulkan.h>

#include "vikSwapChain.hpp"

namespace vik {
class SwapChainVK : public SwapChain {

protected:
 VkInstance instance;
 VkDevice device;
 VkPhysicalDevice physical_device;

public:

  /** @brief Handle to the current swap chain, required for recreation */
  VkSwapchainKHR swap_chain = VK_NULL_HANDLE;

  VkSurfaceKHR surface;

  SwapChainVK() {}
  ~SwapChainVK() {}

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

  void choose_surface_format() {
    uint32_t num_formats = 0;

    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                         &num_formats, NULL);
    assert(num_formats > 0);

    VkSurfaceFormatKHR formats[num_formats];

    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                         &num_formats, formats);

    surface_format.format = VK_FORMAT_UNDEFINED;
    for (int i = 0; i < num_formats; i++) {
      switch (formats[i].format) {
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
          /* These formats are all fine */
          surface_format.format = formats[i].format;
          break;
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
        case VK_FORMAT_B5G6R5_UNORM_PACK16:
          /* We would like to support these but they don't seem to work. */
        default:
          continue;
      }
    }

    assert(surface_format.format != VK_FORMAT_UNDEFINED);
  }

  /**
  * Set instance, physical and logical device to use for the swapchain and get all required function pointers
  *
  * @param instance Vulkan instance to use
  * @param physicalDevice Physical device used to query properties and formats relevant to the swapchain
  * @param device Logical representation of the device to create the swapchain for
  *
  */
  void set_context(VkInstance i, VkPhysicalDevice p, VkDevice d) {
    instance = i;
    physical_device = p;
    device = d;
  }

  void update_images() {
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, NULL);
    assert(image_count > 0);
    vik_log_d("Creating swap chain with %d images.", image_count);
    VkImage images[image_count];
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, images);

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

    VkPresentModeKHR present_modes[count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                              &count, present_modes);

    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;

    for (size_t i = 0; i < count; i++) {
      if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        return VK_PRESENT_MODE_MAILBOX_KHR;
      else if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    return mode;
  }

};
}
