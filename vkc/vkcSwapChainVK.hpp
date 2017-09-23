#pragma once

#include <vulkan/vulkan.h>

#include "vikSwapChainVK.hpp"
#include "../vks/vksLog.hpp"

namespace vkc {

class SwapChainVK : public vik::SwapChainVK {

public:
  uint32_t image_count = 0;

  SwapChainVK() {
  }

  ~SwapChainVK() {
  }

  void init(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface,
            VkFormat image_format, uint32_t width, uint32_t height, VkRenderPass render_pass) {
    VkSurfaceCapabilitiesKHR surface_caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                              &surface_caps);
    assert(surface_caps.supportedCompositeAlpha &
           VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);

    VkBool32 supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, 0, surface, &supported);
    assert(supported);

    uint32_t count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                              &count, NULL);
    VkPresentModeKHR present_modes[count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                              &count, present_modes);
    int i;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
    for (i = 0; i < count; i++) {
      if (present_modes[i] == VK_PRESENT_MODE_FIFO_KHR) {
        present_mode = VK_PRESENT_MODE_FIFO_KHR;
        break;
      }
    }

    uint32_t queueFamilyIndices[] { 0 };

    VkSwapchainCreateInfoKHR swapchainfo = {};
    swapchainfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainfo.surface = surface;
    swapchainfo.minImageCount = 2;
    swapchainfo.imageFormat = image_format;
    swapchainfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    swapchainfo.imageExtent = { width, height };
    swapchainfo.imageArrayLayers = 1;
    swapchainfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainfo.queueFamilyIndexCount = 1;
    swapchainfo.pQueueFamilyIndices = queueFamilyIndices;
    swapchainfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainfo.presentMode = present_mode;

    vkCreateSwapchainKHR(device, &swapchainfo, NULL, &swap_chain);

    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, NULL);
    assert(image_count > 0);
    vik_log_d("Creating swap chain with %d images.", image_count);
    VkImage swap_chain_images[image_count];
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images);

    for (uint32_t i = 0; i < image_count; i++) {
      buffers[i].image = swap_chain_images[i];
      create_image_view(device, buffers[i].image,
                        image_format, &buffers[i].view);
      create_frame_buffer(device, render_pass, &buffers[i].view,
                          width, height, &buffers[i].framebuffer);
    }
  }

  void present(VkQueue queue, uint32_t index) {
    VkSwapchainKHR swapChains[] = { swap_chain, };
    uint32_t indices[] = { index, };

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = indices;
    //presentInfo.pResults = &result,

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    vik_log_f_if(result != VK_SUCCESS, "vkQueuePresentKHR failed.");
  }

};
}
