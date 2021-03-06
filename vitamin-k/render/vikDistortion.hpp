 /*
 * vitamin-k
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <vulkan/vulkan.h>
#include <openhmd.h>

#include <vector>
#include <array>

#include "vikModel.hpp"
#include "../system/vikLog.hpp"

#include "vikOffscreenPass.hpp"
#include "vikShader.hpp"

#include "../system/vikSettings.hpp"

#define VERTEX_BUFFER_BIND_ID 0

namespace vik {
class Distortion {
 private:
  VkDevice device;
  Model quad;
  Buffer ubo_handle;

  struct {
    glm::vec4 hmd_warp_param;
    glm::vec4 aberr;
    glm::vec4 lens_center[2];
    glm::vec2 viewport_scale;
    float warp_scale;
  } ubo_data;

  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorSet descriptor_set;

 public:
  explicit Distortion(const VkDevice& d) {
    device = d;
  }

  ~Distortion() {
    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);

    quad.destroy();
    ubo_handle.destroy();

    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
  }

  void init_pipeline(const VkRenderPass& render_pass,
                     const VkPipelineCache& pipeline_cache,
                     Settings::DistortionType distortion_type) {
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineRasterizationStateCreateInfo rasterization_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .lineWidth = 1.0f
    };

    VkPipelineColorBlendAttachmentState blend_attachment_state = {
      .blendEnable = VK_FALSE,
      .colorWriteMask = 0xf
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &blend_attachment_state
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .front = {
        .compareOp = VK_COMPARE_OP_ALWAYS
      },
      .back = {
        .compareOp = VK_COMPARE_OP_ALWAYS
      }
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1
    };

    VkPipelineMultisampleStateCreateInfo multisample_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    std::vector<VkDynamicState> dynamic_state_enables = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamic_state_enables.size()),
      .pDynamicStates = dynamic_state_enables.data()
    };

    std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages;

    VkPipelineVertexInputStateCreateInfo empty_input_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .flags = 0,
      .stageCount = static_cast<uint32_t>(shader_stages.size()),
      .pStages = shader_stages.data(),
      .pVertexInputState = &empty_input_state,
      .pInputAssemblyState = &input_assembly_state,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterization_state,
      .pMultisampleState = &multisample_state,
      .pDepthStencilState = &depth_stencil_state,
      .pColorBlendState = &color_blend_state,
      .pDynamicState = &dynamic_state,
      .layout = pipeline_layout,
      .renderPass = render_pass,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1
    };

    // Final fullscreen composition pass pipeline
    shader_stages[0] =
        vik::Shader::load(device,
                          "distortion/distortion.vert.spv",
                          VK_SHADER_STAGE_VERTEX_BIT);

    std::string fragment_shader_name;

    switch (distortion_type) {
      case Settings::DistortionType::DISTORTION_TYPE_PANOTOOLS:
        fragment_shader_name = "distortion/panotools.frag.spv";
        break;
      case Settings::DistortionType::DISTORTION_TYPE_VIVE:
        fragment_shader_name = "distortion/vive.frag.spv";
        break;
      default:
        fragment_shader_name = "distortion/panotools.frag.spv";
    }

    shader_stages[1] = vik::Shader::load(device,
                                         fragment_shader_name,
                                         VK_SHADER_STAGE_FRAGMENT_BIT);

    vik_log_check(vkCreateGraphicsPipelines(device, pipeline_cache, 1,
                                            &pipeline_info, nullptr,
                                            &pipeline));

    vkDestroyShaderModule(device, shader_stages[0].module, nullptr);
    vkDestroyShaderModule(device, shader_stages[1].module, nullptr);
  }

  VkWriteDescriptorSet get_uniform_write_descriptor_set(uint32_t binding) {
    return (VkWriteDescriptorSet) {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptor_set,
      .dstBinding = binding,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pBufferInfo = &ubo_handle.descriptor
    };
  }

  void init_descriptor_set(OffscreenPass *offscreenPass,
                           const VkDescriptorPool& descriptorPool) {
    std::vector<VkWriteDescriptorSet> write_descriptor_sets;

    // Textured quad descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &descriptor_set_layout
    };

    vik_log_check(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set));

    VkDescriptorImageInfo offscreen_image_info =
        offscreenPass->get_descriptor_image_info();

    write_descriptor_sets = {
      // Binding 0 : Render texture target
      offscreenPass->get_image_write_descriptor_set(descriptor_set, &offscreen_image_info, 0),
      // Binding 1 : Fragment shader uniform buffer
      get_uniform_write_descriptor_set(1)
    };

    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(write_descriptor_sets.size()),
                           write_descriptor_sets.data(), 0, nullptr);
  }

  void init_descriptor_set_layout() {
    std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
      // Binding 0 : Render texture target
      {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
      },
      // Binding 1 : Fragment shader uniform buffer
      {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
      }
    };

    VkDescriptorSetLayoutCreateInfo set_layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(set_layout_bindings.size()),
      .pBindings = set_layout_bindings.data()
    };

    vik_log_check(vkCreateDescriptorSetLayout(device, &set_layout_info, nullptr,
                                              &descriptor_set_layout));
  }

  void init_pipeline_layout() {
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set_layout
    };
    vik_log_check(vkCreatePipelineLayout(device, &pipeline_layout_info,
                                         nullptr, &pipeline_layout));
  }

  void draw_quad(const VkCommandBuffer& command_buffer) {
    vkCmdBindDescriptorSets(command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, 0, 1,
                            &descriptor_set, 0, nullptr);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    // TODO(lubosz): Use vertex buffer
    /*
    vkCmdBindVertexBuffers(commandBuffer, VERTEX_BUFFER_BIND_ID, 1, &quad.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, quad.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 1);
    */
    vkCmdDraw(command_buffer, 12, 1, 0, 0);
  }

  void init_quads(Device *vik_device) {
    // Setup vertices for multiple screen aligned quads
    // Used for displaying final result and debug
    struct Vertex {
      float pos[3];
      float uv[2];
    };

    std::vector<Vertex> vertex_buffer;
    // Last component of normal is used for debug display sampler index
    vertex_buffer.push_back({ { 1.f, .5f, 0.f }, { 1.f, .5f } });
    vertex_buffer.push_back({ { 0.f, .5f, 0.f }, { 0.f, .5f } });
    vertex_buffer.push_back({ { 0.f, 0.f, 0.f }, { 0.f, 0.f } });
    vertex_buffer.push_back({ { 1.f, 0.f, 0.f }, { 1.f, 0.f } });


    vik_log_check(vik_device->createBuffer(
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    vertex_buffer.size() * sizeof(Vertex),
                    &quad.vertices.buffer,
                    &quad.vertices.memory,
                    vertex_buffer.data()));

    // Setup indices
    std::vector<uint32_t> index_buffer = { 0, 1, 2, 2, 3, 0 };

    quad.indexCount = static_cast<uint32_t>(index_buffer.size());

    vik_log_check(vik_device->createBuffer(
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    index_buffer.size() * sizeof(uint32_t),
                    &quad.indices.buffer,
                    &quad.indices.memory,
                    index_buffer.data()));

    quad.device = device;
  }

  // Update fragment shader hmd warp uniform block
  void update_uniform_buffer_warp(ohmd_device* hmd_device) {
    float viewport_scale[2];
    float distortion_coeffs[4];
    float aberr_scale[4];
    float sep;
    float left_lens_center[4];
    float right_lens_center[4];
    // viewport is half the screen
    ohmd_device_getf(hmd_device, OHMD_SCREEN_HORIZONTAL_SIZE, &(viewport_scale[0]));
    viewport_scale[0] /= 2.0f;
    ohmd_device_getf(hmd_device, OHMD_SCREEN_VERTICAL_SIZE, &(viewport_scale[1]));
    // distortion coefficients
    ohmd_device_getf(hmd_device, OHMD_UNIVERSAL_DISTORTION_K, &(distortion_coeffs[0]));
    ohmd_device_getf(hmd_device, OHMD_UNIVERSAL_ABERRATION_K, &(aberr_scale[0]));
    aberr_scale[3] = 0;

    // calculate lens centers (assuming the eye separation is the distance betweenteh lense centers)
    ohmd_device_getf(hmd_device, OHMD_LENS_HORIZONTAL_SEPARATION, &sep);
    ohmd_device_getf(hmd_device, OHMD_LENS_VERTICAL_POSITION, &(left_lens_center[1]));
    ohmd_device_getf(hmd_device, OHMD_LENS_VERTICAL_POSITION, &(right_lens_center[1]));
    left_lens_center[0] = viewport_scale[0] - sep/2.0f;
    right_lens_center[0] = sep/2.0f;
    // asume calibration was for lens view to which ever edge of screen is further away from lens center

    ubo_data.lens_center[0] = glm::make_vec4(left_lens_center);
    ubo_data.lens_center[1] = glm::make_vec4(right_lens_center);

    ubo_data.viewport_scale = glm::make_vec2(viewport_scale);

    ubo_data.warp_scale = (left_lens_center[0] > right_lens_center[0]) ? left_lens_center[0] : right_lens_center[0];

    ubo_data.hmd_warp_param = glm::make_vec4(distortion_coeffs);
    ubo_data.aberr = glm::make_vec4(aberr_scale);

    memcpy(ubo_handle.mapped, &ubo_data, sizeof(ubo_data));
  }

  void init_uniform_buffer(Device *vik_device) {
    // Warp UBO in deferred fragment shader
    vik_log_check(vik_device->createBuffer(
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &ubo_handle,
                    sizeof(ubo_data)));
    vik_log_check(ubo_handle.map());
  }
};
}  // namespace vik
