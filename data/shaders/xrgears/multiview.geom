#version 450

#extension GL_ARB_viewport_array : enable

layout (triangles, invocations = 2) in;
layout (triangle_strip, max_vertices = 3) out;


layout (binding = 0) uniform UBOMatrices
{
	//mat4 normal[2];
	mat4 model;
} uboModel;

layout (binding = 2) uniform UBOCamera {
	mat4 projection[2];
	mat4 view[2];
	vec3 position;
} uboCamera;

layout (location = 0) in vec3 inNormal[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outWorldPos;

void main(void)
{	
	for(int i = 0; i < gl_in.length(); i++)
	{
		outNormal = mat3(uboModel.model) * inNormal[i];

		vec4 worldPos = uboModel.model * gl_in[i].gl_Position;
		outWorldPos = worldPos.xyz;
	
		gl_Position = uboCamera.projection[gl_InvocationID] * uboCamera.view[gl_InvocationID] * worldPos;

		// Set the viewport index that the vertex will be emitted to
		gl_ViewportIndex = gl_InvocationID;

		EmitVertex();
	}
	EndPrimitive();
}
