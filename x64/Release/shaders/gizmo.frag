#version 460 core
out vec4 FragColor;

uniform vec3 gizmoColor;

void main()
{
    FragColor = vec4(gizmoColor, 1.0);
}