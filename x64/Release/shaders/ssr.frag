#version 450 core
layout (location = 0) out vec4 fragColor;

uniform sampler2D colorBuffer;
uniform sampler2D gNormal;
uniform sampler2D gPBRParams;
uniform sampler2D gPosition;
uniform mat4 projection, view;

uniform int binarySearchCount = 8;
uniform int rayMarchCount = 64;
uniform float rayStep = 0.05;
uniform float thickness = 0.1;

in vec2 TexCoords;

vec2 binarySearch(inout vec3 dir, inout vec3 hitCoord, inout float dDepth) {
    float depth;
    vec4 projectedCoord;
 
    for(int i = 0; i < binarySearchCount; i++) {
        projectedCoord = projection * vec4(hitCoord, 1.0);
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
 
        depth = texture(gPosition, projectedCoord.xy).z;
 
        dDepth = hitCoord.z - depth;

        dir *= 0.5;
        if(dDepth > 0.0)
            hitCoord += dir;
        else
            hitCoord -= dir;    
    }

    projectedCoord = projection * vec4(hitCoord, 1.0);
    projectedCoord.xy /= projectedCoord.w;
    projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
 
    return projectedCoord.xy;
}

vec2 rayCast(vec3 dir, inout vec3 hitCoord, out float dDepth) {
    dir *= rayStep;
    
    for (int i = 0; i < rayMarchCount; i++) {
        hitCoord += dir;

        vec4 projectedCoord = projection * vec4(hitCoord, 1.0);
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5; 

        if (projectedCoord.x < 0.0 || projectedCoord.x > 1.0 || projectedCoord.y < 0.0 || projectedCoord.y > 1.0) {
            return vec2(-1.0);
        }

        float depth = texture(gPosition, projectedCoord.xy).z;
        dDepth = hitCoord.z - depth;
        
        if (dDepth <= 0.0 && dDepth > -thickness) {
            return binarySearch(dir, hitCoord, dDepth);
        }
    }

    return vec2(-1.0);
}

float fresnel(vec3 viewDir, vec3 normal) {
    return pow(1.0 - max(dot(viewDir, normal), 0.0), 5.0);
}

void main() {
    vec3 baseColor = texture(colorBuffer, TexCoords).rgb;
    vec4 pbrParams = texture(gPBRParams, TexCoords);
    float metalness = pbrParams.r;
    float roughness = pbrParams.g;

    float reflectionStrength = metalness * (1.0 - roughness);

    if (reflectionStrength < 0.1) {
        fragColor = vec4(baseColor, 1.0);
        return;
    }

    vec3 viewPos = texture(gPosition, TexCoords).xyz;
    if (viewPos.z == 0.0) {
        fragColor = vec4(baseColor, 1.0);
        return;
    }

    vec3 normal = normalize(texture(gNormal, TexCoords).xyz);
    vec3 reflected = normalize(reflect(normalize(viewPos), normal));

    vec3 hitPos = viewPos;
    float dDepth; 
    vec2 coords = rayCast(reflected * max(-viewPos.z, 0.2), hitPos, dDepth);
    
    vec3 finalColor;

    if (coords.x >= 0.0) {
        vec3 reflectedColor = texture(colorBuffer, coords).rgb;
        
        float dist = length(texture(gPosition, coords).xyz - viewPos);
        float fade = clamp(1.0 - dist / 50.0, 0.0, 1.0);
        
        float fresnelFactor = fresnel(-normalize(viewPos), normal);
        
        finalColor = mix(baseColor, reflectedColor, reflectionStrength * fade * fresnelFactor);
    } else {
        finalColor = baseColor;
    }
    
    fragColor = vec4(finalColor, 1.0);
}