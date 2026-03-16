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
layout (location = 5) out vec3 out_GeometryNormal;

in vec3 FragPos_view;
in vec3 Normal_view;

in vec3 FragPos_world;
in vec2 TexCoords;
in vec2 TexCoords2;
in vec2 TexCoords3;
in vec2 TexCoords4;
in vec2 TexCoordsLightmap;
in mat3 TBN;
in vec4 FragPosSunLightSpace;
in vec4 v_Color;
in vec4 v_Color2;
in vec4 v_Color3;
in float fadeAlpha;

flat in int isBrush;

uniform mat4 view;
uniform mat4 projection;

uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
uniform sampler2D rmaMap;
uniform sampler2D heightMap;
uniform sampler2D detailDiffuseMap;
uniform float detailScale;
uniform sampler2D diffuseMap2;
uniform sampler2D normalMap2;
uniform sampler2D rmaMap2;
uniform sampler2D heightMap2;
uniform sampler2D diffuseMap3;
uniform sampler2D normalMap3;
uniform sampler2D rmaMap3;
uniform sampler2D heightMap3;
uniform sampler2D diffuseMap4;
uniform sampler2D normalMap4;
uniform sampler2D rmaMap4;
uniform sampler2D heightMap4;

layout(bindless_sampler) uniform sampler2D lightmap;
layout(bindless_sampler) uniform sampler2D directionalLightmap;
uniform bool useLightmap;
uniform bool useDirectionalLightmap;

uniform sampler2D sunShadowMap;

layout(std430, binding = 3) readonly buffer LightBlock {
    ShaderLight lights[];
};

uniform int numActiveLights;
uniform Sun sun;
uniform Flashlight flashlight;
uniform vec3 viewPos;
uniform bool is_unlit;
uniform bool is_debug_vpl;
uniform samplerCube environmentMap;
uniform bool useEnvironmentMap;
uniform sampler2D brdfLUT;
uniform float heightScale;
uniform float heightScale2;
uniform float heightScale3;
uniform float heightScale4;
uniform bool u_isParallaxEnabled;
uniform float u_relief_max_steps;
uniform float u_relief_min_steps;
uniform int u_relief_refine_steps;
uniform bool u_useAlphaTest;

uniform float u_roughness_override;
uniform float u_metalness_override;
uniform float u_roughness_override2;
uniform float u_metalness_override2;
uniform float u_roughness_override3;
uniform float u_metalness_override3;
uniform float u_roughness_override4;
uniform float u_metalness_override4;

uniform AmbientProbe u_probes[8];
uniform int u_numAmbientProbes;

uniform bool useParallaxCorrection;
uniform vec3 probeBoxMin;
uniform vec3 probeBoxMax;
uniform vec3 probePosition;

uniform bool useVertexLighting;
uniform bool r_debug_lightmaps;
uniform bool r_debug_lightmaps_directional;
uniform bool r_debug_vertex_light;
uniform bool r_debug_vertex_light_directional;
uniform bool r_lightmaps_bicubic;

const float PARALLAX_START_FADE_DISTANCE = 20.0;
const float PARALLAX_END_FADE_DISTANCE = 40.0;

void main()
{
    vec2 finalTexCoords1 = TexCoords;
    vec2 finalTexCoords2 = TexCoords2;
    vec2 finalTexCoords3 = TexCoords3;
    vec2 finalTexCoords4 = TexCoords4;

    if (u_isParallaxEnabled) {
        vec3 viewDir_world = normalize(viewPos - FragPos_world);
        vec3 viewDir_tangent = normalize(transpose(TBN) * viewDir_world);
        float dist = length(FragPos_world - viewPos);
        float parallaxFadeFactor = smoothstep(PARALLAX_START_FADE_DISTANCE, PARALLAX_END_FADE_DISTANCE, dist);
        finalTexCoords1 = ReliefMapping(heightMap, TexCoords, heightScale, viewDir_tangent, parallaxFadeFactor, u_relief_max_steps, u_relief_min_steps, u_relief_refine_steps);
        finalTexCoords2 = ReliefMapping(heightMap2, TexCoords2, heightScale2, viewDir_tangent, parallaxFadeFactor, u_relief_max_steps, u_relief_min_steps, u_relief_refine_steps);
        finalTexCoords3 = ReliefMapping(heightMap3, TexCoords3, heightScale3, viewDir_tangent, parallaxFadeFactor, u_relief_max_steps, u_relief_min_steps, u_relief_refine_steps);
        finalTexCoords4 = ReliefMapping(heightMap4, TexCoords4, heightScale4, viewDir_tangent, parallaxFadeFactor, u_relief_max_steps, u_relief_min_steps, u_relief_refine_steps);
    }

    vec4 texColor1 = texture(diffuseMap, finalTexCoords1);
    vec3 normalTex1 = texture(normalMap, finalTexCoords1).rgb;
    vec3 rma1 = texture(rmaMap, finalTexCoords1).rgb;

    vec4 texColor2 = texture(diffuseMap2, finalTexCoords2);
    vec3 normalTex2 = texture(normalMap2, finalTexCoords2).rgb;
    vec3 rma2 = texture(rmaMap2, finalTexCoords2).rgb;

    vec4 texColor3 = texture(diffuseMap3, finalTexCoords3);
    vec3 normalTex3 = texture(normalMap3, finalTexCoords3).rgb;
    vec3 rma3 = texture(rmaMap3, finalTexCoords3).rgb;

    vec4 texColor4 = texture(diffuseMap4, finalTexCoords4);
    vec3 normalTex4 = texture(normalMap4, finalTexCoords4).rgb;
    vec3 rma4 = texture(rmaMap4, finalTexCoords4).rgb;
	
	if (textureSize(detailDiffuseMap, 0).x > 1) {
        vec2 detailCoords = finalTexCoords1 * detailScale;
        vec3 detailColor = texture(detailDiffuseMap, detailCoords).rgb;
        texColor1.rgb *= detailColor * 2.0;
    }

    vec3 albedo;
    float alpha;
    vec3 normalTex;
    float roughness;
    float metallic;
    float ao;

    if (isBrush == 1) {
        float blendR = v_Color3.r;
        float blendG = v_Color3.g;
        float blendB = v_Color3.b;
        float totalWeight = max(blendR + blendG + blendB, 0.0001);
        if (totalWeight > 1.0) {
            blendR /= totalWeight;
            blendG /= totalWeight;
            blendB /= totalWeight;
        }
        float blendTotal = clamp(blendR + blendG + blendB, 0.0, 1.0);
        float blendBase = 1.0 - blendTotal;
        albedo = texColor1.rgb * blendBase + texColor2.rgb * blendR + texColor3.rgb * blendG + texColor4.rgb * blendB;
        alpha = texColor1.a * blendBase + texColor2.a * blendR + texColor3.a * blendG + texColor4.a * blendB;
        normalTex = normalTex1 * blendBase + normalTex2 * blendR + normalTex3 * blendG + normalTex4 * blendB;
        
        float r1 = (u_roughness_override >= 0.0) ? u_roughness_override : rma1.g;
        float m1 = (u_metalness_override >= 0.0) ? u_metalness_override : rma1.b;
        float r2 = (u_roughness_override2 >= 0.0) ? u_roughness_override2 : rma2.g;
        float m2 = (u_metalness_override2 >= 0.0) ? u_metalness_override2 : rma2.b;
        float r3 = (u_roughness_override3 >= 0.0) ? u_roughness_override3 : rma3.g;
        float m3 = (u_metalness_override3 >= 0.0) ? u_metalness_override3 : rma3.b;
        float r4 = (u_roughness_override4 >= 0.0) ? u_roughness_override4 : rma4.g;
        float m4 = (u_metalness_override4 >= 0.0) ? u_metalness_override4 : rma4.b;
        
        roughness = r1 * blendBase + r2 * blendR + r3 * blendG + r4 * blendB;
        metallic = m1 * blendBase + m2 * blendR + m3 * blendG + m4 * blendB;
        ao = rma1.r * blendBase + rma2.r * blendR + rma3.r * blendG + rma4.r * blendB;
    } else {
        albedo = texColor1.rgb;
        alpha = texColor1.a;
        normalTex = normalTex1;
        roughness = (u_roughness_override >= 0.0) ? u_roughness_override : rma1.g;
        metallic = (u_metalness_override >= 0.0) ? u_metalness_override : rma1.b;
        ao = rma1.r;
    }

    if (u_useAlphaTest) {
        alpha *= fadeAlpha;
        if (alpha < 0.1) 
            discard;
    }
	
    vec3 N = normalize(TBN * (normalTex * 2.0 - 1.0));
    vec3 V = normalize(viewPos - FragPos_world);
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    vec3 totalDirectDiffuse = vec3(0.0);
	
    if (sun.enabled)
    {
        vec3 lightDir = -sun.direction;
        vec3 H = normalize(V + lightDir);
        float NdotL = max(dot(N, lightDir), 0.0);
        vec3 radiance = sun.color * sun.intensity;
        float shadow = 1.0 - calculateSunShadow(sunShadowMap, FragPosSunLightSpace, N, lightDir);
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, lightDir, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
        vec3 specular     = numerator / denominator;
        vec3 diffuseContrib = (kD * albedo / PI) * radiance * NdotL * (1.0 - shadow);
        totalDirectDiffuse += diffuseContrib;
        Lo += (diffuseContrib + specular * radiance * NdotL * (1.0 - shadow));
    }

    for (int i = 0; i < numActiveLights; ++i) {
        ShaderLight light = lights[i];
        vec3 lightPos = light.position.xyz;
        float lightType = light.position.w;
        vec3 L = normalize(lightPos - FragPos_world);
        float distance = length(lightPos - FragPos_world);
        float radius = light.params1.x;
        float NdotL = max(dot(N, L), 0.0);
        float radiusFalloff = pow(1.0 - clamp(distance / radius, 0.0, 1.0), 2.0);
        float baseAttenuation = radiusFalloff / (distance * distance + 1.0);
        float spotFactor = 1.0;
        vec3 lightDir = light.direction.xyz;
        if (lightType > 0.5) {
            float theta = dot(L, -lightDir);
            float outerCutOff = light.params1.z;
            spotFactor = smoothstep(outerCutOff, light.params1.y, theta);
        }
        float attenuation = baseAttenuation * spotFactor;
        float shadow = 0.0;
        bool hasShadow = (light.shadowMapHandle.x > 0u) || (light.shadowMapHandle.y > 0u);
        if (hasShadow) {
            if (lightType < 0.5) {
                shadow = calculatePointShadow(light.shadowMapHandle, FragPos_world, lightPos, light.params2.x, light.params2.y, viewPos);
            } else {
                float angle_rad = acos(clamp(light.params1.y, -1.0, 1.0));
                angle_rad = max(angle_rad, 0.01);
                mat4 lightProj = perspective(angle_rad * 2.0, 1.0, 1.0, light.params2.x);
                float nearVertical = step(0.99, abs(dot(lightDir, vec3(0.0, 1.0, 0.0))));
                vec3 spotLightUp = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), nearVertical);
                mat4 lightView = lookAt(lightPos, lightPos + lightDir, spotLightUp);
                mat4 lightSpaceMatrix = lightProj * lightView;
                shadow = calculateSpotShadow(light.shadowMapHandle, lightSpaceMatrix * vec4(FragPos_world, 1.0), N, L, light.params2.y);
            }
        }
        if (attenuation > 0.0 && NdotL > 0.0) {
            vec3 H = normalize(V + L);
            vec3 radiance = light.color.rgb * light.color.a * attenuation * (1.0 - shadow);
            float NDF = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
            vec3 specular = numerator / denominator;
            vec3 diffuse = kD * albedo / PI;
            Lo += (diffuse + specular) * radiance * NdotL;
            totalDirectDiffuse += diffuse * radiance * NdotL;
        }
    }
	
	if(flashlight.enabled)
    {
        vec3 L = normalize(flashlight.position - FragPos_world);
        vec3 H = normalize(V + L);
        float distance = length(flashlight.position - FragPos_world);
        float NdotL = max(dot(N, L), 0.0);
        float intensity = 10.0;
        float radius = 35.0;
        float cutOff = cos(radians(12.5));
        float outerCutOff = cos(radians(20.0));
        float theta = dot(L, -flashlight.direction);
        float attenuation = 0.0;
        if (theta > outerCutOff) {
            float epsilon = cutOff - outerCutOff;
            float cone_intensity = clamp((theta - outerCutOff) / epsilon, 0.0, 1.0);
            float radiusFalloff = pow(1.0 - clamp(distance / radius, 0.0, 1.0), 2.0);
            attenuation = cone_intensity * radiusFalloff / (distance * distance + 1.0);
        }
        if(attenuation > 0.0) {
            vec3 radiance = vec3(1.0, 1.0, 0.9) * intensity;
            float NDF = DistributionGGX(N, H, roughness);
            float G   = GeometrySmith(N, V, L, roughness);
            vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);
            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;
            vec3 numerator    = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
            vec3 specular     = numerator / denominator;
            vec3 diffuseContrib = (kD * albedo / PI) * radiance * NdotL * attenuation;
            totalDirectDiffuse += diffuseContrib;
            Lo += (diffuseContrib + specular * radiance * NdotL * attenuation);
        }
    }

    vec3 bakedDiffuse = vec3(0.0);
	vec3 bakedSpecular = vec3(0.0);
	vec3 bakedRadiance = vec3(0.0);

    if (useLightmap) {
        bakedRadiance = r_lightmaps_bicubic ? texture_bicubic(lightmap, TexCoordsLightmap, textureSize(lightmap, 0)).rgb : texture(lightmap, TexCoordsLightmap).rgb;
        
        if (useDirectionalLightmap) {
            vec4 directionalData = texture(directionalLightmap, TexCoordsLightmap);
            vec3 bakedLightDir = normalize(directionalData.rgb * 2.0 - 1.0);
            float NdotL_baked = max(dot(N, bakedLightDir), 0.0);
            bakedDiffuse = bakedRadiance * albedo * NdotL_baked;
            
            if (NdotL_baked > 0.0) {
                vec3 H_baked = normalize(bakedLightDir + V);
                float NDF = DistributionGGX(N, H_baked, roughness);
                float G = GeometrySmith(N, V, bakedLightDir, roughness);
                vec3 F = fresnelSchlick(max(dot(H_baked, V), 0.0), F0);
                vec3 specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL_baked + 0.001);
                bakedSpecular = specular * 2.0 * bakedRadiance * NdotL_baked;
            }
        } else {
            bakedDiffuse = bakedRadiance * albedo;
        }
    }
    else {
        if (u_numAmbientProbes > 0 && v_Color.a < 0.5) {
            vec3 total_color = vec3(0.0);
            vec3 total_dir = vec3(0.0);
            float total_weight = 0.0;
            for (int i = 0; i < 8; i++) {
                if (length(u_probes[i].position) < 0.01) continue;
                float dist_sq = max(0.001, dot(FragPos_world - u_probes[i].position, FragPos_world - u_probes[i].position));
                float weight = 1.0 / dist_sq;
                vec3 probe_color = vec3(0.0);
                probe_color += u_probes[i].colors[0] * max(0, dot(N, vec3(1, 0, 0)));
                probe_color += u_probes[i].colors[1] * max(0, dot(N, vec3(-1, 0, 0)));
                probe_color += u_probes[i].colors[2] * max(0, dot(N, vec3(0, 1, 0)));
                probe_color += u_probes[i].colors[3] * max(0, dot(N, vec3(0, -1, 0)));
                probe_color += u_probes[i].colors[4] * max(0, dot(N, vec3(0, 0, 1)));
                probe_color += u_probes[i].colors[5] * max(0, dot(N, vec3(0, 0, -1)));
                total_color += probe_color * weight;
                total_dir += u_probes[i].dominant_direction * weight;
                total_weight += weight;
            }
            if (total_weight > 0.0) {
                bakedRadiance = total_color / total_weight;
                vec3 bakedLightDir = normalize(total_dir / total_weight);
                float NdotL_baked = max(dot(N, bakedLightDir), 0.0);
                bakedDiffuse = bakedRadiance * albedo * NdotL_baked;

                if (NdotL_baked > 0.0) {
                    vec3 H_baked = normalize(bakedLightDir + V);
                    float NDF = DistributionGGX(N, H_baked, roughness);
                    float G = GeometrySmith(N, V, bakedLightDir, roughness);
                    vec3 F = fresnelSchlick(max(dot(H_baked, V), 0.0), F0);
                    vec3 specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL_baked + 0.001);
                    bakedSpecular = specular * bakedRadiance * NdotL_baked;
                }
            }
        } 
        else if (v_Color.a > 0.5) {
            bakedRadiance = v_Color.rgb;
            if (v_Color2.a > 0.0) {
                vec3 bakedLightDir = normalize(v_Color2.rgb);
                float NdotL_baked = max(dot(N, bakedLightDir), 0.0);
                bakedDiffuse = bakedRadiance * albedo * NdotL_baked;
                if (NdotL_baked > 0.0) {
                    vec3 H_baked = normalize(bakedLightDir + V);
                    float NDF = DistributionGGX(N, H_baked, roughness);
                    float G = GeometrySmith(N, V, bakedLightDir, roughness);
                    vec3 F = fresnelSchlick(max(dot(H_baked, V), 0.0), F0);
                    vec3 specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL_baked + 0.001);
                    bakedSpecular = specular * bakedRadiance * NdotL_baked;
                }
            } else {
                bakedDiffuse = bakedRadiance * albedo;
            }
        }
    }

    vec3 ambient = vec3(0.0);
    if (useEnvironmentMap)
    {
        vec3 R_env = reflect(-V, N); 
        if (useParallaxCorrection) {
            R_env = ParallaxCorrect(R_env, FragPos_world, probeBoxMin, probeBoxMax, probePosition);
        }
        vec3 F_for_IBL_specular = fresnelSchlick(max(dot(N, V), 0.0), F0);
        vec3 irradiance = texture(environmentMap, N).rgb;
        vec3 diffuse_ibl_contribution = irradiance * albedo;
        const float MAX_REFLECTION_LOD = 4.0; 
        vec3 prefilteredColor = textureLod(environmentMap, R_env,  roughness * MAX_REFLECTION_LOD).rgb;
        vec2 envBRDF  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular_ibl_contribution = prefilteredColor * (F_for_IBL_specular * envBRDF.x + envBRDF.y);
        vec3 kS_ibl = F_for_IBL_specular;
        vec3 kD_ibl = vec3(1.0) - kS_ibl;
        kD_ibl *= (1.0 - metallic);

        vec3 totalIllumination = totalDirectDiffuse + bakedRadiance;
        float diffuseLuminance = dot(totalIllumination, vec3(0.2126, 0.7152, 0.0722));
        float reflectionOcclusion = smoothstep(0.0, 0.2, diffuseLuminance);
        float specularIBLStrength = 2.0 + (10.0 * diffuseLuminance);
        
        ambient = (kD_ibl * diffuse_ibl_contribution * reflectionOcclusion + specular_ibl_contribution * specularIBLStrength * reflectionOcclusion) * ao;
    }
	
    vec3 finalColor = Lo + ambient + bakedDiffuse + bakedSpecular;
	
    if (r_debug_lightmaps) {
        if (r_lightmaps_bicubic) {
            finalColor = texture_bicubic(lightmap, TexCoordsLightmap, textureSize(lightmap, 0)).rgb;
        } else {
            finalColor = texture(lightmap, TexCoordsLightmap).rgb;
        }
    } 
    else if (r_debug_lightmaps_directional) {
        finalColor = texture(directionalLightmap, TexCoordsLightmap).rgb;
    } 
    else if (r_debug_vertex_light) {
        if ((isBrush == 0 && v_Color.a > 0.5 && !useLightmap) || (isBrush == 1 && useVertexLighting)) {
            finalColor = v_Color.rgb;
        } else {
            finalColor = vec3(0.0);
        }
    }
    else if (r_debug_vertex_light_directional) {
        if ((isBrush == 0 && v_Color2.a > 0.0 && !useLightmap) || (isBrush == 1 && useVertexLighting && v_Color2.a > 0.0)) {
            finalColor = normalize(v_Color2.rgb * 2.0 - 1.0) * 0.5 + 0.5;
        } else {
            finalColor = vec3(0.0);
        }
    }

    if (is_unlit) {
        out_LitColor = vec4(albedo, 1.0);
    }
    else {
        out_LitColor = vec4(finalColor, alpha);
    }
    out_Position = FragPos_view; 
    out_Normal = normalize(mat3(view) * N);
	out_GeometryNormal = normalize(TBN * vec3(0.5, 0.5, 1.0));
    out_AlbedoSpec = vec4(albedo, 1.0);
    out_PBRParams = vec4(metallic, roughness, ao, alpha);
}