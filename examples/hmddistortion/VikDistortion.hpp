#include <vulkan/vulkan.h>

#include "VulkanModel.hpp"

#define VERTEX_BUFFER_BIND_ID 0

class VikDistortion{
public:
    vks::Model quad;

    VikDistortion() {

    }

    ~VikDistortion() {
	quad.destroy();
    }

    void drawQuad(VkCommandBuffer& commandBuffer, VkPipeline& hmdWarp) {
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmdWarp);
	vkCmdBindVertexBuffers(commandBuffer, VERTEX_BUFFER_BIND_ID, 1, &quad.vertices.buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, quad.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(commandBuffer, quad.indexCount, 1, 0, 0, 1);
    }

    void generateQuads(vks::VulkanDevice *vulkanDevice, VkDevice& device)
    {
      // Setup vertices for multiple screen aligned quads
      // Used for displaying final result and debug
      struct Vertex {
	float pos[3];
	float uv[2];
      };

      std::vector<Vertex> vertexBuffer;

      float x = 0.0f;
      float y = 0.0f;
      for (uint32_t i = 0; i < 3; i++)
      {
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
};
