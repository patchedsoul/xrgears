/*
 * xrcube
 *
 * Copyright 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright 2012 Rob Clark <rob@ti.com>
 * Copyright 2015 Intel Corporation
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 *
 * Based on vkcube example.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <fstream>
#include <vector>
#include <exception>

#include "system/vikApplication.hpp"
#include "system/vikLog.hpp"
#include "render/vikShader.hpp"
#include "render/vikTools.hpp"

class XrCube : public vik::Application {
 public:
  void *map;

  struct ubo {
    glm::mat4 modelview;
    glm::mat4 modelviewprojection;
    float normal[12];
  };

  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  VkDeviceMemory mem;
  VkBuffer buffer;
  VkDescriptorSet descriptor_set;
  VkFence fence;
  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorPool descriptor_pool;

  timeval start_tv;

  uint32_t vertex_offset, colors_offset, normals_offset;

  XrCube(int argc, char *argv[]) : Application(argc, argv) {
    name = "Cube";
    camera = new vik::Camera();
  }

  ~XrCube() {
    vkDestroyPipeline(renderer->device, pipeline, nullptr);
    vkDestroyFence(renderer->device, fence, nullptr);
    vkFreeMemory(renderer->device, mem, nullptr);
    vkDestroyBuffer(renderer->device, buffer, nullptr);
    vkDestroyPipelineLayout(renderer->device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(renderer->device, descriptor_set_layout, nullptr);
    vkDestroyDescriptorPool(renderer->device, descriptor_pool, nullptr);
  }

  void build_command_buffers() {
    for (uint32_t i = 0; i < renderer->cmd_buffers.size(); ++i) {
      build_command_buffer(renderer->cmd_buffers[i], renderer->frame_buffers[i]);
    }
  }

  void init() {
    Application::init();
    descriptor_set_layout = init_descriptor_set_layout();
    init_pipeline_layout(descriptor_set_layout);
    init_pipeline();
    init_vertex_buffer();
    descriptor_pool = init_descriptor_pool();
    init_descriptor_sets(descriptor_pool, descriptor_set_layout);
    update_descriptor_sets();

    VkFenceCreateInfo fenceinfo = {};
    fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceinfo.flags = 0;

    vkCreateFence(renderer->device,
                  &fenceinfo,
                  NULL,
                  &fence);

    gettimeofday(&start_tv, NULL);

    build_command_buffers();
  }

  void submit_queue(VkCommandBuffer cmd_buffer) {
    VkSubmitInfo submit_info = renderer->init_render_submit_info();

    std::array<VkPipelineStageFlags, 1> stage_flags = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submit_info.pWaitDstStageMask = stage_flags.data();

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer;

    vkQueueSubmit(renderer->queue, 1, &submit_info, fence);
  }

  virtual void render() {
    uint64_t t = get_animation_time();
    update_uniform_buffer(t);
    submit_queue(renderer->cmd_buffers[renderer->current_buffer]);
    VkFence fences[] = { fence };
    vkWaitForFences(renderer->device, 1, fences, VK_TRUE, INT64_MAX);
    vkResetFences(renderer->device, 1, &fence);
  }

  static uint64_t get_ms_from_tv(const timeval& tv) {
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
  }

  uint64_t get_animation_time() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return (get_ms_from_tv(tv) - get_ms_from_tv(start_tv)) / 5;
  }

  virtual void view_changed_cb() {
  }

  void init_pipeline() {
    VkVertexInputBindingDescription vertexBinding[] = {
      {
        .binding = 0,
        .stride = 3 * sizeof(float),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
      },
      {
        .binding = 1,
        .stride = 3 * sizeof(float),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
      },
      {
        .binding = 2,
        .stride = 3 * sizeof(float),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
      }
    };

    VkVertexInputAttributeDescription vertexAttribute[] = {
      {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0
      },
      {
        .location = 1,
        .binding = 1,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0
      },
      {
        .location = 2,
        .binding = 2,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0
      }
    };

    VkPipelineVertexInputStateCreateInfo vi_create_info = {};
    vi_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi_create_info.vertexBindingDescriptionCount = 3;
    vi_create_info.pVertexBindingDescriptions = vertexBinding;
    vi_create_info.vertexAttributeDescriptionCount = 3;
    vi_create_info.pVertexAttributeDescriptions = vertexAttribute;

    VkPipelineShaderStageCreateInfo stagesInfo[] =  {
      vik::Shader::load(renderer->device, "vkcube/vkcube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
      vik::Shader::load(renderer->device, "vkcube/vkcube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    VkPipelineInputAssemblyStateCreateInfo assmblyInfo = {};
    assmblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assmblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    assmblyInfo.primitiveRestartEnable = false;

    VkPipelineViewportStateCreateInfo viewPorInfo = {};
    viewPorInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewPorInfo.viewportCount = 1;
    viewPorInfo.scissorCount = 1;


    VkPipelineRasterizationStateCreateInfo rasterInfo = {};
    rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterInfo.rasterizerDiscardEnable = false;
    rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterInfo.lineWidth = 1.0f;


    VkPipelineMultisampleStateCreateInfo multiInfo = {};
    multiInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multiInfo.rasterizationSamples = (VkSampleCountFlagBits) 1;


    VkPipelineDepthStencilStateCreateInfo sencilinfo = {};
    sencilinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;


    std::array<VkPipelineColorBlendAttachmentState, 1> attachments = {};
    attachments[0].colorWriteMask =
        VK_COLOR_COMPONENT_A_BIT |
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT;

    VkPipelineColorBlendStateCreateInfo colorinfo = {};
    colorinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorinfo.attachmentCount = 1;
    colorinfo.pAttachments = attachments.data();

    VkDynamicState dynamicStates[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicinfo = {};
    dynamicinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicinfo.dynamicStateCount = 2;
    dynamicinfo.pDynamicStates = dynamicStates;

    VkPipeline basehandle = { 0 };

    VkGraphicsPipelineCreateInfo pipeLineCreateInfo = {};
    pipeLineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeLineCreateInfo.stageCount = 2;
    pipeLineCreateInfo.pStages = stagesInfo;
    pipeLineCreateInfo.pVertexInputState = &vi_create_info;
    pipeLineCreateInfo.pInputAssemblyState = &assmblyInfo;
    pipeLineCreateInfo.pViewportState = &viewPorInfo;
    pipeLineCreateInfo.pRasterizationState = &rasterInfo;
    pipeLineCreateInfo.pMultisampleState = &multiInfo;
    pipeLineCreateInfo.pDepthStencilState = &sencilinfo;
    pipeLineCreateInfo.pColorBlendState = &colorinfo;
    pipeLineCreateInfo.pDynamicState = &dynamicinfo;
    pipeLineCreateInfo.flags = 0;
    pipeLineCreateInfo.layout = pipeline_layout;
    pipeLineCreateInfo.renderPass = renderer->render_pass;
    pipeLineCreateInfo.subpass = 0;
    pipeLineCreateInfo.basePipelineHandle = basehandle;
    pipeLineCreateInfo.basePipelineIndex = 0;

    VkPipelineCache cache = { VK_NULL_HANDLE };

    vkCreateGraphicsPipelines(renderer->device,
                              cache,
                              1,
                              &pipeLineCreateInfo,
                              NULL,
                              &pipeline);

    vkDestroyShaderModule(renderer->device, stagesInfo[0].module, nullptr);
    vkDestroyShaderModule(renderer->device, stagesInfo[1].module, nullptr);
  }

  VkDescriptorSetLayout init_descriptor_set_layout() {
    VkDescriptorSetLayoutBinding bindings = {};
    bindings.binding = 0;
    bindings.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings.descriptorCount = 1;
    bindings.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings.pImmutableSamplers = NULL;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = NULL;
    layoutInfo.flags = 0;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &bindings;

    VkDescriptorSetLayout set_layout;
    vkCreateDescriptorSetLayout(renderer->device,
                                &layoutInfo,
                                NULL,
                                &set_layout);
    return set_layout;
  }

  void init_pipeline_layout(const VkDescriptorSetLayout& set_layout) {
    VkPipelineLayoutCreateInfo pipeLineInfo = {};
    pipeLineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeLineInfo.setLayoutCount = 1;
    pipeLineInfo.pSetLayouts = &set_layout;

    vkCreatePipelineLayout(renderer->device,
                           &pipeLineInfo,
                           NULL,
                           &pipeline_layout);
  }

  VkDescriptorPool init_descriptor_pool() {
    std::array<VkDescriptorPoolSize, 1> poolsizes = {};
    poolsizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolsizes[0].descriptorCount = 1;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = NULL;
    descriptorPoolCreateInfo.flags = 0;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = poolsizes.data();

    VkDescriptorPool descriptorPool;
    vkCreateDescriptorPool(renderer->device, &descriptorPoolCreateInfo, NULL, &descriptorPool);

    return descriptorPool;
  }

  void init_descriptor_sets(const VkDescriptorPool& descriptorPool,
                            const VkDescriptorSetLayout& set_layout) {
    VkDescriptorSetAllocateInfo descriptorAllocateInfo = {};
    descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocateInfo.descriptorPool = descriptorPool;
    descriptorAllocateInfo.descriptorSetCount = 1;
    descriptorAllocateInfo.pSetLayouts = &set_layout;

    vkAllocateDescriptorSets(renderer->device, &descriptorAllocateInfo, &descriptor_set);
  }

  void update_descriptor_sets() {
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = buffer;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = sizeof(struct ubo);

    std::array<VkWriteDescriptorSet, 1> writeDescriptorSet = {};
    writeDescriptorSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet[0].dstSet = descriptor_set;
    writeDescriptorSet[0].dstBinding = 0;
    writeDescriptorSet[0].dstArrayElement = 0;
    writeDescriptorSet[0].descriptorCount = 1;
    writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet[0].pBufferInfo = &descriptorBufferInfo;

    vkUpdateDescriptorSets(renderer->device, 1, writeDescriptorSet.data(), 0, NULL);
  }

  uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < renderer->device_memory_properties.memoryTypeCount; i++) {
      if ((typeBits & 1) == 1
          && (renderer->device_memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
        return i;
      typeBits >>= 1;
    }
    throw "Could not find a suitable memory type!";
  }

  void init_vertex_buffer() {
    static const float vVertices[] = {
      // front
      -1.0f, -1.0f, +1.0f,  // point blue
      +1.0f, -1.0f, +1.0f,  // point magenta
      -1.0f, +1.0f, +1.0f,  // point cyan
      +1.0f, +1.0f, +1.0f,  // point white
      // back
      +1.0f, -1.0f, -1.0f,  // point red
      -1.0f, -1.0f, -1.0f,  // point black
      +1.0f, +1.0f, -1.0f,  // point yellow
      -1.0f, +1.0f, -1.0f,  // point green
      // right
      +1.0f, -1.0f, +1.0f,  // point magenta
      +1.0f, -1.0f, -1.0f,  // point red
      +1.0f, +1.0f, +1.0f,  // point white
      +1.0f, +1.0f, -1.0f,  // point yellow
      // left
      -1.0f, -1.0f, -1.0f,  // point black
      -1.0f, -1.0f, +1.0f,  // point blue
      -1.0f, +1.0f, -1.0f,  // point green
      -1.0f, +1.0f, +1.0f,  // point cyan
      // top
      -1.0f, +1.0f, +1.0f,  // point cyan
      +1.0f, +1.0f, +1.0f,  // point white
      -1.0f, +1.0f, -1.0f,  // point green
      +1.0f, +1.0f, -1.0f,  // point yellow
      // bottom
      -1.0f, -1.0f, -1.0f,  // point black
      +1.0f, -1.0f, -1.0f,  // point red
      -1.0f, -1.0f, +1.0f,  // point blue
      +1.0f, -1.0f, +1.0f   // point magenta
    };

    static const float vColors[] = {
      // front
      0.0f,  0.0f,  1.0f,  // blue
      1.0f,  0.0f,  1.0f,  // magenta
      0.0f,  1.0f,  1.0f,  // cyan
      1.0f,  1.0f,  1.0f,  // white
      // back
      1.0f,  0.0f,  0.0f,  // red
      0.0f,  0.0f,  0.0f,  // black
      1.0f,  1.0f,  0.0f,  // yellow
      0.0f,  1.0f,  0.0f,  // green
      // right
      1.0f,  0.0f,  1.0f,  // magenta
      1.0f,  0.0f,  0.0f,  // red
      1.0f,  1.0f,  1.0f,  // white
      1.0f,  1.0f,  0.0f,  // yellow
      // left
      0.0f,  0.0f,  0.0f,  // black
      0.0f,  0.0f,  1.0f,  // blue
      0.0f,  1.0f,  0.0f,  // green
      0.0f,  1.0f,  1.0f,  // cyan
      // top
      0.0f,  1.0f,  1.0f,  // cyan
      1.0f,  1.0f,  1.0f,  // white
      0.0f,  1.0f,  0.0f,  // green
      1.0f,  1.0f,  0.0f,  // yellow
      // bottom
      0.0f,  0.0f,  0.0f,  // black
      1.0f,  0.0f,  0.0f,  // red
      0.0f,  0.0f,  1.0f,  // blue
      1.0f,  0.0f,  1.0f   // magenta
    };

    static const float vNormals[] = {
      // front
      +0.0f, +0.0f, +1.0f,  // forward
      +0.0f, +0.0f, +1.0f,  // forward
      +0.0f, +0.0f, +1.0f,  // forward
      +0.0f, +0.0f, +1.0f,  // forward
      // back
      +0.0f, +0.0f, -1.0f,  // backbard
      +0.0f, +0.0f, -1.0f,  // backbard
      +0.0f, +0.0f, -1.0f,  // backbard
      +0.0f, +0.0f, -1.0f,  // backbard
      // right
      +1.0f, +0.0f, +0.0f,  // right
      +1.0f, +0.0f, +0.0f,  // right
      +1.0f, +0.0f, +0.0f,  // right
      +1.0f, +0.0f, +0.0f,  // right
      // left
      -1.0f, +0.0f, +0.0f,  // left
      -1.0f, +0.0f, +0.0f,  // left
      -1.0f, +0.0f, +0.0f,  // left
      -1.0f, +0.0f, +0.0f,  // left
      // top
      +0.0f, +1.0f, +0.0f,  // up
      +0.0f, +1.0f, +0.0f,  // up
      +0.0f, +1.0f, +0.0f,  // up
      +0.0f, +1.0f, +0.0f,  // up
      // bottom
      +0.0f, -1.0f, +0.0f,  // down
      +0.0f, -1.0f, +0.0f,  // down
      +0.0f, -1.0f, +0.0f,  // down
      +0.0f, -1.0f, +0.0f   // down
    };

    vertex_offset = sizeof(struct ubo);
    colors_offset = vertex_offset + sizeof(vVertices);
    normals_offset = colors_offset + sizeof(vColors);
    uint32_t mem_size = normals_offset + sizeof(vNormals);


    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = mem_size;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.flags = 0;

    vkCreateBuffer(renderer->device,
                   &bufferInfo,
                   nullptr,
                   &buffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(renderer->device, buffer, &memReqs);

    VkMemoryAllocateInfo allcoinfo = {};
    allcoinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allcoinfo.allocationSize = memReqs.size;
    allcoinfo.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkResult r = vkAllocateMemory(renderer->device,
                                   &allcoinfo,
				    nullptr,
				   &mem);
    vik_log_f_if(r != VK_SUCCESS, "vkAllocateMemory failed");

    r = vkMapMemory(renderer->device, mem, 0, allcoinfo.allocationSize, 0, &map);
    vik_log_f_if(r != VK_SUCCESS, "vkMapMemory failed");

    memcpy(((char*)map + vertex_offset), vVertices, sizeof(vVertices));
    memcpy(((char*)map + colors_offset), vColors, sizeof(vColors));
    memcpy(((char*)map + normals_offset), vNormals, sizeof(vNormals));

    vkBindBufferMemory(renderer->device, buffer, mem, 0);
  }

  void update_uniform_buffer(uint64_t t) {
    ubo cube_ubo;

    glm::mat4 t_matrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -8.0f));
    glm::mat4 r_matrix = glm::eulerAngleYXZ(
          glm::radians(45.0f + (0.25f * t)),
          glm::radians(45.0f - (0.5f * t)),
          glm::radians(10.0f + (0.15f * t)));

    float aspect = (float) renderer->height / (float) renderer->width;

    cube_ubo.modelview = t_matrix * r_matrix;

    glm::mat4 p_matrix = glm::frustum(-2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 3.5f, 10.0f);

    cube_ubo.modelviewprojection = p_matrix * cube_ubo.modelview;

    /* The mat3 normalMatrix is laid out as 3 vec4s. */
    memcpy(cube_ubo.normal, &cube_ubo.modelview, sizeof cube_ubo.normal);

    memcpy(map, &cube_ubo, sizeof(ubo));
  }

  void build_command_buffer(VkCommandBuffer cmd_buffer,
                            VkFramebuffer frame_buffer) {
    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBufferBeginInfo.flags = 0;

    VkResult r = vkBeginCommandBuffer(cmd_buffer, &cmdBufferBeginInfo);
    vik_log_e_if(r != VK_SUCCESS, "vkBeginCommandBuffer: %s",
                 vik::Log::result_string(r).c_str());

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = { { 0.2f, 0.2f, 0.2f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo passBeginInfo = {};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = renderer->render_pass;
    passBeginInfo.framebuffer = frame_buffer;
    passBeginInfo.renderArea = { { 0, 0 }, { renderer->width, renderer->height } };
    passBeginInfo.clearValueCount = clearValues.size();
    passBeginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd_buffer,
                         &passBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    VkBuffer buffers[] = {
      buffer,
      buffer,
      buffer
    };

    VkDeviceSize offsets[] = {
      vertex_offset,
      colors_offset,
      normals_offset
    };

    vkCmdBindVertexBuffers(cmd_buffer, 0, 3,
                           buffers,
                           offsets);

    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdBindDescriptorSets(cmd_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout,
                            0, 1,
                            &descriptor_set, 0, NULL);

    const VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = (float)renderer->width,
      .height = (float)renderer->height,
      .minDepth = 0,
      .maxDepth = 1,
    };
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

    const VkRect2D scissor = {
      .offset = { 0, 0 },
      .extent = { renderer->width, renderer->height },
    };
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    vkCmdDraw(cmd_buffer, 4, 1, 0, 0);
    vkCmdDraw(cmd_buffer, 4, 1, 4, 0);
    vkCmdDraw(cmd_buffer, 4, 1, 8, 0);
    vkCmdDraw(cmd_buffer, 4, 1, 12, 0);
    vkCmdDraw(cmd_buffer, 4, 1, 16, 0);
    vkCmdDraw(cmd_buffer, 4, 1, 20, 0);

    vkCmdEndRenderPass(cmd_buffer);

    r = vkEndCommandBuffer(cmd_buffer);
    vik_log_e_if(r != VK_SUCCESS,
                 "vkEndCommandBuffer: %s", vik::Log::result_string(r).c_str());
  }
};

int main(int argc, char *argv[]) {
  XrCube app(argc, argv);
  app.init();
  app.loop();
  return 0;
}
