#pragma once

#include "../vks/vksModel.hpp"

#include "VikMaterial.hpp"
#include "VikAssets.hpp"


class VikNode {

public:

    struct UBO
    {
      glm::mat4 normal[2];
      glm::mat4 model;
    };
    UBO ubo;

    vks::Model model;
    Material material;
    VkDescriptorSet descriptorSet;

    glm::vec3 pos;
    float rotSpeed;
    float rotOffset;

    vks::Buffer uniformBuffer;

    VikNode() {

    }
    ~VikNode() {
	model.destroy();
	uniformBuffer.destroy();
    }

    void loadModel(const std::string& name, vks::VertexLayout layout, float scale, vks::VulkanDevice *device, VkQueue queue) {
	model.loadFromFile(
	            VikAssets::getAssetPath() + "models/" + name,
	            layout,
	            scale,
	            device,
	            queue);
    }

    void draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout) {
	vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &model.vertices.buffer, offsets);
	vkCmdBindIndexBuffer(cmdbuffer, model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdPushConstants(cmdbuffer,
	                   pipelineLayout,
	                   VK_SHADER_STAGE_FRAGMENT_BIT,
	                   sizeof(glm::vec3),
	                   sizeof(Material::PushBlock), &material);
	vkCmdDrawIndexed(cmdbuffer, model.indexCount, 1, 0, 0, 0);
    }

    void updateUniformBuffer(StereoView sv, float timer) {
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

    void prepareUniformBuffer(vks::VulkanDevice *vulkanDevice) {
      VK_CHECK_RESULT(vulkanDevice->createBuffer(
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        &uniformBuffer,
                        sizeof(ubo)));
      // Map persistent
      VK_CHECK_RESULT(uniformBuffer.map());
    }
};
