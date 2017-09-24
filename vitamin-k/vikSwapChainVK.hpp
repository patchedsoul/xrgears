#pragma once

#include <vulkan/vulkan.h>

#include "vikSwapChain.hpp"

namespace vik {
class SwapChainVK : public SwapChain {
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
  VkResult acquire_next_image(VkDevice device, VkSemaphore semaphore, uint32_t *index) {
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

  void choose_surface_format(VkPhysicalDevice physical_device) {
    uint32_t num_formats = 0;

    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                         &num_formats, NULL);
    assert(num_formats > 0);

    VkSurfaceFormatKHR formats[num_formats];

    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                         &num_formats, formats);

    image_format = VK_FORMAT_UNDEFINED;
    for (int i = 0; i < num_formats; i++) {
      switch (formats[i].format) {
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
          /* These formats are all fine */
          image_format = formats[i].format;
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

    assert(image_format != VK_FORMAT_UNDEFINED);
  }

};
}
