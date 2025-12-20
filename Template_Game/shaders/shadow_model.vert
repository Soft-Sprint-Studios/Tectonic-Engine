#version 450 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    vec3 modelOrigin = model[3].xyz;

    float upY = model[1].y;
    float baseY = (abs(upY) > 0.1) ? modelOrigin.y + 0.001 : 0.001;

    vec3 shadowWorldPos = vec3(worldPos.x, baseY, worldPos.z);

    gl_Position = projection * view * vec4(shadowWorldPos, 1.0);
}
