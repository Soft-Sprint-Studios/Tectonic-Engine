#version 460 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D uiTexture;
uniform vec4 uiColor;
uniform bool useTexture;
void main() {
    if (useTexture) {
        FragColor = vec4(uiColor.rgb, texture(uiTexture, TexCoords).a * uiColor.a);
    } else {
        FragColor = uiColor;
    }
}