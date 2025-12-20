#version 450 core
layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

in VStoGS {
    float size;
    vec3 color;
} gs_in[];

out vec2 TexCoords;
out vec3 GlowColor;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec3 pos = gl_in[0].gl_Position.xyz;
    float size = gs_in[0].size;
    GlowColor = gs_in[0].color;
    
    vec3 camRight_worldspace = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camUp_worldspace = vec3(view[0][1], view[1][1], view[2][1]);
    
    vec3 p0 = pos - camRight_worldspace * size * 0.5 - camUp_worldspace * size * 0.5;
    vec3 p1 = pos + camRight_worldspace * size * 0.5 - camUp_worldspace * size * 0.5;
    vec3 p2 = pos - camRight_worldspace * size * 0.5 + camUp_worldspace * size * 0.5;
    vec3 p3 = pos + camRight_worldspace * size * 0.5 + camUp_worldspace * size * 0.5;
    
    gl_Position = projection * view * vec4(p2, 1.0);
    TexCoords = vec2(0.0, 1.0);
    EmitVertex();

    gl_Position = projection * view * vec4(p0, 1.0);
    TexCoords = vec2(0.0, 0.0);
    EmitVertex();

    gl_Position = projection * view * vec4(p3, 1.0);
    TexCoords = vec2(1.0, 1.0);
    EmitVertex();

    gl_Position = projection * view * vec4(p1, 1.0);
    TexCoords = vec2(1.0, 0.0);
    EmitVertex();
    
    EndPrimitive();
}