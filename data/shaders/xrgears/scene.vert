#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 normal;
	mat4 view;
	vec3 lightpos;
} ubo;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	outColor = inColor;
	outNormal = inNormal;
	//gl_Position = vec4(inPos.xyz, 1.0);	
	
	gl_Position = ubo.model * vec4(inPos,1);
}
