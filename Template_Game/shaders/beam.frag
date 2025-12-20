#version 450 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec3 u_color;
uniform float u_time;

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    vec2 centered_uv = TexCoords - 0.5;

    float core_width = 0.05;
    float core = 1.0 - smoothstep(core_width, core_width + 0.1, abs(centered_uv.x));

    float glow = pow(1.0 - abs(centered_uv.x * 2.0), 4.0);

    float flicker = sin(TexCoords.y * 30.0 + u_time * 20.0) * 0.5 + 0.5;
    float slow_pulse = sin(TexCoords.y * 5.0 - u_time * 5.0) * 0.5 + 0.5;

    float dynamic_effect = (flicker * 0.7 + slow_pulse * 0.3) * glow;

    float intensity = core * 1.5 + glow * 0.3 + dynamic_effect * 0.5;

    vec3 final_color = u_color * intensity;
    float alpha = clamp(intensity * 0.3, 0.0, 1.0);
    
    FragColor = vec4(final_color, alpha);
}