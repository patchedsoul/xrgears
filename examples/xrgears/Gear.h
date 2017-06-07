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

	~Gear();
	int32_t newVertex(std::vector<Vertex> *vBuffer, float x, float y, float z, const glm::vec3& normal);
	void newFace(std::vector<uint32_t> *iBuffer, int a, int b, int c);
	void generate(vks::VulkanDevice *vulkanDevice, GearInfo *gearinfo, VkQueue queue);
};
