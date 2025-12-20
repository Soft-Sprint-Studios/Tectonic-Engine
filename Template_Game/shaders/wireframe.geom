#version 450 core
layout (triangles) in;
layout (line_strip, max_vertices = 4) out;

uniform mat4 view;
uniform mat4 projection;

void main() {
    mat4 viewProjection = projection * view;

    gl_Position = viewProjection * gl_in[0].gl_Position;
    EmitVertex();

    gl_Position = viewProjection * gl_in[1].gl_Position;
    EmitVertex();

    gl_Position = viewProjection * gl_in[2].gl_Position;
    EmitVertex();

    gl_Position = viewProjection * gl_in[0].gl_Position;
    EmitVertex();
    
    EndPrimitive();
}