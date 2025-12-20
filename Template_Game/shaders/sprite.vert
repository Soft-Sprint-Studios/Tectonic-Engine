#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 spritePos;
uniform float spriteScale;

void main()
{
    vec3 camRight_worldspace = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camUp_worldspace = vec3(view[0][1], view[1][1], view[2][1]);

    vec3 vertexPos_worldspace = spritePos 
                              + camRight_worldspace * aPos.x * spriteScale
                              + camUp_worldspace * aPos.y * spriteScale;

    gl_Position = projection * view * vec4(vertexPos_worldspace, 1.0);
    TexCoords = aTexCoords;
}