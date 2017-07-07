#pragma once

#include <vulkan/vulkan.h>

#include "../vks/VulkanModel.hpp"
#include "VikOffscreenPass.hpp"

#define VERTEX_BUFFER_BIND_ID 0

class VikDistortion{
private:
  VkDevice device;
  vks::Model quad;
  vks::Buffer uboHandle;

  struct {
    glm::vec4 hmdWarpParam;
    glm::vec4 aberr;
    glm::vec2 lensCenter;
    glm::vec2 viewportScale;
    float warpScale;
  } uboData;

  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;

  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorSet descriptorSet;

public:
  VikDistortion(VkDevice& d) {
    device = d;
  }

  ~VikDistortion() {
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    quad.destroy();
    uboHandle.destroy();
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
  }

  void createPipeLine(VkGraphicsPipelineCreateInfo &pipelineCreateInfo, VkPipelineCache& pipelineCache, std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages) {
    VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
    pipelineCreateInfo.pVertexInputState = &emptyInputState;
    pipelineCreateInfo.layout = pipelineLayout;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
  }

  VkWriteDescriptorSet getUniformWriteDescriptorSet(VkDescriptorSet& descriptorSet, uint32_t binding) {
    return vks::initializers::writeDescriptorSet(
          descriptorSet,
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          binding,
          &uboHandle.descriptor);
  }

  void createDescriptorSet(VikOffscreenPass *offscreenPass, VkDescriptorPool& descriptorPool) {
    std::vector<VkWriteDescriptorSet> writeDescriptorSets;

    // Textured quad descriptor set
    VkDescriptorSetAllocateInfo allocInfo =
        vks::initializers::descriptorSetAllocateInfo(
          descriptorPool,
          &descriptorSetLayout,
          1);

    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    VkDescriptorImageInfo offScreenImageInfo = offscreenPass->getDescriptorImageInfo();

    writeDescriptorSets = {
      // Binding 1 : Position texture target
      offscreenPass->getImageWriteDescriptorSet(descriptorSet, &offScreenImageInfo, 1),
      // Binding 4 : Fragment shader uniform buffer
      getUniformWriteDescriptorSet(descriptorSet, 2)
    };

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

  }

  void createDescriptorSetLayout() {
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
  }

  void createPipeLineLayout() {
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
        vks::initializers::pipelineLayoutCreateInfo(
          &descriptorSetLayout,
          1);

    VK_CHECK_RESULT(vkCreatePipelineLayout(device,
                                           &pPipelineLayoutCreateInfo,
                                           nullptr,
                                           &pipelineLayout));

  }

  void drawQuad(VkCommandBuffer& commandBuffer) {
    VkDeviceSize offsets[1] = { 0 };

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1,
                            &descriptorSet, 0, NULL);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindVertexBuffers(commandBuffer, VERTEX_BUFFER_BIND_ID, 1, &quad.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, quad.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, quad.indexCount, 1, 0, 0, 1);
  }

  void generateQuads(vks::VulkanDevice *vulkanDevice) {
    // Setup vertices for multiple screen aligned quads
    // Used for displaying final result and debug
    struct Vertex {
      float pos[3];
      float uv[2];
    };

    std::vector<Vertex> vertexBuffer;

    float x = 0.0f;
    float y = 0.0f;
    for (uint32_t i = 0; i < 3; i++) {
      // Last component of normal is used for debug display sampler index
      vertexBuffer.push_back({ { x+1.0f, y+1.0f, 0.0f }, { 1.0f, 1.0f } });
      vertexBuffer.push_back({ { x,      y+1.0f, 0.0f }, { 0.0f, 1.0f } });
      vertexBuffer.push_back({ { x,      y,      0.0f }, { 0.0f, 0.0f } });
      vertexBuffer.push_back({ { x+1.0f, y,      0.0f }, { 1.0f, 0.0f } });
      x += 1.0f;
      if (x > 1.0f)
      {
        x = 0.0f;
        y += 1.0f;
      }
    }

    VK_CHECK_RESULT(vulkanDevice->createBuffer(
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      vertexBuffer.size() * sizeof(Vertex),
                      &quad.vertices.buffer,
                      &quad.vertices.memory,
                      vertexBuffer.data()));

    // Setup indices
    std::vector<uint32_t> indexBuffer = { 0,1,2, 2,3,0 };

    quad.indexCount = static_cast<uint32_t>(indexBuffer.size());

    VK_CHECK_RESULT(vulkanDevice->createBuffer(
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      indexBuffer.size() * sizeof(uint32_t),
                      &quad.indices.buffer,
                      &quad.indices.memory,
                      indexBuffer.data()));

    quad.device = device;
  }

  // Update fragment shader hmd warp uniform block
  void updateUniformBufferWarp() {
    uboData.lensCenter = glm::vec2(0.0297, 0.0497);
    uboData.viewportScale = glm::vec2(0.0614, 0.0682);
    uboData.warpScale = 0.0318;
    uboData.hmdWarpParam = glm::vec4(0.2470, -0.1450, 0.1030, 0.7950);
    uboData.aberr = glm::vec4(0.9850, 1.0000, 1.0150, 1.0);
    memcpy(uboHandle.mapped, &uboData, sizeof(uboData));
  }

  void prepareUniformBuffer(vks::VulkanDevice *vulkanDevice) {
    // Warp UBO in deferred fragment shader
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &uboHandle,
                      sizeof(uboData)));
    VK_CHECK_RESULT(uboHandle.map());
  }
};
