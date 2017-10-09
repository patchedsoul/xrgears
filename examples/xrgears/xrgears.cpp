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
  vik::VertexLayout vertexLayout = vik::VertexLayout({
    vik::VERTEX_COMPONENT_POSITION,
    vik::VERTEX_COMPONENT_NORMAL
  });

  vik::HMD* hmd;

  bool enableSky = true;
  bool enableHMDCam = false;
  bool enableDistortion = true;
  bool enableStereo = true;

  vik::SkyBox *skyBox;
  vik::Distortion *hmdDistortion;
  vik::OffscreenPass *offscreenPass;

  struct {
    VkDescriptorSet object;
  } descriptorSets;

  struct {
    VkPipelineVertexInputStateCreateInfo inputState;
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
  } vertices;

  std::vector<vik::Node*> nodes;

  struct UBOLights {
    glm::vec4 lights[4];
  } uboLights;

  struct {
    vik::Buffer lights;
  } uniformBuffers;

  struct {
    VkPipeline pbr;
  } pipelines;

  VkPipelineLayout pipelineLayout;
  VkDescriptorSetLayout descriptorSetLayout;

  VkCommandBuffer offScreenCmdBuffer = VK_NULL_HANDLE;
  // Semaphore used to synchronize between offscreen and final scene rendering
  VkSemaphore offscreenSemaphore = VK_NULL_HANDLE;

  XRGears(int argc, char *argv[]) : Application(argc, argv) {
    name = "XR Gears";

    renderer->timer.animation_timer_speed *= 0.25f;
  }

  ~XRGears() {
    delete offscreenPass;

    vkDestroyPipeline(renderer->device, pipelines.pbr, nullptr);

    if (enableSky)
      delete skyBox;

    vkDestroyPipelineLayout(renderer->device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(renderer->device, descriptorSetLayout, nullptr);

    delete hmdDistortion;

    // uniformBuffers.camera.destroy();
    uniformBuffers.lights.destroy();

    for (auto& node : nodes)
      delete(node);

    vkDestroySemaphore(renderer->device, offscreenSemaphore, nullptr);

    delete hmd;
  }

  // Enable physical device features required for this example
  virtual void get_enabled_features() {
    // Geometry shader support is required for this example
    if (renderer->deviceFeatures.geometryShader)
      renderer->enabledFeatures.geometryShader = VK_TRUE;
    else
      vik_log_f("Feature not supported: Selected GPU does not support geometry shaders!");

    // Multiple viewports must be supported
    if (renderer->deviceFeatures.multiViewport)
      renderer->enabledFeatures.multiViewport = VK_TRUE;
    else
      vik_log_f("Feature not supported: Selected GPU does not support multi viewports!");
  }

  void build_command_buffers() {
    vik_log_d("Draw command buffers size: %ld", renderer->cmd_buffers.size());

    if (enableDistortion) {
      for (uint32_t i = 0; i < renderer->cmd_buffers.size(); ++i)
        buildWarpCommandBuffer(renderer->cmd_buffers[i], renderer->frame_buffers[i]);
    } else {
      for (uint32_t i = 0; i < renderer->cmd_buffers.size(); ++i)
        buildPbrCommandBuffer(renderer->cmd_buffers[i], renderer->frame_buffers[i], false);
    }
  }

  inline VkRenderPassBeginInfo defaultRenderPassBeginInfo() {
    VkRenderPassBeginInfo renderPassBeginInfo = vik::initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderer->render_pass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = renderer->width;
    renderPassBeginInfo.renderArea.extent.height = renderer->height;
    return renderPassBeginInfo;
  }

  void buildWarpCommandBuffer(VkCommandBuffer cmdBuffer, VkFramebuffer frameBuffer) {
    std::array<VkClearValue, 2> clearValues;
    clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = defaultRenderPassBeginInfo();
    renderPassBeginInfo.clearValueCount = clearValues.size();
    renderPassBeginInfo.pClearValues = clearValues.data();

    // Set target frame buffer
    renderPassBeginInfo.framebuffer = frameBuffer;

    VkCommandBufferBeginInfo cmdBufInfo = vik::initializers::commandBufferBeginInfo();
    vik_log_check(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = vik::initializers::viewport(
          (float) renderer->width, (float) renderer->height,
          0.0f, 1.0f);
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor = vik::initializers::rect2D(renderer->width, renderer->height, 0, 0);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    // Final composition as full screen quad
    hmdDistortion->drawQuad(cmdBuffer);

    vkCmdEndRenderPass(cmdBuffer);

    vik_log_check(vkEndCommandBuffer(cmdBuffer));
  }

  // Build command buffer for rendering the scene to the offscreen frame buffer attachments
  void buildOffscreenCommandBuffer() {
    if (offScreenCmdBuffer == VK_NULL_HANDLE)
      offScreenCmdBuffer = renderer->create_command_buffer();

    // Create a semaphore used to synchronize offscreen rendering and usage
    VkSemaphoreCreateInfo semaphoreCreateInfo = vik::initializers::semaphoreCreateInfo();
    vik_log_check(vkCreateSemaphore(renderer->device, &semaphoreCreateInfo, nullptr, &offscreenSemaphore));

    VkFramebuffer unused;

    buildPbrCommandBuffer(offScreenCmdBuffer, unused, true);
  }

  void buildPbrCommandBuffer(const VkCommandBuffer& cmdBuffer,
                             const VkFramebuffer& framebuffer, bool offScreen) {
    VkCommandBufferBeginInfo cmdBufInfo = vik::initializers::commandBufferBeginInfo();
    vik_log_check(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

    if (vik::debugmarker::active)
      vik::debugmarker::beginRegion(cmdBuffer,
                                    offScreen ? "Pbr offscreen" : "PBR Pass Onscreen",
                                    glm::vec4(0.3f, 0.94f, 1.0f, 1.0f));

    if (offScreen) {
      offscreenPass->beginRenderPass(cmdBuffer);
      offscreenPass->setViewPortAndScissorStereo(cmdBuffer);
    } else {
      std::array<VkClearValue, 2> clearValues;
      clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
      clearValues[1].depthStencil = { 1.0f, 0 };

      VkRenderPassBeginInfo renderPassBeginInfo = defaultRenderPassBeginInfo();
      renderPassBeginInfo.clearValueCount = clearValues.size();
      renderPassBeginInfo.pClearValues = clearValues.data();
      renderPassBeginInfo.framebuffer = framebuffer;

      vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      if (enableStereo)
        setStereoViewPortAndScissors(cmdBuffer);
      else
        setMonoViewPortAndScissors(cmdBuffer);
    }

    drawScene(cmdBuffer);

    vkCmdEndRenderPass(cmdBuffer);

    if (vik::debugmarker::active)
      vik::debugmarker::endRegion(cmdBuffer);

    vik_log_check(vkEndCommandBuffer(cmdBuffer));
  }

  void drawScene(VkCommandBuffer cmdBuffer) {
    if (enableSky)
      skyBox->draw(cmdBuffer, pipelineLayout);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);

    for (auto& node : nodes)
      node->draw(cmdBuffer, pipelineLayout);
  }

  void setMonoViewPortAndScissors(VkCommandBuffer cmdBuffer) {
    VkViewport viewport = vik::initializers::viewport((float)renderer->width, (float)renderer->height, 0.0f, 1.0f);
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor = vik::initializers::rect2D(renderer->width, renderer->height, 0, 0);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
  }

  void setStereoViewPortAndScissors(VkCommandBuffer cmdBuffer) {
    VkViewport viewports[2];
    // Left
    viewports[0] = { 0, 0, (float)renderer->width / 2.0f, (float)renderer->height, 0.0, 1.0f };
    // Right
    viewports[1] = { (float)renderer->width / 2.0f, 0, (float)renderer->width / 2.0f, (float)renderer->height, 0.0, 1.0f };

    vkCmdSetViewport(cmdBuffer, 0, 2, viewports);

    VkRect2D scissorRects[2] = {
      vik::initializers::rect2D(renderer->width/2, renderer->height, 0, 0),
      vik::initializers::rect2D(renderer->width/2, renderer->height, renderer->width / 2, 0),
    };
    vkCmdSetScissor(cmdBuffer, 0, 2, scissorRects);
  }

  void loadAssets() {
    if (enableSky)
      skyBox->loadAssets(vertexLayout, renderer->vksDevice, renderer->queue);
  }

  void initGears() {
    // Gear definitions
    std::vector<float> innerRadiuses = { 1.0f, 0.5f, 1.3f };
    std::vector<float> outerRadiuses = { 4.0f, 2.0f, 2.0f };
    std::vector<float> widths = { 1.0f, 2.0f, 0.5f };
    std::vector<int32_t> toothCount = { 20, 10, 10 };
    std::vector<float> toothDepth = { 0.7f, 0.7f, 0.7f };
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

    std::vector<float> rotationSpeeds = { 1.0f, -2.0f, -2.0f };
    std::vector<float> rotationOffsets = { 0.0f, -9.0f, -30.0f };

    nodes.resize(positions.size());
    for (uint32_t i = 0; i < nodes.size(); ++i) {
      vik::Node::NodeInfo gearNodeInfo = {};
      vik::GearInfo gearInfo = {};
      gearInfo.innerRadius = innerRadiuses[i];
      gearInfo.outerRadius = outerRadiuses[i];
      gearInfo.width = widths[i];
      gearInfo.numTeeth = toothCount[i];
      gearInfo.toothDepth = toothDepth[i];
      gearNodeInfo.pos = positions[i];
      gearNodeInfo.rotSpeed = rotationSpeeds[i];
      gearNodeInfo.rotOffset = rotationOffsets[i];
      gearNodeInfo.material = materials[i];

      nodes[i] = new vik::NodeGear();
      nodes[i]->setInfo(&gearNodeInfo);
      ((vik::NodeGear*)nodes[i])->generate(renderer->vksDevice, &gearInfo, renderer->queue);
    }

    vik::NodeModel* teapotNode = new vik::NodeModel();
    teapotNode->loadModel("teapot.dae",
                          vertexLayout,
                          0.25f,
                          renderer->vksDevice,
                          renderer->queue);

    vik::Material teapotMaterial = vik::Material("Cream", glm::vec3(1.0f, 1.0f, 0.7f), 1.0f, 1.0f);
    teapotNode->setMateral(teapotMaterial);
    nodes.push_back(teapotNode);

    glm::vec3 teapotPosition = glm::vec3(-15.0, -5.0, -5.0);
    teapotNode->setPosition(teapotPosition);
  }

  void prepareVertices() {
    // Binding and attribute descriptions are shared across all gears
    vertices.bindingDescriptions.resize(1);
    vertices.bindingDescriptions[0] =
        vik::initializers::vertexInputBindingDescription(
          VERTEX_BUFFER_BIND_ID,
          sizeof(vik::Vertex),
          VK_VERTEX_INPUT_RATE_VERTEX);

    // Attribute descriptions
    // Describes memory layout and shader positions
    vertices.attributeDescriptions.resize(3);
    // Location 0 : Position
    vertices.attributeDescriptions[0] =
        vik::initializers::vertexInputAttributeDescription(
          VERTEX_BUFFER_BIND_ID,
          0,
          VK_FORMAT_R32G32B32_SFLOAT,
          0);
    // Location 1 : Normal
    vertices.attributeDescriptions[1] =
        vik::initializers::vertexInputAttributeDescription(
          VERTEX_BUFFER_BIND_ID,
          1,
          VK_FORMAT_R32G32B32_SFLOAT,
          sizeof(float) * 3);
    // Location 2 : Color
    vertices.attributeDescriptions[2] =
        vik::initializers::vertexInputAttributeDescription(
          VERTEX_BUFFER_BIND_ID,
          2,
          VK_FORMAT_R32G32B32_SFLOAT,
          sizeof(float) * 6);

    vertices.inputState = vik::initializers::pipelineVertexInputStateCreateInfo();
    vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
    vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
    vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
    vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
  }

  void setupDescriptorPool() {
    // Example uses two ubos
    std::vector<VkDescriptorPoolSize> poolSizes = {
      vik::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 16),
      vik::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6)
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo =
        vik::initializers::descriptorPoolCreateInfo(
          static_cast<uint32_t>(poolSizes.size()),
          poolSizes.data(),
          6);

    vik_log_check(vkCreateDescriptorPool(renderer->device, &descriptorPoolInfo, nullptr, &renderer->descriptorPool));
  }

  void setupDescriptorSetLayout() {
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
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
    // if (enableSky)
      setLayoutBindings.push_back(vik::initializers::descriptorSetLayoutBinding(
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    3));

    VkDescriptorSetLayoutCreateInfo descriptorLayout =
        vik::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);

    vik_log_check(vkCreateDescriptorSetLayout(renderer->device, &descriptorLayout, nullptr, &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
        vik::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

    /*
     * Push Constants
     */

    std::vector<VkPushConstantRange> pushConstantRanges = {
      vik::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(vik::Material::PushBlock), sizeof(glm::vec3)),
    };

    pPipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRanges.size();
    pPipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

    vik_log_check(vkCreatePipelineLayout(renderer->device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
  }

  void setupDescriptorSet() {
    if (enableSky) {
      VkDescriptorSetAllocateInfo allocInfo =
          vik::initializers::descriptorSetAllocateInfo(
            renderer->descriptorPool,
            &descriptorSetLayout,
            1);

      skyBox->createDescriptorSet(allocInfo, &((vik::Camera*)camera)->uniformBuffer.descriptor);
    }

    for (auto& node : nodes)
      node->createDescriptorSet(renderer->device, renderer->descriptorPool, descriptorSetLayout,
                                &uniformBuffers.lights.descriptor,
                                &((vik::Camera*)camera)->uniformBuffer.descriptor,
                                skyBox);
}

  void preparePipelines() {
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        vik::initializers::pipelineInputAssemblyStateCreateInfo(
          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
          0,
          VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
        vik::initializers::pipelineRasterizationStateCreateInfo(
          VK_POLYGON_MODE_FILL,
          VK_CULL_MODE_BACK_BIT,
          VK_FRONT_FACE_CLOCKWISE);

    VkPipelineColorBlendAttachmentState blendAttachmentState =
        vik::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        vik::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        vik::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineViewportStateCreateInfo viewportState;
    if (enableStereo)
      viewportState = vik::initializers::pipelineViewportStateCreateInfo(2, 2, 0);
    else
      viewportState = vik::initializers::pipelineViewportStateCreateInfo(1, 1, 0);


    VkPipelineMultisampleStateCreateInfo multisampleState =
        vik::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

    std::vector<VkDynamicState> dynamicStateEnables = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_LINE_WIDTH
    };
    VkPipelineDynamicStateCreateInfo dynamicState =
        vik::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages;
    shaderStages[0] = vik::Shader::load(renderer->device, "xrgears/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = vik::Shader::load(renderer->device, "xrgears/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shaderStages[2] = vik::Shader::load(renderer->device, "xrgears/multiview.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo;

    VkRenderPass usedPass;
    if (enableDistortion)
      usedPass = offscreenPass->getRenderPass();
    else
      usedPass = renderer->render_pass;

    pipelineCreateInfo = vik::initializers::pipelineCreateInfo(pipelineLayout, usedPass);

    // Vertex bindings an attributes
    std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
      vik::initializers::vertexInputBindingDescription(0, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
      // Location 0: Position
      vik::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),
      // Location 1: Normals
      vik::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),
      // Location 2: Color
      vik::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 6),
    };
    VkPipelineVertexInputStateCreateInfo vertexInputState = vik::initializers::pipelineVertexInputStateCreateInfo();
    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();

    pipelineCreateInfo.renderPass = usedPass;



    vik_log_check(vkCreateGraphicsPipelines(renderer->device, renderer->pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.pbr));

    if (enableSky)
      skyBox->createPipeline(&pipelineCreateInfo, renderer->pipelineCache);
  }

  // Prepare and initialize uniform buffer containing shader uniforms
  void prepareUniformBuffers() {
    renderer->vksDevice->create_and_map(&uniformBuffers.lights, sizeof(uboLights));

    ((vik::Camera*)camera)->prepareUniformBuffers(renderer->vksDevice);

    for (auto& node : nodes)
      node->prepareUniformBuffer(renderer->vksDevice);

    updateUniformBuffers();
  }

  void updateUniformBuffers() {
    ((vik::Camera*)camera)->update_ubo();

    vik::StereoView sv = {};
    sv.view[0] = ((vik::Camera*)camera)->ubo.view[0];
    sv.view[1] = ((vik::Camera*)camera)->ubo.view[1];

    for (auto& node : nodes)
      node->updateUniformBuffer(sv, renderer->timer.animation_timer);

    updateLights();
  }

  void updateLights() {
    const float p = 15.0f;
    uboLights.lights[0] = glm::vec4(-p, -p*0.5f, -p, 1.0f);
    uboLights.lights[1] = glm::vec4(-p, -p*0.5f,  p, 1.0f);
    uboLights.lights[2] = glm::vec4( p, -p*0.5f,  p, 1.0f);
    uboLights.lights[3] = glm::vec4( p, -p*0.5f, -p, 1.0f);

    if (!renderer->timer.animation_paused) {
      uboLights.lights[0].x = sin(glm::radians(renderer->timer.animation_timer * 360.0f)) * 20.0f;
      uboLights.lights[0].z = cos(glm::radians(renderer->timer.animation_timer * 360.0f)) * 20.0f;
      uboLights.lights[1].x = cos(glm::radians(renderer->timer.animation_timer * 360.0f)) * 20.0f;
      uboLights.lights[1].y = sin(glm::radians(renderer->timer.animation_timer * 360.0f)) * 20.0f;
    }

    memcpy(uniformBuffers.lights.mapped, &uboLights, sizeof(uboLights));
  }

  void draw() {
    VkSubmitInfo submit_info = renderer->init_render_submit_info();

    if (enableDistortion) {
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
      submit_info.pSignalSemaphores = &offscreenSemaphore;

      // Submit work

      submit_info.pCommandBuffers = &offScreenCmdBuffer;
      vik_log_check(vkQueueSubmit(renderer->queue, 1, &submit_info, VK_NULL_HANDLE));

      // Scene rendering

      // Wait for offscreen semaphore
      submit_info.pWaitSemaphores = &offscreenSemaphore;
      // Signal ready with render complete semaphpre
      submit_info.pSignalSemaphores = &renderer->semaphores.render_complete;
    }

    // Submit to queue
    submit_info.pCommandBuffers = renderer->get_current_command_buffer();
    vik_log_check(vkQueueSubmit(renderer->queue, 1, &submit_info, VK_NULL_HANDLE));
  }

  void init() {
    Application::init();

    hmd = new vik::HMD();

    if (enableStereo) {
      if (enableHMDCam)
        camera = new vik::CameraHMD(hmd);
      else
        camera = new vik::CameraStereo(renderer->width, renderer->height);
    } else {
      camera = new vik::Camera();
    }

    camera->setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
    camera->setPosition(glm::vec3(2.2f, 3.2f, -7.6));
    camera->setPerspective(60.0f, (float)renderer->width / (float)renderer->height, 0.1f, 256.0f);
    camera->movementSpeed = 5.0f;

    if (enableSky)
      skyBox = new vik::SkyBox(renderer->device);


    loadAssets();
    initGears();
    prepareVertices();
    prepareUniformBuffers();
    setupDescriptorPool();
    setupDescriptorSetLayout();

    if (enableDistortion) {
      offscreenPass = new vik::OffscreenPass(renderer->device);
      offscreenPass->prepareOffscreenFramebuffer(renderer->vksDevice, renderer->physical_device);
      hmdDistortion = new vik::Distortion(renderer->device);
      hmdDistortion->generateQuads(renderer->vksDevice);
      hmdDistortion->prepareUniformBuffer(renderer->vksDevice);
      hmdDistortion->updateUniformBufferWarp(hmd->openHmdDevice);
      hmdDistortion->createDescriptorSetLayout();
      hmdDistortion->createPipeLineLayout();
      hmdDistortion->createPipeLine(renderer->render_pass, renderer->pipelineCache);
      hmdDistortion->createDescriptorSet(offscreenPass, renderer->descriptorPool);
    }

    preparePipelines();
    setupDescriptorSet();
    build_command_buffers();

    if (enableDistortion)
      buildOffscreenCommandBuffer();
  }

  virtual void render() {
    vkDeviceWaitIdle(renderer->device);
    draw();
    vkDeviceWaitIdle(renderer->device);
    if (!renderer->timer.animation_paused)
      updateUniformBuffers();
  }

  virtual void view_changed_cb() {
    updateUniformBuffers();
  }

  void changeEyeSeparation(float delta) {
    if (!enableHMDCam)
      ((vik::CameraStereo*)camera)->changeEyeSeparation(delta);
    updateUniformBuffers();
  }

  virtual void key_pressed(vik::Input::Key keyCode) {
    /*
    switch (keyCode) {
      case KEY_KPADD:
        changeEyeSeparation(0.005);
        break;
      case KEY_KPSUB:
        changeEyeSeparation(-0.005);
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
