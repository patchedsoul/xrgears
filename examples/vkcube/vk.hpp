#pragma once

#include "common.h"

void
init_vk(struct vkcube *vc, const char *extension)
{
    VkApplicationInfo appinfo = {};
    appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appinfo.pApplicationName = "vkcube";
    appinfo.apiVersion = VK_MAKE_VERSION(1, 0, 2);

    const char * names[2] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        extension,
    };

    VkInstanceCreateInfo instanceinfo = {};
    instanceinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceinfo.pApplicationInfo = &appinfo;
    instanceinfo.enabledExtensionCount = extension ? (uint32_t) 2 : 0;
    instanceinfo.ppEnabledExtensionNames = names;

    vkCreateInstance(&instanceinfo,
                     NULL,
                     &vc->instance);

    uint32_t count;
    vkEnumeratePhysicalDevices(vc->instance, &count, NULL);
    VkPhysicalDevice pd[count];
    vkEnumeratePhysicalDevices(vc->instance, &count, pd);
    vc->physical_device = pd[0];
    printf("%d physical devices\n", count);

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(vc->physical_device, &properties);
    printf("vendor id %04x, device name %s\n",
           properties.vendorID, properties.deviceName);

    vkGetPhysicalDeviceQueueFamilyProperties(vc->physical_device, &count, NULL);
    assert(count > 0);
    VkQueueFamilyProperties props[count];
    vkGetPhysicalDeviceQueueFamilyProperties(vc->physical_device, &count, props);
    assert(props[0].queueFlags & VK_QUEUE_GRAPHICS_BIT);

    float prois[] = { 1.0f };

    VkDeviceQueueCreateInfo queueinfo = {};
    queueinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueinfo.queueFamilyIndex = 0;
    queueinfo.queueCount = 1;
    queueinfo.pQueuePriorities = prois;

    const char * const extnames[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueinfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = extnames;

    vkCreateDevice(vc->physical_device,
                   &deviceInfo,
                   NULL,
                   &vc->device);

    vkGetDeviceQueue(vc->device, 0, 0, &vc->queue);
}

VkFormat
choose_surface_format(struct vkcube *vc)
{
    uint32_t num_formats = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vc->physical_device, vc->surface,
                                         &num_formats, NULL);
    assert(num_formats > 0);

    VkSurfaceFormatKHR formats[num_formats];

    vkGetPhysicalDeviceSurfaceFormatsKHR(vc->physical_device, vc->surface,
                                         &num_formats, formats);

    VkFormat format = VK_FORMAT_UNDEFINED;
    for (int i = 0; i < num_formats; i++) {
	switch (formats[i].format) {
	    case VK_FORMAT_R8G8B8A8_SRGB:
	    case VK_FORMAT_B8G8R8A8_SRGB:
		/* These formats are all fine */
		format = formats[i].format;
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

    assert(format != VK_FORMAT_UNDEFINED);

    return format;
}

void
init_vk_objects(struct vkcube *vc)
{

    VkAttachmentDescription attachementDesc[] = {
        {
            .format = vc->image_format,
            .samples = (VkSampleCountFlagBits) 1,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        }
    };

    VkAttachmentReference atref[] = {
        {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        }
    };

    VkAttachmentReference atref2[] = {
        {
            .attachment = VK_ATTACHMENT_UNUSED,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        }
    };

    uint32_t presereve[] = { 0 };

    VkSubpassDescription passdesc[] = {
        {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments = atref,
            .pResolveAttachments = atref2,
            .pDepthStencilAttachment = NULL,
            .preserveAttachmentCount = 1,
            .pPreserveAttachments = presereve,
        }
    };

    VkRenderPassCreateInfo passcreateinfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = attachementDesc,
        .subpassCount = 1,
        .pSubpasses = passdesc,
        .dependencyCount = 0
    };

    vkCreateRenderPass(vc->device,
                       &passcreateinfo,
                       NULL,
                       &vc->render_pass);

    vc->model.init(vc);

    VkFenceCreateInfo fenceinfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = 0
    };

    vkCreateFence(vc->device,
                  &fenceinfo,
                  NULL,
                  &vc->fence);


    const VkCommandPoolCreateInfo commandpoolinfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = 0,
        .flags = 0
    };

    vkCreateCommandPool(vc->device,
                        &commandpoolinfo,
                        NULL,
                        &vc->cmd_pool);

    VkSemaphoreCreateInfo semaphoreinfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    vkCreateSemaphore(vc->device,
                      &semaphoreinfo,
                      NULL,
                      &vc->semaphore);
}


void
init_buffer(struct vkcube *vc, struct vkcube_buffer *b)
{

    VkImageViewCreateInfo imageviewinfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = b->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = vc->image_format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
            .a = VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    vkCreateImageView(vc->device,
                      &imageviewinfo,
                      NULL,
                      &b->view);


    VkFramebufferCreateInfo framebufferinfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = vc->render_pass,
        .attachmentCount = 1,
        .pAttachments = &b->view,
        .width = vc->width,
        .height = vc->height,
        .layers = 1
    };

    vkCreateFramebuffer(vc->device,
                        &framebufferinfo,
                        NULL,
                        &b->framebuffer);
}


static void
create_swapchain(struct vkcube *vc)
{
    VkSurfaceCapabilitiesKHR surface_caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vc->physical_device, vc->surface,
                                              &surface_caps);
    assert(surface_caps.supportedCompositeAlpha &
           VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);

    VkBool32 supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(vc->physical_device, 0, vc->surface,
                                         &supported);
    assert(supported);

    uint32_t count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vc->physical_device, vc->surface,
                                              &count, NULL);
    VkPresentModeKHR present_modes[count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(vc->physical_device, vc->surface,
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
        .surface = vc->surface,
        .minImageCount = 2,
        .imageFormat = vc->image_format,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = { vc->width, vc->height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = queueFamilyIndices,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
    };

    vkCreateSwapchainKHR(vc->device,
                         &swapchainfo, NULL, &vc->swap_chain);

    vkGetSwapchainImagesKHR(vc->device, vc->swap_chain,
                            &vc->image_count, NULL);
    assert(vc->image_count > 0);
    VkImage swap_chain_images[vc->image_count];
    vkGetSwapchainImagesKHR(vc->device, vc->swap_chain,
                            &vc->image_count, swap_chain_images);

    for (uint32_t i = 0; i < vc->image_count; i++) {
	vc->buffers[i].image = swap_chain_images[i];
	init_buffer(vc, &vc->buffers[i]);
    }
}
