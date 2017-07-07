#pragma once

#include "../vks/VulkanTexture.hpp"
#include "../vks/VulkanModel.hpp"

class SkyDome
{
private:
	vks::Texture cubeMap;

	VkDescriptorSet descriptorSet;

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

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

	void loadAssets(const std::string path, vks::VertexLayout vertexLayout, vks::VulkanDevice *vulkanDevice, VkQueue queue) {
		// Skybox
		skyboxModel.loadFromFile(path + "models/cube.obj", vertexLayout, 10.0f, vulkanDevice, queue);
	}

	void loadCubemap(VkDevice device, vks::VulkanDevice *vulkanDevice, VkCommandBuffer copyCmd, std::string filename,
									 VkFormat format)
	{
#if defined(__ANDROID__)
		// Textures are stored inside the apk on Android (compressed)
		// So they need to be loaded via the asset manager
		AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
		assert(asset);
		size_t size = AAsset_getLength(asset);
		assert(size > 0);

		void *textureData = malloc(size);
		AAsset_read(asset, textureData, size);
		AAsset_close(asset);

		gli::texture_cube texCube(gli::load((const char*)textureData, size));
#else
		gli::texture_cube texCube(gli::load(filename));
#endif

		assert(!texCube.empty());

		cubeMap.device = vulkanDevice;

		cubeMap.width = texCube.extent().x;
		cubeMap.height = texCube.extent().y;
		cubeMap.mipLevels = texCube.levels();

		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
		bufferCreateInfo.size = texCube.size();
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();

		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, &stagingBuffer));

		VkMemoryRequirements memReqs;
		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &stagingMemory));
		VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

		// Copy texture data into staging buffer
		uint8_t *data;
		VK_CHECK_RESULT(vkMapMemory(device, stagingMemory, 0, memReqs.size, 0, (void **)&data));
		memcpy(data, texCube.data(), texCube.size());
		vkUnmapMemory(device, stagingMemory);

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = cubeMap.mipLevels;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { cubeMap.width, cubeMap.height, 1 };
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		// Cube faces count as array layers in Vulkan
		imageCreateInfo.arrayLayers = 6;
		// This flag is required for cube map images
		imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &cubeMap.image));

		vkGetImageMemoryRequirements(device, cubeMap.image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &cubeMap.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, cubeMap.image, cubeMap.deviceMemory, 0));

		//VkCommandBuffer copyCmd = VulkanExampleBase::createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// Setup buffer copy regions for each face including all of it's miplevels
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		uint32_t offset = 0;

		for (uint32_t face = 0; face < 6; face++)
		{
			for (uint32_t level = 0; level < cubeMap.mipLevels; level++)
			{
				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = level;
				bufferCopyRegion.imageSubresource.baseArrayLayer = face;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = texCube[face][level].extent().x;
				bufferCopyRegion.imageExtent.height = texCube[face][level].extent().y;
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;

				bufferCopyRegions.push_back(bufferCopyRegion);

				// Increase offset into staging buffer for next level / face
				offset += texCube[face][level].size();
			}
		}

		// Image barrier for optimal image (target)
		// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = cubeMap.mipLevels;
		subresourceRange.layerCount = 6;

		vks::tools::setImageLayout(
					copyCmd,
					cubeMap.image,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					subresourceRange);

		// Copy the cube map faces from the staging buffer to the optimal tiled image
		vkCmdCopyBufferToImage(
					copyCmd,
					stagingBuffer,
					cubeMap.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					static_cast<uint32_t>(bufferCopyRegions.size()),
					bufferCopyRegions.data()
					);

		// Change texture image layout to shader read after all faces have been copied
		cubeMap.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vks::tools::setImageLayout(
					copyCmd,
					cubeMap.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					cubeMap.imageLayout,
					subresourceRange);

	}

	void deleteStatingBuffer(VkDevice device) {
		// Clean up staging resources
		vkFreeMemory(device, stagingMemory, nullptr);
		vkDestroyBuffer(device, stagingBuffer, nullptr);
	}

	void createSampler(VkDevice device, vks::VulkanDevice *vulkanDevice) {
		// Create sampler
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = cubeMap.mipLevels;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		sampler.maxAnisotropy = 1.0f;

		// TODO: Validation fails
		if (false)
		// if (vulkanDevice->features.samplerAnisotropy)
		{
			printf("We think we have anisotropy %f. Feature enabled %d\n",
						 vulkanDevice->properties.limits.maxSamplerAnisotropy,
						 vulkanDevice->features.samplerAnisotropy);
			sampler.maxAnisotropy = vulkanDevice->properties.limits.maxSamplerAnisotropy;
			sampler.anisotropyEnable = VK_TRUE;
		}
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &cubeMap.sampler));
	}


	void createImageView(VkDevice device, VkFormat format) {
		// Create image view
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		// Cube map view type
		view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		view.format = format;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		// 6 array layers (faces)
		view.subresourceRange.layerCount = 6;
		// Set number of mip levels
		view.subresourceRange.levelCount = cubeMap.mipLevels;
		view.image = cubeMap.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &cubeMap.view));
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
