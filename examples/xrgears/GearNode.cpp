/*
* Vulkan Example - Animated gears using multiple uniform buffers
*
* See readme.md for details
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "GearNode.h"

GearNode::~GearNode()
{
	uniformBuffer.destroy();
}

void GearNode::generate(GearNodeInfo *gearNodeinfo, GearInfo *gearinfo, VkQueue queue)
{
//	this->color = gearinfo->color;
	this->pos = gearNodeinfo->pos;
	this->rotOffset = gearNodeinfo->rotOffset;
	this->rotSpeed = gearNodeinfo->rotSpeed;
	this->material = gearNodeinfo->material;

	gear.generate(vulkanDevice, gearinfo, queue);
}

void GearNode::draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout)
{
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
	vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &gear.vertexBuffer.buffer, offsets);
	vkCmdBindIndexBuffer(cmdbuffer, gear.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdPushConstants(cmdbuffer,
										 pipelineLayout,
										 VK_SHADER_STAGE_FRAGMENT_BIT,
										 sizeof(glm::vec3),
										 sizeof(Material::PushBlock), &material);

	vkCmdDrawIndexed(cmdbuffer, gear.indexCount, 1, 0, 0, 1);
}

void GearNode::updateUniformBuffer(StereoView sv, float timer)
{
	ubo.model = glm::mat4();

	ubo.model = glm::translate(ubo.model, pos);
	float rotation_z = (rotSpeed * timer * 360.0f) + rotOffset;
	ubo.model = glm::rotate(ubo.model, glm::radians(rotation_z), glm::vec3(0.0f, 0.0f, 1.0f));

	/*
	*/
	ubo.normal[0] = glm::inverseTranspose(sv.view[0] * ubo.model);
	ubo.normal[1] = glm::inverseTranspose(sv.view[1] * ubo.model);
	memcpy(uniformBuffer.mapped, &ubo, sizeof(ubo));
}

void GearNode::prepareUniformBuffer()
{
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&uniformBuffer,
		sizeof(ubo)));
	// Map persistent
	VK_CHECK_RESULT(uniformBuffer.map());
}

