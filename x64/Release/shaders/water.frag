#version 460 core
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require

out vec4 FragColor;

in TES_OUT {
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosSunLightSpace;
} fs_in;

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

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.141592 * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float calculateSpotShadow(uint64_t shadowMap, vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, float bias) {
    sampler2D shadowSampler = sampler2D(shadowMap);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;
    float currentDepth = projCoords.z;
    float final_bias = max(bias * (1.0 - dot(normal, lightDir)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowSampler, 0);
    for(int x = -1; x <= 1; ++x)
        for(int y = -1; y <= 1; ++y)
            shadow += currentDepth > texture(shadowSampler, projCoords.xy + vec2(x, y) * texelSize).r + final_bias ? 1.0 : 0.0;
    return shadow / 9.0;
}

float calculatePointShadow(uint64_t shadowMap, vec3 fragPos, vec3 lightPos, float farPlane, float bias) {
    samplerCube shadowSampler = samplerCube(shadowMap);
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);
    if(currentDepth > farPlane) return 0.0;
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
    for(int i = 0; i < 20; ++i) {
        float closestDepth = texture(shadowSampler, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
        closestDepth *= farPlane; 
        if(currentDepth > closestDepth + bias)
            shadow += 1.0;
    }
    return shadow / 20.0;
}

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

vec3 ParallaxCorrect(vec3 R, vec3 fragPos, vec3 boxMin, vec3 boxMax, vec3 probePos) {
    vec3 invR = 1.0 / R;
    vec3 t1 = (boxMin - fragPos) * invR;
    vec3 t2 = (boxMax - fragPos) * invR;
    vec3 tmin = min(t1, t2);
    vec3 tmax = max(t1, t2);
    float t_near = max(max(tmin.x, tmin.y), tmin.z);
    float t_far = min(min(tmax.x, tmax.y), tmax.z);
    if (t_near > t_far || t_far < 0.0) return R;
    return normalize((fragPos + R * t_far) - probePos);
}

void main() {
    vec2 normalScroll1 = fs_in.TexCoords * normalTiling1 + vec2(time * normalSpeed1, time * normalSpeed1 * 0.8);
    vec3 totalNormalDistortion = texture(normalMap, normalScroll1).rgb * 2.0 - 1.0;

    vec3 geoNormal = normalize(fs_in.Normal);
    vec3 tangent = normalize(cross(vec3(0.0, 0.0, 1.0), geoNormal));
    if (length(tangent) < 0.1) tangent = normalize(cross(vec3(1.0, 0.0, 0.0), geoNormal));
    vec3 bitangent = cross(geoNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, geoNormal);
    vec3 N = normalize(TBN * totalNormalDistortion);
    vec3 V = normalize(viewPos - fs_in.WorldPos);
    vec3 baseColor = vec3(0.0, 0.2, 0.3);
    vec3 F0 = mix(vec3(0.04), baseColor, 0.0);
    float roughness = 0.08;
    float metallic = 0.0;
    vec3 diffuseLighting = vec3(0.0);
    vec3 specularHighlights = vec3(0.0);

    if (sun.enabled) {
        vec3 L = normalize(-sun.direction);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float shadow = calculateSunShadow(fs_in.FragPosSunLightSpace, N, L);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 spec = (D * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.001);
        diffuseLighting += sun.color * sun.intensity * NdotL * (1.0 - shadow);
        specularHighlights += sun.color * sun.intensity * spec * (1.0 - shadow);
    }

    for (int i = 0; i < numActiveLights; ++i) {
        vec3 lightPos = lights[i].position.xyz;
        float lightType = lights[i].position.w;
        vec3 L = normalize(lightPos - fs_in.WorldPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPos - fs_in.WorldPos);
        float attenuation = 0.0;
        float shadow = 0.0;
        if (lightType == 0) {
            float radius = lights[i].params1.x;
            float radiusFalloff = pow(1.0 - clamp(distance / radius, 0.0, 1.0), 2.0);
            attenuation = radiusFalloff / (distance * distance + 1.0);
        } else {
            float lightCutOff = lights[i].params1.y;
            float lightOuterCutOff = lights[i].params1.z;
            vec3 lightDir = lights[i].direction.xyz;
            float theta = dot(L, -lightDir);
            if (theta > lightOuterCutOff) {
                float cone_intensity = clamp((theta - lightOuterCutOff) / (lightCutOff - lightOuterCutOff), 0.0, 1.0);
                float radius = lights[i].params1.x;
                float radiusFalloff = pow(1.0 - clamp(distance / radius, 0.0, 1.0), 2.0);
                attenuation = cone_intensity * radiusFalloff / (distance * distance + 1.0);
            }
        }
        if (attenuation > 0.0) {
            if (lights[i].shadowMapHandle > 0) {
                if (lightType == 0)
                    shadow = calculatePointShadow(lights[i].shadowMapHandle, fs_in.WorldPos, lightPos, lights[i].params2.x, lights[i].params2.y);
                else {
                    float angle_rad = acos(clamp(lights[i].params1.y, -1.0, 1.0));
                    if (angle_rad < 0.01) angle_rad = 0.01;
                    mat4 lightProjection = mat4(1.0);
                    mat4 lightView = mat4(1.0);
                    vec3 up = vec3(0,1,0);
                    if (abs(dot(lights[i].direction.xyz, up)) > 0.99) up = vec3(1,0,0);
                    vec3 center = lightPos + lights[i].direction.xyz;
                    lightView[3].xyz = -lightPos;
                    vec4 fragPosLightSpace = lightProjection * lightView * vec4(fs_in.WorldPos, 1.0);
                    shadow = calculateSpotShadow(lights[i].shadowMapHandle, fragPosLightSpace, N, L, lights[i].params2.y);
                }
            }
            float NdotL = max(dot(N, L), 0.0);
            vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
            float D = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3 spec = (D * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.001);
            vec3 lightColor = lights[i].color.rgb;
            float lightIntensity = lights[i].color.a;
            diffuseLighting += lightColor * lightIntensity * NdotL * attenuation * (1.0 - shadow);
            specularHighlights += lightColor * lightIntensity * spec * attenuation * (1.0 - shadow);
        }
    }

    if (flashlight.enabled) {
        vec3 L = normalize(flashlight.position - fs_in.WorldPos);
        vec3 H = normalize(L + V);
        float NdotL = max(dot(N, L), 0.0);
        float distance = length(flashlight.position - fs_in.WorldPos);
        float attenuation = pow(max(0.0, 1.0 - distance / 35.0), 2.0);
        float theta = dot(L, -flashlight.direction);
        float innerCutOff = cos(radians(12.5));
        float outerCutOff = cos(radians(17.5));
        if (theta > outerCutOff) {
            float cone_intensity = clamp((theta - outerCutOff) / (innerCutOff - outerCutOff), 0.0, 1.0);
            vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
            float D = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3 spec = (D * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.001);
            diffuseLighting += vec3(1.0) * 3.0 * NdotL * attenuation * cone_intensity;
            specularHighlights += vec3(1.0) * 3.0 * spec * attenuation * cone_intensity;
        }
    }

    vec2 dudvScroll = fs_in.TexCoords * 4.0 + vec2(time * dudvMoveSpeed, time * -dudvMoveSpeed);
    vec2 distortion = (texture(dudvMap, dudvScroll).rg * 2.0 - 1.0) * waveStrength;
    vec3 reflectDir = reflect(-V, N);
    if (useParallaxCorrection) reflectDir = ParallaxCorrect(reflectDir, fs_in.WorldPos, probeBoxMin, probeBoxMax, probePosition);
    reflectDir.xy += distortion;
    vec3 reflectionColor = texture(reflectionMap, reflectDir).rgb;

    float fresnelFactor = 0.02 + 0.98 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 litWaterColor = baseColor * diffuseLighting + specularHighlights;
    vec3 finalColor = mix(litWaterColor, reflectionColor, fresnelFactor);

    FragColor = vec4(finalColor, 0.85);
}
