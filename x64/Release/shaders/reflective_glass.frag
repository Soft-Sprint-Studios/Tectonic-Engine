#version 450 core

const float Eta = 0.15;

out vec4 fragColor;

in vec3 v_normal;
in vec2 v_texCoord;
in vec3 FragPos_world;
in vec4 v_clipSpace;
in mat3 v_tbn;

uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D normalMap;

uniform vec3 viewPos;
uniform float refractionStrength;

void main() {
    vec2 ndc = (v_clipSpace.xy / v_clipSpace.w) * 0.5 + 0.5;
    
    vec3 normalMapSample = texture(normalMap, v_texCoord).rgb * 2.0 - 1.0;
    vec3 N = normalize(v_tbn * normalMapSample);

    vec2 distortion = N.xy * refractionStrength;

    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y) + distortion;
    vec2 refractTexCoords = ndc + distortion;

    vec3 reflectionColor = texture(reflectionTexture, reflectTexCoords).rgb;
    vec3 refractionColor = texture(refractionTexture, refractTexCoords).rgb;

    vec3 V = normalize(viewPos - FragPos_world);
    float fresnel = Eta + (0.95 - Eta) * pow(max(0.0, 1.0 - dot(V, N)), 5.0);

    vec3 finalColor = mix(refractionColor, reflectionColor, fresnel);
    
    fragColor = vec4(clamp(finalColor * 2.5, 0.0, 1.0), 1.0);
}