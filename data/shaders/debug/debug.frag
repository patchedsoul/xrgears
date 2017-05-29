#version 450

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform PushConsts {
	layout(offset = 12) float roughness;
	layout(offset = 16) float metallic;
	layout(offset = 20) float r;
	layout(offset = 24) float g;
	layout(offset = 28) float b;
} material;

layout (binding = 1) uniform UBOLights {
	vec4 lights[4];
} uboLights;

layout (binding = 2) uniform UBOCamera {
	mat4 projection[2];
	mat4 view[2];
	vec3 position;
} uboCamera;

layout (binding = 3) uniform samplerCube samplerCubeMap;

void main() {		  
	outColor = texture(samplerCubeMap, inUVW);
}
