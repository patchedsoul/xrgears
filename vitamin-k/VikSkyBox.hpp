#pragma once

#include "../vks/vksTexture.hpp"
#include "../vks/vksModel.hpp"

#include "VikAssets.hpp"
#include "VikShader.hpp"
#include "VikBuffer.hpp"

class VikSkyBox
{
private:
  vks::TextureCubeMap cubeMap;
	VkDescriptorSet descriptorSet;
  VkDevice device;
  VkDescriptorImageInfo textureDescriptor;
  vks::Model model;
  VkPipeline pipeline;

public:
  VikSkyBox(VkDevice device) : device(device) {}

  ~VikSkyBox() {
		cubeMap.destroy();
    model.destroy();
    vkDestroyPipeline(device, pipeline, nullptr);
	}

	void initTextureDescriptor() {
		// Image descriptor for the cube map texture
		textureDescriptor =
				vks::initializers::descriptorImageInfo(
					cubeMap.sampler,
					cubeMap.view,
					cubeMap.imageLayout);
	}

	VkWriteDescriptorSet getCubeMapWriteDescriptorSet(unsigned binding, VkDescriptorSet ds) {

		return vks::initializers::writeDescriptorSet(
					ds,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					binding,
					&textureDescriptor);
	}

  void loadAssets(vks::VertexLayout vertexLayout, vks::VulkanDevice *vulkanDevice, VkQueue queue) {
		// Skybox
    model.loadFromFile(VikAssets::getAssetPath() + "models/cube.obj", vertexLayout, 10.0f, vulkanDevice, queue);
    cubeMap.loadFromFile(
          //VikAssets::getTexturePath() + "cubemap_yokohama_bc3_unorm.ktx", VK_FORMAT_BC2_UNORM_BLOCK,
          //VikAssets::getTexturePath() + "equirect/cube2/cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT,
          //VikAssets::getTexturePath() + "hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT,
          VikAssets::getAssetPath() + "textures/cubemap_space.ktx", VK_FORMAT_R8G8B8A8_UNORM,
          vulkanDevice,
          queue);
    initTextureDescriptor();
  }

  void createDescriptorSet(VkDescriptorSetAllocateInfo& allocInfo,
                           VkDescriptorBufferInfo& cameraDescriptor) {
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
      vks::initializers::writeDescriptorSet(
      descriptorSet,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      2,
      &cameraDescriptor)
    };

    writeDescriptorSets.push_back(getCubeMapWriteDescriptorSet(3, descriptorSet));

    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(),
                           0,
                           nullptr);
  }

  void draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout) {
		VkDeviceSize offsets[1] = { 0 };

		vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &model.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(cmdbuffer, model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdDrawIndexed(cmdbuffer, model.indexCount, 1, 0, 0, 0);
	}

  void createPipeline(VkGraphicsPipelineCreateInfo& pipelineCreateInfo,
                      VkPipelineCache& pipelineCache) {
    VkPipelineRasterizationStateCreateInfo rasterizationStateSky =
        vks::initializers::pipelineRasterizationStateCreateInfo(
          VK_POLYGON_MODE_FILL,
          VK_CULL_MODE_BACK_BIT,
          VK_FRONT_FACE_COUNTER_CLOCKWISE,
          0);

    // Skybox pipeline (background cube)
    std::array<VkPipelineShaderStageCreateInfo,3> shaderStagesSky;

    shaderStagesSky[0] = VikShader::load(device, "xrgears/sky.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStagesSky[1] = VikShader::load(device, "xrgears/sky.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shaderStagesSky[2] = VikShader::load(device, "xrgears/sky.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    pipelineCreateInfo.stageCount = shaderStagesSky.size();
    pipelineCreateInfo.pStages = shaderStagesSky.data();
    pipelineCreateInfo.pRasterizationState = &rasterizationStateSky;

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

    vkDestroyShaderModule(device, shaderStagesSky[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStagesSky[1].module, nullptr);
    vkDestroyShaderModule(device, shaderStagesSky[2].module, nullptr);
  }
};
