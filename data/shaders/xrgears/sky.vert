#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	gl_Position = vec4(inPos.xyz, 1.0);	
}
