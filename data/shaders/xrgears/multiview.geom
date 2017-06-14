#version 450

#extension GL_ARB_viewport_array : enable

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#extension GL_ARB_shader_viewport_layer_array : require
#extension GL_NV_viewport_array2 : require


layout (triangles, invocations = 2) in;
layout (triangle_strip, max_vertices = 3) out;


layout (binding = 0) uniform UBOMatrices
{
	mat4 normal[2];
	mat4 model;
} uboModel;

layout (binding = 2) uniform UBOCamera {
	mat4 projection[2];
	mat4 view[2];
	mat4 skyView[2];
	vec3 position;
} uboCamera;

layout (location = 0) in vec3 inNormal[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outWorldPos;

layout (location = 2) out vec3 outViewPos;
layout (location = 3) out mat4 outInvModelView;

layout (location = 10) out vec3 outViewNormal;

void main(void)
{	
	for(int i = 0; i < gl_in.length(); i++)
	{
		outNormal = mat3(uboModel.model) * inNormal[i];
		
		outViewNormal = (uboModel.normal[gl_InvocationID] * vec4(inNormal[i],1)).xyz;

		vec4 worldPos = uboModel.model * gl_in[i].gl_Position;
		outWorldPos = worldPos.xyz;
		
		mat4 modelView = uboCamera.view[gl_InvocationID] * uboModel.model;
		
		outViewPos = (modelView * gl_in[i].gl_Position).xyz;
		
		outInvModelView = inverse(uboCamera.view[gl_InvocationID]);
	
		gl_Position = uboCamera.projection[gl_InvocationID] * modelView * gl_in[i].gl_Position;

		// Set the viewport index that the vertex will be emitted to
		gl_ViewportIndex = gl_InvocationID;
		
		// VK 1.0: write in GS, read in FS
    // GL_ARB_shader_viewport_layer_array: write in VS, TS, GS
    //gl_ViewportIndex = 2;
    // GL_NV_viewport_array2: write in VS, TS, GS
    //gl_ViewportMask[0] = 0x07; // viewports 0, 1 and 2


		EmitVertex();
	}
	EndPrimitive();
}
