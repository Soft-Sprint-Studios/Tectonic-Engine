/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
layout (location = 0) out vec4 out_LitColor;
layout (location = 1) out vec3 out_Position;
layout (location = 2) out vec3 out_Normal;
layout (location = 3) out vec4 out_AlbedoSpec;
layout (location = 4) out vec4 out_PBRParams;
layout (location = 5) out vec2 out_Velocity;
layout (location = 6) out vec3 out_GeometryNormal;

in vec3 v_incident;
in vec3 v_bitangent;
in vec3 v_normal;
in vec3 v_tangent;
in vec2 v_texCoord;
in vec4 FragPosSunLightSpace;
in vec3 FragPos_world;
in vec2 v_texCoordLightmap;
in vec4 v_clipSpace;
in vec3 FragPos_view;

uniform sampler2D reflectionTexture;
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

uniform float u_uv_scale;

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
uniform mat4 view;

mat4 perspective(float fov, float aspect, float near, float far) {
    float f = 1.0 / tan(fov / 2.0);
    return mat4(
        f / aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (far + near) / (near - far), -1,
        0, 0, (2.0 * far * near) / (near - far), 0
    );
}

mat4 lookAt(vec3 eye, vec3 center, vec3 up) {
    vec3 f = normalize(center - eye);
    vec3 s = normalize(cross(f, up));
    vec3 u = cross(s, f);
    return mat4(
        s.x, u.x, -f.x, 0,
        s.y, u.y, -f.y, 0,
        s.z, u.z, -f.z, 0,
        -dot(s, eye), -dot(u, eye), dot(f, eye), 1
    );
}

float calculateSpotShadow(uvec2 shadowMapHandleUvec2, vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, float bias)
{
    sampler2D shadowSampler = sampler2D(shadowMapHandleUvec2);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0)
        return 0.0;
    float currentDepth = projCoords.z;
    float final_bias = max(bias * (1.0 - dot(normal, lightDir)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowSampler, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowSampler, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth > pcfDepth + final_bias ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

float calculatePointShadow(uvec2 shadowMapHandleUvec2, vec3 fragPos, vec3 lightPos, float farPlane, float bias)
{
    samplerCube shadowSampler = samplerCube(shadowMapHandleUvec2);
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    if(currentDepth > farPlane) {
        return 0.0;
    }
    float shadow = 0.0;
    float closestDepth = 0.0;
    vec3 sampleOffsetDirections[20] = vec3[](
       vec3( 1, 1, 1), vec3( 1,-1, 1), vec3(-1,-1, 1), vec3(-1, 1, 1), 
       vec3( 1, 1,-1), vec3( 1,-1,-1), vec3(-1,-1,-1), vec3(-1, 1,-1),
       vec3( 1, 1, 0), vec3( 1,-1, 0), vec3(-1,-1, 0), vec3(-1, 1, 0),
       vec3( 1, 0, 1), vec3(-1, 0, 1), vec3( 1, 0,-1), vec3(-1, 0,-1),
       vec3( 0, 1, 1), vec3( 0,-1, 1), vec3( 0,-1,-1), vec3( 0, 1,-1)
    );
    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + viewDistance / farPlane) * 0.02;
    for(int i = 0; i < 20; ++i)
    {
        closestDepth = texture(shadowSampler, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
        closestDepth *= farPlane; 
        if(currentDepth > closestDepth + bias)
            shadow += 1.0;
    }
    return shadow / 20.0;
}

float calculateSunShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0)
        return 0.0;
    float currentDepth = projCoords.z;
    float bias = max(0.0015 * (1.0 - dot(normal, lightDir)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(sunShadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(sunShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - 0.001 > pcfDepth ? 1.0 : 0.0;        
        }
    }
    return 1.0 - (shadow / 9.0);
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
    vec2 base_uv;
    if (u_uv_scale > 0.0) {
        base_uv = FragPos_world.xz / u_uv_scale;
    } else {
        base_uv = (FragPos_world.xz - u_waterAabbMin.xz) / (u_waterAabbMax.xz - u_waterAabbMin.xz);
    }
    vec2 flowDirection = vec2(0.0);
    vec2 texCoord = base_uv;
    if (useFlowMap) {
        flowDirection = (texture(flowMap, base_uv).xy * 2.0 - 1.0);
        texCoord += flowDirection * time * flowSpeed;
    }

    vec2 distortion = ((texture(dudvMap, texCoord * 4.0 + vec2(time * dudvMoveSpeed, 0)).rg * 2.0 - 1.0) * waveStrength) * 0.4;
    vec2 normalScroll = texCoord * normalTiling1 + vec2(time * normalSpeed1, time * normalSpeed1 * 0.8) + distortion;

    vec3 normalMapSample = texture(normalMap, normalScroll).rgb * 2.0 - 1.0;

    mat3 TBN = mat3(v_tangent, v_bitangent, v_normal);
    vec3 N = normalize(TBN * normalMapSample);
    vec3 V = normalize(viewPos - FragPos_world);

    vec2 ndc = (v_clipSpace.xy / v_clipSpace.w) / 2.0 + 0.5;
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y) + distortion;
    
    vec3 reflectionColor = 2.0 * texture(reflectionTexture, clamp(reflectTexCoords, 0.0, 1.0)).rgb;
    vec3 baseWaterColor = reflectionColor;

    vec3 ambient = baseWaterColor;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    vec3 bakedDiffuse = vec3(0.0);
    vec3 bakedSpecular = vec3(0.0);
    float shininess = 512.0;
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
        float shadow = 0.0;
        bool hasShadow = (lights[i].shadowMapHandle.x > 0u) || (lights[i].shadowMapHandle.y > 0u);
        if (hasShadow) {
            if (lightType < 0.5) {
                shadow = 1.0 - calculatePointShadow(lights[i].shadowMapHandle, FragPos_world, lightPos, lights[i].params2.x, lights[i].params2.y);
            } else {
                float angle_rad = acos(clamp(lights[i].params1.y, -1.0, 1.0));
                if (angle_rad < 0.01) angle_rad = 0.01;
                mat4 lightProjection = perspective(angle_rad * 2.0, 1.0, 1.0, lights[i].params2.x);
                float nearVertical = step(0.99, abs(dot(lights[i].direction.xyz, vec3(0.0, 1.0, 0.0))));
                vec3 spotLightUp = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), nearVertical);
                mat4 lightView = lookAt(lightPos, lightPos + lights[i].direction.xyz, spotLightUp);
                mat4 lightSpaceMatrix = lightProjection * lightView;
                shadow = 1.0 - calculateSpotShadow(lights[i].shadowMapHandle, lightSpaceMatrix * vec4(FragPos_world, 1.0), N, L, lights[i].params2.y);
            }
        }
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
            diffuse += lightColor * lightIntensity * NdotL * attenuation * shadow;
            if (NdotL > 0.0) {
                vec3 H = normalize(L + V);
                float NdotH = max(dot(N, H), 0.0);
                specular += lightColor * lightIntensity * specularStrength * pow(NdotH, shininess) * attenuation * shadow;
            }
        }
    }

    if (flashlight.enabled) {
        vec3 L = normalize(flashlight.position - FragPos_world);
        float NdotL = max(dot(N, L), 0.0);
        float distance = length(flashlight.position - FragPos_world);
        float attenuation = pow(max(0.0, 1.0 - distance / 35.0), 2.0) / (distance * distance + 1.0);
        float theta = dot(L, -flashlight.direction);
        float innerCutOff = cos(radians(12.5));
        float outerCutOff = cos(radians(17.5));
        if (theta > outerCutOff) {
            float cone_intensity = clamp((theta - outerCutOff) / (innerCutOff - outerCutOff), 0.0, 1.0);
            diffuse += NdotL * attenuation * cone_intensity;
            if (NdotL > 0.0) {
                vec3 H = normalize(L + V);
                float NdotH = max(dot(N, H), 0.0);
                specular += specularStrength * pow(NdotH, shininess) * attenuation * cone_intensity;
            }
        }
    }

    vec3 finalColor = (baseWaterColor + diffuse + specular + ambient + bakedDiffuse + bakedSpecular) * 0.1;

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

    out_LitColor = vec4(finalColor, 0.95);
    out_Position = FragPos_view;
    out_Normal = normalize(mat3(transpose(inverse(view))) * N);
    out_GeometryNormal = normalize(mat3(transpose(inverse(view))) * v_normal);
    out_AlbedoSpec = vec4(baseWaterColor, 1.0); 
    out_PBRParams = vec4(0.0, 0.1, 1.0, 0.95);
    out_Velocity = vec2(0.0);
}