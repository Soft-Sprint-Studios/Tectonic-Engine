#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 aTangent;
layout (location = 4) in vec4 aColor;
layout (location = 5) in vec2 aTexCoords2;
layout (location = 6) in vec2 aTexCoords3;
layout (location = 7) in vec2 aTexCoords4;
layout (location = 8) in vec2 aTexCoordsLightmap;
layout (location = 9) in vec4 aColor2;
layout (location = 10) in ivec4 aBoneIndices;
layout (location = 11) in vec4 aBoneWeights;

out VS_OUT {
    vec3 worldPos;
    vec2 texCoords;
    vec2 texCoords2;
    vec2 texCoords3;
    vec2 texCoords4;
	vec2 lightmapTexCoords; 
    vec3 worldNormal;
    mat3 tbn;
    vec4 color;
	vec4 color2;
    ivec4 boneIndices;
    vec4 boneWeights;
    flat int isBrush;
    float clipDist;
} vs_out;

uniform mat4 model;
uniform bool u_hasAnimation;
uniform mat4 u_boneMatrices[128];
uniform bool isBrush;
uniform vec4 clipPlane;

uniform bool u_swayEnabled;
uniform float u_time;
uniform vec3 u_windDirection;
uniform float u_windStrength;

uniform float u_fadeStartDist;
uniform float u_fadeEndDist;

void main()
{
    mat4 boneTransform = mat4(1.0);
    if(u_hasAnimation)
    {
        boneTransform  = aBoneWeights[0] * u_boneMatrices[aBoneIndices[0]];
        boneTransform += aBoneWeights[1] * u_boneMatrices[aBoneIndices[1]];
        boneTransform += aBoneWeights[2] * u_boneMatrices[aBoneIndices[2]];
        boneTransform += aBoneWeights[3] * u_boneMatrices[aBoneIndices[3]];
    }
    vec3 finalPos = aPos;
    if (u_swayEnabled) {
        float swayFrequency = 0.5;
        float swayAmplitude = 0.05 * u_windStrength;
        float windDot = dot(aPos.xz, normalize(u_windDirection.xz));
        float sway = sin(u_time * swayFrequency + windDot) * swayAmplitude * (aPos.y * 0.5);

        float flutterFrequency = 10.0;
        float flutterAmplitude = 0.01 * u_windStrength;
        float flutter = sin(u_time * flutterFrequency + aPos.x + aPos.z) * flutterAmplitude;

        finalPos.x += (sway + flutter) * u_windDirection.x;
        finalPos.z += (sway + flutter) * u_windDirection.z;
    }

    mat4 finalModelMatrix = model * boneTransform;
    vs_out.worldPos = vec3(finalModelMatrix * vec4(finalPos, 1.0));
    vs_out.texCoords = aTexCoords;
    vs_out.texCoords2 = aTexCoords2;
    vs_out.texCoords3 = aTexCoords3;
    vs_out.texCoords4 = aTexCoords4;
	vs_out.lightmapTexCoords = aTexCoordsLightmap;
    vs_out.color = aColor;
	vs_out.color2 = aColor2;
    vs_out.boneIndices = aBoneIndices;
    vs_out.boneWeights = aBoneWeights;
    vs_out.isBrush = (isBrush ? 1 : 0);

    mat3 normalMatrix = mat3(transpose(inverse(finalModelMatrix)));
    vs_out.worldNormal = normalize(normalMatrix * aNormal);
    vec3 T_world = normalize(normalMatrix * aTangent.xyz);
    vec3 B_world = cross(vs_out.worldNormal, T_world) * aTangent.w;
    vs_out.tbn = mat3(T_world, B_world, vs_out.worldNormal);
    vs_out.clipDist = dot(vec4(vs_out.worldPos, 1.0), clipPlane);

    gl_Position = vec4(aPos, 1.0);
}