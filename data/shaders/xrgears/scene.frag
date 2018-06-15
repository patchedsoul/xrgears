#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inViewVec;
layout (location = 3) in vec3 inLightVec;

layout (location = 4) in vec3 inWorldPos;

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
	vec3 position;
} uboCamera;

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
vec3 F_Schlick(float cosTheta, float metallic) {
	vec3 F0 = mix(vec3(0.04), materialcolor(), metallic); // * material.specular
	vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;    
}

// Specular BRDF composition --------------------------------------------

vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness) {
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = vec3(1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0)
	{
		float rroughness = max(0.05, roughness);
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, metallic);

		vec3 spec = D * F * G / (4.0 * dotNL * dotNV);

		color += spec * dotNL * lightColor;
	}

	return color;
}

// ----------------------------------------------------------------------------
void main() {		  
	vec3 N = normalize(inNormal);
	vec3 V = normalize(uboCamera.position - inWorldPos);

	float roughness = material.roughness;

	// Add striped pattern to roughness based on vertex position
#ifdef ROUGHNESS_PATTERN
	roughness = max(roughness, step(fract(inWorldPos.y * 2.02), 0.5));
#endif

	// Specular contribution
	vec3 Lo = vec3(0.0);
	for (int i = 0; i < uboLights.lights.length(); i++) {
		vec3 L = normalize(uboLights.lights[i].xyz - inWorldPos);
		Lo += BRDF(L, V, N, material.metallic, roughness);
	};

	// Combine with ambient
	vec3 color = materialcolor() * 0.02;
	color += Lo;

	// Gamma correct
	color = pow(color, vec3(0.4545));

	outColor = vec4(color, 1.0);
}

/*
void main() 
{
  vec3 color = vec3(material.r, material.g, material.b);

	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);
	vec3 ambient = vec3(0.1);
	vec3 diffuse = max(dot(N, L), 0.0) * vec3(1.0);
	vec3 specular = pow(max(dot(R, V), 0.0), 16.0) * vec3(0.75);
	outColor = vec4((ambient + diffuse) * color + specular, 1.0);
	
	//outColor = vec4(inNormal, 1.0f);	
}
*/
