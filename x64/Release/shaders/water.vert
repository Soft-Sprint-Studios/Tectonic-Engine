#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT {
    vec3 worldPos;
    vec2 texCoords;
    vec3 normal;
} vs_out;

uniform mat4 model;

void main()
{
    vs_out.worldPos = vec3(model * vec4(aPos, 1.0));
    vs_out.texCoords = aTexCoords;
    vs_out.normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = vec4(aPos, 1.0);
}