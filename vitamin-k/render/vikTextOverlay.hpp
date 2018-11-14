/*
* Text overlay class for displaying debug information
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <vulkan/vulkan.h>

#include <vector>
#include <sstream>
#include <iomanip>
#include <string>
#include <functional>

#include "vikTools.hpp"
#include "vikDebug.hpp"
#include "vikBuffer.hpp"
#include "vikDevice.hpp"

#include "stb_font_consolas_24_latin1.inl"

// Defines for the STB font used
// STB font files can be found at http://nothings.org/stb/font/
#define STB_FONT_NAME stb_font_consolas_24_latin1
#define STB_FONT_WIDTH STB_FONT_consolas_24_latin1_BITMAP_WIDTH
#define STB_FONT_HEIGHT STB_FONT_consolas_24_latin1_BITMAP_HEIGHT
#define STB_FIRST_CHAR STB_FONT_consolas_24_latin1_FIRST_CHAR
#define STB_NUM_CHARS STB_FONT_consolas_24_latin1_NUM_CHARS

// Max. number of chars the text overlay buffer can hold
#define MAX_CHAR_COUNT 1024

/**
* @brief Mostly self-contained text overlay class
* @note Will only work with compatible render passes
*/ 

namespace vik {

class TextOverlay {
 private:
  Device *vulkanDevice;

  VkQueue queue;
  VkFormat colorFormat;
  VkFormat depthFormat;

  uint32_t *frameBufferWidth;
  uint32_t *frameBufferHeight;

  VkSampler sampler;
  VkImage image;
  VkImageView view;
  Buffer vertexBuffer;
  VkDeviceMemory imageMemory;
  VkDescriptorPool descriptorPool;
  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorSet descriptorSet;
  VkPipelineLayout pipelineLayout;
  VkPipelineCache pipelineCache;
  VkPipeline pipeline;
  VkRenderPass renderPass;
  VkCommandPool commandPool;
  std::vector<VkFramebuffer*> frameBuffers;
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
  VkFence fence;

  // Used during text updates
  glm::vec4 *mappedLocal = nullptr;

  stb_fontchar stbFontData[STB_NUM_CHARS];
  uint32_t numLetters;

 public:
  enum TextAlign { alignLeft, alignCenter, alignRight };

  bool visible = true;
  bool invalidated = false;

  float scale = 1.0f;

  std::function<void(vik::TextOverlay *overlay)>
    update_cb = [](vik::TextOverlay *overlay) {};

  void set_update_cb(std::function<void(vik::TextOverlay *overlay)> cb) {
    update_cb = cb;
  }

  std::vector<VkCommandBuffer> cmdBuffers;

  /**
  * Default constructor
  *
  * @param vulkanDevice Pointer to a valid VulkanDevice
  */
  TextOverlay(
      Device *vulkanDevice,
      VkQueue queue,
      std::vector<VkFramebuffer> *framebuffers,
      VkFormat colorformat,
      VkFormat depthformat,
      uint32_t *framebufferwidth,
      uint32_t *framebufferheight,
      std::vector<VkPipelineShaderStageCreateInfo> shaderstages) {
    this->vulkanDevice = vulkanDevice;
    this->queue = queue;
    this->colorFormat = colorformat;
    this->depthFormat = depthformat;

    this->frameBuffers.resize(framebuffers->size());
    for (uint32_t i = 0; i < framebuffers->size(); i++)
      this->frameBuffers[i] = &framebuffers->at(i);

    this->shaderStages = shaderstages;

    this->frameBufferWidth = framebufferwidth;
    this->frameBufferHeight = framebufferheight;

    cmdBuffers.resize(framebuffers->size());
    prepareResources();
    prepareRenderPass();
    preparePipeline();
  }

  /**
  * Default destructor, frees up all Vulkan resources acquired by the text overlay
  */
  ~TextOverlay() {
    // Free up all Vulkan resources requested by the text overlay
    vertexBuffer.destroy();
    vkDestroySampler(vulkanDevice->logicalDevice, sampler, nullptr);
    vkDestroyImage(vulkanDevice->logicalDevice, image, nullptr);
    vkDestroyImageView(vulkanDevice->logicalDevice, view, nullptr);
    vkFreeMemory(vulkanDevice->logicalDevice, imageMemory, nullptr);
    vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(vulkanDevice->logicalDevice, descriptorPool, nullptr);
    vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayout, nullptr);
    vkDestroyPipelineCache(vulkanDevice->logicalDevice, pipelineCache, nullptr);
    vkDestroyPipeline(vulkanDevice->logicalDevice, pipeline, nullptr);
    vkDestroyRenderPass(vulkanDevice->logicalDevice, renderPass, nullptr);
    vkFreeCommandBuffers(vulkanDevice->logicalDevice, commandPool, static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());
    vkDestroyCommandPool(vulkanDevice->logicalDevice, commandPool, nullptr);
    vkDestroyFence(vulkanDevice->logicalDevice, fence, nullptr);

    vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);
  }

  /**
  * Prepare all vulkan resources required to render the font
  * The text overlay uses separate resources for descriptors (pool, sets, layouts), pipelines and command buffers
  */
  void prepareResources() {
    static unsigned char font24pixels[STB_FONT_HEIGHT][STB_FONT_WIDTH];
    STB_FONT_NAME(stbFontData, font24pixels, STB_FONT_HEIGHT);

    // Command buffer

    // Pool
    VkCommandPoolCreateInfo cmdPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics
    };
    vik_log_check(vkCreateCommandPool(vulkanDevice->logicalDevice, &cmdPoolInfo,
                                      nullptr, &commandPool));

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = (uint32_t)cmdBuffers.size()
    };

    vik_log_check(vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &cmdBufAllocateInfo, cmdBuffers.data()));

    // Vertex buffer
    vik_log_check(vulkanDevice->createBuffer(
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &vertexBuffer,
                      MAX_CHAR_COUNT * sizeof(glm::vec4)));

    // Map persistent
    vertexBuffer.map();

    // Font texture
    VkImageCreateInfo imageInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8_UNORM,
      .extent = {
        .width = STB_FONT_WIDTH,
        .height = STB_FONT_HEIGHT,
        .depth = 1,
      },
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED
    };

    vik_log_check(vkCreateImage(vulkanDevice->logicalDevice, &imageInfo, nullptr, &image));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, image, &memReqs);

    VkMemoryAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      .memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    vik_log_check(vkAllocateMemory(vulkanDevice->logicalDevice, &allocInfo, nullptr, &imageMemory));
    vik_log_check(vkBindImageMemory(vulkanDevice->logicalDevice, image, imageMemory, 0));

    // Staging
    Buffer stagingBuffer;

    vik_log_check(vulkanDevice->createBuffer(
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &stagingBuffer,
                      allocInfo.allocationSize));

    stagingBuffer.map();
    // Only one channel, so data size = W * H (*R8)
    memcpy(stagingBuffer.mapped, &font24pixels[0][0], STB_FONT_WIDTH * STB_FONT_HEIGHT);
    stagingBuffer.unmap();

    // Copy to image
    VkCommandBuffer copyCmd;
    cmdBufAllocateInfo.commandBufferCount = 1;
    vik_log_check(vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &cmdBufAllocateInfo, &copyCmd));

    VkCommandBufferBeginInfo cmdBufInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };
    vik_log_check(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

    // Prepare for transfer
    tools::setImageLayout(
          copyCmd,
          image,
          VK_IMAGE_ASPECT_COLOR_BIT,
          VK_IMAGE_LAYOUT_PREINITIALIZED,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy bufferCopyRegion = {
      .imageSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .layerCount = 1,
      },
      .imageExtent = {
        .width = STB_FONT_WIDTH,
        .height = STB_FONT_HEIGHT,
        .depth = 1
      },
    };

    vkCmdCopyBufferToImage(
          copyCmd,
          stagingBuffer.buffer,
          image,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          1,
          &bufferCopyRegion);

    // Prepare for shader read
    tools::setImageLayout(
          copyCmd,
          image,
          VK_IMAGE_ASPECT_COLOR_BIT,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vik_log_check(vkEndCommandBuffer(copyCmd));

    VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &copyCmd
    };

    vik_log_check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    vik_log_check(vkQueueWaitIdle(queue));

    stagingBuffer.destroy();

    vkFreeCommandBuffers(vulkanDevice->logicalDevice, commandPool, 1, &copyCmd);

    VkImageViewCreateInfo imageViewInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = imageInfo.format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_R,
        .g = VK_COMPONENT_SWIZZLE_G,
        .b = VK_COMPONENT_SWIZZLE_B,
        .a = VK_COMPONENT_SWIZZLE_A
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };

    vik_log_check(vkCreateImageView(vulkanDevice->logicalDevice, &imageViewInfo, nullptr, &view));

    // Sampler
    VkSamplerCreateInfo samplerInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias = 0.0f,
      .maxAnisotropy = 1.0f,
      .compareOp = VK_COMPARE_OP_NEVER,
      .minLod = 0.0f,
      .maxLod = 1.0f,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    };

    vik_log_check(vkCreateSampler(vulkanDevice->logicalDevice, &samplerInfo, nullptr, &sampler));

    // Descriptor
    // Font uses a separate descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes = {{
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1
    }};

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data()
    };

    vik_log_check(vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    // Descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    }};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(setLayoutBindings.size()),
      .pBindings = setLayoutBindings.data()
    };
    vik_log_check(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptorSetLayout
    };
    vik_log_check(vkCreatePipelineLayout(vulkanDevice->logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    // Descriptor set
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &descriptorSetLayout
    };

    vik_log_check(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &descriptorSetAllocInfo, &descriptorSet));

    VkDescriptorImageInfo texDescriptor = {
      .sampler = sampler,
      .imageView = view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
      (VkWriteDescriptorSet) {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &texDescriptor
      }
    };
    vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // Pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vik_log_check(vkCreatePipelineCache(vulkanDevice->logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

    // Command buffer execution fence
    VkFenceCreateInfo fenceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
    };
    vik_log_check(vkCreateFence(vulkanDevice->logicalDevice, &fenceCreateInfo, nullptr, &fence));
  }

  /**
  * Prepare a separate pipeline for the font rendering decoupled from the main application
  */
  void preparePipeline() {
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineRasterizationStateCreateInfo rasterizationState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .lineWidth = 1.0f
    };

    // Enable blending
    VkPipelineColorBlendAttachmentState blendAttachmentState = {
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                        VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT |
                        VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &blendAttachmentState
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .front = {
        .compareOp = VK_COMPARE_OP_ALWAYS
      },
      .back = {
        .compareOp = VK_COMPARE_OP_ALWAYS
      }
    };

    VkPipelineViewportStateCreateInfo viewportState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1
    };

    VkPipelineMultisampleStateCreateInfo multisampleState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    std::vector<VkDynamicState> dynamicStateEnables = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
      .pDynamicStates = dynamicStateEnables.data()
    };

    std::vector<VkVertexInputBindingDescription> vertexBindings = {
      {
        .binding = 0,
        .stride = sizeof(glm::vec4),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
      },
      {
        .binding = 1,
        .stride = sizeof(glm::vec4),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
      }
    };

    std::vector<VkVertexInputAttributeDescription> vertexAttribs = {
      // Position
      {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = 0
      },
      // UV
      {
        .location = 1,
        .binding = 1,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = sizeof(glm::vec2)
      }
    };

    VkPipelineVertexInputStateCreateInfo inputState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindings.size()),
      .pVertexBindingDescriptions = vertexBindings.data(),
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttribs.size()),
      .pVertexAttributeDescriptions = vertexAttribs.data()
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = static_cast<uint32_t>(shaderStages.size()),
      .pStages = shaderStages.data(),
      .pVertexInputState = &inputState,
      .pInputAssemblyState = &inputAssemblyState,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizationState,
      .pMultisampleState = &multisampleState,
      .pDepthStencilState = &depthStencilState,
      .pColorBlendState = &colorBlendState,
      .pDynamicState = &dynamicState,
      .layout = pipelineLayout,
      .renderPass = renderPass,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1
    };
    vik_log_check(vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
  }

  /**
  * Prepare a separate render pass for rendering the text as an overlay
  */
  void prepareRenderPass() {
    VkAttachmentDescription attachments[2] = {
      {
        // Color attachment
        .format = colorFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        // Don't clear the framebuffer (like the renderpass from the example does)
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
      },
      {
        // Depth attachment
        .format = depthFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
      }
    };

    VkAttachmentReference colorReference = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depthReference = {
      depthReference.attachment = 1,
      depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDependency subpassDependencies[2] = {
      {
        // Transition from final to initial (VK_SUBPASS_EXTERNAL refers
        // to all commmands executed outside of the actual renderpass)
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
      },
      {
        // Transition from initial to final
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
      }
    };

    VkSubpassDescription subpassDescription = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorReference,
      .pDepthStencilAttachment = &depthReference
    };

    VkRenderPassCreateInfo renderPassInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 2,
      .pAttachments = attachments,
      .subpassCount = 1,
      .pSubpasses = &subpassDescription,
      .dependencyCount = 2,
      .pDependencies = subpassDependencies
    };
    vik_log_check(vkCreateRenderPass(vulkanDevice->logicalDevice,
                                     &renderPassInfo, nullptr, &renderPass));
  }

  /**
  * Maps the buffer, resets letter count
  */
  void beginTextUpdate() {
    mappedLocal = (glm::vec4*)vertexBuffer.mapped;
    numLetters = 0;
  }

  /**
  * Add text to the current buffer
  *
  * @param text Text to add
  * @param x x position of the text to add in window coordinate space
  * @param y y position of the text to add in window coordinate space
  * @param align Alignment for the new text (left, right, center)
  */
  void addText(std::string text, float x, float y, TextAlign align) {
    assert(vertexBuffer.mapped != nullptr);

    if (align == alignLeft) {
      x *= scale;
    }

    y *= scale;

    const float charW = (1.5f * scale) / *frameBufferWidth;
    const float charH = (1.5f * scale) / *frameBufferHeight;

    float fbW = (float)*frameBufferWidth;
    float fbH = (float)*frameBufferHeight;
    x = (x / fbW * 2.0f) - 1.0f;
    y = (y / fbH * 2.0f) - 1.0f;

    // Calculate text width
    float textWidth = 0;
    for (auto letter : text) {
      stb_fontchar *charData = &stbFontData[(uint32_t)letter - STB_FIRST_CHAR];
      textWidth += charData->advance * charW;
    }

    switch (align) {
      case alignRight:
        x -= textWidth;
        break;
      case alignCenter:
        x -= textWidth / 2.0f;
        break;
      case alignLeft:
        break;
    }

    // Generate a uv mapped quad per char in the new text
    for (auto letter : text) {
      stb_fontchar *charData = &stbFontData[(uint32_t)letter - STB_FIRST_CHAR];

      mappedLocal->x = (x + (float)charData->x0 * charW);
      mappedLocal->y = (y + (float)charData->y0 * charH);
      mappedLocal->z = charData->s0;
      mappedLocal->w = charData->t0;
      mappedLocal++;

      mappedLocal->x = (x + (float)charData->x1 * charW);
      mappedLocal->y = (y + (float)charData->y0 * charH);
      mappedLocal->z = charData->s1;
      mappedLocal->w = charData->t0;
      mappedLocal++;

      mappedLocal->x = (x + (float)charData->x0 * charW);
      mappedLocal->y = (y + (float)charData->y1 * charH);
      mappedLocal->z = charData->s0;
      mappedLocal->w = charData->t1;
      mappedLocal++;

      mappedLocal->x = (x + (float)charData->x1 * charW);
      mappedLocal->y = (y + (float)charData->y1 * charH);
      mappedLocal->z = charData->s1;
      mappedLocal->w = charData->t1;
      mappedLocal++;

      x += charData->advance * charW;

      numLetters++;
    }
  }

  /**
  * Unmap buffer and update command buffers
  */
  void endTextUpdate() {
    updateCommandBuffers();
  }

  /**
  * Update the command buffers to reflect text changes
  */
  void updateCommandBuffers() {
    VkCommandBufferBeginInfo cmdBufInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };

    VkRenderPassBeginInfo renderPassBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderPass,
      .renderArea = {
        .extent = {
          .width = *frameBufferWidth,
          .height = *frameBufferHeight
        }
      },
      // None of the attachments will be cleared
      .clearValueCount = 0,
      .pClearValues = nullptr
    };

    for (size_t i = 0; i < cmdBuffers.size(); ++i) {
      renderPassBeginInfo.framebuffer = *frameBuffers[i];

      vik_log_check(vkBeginCommandBuffer(cmdBuffers[i], &cmdBufInfo));

      if (debugmarker::active)
        debugmarker::beginRegion(cmdBuffers[i], "Text overlay", glm::vec4(1.0f, 0.94f, 0.3f, 1.0f));

      vkCmdBeginRenderPass(cmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = {
        .width = (float)*frameBufferWidth,
        .height = (float)*frameBufferHeight,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
      };
      vkCmdSetViewport(cmdBuffers[i], 0, 1, &viewport);

      VkRect2D scissor = {
        .offset = { .x = 0, .y = 0 },
        .extent = { .width = *frameBufferWidth, .height = *frameBufferHeight }
      };
      vkCmdSetScissor(cmdBuffers[i], 0, 1, &scissor);

      vkCmdBindPipeline(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
      vkCmdBindDescriptorSets(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

      VkDeviceSize offsets = 0;
      vkCmdBindVertexBuffers(cmdBuffers[i], 0, 1, &vertexBuffer.buffer, &offsets);
      vkCmdBindVertexBuffers(cmdBuffers[i], 1, 1, &vertexBuffer.buffer, &offsets);
      for (uint32_t j = 0; j < numLetters; j++)
        vkCmdDraw(cmdBuffers[i], 4, 1, j * 4, 0);

      vkCmdEndRenderPass(cmdBuffers[i]);

      if (debugmarker::active)
        debugmarker::endRegion(cmdBuffers[i]);

      vik_log_check(vkEndCommandBuffer(cmdBuffers[i]));
    }
  }

  /**
  * Submit the text command buffers to a queue
  */
  void submit(VkQueue queue, uint32_t bufferindex, VkSubmitInfo submitInfo) {
    if (!visible)
      return;

    submitInfo.pCommandBuffers = &cmdBuffers[bufferindex];
    submitInfo.commandBufferCount = 1;

    vik_log_check(vkQueueSubmit(queue, 1, &submitInfo, fence));

    vik_log_check(vkWaitForFences(vulkanDevice->logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX));
    vik_log_check(vkResetFences(vulkanDevice->logicalDevice, 1, &fence));
  }

  /**
  * Reallocate command buffers for the text overlay
  * @note Frees the existing command buffers
  */
  void reallocateCommandBuffers() {
    vkFreeCommandBuffers(vulkanDevice->logicalDevice, commandPool, static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = static_cast<uint32_t>(cmdBuffers.size())
    };

    vik_log_check(vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &cmdBufAllocateInfo, cmdBuffers.data()));
  }

  void update(const std::string& title, const std::string& fps, const std::string& device) {
    beginTextUpdate();
    addText(title, 5.0f, 5.0f, TextOverlay::alignLeft);
    addText(fps, 5.0f, 25.0f, TextOverlay::alignLeft);
    addText(device, 5.0f, 45.0f, TextOverlay::alignLeft);
    update_cb(this);
    endTextUpdate();
  }
};
}  // namespace vik
