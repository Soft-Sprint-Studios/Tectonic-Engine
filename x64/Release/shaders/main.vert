#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 aTangent;
layout (location = 4) in vec4 aColor;
layout (location = 5) in vec2 aTexCoords2;
layout (location = 6) in vec2 aTexCoords3;
layout (location = 7) in vec2 aTexCoords4;

out VS_OUT {
    vec3 worldPos;
    vec2 texCoords;
    vec2 texCoords2;
    vec2 texCoords3;
    vec2 texCoords4;
    vec3 worldNormal;
    mat3 tbn;
    vec4 color;
    flat int isBrush;
} vs_out;

uniform mat4 model;
uniform bool isBrush;

uniform bool u_swayEnabled;
uniform float u_time;
uniform vec3 u_windDirection;
uniform float u_windStrength;

void main()
{
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

    vs_out.worldPos = vec3(model * vec4(finalPos, 1.0));
    vs_out.texCoords = aTexCoords;
    vs_out.texCoords2 = aTexCoords2;
    vs_out.texCoords3 = aTexCoords3;
    vs_out.texCoords4 = aTexCoords4;
    vs_out.color = aColor;
    vs_out.isBrush = (isBrush ? 1 : 0);

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vs_out.worldNormal = normalize(normalMatrix * aNormal);
    vec3 T_world = normalize(normalMatrix * aTangent.xyz);
    vec3 B_world = cross(vs_out.worldNormal, T_world) * aTangent.w;
    vs_out.tbn = mat3(T_world, B_world, vs_out.worldNormal);

    gl_Position = vec4(aPos, 1.0);
}