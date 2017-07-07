/*
* Vulkan Example - Viewport array with single pass rendering using geometry shaders
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
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
#include <gli/gli.hpp>

#include <vulkan/vulkan.h>
#include "VikGearNode.hpp"
#include "vulkanexamplebase.h"
#include "VulkanModel.hpp"

#include "VikSkyDome.hpp"
#include "VikDistortion.hpp"
#include "VikOffscreenPass.hpp"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION true

class VulkanExample : public VulkanExampleBase
{
public:
  // Vertex layout for the models
  vks::VertexLayout vertexLayout = vks::VertexLayout({
    vks::VERTEX_COMPONENT_POSITION,
    vks::VERTEX_COMPONENT_NORMAL,
    vks::VERTEX_COMPONENT_COLOR,
  });

  //vks::Model teapotModel;

  bool enableSky = false;

  SkyDome skyDome;
  VikDistortion *hmdDistortion;
  VikOffscreenPass *offscreenPass;

  struct {
    VkDescriptorSet object;
  } descriptorSets;

  struct {
    VkPipelineVertexInputStateCreateInfo inputState;
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
  } vertices;

  std::vector<GearNode*> gears;

  struct UBOLights {
    glm::vec4 lights[4];
  } uboLights;

  struct UBOCamera {
    glm::mat4 projection[2];
    glm::mat4 view[2];
    glm::mat4 skyView[2];
    glm::vec3 position;
  } uboCamera;

  struct {
    vks::Buffer lights;
    vks::Buffer camera;
  } uniformBuffers;

  struct {
    VkPipeline pbr;
  } pipelines;


  VkPipelineLayout pipelineLayout;

  //VkPipelineLayout pipelineLayoutSky;
  //VkDescriptorSetLayout descriptorSetLayoutSky;

  //VkDescriptorSet skydomeDescriptorSet;
  VkDescriptorSetLayout descriptorSetLayout;

  VkCommandBuffer offScreenCmdBuffer = VK_NULL_HANDLE;
  // Semaphore used to synchronize between offscreen and final scene rendering
  VkSemaphore offscreenSemaphore = VK_NULL_HANDLE;

  // Camera and view properties
  float eyeSeparation = 0.08f;
  const float focalLength = 0.5f;
  const float fov = 90.0f;
  const float zNear = 0.1f;
  const float zFar = 256.0f;

  VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
  {
    title = "XR Gears";
    enableTextOverlay = true;
    camera.type = Camera::CameraType::firstperson;
    camera.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
    camera.setTranslation(glm::vec3(2.2f, 3.2f, -7.6));

    camera.movementSpeed = 5.0f;
    timerSpeed *= 0.25f;
    //paused = true;

  }

  ~VulkanExample()
  {
    delete offscreenPass;

    vkDestroyPipeline(device, pipelines.pbr, nullptr);

    if (enableSky)
      vkDestroyPipeline(device, skyDome.pipeline, nullptr);

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    delete hmdDistortion;

    uniformBuffers.camera.destroy();
    uniformBuffers.lights.destroy();

    //		teapotModel.destroy();

    for (auto& gear : gears)
      delete(gear);

    vkDestroySemaphore(device, offscreenSemaphore, nullptr);
  }

  // Enable physical device features required for this example
  virtual void getEnabledFeatures()
  {
    // Geometry shader support is required for this example
    if (deviceFeatures.geometryShader) {
      enabledFeatures.geometryShader = VK_TRUE;
    }
    else {
      vks::tools::exitFatal("Selected GPU does not support geometry shaders!", "Feature not supported");
    }
    // Multiple viewports must be supported
    if (deviceFeatures.multiViewport) {
      enabledFeatures.multiViewport = VK_TRUE;
    }
    else {
      vks::tools::exitFatal("Selected GPU does not support multi viewports!", "Feature not supported");
    }
  }

  void buildCommandBuffers()
  {
    printf("Draw command buffers size: %d\n", drawCmdBuffers.size());

    for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
      //buildPbrCommandBuffer(drawCmdBuffers[i], frameBuffers[i]);
      buildWarpCommandBuffer(drawCmdBuffers[i], frameBuffers[i]);
  }

  void buildWarpCommandBuffer(VkCommandBuffer& cmdBuffer, VkFramebuffer frameBuffer) {

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

    // Set target frame buffer
    renderPassBeginInfo.framebuffer = frameBuffer;

    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = vks::initializers::viewport(
          (float) width, (float) height,
          0.0f, 1.0f);
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    // Final composition as full screen quad
    hmdDistortion->drawQuad(cmdBuffer);

    vkCmdEndRenderPass(cmdBuffer);

    VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));
  }

  // Build command buffer for rendering the scene to the offscreen frame buffer attachments
  void buildOffscreenCommandBuffer()
  {
    if (offScreenCmdBuffer == VK_NULL_HANDLE)
      offScreenCmdBuffer = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

    // Create a semaphore used to synchronize offscreen rendering and usage
    VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &offscreenSemaphore));

    buildPbrCommandBuffer(offScreenCmdBuffer);

    /*
    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

    // Clear values for all attachments written in the fragment sahder
    std::array<VkClearValue,2> clearValues;
    clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VK_CHECK_RESULT(vkBeginCommandBuffer(offScreenCmdBuffer, &cmdBufInfo));

    offscreenPass->beginRenderPass(offScreenCmdBuffer);
    offscreenPass->setViewPortAndScissor(offScreenCmdBuffer);

    vkCmdBindPipeline(offScreenCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.offscreen);

    drawModel(offScreenCmdBuffer);

    vkCmdEndRenderPass(offScreenCmdBuffer);

    VK_CHECK_RESULT(vkEndCommandBuffer(offScreenCmdBuffer));
    */
  }

  void setViewPortAndScissors(VkCommandBuffer cmdBuffer) {
    VkViewport viewports[2];
    // Left
    viewports[0] = { 0, 0, (float)width / 2.0f, (float)height, 0.0, 1.0f };
    // Right
    viewports[1] = { (float)width / 2.0f, 0, (float)width / 2.0f, (float)height, 0.0, 1.0f };

    vkCmdSetViewport(cmdBuffer, 0, 2, viewports);

    VkRect2D scissorRects[2] = {
      vks::initializers::rect2D(width/2, height, 0, 0),
      vks::initializers::rect2D(width/2, height, width / 2, 0),
    };
    vkCmdSetScissor(cmdBuffer, 0, 2, scissorRects);
  }

  void buildPbrCommandBuffer(VkCommandBuffer cmdBuffer) {
    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

    /*
    VkClearValue clearValues[2];
    clearValues[0].color = defaultClearColor;
    clearValues[1].depthStencil = { 1.0f, 0 };


    VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = width;
    renderPassBeginInfo.renderArea.extent.height = height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;
    // Set target frame buffer
    renderPassBeginInfo.framebuffer = frameBuffer;
    */

    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

    if (vks::debugmarker::active)
      vks::debugmarker::beginRegion(cmdBuffer, "Pbr offscreen", glm::vec4(0.3f, 0.94f, 1.0f, 1.0f));

    //vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    //setViewPortAndScissors(cmdBuffer);

    offscreenPass->beginRenderPass(cmdBuffer);
    //offscreenPass->setViewPortAndScissor(cmdBuffer);
    offscreenPass->setViewPortAndScissorStereo(cmdBuffer);
    //setViewPortAndScissors(cmdBuffer);

    vkCmdSetLineWidth(cmdBuffer, 1.0f);

    if (enableSky)
      skyDome.draw(cmdBuffer, pipelineLayout, gears[0]->descriptorSet);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);
    //drawTeapot(drawCmdBuffers[i]);

    for (auto& gear : gears)
      gear->draw(cmdBuffer, pipelineLayout);

    vkCmdEndRenderPass(cmdBuffer);

    if (vks::debugmarker::active)
      vks::debugmarker::endRegion(cmdBuffer);

    VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));
  }

  /*
  void drawTeapot(VkCommandBuffer cmdbuffer) {
    vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &gears[0]->descriptorSet, 0, nullptr);
    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &teapotModel.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(cmdbuffer, teapotModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmdbuffer,
                       pipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(glm::vec3),
                       sizeof(Material::PushBlock), &gears[0]->material);
    vkCmdDrawIndexed(cmdbuffer, teapotModel.indexCount, 1, 0, 0, 0);
  }
  */

  void loadAssets() {
    if (enableSky)
      skyDome.loadAssets(getAssetPath(), vertexLayout, vulkanDevice, queue);
    //teapotModel.loadFromFile(getAssetPath() + "models/sphere.obj", vertexLayout, 0.25f, vulkanDevice, queue);
  }

  void initGears() {
    // Gear definitions
    std::vector<float> innerRadiuses = { 1.0f, 0.5f, 1.3f };
    std::vector<float> outerRadiuses = { 4.0f, 2.0f, 2.0f };
    std::vector<float> widths = { 1.0f, 2.0f, 0.5f };
    std::vector<int32_t> toothCount = { 20, 10, 10 };
    std::vector<float> toothDepth = { 0.7f, 0.7f, 0.7f };
    std::vector<glm::vec3> colors = {
      glm::vec3(1.0f, 0.0f, 0.0f),
      glm::vec3(0.0f, 1.0f, 0.2f),
      glm::vec3(0.0f, 0.0f, 1.0f)
    };
    std::vector<glm::vec3> positions = {
      glm::vec3(-3.0, 0.0, 0.0),
      glm::vec3(3.1, 0.0, 0.0),
      glm::vec3(-3.1, -6.2, 0.0)
    };

    std::vector<Material> materials = {
      Material("Red", glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, 1.0f),
      Material("Green", glm::vec3(0.0f, 1.0f, 0.2f), 0.5f, 1.0f),
      Material("Blue", glm::vec3(0.0f, 0.0f, 1.0f), 0.5f, 1.0f)
    };

    std::vector<float> rotationSpeeds = { 1.0f, -2.0f, -2.0f };
    std::vector<float> rotationOffsets = { 0.0f, -9.0f, -30.0f };

    gears.resize(positions.size());
    for (int32_t i = 0; i < gears.size(); ++i)
    {
      GearNodeInfo gearNodeInfo = {};
      GearInfo gearInfo = {};
      gearInfo.innerRadius = innerRadiuses[i];
      gearInfo.outerRadius = outerRadiuses[i];
      gearInfo.width = widths[i];
      gearInfo.numTeeth = toothCount[i];
      gearInfo.toothDepth = toothDepth[i];
      //gearNodeInfo.color = colors[i];
      gearNodeInfo.pos = positions[i];
      gearNodeInfo.rotSpeed = rotationSpeeds[i];
      gearNodeInfo.rotOffset = rotationOffsets[i];
      gearNodeInfo.material = materials[i];

      gears[i] = new GearNode(vulkanDevice);
      gears[i]->generate(&gearNodeInfo, &gearInfo, queue);
    }
  }

  void prepareVertices()
  {
    // Binding and attribute descriptions are shared across all gears
    vertices.bindingDescriptions.resize(1);
    vertices.bindingDescriptions[0] =
        vks::initializers::vertexInputBindingDescription(
          VERTEX_BUFFER_BIND_ID,
          sizeof(Vertex),
          VK_VERTEX_INPUT_RATE_VERTEX);

    // Attribute descriptions
    // Describes memory layout and shader positions
    vertices.attributeDescriptions.resize(3);
    // Location 0 : Position
    vertices.attributeDescriptions[0] =
        vks::initializers::vertexInputAttributeDescription(
          VERTEX_BUFFER_BIND_ID,
          0,
          VK_FORMAT_R32G32B32_SFLOAT,
          0);
    // Location 1 : Normal
    vertices.attributeDescriptions[1] =
        vks::initializers::vertexInputAttributeDescription(
          VERTEX_BUFFER_BIND_ID,
          1,
          VK_FORMAT_R32G32B32_SFLOAT,
          sizeof(float) * 3);
    // Location 2 : Color
    vertices.attributeDescriptions[2] =
        vks::initializers::vertexInputAttributeDescription(
          VERTEX_BUFFER_BIND_ID,
          2,
          VK_FORMAT_R32G32B32_SFLOAT,
          sizeof(float) * 6);

    vertices.inputState = vks::initializers::pipelineVertexInputStateCreateInfo();
    vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
    vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
    vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
    vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
  }

  void setupDescriptorPool()
  {
    // Example uses two ubos
    std::vector<VkDescriptorPoolSize> poolSizes = {
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10),
      vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4)
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo =
        vks::initializers::descriptorPoolCreateInfo(
          static_cast<uint32_t>(poolSizes.size()),
          poolSizes.data(),
          4);

    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
  }

  void setupDescriptorSetLayoutShading()
  {
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
    {
      // ubo model
      vks::initializers::descriptorSetLayoutBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_GEOMETRY_BIT, //VK_SHADER_STAGE_VERTEX_BIT,
      0),
      // ubo lights
      vks::initializers::descriptorSetLayoutBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      1),
      // ubo camera
      vks::initializers::descriptorSetLayoutBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      2)
    };

    // cube map sampler
    //if (enableSky)
      setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    3));

    VkDescriptorSetLayoutCreateInfo descriptorLayout =
        vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);

    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
        vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

    /*
     * Push Constants
     */

    std::vector<VkPushConstantRange> pushConstantRanges = {
      vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Material::PushBlock), sizeof(glm::vec3)),
    };

    pPipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRanges.size();
    pPipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));

    //setupDescriptorSetLayoutSky();
  }

  /*
  void setupDescriptorSetLayoutSky()
  {
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
    {
      // Binding 0 : Vertex shader uniform buffer
      vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT,
        0),
      // Binding 1 : Fragment shader image sampler
      vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        1)
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayout =
      vks::initializers::descriptorSetLayoutCreateInfo(
        setLayoutBindings.data(),
        setLayoutBindings.size());

    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayoutSky));

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
      vks::initializers::pipelineLayoutCreateInfo(
        &descriptorSetLayoutSky,
        1);

    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
  }
  */

  void setupDescriptorSet()
  {

    if (enableSky)
      skyDome.initTextureDescriptor();

    for (auto& gear : gears)
    {
      VkDescriptorSetAllocateInfo allocInfo =
          vks::initializers::descriptorSetAllocateInfo(
            descriptorPool,
            &descriptorSetLayout,
            1);

      VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &gear->descriptorSet));

      std::vector<VkWriteDescriptorSet> writeDescriptorSets = {

        // Binding 0 : Vertex shader uniform buffer
        vks::initializers::writeDescriptorSet(
        gear->descriptorSet,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        0,
        &gear->uniformBuffer.descriptor),
        vks::initializers::writeDescriptorSet(
        gear->descriptorSet,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        1,
        &uniformBuffers.lights.descriptor),
        vks::initializers::writeDescriptorSet(
        gear->descriptorSet,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        2,
        &uniformBuffers.camera.descriptor)
      };

      if (enableSky)
        writeDescriptorSets.push_back(skyDome.getCubeMapWriteDescriptorSet(3, gear->descriptorSet));

      vkUpdateDescriptorSets(device,
                             static_cast<uint32_t>(writeDescriptorSets.size()),
                             writeDescriptorSets.data(),
                             0,
                             nullptr);
    }
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
          VK_FRONT_FACE_CLOCKWISE);

    VkPipelineColorBlendAttachmentState blendAttachmentState =
        vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    // We use two viewports
    VkPipelineViewportStateCreateInfo viewportState =
        vks::initializers::pipelineViewportStateCreateInfo(2, 2, 0);

    VkPipelineMultisampleStateCreateInfo multisampleState =
        vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

    std::vector<VkDynamicState> dynamicStateEnables = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_LINE_WIDTH
    };
    VkPipelineDynamicStateCreateInfo dynamicState =
        vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    // Tessellation pipeline
    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages;


    VkGraphicsPipelineCreateInfo pipelineCreateInfo =
        vks::initializers::pipelineCreateInfo(pipelineLayout, offscreenPass->getRenderPass());


    /*
    VkGraphicsPipelineCreateInfo pipelineCreateInfo =
        vks::initializers::pipelineCreateInfo(
          nullptr,
          offscreenPass->getRenderPass(),
          0);
*/

    // Vertex bindings an attributes
    std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
      vks::initializers::vertexInputBindingDescription(0, vertexLayout.stride(), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
      vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),					// Location 0: Position
      vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3),	// Location 1: Normals
      vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 6),	// Location 2: Color

    };
    VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
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
    pipelineCreateInfo.renderPass = offscreenPass->getRenderPass();

    shaderStages[0] = loadShader(getAssetPath() + "shaders/xrgears/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(getAssetPath() + "shaders/xrgears/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    // A geometry shader is used to output geometry to multiple viewports in one single pass
    // See the "invoctations" decorator of the layout input in the shader
    shaderStages[2] = loadShader(getAssetPath() + "shaders/xrgears/multiview.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.pbr));

    if (enableSky)
      createSkyDomePipeline(pipelineCreateInfo);

  }

  void createSkyDomePipeline(VkGraphicsPipelineCreateInfo& pipelineCreateInfo) {
    VkPipelineRasterizationStateCreateInfo rasterizationStateSky =
        vks::initializers::pipelineRasterizationStateCreateInfo(
          VK_POLYGON_MODE_FILL,
          VK_CULL_MODE_BACK_BIT,
          VK_FRONT_FACE_COUNTER_CLOCKWISE,
          0);


    // Skybox pipeline (background cube)
    std::array<VkPipelineShaderStageCreateInfo,3> shaderStagesSky;

    shaderStagesSky[0] = loadShader(getAssetPath() + "shaders/xrgears/sky.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStagesSky[1] = loadShader(getAssetPath() + "shaders/xrgears/sky.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shaderStagesSky[2] = loadShader(getAssetPath() + "shaders/xrgears/sky.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    pipelineCreateInfo.stageCount = shaderStagesSky.size();
    pipelineCreateInfo.pStages = shaderStagesSky.data();
    pipelineCreateInfo.pRasterizationState = &rasterizationStateSky;

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &skyDome.pipeline));
  }

  void createUniformBuffer(vks::Buffer *buffer,  VkDeviceSize size) {
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      buffer,
                      size));

    // Map persistent
    VK_CHECK_RESULT(buffer->map());
  }

  // Prepare and initialize uniform buffer containing shader uniforms
  void prepareUniformBuffers()
  {
    createUniformBuffer(&uniformBuffers.lights, sizeof(uboLights));
    createUniformBuffer(&uniformBuffers.camera, sizeof(uboCamera));

    for (auto& gear : gears)
      gear->prepareUniformBuffer();

    updateUniformBuffers();
  }

  void updateUniformBuffers()
  {
    updateCamera();

    StereoView sv = {};
    sv.view[0] = uboCamera.view[0];
    sv.view[1] = uboCamera.view[1];

    for (auto& gear : gears)
      gear->updateUniformBuffer(sv, timer);

    updateLights();
  }


  void updateCamera() {
    // Geometry shader matrices for the two viewports
    // See http://paulbourke.net/stereographics/stereorender/

    // Calculate some variables
    float aspectRatio = (float)(width * 0.5f) / (float)height;
    float wd2 = zNear * tan(glm::radians(fov / 2.0f));
    float ndfl = zNear / focalLength;
    float left, right;
    float top = wd2;
    float bottom = -wd2;

    glm::vec3 camFront;
    camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
    camFront.y = sin(glm::radians(rotation.x));
    camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
    camFront = glm::normalize(camFront);
    glm::vec3 camRight = glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::mat4 rotM = glm::mat4();
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(camera.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(camera.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(camera.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    // Left eye
    left = -aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;
    right = aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;

    transM = glm::translate(glm::mat4(), camera.position - camRight * (eyeSeparation / 2.0f));

    uboCamera.projection[0] = glm::frustum(left, right, bottom, top, zNear, zFar);
    uboCamera.view[0] = rotM * transM;
    uboCamera.skyView[0] = rotM * glm::translate(glm::mat4(), -camRight * (eyeSeparation / 2.0f));

    // Right eye
    left = -aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;
    right = aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;

    transM = glm::translate(glm::mat4(), camera.position + camRight * (eyeSeparation / 2.0f));

    uboCamera.projection[1] = glm::frustum(left, right, bottom, top, zNear, zFar);
    uboCamera.view[1] = rotM * transM;
    uboCamera.skyView[1] = rotM * glm::translate(glm::mat4(), camRight * (eyeSeparation / 2.0f));;

    uboCamera.position = camera.position * -1.0f;

    memcpy(uniformBuffers.camera.mapped, &uboCamera, sizeof(uboCamera));
  }

  void updateLights()
  {
    const float p = 15.0f;
    uboLights.lights[0] = glm::vec4(-p, -p*0.5f, -p, 1.0f);
    uboLights.lights[1] = glm::vec4(-p, -p*0.5f,  p, 1.0f);
    uboLights.lights[2] = glm::vec4( p, -p*0.5f,  p, 1.0f);
    uboLights.lights[3] = glm::vec4( p, -p*0.5f, -p, 1.0f);

    if (!paused)
    {
      uboLights.lights[0].x = sin(glm::radians(timer * 360.0f)) * 20.0f;
      uboLights.lights[0].z = cos(glm::radians(timer * 360.0f)) * 20.0f;
      uboLights.lights[1].x = cos(glm::radians(timer * 360.0f)) * 20.0f;
      uboLights.lights[1].y = sin(glm::radians(timer * 360.0f)) * 20.0f;
    }

    memcpy(uniformBuffers.lights.mapped, &uboLights, sizeof(uboLights));
  }


  void loadCubemap(std::string filename, VkFormat format)
  {
    VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    skyDome.loadCubemap(device, vulkanDevice, copyCmd, filename, format);
    VulkanExampleBase::flushCommandBuffer(copyCmd, queue, true);
    skyDome.deleteStatingBuffer(device);
    skyDome.createSampler(device, vulkanDevice);
    skyDome.createImageView(device, format);
  }


  void loadTextures()
  {

    /*
    // Vulkan core supports three different compressed texture formats
    // As the support differs between implemementations we need to check device features and select a proper format and file
    std::string filename;
    VkFormat format;
    if (deviceFeatures.textureCompressionBC) {
      filename = "cubemap_yokohama_bc3_unorm.ktx";
      format = VK_FORMAT_BC2_UNORM_BLOCK;
    }
    else if (deviceFeatures.textureCompressionASTC_LDR) {
      filename = "cubemap_yokohama_astc_8x8_unorm.ktx";
      format = VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    }
    else if (deviceFeatures.textureCompressionETC2) {
      filename = "cubemap_yokohama_etc2_unorm.ktx";
      format = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    }
    else {
      vks::tools::exitFatal("Device does not support any compressed texture format!", "Error");
    }

    printf("Using texture %s\n", filename.c_str());

    loadCubemap(getAssetPath() + "textures/" + filename, format);
    */


    // ==>
    if (enableSky)
      loadCubemap(getAssetPath() + "textures/equirect/cube2/cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);



    //loadCubemap(getAssetPath() + "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);

    //loadCubemap(getAssetPath() + "textures/cubemap_yokohama_bc3_unorm.ktx", VK_FORMAT_BC2_UNORM_BLOCK);

  }

  void draw()
  {
    /*
    VulkanExampleBase::prepareFrame();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VulkanExampleBase::submitFrame();
    */

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
    loadTextures();
    loadAssets();

    hmdDistortion = new VikDistortion(device);
    hmdDistortion->generateQuads(vulkanDevice);

    initGears();
    prepareVertices();

    offscreenPass = new VikOffscreenPass(device);
    offscreenPass->prepareOffscreenFramebuffer(vulkanDevice, physicalDevice);

    prepareUniformBuffers();
    hmdDistortion->prepareUniformBuffer(vulkanDevice);
    hmdDistortion->updateUniformBufferWarp();

    setupDescriptorSetLayoutShading();

    hmdDistortion->createDescriptorSetLayout();
    hmdDistortion->createPipeLineLayout();
    hmdDistortion->createPipeLine(renderPass, pipelineCache);

    preparePipelines();
    setupDescriptorPool();
    hmdDistortion->createDescriptorSet(offscreenPass, descriptorPool);
    setupDescriptorSet();
    buildCommandBuffers();
    buildOffscreenCommandBuffer();
    prepared = true;
  }

  virtual void render()
  {
    if (!prepared)
      return;

    vkDeviceWaitIdle(device);
    draw();
    vkDeviceWaitIdle(device);
    if (!paused)
    {
      updateUniformBuffers();
    }
  }

  virtual void viewChanged()
  {
    updateUniformBuffers();
  }

  void changeEyeSeparation(float delta)
  {
    eyeSeparation += delta;
    updateUniformBuffers();
  }

  virtual void keyPressed(uint32_t keyCode)
  {
    switch (keyCode)
    {
      case KEY_KPADD:
      case GAMEPAD_BUTTON_R1:
        changeEyeSeparation(0.005);
        break;
      case KEY_KPSUB:
      case GAMEPAD_BUTTON_L1:
        changeEyeSeparation(-0.005);
        break;
    }
  }

};

VULKAN_EXAMPLE_MAIN()
