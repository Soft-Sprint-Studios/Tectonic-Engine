#version 450 core
out vec4 FragColor;

in vec2 TexCoords;
in vec4 ParticleColor;
in float ParticleViewZ;

uniform sampler2D particleTexture;
uniform sampler2D gPosition;
uniform vec2 screenSize;
uniform float softness;

void main()
{
    vec4 texColor = texture(particleTexture, TexCoords);
    if (texColor.a < 0.1)
        discard;
		
    vec2 screenUV = gl_FragCoord.xy / screenSize;
    float sceneViewZ = texture(gPosition, screenUV).z;

    float depthDelta = ParticleViewZ - sceneViewZ;
   
    float fade = clamp(depthDelta / softness, 0.0, 1.0);
    
    texColor.a *= fade;

    FragColor = texColor * ParticleColor;
}