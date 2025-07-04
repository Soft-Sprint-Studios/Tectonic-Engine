#version 460 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D sceneTexture;

void main()
{
    vec3 color = texture(sceneTexture, TexCoords).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 0.5)
        FragColor = vec4(color, 1.0);
    else
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}