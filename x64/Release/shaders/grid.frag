#version 460 core
out vec4 FragColor;

uniform vec4 grid_color;

void main()
{
    FragColor = grid_color;
}