#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texY;
uniform sampler2D texCb;
uniform sampler2D texCr;

void main()
{
    vec2 flipped_coords = vec2(TexCoords.x, 1.0 - TexCoords.y);
    
    float y = texture(texY, flipped_coords).r;
    float cb = texture(texCb, flipped_coords).r;
    float cr = texture(texCr, flipped_coords).r;

    mat4 bt601 = mat4(
        1.16438,  0.00000,  1.59603, -0.87079,
        1.16438, -0.39176, -0.81297,  0.52959,
        1.16438,  2.01723,  0.00000, -1.08139,
        0, 0, 0, 1
    );

    FragColor = vec4(y, cb, cr, 1.0) * bt601;
}