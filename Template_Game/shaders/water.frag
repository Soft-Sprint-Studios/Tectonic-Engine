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
layout(bindless_sampler) uniform sampler2D lightmap;
layout(bindless_sampler) uniform sampler2D directionalLightmap;
uniform bool useLightmap;
uniform bool useDirectionalLightmap;
uniform bool r_lightmaps_bicubic;
uniform bool r_debug_lightmaps;
uniform bool r_debug_lightmaps_directional;

uniform float u_uv_scale;

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
        float shadow = 1.0 - calculateSunShadow(sunShadowMap, FragPosSunLightSpace, N, L);
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
                shadow = 1.0 - calculatePointShadow(lights[i].shadowMapHandle, FragPos_world, lightPos, lights[i].params2.x, lights[i].params2.y, viewPos);
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