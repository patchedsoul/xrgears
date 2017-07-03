#pragma once

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "queue.h"

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VikSwapChain {

public:

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;

    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    std::vector<VkImageView> swapChainImageViews;

    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkDevice device;

    VikSwapChain(VkDevice d) {
	device = d;
    }

    ~VikSwapChain() {
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
	    vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
	}

	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
	    vkDestroyImageView(device, swapChainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    void createSwapChain(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, GLFWwindow* window) {
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
	    imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	//printf("Creating swapchain with %d images.\n", imageCount);

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);
	uint32_t queueFamilyIndices[] = {(uint32_t) indices.graphicsFamily, (uint32_t) indices.presentFamily};

	if (indices.graphicsFamily != indices.presentFamily) {
	    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	    createInfo.queueFamilyIndexCount = 2;
	    createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
	    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	    createInfo.queueFamilyIndexCount = 0; // Optional
	    createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
	    throw std::runtime_error("failed to create swap chain!");
	}


	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

    }


    void createFramebuffers(VkRenderPass renderPass) {
	swapChainFramebuffers.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
	    VkImageView attachments[] = {
	        swapChainImageViews[i]
	    };

	    VkFramebufferCreateInfo framebufferInfo = {};
	    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	    framebufferInfo.renderPass = renderPass;
	    framebufferInfo.attachmentCount = 1;
	    framebufferInfo.pAttachments = attachments;
	    framebufferInfo.width = swapChainExtent.width;
	    framebufferInfo.height = swapChainExtent.height;
	    framebufferInfo.layers = 1;

	    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
		throw std::runtime_error("failed to create framebuffer!");
	    }
	}
    }

    void createImageViews() {
	swapChainImageViews.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); i++) {
	    VkImageViewCreateInfo createInfo = {};
	    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	    createInfo.image = swapChainImages[i];

	    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	    createInfo.format = swapChainImageFormat;

	    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	    /*
	     * If you were working on a stereographic 3D application,
	     * then you would create a swap chain with multiple layers.
	     * You could then create multiple image views for each image
	     * representing the views for the left and right eyes
	     * by accessing different layers.
	     */

	    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	    createInfo.subresourceRange.baseMipLevel = 0;
	    createInfo.subresourceRange.levelCount = 1;
	    createInfo.subresourceRange.baseArrayLayer = 0;
	    createInfo.subresourceRange.layerCount = 1;

	    if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image views!");
	    }

	}

    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
	    details.formats.resize(formatCount);
	    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
	    details.presentModes.resize(presentModeCount);
	    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
	    return capabilities.currentExtent;
	} else {
	    int w, h;
	    glfwGetWindowSize(window, &w, &h);

	    uint32_t width, height;

	    width = w;
	    height = h;

	    VkExtent2D actualExtent = {width, height};

	    actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
	    actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

	    return actualExtent;
	}
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& availablePresentMode : availablePresentModes) {
	    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
		return availablePresentMode;
	    } else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
		bestMode = availablePresentMode;
	    }
	}

	return bestMode;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
	    return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	}

	for (const auto& availableFormat : availableFormats) {
	    if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
		return availableFormat;
	    }
	}

	return availableFormats[0];
    }

};
