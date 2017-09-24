#pragma once

#include <vulkan/vulkan.h>

#include "vikSwapChainVK.hpp"
#include "../vks/vksLog.hpp"

namespace vkc {

class SwapChainVK : public vik::SwapChainVK {

public:
  SwapChainVK() {
  }

  ~SwapChainVK() {
  }

  void create(uint32_t width, uint32_t height) {
    VkSurfaceCapabilitiesKHR surface_caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                              &surface_caps);
    assert(surface_caps.supportedCompositeAlpha &
           VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);

    VkBool32 supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, 0, surface, &supported);
    assert(supported);

    VkSwapchainCreateInfoKHR swapchainfo = {};
    swapchainfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainfo.surface = surface;
    swapchainfo.minImageCount = 2;
    swapchainfo.imageFormat = surface_format.format;
    swapchainfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    swapchainfo.imageExtent = { width, height };
    swapchainfo.imageArrayLayers = 1;
    swapchainfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainfo.queueFamilyIndexCount = 1;
    swapchainfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainfo.presentMode = select_present_mode();

    vkCreateSwapchainKHR(device, &swapchainfo, NULL, &swap_chain);
  }

  void destroy() {
    if (image_count > 0) {
      vkDestroySwapchainKHR(device, swap_chain, NULL);
      image_count = 0;
    }
  }



};
}
