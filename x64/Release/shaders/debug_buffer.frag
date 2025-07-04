#version 460 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D debugTexture;
uniform int viewMode;

void main()
{
    vec4 texColor = texture(debugTexture, TexCoords);
    if (viewMode == 1) {
        FragColor = vec4(texColor.r, texColor.r, texColor.r, 1.0);
    } else if (viewMode == 2) {
        FragColor = vec4(texColor.g, texColor.g, texColor.g, 1.0);
    } else if (viewMode == 3) {
        FragColor = vec4(texColor.b, texColor.b, texColor.b, 1.0);
    } else if (viewMode == 4) {
        FragColor = vec4(texColor.a, texColor.a, texColor.a, 1.0);
    } else if (viewMode == 5) {
        FragColor = vec4(texColor.xyz, 1.0);
    } else {
        FragColor = vec4(texColor.rgb, 1.0);
    }
}