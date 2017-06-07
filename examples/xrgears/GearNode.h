/*
* Vulkan Example - Animated gears using multiple uniform buffers
*
* See readme.md for details
*
* Copyright (C) 2015 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <math.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "vulkan/vulkan.h"

#include "VulkanTools.h"

#include "VulkanBuffer.hpp"
#include "camera.hpp"
#include "uniformbuffers.h"
#include "Gear.h"

struct GearNodeInfo
{
	glm::vec3 pos;
	float rotSpeed;
	float rotOffset;
	Material material;
};

class GearNode
{
private:

	Gear gear;

	struct UBO
	{
		glm::mat4 normal[2];
		glm::mat4 model;
	};
	UBO ubo;

	vks::VulkanDevice *vulkanDevice;

	glm::vec3 pos;
	float rotSpeed;
	float rotOffset;

public:
		Material material;

	void prepareUniformBuffer();
	VkDescriptorSet descriptorSet;
	vks::Buffer uniformBuffer;

	void draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout);
	void updateUniformBuffer(StereoView sv, float timer);

	void setupDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, std::vector<VkWriteDescriptorSet> *writeDescriptorSets);

	GearNode(vks::VulkanDevice *vulkanDevice) : vulkanDevice(vulkanDevice) {}
	~GearNode();

	void generate(GearNodeInfo *gearNodeinfo, GearInfo *gearinfo, VkQueue queue);

	glm::mat4 getModelMatrix(glm::vec3 rotation, float timer);

};

