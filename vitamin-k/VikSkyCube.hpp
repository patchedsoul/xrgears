#pragma once

#include "../vks/VulkanTexture.hpp"
#include "../vks/VulkanModel.hpp"

#include "VikAssets.hpp"
#include "VikShader.hpp"

#define VERTEX_BUFFER_BIND_ID 0

class VikSkyCube {
private:
  VkDevice device;
  VkPipeline pipeline;
  VkDescriptorSet descriptorSet;

  vks::TextureCubeMap cubemap;
  vks::Model model;
  vks::Buffer uniformBuffer;

  struct UBO {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model;
  };

  UBO ubo;

public:


  VikSkyCube(VkDevice d) {
    device = d;
  }

  ~VikSkyCube() {

    vkDestroyPipeline(device, pipeline, nullptr);

    model.destroy();
    uniformBuffer.destroy();
    cubemap.destroy();
  }

  void loadAssets(vks::VertexLayout vertexLayout, vks::VulkanDevice *vulkanDevice, VkQueue queue) {
    model.loadFromFile(VikAssets::getAssetPath() + "models/cube.obj", vertexLayout, 1.0f, vulkanDevice, queue);
    cubemap.loadFromFile(VikAssets::getAssetPath() + "textures/cubemap_space.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
  }


  void updateDescriptorSets(VkDescriptorSetAllocateInfo &descriptorSetAllocInfo,
                            std::vector<VkWriteDescriptorSet> &writeDescriptorSets) {
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &descriptorSet));
    writeDescriptorSets = {
      vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffer.descriptor),						// Binding 0: Vertex shader uniform buffer
      vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	1, &cubemap.descriptor),					// Binding 1: Fragment shader texture sampler
    };
    vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
  }

  void preparePipeline(VkPipelineCache& cache, VkGraphicsPipelineCreateInfo* pipelineCreateInfo) {
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = VikShader::load(device, "bloom/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = VikShader::load(device, "bloom/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    pipelineCreateInfo->stageCount = shaderStages.size();
    pipelineCreateInfo->pStages = shaderStages.data();

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, cache, 1, pipelineCreateInfo, nullptr, &pipeline));
  }

  void draw(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout) {
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(cmdBuffer, VERTEX_BUFFER_BIND_ID, 1, &model.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmdBuffer, model.indexCount, 1, 0, 0, 0);
  }

  void prepareUniformbuffers(vks::VulkanDevice *vulkanDevice) {
    // Skybox
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      &uniformBuffer,
      sizeof(ubo)));
     VK_CHECK_RESULT(uniformBuffer.map());
  }

  void updateUniformBuffers(int width, int height, glm::mat4& view) {
    // Skybox
    ubo.projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 256.0f);
    ubo.view = glm::mat4(glm::mat3(view));
    ubo.model = glm::mat4();
    memcpy(uniformBuffer.mapped, &ubo, sizeof(ubo));
  }

};
