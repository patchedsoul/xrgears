#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable
#extension GL_ARB_shading_language_420pack : enable

	/*
layout (binding = 2) uniform UBO {
	vec4 center[2];
	 * center[0].z : aspect_x_over_y
	 * center[0].w : undistort_r2_cutoff[0]
	 * center[1].z : grow_for_undistort
	 * center[1].w : undistort_r2_cutoff[1]
	vec3 coeffs[2][3];
} ubo;
	 */


vec4 center[2] = vec4[](
  vec4(0.08946027017045266,-0.009002181016260827, 0.8999999761581421, 1.11622154712677),
  vec4(-0.08933516629552526,-0.006014565287238661, 0.6000000238418579, 1.101870775222778)
);

vec3 coeffs[2][3] = {
{
  //left
  vec3(-0.188236068524731, -0.221086205321053, -0.2537849057915209),
  //left blue
  vec3(-0.07316590815739493, -0.02332400789561968, 0.02469959434698275),
  // left red
  vec3(-0.02223805567703767, -0.04931309279533211, -0.07862881939243466),
},
{
  //right
  vec3(-0.1906209981894497, -0.2248896677207884, -0.2721364516782803),
  // right blue
  vec3(-0.07346071902951497, -0.02189527566250131, 0.0581378652359256),
  //right red
  vec3(-0.01755850332081247, -0.04517245633373419, -0.0928909347763)
}
};

//layout(location = 0) in vec3 fragNormal;
//layout(location = 2) in vec2 fragTexCoord;

layout (binding = 0) uniform sampler2D texSampler;

layout (binding = 1) uniform UBO 
{
  //Distoriton coefficients (PanoTools model) [a,b,c,d]
  vec4 HmdWarpParam;

  //chromatic distortion post scaling
  vec4 aberr;

  //Position of lens center in m (usually eye_w/2, eye_h/2)
  vec2 LensCenter;

  //Scale from texture co-ords to m (usually eye_w, eye_h)
  vec2 ViewportScale;

  //Distortion overall scale in m (usually ~eye_w/2)
  float WarpScale;
} ubo;

layout (location = 0) in vec2 inStereoUV;
layout (location = 1) in vec2 inMonoUV;
layout (location = 2) flat in int inViewIndex;

layout(location = 0) out vec4 outColor;

void main() {
	const int i = inViewIndex;
	const float aspect_x_over_y = center[0].z;
	const float aspect_y_over_x = 1.0 / aspect_x_over_y;
	const float grow_for_undistort = center[1].z;
	const float undistort_r2_cutoff = center[i].w;
	const vec2 center_2d = center[i].xy;
	//const vec2 factor = 0.5 / (1.0 + grow_for_undistort) * vec2(1.0, aspect_x_over_y);
	const vec2 factor = 0.5 / (1.0 + grow_for_undistort) * vec2(0.5, aspect_x_over_y);
	vec2 texCoord = 2.0 * inMonoUV - vec2(1.0);

//vec2 texCoord = inStereoUV;	
	
	texCoord.y *= aspect_y_over_x;
	texCoord -= center_2d;
	float r2 = texCoord.x * texCoord.x + texCoord.y * texCoord.y;

	//if (r2 > undistort_r2_cutoff)
	//	outColor = vec4(0.0, 0.0, 0.0, 1.0);
	//else {
		const vec3 d = 1.0 / (((r2 * coeffs[i][2] + coeffs[i][1]) * r2 + coeffs[i][0]) * r2 + vec3(1.0));
	//	const vec2 offset = vec2(0.5);
		const vec2 offset = vec2(0.25 + i * 0.5, 0.5);
		vec2 tcFoo = offset + (texCoord + center_2d) * factor;
		vec2 tcR = offset + (texCoord * d.r + center_2d) * factor;
		vec2 tcG = offset + (texCoord * d.g + center_2d) * factor;
		vec2 tcB = offset + (texCoord * d.b + center_2d) * factor;
		vec3 color = vec3(texture(texSampler, tcR).r,
				  texture(texSampler, tcG).g,
				  texture(texSampler, tcB).b);
		if (r2 > undistort_r2_cutoff)
			outColor = vec4(0.125 * color, 1.0);
		else
			outColor = vec4(color, 1.0);
	
	//}

}
