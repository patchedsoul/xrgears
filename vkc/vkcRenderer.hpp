#pragma once

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#define MAX_NUM_IMAGES 4



namespace vkc {
struct CubeBuffer {
   VkImage image;
   VkImageView view;
   VkFramebuffer framebuffer;
};

class Cube;

class Renderer {
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
    struct CubeBuffer buffers[MAX_NUM_IMAGES];
    uint32_t image_count;
    int current;

    Renderer(uint32_t w, uint32_t h) ;
    ~Renderer() ;

    void init_vk(const char *extension);
    VkFormat choose_surface_format();
    void init_vk_objects(Cube * model);
    void init_buffer(struct CubeBuffer *b);
    void create_swapchain();
};
}
