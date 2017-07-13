#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

// per eye texture to warp for lens distortion
layout (binding = 0) uniform sampler2D warpTexture;

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

layout (location = 0) out vec4 outFragcolor;

void main()
{
    // output_loc is the fragment location on screen from [0,1]x[0,1]
    //vec2 output_loc = vec2(gl_TexCoord[0].s, gl_TexCoord[0].t);
    // Compute fragment location in lens-centered co-ordinates at world scale
    
    //vec2 texCoord = 2.0 * inMonoUV - vec2(1.0);
    
    //const vec2 offset2 = vec2(0.25 + inViewIndex * 0.5, 0.5);
    
	  vec2 r = inMonoUV * ubo.ViewportScale - ubo.LensCenter[inViewIndex];
	  
	      //const vec2 offset = vec2(0.25 + inViewIndex * 0.5, 0.5);

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

    //const vec2 offset = vec2(0.25 + inViewIndex * 0.5, 0.5);
    
    const vec2 offset = vec2(0);

    // back to viewport co-ord
    vec2 tc_r = offset + (ubo.LensCenter[inViewIndex] + ubo.aberr.r * r_displaced) / ubo.ViewportScale;
    vec2 tc_g = offset + (ubo.LensCenter[inViewIndex] + ubo.aberr.g * r_displaced) / ubo.ViewportScale;
    vec2 tc_b = offset + (ubo.LensCenter[inViewIndex] + ubo.aberr.b * r_displaced) / ubo.ViewportScale;
    
    vec3 color = vec3(
      texture(warpTexture, tc_r).r,
			texture(warpTexture, tc_g).g,
			texture(warpTexture, tc_b).b);
    
    // Black edges off the texture
    if (tc_g.x < 0.0 || tc_g.x > 1.0 || tc_g.y < 0.0 || tc_g.y > 1.0)
      color = vec3(0);
      
    outFragcolor =  vec4(color, 1.0);
    
    
    outFragcolor =  texture(warpTexture, r);
    
    //outFragcolor = vec4(inStereoUV, 0, 1);
    //outFragcolor = texture(warpTexture, inUV);
}
