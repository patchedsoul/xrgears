/*
 * XRGears
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
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gli/gli.hpp>

#include <vector>

#include "system/vikApplication.hpp"
#include "render/vikModel.hpp"
#include "scene/vikNodeGear.hpp"
#include "scene/vikSkyBox.hpp"
#include "render/vikDistortion.hpp"
#include "render/vikOffscreenPass.hpp"
#include "scene/vikNodeModel.hpp"
#include "scene/vikCamera.hpp"
#include "input/vikHMD.hpp"
#include "scene/vikCameraStereo.hpp"
#include "scene/vikCameraHMD.hpp"
#include "system/vikLog.hpp"

#define VERTEX_BUFFER_BIND_ID 0

class XRGears : public vik::Application {
 public:
  // Vertex layout for the models
  vik::VertexLayout vertex_layout = vik::VertexLayout({
    vik::VERTEX_COMPONENT_POSITION,
    vik::VERTEX_COMPONENT_NORMAL
  });

  vik::HMD* hmd;

  bool enable_sky = true;
  bool enable_hmd_cam = true;
  bool enable_distortion = true;
  bool enable_stereo = true;

  vik::SkyBox *sky_box = nullptr;
  vik::Distortion *distortion = nullptr;
  vik::OffscreenPass *offscreen_pass = nullptr;

  struct {
    VkDescriptorSet object;
  } descriptor_sets;

  struct {
    VkPipelineVertexInputStateCreateInfo input_state;
    std::vector<VkVertexInputBindingDescription> binding_descriptions;
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
  } vertices;

  std::vector<vik::Node*> nodes;

  struct UBOLights {
    glm::vec4 lights[4];
  } ubo_lights;

  struct {
    vik::Buffer lights;
  } uniform_buffers;

  struct {
    VkPipeline pbr;
  } pipelines;

  VkPipelineLayout pipeline_layout;
  VkDescriptorSetLayout descriptor_set_layout;

  VkCommandBuffer offscreen_command_buffer = VK_NULL_HANDLE;
  // Semaphore used to synchronize between offscreen and final scene rendering
  VkSemaphore offscreen_semaphore = VK_NULL_HANDLE;

  XRGears(int argc, char *argv[]) : Application(argc, argv) {
    name = "XR Gears";

    renderer->timer.animation_timer_speed *= 0.25f;
  }

  ~XRGears() {
    if (offscreen_pass)
      delete offscreen_pass;

    vkDestroyPipeline(renderer->device, pipelines.pbr, nullptr);

    if (enable_sky)
      delete sky_box;

    vkDestroyPipelineLayout(renderer->device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(renderer->device, descriptor_set_layout, nullptr);

    if (distortion)
      delete distortion;

    uniform_buffers.lights.destroy();

    for (auto& node : nodes)
      delete(node);

    vkDestroySemaphore(renderer->device, offscreen_semaphore, nullptr);

    delete hmd;
  }

  // Enable physical device features required for this example
  virtual void enable_required_features() {
    check_feature(geometryShader);
    check_feature(multiViewport);
    check_feature(textureCompressionBC);
    check_feature(samplerAnisotropy);
  }

  void build_command_buffers() {
    if (enable_distortion) {
      for (uint32_t i = 0; i < renderer->cmd_buffers.size(); ++i)
        build_warp_command_buffer(renderer->cmd_buffers[i], renderer->frame_buffers[i]);
    } else {
      for (uint32_t i = 0; i < renderer->cmd_buffers.size(); ++i)
        build_pbr_command_buffer(renderer->cmd_buffers[i], renderer->frame_buffers[i], false);
    }
  }

  inline VkRenderPassBeginInfo default_render_pass_info() {
    VkRenderPassBeginInfo info = vik::initializers::renderPassBeginInfo();
    info.renderPass = renderer->render_pass;
    info.renderArea.offset.x = 0;
    info.renderArea.offset.y = 0;
    info.renderArea.extent.width = renderer->width;
    info.renderArea.extent.height = renderer->height;
    return info;
  }

  void build_warp_command_buffer(VkCommandBuffer command_buffer,
                                 VkFramebuffer framebuffer) {
    std::array<VkClearValue, 2> clear_values;
    clear_values[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
    clear_values[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo render_pass_begin_info = default_render_pass_info();
    render_pass_begin_info.clearValueCount = clear_values.size();
    render_pass_begin_info.pClearValues = clear_values.data();

    // Set target frame buffer
    render_pass_begin_info.framebuffer = framebuffer;

    VkCommandBufferBeginInfo command_bufffer_info = vik::initializers::commandBufferBeginInfo();
    vik_log_check(vkBeginCommandBuffer(command_buffer, &command_bufffer_info));

    vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = vik::initializers::viewport(
          (float) renderer->width, (float) renderer->height, 0.0f, 1.0f);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = vik::initializers::rect2D(
          renderer->width, renderer->height, 0, 0);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // Final composition as full screen quad
    distortion->draw_quad(command_buffer);

    vkCmdEndRenderPass(command_buffer);

    vik_log_check(vkEndCommandBuffer(command_buffer));
  }

  // Build command buffer for rendering the scene to the offscreen frame buffer attachments
  void build_offscreen_command_buffer() {
    if (offscreen_command_buffer == VK_NULL_HANDLE)
      offscreen_command_buffer = renderer->create_command_buffer();

    // Create a semaphore used to synchronize offscreen rendering and usage
    VkSemaphoreCreateInfo semaphore_info = vik::initializers::semaphoreCreateInfo();
    vik_log_check(vkCreateSemaphore(renderer->device, &semaphore_info,
                                    nullptr, &offscreen_semaphore));

    VkFramebuffer unused;
    build_pbr_command_buffer(offscreen_command_buffer, unused, true);
  }

  void build_pbr_command_buffer(const VkCommandBuffer& command_buffer,
                                const VkFramebuffer& framebuffer,
                                bool offscreen) {
    VkCommandBufferBeginInfo command_buffer_info = vik::initializers::commandBufferBeginInfo();
    vik_log_check(vkBeginCommandBuffer(command_buffer, &command_buffer_info));

    if (vik::debugmarker::active)
      vik::debugmarker::beginRegion(command_buffer,
                                    offscreen ? "Pbr offscreen" : "PBR Pass Onscreen",
                                    glm::vec4(0.3f, 0.94f, 1.0f, 1.0f));

    if (offscreen) {
      offscreen_pass->beginRenderPass(command_buffer);
      offscreen_pass->setViewPortAndScissorStereo(command_buffer);
    } else {
      std::array<VkClearValue, 2> clear_values;
      clear_values[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
      clear_values[1].depthStencil = { 1.0f, 0 };

      VkRenderPassBeginInfo render_pass_begin_info = default_render_pass_info();
      render_pass_begin_info.clearValueCount = clear_values.size();
      render_pass_begin_info.pClearValues = clear_values.data();
      render_pass_begin_info.framebuffer = framebuffer;

      vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

      if (enable_stereo)
        set_stereo_viewport_and_scissors(command_buffer);
      else
        set_mono_viewport_and_scissors(command_buffer);
    }

    draw_scene(command_buffer);

    vkCmdEndRenderPass(command_buffer);

    if (vik::debugmarker::active)
      vik::debugmarker::endRegion(command_buffer);

    vik_log_check(vkEndCommandBuffer(command_buffer));
  }

  void draw_scene(VkCommandBuffer command_buffer) {
    if (enable_sky)
      sky_box->draw(command_buffer, pipeline_layout);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);

    for (auto& node : nodes)
      node->draw(command_buffer, pipeline_layout);
  }

  void set_mono_viewport_and_scissors(VkCommandBuffer command_buffer) {
    VkViewport viewport = vik::initializers::viewport(
          (float)renderer->width,
          (float)renderer->height,
          0.0f, 1.0f);
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = vik::initializers::rect2D(
          renderer->width,
          renderer->height,
          0.0f, 0.0f);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
  }

  void set_stereo_viewport_and_scissors(VkCommandBuffer command_buffer) {
    VkViewport viewports[2];
    // Left
    viewports[0] = {
      0.0f,
      0.0f,
      (float)renderer->width / 2.0f,
      (float)renderer->height,
      0.0f,
      1.0f };
    // Right
    viewports[1] = {
      (float)renderer->width / 2.0f,
      0.0f,
      (float)renderer->width / 2.0f,
      (float)renderer->height,
      0.0f,
      1.0f };

    vkCmdSetViewport(command_buffer, 0, 2, viewports);

    VkRect2D scissor_rects[2] = {
      vik::initializers::rect2D(renderer->width/2, renderer->height, 0, 0),
      vik::initializers::rect2D(renderer->width/2, renderer->height, renderer->width / 2, 0),
    };
    vkCmdSetScissor(command_buffer, 0, 2, scissor_rects);
  }

  void load_assets() {
    if (enable_sky) {

      std::string file_name;
      VkFormat format;

      //file_name = "hdr/uffizi_cube.ktx";
      //format = VK_FORMAT_R16G16B16A16_SFLOAT;

      file_name = "cubemap_yokohama_bc3_unorm.ktx";
      format = VK_FORMAT_BC2_UNORM_BLOCK;

      // file_name = "equirect/cube2/cube.ktx";
      // format = VK_FORMAT_R16G16B16A16_SFLOAT;

      // file_name = "cubemap_space.ktx";
      // format = VK_FORMAT_R8G8B8A8_UNORM;

      sky_box->load_assets(vertex_layout, renderer->vik_device, renderer->queue,
                           vik::Assets::get_texture_path() + file_name, format);
    }
  }

  void init_gears() {
    // Gear definitions
    std::vector<float> inner_radiuses = { 1.0f, 0.5f, 1.3f };
    std::vector<float> outer_radiuses = { 4.0f, 2.0f, 2.0f };
    std::vector<float> widths = { 1.0f, 2.0f, 0.5f };
    std::vector<int32_t> tooth_count = { 20, 10, 10 };
    std::vector<float> tooth_depth = { 0.7f, 0.7f, 0.7f };
    std::vector<glm::vec3> positions = {
      glm::vec3(-3.0, 0.0, 0.0),
      glm::vec3(3.1, 0.0, 0.0),
      glm::vec3(-3.1, -6.2, 0.0)
    };

    std::vector<vik::Material> materials = {
      vik::Material("Red", glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, 0.9f),
      vik::Material("Green", glm::vec3(0.0f, 1.0f, 0.2f), 0.5f, 0.1f),
      vik::Material("Blue", glm::vec3(0.0f, 0.0f, 1.0f), 0.5f, 0.5f)
    };

    std::vector<float> rotation_speeds = { 1.0f, -2.0f, -2.0f };
    std::vector<float> rotation_offsets = { 0.0f, -9.0f, -30.0f };

    nodes.resize(positions.size());
    for (uint32_t i = 0; i < nodes.size(); ++i) {
      vik::Node::NodeInfo gear_node_info = {};
      vik::GearInfo gear_info = {};
      gear_info.inner_radius = inner_radiuses[i];
      gear_info.outer_radius = outer_radiuses[i];
      gear_info.width = widths[i];
      gear_info.tooth_count = tooth_count[i];
      gear_info.tooth_depth = tooth_depth[i];
      gear_node_info.position = positions[i];
      gear_node_info.rotation_speed = rotation_speeds[i];
      gear_node_info.rotation_offset = rotation_offsets[i];
      gear_node_info.material = materials[i];

      nodes[i] = new vik::NodeGear();
      nodes[i]->setInfo(&gear_node_info);
      ((vik::NodeGear*)nodes[i])->generate(renderer->vik_device,
                                           &gear_info, renderer->queue);
    }

    vik::NodeModel* teapot_node = new vik::NodeModel();
    teapot_node->load_model("teapot.dae",
                          vertex_layout,
                          0.25f,
                          renderer->vik_device,
                          renderer->queue);

    vik::Material teapot_material = vik::Material("Cream", glm::vec3(1.0f, 1.0f, 0.7f), 1.0f, 1.0f);
    teapot_node->setMateral(teapot_material);
    nodes.push_back(teapot_node);

    glm::vec3 teapot_position = glm::vec3(-15.0, -5.0, -5.0);
    teapot_node->setPosition(teapot_position);
  }

  void prepare_vertices() {
    // Binding and attribute descriptions are shared across all gears
    vertices.binding_descriptions.resize(1);
    vertices.binding_descriptions[0] =
        vik::initializers::vertexInputBindingDescription(
          VERTEX_BUFFER_BIND_ID,
          sizeof(vik::Vertex),
          VK_VERTEX_INPUT_RATE_VERTEX);

    // Attribute descriptions
    // Describes memory layout and shader positions
    vertices.attribute_descriptions.resize(3);
    // Location 0 : Position
    vertices.attribute_descriptions[0] =
        vik::initializers::vertexInputAttributeDescription(
          VERTEX_BUFFER_BIND_ID,
          0,
          VK_FORMAT_R32G32B32_SFLOAT,
          0);
    // Location 1 : Normal
    vertices.attribute_descriptions[1] =
        vik::initializers::vertexInputAttributeDescription(
          VERTEX_BUFFER_BIND_ID,
          1,
          VK_FORMAT_R32G32B32_SFLOAT,
          sizeof(float) * 3);
    // Location 2 : Color
    vertices.attribute_descriptions[2] =
        vik::initializers::vertexInputAttributeDescription(
          VERTEX_BUFFER_BIND_ID,
          2,
          VK_FORMAT_R32G32B32_SFLOAT,
          sizeof(float) * 6);

    vertices.input_state = vik::initializers::pipelineVertexInputStateCreateInfo();
    vertices.input_state.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.binding_descriptions.size());
    vertices.input_state.pVertexBindingDescriptions = vertices.binding_descriptions.data();
    vertices.input_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attribute_descriptions.size());
    vertices.input_state.pVertexAttributeDescriptions = vertices.attribute_descriptions.data();
  }

  void init_descriptor_pool() {
    // Example uses two ubos
    std::vector<VkDescriptorPoolSize> pool_sizes = {
      vik::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16),
      vik::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6)
    };

    VkDescriptorPoolCreateInfo descriptor_pool_info =
        vik::initializers::descriptorPoolCreateInfo(
          static_cast<uint32_t>(pool_sizes.size()),
          pool_sizes.data(),
          6);

    vik_log_check(vkCreateDescriptorPool(renderer->device,
                                         &descriptor_pool_info,
                                         nullptr, &renderer->descriptor_pool));
  }

  void init_descriptor_set_layout() {
    std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
      // ubo model
      vik::initializers::descriptorSetLayoutBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_GEOMETRY_BIT,  // VK_SHADER_STAGE_VERTEX_BIT,
      0),
      // ubo lights
      vik::initializers::descriptorSetLayoutBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      1),
      // ubo camera
      vik::initializers::descriptorSetLayoutBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      2)
    };

    // cube map sampler
    if (enable_sky)
      set_layout_bindings.push_back(vik::initializers::descriptorSetLayoutBinding(
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    3));

    VkDescriptorSetLayoutCreateInfo descriptor_layout =
        vik::initializers::descriptorSetLayoutCreateInfo(set_layout_bindings);

    vik_log_check(vkCreateDescriptorSetLayout(renderer->device,
                                              &descriptor_layout,
                                              nullptr, &descriptor_set_layout));

    VkPipelineLayoutCreateInfo pipeline_layout_info =
        vik::initializers::pipelineLayoutCreateInfo(&descriptor_set_layout, 1);

    /*
     * Push Constants
     */

    std::vector<VkPushConstantRange> push_constant_ranges = {
      vik::initializers::pushConstantRange(
        VK_SHADER_STAGE_FRAGMENT_BIT,
        sizeof(vik::Material::PushBlock),
        sizeof(glm::vec3)),
    };

    pipeline_layout_info.pushConstantRangeCount = push_constant_ranges.size();
    pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();

    vik_log_check(vkCreatePipelineLayout(renderer->device,
                                         &pipeline_layout_info,
                                         nullptr, &pipeline_layout));
  }

  void init_descriptor_set() {
    if (enable_sky) {
      VkDescriptorSetAllocateInfo info =
          vik::initializers::descriptorSetAllocateInfo(
            renderer->descriptor_pool,
            &descriptor_set_layout,
            1);

      sky_box->create_descriptor_set(info, &camera->uniform_buffer.descriptor);
    }

    for (auto& node : nodes)
      node->create_descriptor_set(renderer->device, renderer->descriptor_pool,
                                descriptor_set_layout,
                                &uniform_buffers.lights.descriptor,
                                &camera->uniform_buffer.descriptor,
                                sky_box);
}

  void init_pipelines() {
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
        vik::initializers::pipelineInputAssemblyStateCreateInfo(
          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
          0,
          VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterization_state =
        vik::initializers::pipelineRasterizationStateCreateInfo(
          VK_POLYGON_MODE_FILL,
          VK_CULL_MODE_BACK_BIT,
          VK_FRONT_FACE_CLOCKWISE);

    VkPipelineColorBlendAttachmentState blend_attachment_state =
        vik::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);

    VkPipelineColorBlendStateCreateInfo color_blend_state =
        vik::initializers::pipelineColorBlendStateCreateInfo(1, &blend_attachment_state);

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
        vik::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineViewportStateCreateInfo viewport_state;
    if (enable_stereo)
      viewport_state = vik::initializers::pipelineViewportStateCreateInfo(2, 2, 0);
    else
      viewport_state = vik::initializers::pipelineViewportStateCreateInfo(1, 1, 0);


    VkPipelineMultisampleStateCreateInfo multisample_state =
        vik::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

    std::vector<VkDynamicState> dynamic_state_enables = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_LINE_WIDTH
    };
    VkPipelineDynamicStateCreateInfo dynamicState =
        vik::initializers::pipelineDynamicStateCreateInfo(dynamic_state_enables);

    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 3> shader_stages;
    shader_stages[0] = vik::Shader::load(renderer->device, "xrgears/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);

    if (enable_sky)
      shader_stages[1] = vik::Shader::load(renderer->device, "xrgears/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    else
      shader_stages[1] = vik::Shader::load(renderer->device, "xrgears/scene_no_sky.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    shader_stages[2] = vik::Shader::load(renderer->device, "xrgears/multiview.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    VkGraphicsPipelineCreateInfo pipeline_info;

    VkRenderPass used_pass;
    if (enable_distortion)
      used_pass = offscreen_pass->getRenderPass();
    else
      used_pass = renderer->render_pass;

    pipeline_info = vik::initializers::pipelineCreateInfo(pipeline_layout, used_pass);

    // Vertex bindings an attributes
    std::vector<VkVertexInputBindingDescription> vertex_input_bindings = {
      vik::initializers::vertexInputBindingDescription(0, vertex_layout.stride(), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    std::vector<VkVertexInputAttributeDescription> vertex_input_attributes = {
      // Location 0: Position
      vik::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),
      // Location 1: Normals
      vik::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),
      // Location 2: Color
      vik::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 6),
    };
    VkPipelineVertexInputStateCreateInfo vertex_input_state = vik::initializers::pipelineVertexInputStateCreateInfo();
    vertex_input_state.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_input_bindings.size());
    vertex_input_state.pVertexBindingDescriptions = vertex_input_bindings.data();
    vertex_input_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_input_attributes.size());
    vertex_input_state.pVertexAttributeDescriptions = vertex_input_attributes.data();

    pipeline_info.pVertexInputState = &vertex_input_state;
    pipeline_info.pInputAssemblyState = &input_assembly_state;
    pipeline_info.pRasterizationState = &rasterization_state;
    pipeline_info.pColorBlendState = &color_blend_state;
    pipeline_info.pMultisampleState = &multisample_state;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pDepthStencilState = &depth_stencil_state;
    pipeline_info.pDynamicState = &dynamicState;
    pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.renderPass = used_pass;

    vik_log_check(vkCreateGraphicsPipelines(renderer->device,
                                            renderer->pipeline_cache, 1,
                                            &pipeline_info,
                                            nullptr, &pipelines.pbr));

    if (enable_sky)
      sky_box->init_pipeline(&pipeline_info, renderer->pipeline_cache);

    vkDestroyShaderModule(renderer->device, shader_stages[0].module, nullptr);
    vkDestroyShaderModule(renderer->device, shader_stages[1].module, nullptr);
    vkDestroyShaderModule(renderer->device, shader_stages[2].module, nullptr);
  }

  // Prepare and initialize uniform buffer containing shader uniforms
  void init_uniform_buffers() {
    renderer->vik_device->create_and_map(&uniform_buffers.lights, sizeof(ubo_lights));

    camera->init_uniform_buffer(renderer->vik_device);

    for (auto& node : nodes)
      node->init_uniform_buffer(renderer->vik_device);

    update_uniform_buffers();
  }

  void update_uniform_buffers() {
    camera->update_uniform_buffer();

    vik::Camera::StereoView sv = {};
    sv.view[0] = camera->ubo.view[0];
    sv.view[1] = camera->ubo.view[1];

    for (auto& node : nodes)
      node->update_uniform_buffer(sv, renderer->timer.animation_timer);

    update_lights();
  }

  void update_lights() {
    const float p = 15.0f;
    ubo_lights.lights[0] = glm::vec4(-p, -p*0.5f, -p, 1.0f);
    ubo_lights.lights[1] = glm::vec4(-p, -p*0.5f,  p, 1.0f);
    ubo_lights.lights[2] = glm::vec4( p, -p*0.5f,  p, 1.0f);
    ubo_lights.lights[3] = glm::vec4( p, -p*0.5f, -p, 1.0f);

    if (!renderer->timer.animation_paused) {

      float rad = glm::radians(renderer->timer.animation_timer * 360.0f);

      ubo_lights.lights[0].x = sin(rad) * 20.0f;
      ubo_lights.lights[0].z = cos(rad) * 20.0f;
      ubo_lights.lights[1].x = cos(rad) * 20.0f;
      ubo_lights.lights[1].y = sin(rad) * 20.0f;
    }

    memcpy(uniform_buffers.lights.mapped, &ubo_lights, sizeof(ubo_lights));
  }

  void draw() {
    VkSubmitInfo submit_info = renderer->init_render_submit_info();

    std::array<VkPipelineStageFlags, 1> stage_flags = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submit_info.pWaitDstStageMask = stage_flags.data();

    if (enable_distortion) {
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
      submit_info.pWaitSemaphores = &renderer->semaphores.present_complete;
      // Signal ready with offscreen semaphore
      submit_info.pSignalSemaphores = &offscreen_semaphore;

      // Submit work
      submit_info.pCommandBuffers = &offscreen_command_buffer;
      vik_log_check(vkQueueSubmit(renderer->queue, 1, &submit_info, VK_NULL_HANDLE));

      // Scene rendering

      // Wait for offscreen semaphore
      submit_info.pWaitSemaphores = &offscreen_semaphore;
      // Signal ready with render complete semaphpre
      submit_info.pSignalSemaphores = &renderer->semaphores.render_complete;
    }

    // Submit to queue
    submit_info.pCommandBuffers = renderer->get_current_command_buffer();
    vik_log_check(vkQueueSubmit(renderer->queue, 1, &submit_info, VK_NULL_HANDLE));
  }

  void init() {
    Application::init();

    hmd = new vik::HMD(&settings);

    if (enable_stereo) {
      if (enable_hmd_cam)
        camera = new vik::CameraHMD(hmd);
      else
        camera = new vik::CameraStereo(renderer->width, renderer->height);
    } else {
      camera = new vik::CameraFirstPerson();
    }

    camera->set_rotation(glm::vec3(-4.f, 23.f, 0));
    camera->set_position(glm::vec3(6.2f, 4.f, -15.2f));
    camera->set_perspective(60.0f, (float)renderer->width / (float)renderer->height, 0.1f, 256.0f);
    camera->movement_speed = 5.0f;

    camera->set_view_updated_cb([this]() { view_updated = true; });

    if (enable_sky)
      sky_box = new vik::SkyBox(renderer->device);


    load_assets();
    init_gears();
    prepare_vertices();
    init_uniform_buffers();
    init_descriptor_pool();
    init_descriptor_set_layout();

    if (enable_distortion) {
      offscreen_pass = new vik::OffscreenPass(renderer->device);
      offscreen_pass->init_offscreen_framebuffer(renderer->vik_device, renderer->physical_device);
      distortion = new vik::Distortion(renderer->device);
      distortion->init_quads(renderer->vik_device);
      distortion->init_uniform_buffer(renderer->vik_device);
      distortion->update_uniform_buffer_warp(hmd->device);
      distortion->init_descriptor_set_layout();
      distortion->init_pipeline_layout();
      distortion->init_pipeLine(renderer->render_pass, renderer->pipeline_cache);
      distortion->init_descriptor_set(offscreen_pass, renderer->descriptor_pool);
    }

    init_pipelines();
    init_descriptor_set();
    build_command_buffers();

    if (enable_distortion)
      build_offscreen_command_buffer();
  }

  virtual void render() {
    vkDeviceWaitIdle(renderer->device);
    draw();
    vkDeviceWaitIdle(renderer->device);
    if (!renderer->timer.animation_paused)
      update_uniform_buffers();
  }

  virtual void view_changed_cb() {
    update_uniform_buffers();
  }

  /*
  void change_eye_separation(float delta) {
    if (!enable_hmd_cam)
      ((vik::CameraStereo*)camera)->changeEyeSeparation(delta);
    update_uniform_buffers();
  }
  */

  virtual void key_pressed(vik::Input::Key keyCode) {
    /*
    switch (keyCode) {
      case KEY_KPADD:
        change_eye_separation(0.005);
        break;
      case KEY_KPSUB:
        change_eye_separation(-0.005);
        break;
    }
    */
  }
};

int main(int argc, char *argv[]) {
  XRGears app(argc, argv);
  app.init();
  app.loop();
  return 0;
}
