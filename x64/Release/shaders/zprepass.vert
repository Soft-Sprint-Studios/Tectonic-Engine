#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 10) in ivec4 aBoneIndices;
layout (location = 11) in vec4 aBoneWeights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool u_hasAnimation;
uniform mat4 u_boneMatrices[128];

void main()
{
    mat4 boneTransform = mat4(1.0);
    if(u_hasAnimation)
    {
        boneTransform  = aBoneWeights[0] * u_boneMatrices[aBoneIndices[0]];
        boneTransform += aBoneWeights[1] * u_boneMatrices[aBoneIndices[1]];
        boneTransform += aBoneWeights[2] * u_boneMatrices[aBoneIndices[2]];
        boneTransform += aBoneWeights[3] * u_boneMatrices[aBoneIndices[3]];
    }
    gl_Position = projection * view * model * boneTransform * vec4(aPos, 1.0);
}