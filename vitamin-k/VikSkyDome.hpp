#pragma once

#include "../vks/VulkanTexture.hpp"
#include "../vks/VulkanModel.hpp"

#include "VikAssets.hpp"

class SkyDome
{
private:
  vks::TextureCubeMap cubeMap;
	VkDescriptorSet descriptorSet;

public:
	VkDescriptorImageInfo textureDescriptor;
	vks::Model skyboxModel;
	VkPipeline pipeline;

	~SkyDome() {
		cubeMap.destroy();
		skyboxModel.destroy();
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
    skyboxModel.loadFromFile(VikAssets::getAssetPath() + "models/cube.obj", vertexLayout, 10.0f, vulkanDevice, queue);
    cubeMap.loadFromFile(
          //VikAssets::getTexturePath() + "cubemap_yokohama_bc3_unorm.ktx", VK_FORMAT_BC2_UNORM_BLOCK,
          //VikAssets::getTexturePath() + "equirect/cube2/cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT,
          VikAssets::getTexturePath() + "hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT,
          vulkanDevice,
          queue);
  }

	void draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
		VkDeviceSize offsets[1] = { 0 };

		vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &skyboxModel.vertices.buffer, offsets);
		vkCmdBindIndexBuffer(cmdbuffer, skyboxModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		vkCmdDrawIndexed(cmdbuffer, skyboxModel.indexCount, 1, 0, 0, 0);
	}
};
