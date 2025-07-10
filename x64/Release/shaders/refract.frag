#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D normalMap;
uniform float refractionStrength;

void main()
{
    vec2 screenUV = gl_FragCoord.xy / textureSize(sceneTexture, 0);
    vec3 normal = texture(normalMap, TexCoords).rgb * 2.0 - 1.0;
    vec2 offset = normal.xy * refractionStrength;
    vec2 distortedUV = screenUV + offset;
    
    vec3 refractedColor = texture(sceneTexture, distortedUV).rgb;
    
    FragColor = vec4(refractedColor, 0.85);
}