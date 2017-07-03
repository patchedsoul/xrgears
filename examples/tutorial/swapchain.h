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
    VkDevice device;
    VkSwapchainKHR swapChain;
    VkFormat imageFormat;
    VkExtent2D extent;

    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    VikSwapChain(VkDevice d, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, GLFWwindow* window) {
	device = d;

	SwapChainSupportDetails swapChainSupport = querySupport(physicalDevice, surface);

	VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = choosePresentMode(swapChainSupport.presentModes);
	extent = chooseExtent(swapChainSupport.capabilities, window);

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
	images.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data());

	imageFormat = surfaceFormat.format;
    }

    ~VikSwapChain() {
	for (size_t i = 0; i < framebuffers.size(); i++) {
	    vkDestroyFramebuffer(device, framebuffers[i], nullptr);
	}

	for (size_t i = 0; i < imageViews.size(); i++) {
	    vkDestroyImageView(device, imageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    void createFramebuffers(VkRenderPass renderPass) {
	framebuffers.resize(imageViews.size());

	for (size_t i = 0; i < imageViews.size(); i++) {
	    VkImageView attachments[] = {
	        imageViews[i]
	    };

	    VkFramebufferCreateInfo framebufferInfo = {};
	    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	    framebufferInfo.renderPass = renderPass;
	    framebufferInfo.attachmentCount = 1;
	    framebufferInfo.pAttachments = attachments;
	    framebufferInfo.width = extent.width;
	    framebufferInfo.height = extent.height;
	    framebufferInfo.layers = 1;

	    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
		throw std::runtime_error("failed to create framebuffer!");
	    }
	}
    }

    void createImageViews() {
	imageViews.resize(images.size());

	for (size_t i = 0; i < images.size(); i++) {
	    VkImageViewCreateInfo createInfo = {};
	    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	    createInfo.image = images[i];

	    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	    createInfo.format = imageFormat;

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

	    if (vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image views!");
	    }

	}

    }

    static SwapChainSupportDetails querySupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
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

    static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
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

    static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
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

    static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
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
