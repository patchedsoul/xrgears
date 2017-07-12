#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) out vec2 outStereoUVs;
layout (location = 1) out vec2 outMonoUVs;

layout (location = 2) out int outViewIndex;

vec2 positions[12] = vec2[](
  vec2(-1.0, -1.0), // LT
  vec2(0, -1.0), // RT
  vec2(-1.0,  1.0), // LB

  vec2(0, -1.0), // RT
  vec2(0,  1.0), // RB
  vec2(-1.0,  1.0), // LB

  vec2(0, -1.0), // LT
  vec2(1.0, -1.0), // RT
  vec2(0,  1.0), // LB

  vec2(1.0, -1.0), // RT
  vec2(1.0,  1.0), // RB
  vec2(0,  1.0) // LB
);


vec2 mono_uvs[12] = vec2[](
  vec2(0.0, 0.0),
  vec2(1.0, 0.0), 
  vec2(0.0, 1.0),
  
  vec2(1.0, 0.0),
  vec2(1.0, 1.0),
  vec2(0.0, 1.0),

  vec2(0.0, 0.0),
  vec2(1.0, 0.0), 
  vec2(0.0, 1.0),
  
  vec2(1.0, 0.0),
  vec2(1.0, 1.0),
  vec2(0.0, 1.0)
);

vec2 stereo_uvs[12] = vec2[](
  vec2(0.0, 0.0),
  vec2(.5, 0.0), 
  vec2(0.0, 1.0),
  
  vec2(.5, 0.0),
  vec2(.5, 1.0),
  vec2(0.0, 1.0),

  vec2(.5, 0.0),
  vec2(1.0, 0.0), 
  vec2(.5, 1.0),
  
  vec2(1.0, 0.0),
  vec2(1.0, 1.0),
  vec2(.5, 1.0)
);

int view_indices[12] = int[](
  0,
  0,
  0,
  0,
  0,
  0,
  1,
  1,
  1,
  1,
  1,
  1
);

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	//outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	//gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
	
  outViewIndex = view_indices[gl_VertexIndex];

  outStereoUVs = stereo_uvs[gl_VertexIndex];
  outMonoUVs = mono_uvs[gl_VertexIndex];
	gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}
