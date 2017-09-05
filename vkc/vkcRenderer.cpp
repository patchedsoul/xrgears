#include <assert.h>
#include <sys/time.h>

#include "vkcRenderer.hpp"
#include "vksApplication.hpp"

namespace vkc {

Renderer::Renderer(uint32_t w, uint32_t h) : width(w), height(h) {
  gettimeofday(&start_tv, NULL);
}

Renderer::~Renderer() {
}

void Renderer::init_vk(const char *extension) {
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
                   &instance);

  uint32_t count;
  vkEnumeratePhysicalDevices(instance, &count, NULL);
  VkPhysicalDevice pd[count];
  vkEnumeratePhysicalDevices(instance, &count, pd);
  physical_device = pd[0];
  printf("%d physical devices\n", count);

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  printf("vendor id %04x, device name %s\n",
         properties.vendorID, properties.deviceName);

  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, NULL);
  assert(count > 0);
  VkQueueFamilyProperties props[count];
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, props);
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

  vkCreateDevice(physical_device,
                 &deviceInfo,
                 NULL,
                 &device);

  vkGetDeviceQueue(device, 0, 0, &queue);
}

VkFormat Renderer::choose_surface_format() {
  uint32_t num_formats = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                       &num_formats, NULL);
  assert(num_formats > 0);

  VkSurfaceFormatKHR formats[num_formats];

  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
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

void Renderer::init_render_pass() {
  VkAttachmentDescription attachement_desc[] = {
    {
      .format = image_format,
      .samples = (VkSampleCountFlagBits) 1,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    }
  };

  VkAttachmentReference color_attachments[] = {
    {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    }
  };

  VkAttachmentReference resolve_attachments[] = {
    {
      .attachment = VK_ATTACHMENT_UNUSED,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    }
  };

  uint32_t presereve_attachments[] = { 0 };

  VkSubpassDescription sub_pass_desc[] = {
    {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .colorAttachmentCount = 1,
      .pColorAttachments = color_attachments,
      .pResolveAttachments = resolve_attachments,
      .pDepthStencilAttachment = NULL,
      .preserveAttachmentCount = 1,
      .pPreserveAttachments = presereve_attachments,
    }
  };

  VkRenderPassCreateInfo render_pass_create_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = attachement_desc,
    .subpassCount = 1,
    .pSubpasses = sub_pass_desc,
    .dependencyCount = 0
  };

  vkCreateRenderPass(device,
                     &render_pass_create_info,
                     NULL,
                     &render_pass);
}

void Renderer::init_vk_objects() {
  VkFenceCreateInfo fenceinfo = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = 0
  };

  vkCreateFence(device,
                &fenceinfo,
                NULL,
                &fence);


  const VkCommandPoolCreateInfo commandpoolinfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = 0,
    .flags = 0
  };

  vkCreateCommandPool(device,
                      &commandpoolinfo,
                      NULL,
                      &cmd_pool);

  VkSemaphoreCreateInfo semaphoreinfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  vkCreateSemaphore(device,
                    &semaphoreinfo,
                    NULL,
                    &semaphore);
}

void Renderer::init_buffer(struct CubeBuffer *b) {

  VkImageViewCreateInfo imageviewinfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = b->image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = image_format,
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

  vkCreateImageView(device,
                    &imageviewinfo,
                    NULL,
                    &b->view);


  VkFramebufferCreateInfo framebufferinfo = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass = render_pass,
    .attachmentCount = 1,
    .pAttachments = &b->view,
    .width = width,
    .height = height,
    .layers = 1
  };

  vkCreateFramebuffer(device,
                      &framebufferinfo,
                      NULL,
                      &b->framebuffer);
}


void Renderer::create_swapchain() {
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
    init_buffer(&buffers[i]);
  }
}

}
