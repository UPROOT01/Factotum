#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNorm;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec4 fragTan;

layout(location = 0) out vec4 outColor;

layout(binding = 1, set = 1) uniform sampler2D albedoMap;

layout(binding = 0, set = 0) uniform GlobalUniformBufferObject {
	vec3 lightDir;
	vec4 lightColor;
	vec3 eyePos;
} gubo;

const float PI = 3.14159265359;

// GGX Distribution function
float DistributionGGX(vec3 N, vec3 H, float roughness) {
	float a = roughness * roughness;

	float NdotH = max(dot(N, H), 0.0);
	float denom = (NdotH * NdotH) * (a - 1.0) + 1.0;

	return a / (PI * denom * denom);
}

// GGX helper function for geometric
float gGGX(float NdotV, float roughness) {
	float a = roughness * roughness;

	return 2 / (1 + sqrt(1 + a * (1 - NdotV * NdotV) / (NdotV * NdotV)));
}

// GGX Geometric function
float GeometricGGX(vec3 N, vec3 V, vec3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);

	return gGGX(NdotV, roughness) * gGGX(NdotL, roughness);
}

// Fresnel Function
vec3 Fresnel(float HdotV, vec3 F0) {
	return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}

mat3 computeTBN(vec3 N, vec3 T, float tangentW) {
	vec3 B = cross(N, T) * tangentW;
	return mat3(normalize(T), normalize(B), normalize(N));
}

vec3 getNormalFromMap(mat3 TBN) {
	vec3 tangentNormal = vec3(1.0, 1.0, 1.0); // Flat normal for ground
	return normalize(TBN * tangentNormal);
}

void main() {
	vec3 N = normalize(fragNorm);
	vec3 L = gubo.lightDir;
	vec3 V = normalize(gubo.eyePos - fragPos);
	vec3 H = normalize(V + L);
	vec3 T = normalize(fragTan.xyz);
	float w = fragTan.w;

	mat3 TBN = computeTBN(N, T, w);
	vec3 Nmap = getNormalFromMap(TBN);

	// get albedo color
	vec3 albedo = texture(albedoMap, fragUV).rgb;

	// get roughness and metallic
	float roughness = 1.0; // Fixed roughness for ground
	float metallic = 0.0; // Fixed metallic for ground

	// calculate F0 using the metallic value
	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	// CORRECTED diffuse component for PBR
	vec3 diffuse = (1.0 - metallic) * albedo / PI;

	// CORRECTED specular component for PBR
	float D = DistributionGGX(Nmap, H, roughness);
	float G = GeometricGGX(Nmap, V, L, roughness);
	vec3 F = Fresnel(max(dot(H, V), 0.0), F0);
	vec3 specular = (D * G * F) / (4.0 * max(dot(Nmap, V), 0.0) * max(dot(Nmap, L), 0.0) + 0.001);

	// Direct lighting calculation
	float NdotL = max(dot(Nmap, L), 0.0);
	vec3 directLight = (diffuse) * gubo.lightColor.rgb * NdotL * 1.0f;

	// Hemispherical ambient for moon environment
	vec3 skyColor = vec3(0.2, 0.2, 0.2); // Black sky for moon
	vec3 groundColor = vec3(0.8, 0.8, 0.8); // Bright ground for moon
	float ambientFactor = (dot(N, vec3(0.0, 1.0, 0.0)) + 1.0) / 2.0;
	vec3 ambient = mix(groundColor, skyColor, ambientFactor) * albedo * (1.0 - metallic);

	vec3 finalColor = (directLight) * 0.5;

	outColor = vec4(finalColor, 1.0);
}
