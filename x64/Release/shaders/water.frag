#version 450 core
#extension GL_ARB_bindless_texture : require

const float Eta = 0.25;

out vec4 fragColor;

in vec3 v_incident;
in vec3 v_bitangent;
in vec3 v_normal;
in vec3 v_tangent;
in vec2 v_texCoord;
in vec4 FragPosSunLightSpace;
in vec3 FragPos_world;
in vec2 v_texCoordLightmap;
in vec4 v_clipSpace;

uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D refractionDepthTexture;
uniform sampler2D flowMap;
uniform sampler2D dudvMap;
uniform sampler2D normalMap;
uniform sampler2D sunShadowMap;
uniform sampler2D lightmap;
uniform sampler2D directionalLightmap;
uniform bool useLightmap;
uniform bool useDirectionalLightmap;
uniform bool r_lightmaps_bicubic;
uniform bool r_debug_lightmaps;
uniform bool r_debug_lightmaps_directional;

struct ShaderLight {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params1;
    vec4 params2;
    uvec2 shadowMapHandle;
    uvec2 _padding;
};

struct Sun {
    bool enabled;
    vec3 direction;
    vec3 color;
    float intensity;
};

struct Flashlight {
    bool enabled;
    vec3 position;
    vec3 direction;
};

layout(std430, binding = 3) readonly buffer LightBlock {
    ShaderLight lights[];
};

uniform int numActiveLights;
uniform Sun sun;
uniform Flashlight flashlight;
uniform vec3 viewPos;
uniform float time;
uniform float waveStrength = 0.02;
uniform float normalTiling1 = 2.0;
uniform float normalSpeed1 = 0.02;
uniform float dudvMoveSpeed = 0.03;
uniform float flowSpeed = 0.01;
uniform bool useFlowMap;
uniform vec3 u_waterAabbMin;
uniform vec3 u_waterAabbMax;
uniform bool u_debug_reflection;

float calculateSunShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;
    float currentDepth = projCoords.z;
    float bias = max(0.01 * (1.0 - dot(normal, lightDir)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(sunShadowMap, 0);
    for(int x = -1; x <= 1; ++x)
        for(int y = -1; y <= 1; ++y)
            shadow += currentDepth > texture(sunShadowMap, projCoords.xy + vec2(x, y) * texelSize).r + bias ? 1.0 : 0.0;
    return shadow / 9.0;
}

// Bicubic filtering functions adapted from Godot Engine
float w0(float a) {
	return (1.0 / 6.0) * (a * (a * (-a + 3.0) - 3.0) + 1.0);
}

float w1(float a) {
	return (1.0 / 6.0) * (a * a * (3.0 * a - 6.0) + 4.0);
}

float w2(float a) {
	return (1.0 / 6.0) * (a * (a * (-3.0 * a + 3.0) + 3.0) + 1.0);
}

float w3(float a) {
	return (1.0 / 6.0) * (a * a * a);
}

float g0(float a) {
	return w0(a) + w1(a);
}

float g1(float a) {
	return w2(a) + w3(a);
}

float h0(float a) {
	return -1.0 + w1(a) / (w0(a) + w1(a));
}

float h1(float a) {
	return 1.0 + w3(a) / (w2(a) + w3(a));
}

vec4 texture_bicubic(sampler2D tex, vec2 uv, vec2 texture_size) {
	vec2 texel_size = vec2(1.0) / texture_size;
	uv = uv * texture_size + vec2(0.5);

	vec2 iuv = floor(uv);
	vec2 fuv = fract(uv);

	float g0x = g0(fuv.x);
	float g1x = g1(fuv.x);
	float h0x = h0(fuv.x);
	float h1x = h1(fuv.x);
	float h0y = h0(fuv.y);
	float h1y = h1(fuv.y);

	vec2 p0 = (vec2(iuv.x + h0x, iuv.y + h0y) - vec2(0.5)) * texel_size;
	vec2 p1 = (vec2(iuv.x + h1x, iuv.y + h0y) - vec2(0.5)) * texel_size;
	vec2 p2 = (vec2(iuv.x + h0x, iuv.y + h1y) - vec2(0.5)) * texel_size;
	vec2 p3 = (vec2(iuv.x + h1x, iuv.y + h1y) - vec2(0.5)) * texel_size;

	return (g0(fuv.y) * (g0x * texture(tex, p0) + g1x * texture(tex, p1))) +
		   (g1(fuv.y) * (g0x * texture(tex, p2) + g1x * texture(tex, p3)));
}

void main() {
    vec2 waterUv = (FragPos_world.xz - u_waterAabbMin.xz) / (u_waterAabbMax.xz - u_waterAabbMin.xz);
    vec2 flowDirection = vec2(0.0);
    vec2 texCoord = waterUv;
    if (useFlowMap) {
        flowDirection = (texture(flowMap, waterUv).xy * 2.0 - 1.0);
        texCoord += flowDirection * time * flowSpeed;
    }

    vec2 distortion = ((texture(dudvMap, texCoord * 4.0 + vec2(time * dudvMoveSpeed, 0)).rg * 2.0 - 1.0) * waveStrength) * 0.4;
    vec2 normalScroll = texCoord * normalTiling1 + vec2(time * normalSpeed1, time * normalSpeed1 * 0.8) + distortion;

    vec3 normalMapSample = texture(normalMap, normalScroll).rgb * 2.0 - 1.0;

    mat3 TBN = mat3(v_tangent, v_bitangent, v_normal);
    vec3 N = normalize(TBN * normalMapSample);

    vec3 V = normalize(viewPos - FragPos_world);

    vec2 ndc = (v_clipSpace.xy / v_clipSpace.w) / 2.0 + 0.5;
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y);
    vec2 refractTexCoords = ndc;

    refractTexCoords += distortion;
    reflectTexCoords += distortion;
    
    vec3 reflectionColor = 2.0 * texture(reflectionTexture, reflectTexCoords).rgb;
    vec3 refractionColor = texture(refractionTexture, refractTexCoords).rgb;
    float fresnel = Eta + (1.0 - Eta) * pow(max(0.0, 1.0 - dot(V, N)), 5.0);
    vec3 baseWaterColor = mix(refractionColor, reflectionColor, fresnel);

    vec3 ambient = 0.05 * baseWaterColor;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    vec3 bakedDiffuse = vec3(0.0);
    vec3 bakedSpecular = vec3(0.0);
    float shininess = 128.0;
    float specularStrength = 3.0;

    if (useLightmap) {
        vec3 bakedRadiance;
        if (r_lightmaps_bicubic) {
            bakedRadiance = texture_bicubic(lightmap, v_texCoordLightmap, textureSize(lightmap, 0)).rgb;
        } else {
            bakedRadiance = texture(lightmap, v_texCoordLightmap).rgb;
        }

        if (useDirectionalLightmap) {
            vec4 directionalData = texture(directionalLightmap, v_texCoordLightmap);
            vec3 bakedLightDir = normalize(directionalData.rgb * 2.0 - 1.0);
            float NdotL_baked = max(dot(N, bakedLightDir), 0.0);
            bakedDiffuse = bakedRadiance * NdotL_baked;
            if (NdotL_baked > 0.0) {
                vec3 H_baked = normalize(bakedLightDir + V);
                float NdotH_baked = max(dot(N, H_baked), 0.0);
                bakedSpecular = bakedRadiance * specularStrength * pow(NdotH_baked, shininess);
            }
        } else {
            bakedDiffuse = bakedRadiance;
        }
    }

    if (sun.enabled) {
        vec3 L = normalize(-sun.direction);
        float NdotL = max(dot(N, L), 0.0);
        float shadow = calculateSunShadow(FragPosSunLightSpace, N, L);
        diffuse += sun.color * sun.intensity * NdotL * (1.0 - shadow);
        if (NdotL > 0.0) {
            vec3 H = normalize(L + V);
            float NdotH = max(dot(N, H), 0.0);
            specular += sun.color * sun.intensity * specularStrength * pow(NdotH, shininess) * (1.0 - shadow);
        }
    }

    for (int i = 0; i < numActiveLights; ++i) {
        vec3 lightPos = lights[i].position.xyz;
        float lightType = lights[i].position.w;
        vec3 L = normalize(lightPos - FragPos_world);
        float NdotL = max(dot(N, L), 0.0);
        float distance = length(lightPos - FragPos_world);
        float attenuation = 0.0;
        if (lightType == 0) {
            float radius = lights[i].params1.x;
            attenuation = pow(1.0 - clamp(distance / radius, 0.0, 1.0), 2.0) / (distance * distance + 1.0);
        } else {
            float lightCutOff = lights[i].params1.y;
            float lightOuterCutOff = lights[i].params1.z;
            vec3 lightDir = lights[i].direction.xyz;
            float theta = dot(L, -lightDir);
            if (theta > lightOuterCutOff) {
                float epsilon = lightCutOff - lightOuterCutOff;
                float cone_intensity = clamp((theta - lightOuterCutOff) / epsilon, 0.0, 1.0);
                float radius = lights[i].params1.x;
                attenuation = cone_intensity * pow(1.0 - clamp(distance / radius, 0.0, 1.0), 2.0) / (distance * distance + 1.0);
            }
        }
        if (attenuation > 0.0) {
            vec3 lightColor = lights[i].color.rgb;
            float lightIntensity = lights[i].color.a;
            diffuse += lightColor * lightIntensity * NdotL * attenuation;
            if (NdotL > 0.0) {
                vec3 H = normalize(L + V);
                float NdotH = max(dot(N, H), 0.0);
                specular += lightColor * lightIntensity * specularStrength * pow(NdotH, shininess) * attenuation;
            }
        }
    }

    if (flashlight.enabled) {
        vec3 L = normalize(flashlight.position - FragPos_world);
        float NdotL = max(dot(N, L), 0.0);
        float distance = length(flashlight.position - FragPos_world);
        float attenuation = pow(max(0.0, 1.0 - distance / 35.0), 2.0);
        float theta = dot(L, -flashlight.direction);
        float innerCutOff = cos(radians(12.5));
        float outerCutOff = cos(radians(17.5));
        if (theta > outerCutOff) {
            float cone_intensity = clamp((theta - outerCutOff) / (innerCutOff - outerCutOff), 0.0, 1.0);
            diffuse += vec3(1.0) * 3.0 * NdotL * attenuation * cone_intensity;
            if (NdotL > 0.0) {
                vec3 H = normalize(L + V);
                float NdotH = max(dot(N, H), 0.0);
                specular += vec3(1.0) * 3.0 * specularStrength * pow(NdotH, shininess) * attenuation * cone_intensity;
            }
        }
    }

    vec3 finalColor = baseWaterColor + diffuse + specular + ambient + bakedDiffuse + bakedSpecular;

    if (r_debug_lightmaps && useLightmap) {
        if (r_lightmaps_bicubic) {
            finalColor = texture_bicubic(lightmap, v_texCoordLightmap, textureSize(lightmap, 0)).rgb;
        } else {
            finalColor = texture(lightmap, v_texCoordLightmap).rgb;
        }
    } else if (r_debug_lightmaps_directional && useDirectionalLightmap) {
                if (r_lightmaps_bicubic) {
            finalColor = texture_bicubic(directionalLightmap, v_texCoordLightmap, textureSize(directionalLightmap, 0)).rgb;
        } else {
            finalColor = texture(directionalLightmap, v_texCoordLightmap).rgb;
        }
    }

    if (u_debug_reflection) {
        finalColor = reflectionColor;
    }

    fragColor = vec4(finalColor, 1.0);
}
