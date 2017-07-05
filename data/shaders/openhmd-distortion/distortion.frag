#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform sampler2D warpTexture;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform sampler2D samplerAlbedo;

layout (binding = 4) uniform UBO 
{
  //per eye texture to warp for lens distortion

  //Position of lens center in m (usually eye_w/2, eye_h/2)
  uniform vec2 LensCenter;
  //Scale from texture co-ords to m (usually eye_w, eye_h)
  uniform vec2 ViewportScale;
  //Distortion overall scale in m (usually ~eye_w/2)
  uniform float WarpScale;
  //Distoriton coefficients (PanoTools model) [a,b,c,d]
  uniform vec4 HmdWarpParam;

  //chromatic distortion post scaling
  uniform vec3 aberr;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragcolor;





void main()
{

    //output_loc is the fragment location on screen from [0,1]x[0,1]
    //vec2 output_loc = vec2(gl_TexCoord[0].s, gl_TexCoord[0].t);
    //Compute fragment location in lens-centered co-ordinates at world scale
	  vec2 r = inUV * ubo.ViewportScale - ubo.LensCenter;
    //scale for distortion model
    //distortion model has r=1 being the largest circle inscribed (e.g. eye_w/2)

    r /= ubo.WarpScale;
    //|r|**2
    float r_mag = length(r);
    //offset for which fragment is sourced
    vec2 r_displaced = r * (ubo.HmdWarpParam.w + ubo.HmdWarpParam.z * r_mag +
		  ubo.HmdWarpParam.y * r_mag * r_mag +
		  ubo.HmdWarpParam.x * r_mag * r_mag * r_mag);
    //back to world scale
    r_displaced *= ubo.WarpScale;
    //back to viewport co-ord
    vec2 tc_r = (ubo.LensCenter + ubo.aberr.r * r_displaced) / ubo.ViewportScale;
    vec2 tc_g = (ubo.LensCenter + ubo.aberr.g * r_displaced) / ubo.ViewportScale;
    vec2 tc_b = (ubo.LensCenter + ubo.aberr.b * r_displaced) / ubo.ViewportScale;

    float red = texture(warpTexture, tc_r).r;
    float green = texture(warpTexture, tc_g).g;
    float blue = texture(warpTexture, tc_b).b;
    //Black edges off the texture
    outFragcolor = ((tc_g.x < 0.0) || (tc_g.x > 1.0) || (tc_g.y < 0.0) || (tc_g.y > 1.0)) ? vec4(0.0, 0.0, 0.0, 1.0) : vec4(red, green, blue, 1.0);
    
    
    outFragcolor = texture(warpTexture, inUV);
}
