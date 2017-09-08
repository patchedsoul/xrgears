#pragma once

#include <vulkan/vulkan.h>

#include "vkcSwapChain.hpp"


namespace vkc {

class SwapChainVK : public SwapChain {

public:
    VkSwapchainKHR swap_chain;
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

	VkSwapchainCreateInfoKHR swapchainfo = {
	    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
	    .surface = surface,
	    .minImageCount = 2,
	    .imageFormat = image_format,
	    .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
	    .imageExtent = { width, height },
	    .imageArrayLayers = 1,
	    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
	    .queueFamilyIndexCount = 1,
	    .pQueueFamilyIndices = queueFamilyIndices,
	    .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
	    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
	    .presentMode = present_mode,
	};

	vkCreateSwapchainKHR(device, &swapchainfo, NULL, &swap_chain);

	vkGetSwapchainImagesKHR(device, swap_chain, &image_count, NULL);
	assert(image_count > 0);
	VkImage swap_chain_images[image_count];
	vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images);

	for (uint32_t i = 0; i < image_count; i++) {
	    buffers[i].image = swap_chain_images[i];
	    init_buffer(device, image_format, render_pass,
	                width, height, &buffers[i]);
	}
    }
};
}
