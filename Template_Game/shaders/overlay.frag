#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D overlayTexture;
uniform int u_rendermode;

void main()
{
    vec4 texColor = texture(overlayTexture, TexCoords);

    if (u_rendermode == 1) {
        if (texColor.a < 0.5) {
            discard;
        }
        FragColor = vec4(texColor.rgb, 1.0);
    } else {
        FragColor = texColor;
    }
}