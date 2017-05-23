#version 450

#extension GL_ARB_viewport_array : enable

layout (triangles, invocations = 2) in;
layout (triangle_strip, max_vertices = 3) out;


layout (binding = 0) uniform UBOMatrices
{
	//mat4 projection[2];
	//mat4 view[2];
	mat4 normal[2];
	mat4 model;
	//vec4 lightPos;
} matrices;

layout (binding = 1) uniform UBOLights {
	vec4 lights[4];
} uboLights;

layout (binding = 2) uniform UBOCamera {
	mat4 projection[2];
	mat4 view[2];
	vec3 position;
} uboCamera;

layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec3 inColor[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;

layout (location = 4) out vec3 outWorldPos;

void main(void)
{	
	for(int i = 0; i < gl_in.length(); i++)
	{
		mat4 modelview = uboCamera.view[gl_InvocationID] * matrices.model;

		outNormal = mat3(matrices.normal[gl_InvocationID]) * inNormal[i];
		
		//outNormal = inNormal[i];
		
		outColor = inColor[i];

		vec4 pos = gl_in[i].gl_Position;
		vec4 worldPos = (modelview * pos);
		outWorldPos = worldPos.xyz;
		
		vec3 lPos = vec3(uboCamera.view[gl_InvocationID] * uboLights.lights[0]);
		outLightVec = lPos - worldPos.xyz;
		outViewVec = -worldPos.xyz;	
	
		gl_Position = uboCamera.projection[gl_InvocationID] * worldPos;

		// Set the viewport index that the vertex will be emitted to
		gl_ViewportIndex = gl_InvocationID;

		EmitVertex();
	}
	EndPrimitive();
}
