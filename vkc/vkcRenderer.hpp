#pragma once

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#define MAX_NUM_IMAGES 4



namespace vkc {
struct RenderBuffer {
   VkImage image;
   VkImageView view;
   VkFramebuffer framebuffer;
};

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
    struct RenderBuffer buffers[MAX_NUM_IMAGES];
    uint32_t image_count;
    int current;

    VkCommandBuffer cmd_buffer;
    bool cmd_buffer_created = false;

    uint32_t vertex_offset, colors_offset, normals_offset;

    Renderer(uint32_t w, uint32_t h) ;
    ~Renderer() ;

    void init_vk(const char *extension);
    VkFormat choose_surface_format();
    void init_render_pass();
    void init_vk_objects();
    void init_buffer(struct RenderBuffer *b);
    void create_swapchain();

    void present(uint32_t index);
    VkResult aquire_next_image(uint32_t *index);

    void create_swapchain_if_needed();
    void submit_queue();

    void wait_and_reset_fences();

    uint64_t get_animation_time();

    void build_command_buffer(RenderBuffer *b);

    void render(RenderBuffer *b);

    void render_swapchain();


};
}
