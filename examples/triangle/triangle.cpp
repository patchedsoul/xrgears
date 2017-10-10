/*
 * Triangle
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fstream>
#include <vector>
#include <exception>

#include "system/vikApplication.hpp"
#include "render/vikShader.hpp"
#include "scene/vikCameraArcBall.hpp"

#define USE_STAGING true

class Triangle : public vik::Application {
 public:
  struct Vertex {
    float position[3];
    float color[3];
  };

  struct {
    VkDeviceMemory memory;
    VkBuffer buffer;
  } vertices;

  struct {
    VkDeviceMemory memory;
    VkBuffer buffer;
    uint32_t count;
  } indices;

  struct {
    VkDeviceMemory memory;
    VkBuffer buffer;
    VkDescriptorBufferInfo descriptor;
  } uniformBufferVS;

  struct {
    glm::mat4 projectionMatrix;
    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
  } uboVS;

  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;
  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorSet descriptorSet;

  std::vector<VkFence> waitFences;

  Triangle(int argc, char *argv[]) : Application(argc, argv) {
    name = "Triangle";
    camera = new vik::CameraArcBall();
    ((vik::CameraArcBall*)camera)->zoom = -2.5f;
    camera->set_view_updated_cb([this]() { viewUpdated = true; });
  }

  ~Triangle() {
    vkDestroyPipeline(renderer->device, pipeline, nullptr);

    vkDestroyPipelineLayout(renderer->device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(renderer->device, descriptorSetLayout, nullptr);

    vkDestroyBuffer(renderer->device, vertices.buffer, nullptr);
    vkFreeMemory(renderer->device, vertices.memory, nullptr);

    vkDestroyBuffer(renderer->device, indices.buffer, nullptr);
    vkFreeMemory(renderer->device, indices.memory, nullptr);

    vkDestroyBuffer(renderer->device, uniformBufferVS.buffer, nullptr);
    vkFreeMemory(renderer->device, uniformBufferVS.memory, nullptr);

    for (auto& fence : waitFences)
      vkDestroyFence(renderer->device, fence, nullptr);
  }

  uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < renderer->deviceMemoryProperties.memoryTypeCount; i++) {
      if ((typeBits & 1) == 1
          && (renderer->deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
        return i;
      typeBits >>= 1;
    }
    throw "Could not find a suitable memory type!";
  }

  void prepareSynchronizationPrimitives() {
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Create in signaled state so we don't wait on first render of each command buffer
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    waitFences.resize(renderer->cmd_buffers.size());
    for (auto& fence : waitFences)
      vik_log_check(vkCreateFence(renderer->device, &fenceCreateInfo, nullptr, &fence));
  }

  // End the command buffer and submit it to the queue
  // Uses a fence to ensure command buffer has finished executing before deleting it
  void flushCommandBuffer(VkCommandBuffer commandBuffer) {
    assert(commandBuffer != VK_NULL_HANDLE);

    vik_log_check(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &commandBuffer;

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;
    VkFence fence;
    vik_log_check(vkCreateFence(renderer->device, &fenceCreateInfo, nullptr, &fence));

    vik_log_check(vkQueueSubmit(renderer->queue, 1, &submit_info, fence));
    vik_log_check(vkWaitForFences(renderer->device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

    vkDestroyFence(renderer->device, fence, nullptr);
    vkFreeCommandBuffers(renderer->device, renderer->cmd_pool, 1, &commandBuffer);
  }

  void build_command_buffers() {
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufInfo.pNext = nullptr;

    VkClearValue clearValues[2];
    clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = nullptr;
    renderPassBeginInfo.renderPass = renderer->render_pass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = renderer->width;
    renderPassBeginInfo.renderArea.extent.height = renderer->height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    vik_log_d("we will process %ld draw buffers", renderer->cmd_buffers.size());

    for (uint32_t i = 0; i < renderer->cmd_buffers.size(); ++i) {
      renderPassBeginInfo.framebuffer = renderer->frame_buffers[i];

      vik_log_check(vkBeginCommandBuffer(renderer->cmd_buffers[i], &cmdBufInfo));

      vkCmdBeginRenderPass(renderer->cmd_buffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      VkViewport viewport = {};
      viewport.height = (float) renderer->height;
      viewport.width = (float) renderer->width;
      viewport.minDepth = (float) 0.0f;
      viewport.maxDepth = (float) 1.0f;
      vkCmdSetViewport(renderer->cmd_buffers[i], 0, 1, &viewport);

      VkRect2D scissor = {};
      scissor.extent.width = renderer->width;
      scissor.extent.height = renderer->height;
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      vkCmdSetScissor(renderer->cmd_buffers[i], 0, 1, &scissor);

      vkCmdBindDescriptorSets(renderer->cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

      vkCmdBindPipeline(renderer->cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

      VkDeviceSize offsets[1] = { 0 };
      vkCmdBindVertexBuffers(renderer->cmd_buffers[i], 0, 1, &vertices.buffer, offsets);

      vkCmdBindIndexBuffer(renderer->cmd_buffers[i], indices.buffer, 0, VK_INDEX_TYPE_UINT32);

      vkCmdDrawIndexed(renderer->cmd_buffers[i], indices.count, 1, 0, 0, 1);

      vkCmdEndRenderPass(renderer->cmd_buffers[i]);

      vik_log_check(vkEndCommandBuffer(renderer->cmd_buffers[i]));
    }
    vik_log_d("buildCommandBuffers size: %ld", renderer->cmd_buffers.size());
  }

  void draw() {
    vik_log_check(vkWaitForFences(renderer->device, 1, &waitFences[renderer->currentBuffer], VK_TRUE, UINT64_MAX));
    vik_log_check(vkResetFences(renderer->device, 1, &waitFences[renderer->currentBuffer]));

    VkSubmitInfo submit_info = renderer->init_render_submit_info();

    std::array<VkPipelineStageFlags,1> stage_flags = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submit_info.pWaitDstStageMask = stage_flags.data();

    submit_info.pCommandBuffers = renderer->get_current_command_buffer();
    vik_log_check(vkQueueSubmit(renderer->queue, 1, &submit_info, waitFences[renderer->currentBuffer]));
  }

  void prepareVertices(bool useStagingBuffers) {
    std::vector<Vertex> vertexBuffer = {
      { {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
      { { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
      { {  0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
    };
    uint32_t vertexBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(Vertex);

    // Setup indices
    std::vector<uint32_t> indexBuffer = { 0, 1, 2 };
    indices.count = static_cast<uint32_t>(indexBuffer.size());
    uint32_t indexBufferSize = indices.count * sizeof(uint32_t);

    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements memReqs;

    void *data;

    if (useStagingBuffers) {
      // Static data like vertex and index buffer should be stored on the device memory
      // for optimal (and fastest) access by the GPU
      //
      // To achieve this we use so-called "staging buffers" :
      // - Create a buffer that's visible to the host (and can be mapped)
      // - Copy the data to this buffer
      // - Create another buffer that's local on the device (VRAM) with the same size
      // - Copy the data from the host to the device using a command buffer
      // - Delete the host visible (staging) buffer
      // - Use the device local buffers for rendering

      struct StagingBuffer {
        VkDeviceMemory memory;
        VkBuffer buffer;
      };

      struct {
        StagingBuffer vertices;
        StagingBuffer indices;
      } stagingBuffers;

      // Vertex buffer
      VkBufferCreateInfo vertexBufferInfo = {};
      vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      vertexBufferInfo.size = vertexBufferSize;
      // Buffer is used as the copy source
      vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      // Create a host-visible buffer to copy the vertex data to (staging buffer)
      vik_log_check(vkCreateBuffer(renderer->device, &vertexBufferInfo, nullptr, &stagingBuffers.vertices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, stagingBuffers.vertices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      // Request a host visible memory type that can be used to copy our data do
      // Also request it to be coherent, so that writes are visible to the GPU right after unmapping the buffer
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &stagingBuffers.vertices.memory));
      // Map and copy
      vik_log_check(vkMapMemory(renderer->device, stagingBuffers.vertices.memory, 0, memAlloc.allocationSize, 0, &data));
      memcpy(data, vertexBuffer.data(), vertexBufferSize);
      vkUnmapMemory(renderer->device, stagingBuffers.vertices.memory);
      vik_log_check(vkBindBufferMemory(renderer->device, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0));

      // Create a device local buffer to which the (host local) vertex data will be copied and which will be used for rendering
      vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      vik_log_check(vkCreateBuffer(renderer->device, &vertexBufferInfo, nullptr, &vertices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, vertices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &vertices.memory));
      vik_log_check(vkBindBufferMemory(renderer->device, vertices.buffer, vertices.memory, 0));

      // Index buffer
      VkBufferCreateInfo indexbufferInfo = {};
      indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      indexbufferInfo.size = indexBufferSize;
      indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      // Copy index data to a buffer visible to the host (staging buffer)
      vik_log_check(vkCreateBuffer(renderer->device, &indexbufferInfo, nullptr, &stagingBuffers.indices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, stagingBuffers.indices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &stagingBuffers.indices.memory));
      vik_log_check(vkMapMemory(renderer->device, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &data));
      memcpy(data, indexBuffer.data(), indexBufferSize);
      vkUnmapMemory(renderer->device, stagingBuffers.indices.memory);
      vik_log_check(vkBindBufferMemory(renderer->device, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0));

      // Create destination buffer with device only visibility
      indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      vik_log_check(vkCreateBuffer(renderer->device, &indexbufferInfo, nullptr, &indices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, indices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &indices.memory));
      vik_log_check(vkBindBufferMemory(renderer->device, indices.buffer, indices.memory, 0));

      // Buffer copies have to be submitted to a queue, so we need a command buffer for them
      // Note: Some devices offer a dedicated transfer queue (with only the transfer bit set) that may be faster when doing lots of copies
      VkCommandBuffer copyCmd = renderer->create_command_buffer();
      VkCommandBufferBeginInfo cmdBufInfo = vik::initializers::commandBufferBeginInfo();
      vik_log_check(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

      // Put buffer region copies into command buffer
      VkBufferCopy copyRegion = {};

      // Vertex buffer
      copyRegion.size = vertexBufferSize;
      vkCmdCopyBuffer(copyCmd, stagingBuffers.vertices.buffer, vertices.buffer, 1, &copyRegion);
      // Index buffer
      copyRegion.size = indexBufferSize;
      vkCmdCopyBuffer(copyCmd, stagingBuffers.indices.buffer, indices.buffer,  1, &copyRegion);

      // Flushing the command buffer will also submit it to the queue and uses a fence to ensure that all commands have been executed before returning
      flushCommandBuffer(copyCmd);

      // Destroy staging buffers
      // Note: Staging buffer must not be deleted before the copies have been submitted and executed
      vkDestroyBuffer(renderer->device, stagingBuffers.vertices.buffer, nullptr);
      vkFreeMemory(renderer->device, stagingBuffers.vertices.memory, nullptr);
      vkDestroyBuffer(renderer->device, stagingBuffers.indices.buffer, nullptr);
      vkFreeMemory(renderer->device, stagingBuffers.indices.memory, nullptr);
    } else {
      // Don't use staging
      // Create host-visible buffers only and use these for rendering. This is not advised and will usually result in lower rendering performance

      // Vertex buffer
      VkBufferCreateInfo vertexBufferInfo = {};
      vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      vertexBufferInfo.size = vertexBufferSize;
      vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

      // Copy vertex data to a buffer visible to the host
      vik_log_check(vkCreateBuffer(renderer->device, &vertexBufferInfo, nullptr, &vertices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, vertices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT is host visible memory, and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT makes sure writes are directly visible
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &vertices.memory));
      vik_log_check(vkMapMemory(renderer->device, vertices.memory, 0, memAlloc.allocationSize, 0, &data));
      memcpy(data, vertexBuffer.data(), vertexBufferSize);
      vkUnmapMemory(renderer->device, vertices.memory);
      vik_log_check(vkBindBufferMemory(renderer->device, vertices.buffer, vertices.memory, 0));

      // Index buffer
      VkBufferCreateInfo indexbufferInfo = {};
      indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      indexbufferInfo.size = indexBufferSize;
      indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

      // Copy index data to a buffer visible to the host
      vik_log_check(vkCreateBuffer(renderer->device, &indexbufferInfo, nullptr, &indices.buffer));
      vkGetBufferMemoryRequirements(renderer->device, indices.buffer, &memReqs);
      memAlloc.allocationSize = memReqs.size;
      memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      vik_log_check(vkAllocateMemory(renderer->device, &memAlloc, nullptr, &indices.memory));
      vik_log_check(vkMapMemory(renderer->device, indices.memory, 0, indexBufferSize, 0, &data));
      memcpy(data, indexBuffer.data(), indexBufferSize);
      vkUnmapMemory(renderer->device, indices.memory);
      vik_log_check(vkBindBufferMemory(renderer->device, indices.buffer, indices.memory, 0));
    }
  }

  void setupDescriptorPool() {
    VkDescriptorPoolSize typeCounts[1];
    typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    typeCounts[0].descriptorCount = 1;

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = nullptr;
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = typeCounts;
    descriptorPoolInfo.maxSets = 1;

    vik_log_check(vkCreateDescriptorPool(renderer->device, &descriptorPoolInfo, nullptr, &renderer->descriptorPool));
  }

  void setupDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pNext = nullptr;
    descriptorLayout.bindingCount = 1;
    descriptorLayout.pBindings = &layoutBinding;

    vik_log_check(vkCreateDescriptorSetLayout(renderer->device, &descriptorLayout, nullptr, &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = nullptr;
    pPipelineLayoutCreateInfo.setLayoutCount = 1;
    pPipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

    vik_log_check(vkCreatePipelineLayout(renderer->device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
  }

  void setupDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = renderer->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    vik_log_check(vkAllocateDescriptorSets(renderer->device, &allocInfo, &descriptorSet));

    VkWriteDescriptorSet writeDescriptorSet = {};

    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &uniformBufferVS.descriptor;
    writeDescriptorSet.dstBinding = 0;

    vkUpdateDescriptorSets(renderer->device, 1, &writeDescriptorSet, 0, nullptr);
  }

  void preparePipelines() {
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.renderPass = renderer->render_pass;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
    blendAttachmentState[0].colorWriteMask = 0xf;
    blendAttachmentState[0].blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = blendAttachmentState;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    std::vector<VkDynamicState> dynamicStateEnables;
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front = depthStencilState.back;

    VkPipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.pSampleMask = nullptr;

    VkVertexInputBindingDescription vertexInputBinding = {};
    vertexInputBinding.binding = 0;
    vertexInputBinding.stride = sizeof(Vertex);
    vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> vertexInputAttributs;
    vertexInputAttributs[0].binding = 0;
    vertexInputAttributs[0].location = 0;
    vertexInputAttributs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributs[0].offset = offsetof(Vertex, position);
    vertexInputAttributs[1].binding = 0;
    vertexInputAttributs[1].location = 1;
    vertexInputAttributs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributs[1].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputState = {};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
    vertexInputState.vertexAttributeDescriptionCount = 2;
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributs.data();

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
    shaderStages[0] = vik::Shader::load(
          renderer->device,
          "triangle/triangle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = vik::Shader::load(
          renderer->device,
          "triangle/triangle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();

    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.renderPass = renderer->render_pass;
    pipelineCreateInfo.pDynamicState = &dynamicState;

    vik_log_check(vkCreateGraphicsPipelines(renderer->device, renderer->pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

    vkDestroyShaderModule(renderer->device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(renderer->device, shaderStages[1].module, nullptr);
  }

  void prepareUniformBuffers() {
    VkMemoryRequirements memReqs;

    VkBufferCreateInfo bufferInfo = {};
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.allocationSize = 0;
    allocInfo.memoryTypeIndex = 0;

    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(uboVS);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    vik_log_check(vkCreateBuffer(renderer->device, &bufferInfo, nullptr, &uniformBufferVS.buffer));
    vkGetBufferMemoryRequirements(renderer->device, uniformBufferVS.buffer, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vik_log_check(vkAllocateMemory(renderer->device, &allocInfo, nullptr, &(uniformBufferVS.memory)));
    vik_log_check(vkBindBufferMemory(renderer->device, uniformBufferVS.buffer, uniformBufferVS.memory, 0));

    uniformBufferVS.descriptor.buffer = uniformBufferVS.buffer;
    uniformBufferVS.descriptor.offset = 0;
    uniformBufferVS.descriptor.range = sizeof(uboVS);

    updateUniformBuffers();
  }

  void updateUniformBuffers() {
    uboVS.projectionMatrix = camera->get_projection_matrix();
    uboVS.viewMatrix = camera->get_view_matrix();
    uboVS.modelMatrix = glm::mat4();

    uint8_t *pData;
    vik_log_check(vkMapMemory(renderer->device, uniformBufferVS.memory, 0,
                              sizeof(uboVS), 0, (void **)&pData));
    memcpy(pData, &uboVS, sizeof(uboVS));
    vkUnmapMemory(renderer->device, uniformBufferVS.memory);
  }

  void init() {
    Application::init();
    prepareSynchronizationPrimitives();
    prepareVertices(USE_STAGING);

    camera->set_perspective(60.0f,
                            (float)renderer->width / (float)renderer->height,
                            0.001f, 256.0f);

    prepareUniformBuffers();
    setupDescriptorSetLayout();
    preparePipelines();
    setupDescriptorPool();
    setupDescriptorSet();
    build_command_buffers();
  }

  virtual void render() {
    draw();
  }

  virtual void view_changed_cb() {
    updateUniformBuffers();
  }
};

int main(int argc, char *argv[]) {
  Triangle app(argc, argv);
  app.init();
  app.loop();
  return 0;
}
