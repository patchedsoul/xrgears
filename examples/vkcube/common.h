#pragma once

#ifndef VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#endif

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include <gbm.h>

#define MAX_NUM_IMAGES 4

struct vkcube_buffer {
   struct gbm_bo *gbm_bo;
   VkDeviceMemory mem;
   VkImage image;
   VkImageView view;
   VkFramebuffer framebuffer;
   uint32_t fb;
   uint32_t stride;
};

struct vkcube;

enum display_mode {
   DISPLAY_MODE_AUTO = 0,
   DISPLAY_MODE_KMS,
   DISPLAY_MODE_XCB,
};

struct model {
   void (*init)(struct vkcube *vc);
   void (*render)(struct vkcube *vc, struct vkcube_buffer *b);
};

struct vkcube {
   struct model model;

   VkSwapchainKHR swap_chain;

   uint32_t width, height;

   VkInstance instance;
   VkPhysicalDevice physical_device;
   VkDevice device;
   VkRenderPass render_pass;
   VkQueue queue;
   VkPipelineLayout pipeline_layout;
   VkPipeline pipeline;
   VkDeviceMemory mem;
   VkBuffer buffer;
   VkDescriptorSet descriptor_set;
   VkSemaphore semaphore;
   VkFence fence;
   VkCommandPool cmd_pool;

   void *map;
   uint32_t vertex_offset, colors_offset, normals_offset;

   struct timeval start_tv;
   VkSurfaceKHR surface;
   VkFormat image_format;
   struct vkcube_buffer buffers[MAX_NUM_IMAGES];
   uint32_t image_count;
   int current;
};


