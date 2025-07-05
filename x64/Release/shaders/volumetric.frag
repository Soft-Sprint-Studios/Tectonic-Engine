#version 460 core
#extension GL_ARB_bindless_texture : require

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;

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
    float volumetricIntensity;
};

layout(std430, binding = 3) readonly buffer LightBlock {
    ShaderLight lights[];
};

uniform int numActiveLights;
uniform vec3 viewPos;
uniform mat4 invView;
uniform mat4 invProjection;
uniform mat4 projection;
uniform mat4 view; 
uniform Sun sun;
uniform sampler2D sunShadowMap;
uniform mat4 sunLightSpaceMatrix;

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

float calculatePointShadow(uvec2 shadowMapHandleUvec2, vec3 pos, vec3 lightPos, float farPlane, float bias)
{
    samplerCube shadowSampler = samplerCube(shadowMapHandleUvec2);
    vec3 fragToLight = pos - lightPos;
    float currentDepth = length(fragToLight);
    if(currentDepth > farPlane) return 0.0;
    
    float closestDepth = texture(shadowSampler, fragToLight).r;
    closestDepth *= farPlane; 
    
    return currentDepth > closestDepth + bias ? 0.0 : 1.0;
}

float calculateSpotShadow(uvec2 shadowMapHandleUvec2, mat4 lightSpaceMatrix, vec3 pos)
{
    sampler2D shadowSampler = sampler2D(shadowMapHandleUvec2);
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(pos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.z > 1.0 || any(lessThan(projCoords.xy, vec2(0.0))) || any(greaterThan(projCoords.xy, vec2(1.0))))
        return 1.0; 
        
    float currentDepth = projCoords.z;
    float pcfDepth = texture(shadowSampler, projCoords.xy).r;
    
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

        for (int l = 0; l < numActiveLights; ++l)
        {
            float volumetricIntensity = lights[l].params2.z;
            if (volumetricIntensity <= 0.0) continue;
            
            float lightType = lights[l].position.w;
            vec3 lightPos = lights[l].position.xyz;

            float lightVisibility = 1.0;
            if (lights[l].shadowMapHandle.x > 0 || lights[l].shadowMapHandle.y > 0) {
                 if (lightType == 0) {
                     lightVisibility = calculatePointShadow(lights[l].shadowMapHandle, currentPosition, lightPos, lights[l].params2.x, lights[l].params2.y);
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
                attenuation = pow(1.0 - clamp(distToLight / radius, 0.0, 1.0), 2.0);
            } else {
                float lightCutOff = lights[l].params1.y;
                float lightOuterCutOff = lights[l].params1.z;
                vec3 L_direction = lights[l].direction.xyz;

                float theta = dot(normalize(currentPosition - lightPos), L_direction);
                if(theta > lightOuterCutOff) {
                    float epsilon = lightCutOff - lightOuterCutOff;
                    float cone_intensity = clamp((theta - lightOuterCutOff) / epsilon, 0.0, 1.0);
                    float radius = lights[l].params1.x;
                    attenuation = cone_intensity * pow(1.0 - clamp(distToLight / radius, 0.0, 1.0), 2.0);
                }
            }

            if (attenuation > 0.0) {
                float scattering = ComputeScattering(dot(rayDirection, -lightDir));
                vec3 lightColor = lights[l].color.rgb;
                float lightIntensity = lights[l].color.a;
                accumFog += scattering * lightColor * lightIntensity * volumetricIntensity * lightVisibility * attenuation;
            }
        }
        currentPosition += step;
    }

    accumFog /= float(NB_STEPS);
    FragColor = vec4(accumFog, 1.0);
}