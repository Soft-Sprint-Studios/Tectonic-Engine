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
#ifndef PBR_GLSL_H
#define PBR_GLSL_H

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 ParallaxCorrect(vec3 R, vec3 fragPos, vec3 boxMin, vec3 boxMax, vec3 probePos) {
    vec3 invR = 1.0 / R;
    vec3 t1 = (boxMin - fragPos) * invR;
    vec3 t2 = (boxMax - fragPos) * invR;
    vec3 tmin = min(t1, t2);
    vec3 tmax = max(t1, t2);
    float t_near = max(max(tmin.x, tmin.y), tmin.z);
    float t_far = min(min(tmax.x, tmax.y), tmax.z);
    if (t_near > t_far || t_far < 0.0) {
        return R;
    }
    float intersection_t = t_far;
    vec3 intersectPos = fragPos + R * intersection_t;
    return normalize(intersectPos - probePos);
}

vec2 ReliefMapping(sampler2D heightMapSampler, vec2 texCoords, float hScale, vec3 viewDir, float distanceFade, float reliefMaxSteps, float reliefMinSteps, int reliefRefineSteps) {
    if (hScale <= 0.001 || distanceFade >= 1.0)
        return texCoords;

    float numLayers = mix(reliefMaxSteps, reliefMinSteps, distanceFade);
    numLayers = mix(numLayers, reliefMinSteps, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));

    float layerDepth = 1.0 / numLayers;
    vec2 p = viewDir.xy * hScale;
    vec2 deltaTexCoords = p / numLayers;

    vec2 currentTexCoords = texCoords;
    float currentLayerDepth = 0.0;
    float currentHeightMapValue = 1.0 - texture(heightMapSampler, currentTexCoords).r;

    for (int i = 0; i < int(numLayers); i++)
    {
        if (currentLayerDepth >= currentHeightMapValue)
            break;

        currentTexCoords -= deltaTexCoords;
        currentLayerDepth += layerDepth;
        currentHeightMapValue = 1.0 - texture(heightMapSampler, currentTexCoords).r;
    }

    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float depthStart = currentLayerDepth - layerDepth;
    float depthEnd = currentLayerDepth;
    vec2 texCoordsStart = prevTexCoords;
    vec2 texCoordsEnd = currentTexCoords;

    for (int i = 0; i < reliefRefineSteps; i++)
    {
        float midDepth = (depthStart + depthEnd) * 0.5;
        vec2 midTexCoords = mix(texCoordsStart, texCoordsEnd, 0.5);
        float midHeightMapValue = 1.0 - texture(heightMapSampler, midTexCoords).r;

        if (midDepth > midHeightMapValue)
        {
            depthEnd = midDepth;
            texCoordsEnd = midTexCoords;
        }
        else
        {
            depthStart = midDepth;
            texCoordsStart = midTexCoords;
        }
    }

    return texCoordsEnd;
}

#endif // PBR_GLSL_H