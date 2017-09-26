/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Based on kmscube example written by Rob Clark, based on test app originally
 * written by Arvin Schnell.
 *
 * Compile and run this with minigbm:
 *
 *   https://chromium.googlesource.com/chromiumos/platform/minigbm
 *
 * Edit the minigbm Makefile to add -DGBM_I915 to CPPFLAGS, then compile and
 * install with make DESTDIR=<some path>. Then pass --with-minigbm=<some path>
 * to configure when configuring vkcube
 */

#include <sys/time.h>
#include <string.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include "system/vikApplicationVkc.hpp"
#include "system/vikLog.hpp"

#include "render/vikRendererVkc.hpp"
#include "render/vikShader.hpp"



class Cube : public vik::ApplicationVkc {
public:

  void *map;

  struct ubo {
    glm::mat4 modelview;
    glm::mat4 modelviewprojection;
    float normal[12];
  };

  Cube(uint32_t w, uint32_t h) : ApplicationVkc(w, h) {}

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


    std::array<VkPipelineColorBlendAttachmentState,1> attachments = {};
    attachments[0].colorWriteMask =
        VK_COLOR_COMPONENT_A_BIT |
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT;

    VkPipelineColorBlendStateCreateInfo colorinfo = {};
    colorinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorinfo.attachmentCount = 1;
    colorinfo.pAttachments = attachments.data();

    VkDynamicState dynamicStates []  = {
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
    pipeLineCreateInfo.layout = renderer->pipeline_layout;
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
                              &renderer->pipeline);
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
                           &renderer->pipeline_layout);
  }

  VkDescriptorPool init_descriptor_pool() {
    std::array<VkDescriptorPoolSize,1> poolsizes = {};
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

    vkAllocateDescriptorSets(renderer->device, &descriptorAllocateInfo, &renderer->descriptor_set);
  }

  void update_descriptor_sets() {
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = renderer->buffer;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range = sizeof(struct ubo);

    std::array<VkWriteDescriptorSet,1> writeDescriptorSet = {};
    writeDescriptorSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet[0].dstSet = renderer->descriptor_set;
    writeDescriptorSet[0].dstBinding = 0;
    writeDescriptorSet[0].dstArrayElement = 0;
    writeDescriptorSet[0].descriptorCount = 1;
    writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet[0].pBufferInfo = &descriptorBufferInfo;

    vkUpdateDescriptorSets(renderer->device, 1, writeDescriptorSet.data(), 0, NULL);
  }

  void init_vertex_buffer() {
    static const float vVertices[] = {
      // front
      -1.0f, -1.0f, +1.0f, // point blue
      +1.0f, -1.0f, +1.0f, // point magenta
      -1.0f, +1.0f, +1.0f, // point cyan
      +1.0f, +1.0f, +1.0f, // point white
      // back
      +1.0f, -1.0f, -1.0f, // point red
      -1.0f, -1.0f, -1.0f, // point black
      +1.0f, +1.0f, -1.0f, // point yellow
      -1.0f, +1.0f, -1.0f, // point green
      // right
      +1.0f, -1.0f, +1.0f, // point magenta
      +1.0f, -1.0f, -1.0f, // point red
      +1.0f, +1.0f, +1.0f, // point white
      +1.0f, +1.0f, -1.0f, // point yellow
      // left
      -1.0f, -1.0f, -1.0f, // point black
      -1.0f, -1.0f, +1.0f, // point blue
      -1.0f, +1.0f, -1.0f, // point green
      -1.0f, +1.0f, +1.0f, // point cyan
      // top
      -1.0f, +1.0f, +1.0f, // point cyan
      +1.0f, +1.0f, +1.0f, // point white
      -1.0f, +1.0f, -1.0f, // point green
      +1.0f, +1.0f, -1.0f, // point yellow
      // bottom
      -1.0f, -1.0f, -1.0f, // point black
      +1.0f, -1.0f, -1.0f, // point red
      -1.0f, -1.0f, +1.0f, // point blue
      +1.0f, -1.0f, +1.0f  // point magenta
    };

    static const float vColors[] = {
      // front
      0.0f,  0.0f,  1.0f, // blue
      1.0f,  0.0f,  1.0f, // magenta
      0.0f,  1.0f,  1.0f, // cyan
      1.0f,  1.0f,  1.0f, // white
      // back
      1.0f,  0.0f,  0.0f, // red
      0.0f,  0.0f,  0.0f, // black
      1.0f,  1.0f,  0.0f, // yellow
      0.0f,  1.0f,  0.0f, // green
      // right
      1.0f,  0.0f,  1.0f, // magenta
      1.0f,  0.0f,  0.0f, // red
      1.0f,  1.0f,  1.0f, // white
      1.0f,  1.0f,  0.0f, // yellow
      // left
      0.0f,  0.0f,  0.0f, // black
      0.0f,  0.0f,  1.0f, // blue
      0.0f,  1.0f,  0.0f, // green
      0.0f,  1.0f,  1.0f, // cyan
      // top
      0.0f,  1.0f,  1.0f, // cyan
      1.0f,  1.0f,  1.0f, // white
      0.0f,  1.0f,  0.0f, // green
      1.0f,  1.0f,  0.0f, // yellow
      // bottom
      0.0f,  0.0f,  0.0f, // black
      1.0f,  0.0f,  0.0f, // red
      0.0f,  0.0f,  1.0f, // blue
      1.0f,  0.0f,  1.0f  // magenta
    };

    static const float vNormals[] = {
      // front
      +0.0f, +0.0f, +1.0f, // forward
      +0.0f, +0.0f, +1.0f, // forward
      +0.0f, +0.0f, +1.0f, // forward
      +0.0f, +0.0f, +1.0f, // forward
      // back
      +0.0f, +0.0f, -1.0f, // backbard
      +0.0f, +0.0f, -1.0f, // backbard
      +0.0f, +0.0f, -1.0f, // backbard
      +0.0f, +0.0f, -1.0f, // backbard
      // right
      +1.0f, +0.0f, +0.0f, // right
      +1.0f, +0.0f, +0.0f, // right
      +1.0f, +0.0f, +0.0f, // right
      +1.0f, +0.0f, +0.0f, // right
      // left
      -1.0f, +0.0f, +0.0f, // left
      -1.0f, +0.0f, +0.0f, // left
      -1.0f, +0.0f, +0.0f, // left
      -1.0f, +0.0f, +0.0f, // left
      // top
      +0.0f, +1.0f, +0.0f, // up
      +0.0f, +1.0f, +0.0f, // up
      +0.0f, +1.0f, +0.0f, // up
      +0.0f, +1.0f, +0.0f, // up
      // bottom
      +0.0f, -1.0f, +0.0f, // down
      +0.0f, -1.0f, +0.0f, // down
      +0.0f, -1.0f, +0.0f, // down
      +0.0f, -1.0f, +0.0f  // down
    };

    renderer->vertex_offset = sizeof(struct ubo);
    renderer->colors_offset = renderer->vertex_offset + sizeof(vVertices);
    renderer->normals_offset = renderer->colors_offset + sizeof(vColors);
    uint32_t mem_size = renderer->normals_offset + sizeof(vNormals);

    VkMemoryAllocateInfo allcoinfo;
    allcoinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allcoinfo.allocationSize = mem_size;
    allcoinfo.memoryTypeIndex = 0;

    vkAllocateMemory(renderer->device,
                     &allcoinfo,
                     NULL,
                     &renderer->mem);

    VkResult r = vkMapMemory(renderer->device, renderer->mem, 0, mem_size, 0, &map);
    vik_log_f_if(r != VK_SUCCESS, "vkMapMemory failed");

    memcpy(((char*)map + renderer->vertex_offset), vVertices, sizeof(vVertices));
    memcpy(((char*)map + renderer->colors_offset), vColors, sizeof(vColors));
    memcpy(((char*)map + renderer->normals_offset), vNormals, sizeof(vNormals));

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = mem_size;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.flags = 0;

    vkCreateBuffer(renderer->device,
                   &bufferInfo,
                   NULL,
                   &renderer->buffer);

    vkBindBufferMemory(renderer->device, renderer->buffer, renderer->mem, 0);
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

  void update_scene() {
    uint64_t t = renderer->get_animation_time();
    update_uniform_buffer(t);
  }

  void init_cb() {
    renderer->init_render_pass(renderer->swap_chain->surface_format.format);

    VkDescriptorSetLayout set_layout = init_descriptor_set_layout();
    init_pipeline_layout(set_layout);
    init_pipeline();
    init_vertex_buffer();
    VkDescriptorPool descriptor_pool = init_descriptor_pool();
    init_descriptor_sets(descriptor_pool, set_layout);
    update_descriptor_sets();

    renderer->init_vk_objects();
  }

};

int main(int argc, char *argv[]) {
  //Cube app(1920, 1200);
  Cube app(2560, 1440);
  app.parse_args(argc, argv);
  app.init();
  app.loop();
  return 0;
}
