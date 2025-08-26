#version 450 core

out vec4 fragColor;

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 FragPos_world;
in vec4 v_clipSpace;
in mat3 v_tbn;

uniform sampler2D reflectionTexture;
uniform sampler2D normalMap;

void main() {
    vec2 ndc = (v_clipSpace.xy / v_clipSpace.w) * 0.5 + 0.5;

    vec3 normalMapSample = texture(normalMap, v_texCoord).rgb * 2.0 - 1.0;
    vec3 N = normalize(v_tbn * normalMapSample);

    vec2 distortion = N.xy * 0.0;
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y) + distortion;

    vec3 reflectionColor = texture(reflectionTexture, reflectTexCoords).rgb;

    fragColor = vec4(clamp(reflectionColor * 2.5, 0.0, 1.0), 1.0);
}
