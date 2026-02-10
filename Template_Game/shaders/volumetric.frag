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
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;

layout(std430, binding = 3) readonly buffer LightBlock {
    ShaderLight lights[];
};

uniform int numActiveLights;
uniform int numSteps;
uniform vec3 viewPos;
uniform mat4 invView;
uniform mat4 invProjection;
uniform mat4 projection;
uniform mat4 view; 
uniform Sun sun;
uniform sampler2D sunShadowMap;
uniform mat4 sunLightSpaceMatrix;

float dither[16] = float[](
     0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0,
    12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0,
     3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0,
    15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0
);

void main()
{
    vec4 positionClip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
    vec4 positionView = invProjection * positionClip;
    positionView /= positionView.w;
    vec3 fragPos = (invView * positionView).xyz;

    vec3 startPosition = viewPos;
    vec3 rayVector = fragPos - startPosition;
    float rayLength = length(rayVector);
    vec3 rayDirection = rayVector / rayLength;

    float stepLength = rayLength / float(numSteps);
    vec3 step = rayDirection * stepLength;

    int ditherIndex = (int(gl_FragCoord.x) % 4) + (int(gl_FragCoord.y) % 4) * 4;
    vec3 currentPosition = startPosition + step * dither[ditherIndex];
    
    vec3 accumFog = vec3(0.0);

    for (int i = 0; i < numSteps; i++)
    {
        vec4 currentViewPos = view * vec4(currentPosition, 1.0);
        vec4 currentClipPos = projection * currentViewPos;
        vec2 currentScreenUV = currentClipPos.xy / currentClipPos.w * 0.5 + 0.5;
        
        float depthOfGeometry = texture(gPosition, currentScreenUV).z;

        if (currentViewPos.z < depthOfGeometry) {
            break;
        }
		
		if (sun.enabled && sun.volumetricIntensity > 0.0) {
            float sunVisibility = calculateSunShadow(sunShadowMap, sunLightSpaceMatrix * vec4(currentPosition, 1.0), vec3(0.0), -sun.direction);
            if (sunVisibility > 0.0) {
                accumFog += sun.color * sun.intensity * sun.volumetricIntensity * sunVisibility * stepLength;
            }
        }

        for (int l = 0; l < numActiveLights; ++l)
        {
            float volumetricIntensity = lights[l].params2.z;
            if (volumetricIntensity <= 0.0) continue;
            
            float lightType = lights[l].position.w;
            vec3 lightPos = lights[l].position.xyz;

            float lightVisibility = 1.0;
            if (lights[l].shadowMapHandle.x > 0 || lights[l].shadowMapHandle.y > 0) {
                 if (lightType == 0) {
                     lightVisibility = 1.0 - calculatePointShadow(lights[l].shadowMapHandle, currentPosition, lightPos, lights[l].params2.x, lights[l].params2.y, viewPos);
                 } else {
                     float angle_rad = acos(clamp(lights[l].params1.y, -1.0, 1.0));
                     if (angle_rad < 0.01) angle_rad = 0.01;
                     mat4 lightProjection = perspective(angle_rad * 2.0, 1.0, 1.0, lights[l].params2.x);
                     vec3 up_vector = vec3(0,1,0);
                     if (abs(dot(lights[l].direction.xyz, up_vector)) > 0.99) up_vector = vec3(1,0,0);
                     mat4 lightView = lookAt(lightPos, lightPos + lights[l].direction.xyz, up_vector);
                     mat4 lightSpaceMatrix = lightProjection * lightView;

                     lightVisibility = calculateSpotShadow(lights[l].shadowMapHandle, lightSpaceMatrix, currentPosition);
                 }
            }
            
            if(lightVisibility <= 0.0) continue;

            vec3 lightDir = normalize(lightPos - currentPosition);
            float distToLight = length(lightPos - currentPosition);
            
            float attenuation = 0.0;
            if (lightType == 0) {
                float radius = lights[l].params1.x;

                float dist2 = distToLight * distToLight + 1e-4;
                float invSq = 1.0 / (dist2 + radius * radius * 0.25);
                float range = clamp(1.0 - distToLight / radius, 0.0, 1.0);

                attenuation = invSq * range * range;
            } else {
                float lightCutOff = lights[l].params1.y;
                float lightOuterCutOff = lights[l].params1.z;
                vec3 L_direction = lights[l].direction.xyz;

                float theta = dot(normalize(currentPosition - lightPos), L_direction);
                if(theta > lightOuterCutOff) {
                    float epsilon = lightCutOff - lightOuterCutOff;
                    float cone_intensity = clamp((theta - lightOuterCutOff) / epsilon, 0.0, 1.0);
                    float radius = lights[l].params1.x;
                    float dist2 = distToLight * distToLight + 1e-4;
                    float invSq = 1.0 / (dist2 + radius * radius * 0.25);

                    float range = clamp(1.0 - distToLight / radius, 0.0, 1.0);

                    attenuation = cone_intensity * invSq * range * range;
                }
            }

            if (attenuation > 0.0) {
                vec3 lightColor = lights[l].color.rgb;
                float lightIntensity = lights[l].color.a;

                accumFog += lightColor * lightIntensity *
                    volumetricIntensity * lightVisibility *
                    attenuation * stepLength;
            }
        }
        currentPosition += step;
    }

    FragColor = vec4(accumFog, 1.0);
}