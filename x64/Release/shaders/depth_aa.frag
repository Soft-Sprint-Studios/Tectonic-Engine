#version 460 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D finalImage;
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform vec2 screenSize;

const float depthThreshold = 0.05;
const float normalThreshold = 0.4;
const float edgeStrengthCap = 1.0;

const float FXAA_REDUCE_MUL = 1.0 / 8.0;
const float FXAA_REDUCE_MIN = 1.0 / 128.0;
const float FXAA_SPAN_MAX = 8.0;

void main()
{
    float centerDepth = texture(gPosition, TexCoords).z;
    vec3 centerNormal = texture(gNormal, TexCoords).xyz;
    vec2 texelStep = 1.0 / screenSize;

    float depthN = texture(gPosition, TexCoords + vec2(0, texelStep.y)).z;
    float depthS = texture(gPosition, TexCoords - vec2(0, texelStep.y)).z;
    float depthE = texture(gPosition, TexCoords + vec2(texelStep.x, 0)).z;
    float depthW = texture(gPosition, TexCoords - vec2(texelStep.x, 0)).z;

    float dVertical   = abs(centerDepth - ((depthN + depthS) / 2.0));
    float dHorizontal = abs(centerDepth - ((depthE + depthW) / 2.0));
    float depthAmount = (dVertical + dHorizontal) * depthThreshold;

    vec3 normalN = texture(gNormal, TexCoords + vec2(0, texelStep.y)).xyz;
    vec3 normalS = texture(gNormal, TexCoords - vec2(0, texelStep.y)).xyz;
    vec3 normalE = texture(gNormal, TexCoords + vec2(texelStep.x, 0)).xyz;
    vec3 normalW = texture(gNormal, TexCoords - vec2(texelStep.x, 0)).xyz;

    float nVertical   = dot(vec3(1.0), abs(centerNormal - ((normalN + normalS) / 2.0)));
    float nHorizontal = dot(vec3(1.0), abs(centerNormal - ((normalE + normalW) / 2.0)));
    float normalAmount = (nVertical + nHorizontal) * normalThreshold;

    float edgeAmount = max(depthAmount, normalAmount);
    
    vec3 originalColor = texture(finalImage, TexCoords).rgb;
    
    if (edgeAmount < 0.01) {
        FragColor = vec4(originalColor, 1.0);
        return;
    }
    
    const vec3 luma = vec3(0.299, 0.587, 0.114);

    float lumaM  = dot(originalColor, luma);
    float lumaNW = dot(texture(finalImage, TexCoords + vec2(-texelStep.x,  texelStep.y)).rgb, luma);
    float lumaNE = dot(texture(finalImage, TexCoords + vec2( texelStep.x,  texelStep.y)).rgb, luma);
    float lumaSW = dot(texture(finalImage, TexCoords + vec2(-texelStep.x, -texelStep.y)).rgb, luma);
    float lumaSE = dot(texture(finalImage, TexCoords + vec2( texelStep.x, -texelStep.y)).rgb, luma);

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin)) * texelStep;

    vec3 rgbA = 0.5 * (
        texture(finalImage, TexCoords + dir * (1.0/3.0 - 0.5)).rgb +
        texture(finalImage, TexCoords + dir * (2.0/3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(finalImage, TexCoords + dir * -0.5).rgb +
        texture(finalImage, TexCoords + dir * 0.5).rgb);

    float lumaB = dot(rgbB, luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    
    vec3 blurredColor = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;

    float blendFactor = min(edgeAmount, edgeStrengthCap);
    vec3 finalColor = mix(originalColor, blurredColor, blendFactor);

    FragColor = vec4(finalColor, 1.0);
}