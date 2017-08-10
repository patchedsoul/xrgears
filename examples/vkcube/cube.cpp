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

#include "common.h"
#include "silo.h"

struct ubo {
  ESMatrix modelview;
  ESMatrix modelviewprojection;
  float normal[12];
};


static unsigned char vs_spirv_source[] = {
  #include "vkcube.vert.spv.h"
};

static unsigned char fs_spirv_source[] = {
  #include "vkcube.frag.spv.h"
};


static void
init_cube(struct vkcube *vc)
{
  VkResult r;

  VkDescriptorSetLayout set_layout;
  VkDescriptorSetLayoutBinding bindings = {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    .pImmutableSamplers = NULL
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .bindingCount = 1,
    .pBindings = &bindings
  };

  vkCreateDescriptorSetLayout(vc->device,
                              &layoutInfo,
                              NULL,
                              &set_layout);

  VkPipelineLayoutCreateInfo pipeLineInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &set_layout
  };

  vkCreatePipelineLayout(vc->device,
                         &pipeLineInfo,
                         NULL,
                         &vc->pipeline_layout);

  VkVertexInputBindingDescription foo[] = {
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

  VkVertexInputAttributeDescription bar[] = {
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


  VkPipelineVertexInputStateCreateInfo vi_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 3,
    .pVertexBindingDescriptions = foo,
    .vertexAttributeDescriptionCount = 3,
    .pVertexAttributeDescriptions = bar
  };

  VkShaderModule vs_module;

  VkShaderModuleCreateInfo shaderInfo = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = sizeof(vs_spirv_source),
    .pCode = (uint32_t *)vs_spirv_source,
  };

  vkCreateShaderModule(vc->device,
                       &shaderInfo,
                       NULL,
                       &vs_module);


  VkShaderModuleCreateInfo shaderInfo2 = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = sizeof(fs_spirv_source),
    .pCode = (uint32_t *)fs_spirv_source,
  };

  VkShaderModule fs_module;
  vkCreateShaderModule(vc->device,
                       &shaderInfo2,
                       NULL,
                       &fs_module);


  VkPipelineShaderStageCreateInfo stagesInfo[] =  {
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vs_module,
      .pName = "main",
    },
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fs_module,
      .pName = "main",
    },
  };

  VkPipelineInputAssemblyStateCreateInfo assmblyInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    .primitiveRestartEnable = false,
  };

  VkPipelineViewportStateCreateInfo viewPorInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
  };

  VkPipelineRasterizationStateCreateInfo rasterInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .rasterizerDiscardEnable = false,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multiInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = (VkSampleCountFlagBits) 1,
  };


  VkPipelineDepthStencilStateCreateInfo sencilinfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
  };


  VkPipelineColorBlendAttachmentState attachments [] = {
    { .colorWriteMask = VK_COLOR_COMPONENT_A_BIT |
      VK_COLOR_COMPONENT_R_BIT |
      VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT },
  };

  VkPipelineColorBlendStateCreateInfo colorinfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = attachments
  };

  VkDynamicState dynamicstates []  = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineDynamicStateCreateInfo dynamicinfo =  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = 2,
    .pDynamicStates = dynamicstates,
  };

  VkPipeline basehandle = { 0 };

  VkGraphicsPipelineCreateInfo pipeLineCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages = stagesInfo,
    .pVertexInputState = &vi_create_info,
    .pInputAssemblyState = &assmblyInfo,
    .pViewportState = &viewPorInfo,
    .pRasterizationState = &rasterInfo,
    .pMultisampleState = &multiInfo,
    .pDepthStencilState = &sencilinfo,
    .pColorBlendState = &colorinfo,
    .pDynamicState = &dynamicinfo,
    .flags = 0,
    .layout = vc->pipeline_layout,
    .renderPass = vc->render_pass,
    .subpass = 0,
    .basePipelineHandle = basehandle,
    .basePipelineIndex = 0
  };

  VkPipelineCache cache = { VK_NULL_HANDLE };

  vkCreateGraphicsPipelines(vc->device,
                            cache,
                            1,
                            &pipeLineCreateInfo,
                            NULL,
                            &vc->pipeline);

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

  vc->vertex_offset = sizeof(struct ubo);
  vc->colors_offset = vc->vertex_offset + sizeof(vVertices);
  vc->normals_offset = vc->colors_offset + sizeof(vColors);
  uint32_t mem_size = vc->normals_offset + sizeof(vNormals);

  VkMemoryAllocateInfo allcoinfo = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = mem_size,
    .memoryTypeIndex = 0,
  };

  vkAllocateMemory(vc->device,
                   &allcoinfo,
                   NULL,
                   &vc->mem);

  r = vkMapMemory(vc->device, vc->mem, 0, mem_size, 0, &vc->map);
  if (r != VK_SUCCESS)
    fail("vkMapMemory failed");


  memcpy(((char*)vc->map + vc->vertex_offset), vVertices, sizeof(vVertices));
  memcpy(((char*)vc->map + vc->colors_offset), vColors, sizeof(vColors));
  memcpy(((char*)vc->map + vc->normals_offset), vNormals, sizeof(vNormals));


  VkBufferCreateInfo bufferInfo = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = mem_size,
    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    .flags = 0
  };

  vkCreateBuffer(vc->device,
                 &bufferInfo,
                 NULL,
                 &vc->buffer);

  vkBindBufferMemory(vc->device, vc->buffer, vc->mem, 0);

  VkDescriptorPoolSize poolsizes[] = {
    {
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1
    }      };

  VkDescriptorPool desc_pool;
  const VkDescriptorPoolCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .maxSets = 1,
    .poolSizeCount = 1,
    .pPoolSizes = poolsizes,
  };

  vkCreateDescriptorPool(vc->device, &create_info, NULL, &desc_pool);

  VkDescriptorSetAllocateInfo allocateinfo2 = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = desc_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &set_layout,
  };

  vkAllocateDescriptorSets(vc->device,
                           &allocateinfo2, &vc->descriptor_set);

  VkDescriptorBufferInfo bufferinfo3 = {
    .buffer = vc->buffer,
    .offset = 0,
    .range = sizeof(struct ubo),
  };


  VkWriteDescriptorSet descriptroset[] = {
    {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = vc->descriptor_set,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo = &bufferinfo3
    }
  };

  vkUpdateDescriptorSets(vc->device, 1,
                         descriptroset,
                         0, NULL);
}

static void
render_cube(struct vkcube *vc, struct vkcube_buffer *b)
{
  struct ubo ubo;
  struct timeval tv;
  uint64_t t;

  gettimeofday(&tv, NULL);

  t = ((tv.tv_sec * 1000 + tv.tv_usec / 1000) -
       (vc->start_tv.tv_sec * 1000 + vc->start_tv.tv_usec / 1000)) / 5;

  esMatrixLoadIdentity(&ubo.modelview);
  esTranslate(&ubo.modelview, 0.0f, 0.0f, -8.0f);
  esRotate(&ubo.modelview, 45.0f + (0.25f * t), 1.0f, 0.0f, 0.0f);
  esRotate(&ubo.modelview, 45.0f - (0.5f * t), 0.0f, 1.0f, 0.0f);
  esRotate(&ubo.modelview, 10.0f + (0.15f * t), 0.0f, 0.0f, 1.0f);

  float aspect = (float) vc->height / (float) vc->width;
  ESMatrix projection;
  esMatrixLoadIdentity(&projection);
  esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);

  esMatrixLoadIdentity(&ubo.modelviewprojection);
  esMatrixMultiply(&ubo.modelviewprojection, &ubo.modelview, &projection);

  /* The mat3 normalMatrix is laid out as 3 vec4s. */
  memcpy(ubo.normal, &ubo.modelview, sizeof ubo.normal);

  memcpy(vc->map, &ubo, sizeof(ubo));


  VkCommandBufferAllocateInfo allocateinfo14 = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = vc->cmd_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  VkCommandBuffer cmd_buffer;
  vkAllocateCommandBuffers(vc->device,
                           &allocateinfo14,
                           &cmd_buffer);

  VkCommandBufferBeginInfo cmdinfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = 0
  };

  vkBeginCommandBuffer(cmd_buffer,
                       &cmdinfo);


  VkClearValue valuees[] = {
    { .color = { .float32 = { 0.2f, 0.2f, 0.2f, 1.0f } } }
  };

  VkRenderPassBeginInfo passbnegininfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = vc->render_pass,
    .framebuffer = b->framebuffer,
    .renderArea = { { 0, 0 }, { vc->width, vc->height } },
    .clearValueCount = 1,
    .pClearValues = valuees
  };

  vkCmdBeginRenderPass(cmd_buffer,
                       &passbnegininfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  VkBuffer buffers[] = {
    vc->buffer,
    vc->buffer,
    vc->buffer
  };

  VkDeviceSize sizes[] = {
    vc->vertex_offset,
    vc->colors_offset,
    vc->normals_offset
  };


  vkCmdBindVertexBuffers(cmd_buffer, 0, 3,
                         buffers,
                         sizes);

  vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vc->pipeline);

  vkCmdBindDescriptorSets(cmd_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          vc->pipeline_layout,
                          0, 1,
                          &vc->descriptor_set, 0, NULL);

  const VkViewport viewport = {
    .x = 0,
    .y = 0,
    .width = (float)vc->width,
    .height = (float)vc->height,
    .minDepth = 0,
    .maxDepth = 1,
  };
  vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

  const VkRect2D scissor = {
    .offset = { 0, 0 },
    .extent = { vc->width, vc->height },
  };
  vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

  vkCmdDraw(cmd_buffer, 4, 1, 0, 0);
  vkCmdDraw(cmd_buffer, 4, 1, 4, 0);
  vkCmdDraw(cmd_buffer, 4, 1, 8, 0);
  vkCmdDraw(cmd_buffer, 4, 1, 12, 0);
  vkCmdDraw(cmd_buffer, 4, 1, 16, 0);
  vkCmdDraw(cmd_buffer, 4, 1, 20, 0);

  vkCmdEndRenderPass(cmd_buffer);

  vkEndCommandBuffer(cmd_buffer);

  VkPipelineStageFlags stageflags[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
  };


  VkSubmitInfo submitinfo = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &vc->semaphore,
    .pWaitDstStageMask = stageflags,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd_buffer,
  };

  vkQueueSubmit(vc->queue, 1,
                &submitinfo, vc->fence);

  VkFence fenss[] = { vc->fence };

  vkWaitForFences(vc->device, 1, fenss, true, INT64_MAX);
  vkResetFences(vc->device, 1, &vc->fence);

  vkResetCommandPool(vc->device, vc->cmd_pool, 0);
}

struct model cube_model = {
  .init = init_cube,
  .render = render_cube
};