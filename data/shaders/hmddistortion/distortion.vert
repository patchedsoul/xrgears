#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) out vec2 outUV;

vec2 positions[4] = vec2[](
  vec2(-1.0, -1.0), // LT
  vec2( 1.0, -1.0), // RT
  vec2(-1.0,  1.0), // LB
  vec2( 1.0,  1.0)  // RB
);

vec2 uvs[4] = vec2[](
  vec2(0.0, 0.0),
  vec2(1.0, 0.0), 
  vec2(0.0, 1.0),
  vec2(1.0, 1.0)
);


out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	//outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	//gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
	
  outUV = uvs[gl_VertexIndex];
	gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}
