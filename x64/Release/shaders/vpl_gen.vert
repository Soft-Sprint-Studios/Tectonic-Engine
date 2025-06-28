#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 v_worldPos;
out vec3 v_worldNormal;
out vec2 v_texCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    v_worldPos = vec3(model * vec4(aPos, 1.0));
    v_worldNormal = mat3(transpose(inverse(model))) * aNormal;
    v_texCoords = aTexCoords;
    gl_Position = projection * view * vec4(v_worldPos, 1.0);
}