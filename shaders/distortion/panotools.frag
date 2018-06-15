#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable
#extension GL_ARB_shading_language_420pack : enable


layout (binding = 0) uniform sampler2D texSampler;

layout (binding = 1) uniform UBO 
{
  // Distoriton coefficients (PanoTools model) [a,b,c,d]
  vec4 HmdWarpParam;

  // chromatic distortion post scaling
  vec4 aberr;

  // Position of lens center in m (usually eye_w/2, eye_h/2)
  vec2 LensCenter[2];

  // Scale from texture co-ords to m (usually eye_w, eye_h)
  vec2 ViewportScale;

  // Distortion overall scale in m (usually ~eye_w/2)
  float WarpScale;
} ubo;


layout (location = 0) in vec2 inStereoUV;
layout (location = 1) in vec2 inMonoUV;
layout (location = 2) flat in int inViewIndex;

layout(location = 0) out vec4 outColor;

void main() {
	const int i = inViewIndex;

	vec2 r = inMonoUV * ubo.ViewportScale - ubo.LensCenter[inViewIndex];

  // scale for distortion model
  // distortion model has r=1 being the largest circle inscribed (e.g. eye_w/2)
  r /= ubo.WarpScale;

  // |r|**2
  float r_mag = length(r);
    
  // offset for which fragment is sourced
  vec2 r_displaced = r * (
    ubo.HmdWarpParam.w + 
    ubo.HmdWarpParam.z * r_mag +
	  ubo.HmdWarpParam.y * r_mag * r_mag +
	  ubo.HmdWarpParam.x * r_mag * r_mag * r_mag);

  // back to world scale
  r_displaced *= ubo.WarpScale;

  const vec2 stereoViewportScale = ubo.ViewportScale * vec2(2.f, 1.f);

  // back to viewport co-ord
  vec2 tcR = (ubo.LensCenter[inViewIndex] + ubo.aberr.r * r_displaced) / stereoViewportScale;
  vec2 tcG = (ubo.LensCenter[inViewIndex] + ubo.aberr.g * r_displaced) / stereoViewportScale;
  vec2 tcB = (ubo.LensCenter[inViewIndex] + ubo.aberr.b * r_displaced) / stereoViewportScale;

  if (inViewIndex == 1) {
     tcR += vec2(0.5,0);
     tcG += vec2(0.5,0);
     tcB += vec2(0.5,0);
  }

	vec3 color = vec3(
	      texture(texSampler, tcR).r,
			  texture(texSampler, tcG).g,
			  texture(texSampler, tcB).b);
 
  // distortion cuttoff   
  if (tcG.x < 0.0 || tcG.x > 1.0 || tcG.y < 0.0 || tcG.y > 1.0)
	  color *= 0.125;

  // stereo cutoff  
  if ((inViewIndex == 0 && tcG.x > 0.5) || (inViewIndex == 1 && tcG.x < 0.5))
    color *= 0;
    
  outColor = vec4(color, 1.0);
}
