#include <glm/glm.hpp>

#include "VulkanBuffer.hpp"
#include "VulkanDevice.hpp"

struct Vertex
{
  float pos[3];
  float normal[3];
  float color[3];

  Vertex(const glm::vec3& p, const glm::vec3& n, const glm::vec3& c)
  {
    pos[0] = p.x;
    pos[1] = p.y;
    pos[2] = p.z;
    color[0] = c.x;
    color[1] = c.y;
    color[2] = c.z;
    normal[0] = n.x;
    normal[1] = n.y;
    normal[2] = n.z;
  }
};

struct GearInfo
{
  float innerRadius;
  float outerRadius;
  float width;
  int numTeeth;
  float toothDepth;
};

class Gear
{
public:

  vks::Buffer vertexBuffer;
  vks::Buffer indexBuffer;
  uint32_t indexCount;

  ~Gear() {
    // Clean up vulkan resources
    vertexBuffer.destroy();
    indexBuffer.destroy();
  }

  int32_t newVertex(std::vector<Vertex> *vBuffer, float x, float y, float z, const glm::vec3& normal)
  {
    // TODO: remove color
    Vertex v(glm::vec3(x, y, z), normal, glm::vec3());
    vBuffer->push_back(v);
    return static_cast<int32_t>(vBuffer->size()) - 1;
  }

  void newFace(std::vector<uint32_t> *iBuffer, int a, int b, int c)
  {
    iBuffer->push_back(a);
    iBuffer->push_back(b);
    iBuffer->push_back(c);
  }

  void generate(vks::VulkanDevice *vulkanDevice, GearInfo *gearinfo, VkQueue queue)
  {
    std::vector<Vertex> vBuffer;
    std::vector<uint32_t> iBuffer;

    int i, j;
    float r0, r1, r2;
    float ta, da;
    float u1, v1, u2, v2, len;
    float cos_ta, cos_ta_1da, cos_ta_2da, cos_ta_3da, cos_ta_4da;
    float sin_ta, sin_ta_1da, sin_ta_2da, sin_ta_3da, sin_ta_4da;
    int32_t ix0, ix1, ix2, ix3, ix4, ix5;

    r0 = gearinfo->innerRadius;
    r1 = gearinfo->outerRadius - gearinfo->toothDepth / 2.0f;
    r2 = gearinfo->outerRadius + gearinfo->toothDepth / 2.0f;
    da = 2.0f * M_PI / gearinfo->numTeeth / 4.0f;

    glm::vec3 normal;

    for (i = 0; i < gearinfo->numTeeth; i++)
    {
      ta = i * 2.0f * M_PI / gearinfo->numTeeth;

      cos_ta = cos(ta);
      cos_ta_1da = cos(ta + da);
      cos_ta_2da = cos(ta + 2.0f * da);
      cos_ta_3da = cos(ta + 3.0f * da);
      cos_ta_4da = cos(ta + 4.0f * da);
      sin_ta = sin(ta);
      sin_ta_1da = sin(ta + da);
      sin_ta_2da = sin(ta + 2.0f * da);
      sin_ta_3da = sin(ta + 3.0f * da);
      sin_ta_4da = sin(ta + 4.0f * da);

      u1 = r2 * cos_ta_1da - r1 * cos_ta;
      v1 = r2 * sin_ta_1da - r1 * sin_ta;
      len = sqrt(u1 * u1 + v1 * v1);
      u1 /= len;
      v1 /= len;
      u2 = r1 * cos_ta_3da - r2 * cos_ta_2da;
      v2 = r1 * sin_ta_3da - r2 * sin_ta_2da;

      // front face
      normal = glm::vec3(0.0f, 0.0f, 1.0f);
      ix0 = newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta, gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta, gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta, gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, gearinfo->width * 0.5f, normal);
      ix4 = newVertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da, gearinfo->width * 0.5f, normal);
      ix5 = newVertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da, gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);
      newFace(&iBuffer, ix2, ix3, ix4);
      newFace(&iBuffer, ix3, ix5, ix4);

      // front sides of teeth
      normal = glm::vec3(0.0f, 0.0f, 1.0f);
      ix0 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta, gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      // back face
      normal = glm::vec3(0.0f, 0.0f, -1.0f);
      ix0 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta, -gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta, -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, -gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta, -gearinfo->width * 0.5f, normal);
      ix4 = newVertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da, -gearinfo->width * 0.5f, normal);
      ix5 = newVertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da, -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);
      newFace(&iBuffer, ix2, ix3, ix4);
      newFace(&iBuffer, ix3, ix5, ix4);

      // back sides of teeth
      normal = glm::vec3(0.0f, 0.0f, -1.0f);
      ix0 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, -gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta, -gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      // draw outward faces of teeth
      normal = glm::vec3(v1, -u1, 0.0f);
      ix0 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta, gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r1 * cos_ta, r1 * sin_ta, -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      normal = glm::vec3(cos_ta, sin_ta, 0.0f);
      ix0 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r2 * cos_ta_1da, r2 * sin_ta_1da, -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      normal = glm::vec3(v2, -u2, 0.0f);
      ix0 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r2 * cos_ta_2da, r2 * sin_ta_2da, -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      normal = glm::vec3(cos_ta, sin_ta, 0.0f);
      ix0 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, gearinfo->width * 0.5f, normal);
      ix1 = newVertex(&vBuffer, r1 * cos_ta_3da, r1 * sin_ta_3da, -gearinfo->width * 0.5f, normal);
      ix2 = newVertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da, gearinfo->width * 0.5f, normal);
      ix3 = newVertex(&vBuffer, r1 * cos_ta_4da, r1 * sin_ta_4da, -gearinfo->width * 0.5f, normal);
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);

      // draw inside radius cylinder
      ix0 = newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta, -gearinfo->width * 0.5f, glm::vec3(-cos_ta, -sin_ta, 0.0f));
      ix1 = newVertex(&vBuffer, r0 * cos_ta, r0 * sin_ta, gearinfo->width * 0.5f, glm::vec3(-cos_ta, -sin_ta, 0.0f));
      ix2 = newVertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da, -gearinfo->width * 0.5f, glm::vec3(-cos_ta_4da, -sin_ta_4da, 0.0f));
      ix3 = newVertex(&vBuffer, r0 * cos_ta_4da, r0 * sin_ta_4da, gearinfo->width * 0.5f, glm::vec3(-cos_ta_4da, -sin_ta_4da, 0.0f));
      newFace(&iBuffer, ix0, ix1, ix2);
      newFace(&iBuffer, ix1, ix3, ix2);
    }

    size_t vertexBufferSize = vBuffer.size() * sizeof(Vertex);
    size_t indexBufferSize = iBuffer.size() * sizeof(uint32_t);

    bool useStaging = true;

    if (useStaging)
    {
      vks::Buffer vertexStaging, indexStaging;

      // Create staging buffers
      // Vertex data
      vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &vertexStaging,
            vertexBufferSize,
            vBuffer.data());
      // Index data
      vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &indexStaging,
            indexBufferSize,
            iBuffer.data());

      // Create device local buffers
      // Vertex buffer
      vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &vertexBuffer,
            vertexBufferSize);
      // Index buffer
      vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &indexBuffer,
            indexBufferSize);

      // Copy from staging buffers
      VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

      VkBufferCopy copyRegion = {};

      copyRegion.size = vertexBufferSize;
      vkCmdCopyBuffer(
            copyCmd,
            vertexStaging.buffer,
            vertexBuffer.buffer,
            1,
            &copyRegion);

      copyRegion.size = indexBufferSize;
      vkCmdCopyBuffer(
            copyCmd,
            indexStaging.buffer,
            indexBuffer.buffer,
            1,
            &copyRegion);

      vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

      vkDestroyBuffer(vulkanDevice->logicalDevice, vertexStaging.buffer, nullptr);
      vkFreeMemory(vulkanDevice->logicalDevice, vertexStaging.memory, nullptr);
      vkDestroyBuffer(vulkanDevice->logicalDevice, indexStaging.buffer, nullptr);
      vkFreeMemory(vulkanDevice->logicalDevice, indexStaging.memory, nullptr);
    }
    else
    {
      // Vertex buffer
      vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &vertexBuffer,
            vertexBufferSize,
            vBuffer.data());
      // Index buffer
      vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            &indexBuffer,
            indexBufferSize,
            iBuffer.data());
    }

    indexCount = iBuffer.size();
  }
};
