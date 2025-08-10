#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aSize;
layout (location = 2) in vec3 aColor;

out VStoGS {
    float size;
    vec3 color;
} vs_out;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    vs_out.size = aSize;
    vs_out.color = aColor;
}