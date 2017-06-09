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
	mat4 skyView[2];
	vec3 position;
} uboCamera;


layout (location = 0) in vec3 inNormal[];

layout (location = 0) out vec3 outUVW;


void main(void)
{	
	for(int i = 0; i < gl_in.length(); i++)
	{
		outUVW = gl_in[i].gl_Position.xyz;
	  //outUVW.x *= -1.0;
		gl_Position = uboCamera.projection[gl_InvocationID] * uboCamera.skyView[gl_InvocationID] * gl_in[i].gl_Position;

		// Set the viewport index that the vertex will be emitted to
		gl_ViewportIndex = gl_InvocationID;

		EmitVertex();
	}
	EndPrimitive();
}
