#version 450 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 GlowColor;

void main()
{
    float dist = distance(TexCoords, vec2(0.5));
    float falloff = 1.0 - smoothstep(0.0, 0.5, dist);
    falloff *= falloff;

    FragColor = vec4(GlowColor, falloff);
}