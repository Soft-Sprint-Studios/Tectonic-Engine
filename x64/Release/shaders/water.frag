#version 460 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 FragPosLightSpace[16];
in vec4 FragPosSunLightSpace;

uniform sampler2D dudvMap;
uniform sampler2D normalMap;
uniform samplerCube reflectionMap;
uniform samplerCube pointShadowMaps[16];
uniform sampler2D spotShadowMaps[16];
uniform sampler2D sunShadowMap;

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

uniform Light lights[16];
uniform int numLights;
uniform Sun sun;
uniform Flashlight flashlight;
uniform vec3 viewPos;

uniform vec3 cameraPosition;
uniform float time;
uniform float waveStrength = 0.02;
uniform float normalTiling1 = 2.0;
uniform float normalSpeed1 = 0.02;
uniform float dudvMoveSpeed = 0.03;

uniform bool useParallaxCorrection;
uniform vec3 probeBoxMin;
uniform vec3 probeBoxMax;
uniform vec3 probePosition;

float calculateSpotShadow(int lightIndex, vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    int shadowIndex = lights[lightIndex].shadowMapIndex;
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.z > 1.0)
        return 0.0;
        
    float currentDepth = projCoords.z;
    float bias = max(lights[lightIndex].shadowBias * (1.0 - dot(normal, lightDir)), 0.0005);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(spotShadowMaps[shadowIndex], 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(spotShadowMaps[shadowIndex], projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth > pcfDepth + bias ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

float calculatePointShadow(int lightIndex, vec3 fragPos)
{
    int shadowIndex = lights[lightIndex].shadowMapIndex;
    float bias = lights[lightIndex].shadowBias;
    vec3 fragToLight = fragPos - lights[lightIndex].position;
    float currentDepth = length(fragToLight);
    if(currentDepth > lights[lightIndex].shadowFarPlane) {
        return 0.0;
    }

    float shadow = 0.0;
    vec3 sampleOffsetDirections[20] = vec3[](
       vec3( 1, 1, 1), vec3( 1,-1, 1), vec3(-1,-1, 1), vec3(-1, 1, 1), 
       vec3( 1, 1,-1), vec3( 1,-1,-1), vec3(-1,-1,-1), vec3(-1, 1,-1),
       vec3( 1, 1, 0), vec3( 1,-1, 0), vec3(-1,-1, 0), vec3(-1, 1, 0),
       vec3( 1, 0, 1), vec3(-1, 0, 1), vec3( 1, 0,-1), vec3(-1, 0,-1),
       vec3( 0, 1, 1), vec3( 0,-1, 1), vec3( 0,-1,-1), vec3( 0, 1,-1)
    );
    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + viewDistance / lights[lightIndex].shadowFarPlane) * 0.02;
    for(int i = 0; i < 20; ++i)
    {
        float closestDepth = texture(pointShadowMaps[shadowIndex], fragToLight + sampleOffsetDirections[i] * diskRadius).r;
        closestDepth *= lights[lightIndex].shadowFarPlane; 
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
    float bias = max(0.01 * (1.0 - dot(normal, lightDir)), 0.0005);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(sunShadowMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(sunShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth > pcfDepth + bias ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
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

void main()
{
    vec2 normalScroll1 = TexCoords * normalTiling1 + vec2(time * normalSpeed1, time * normalSpeed1 * 0.8);
    vec3 totalNormalDistortion = texture(normalMap, normalScroll1).rgb * 2.0 - 1.0;

    vec3 geoNormal = normalize(Normal);
    vec3 tangent = normalize(cross(vec3(0.0, 0.0, 1.0), geoNormal));
    if (length(tangent) < 0.1) {
        tangent = normalize(cross(vec3(1.0, 0.0, 0.0), geoNormal));
    }
    vec3 bitangent = cross(geoNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, geoNormal);
    vec3 N = normalize(TBN * totalNormalDistortion);
    
    vec3 V = normalize(viewPos - WorldPos);

    vec3 diffuseLighting = vec3(0.0);
    vec3 specularHighlights = vec3(0.0);
    const float sun_shininess = 256.0;
    const float local_light_shininess = 128.0;
    vec3 baseColor = vec3(0.0, 0.2, 0.3);

    if (sun.enabled) {
        vec3 L = -sun.direction;
        vec3 H = normalize(L + V);
        float NdotL = max(dot(N, L), 0.0);
        float shadow = calculateSunShadow(FragPosSunLightSpace, N, L);
        float shadowFactor = 1.0 - shadow;
        
        diffuseLighting += sun.color * sun.intensity * NdotL * shadowFactor;

        float spec = pow(max(dot(N, H), 0.0), sun_shininess);
        specularHighlights += sun.color * sun.intensity * spec * shadowFactor;
    }
    
    for (int i = 0; i < numLights; ++i) {
        vec3 L = normalize(lights[i].position - WorldPos);
        vec3 H = normalize(L + V);
        float distance = length(lights[i].position - WorldPos);
        float NdotL = max(dot(N, L), 0.0);
        
        float attenuation = 0.0;
        if (lights[i].type == 0)
        {
            float radiusFalloff = pow(1.0 - clamp(distance / lights[i].radius, 0.0, 1.0), 2.0);
            attenuation = radiusFalloff / (distance * distance + 1.0);
        }
        else
        {
            float theta = dot(L, -lights[i].direction);
            if (theta > lights[i].outerCutOff) {
               float epsilon = lights[i].cutOff - lights[i].outerCutOff;
               float cone_intensity = clamp((theta - lights[i].outerCutOff) / epsilon, 0.0, 1.0);
               float radiusFalloff = pow(1.0 - clamp(distance / lights[i].radius, 0.0, 1.0), 2.0);
               attenuation = cone_intensity * radiusFalloff / (distance * distance + 1.0);
            }
        }
        
        if (attenuation > 0.0) {
            float shadow = 0.0;
            if(lights[i].shadowMapIndex >= 0) {
                if(lights[i].type == 0) shadow = calculatePointShadow(i, WorldPos);
                else shadow = calculateSpotShadow(i, FragPosLightSpace[i], N, L);
            }
            float shadowFactor = 1.0 - shadow;

            diffuseLighting += lights[i].color * lights[i].intensity * NdotL * attenuation * shadowFactor;
            
            float spec = pow(max(dot(N, H), 0.0), local_light_shininess);
            specularHighlights += lights[i].color * lights[i].intensity * spec * attenuation * shadowFactor;
        }
    }
    
    if (flashlight.enabled) {
        vec3 L = normalize(flashlight.position - WorldPos);
        vec3 H = normalize(L + V);
        float NdotL = max(dot(N, L), 0.0);
        float distance = length(flashlight.position - WorldPos);
        float attenuation = pow(max(0.0, 1.0 - distance / 35.0), 2.0);
        
        float theta = dot(L, -flashlight.direction);
        const float flashlightCutOff = cos(radians(12.5));
        const float flashlightOuterCutOff = cos(radians(17.5));
        
        if (theta > flashlightOuterCutOff) {
            float cone_intensity = clamp((theta - flashlightOuterCutOff) / (flashlightCutOff - flashlightOuterCutOff), 0.0, 1.0);
            
            diffuseLighting += vec3(1.0) * 3.0 * NdotL * attenuation * cone_intensity;
            
            float spec = pow(max(dot(N, H), 0.0), local_light_shininess);
            specularHighlights += vec3(1.0) * 3.0 * spec * attenuation * cone_intensity;
        }
    }

    vec2 dudvScroll = TexCoords * 4.0 + vec2(time * dudvMoveSpeed, time * -dudvMoveSpeed);
    vec2 distortion = (texture(dudvMap, dudvScroll).rg * 2.0 - 1.0) * waveStrength;
    
    vec3 reflectDir = reflect(-V, N);
    vec3 finalReflectDir = reflectDir;
    if (useParallaxCorrection) {
        finalReflectDir = ParallaxCorrect(reflectDir, WorldPos, probeBoxMin, probeBoxMax, probePosition);
    }
    finalReflectDir.xy += distortion;
    vec3 reflectionColor = texture(reflectionMap, finalReflectDir).rgb;

    float fresnelFactor = 0.02 + 0.98 * pow(1.0 - max(dot(geoNormal, V), 0.0), 5.0);
    
    vec3 litWaterColor = baseColor * diffuseLighting + specularHighlights;
    
    vec3 finalColor = mix(litWaterColor, reflectionColor, fresnelFactor);
    
    FragColor = vec4(finalColor, 0.85);
}