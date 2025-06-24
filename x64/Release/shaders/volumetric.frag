#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform samplerCube pointShadowMaps[16];
uniform sampler2D spotShadowMaps[16];

struct Light {
    int type;
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float radius;
    float cutOff;
    float outerCutOff;
    float shadowFarPlane;
    float shadowBias;
    int shadowMapIndex;
    float volumetricIntensity;
};

struct Sun {
    bool enabled;
    vec3 direction;
    vec3 color;
    float intensity;
    float volumetricIntensity;
};

uniform Light lights[16];
uniform int numLights;
uniform vec3 viewPos;
uniform mat4 invView;
uniform mat4 invProjection;
uniform mat4 lightSpaceMatrices[16];
uniform Sun sun;
uniform sampler2D sunShadowMap;
uniform mat4 sunLightSpaceMatrix;
uniform mat4 projection;
uniform mat4 view; 

const float PI = 3.14159265359;
const float G_SCATTERING = 0.4;
const int NB_STEPS = 1024;

float dither[16] = float[](
     0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0,
    12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0,
     3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0,
    15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0
);

float ComputeScattering(float lightDotView)
{
    float g = G_SCATTERING;
    float g2 = g*g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * lightDotView, 1.5));
}

float calculatePointShadow(int lightIndex, vec3 pos)
{
    int shadowIndex = lights[lightIndex].shadowMapIndex;
    vec3 fragToLight = pos - lights[lightIndex].position;
    float currentDepth = length(fragToLight);
    if(currentDepth > lights[lightIndex].shadowFarPlane) return 0.0;
    
    float closestDepth = texture(pointShadowMaps[shadowIndex], fragToLight).r;
    closestDepth *= lights[lightIndex].shadowFarPlane; 
    
    return currentDepth > closestDepth + lights[lightIndex].shadowBias ? 0.0 : 1.0;
}

float calculateSpotShadow(int lightIndex, vec3 pos)
{
    int shadowIndex = lights[lightIndex].shadowMapIndex;
    vec4 fragPosLightSpace = lightSpaceMatrices[lightIndex] * vec4(pos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.z > 1.0 || any(lessThan(projCoords.xy, vec2(0.0))) || any(greaterThan(projCoords.xy, vec2(1.0))))
        return 1.0; 
        
    float currentDepth = projCoords.z;
    float pcfDepth = texture(spotShadowMaps[shadowIndex], projCoords.xy).r;
    
    return currentDepth > pcfDepth + 0.005 ? 0.0 : 1.0;
}

float calculateSunShadow(vec3 pos)
{
    vec4 fragPosLightSpace = sunLightSpaceMatrix * vec4(pos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.z > 1.0)
        return 0.0;
        
    float currentDepth = projCoords.z;
    float closestDepth = texture(sunShadowMap, projCoords.xy).r;

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

    float stepLength = rayLength / float(NB_STEPS);
    vec3 step = rayDirection * stepLength;

    int ditherIndex = (int(gl_FragCoord.x) % 4) + (int(gl_FragCoord.y) % 4) * 4;
    vec3 currentPosition = startPosition + step * dither[ditherIndex];
    
    vec3 accumFog = vec3(0.0);

    for (int i = 0; i < NB_STEPS; i++)
    {
        // 3. For each step, check against the G-Buffer depth
        vec4 currentViewPos = view * vec4(currentPosition, 1.0);
        vec4 currentClipPos = projection * currentViewPos;
        vec2 currentScreenUV = currentClipPos.xy / currentClipPos.w * 0.5 + 0.5;
        
        float depthOfGeometry = texture(gPosition, currentScreenUV).z;

        if (currentViewPos.z < depthOfGeometry) {
             break;
        }
		
		if (sun.enabled && sun.volumetricIntensity > 0.0) {
            float sunVisibility = calculateSunShadow(currentPosition);
            if (sunVisibility > 0.0) {
                float scattering = ComputeScattering(dot(rayDirection, -sun.direction));
                accumFog += scattering * sun.color * sun.intensity * sun.volumetricIntensity * sunVisibility;
            }
        }

        for (int l = 0; l < numLights; ++l)
        {
            if (lights[l].volumetricIntensity <= 0.0) continue;

            float lightVisibility = 1.0;
            if (lights[l].shadowMapIndex >= 0) {
                 if (lights[l].type == 0) lightVisibility = calculatePointShadow(l, currentPosition);
                 else lightVisibility = calculateSpotShadow(l, currentPosition);
            }
            
            if(lightVisibility <= 0.0) continue;

            vec3 lightDir = normalize(lights[l].position - currentPosition);
            float distToLight = length(lights[l].position - currentPosition);
            
            float attenuation = 0.0;
            if (lights[l].type == 0) {
                attenuation = pow(1.0 - clamp(distToLight / lights[l].radius, 0.0, 1.0), 2.0);
            } else {
            float theta = dot(normalize(currentPosition - lights[l].position), lights[l].direction);
            if(theta > lights[l].outerCutOff) {
                float epsilon = lights[l].cutOff - lights[l].outerCutOff;
                float cone_intensity = clamp((theta - lights[l].outerCutOff) / epsilon, 0.0, 1.0);
                attenuation = cone_intensity * pow(1.0 - clamp(distToLight / lights[l].radius, 0.0, 1.0), 2.0);
           }
       }

        if (attenuation > 0.0) {
                float scattering = ComputeScattering(dot(rayDirection, -lightDir));
                accumFog += scattering * lights[l].color * lights[l].intensity * lights[l].volumetricIntensity * lightVisibility * attenuation;
        }
    }
        currentPosition += step;
    }

    accumFog /= float(NB_STEPS);
    FragColor = vec4(accumFog, 1.0);
}