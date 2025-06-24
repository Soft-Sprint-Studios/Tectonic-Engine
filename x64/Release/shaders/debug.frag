#version 460 core
out vec4 FragColor;

uniform vec4 color; // Changed from vec3 to vec4

void main()
{
    FragColor = color; // Use the uniform directly
}