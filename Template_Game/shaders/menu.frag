#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D u_texture;
uniform vec4 u_color_tint;

void main() {
    FragColor = texture(u_texture, TexCoords) * u_color_tint;
}
