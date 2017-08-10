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

#pragma once

#include <sys/time.h>
#include <string.h>

#include "silo.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "VikRenderer.hpp"
#include "VikShader.hpp"

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

class Cube {
public:

    uint32_t vertex_offset, colors_offset, normals_offset;

    void *map;

    struct ubo {
	glm::mat4 modelview;
	glm::mat4 modelviewprojection;
	float normal[12];
    };

    void
    init(VikRenderer* renderer)
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

	vkCreateDescriptorSetLayout(renderer->device,
	                            &layoutInfo,
	                            NULL,
	                            &set_layout);

	VkPipelineLayoutCreateInfo pipeLineInfo = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	    .setLayoutCount = 1,
	    .pSetLayouts = &set_layout
	};

	vkCreatePipelineLayout(renderer->device,
	                       &pipeLineInfo,
	                       NULL,
	                       &renderer->pipeline_layout);

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

	VkPipelineVertexInputStateCreateInfo vi_create_info = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	    .vertexBindingDescriptionCount = 3,
	    .pVertexBindingDescriptions = vertexBinding,
	    .vertexAttributeDescriptionCount = 3,
	    .pVertexAttributeDescriptions = vertexAttribute
	};

	VkPipelineShaderStageCreateInfo stagesInfo[] =  {
	    VikShader::load(renderer->device, "vkcube/vkcube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
	    VikShader::load(renderer->device, "vkcube/vkcube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
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

	VkDynamicState dynamicStates []  = {
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamicinfo =  {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
	    .dynamicStateCount = 2,
	    .pDynamicStates = dynamicStates,
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
	    .layout = renderer->pipeline_layout,
	    .renderPass = renderer->render_pass,
	    .subpass = 0,
	    .basePipelineHandle = basehandle,
	    .basePipelineIndex = 0
	};

	VkPipelineCache cache = { VK_NULL_HANDLE };

	vkCreateGraphicsPipelines(renderer->device,
	                          cache,
	                          1,
	                          &pipeLineCreateInfo,
	                          NULL,
	                          &renderer->pipeline);

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

	vertex_offset = sizeof(struct ubo);
	colors_offset = vertex_offset + sizeof(vVertices);
	normals_offset = colors_offset + sizeof(vColors);
	uint32_t mem_size = normals_offset + sizeof(vNormals);

	VkMemoryAllocateInfo allcoinfo = {
	    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
	    .allocationSize = mem_size,
	    .memoryTypeIndex = 0,
	};

	vkAllocateMemory(renderer->device,
	                 &allcoinfo,
	                 NULL,
	                 &renderer->mem);

	r = vkMapMemory(renderer->device, renderer->mem, 0, mem_size, 0, &map);
	if (r != VK_SUCCESS)
	    fail("vkMapMemory failed");


	memcpy(((char*)map + vertex_offset), vVertices, sizeof(vVertices));
	memcpy(((char*)map + colors_offset), vColors, sizeof(vColors));
	memcpy(((char*)map + normals_offset), vNormals, sizeof(vNormals));


	VkBufferCreateInfo bufferInfo = {
	    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
	    .size = mem_size,
	    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
	    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	    .flags = 0
	};

	vkCreateBuffer(renderer->device,
	               &bufferInfo,
	               NULL,
	               &renderer->buffer);

	vkBindBufferMemory(renderer->device, renderer->buffer, renderer->mem, 0);

	VkDescriptorPoolSize poolsizes[] = {
	    {
	        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .descriptorCount = 1
	    }      };

	VkDescriptorPool descriptorPool;
	const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
	    .pNext = NULL,
	    .flags = 0,
	    .maxSets = 1,
	    .poolSizeCount = 1,
	    .pPoolSizes = poolsizes,
	};

	vkCreateDescriptorPool(renderer->device, &descriptorPoolCreateInfo, NULL, &descriptorPool);

	VkDescriptorSetAllocateInfo descriptorAllocateInfo = {
	    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
	    .descriptorPool = descriptorPool,
	    .descriptorSetCount = 1,
	    .pSetLayouts = &set_layout,
	};

	vkAllocateDescriptorSets(renderer->device, &descriptorAllocateInfo, &renderer->descriptor_set);

	VkDescriptorBufferInfo descriptorBufferInfo = {
	    .buffer = renderer->buffer,
	    .offset = 0,
	    .range = sizeof(struct ubo),
	};


	VkWriteDescriptorSet writeDescriptorSet[] = {
	    {
	        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
	        .dstSet = renderer->descriptor_set,
	        .dstBinding = 0,
	        .dstArrayElement = 0,
	        .descriptorCount = 1,
	        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        .pBufferInfo = &descriptorBufferInfo
	    }
	};

	vkUpdateDescriptorSets(renderer->device, 1, writeDescriptorSet, 0, NULL);
    }

    void
    render(VikRenderer *vc, struct vkcube_buffer *b)
    {
	struct ubo ubo;
	struct timeval tv;
	uint64_t t;

	gettimeofday(&tv, NULL);

	t = ((tv.tv_sec * 1000 + tv.tv_usec / 1000) -
	     (vc->start_tv.tv_sec * 1000 + vc->start_tv.tv_usec / 1000)) / 5;

	glm::mat4 t_matrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -8.0f));

	glm::vec3 r_vec = glm::vec3(45.0f + (0.25f * t),
	                               45.0f - (0.5f * t),
	                               10.0f + (0.15f * t));

	glm::mat4  r_matrix = glm::mat4();
	r_matrix = glm::rotate(r_matrix, glm::radians(r_vec.x), glm::vec3(-1.0f, 0.0f, 0.0f));
	r_matrix = glm::rotate(r_matrix, glm::radians(r_vec.y), glm::vec3(0.0f, -1.0f, 0.0f));
	r_matrix = glm::rotate(r_matrix, glm::radians(r_vec.z), glm::vec3(0.0f, 0.0f, -1.0f));

	float aspect = (float) vc->height / (float) vc->width;

	ubo.modelview = t_matrix * r_matrix;

	glm::mat4 p_matrix = glm::frustum(-2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 3.5f, 10.0f);

	ubo.modelviewprojection = p_matrix * ubo.modelview;

	/* The mat3 normalMatrix is laid out as 3 vec4s. */
	memcpy(ubo.normal, &ubo.modelview, sizeof ubo.normal);

	memcpy(map, &ubo, sizeof(ubo));


	VkCommandBufferAllocateInfo cmdBufferAllocateInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
	    .commandPool = vc->cmd_pool,
	    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
	    .commandBufferCount = 1,
	};

	VkCommandBuffer cmd_buffer;
	vkAllocateCommandBuffers(vc->device,
	                         &cmdBufferAllocateInfo,
	                         &cmd_buffer);

	VkCommandBufferBeginInfo cmdBufferBeginInfo = {
	    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	    .flags = 0
	};

	vkBeginCommandBuffer(cmd_buffer, &cmdBufferBeginInfo);


	VkClearValue clearValues[] = {
	    { .color = { .float32 = { 0.2f, 0.2f, 0.2f, 1.0f } } }
	};

	VkRenderPassBeginInfo passBeginInfo = {
	    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
	    .renderPass = vc->render_pass,
	    .framebuffer = b->framebuffer,
	    .renderArea = { { 0, 0 }, { vc->width, vc->height } },
	    .clearValueCount = 1,
	    .pClearValues = clearValues
	};

	vkCmdBeginRenderPass(cmd_buffer,
	                     &passBeginInfo,
	                     VK_SUBPASS_CONTENTS_INLINE);

	VkBuffer buffers[] = {
	    vc->buffer,
	    vc->buffer,
	    vc->buffer
	};

	VkDeviceSize sizes[] = {
	    vertex_offset,
	    colors_offset,
	    normals_offset
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


	VkSubmitInfo submitInfo = {
	    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
	    .waitSemaphoreCount = 1,
	    .pWaitSemaphores = &vc->semaphore,
	    .pWaitDstStageMask = stageflags,
	    .commandBufferCount = 1,
	    .pCommandBuffers = &cmd_buffer,
	};

	vkQueueSubmit(vc->queue, 1, &submitInfo, vc->fence);

	VkFence fences[] = { vc->fence };

	vkWaitForFences(vc->device, 1, fences, true, INT64_MAX);
	vkResetFences(vc->device, 1, &vc->fence);

	vkResetCommandPool(vc->device, vc->cmd_pool, 0);
    }
};
