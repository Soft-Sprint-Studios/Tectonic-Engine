#version 460 core
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require

out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 FragPosSunLightSpace;

uniform sampler2D dudvMap;
uniform sampler2D normalMap;
uniform samplerCube reflectionMap;
uniform sampler2D sunShadowMap;

struct ShaderLight {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params1;
    vec4 params2;
    uint64_t shadowMapHandle;
    uint64_t _padding;
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

float calculateSpotShadow(uint64_t shadowMap, vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, float bias)
{
    sampler2D shadowSampler = sampler2D(shadowMap);
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

float calculatePointShadow(uint64_t shadowMap, vec3 fragPos, vec3 lightPos, float farPlane, float bias)
{
    samplerCube shadowSampler = samplerCube(shadowMap);
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    if(currentDepth > farPlane) {
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
    float diskRadius = (1.0 + viewDistance / farPlane) * 0.02;
    for(int i = 0; i < 20; ++i)
    {
        float closestDepth = texture(shadowSampler, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
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
    
    for (int i = 0; i < numActiveLights; ++i) {
        vec3 lightPos = lights[i].position.xyz;
        float lightType = lights[i].position.w;

        vec3 L = normalize(lightPos - WorldPos);
        vec3 H = normalize(L + V);
        float distance = length(lightPos - WorldPos);
        float NdotL = max(dot(N, L), 0.0);
        
        float attenuation = 0.0;
        if (lightType == 0)
        {
            float radius = lights[i].params1.x;
            float radiusFalloff = pow(1.0 - clamp(distance / radius, 0.0, 1.0), 2.0);
            attenuation = radiusFalloff / (distance * distance + 1.0);
        }
        else
        {
            float lightCutOff = lights[i].params1.y;
            float lightOuterCutOff = lights[i].params1.z;
            vec3 lightDir = lights[i].direction.xyz;

            float theta = dot(L, -lightDir);
            if (theta > lightOuterCutOff) {
               float epsilon = lightCutOff - lightOuterCutOff;
               float cone_intensity = clamp((theta - lightOuterCutOff) / epsilon, 0.0, 1.0);
               float radius = lights[i].params1.x;
               float radiusFalloff = pow(1.0 - clamp(distance / radius, 0.0, 1.0), 2.0);
               attenuation = cone_intensity * radiusFalloff / (distance * distance + 1.0);
            }
        }
        
        if (attenuation > 0.0) {
            float shadow = 0.0;
            if(lights[i].shadowMapHandle > 0) {
                if(lightType == 0) {
                    shadow = calculatePointShadow(lights[i].shadowMapHandle, WorldPos, lightPos, lights[i].params2.x, lights[i].params2.y);
                } else {
                    float angle_rad = acos(clamp(lights[i].params1.y, -1.0, 1.0));
                    if (angle_rad < 0.01) angle_rad = 0.01;
                    mat4 lightProjection = perspective(angle_rad * 2.0, 1.0, 1.0, lights[i].params2.x);
                    vec3 up_vector = vec3(0,1,0);
                    if (abs(dot(lights[i].direction.xyz, up_vector)) > 0.99) up_vector = vec3(1,0,0);
                    mat4 lightView = lookAt(lightPos, lightPos + lights[i].direction.xyz, up_vector);
                    mat4 lightSpaceMatrix = lightProjection * lightView;
                    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(WorldPos, 1.0);

                    shadow = calculateSpotShadow(lights[i].shadowMapHandle, fragPosLightSpace, N, L, lights[i].params2.y);
                }
            }
            float shadowFactor = 1.0 - shadow;
            
            vec3 lightColor = lights[i].color.rgb;
            float lightIntensity = lights[i].color.a;

            diffuseLighting += lightColor * lightIntensity * NdotL * attenuation * shadowFactor;
            
            float spec = pow(max(dot(N, H), 0.0), local_light_shininess);
            specularHighlights += lightColor * lightIntensity * spec * attenuation * shadowFactor;
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