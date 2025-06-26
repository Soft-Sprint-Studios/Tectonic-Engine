#version 460 core
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require

layout (location = 0) out vec4 out_LitColor;
layout (location = 1) out vec3 out_Position;
layout (location = 2) out vec3 out_Normal;
layout (location = 3) out vec4 out_AlbedoSpec;
layout (location = 4) out vec4 out_PBRParams;
layout (location = 5) out vec2 out_Velocity;

in vec3 FragPos_view;
in vec3 Normal_view;

in vec3 FragPos_world;
in vec2 TexCoords;
in vec2 TexCoords2;
in mat3 TBN;
in vec4 FragPosSunLightSpace;
in vec4 v_Color;

in vec3 TangentViewPos;
in vec3 TangentFragPos;
in vec2 Velocity;

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
uniform sampler2D sunShadowMap;

layout(std430, binding = 3) readonly buffer LightBlock {
    ShaderLight lights[];
};

uniform int numActiveLights;
uniform Sun sun;
uniform Flashlight flashlight;
uniform vec3 viewPos;
uniform bool is_unlit;
uniform samplerCube environmentMap;
uniform bool useEnvironmentMap;
uniform sampler2D brdfLUT;
uniform float heightScale;
uniform float heightScale2;

uniform bool useParallaxCorrection;
uniform vec3 probeBoxMin;
uniform vec3 probeBoxMax;
uniform vec3 probePosition;

const float PI = 3.14159265359;

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

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir, float blendFactor, float blendedHeightScale)
{ 
    const float minLayers = 16.0;
    const float maxLayers = 64.0;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));  
    
    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    
    vec2 P = viewDir.xy * blendedHeightScale; 
    vec2 deltaTexCoords = P / numLayers;
  
    vec2  currentTexCoords = texCoords;
    
    float height1 = 1.0 - texture(heightMap, currentTexCoords).r;
    float height2 = 1.0 - texture(heightMap2, TexCoords2).r;
    float currentDepthMapValue = mix(height1, height2, blendFactor);
      
    while(currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords -= deltaTexCoords;

        height1 = 1.0 - texture(heightMap, currentTexCoords).r;
        height2 = 1.0 - texture(heightMap2, currentTexCoords).r;
        currentDepthMapValue = mix(height1, height2, blendFactor);
        
        currentLayerDepth += layerDepth;  
    }
    
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    float after_h1 = 1.0 - texture(heightMap, currentTexCoords).r;
    float after_h2 = 1.0 - texture(heightMap2, currentTexCoords).r;
    float afterDepth  = mix(after_h1, after_h2, blendFactor) - currentLayerDepth;

    float before_h1 = 1.0 - texture(heightMap, prevTexCoords).r;
    float before_h2 = 1.0 - texture(heightMap2, prevTexCoords).r;
    float beforeDepth = mix(before_h1, before_h2, blendFactor) - currentLayerDepth + layerDepth;
    
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

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

void main()
{
    vec3 viewDir_tangent = normalize(TangentViewPos - TangentFragPos);
	float blendFactor = v_Color.r;
	
	float blendedHeightScale = mix(heightScale, heightScale2, blendFactor);
    
    vec2 finalTexCoords = TexCoords;
    bool hasHeightMap1 = textureSize(heightMap, 0).x > 1;
    bool hasHeightMap2 = textureSize(heightMap2, 0).x > 1;

    if(heightScale > 0.0 && !is_unlit && (hasHeightMap1 || hasHeightMap2))
    {
        finalTexCoords = ParallaxMapping(TexCoords,  viewDir_tangent, blendFactor, blendedHeightScale);
    }

    vec4 texColor1 = texture(diffuseMap, finalTexCoords);
    vec3 normalTex1 = texture(normalMap, finalTexCoords).rgb;
    vec3 rma1 = texture(rmaMap, finalTexCoords).rgb;
	
	if (textureSize(detailDiffuseMap, 0).x > 1) {
        vec2 detailCoords = TexCoords * detailScale;
        vec3 detailColor = texture(detailDiffuseMap, detailCoords).rgb;
        texColor1.rgb *= detailColor * 2.0;
    }
    
    vec4 texColor2 = texture(diffuseMap2, TexCoords2);
    vec3 normalTex2 = texture(normalMap2, TexCoords2).rgb;
    vec3 rma2 = texture(rmaMap2, TexCoords2).rgb;

    vec3 albedo = mix(texColor1.rgb, texColor2.rgb, blendFactor);
    float alpha = mix(texColor1.a, texColor2.a, blendFactor);
    vec3 normalTex = mix(normalTex1, normalTex2, blendFactor);
    float roughness = mix(rma1.g, rma2.g, blendFactor);
    float metallic = mix(rma1.b, rma2.b, blendFactor);
    float ao = mix(rma1.r, rma2.r, blendFactor);

    if (is_unlit) {
        out_LitColor = vec4(albedo, 1.0);
        out_Position = FragPos_world;
        out_Normal = normalize(TBN * vec3(0.5, 0.5, 1.0));
        out_PBRParams = vec4(metallic, roughness, ao, 1.0);
        return;
    }
	
    vec3 N = normalize(TBN * (normalTex * 2.0 - 1.0));
    vec3 V = normalize(viewPos - FragPos_world);
    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
	
    if (sun.enabled)
    {
        vec3 lightDir = -sun.direction;
        vec3 H = normalize(V + lightDir);
        float NdotL = max(dot(N, lightDir), 0.0);
        vec3 radiance = sun.color * sun.intensity;
        float shadow = calculateSunShadow(FragPosSunLightSpace, N, lightDir);
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, lightDir, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
        vec3 specular     = numerator / denominator;
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    }

    for (int i = 0; i < numActiveLights; ++i)
    {
        vec3 lightPos = lights[i].position.xyz;
        float lightType = lights[i].position.w;

        vec3 L = normalize(lightPos - FragPos_world);
        vec3 H = normalize(V + L);
        float distance = length(lightPos - FragPos_world);
        float NdotL = max(dot(N, L), 0.0);
        vec3 radiance = lights[i].color.rgb * lights[i].color.a;
        float attenuation = 0.0;
        float shadow = 0.0;
        if (lightType == 0)
        {
            float radius = lights[i].params1.x;
            float radiusFalloff = pow(1.0 - clamp(distance / radius, 0.0, 1.0), 2.0);
            attenuation = radiusFalloff / (distance * distance + 1.0);
            if(attenuation > 0.0 && lights[i].shadowMapHandle > 0) shadow = calculatePointShadow(lights[i].shadowMapHandle, FragPos_world, lightPos, lights[i].params2.x, lights[i].params2.y);
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
               if (attenuation > 0.0 && lights[i].shadowMapHandle > 0) {
                   float angle_rad = acos(clamp(lightCutOff, -1.0, 1.0));
                   if (angle_rad < 0.01) angle_rad = 0.01;
                   mat4 lightProjection = perspective(angle_rad * 2.0, 1.0, 1.0, lights[i].params2.x);
                   vec3 up_vector = vec3(0,1,0);
                   if (abs(dot(lightDir, up_vector)) > 0.99) up_vector = vec3(1,0,0);
                   mat4 lightView = lookAt(lightPos, lightPos + lightDir, up_vector);
                   mat4 lightSpaceMatrix = lightProjection * lightView;
                   vec4 fragPosLightSpace = lightSpaceMatrix * vec4(FragPos_world, 1.0);

                   shadow = calculateSpotShadow(lights[i].shadowMapHandle, fragPosLightSpace, N, L, lights[i].params2.y);
               }
            }
        }
        if(attenuation > 0.0) {
            float NDF = DistributionGGX(N, H, roughness);
            float G   = GeometrySmith(N, V, L, roughness);
            vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);
            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;
            vec3 numerator    = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
            vec3 specular     = numerator / denominator;
            Lo += (kD * albedo / PI + specular) * radiance * NdotL * attenuation * (1.0 - shadow);
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
            Lo += (kD * albedo / PI + specular) * radiance * NdotL * attenuation;
        }
    }

    vec3 R_env = reflect(-V, N); 
    if (useParallaxCorrection) {
         R_env = ParallaxCorrect(R_env, FragPos_world, probeBoxMin, probeBoxMax, probePosition);
    }
    
    vec3 F_for_IBL_specular = fresnelSchlick(max(dot(N, V), 0.0), F0); 

    vec3 ambient = vec3(0.0);
    if (useEnvironmentMap)
    {
        vec3 irradiance = texture(environmentMap, N).rgb;
        vec3 diffuse_ibl_contribution = irradiance * albedo;
        
        const float MAX_REFLECTION_LOD = 4.0; 
        vec3 prefilteredColor = textureLod(environmentMap, R_env,  roughness * MAX_REFLECTION_LOD).rgb;
        
        vec2 envBRDF  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular_ibl_contribution = prefilteredColor * (F_for_IBL_specular * envBRDF.x + envBRDF.y);
        
        vec3 kS_ibl = F_for_IBL_specular;
        vec3 kD_ibl = vec3(1.0) - kS_ibl;
        kD_ibl *= (1.0 - metallic);

        ambient = (kD_ibl * diffuse_ibl_contribution + specular_ibl_contribution) * ao;
    }
	
    out_Velocity = Velocity;
    out_LitColor = vec4(ambient + Lo, alpha);
    out_Position = FragPos_view; 
    out_Normal = normalize(Normal_view);
    out_AlbedoSpec = vec4(albedo, 1.0);
    out_PBRParams = vec4(metallic, roughness, ao, alpha);
}