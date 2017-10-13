#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_EXT_multiview : enable


layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inWorldPos;

layout (location = 2) in vec3 inViewPos;
layout (location = 3) in mat4 inInvModelView;

layout (location = 10) in vec3 inViewNormal;
layout (location = 11) in flat int inViewPortIndex;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform PushConsts {
	layout(offset = 12) float roughness;
	layout(offset = 16) float metallic;
	layout(offset = 20) float r;
	layout(offset = 24) float g;
	layout(offset = 28) float b;
} material;

layout (binding = 1) uniform UBOLights {
	vec4 lights[4];
} uboLights;

layout (binding = 2) uniform UBOCamera {
	mat4 projection[2];
	mat4 view[2];
	mat4 skyView[2];
	vec3 position;
} uboCamera;

layout (binding = 3) uniform samplerCube samplerCubeMap;


const float PI = 3.14159265359;

//#define ROUGHNESS_PATTERN 1

vec3 materialcolor() {
	return vec3(material.r, material.g, material.b);
}

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness) {
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, float metallic, vec3 reflectionColor) {
	vec3 F0 = mix(vec3(0.04) * reflectionColor, materialcolor(), metallic) ;
	vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;    
}

// Specular BRDF composition --------------------------------------------

vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness, vec3 reflectionColor) {
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = vec3(1.0);
	//vec3 lightColor = reflectionColor;

	vec3 color = vec3(0.0);

	if (dotNL > 0.0)
	{
		float rroughness = max(0.05, roughness);
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, metallic, reflectionColor);

		vec3 spec = D * F * G / (4.0 * dotNL * dotNV);

		color += spec * dotNL * lightColor;
		
		//color = F;
	}

	return color;
}

// ----------------------------------------------------------------------------
void main() {		  
	vec3 N = normalize(inNormal);
	//vec3 N = normalize(inViewNormal);
	vec3 V = normalize(uboCamera.position - inWorldPos);

	float roughness = material.roughness;

	// Add striped pattern to roughness based on vertex position
#ifdef ROUGHNESS_PATTERN
	roughness = max(roughness, step(fract(inWorldPos.y * 2.02), 0.5));
#endif


  // reflection mapping
	vec3 cI = normalize (inViewPos);
	vec3 cR = reflect (cI, inViewNormal);

	cR = vec3(inInvModelView * vec4(cR, 0.0));
	cR.x *= -1.0;
	vec3 reflectionColor = texture(samplerCubeMap, cR, 1.0).rgb;

	// Specular contribution
	vec3 Lo = vec3(0.0);
	for (int i = 0; i < uboLights.lights.length(); i++) {
		vec3 L = normalize(uboLights.lights[i].xyz - inWorldPos);
		Lo += BRDF(L, V, N, material.metallic, roughness, reflectionColor);
	};
	
	// Combine with ambient
	vec3 color = materialcolor() * 0.02;
	//color += 0.1 * reflectionColor;
	color += Lo;

	// Gamma correct
	color = pow(color, vec3(0.4545));

  outColor = vec4(color, 1.0);

	//outColor = inViewPortIndex * vec4(color, 1.0); //+ 0.1* vec4(reflectionColor, 1);

  // debug
 // outColor = vec4(L, 1.0);

}
