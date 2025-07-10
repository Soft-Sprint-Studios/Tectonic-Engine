#version 450 core
layout(location = 0) in vec3 aPos;

out vec3 v_worldPos;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    mat4 viewNoTranslation = view;
    viewNoTranslation[3] = vec4(0.0, 0.0, 0.0, 1.0);
    
    vec4 pos = projection * viewNoTranslation * vec4(aPos, 1.0);

    gl_Position = pos.xyww; 
    v_worldPos = aPos;
}