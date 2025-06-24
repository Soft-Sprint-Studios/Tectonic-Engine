#version 460 core

in vec2 TexCoords;
uniform sampler2D u_colorTexture; 
uniform vec2 u_texelStep;

out vec4 fragColor;

#define FXAA_LUMA_THRESHOLD 0.125
#define FXAA_MUL_REDUCE 1.0/8.0
#define FXAA_MIN_REDUCE 1.0/128.0
#define FXAA_MAX_SPAN 8.0

void main()
{
    vec3 rgbM = texture(u_colorTexture, TexCoords).rgb;

	const vec3 toLuma = vec3(0.299, 0.587, 0.114);
	float lumaM = dot(rgbM, toLuma);

	vec3 rgbNW = textureOffset(u_colorTexture, TexCoords, ivec2(-1, 1)).rgb; // <-- FIXED NAME
    vec3 rgbNE = textureOffset(u_colorTexture, TexCoords, ivec2(1, 1)).rgb;  // <-- FIXED NAME
    vec3 rgbSW = textureOffset(u_colorTexture, TexCoords, ivec2(-1, -1)).rgb;// <-- FIXED NAME
    vec3 rgbSE = textureOffset(u_colorTexture, TexCoords, ivec2(1, -1)).rgb; // <-- FIXED NAME
	float lumaNW = dot(rgbNW, toLuma);
	float lumaNE = dot(rgbNE, toLuma);
	float lumaSW = dot(rgbSW, toLuma);
	float lumaSE = dot(rgbSE, toLuma);

	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

	if (lumaMax - lumaMin <= lumaMax * FXAA_LUMA_THRESHOLD)
	{
		fragColor = vec4(rgbM, 1.0);
		return;
	}  

	vec2 samplingDirection;	
	samplingDirection.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    samplingDirection.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float samplingDirectionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * FXAA_MUL_REDUCE, FXAA_MIN_REDUCE);
	float minSamplingDirectionFactor = 1.0 / (min(abs(samplingDirection.x), abs(samplingDirection.y)) + samplingDirectionReduce);
    samplingDirection = clamp(samplingDirection * minSamplingDirectionFactor, vec2(-FXAA_MAX_SPAN), vec2(FXAA_MAX_SPAN)) * u_texelStep;

	vec3 rgbSampleNeg = texture(u_colorTexture, TexCoords + samplingDirection * (1.0/3.0 - 0.5)).rgb;
	vec3 rgbSamplePos = texture(u_colorTexture, TexCoords + samplingDirection * (2.0/3.0 - 0.5)).rgb;
	vec3 rgbTwoTab = (rgbSamplePos + rgbSampleNeg) * 0.5;  

	vec3 rgbSampleNegOuter = texture(u_colorTexture, TexCoords + samplingDirection * (0.0/3.0 - 0.5)).rgb;
	vec3 rgbSamplePosOuter = texture(u_colorTexture, TexCoords + samplingDirection * (3.0/3.0 - 0.5)).rgb;
	vec3 rgbFourTab = (rgbSamplePosOuter + rgbSampleNegOuter) * 0.25 + rgbTwoTab * 0.5;   

	float lumaFourTab = dot(rgbFourTab, toLuma);
	if (lumaFourTab < lumaMin || lumaFourTab > lumaMax)
	{
		fragColor = vec4(rgbTwoTab, 1.0); 
	}
	else
	{
		fragColor = vec4(rgbFourTab, 1.0);
	}
}