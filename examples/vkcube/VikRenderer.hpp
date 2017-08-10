#pragma once

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

class Cube;

class VikRenderer {
public:
    VkSwapchainKHR swap_chain;
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

    uint32_t width, height;


    struct timeval start_tv;
    VkSurfaceKHR surface;
    VkFormat image_format;
    struct vkcube_buffer buffers[MAX_NUM_IMAGES];
    uint32_t image_count;
    int current;

    VikRenderer() ;

    ~VikRenderer() ;

    void
    init_vk(const char *extension);

    VkFormat
    choose_surface_format();

    void
    init_vk_objects(Cube * model);


    void
    init_buffer(struct vkcube_buffer *b);


    void
    create_swapchain();
};
