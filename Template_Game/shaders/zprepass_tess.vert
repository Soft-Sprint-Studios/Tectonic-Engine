#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;
layout (location = 8) in vec2 aTexCoordsLightmap;
layout (location = 4) in vec4 aColor;

out VS_OUT {
    vec3 worldPos;
    vec2 texCoords;
	vec2 lightmapTexCoords;
    vec4 color;
} vs_out;

uniform mat4 model;

void main()
{
    vs_out.worldPos = vec3(model * vec4(aPos, 1.0));
    vs_out.texCoords = aTexCoords;
	vs_out.lightmapTexCoords = aTexCoordsLightmap;
    vs_out.color = aColor;
    gl_Position = vec4(aPos, 1.0);
}