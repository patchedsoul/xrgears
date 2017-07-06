/*
* Vulkan Example - Deferred shading with multiple render targets (aka G-Buffer) example
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"
#include "VulkanBuffer.hpp"
#include "VulkanTexture.hpp"
#include "VulkanModel.hpp"

#include "VikDistortion.hpp"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION true

// Texture properties
#define TEX_DIM 2048
#define TEX_FILTER VK_FILTER_LINEAR

// Offscreen frame buffer properties
#define FB_DIM TEX_DIM

class VulkanExample : public VulkanExampleBase
{
public:

  struct {
    struct {
      vks::Texture2D colorMap;
    } model;
  } textures;

  // Vertex layout for the models
  vks::VertexLayout vertexLayout = vks::VertexLayout({
    vks::VERTEX_COMPONENT_POSITION,
    vks::VERTEX_COMPONENT_UV,
    vks::VERTEX_COMPONENT_COLOR,
    vks::VERTEX_COMPONENT_NORMAL,
    vks::VERTEX_COMPONENT_TANGENT,
  });

  VikDistortion *hmdDistortion;

  struct {
    vks::Model model;
  } models;

  struct {
    VkPipelineVertexInputStateCreateInfo inputState;
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
  } vertices;

  struct {
    glm::mat4 projection;
    glm::mat4 model;
    glm::mat4 view;
    glm::vec4 instancePos[3];
  } uboOffscreenVS;

  struct {
    vks::Buffer vsOffscreen;
  } uniformBuffers;

  struct {
    VkPipeline offscreen;
  } pipelines;

  struct {
    VkPipelineLayout offscreen;
  } pipelineLayouts;

  struct {
    VkDescriptorSet model;
  } descriptorSets;

  VkDescriptorSet descriptorSet;
  VkDescriptorSetLayout descriptorSetLayout;

  // Framebuffer for offscreen rendering
  struct FrameBufferAttachment {
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
    VkFormat format;
  };
  struct FrameBuffer {
    int32_t width, height;
    VkFramebuffer frameBuffer;
    FrameBufferAttachment position;
    FrameBufferAttachment depth;
    VkRenderPass renderPass;
  } offScreenFrameBuf;

  // One sampler for the frame buffer color attachments
  VkSampler colorSampler;

  VkCommandBuffer offScreenCmdBuffer = VK_NULL_HANDLE;

  // Semaphore used to synchronize between offscreen and final scene rendering
  VkSemaphore offscreenSemaphore = VK_NULL_HANDLE;

  VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
  {
    title = "Vulkan Example - Deferred shading (2016 by Sascha Willems)";
    enableTextOverlay = true;
    camera.type = Camera::CameraType::firstperson;
    camera.movementSpeed = 5.0f;
#ifndef __ANDROID__
    camera.rotationSpeed = 0.25f;
#endif
    camera.position = { 2.15f, 0.3f, -8.75f };
    camera.setRotation(glm::vec3(-0.75f, 12.5f, 0.0f));
    camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
  }

  ~VulkanExample()
  {
    // Clean up used Vulkan resources
    // Note : Inherited destructor cleans up resources stored in base class

    vkDestroySampler(device, colorSampler, nullptr);

    // Frame buffer

    // Color attachments
    vkDestroyImageView(device, offScreenFrameBuf.position.view, nullptr);
    vkDestroyImage(device, offScreenFrameBuf.position.image, nullptr);
    vkFreeMemory(device, offScreenFrameBuf.position.mem, nullptr);

    // Depth attachment
    vkDestroyImageView(device, offScreenFrameBuf.depth.view, nullptr);
    vkDestroyImage(device, offScreenFrameBuf.depth.image, nullptr);
    vkFreeMemory(device, offScreenFrameBuf.depth.mem, nullptr);

    vkDestroyFramebuffer(device, offScreenFrameBuf.frameBuffer, nullptr);

    vkDestroyPipeline(device, pipelines.offscreen, nullptr);

    vkDestroyPipelineLayout(device, pipelineLayouts.offscreen, nullptr);

    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    // Meshes
    models.model.destroy();

    delete hmdDistortion;

    // Uniform buffers
    uniformBuffers.vsOffscreen.destroy();

    vkFreeCommandBuffers(device, cmdPool, 1, &offScreenCmdBuffer);

    vkDestroyRenderPass(device, offScreenFrameBuf.renderPass, nullptr);

    textures.model.colorMap.destroy();

    vkDestroySemaphore(device, offscreenSemaphore, nullptr);
  }

  // Create a frame buffer attachment
  void createAttachment(
    VkFormat format,
    VkImageUsageFlagBits usage,
    FrameBufferAttachment *attachment)
  {
    VkImageAspectFlags aspectMask = 0;
    VkImageLayout imageLayout;

    attachment->format = format;

    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
    {
      aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    assert(aspectMask > 0);

    VkImageCreateInfo image = vks::initializers::imageCreateInfo();
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = format;
    image.extent.width = offScreenFrameBuf.width;
    image.extent.height = offScreenFrameBuf.height;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

    VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &attachment->image));
    vkGetImageMemoryRequirements(device, attachment->image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &attachment->mem));
    VK_CHECK_RESULT(vkBindImageMemory(device, attachment->image, attachment->mem, 0));

    VkImageViewCreateInfo imageView = vks::initializers::imageViewCreateInfo();
    imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageView.format = format;
    imageView.subresourceRange = {};
    imageView.subresourceRange.aspectMask = aspectMask;
    imageView.subresourceRange.baseMipLevel = 0;
    imageView.subresourceRange.levelCount = 1;
    imageView.subresourceRange.baseArrayLayer = 0;
    imageView.subresourceRange.layerCount = 1;
    imageView.image = attachment->image;
    VK_CHECK_RESULT(vkCreateImageView(device, &imageView, nullptr, &attachment->view));
  }

  // Prepare a new framebuffer and attachments for offscreen rendering (G-Buffer)
  void prepareOffscreenFramebuffer()
  {
    offScreenFrameBuf.width = FB_DIM;
    offScreenFrameBuf.height = FB_DIM;

    // Color attachments

    // (World space) Positions
    createAttachment(
      VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      &offScreenFrameBuf.position);

    // Depth attachment

    // Find a suitable depth format
    VkFormat attDepthFormat;
    VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &attDepthFormat);
    assert(validDepthFormat);

    createAttachment(
      attDepthFormat,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      &offScreenFrameBuf.depth);

    // Set up separate renderpass with references to the color and depth attachments
    std::array<VkAttachmentDescription, 2> attachmentDescs = {};

    // Init attachment properties
    for (uint32_t i = 0; i < 2; ++i)
    {
      attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
      attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      if (i == 1)
      {
        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      }
      else
      {
        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }
    }

    // Formats
    attachmentDescs[0].format = offScreenFrameBuf.position.format;
    attachmentDescs[1].format = offScreenFrameBuf.depth.format;

    std::vector<VkAttachmentReference> colorReferences;
    colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pColorAttachments = colorReferences.data();
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
    subpass.pDepthStencilAttachment = &depthReference;

    // Use subpass dependencies for attachment layput transitions
    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments = attachmentDescs.data();
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &offScreenFrameBuf.renderPass));

    std::array<VkImageView,2> attachments;
    attachments[0] = offScreenFrameBuf.position.view;
    attachments[1] = offScreenFrameBuf.depth.view;

    VkFramebufferCreateInfo fbufCreateInfo = {};
    fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.pNext = NULL;
    fbufCreateInfo.renderPass = offScreenFrameBuf.renderPass;
    fbufCreateInfo.pAttachments = attachments.data();
    fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbufCreateInfo.width = offScreenFrameBuf.width;
    fbufCreateInfo.height = offScreenFrameBuf.height;
    fbufCreateInfo.layers = 1;
    VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offScreenFrameBuf.frameBuffer));

    // Create sampler to sample from the color attachments
    VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
    sampler.magFilter = VK_FILTER_NEAREST;
    sampler.minFilter = VK_FILTER_NEAREST;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV = sampler.addressModeU;
    sampler.addressModeW = sampler.addressModeU;
    sampler.mipLodBias = 0.0f;
    sampler.maxAnisotropy = 0;
    sampler.minLod = 0.0f;
    sampler.maxLod = 1.0f;
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &colorSampler));
  }

  // Build command buffer for rendering the scene to the offscreen frame buffer attachments
  void buildOffscreenCommandBuffer()
  {
    if (offScreenCmdBuffer == VK_NULL_HANDLE)
    {
      offScreenCmdBuffer = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    }

    // Create a semaphore used to synchronize offscreen rendering and usage
    VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &offscreenSemaphore));

    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

    // Clear values for all attachments written in the fragment sahder
    std::array<VkClearValue,2> clearValues;
    clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass =  offScreenFrameBuf.renderPass;
    renderPassBeginInfo.framebuffer = offScreenFrameBuf.frameBuffer;
    renderPassBeginInfo.renderArea.extent.width = offScreenFrameBuf.width;
    renderPassBeginInfo.renderArea.extent.height = offScreenFrameBuf.height;
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();

    VK_CHECK_RESULT(vkBeginCommandBuffer(offScreenCmdBuffer, &cmdBufInfo));

    vkCmdBeginRenderPass(offScreenCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = vks::initializers::viewport((float)offScreenFrameBuf.width, (float)offScreenFrameBuf.height, 0.0f, 1.0f);
    vkCmdSetViewport(offScreenCmdBuffer, 0, 1, &viewport);

    VkRect2D scissor = vks::initializers::rect2D(offScreenFrameBuf.width, offScreenFrameBuf.height, 0, 0);
    vkCmdSetScissor(offScreenCmdBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.offscreen);

    VkDeviceSize offsets[1] = { 0 };

    // Object
    vkCmdBindDescriptorSets(offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.offscreen, 0, 1, &descriptorSets.model, 0, NULL);
    vkCmdBindVertexBuffers(offScreenCmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &models.model.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(offScreenCmdBuffer, models.model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(offScreenCmdBuffer, models.model.indexCount, 3, 0, 0, 0);

    vkCmdEndRenderPass(offScreenCmdBuffer);

    VK_CHECK_RESULT(vkEndCommandBuffer(offScreenCmdBuffer));
  }

  void loadAssets()
  {
    models.model.loadFromFile(getAssetPath() + "models/armor/armor.dae", vertexLayout, 1.0f, vulkanDevice, queue);

    vks::ModelCreateInfo modelCreateInfo;
    modelCreateInfo.scale = glm::vec3(2.0f);
    modelCreateInfo.uvscale = glm::vec2(4.0f);
    modelCreateInfo.center = glm::vec3(0.0f, 2.35f, 0.0f);

    // Textures
    std::string texFormatSuffix;
    VkFormat texFormat;
    // Get supported compressed texture format
    if (vulkanDevice->features.textureCompressionBC) {
      texFormatSuffix = "_bc3_unorm";
      texFormat = VK_FORMAT_BC3_UNORM_BLOCK;
    }
    else if (vulkanDevice->features.textureCompressionASTC_LDR) {
      texFormatSuffix = "_astc_8x8_unorm";
      texFormat = VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    }
    else if (vulkanDevice->features.textureCompressionETC2) {
      texFormatSuffix = "_etc2_unorm";
      texFormat = VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    }
    else {
      vks::tools::exitFatal("Device does not support any compressed texture format!", "Error");
    }

    textures.model.colorMap.loadFromFile(getAssetPath() + "models/armor/color" + texFormatSuffix + ".ktx", texFormat, vulkanDevice, queue);
  }

  void reBuildCommandBuffers()
  {
    if (!checkCommandBuffers())
    {
      destroyCommandBuffers();
      createCommandBuffers();
    }
    buildCommandBuffers();
  }

  void buildCommandBuffers()
  {
    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

    VkClearValue clearValues[2];
    clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = width;
    renderPassBeginInfo.renderArea.extent.height = height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
    {
      // Set target frame buffer
      renderPassBeginInfo.framebuffer = frameBuffers[i];

      VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

      vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
      vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

      VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
      vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

      // Final composition as full screen quad
      hmdDistortion->drawQuad(drawCmdBuffers[i], descriptorSet);

      vkCmdEndRenderPass(drawCmdBuffers[i]);

      VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
    }
  }



  void setupVertexDescriptions()
  {
    // Binding description
    vertices.bindingDescriptions.resize(1);
    vertices.bindingDescriptions[0] =
      vks::initializers::vertexInputBindingDescription(
        VERTEX_BUFFER_BIND_ID,
        vertexLayout.stride(),
        VK_VERTEX_INPUT_RATE_VERTEX);

    // Attribute descriptions
    vertices.attributeDescriptions.resize(2);
    // Location 0: Position
    vertices.attributeDescriptions[0] =
      vks::initializers::vertexInputAttributeDescription(
        VERTEX_BUFFER_BIND_ID,
        0,
        VK_FORMAT_R32G32B32_SFLOAT,
        0);
    // Location 1: Texture coordinates
    vertices.attributeDescriptions[1] =
      vks::initializers::vertexInputAttributeDescription(
        VERTEX_BUFFER_BIND_ID,
        1,
        VK_FORMAT_R32G32_SFLOAT,
        sizeof(float) * 3);

    vertices.inputState = vks::initializers::pipelineVertexInputStateCreateInfo();
    vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
    vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
    vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
    vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
  }

  void setupDescriptorPool()
  {
    std::vector<VkDescriptorPoolSize> poolSizes =
    {
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8),
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9)
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo =
      vks::initializers::descriptorPoolCreateInfo(
        static_cast<uint32_t>(poolSizes.size()),
        poolSizes.data(),
        3);

    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
  }

  void setupDescriptorSetLayout()
  {
    // Deferred shading layout
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
    {
      // Binding 0 : Vertex shader uniform buffer
      vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT,
        0),
      // Binding 1 : Position texture target / Scene colormap
      vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        1),
      // Binding 4 : Fragment shader uniform buffer
      vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        2),
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayout =
      vks::initializers::descriptorSetLayoutCreateInfo(
        setLayoutBindings.data(),
        static_cast<uint32_t>(setLayoutBindings.size()));

    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
      vks::initializers::pipelineLayoutCreateInfo(
        &descriptorSetLayout,
        1);

    hmdDistortion->createPipeLineLayout(pPipelineLayoutCreateInfo);

    // Offscreen (scene) rendering pipeline layout
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayouts.offscreen));
  }

  void setupDescriptorSet()
  {
    std::vector<VkWriteDescriptorSet> writeDescriptorSets;

    // Textured quad descriptor set
    VkDescriptorSetAllocateInfo allocInfo =
      vks::initializers::descriptorSetAllocateInfo(
        descriptorPool,
        &descriptorSetLayout,
        1);

    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    // Image descriptors for the offscreen color attachments
    VkDescriptorImageInfo texDescriptorPosition =
      vks::initializers::descriptorImageInfo(
        colorSampler,
        offScreenFrameBuf.position.view,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    writeDescriptorSets = {
      // Binding 1 : Position texture target
      vks::initializers::writeDescriptorSet(
        descriptorSet,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        &texDescriptorPosition),
      // Binding 4 : Fragment shader uniform buffer
      hmdDistortion->getUniformWriteDescriptorSet(descriptorSet, 2)
    };

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

    // Offscreen (scene)

    // Model
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.model));
    writeDescriptorSets =
    {
      // Binding 0: Vertex shader uniform buffer
      vks::initializers::writeDescriptorSet(
        descriptorSets.model,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        0,
        &uniformBuffers.vsOffscreen.descriptor),
      // Binding 1: Color map
      vks::initializers::writeDescriptorSet(
        descriptorSets.model,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        &textures.model.colorMap.descriptor)
    };
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
  }

  void preparePipelines()
  {
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
      vks::initializers::pipelineInputAssemblyStateCreateInfo(
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        0,
        VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
      vks::initializers::pipelineRasterizationStateCreateInfo(
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_BACK_BIT,
        VK_FRONT_FACE_CLOCKWISE,
        0);

    VkPipelineColorBlendAttachmentState blendAttachmentState =
      vks::initializers::pipelineColorBlendAttachmentState(
        0xf,
        VK_FALSE);

    VkPipelineColorBlendStateCreateInfo colorBlendState =
      vks::initializers::pipelineColorBlendStateCreateInfo(
        1,
        &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
      vks::initializers::pipelineDepthStencilStateCreateInfo(
        VK_TRUE,
        VK_TRUE,
        VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineViewportStateCreateInfo viewportState =
      vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisampleState =
      vks::initializers::pipelineMultisampleStateCreateInfo(
        VK_SAMPLE_COUNT_1_BIT,
        0);

    std::vector<VkDynamicState> dynamicStateEnables = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState =
      vks::initializers::pipelineDynamicStateCreateInfo(
        dynamicStateEnables.data(),
        static_cast<uint32_t>(dynamicStateEnables.size()),
        0);
    VkGraphicsPipelineCreateInfo pipelineCreateInfo =
      vks::initializers::pipelineCreateInfo(
        nullptr,
        renderPass,
        0);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();

    // Final fullscreen composition pass pipeline
    shaderStages[0] = loadShader(getAssetPath() + "shaders/hmddistortion/distortion.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(getAssetPath() + "shaders/hmddistortion/distortion.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    // Empty vertex input state, quads are generated by the vertex shader
    hmdDistortion->createPipeLine(pipelineCreateInfo, pipelineCache, shaderStages);

    // Debug display pipeline
    pipelineCreateInfo.pVertexInputState = &vertices.inputState;

    // Offscreen pipeline
    shaderStages[0] = loadShader(getAssetPath() + "shaders/hmddistortion/diffuse-pass.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(getAssetPath() + "shaders/hmddistortion/diffuse-pass.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    // Separate render pass
    pipelineCreateInfo.renderPass = offScreenFrameBuf.renderPass;

    // Separate layout
    pipelineCreateInfo.layout = pipelineLayouts.offscreen;

    // Blend attachment states required for all color attachments
    // This is important, as color write mask will otherwise be 0x0 and you
    // won't see anything rendered to the attachment
    std::array<VkPipelineColorBlendAttachmentState, 1> blendAttachmentStates = {
      vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
    };

    colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
    colorBlendState.pAttachments = blendAttachmentStates.data();

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.offscreen));
  }

  // Prepare and initialize uniform buffer containing shader uniforms
  void prepareUniformBuffers()
  {
    // Deferred vertex shader
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &uniformBuffers.vsOffscreen,
      sizeof(uboOffscreenVS)));



    // Map persistent
    VK_CHECK_RESULT(uniformBuffers.vsOffscreen.map());


    hmdDistortion->prepareUniformBuffer(vulkanDevice);

    // Init some values
    uboOffscreenVS.instancePos[0] = glm::vec4(0.0f);
    uboOffscreenVS.instancePos[1] = glm::vec4(-4.0f, 0.0, -4.0f, 0.0f);
    uboOffscreenVS.instancePos[2] = glm::vec4(4.0f, 0.0, -4.0f, 0.0f);

    // Update
    updateUniformBufferDeferredMatrices();
    hmdDistortion->updateUniformBufferWarp();
  }

  void updateUniformBufferDeferredMatrices()
  {
    uboOffscreenVS.projection = camera.matrices.perspective;
    uboOffscreenVS.view = camera.matrices.view;
    uboOffscreenVS.model = glm::mat4();

    memcpy(uniformBuffers.vsOffscreen.mapped, &uboOffscreenVS, sizeof(uboOffscreenVS));
  }

  void draw()
  {
    VulkanExampleBase::prepareFrame();

    // The scene render command buffer has to wait for the offscreen
    // rendering to be finished before we can use the framebuffer
    // color image for sampling during final rendering
    // To ensure this we use a dedicated offscreen synchronization
    // semaphore that will be signaled when offscreen renderin
    // has been finished
    // This is necessary as an implementation may start both
    // command buffers at the same time, there is no guarantee
    // that command buffers will be executed in the order they
    // have been submitted by the application

    // Offscreen rendering

    // Wait for swap chain presentation to finish
    submitInfo.pWaitSemaphores = &semaphores.presentComplete;
    // Signal ready with offscreen semaphore
    submitInfo.pSignalSemaphores = &offscreenSemaphore;

    // Submit work
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &offScreenCmdBuffer;
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

    // Scene rendering

    // Wait for offscreen semaphore
    submitInfo.pWaitSemaphores = &offscreenSemaphore;
    // Signal ready with render complete semaphpre
    submitInfo.pSignalSemaphores = &semaphores.renderComplete;

    // Submit work
    submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

    VulkanExampleBase::submitFrame();
  }

  void prepare()
  {
    VulkanExampleBase::prepare();
    loadAssets();
    hmdDistortion = new VikDistortion(device);
    hmdDistortion->generateQuads(vulkanDevice);
    setupVertexDescriptions();
    prepareOffscreenFramebuffer();
    prepareUniformBuffers();
    setupDescriptorSetLayout();
    preparePipelines();
    setupDescriptorPool();
    setupDescriptorSet();
    buildCommandBuffers();
    buildOffscreenCommandBuffer();
    prepared = true;
  }

  virtual void render()
  {
    if (!prepared)
      return;
    draw();
    hmdDistortion->updateUniformBufferWarp();
  }

  virtual void viewChanged()
  {
    updateUniformBufferDeferredMatrices();
  }

  virtual void keyPressed(uint32_t keyCode)
  {
    switch (keyCode)
    {
    case KEY_F2:
    case GAMEPAD_BUTTON_A:
      updateTextOverlay();
      break;
    }
  }

  virtual void getOverlayText(VulkanTextOverlay *textOverlay)
  {
#if defined(__ANDROID__)
    textOverlay->addText("\"Button A\" to toggle debug display", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#else
    textOverlay->addText("\"F2\" to toggle debug display", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
#endif
    // Render targets
  }
};

VULKAN_EXAMPLE_MAIN()
