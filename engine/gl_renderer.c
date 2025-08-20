/*
 * MIT License
 *
 * Copyright (c) 2025 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "gl_renderer.h"
#include "gl_console.h"
#include "gl_misc.h"
#include "cvar.h"
#include "water_manager.h"
#include "gl_beams.h"
#include "gl_cables.h"
#include "gl_overlay.h"
#include "gl_glow.h"
#include "gl_decals.h"
#include "gl_video_player.h"
#include "model_loader.h"

static float quadVertices[] = { -1.0f,1.0f,0.0f,1.0f,-1.0f,-1.0f,0.0f,0.0f,1.0f,-1.0f,1.0f,0.0f,-1.0f,1.0f,0.0f,1.0f,1.0f,-1.0f,1.0f,0.0f,1.0f,1.0f,1.0f,1.0f };
static float parallaxRoomVertices[] = {
    -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f,
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   1.0f, 0.0f, 0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 0.0f,   1.0f, 0.0f, 0.0f, 0.0f,
    -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 0.0f,   1.0f, 0.0f, 0.0f, 0.0f,
     0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f
};

void Renderer_Init(Renderer* renderer, Engine* engine) {
    renderer->zPrepassShader = createShaderProgram("shaders/zprepass.vert", "shaders/zprepass.frag");
    renderer->zPrepassTessShader = createShaderProgramTess("shaders/zprepass_tess.vert", "shaders/zprepass_tess.tcs", "shaders/zprepass_tess.tes", "shaders/zprepass_tess.frag");
    renderer->wireframeShader = createShaderProgramGeom("shaders/wireframe.vert", "shaders/wireframe.geom", "shaders/wireframe.frag");
    renderer->mainShader = createShaderProgramTess("shaders/main.vert", "shaders/main.tcs", "shaders/main.tes", "shaders/main.frag");
    renderer->debugBufferShader = createShaderProgram("shaders/debug_buffer.vert", "shaders/debug_buffer.frag");
    renderer->pointDepthShader = createShaderProgramGeom("shaders/depth_point.vert", "shaders/depth_point.geom", "shaders/depth_point.frag");
    renderer->spotDepthShader = createShaderProgram("shaders/depth_spot.vert", "shaders/depth_spot.frag");
    renderer->postProcessShader = createShaderProgram("shaders/postprocess.vert", "shaders/postprocess.frag");
    renderer->histogramShader = createShaderProgramCompute("shaders/histogram.comp");
    renderer->exposureShader = createShaderProgramCompute("shaders/exposure.comp");
    renderer->bloomShader = createShaderProgram("shaders/bloom.vert", "shaders/bloom.frag");
    renderer->bloomBlurShader = createShaderProgram("shaders/bloom_blur.vert", "shaders/bloom_blur.frag");
    renderer->dofShader = createShaderProgram("shaders/dof.vert", "shaders/dof.frag");
    renderer->volumetricShader = createShaderProgram("shaders/volumetric.vert", "shaders/volumetric.frag");
    renderer->volumetricBlurShader = createShaderProgram("shaders/volumetric_blur.vert", "shaders/volumetric_blur.frag");
    renderer->motionBlurShader = createShaderProgram("shaders/motion_blur.vert", "shaders/motion_blur.frag");
    renderer->ssaoShader = createShaderProgram("shaders/ssao.vert", "shaders/ssao.frag");
    renderer->ssaoBlurShader = createShaderProgram("shaders/ssao_blur.vert", "shaders/ssao_blur.frag");
    renderer->modelShadowShader = createShaderProgram("shaders/shadow_model.vert", "shaders/shadow_model.frag");
    renderer->ssrShader = createShaderProgram("shaders/ssr.vert", "shaders/ssr.frag");
    renderer->glassShader = createShaderProgram("shaders/glass.vert", "shaders/glass.frag");
    renderer->waterShader = createShaderProgram("shaders/water.vert", "shaders/water.frag");
    renderer->reflectiveGlassShader = createShaderProgram("shaders/reflective_glass.vert", "shaders/reflective_glass.frag");
    renderer->parallaxInteriorShader = createShaderProgram("shaders/parallax_interior.vert", "shaders/parallax_interior.frag");
    renderer->spriteShader = createShaderProgram("shaders/sprite.vert", "shaders/sprite.frag");
    renderer->blackholeShader = createShaderProgram("shaders/blackhole.vert", "shaders/blackhole.frag");
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    const int LOW_RES_WIDTH = engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
    const int LOW_RES_HEIGHT = engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
    glGenFramebuffers(1, &renderer->gBufferFBO); glBindFramebuffer(GL_FRAMEBUFFER, renderer->gBufferFBO);
    glGenTextures(1, &renderer->gLitColor); glBindTexture(GL_TEXTURE_2D, renderer->gLitColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->gLitColor, 0);
    glGenTextures(1, &renderer->gPosition); glBindTexture(GL_TEXTURE_2D, renderer->gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, renderer->gPosition, 0);
    glGenTextures(1, &renderer->gNormal); glBindTexture(GL_TEXTURE_2D, renderer->gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, renderer->gNormal, 0);
    glGenTextures(1, &renderer->gAlbedo); glBindTexture(GL_TEXTURE_2D, renderer->gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, renderer->gAlbedo, 0);
    glGenTextures(1, &renderer->gPBRParams); glBindTexture(GL_TEXTURE_2D, renderer->gPBRParams);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, renderer->gPBRParams, 0);
    glGenTextures(1, &renderer->gVelocity);
    glBindTexture(GL_TEXTURE_2D, renderer->gVelocity);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RG, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, renderer->gVelocity, 0);
    glGenTextures(1, &renderer->gGeometryNormal);
    glBindTexture(GL_TEXTURE_2D, renderer->gGeometryNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT6, GL_TEXTURE_2D, renderer->gGeometryNormal, 0);
    GLuint attachments[7] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6 };
    glDrawBuffers(7, attachments);
    GLuint rboDepth; glGenRenderbuffers(1, &rboDepth); glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, LOW_RES_WIDTH, LOW_RES_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Console_Printf("G-Buffer Framebuffer not complete!\n");
    const int bloom_width = engine->width / BLOOM_DOWNSAMPLE;
    const int bloom_height = engine->height / BLOOM_DOWNSAMPLE;
    glGenFramebuffers(1, &renderer->bloomFBO); glBindFramebuffer(GL_FRAMEBUFFER, renderer->bloomFBO);
    glGenTextures(1, &renderer->bloomBrightnessTexture); glBindTexture(GL_TEXTURE_2D, renderer->bloomBrightnessTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, bloom_width, bloom_height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->bloomBrightnessTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Console_Printf("Bloom FBO not complete!\n");
    glGenFramebuffers(2, renderer->pingpongFBO); glGenTextures(2, renderer->pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->pingpongFBO[i]); glBindTexture(GL_TEXTURE_2D, renderer->pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, bloom_width, bloom_height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 0.0f, 0.0f, 0.0f, 1.0f }; glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->pingpongColorbuffers[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Console_Printf("Ping-pong FBO %d not complete!\n", i);
    }
    glGenFramebuffers(1, &renderer->finalRenderFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->finalRenderFBO);
    glGenTextures(1, &renderer->finalRenderTexture);
    glBindTexture(GL_TEXTURE_2D, renderer->finalRenderTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, engine->width, engine->height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->finalRenderTexture, 0);
    glGenTextures(1, &renderer->finalDepthTexture);
    glBindTexture(GL_TEXTURE_2D, renderer->finalDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, engine->width, engine->height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderer->finalDepthTexture, 0);
    GLuint final_rboDepth; glGenRenderbuffers(1, &final_rboDepth); glBindRenderbuffer(GL_RENDERBUFFER, final_rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, engine->width, engine->height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, final_rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Final Render Framebuffer not complete!\n");
    glGenFramebuffers(1, &renderer->postProcessFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->postProcessFBO);
    glGenTextures(1, &renderer->postProcessTexture);
    glBindTexture(GL_TEXTURE_2D, renderer->postProcessTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, engine->width, engine->height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->postProcessTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Console_Printf("Post Process Framebuffer not complete!\n");
    glGenFramebuffers(1, &renderer->ssrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->ssrFBO);
    glGenTextures(1, &renderer->ssrTexture);
    glBindTexture(GL_TEXTURE_2D, renderer->ssrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, engine->width, engine->height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->ssrTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Console_Printf("SSR Framebuffer not complete!\n");
    glGenFramebuffers(1, &renderer->volumetricFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->volumetricFBO);
    glGenTextures(1, &renderer->volumetricTexture);
    glBindTexture(GL_TEXTURE_2D, renderer->volumetricTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, engine->width / VOLUMETRIC_DOWNSAMPLE, engine->height / VOLUMETRIC_DOWNSAMPLE, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->volumetricTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Console_Printf("Volumetric FBO not complete!\n");
    glGenFramebuffers(2, renderer->volPingpongFBO);
    glGenTextures(2, renderer->volPingpongTextures);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->volPingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, renderer->volPingpongTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, engine->width / VOLUMETRIC_DOWNSAMPLE, engine->height / VOLUMETRIC_DOWNSAMPLE, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->volPingpongTextures[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) Console_Printf("Volumetric Ping-Pong FBO %d not complete!\n", i);
    }
    glGenFramebuffers(1, &renderer->sunShadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->sunShadowFBO);
    glGenTextures(1, &renderer->sunShadowMap);
    glBindTexture(GL_TEXTURE_2D, renderer->sunShadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SUN_SHADOW_MAP_SIZE, SUN_SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    {
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderer->sunShadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Console_Printf("Sun Shadow Framebuffer not complete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glGenVertexArrays(1, &renderer->quadVAO); glGenBuffers(1, &renderer->quadVBO);
    glBindVertexArray(renderer->quadVAO); glBindBuffer(GL_ARRAY_BUFFER, renderer->quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))); glEnableVertexAttribArray(1);
    glGenVertexArrays(1, &renderer->parallaxRoomVAO); glGenBuffers(1, &renderer->parallaxRoomVBO);
    glBindVertexArray(renderer->parallaxRoomVAO); glBindBuffer(GL_ARRAY_BUFFER, renderer->parallaxRoomVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(parallaxRoomVertices), parallaxRoomVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(8 * sizeof(float))); glEnableVertexAttribArray(3);
    glBindVertexArray(0);
    renderer->brdfLUTTexture = TextureManager_LoadLUT("brdf_lut.png");
    if (renderer->brdfLUTTexture == 0) {
        Console_Printf_Error("[ERROR] Failed to load brdf_lut.png! Ensure it's in the 'textures' folder.");
    }
    glUseProgram(renderer->mainShader);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "diffuseMap"), 0); glUniform1i(glGetUniformLocation(renderer->mainShader, "normalMap"), 1);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "rmaMap"), 2); glUniform1i(glGetUniformLocation(renderer->mainShader, "heightMap"), 3); glUniform1i(glGetUniformLocation(renderer->mainShader, "detailDiffuseMap"), 7);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "environmentMap"), 10);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "brdfLUT"), 16);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "diffuseMap2"), 12);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "normalMap2"), 13);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "rmaMap2"), 14);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "heightMap2"), 15);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "diffuseMap3"), 17);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "normalMap3"), 18);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "rmaMap3"), 19);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "heightMap3"), 20);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "diffuseMap4"), 21);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "normalMap4"), 22);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "rmaMap4"), 23);
    glUniform1i(glGetUniformLocation(renderer->mainShader, "heightMap4"), 24);
    glUseProgram(renderer->volumetricShader);
    glUniform1i(glGetUniformLocation(renderer->volumetricShader, "gPosition"), 0);
    glUseProgram(renderer->volumetricBlurShader);
    glUniform1i(glGetUniformLocation(renderer->volumetricBlurShader, "image"), 0);
    glUseProgram(renderer->skyboxShader);
    glUseProgram(renderer->postProcessShader);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "sceneTexture"), 0);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "bloomBlur"), 1);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "gPosition"), 2);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "volumetricTexture"), 3);
    glUseProgram(renderer->bloomShader); glUniform1i(glGetUniformLocation(renderer->bloomShader, "sceneTexture"), 0);
    glUseProgram(renderer->bloomBlurShader); glUniform1i(glGetUniformLocation(renderer->bloomBlurShader, "image"), 0);
    glUseProgram(renderer->dofShader);
    glUniform1i(glGetUniformLocation(renderer->dofShader, "screenTexture"), 0);
    glUniform1i(glGetUniformLocation(renderer->dofShader, "depthTexture"), 1);
    mat4_identity(&renderer->prevViewProjection);
    glGenBuffers(1, &renderer->exposureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer->exposureSSBO);
    struct GPUExposureData {
        float exposure;
    } initialData = { 1.0f };
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(initialData), &initialData, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, renderer->exposureSSBO);
    glGenBuffers(1, &renderer->histogramSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer->histogramSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 256 * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderer->histogramSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    const int ssao_width = engine->width / SSAO_DOWNSAMPLE;
    const int ssao_height = engine->height / SSAO_DOWNSAMPLE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glGenFramebuffers(1, &renderer->ssaoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->ssaoFBO);
    glGenTextures(1, &renderer->ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, renderer->ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ssao_width, ssao_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->ssaoColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Console_Printf("SSAO Framebuffer not complete!\n");
    glGenFramebuffers(1, &renderer->ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->ssaoBlurFBO);
    glGenTextures(1, &renderer->ssaoBlurColorBuffer);
    glBindTexture(GL_TEXTURE_2D, renderer->ssaoBlurColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ssao_width, ssao_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->ssaoBlurColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Console_Printf("SSAO Blur Framebuffer not complete!\n");
    int downsample = Cvar_GetInt("r_planar_downsample");
    if (downsample < 1) downsample = 1;
    int reflection_width = engine->width / downsample;
    int reflection_height = engine->height / downsample;
    glGenFramebuffers(1, &renderer->reflectionFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->reflectionFBO);
    glGenTextures(1, &renderer->reflectionTexture);
    glBindTexture(GL_TEXTURE_2D, renderer->reflectionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, reflection_width, reflection_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->reflectionTexture, 0);
    glGenRenderbuffers(1, &renderer->reflectionDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, renderer->reflectionDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, reflection_width, reflection_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderer->reflectionDepthRBO);

    glGenFramebuffers(1, &renderer->refractionFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->refractionFBO);
    glGenTextures(1, &renderer->refractionTexture);
    glBindTexture(GL_TEXTURE_2D, renderer->refractionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, reflection_width, reflection_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->refractionTexture, 0);
    glGenTextures(1, &renderer->refractionDepthTexture);
    glBindTexture(GL_TEXTURE_2D, renderer->refractionDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, reflection_width, reflection_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderer->refractionDepthTexture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(renderer->ssaoShader);
    glUniform1i(glGetUniformLocation(renderer->ssaoShader, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(renderer->ssaoShader, "gGeometryNormal"), 1);
    glUniform1i(glGetUniformLocation(renderer->ssaoShader, "texNoise"), 2);
    glUseProgram(renderer->ssaoBlurShader);
    glUniform1i(glGetUniformLocation(renderer->ssaoBlurShader, "ssaoInput"), 0);
    glUseProgram(renderer->postProcessShader);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "ssao"), 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUniform1i(glGetUniformLocation(renderer->ssaoBlurShader, "ssaoInput"), 0);
    glUseProgram(renderer->postProcessShader);
    glUniform1i(glGetUniformLocation(renderer->postProcessShader, "ssao"), 4);
    glUseProgram(renderer->waterShader);
    glUniform1i(glGetUniformLocation(renderer->waterShader, "dudvMap"), 0);
    glUniform1i(glGetUniformLocation(renderer->waterShader, "normalMap"), 1);
    glUniform1i(glGetUniformLocation(renderer->waterShader, "reflectionMap"), 2);
    WaterManager_Init();
    WaterManager_ParseWaters("waters.def");
    renderer->cloudTexture = loadTexture("clouds.png", false, TEXTURE_LOAD_CONTEXT_WORLD);
    if (renderer->cloudTexture == 0) {
        Console_Printf_Error("[ERROR] Failed to load clouds.png! Ensure it's in the 'textures' folder.");
    }
    glGenBuffers(1, &renderer->lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer->lightSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_LIGHTS * sizeof(ShaderLight), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, renderer->lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Beams_Init();
    Cable_Init();
    Overlay_Init();
    Glow_Init();
    Decals_Init(&renderer);
    Skybox_Init(renderer);
    VideoPlayer_InitSystem();
    const GLubyte* gpu = glGetString(GL_RENDERER);
    const GLubyte* gl_version = glGetString(GL_VERSION);
    Console_Printf("------------------------------------------------------\n");
    Console_Printf("Renderer Context Initialized:\n");
    Console_Printf("  GPU: %s\n", (const char*)gpu);
    Console_Printf("  OpenGL Version: %s\n", (const char*)gl_version);
    Console_Printf("------------------------------------------------------\n");
}

void Renderer_Present(GLuint source_fbo, Engine* engine) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, engine->width, engine->height, 0, 0, engine->width, engine->height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer_Shutdown(Renderer* renderer) {
    glDeleteProgram(renderer->mainShader);
    glDeleteProgram(renderer->pointDepthShader);
    glDeleteProgram(renderer->zPrepassShader);
    glDeleteProgram(renderer->debugBufferShader);
    glDeleteProgram(renderer->spotDepthShader);
    glDeleteProgram(renderer->skyboxShader);
    glDeleteProgram(renderer->postProcessShader);
    glDeleteProgram(renderer->bloomShader);
    glDeleteProgram(renderer->bloomBlurShader);
    glDeleteProgram(renderer->dofShader);
    glDeleteProgram(renderer->ssaoShader);
    glDeleteProgram(renderer->ssaoBlurShader);
    glDeleteProgram(renderer->parallaxInteriorShader);
    glDeleteProgram(renderer->ssrShader);
    glDeleteProgram(renderer->volumetricShader);
    glDeleteProgram(renderer->volumetricBlurShader);
    glDeleteProgram(renderer->histogramShader);
    glDeleteProgram(renderer->exposureShader);
    glDeleteProgram(renderer->modelShadowShader);
    glDeleteProgram(renderer->motionBlurShader);
    glDeleteProgram(renderer->waterShader);
    glDeleteProgram(renderer->glassShader);
    glDeleteProgram(renderer->blackholeShader);
    glDeleteFramebuffers(1, &renderer->gBufferFBO);
    glDeleteTextures(1, &renderer->gLitColor);
    glDeleteTextures(1, &renderer->gPosition);
    glDeleteTextures(1, &renderer->gNormal);
    glDeleteTextures(1, &renderer->gAlbedo);
    glDeleteTextures(1, &renderer->gGeometryNormal);
    glDeleteTextures(1, &renderer->gPBRParams);
    glDeleteTextures(1, &renderer->gVelocity);
    glDeleteFramebuffers(1, &renderer->ssaoFBO);
    glDeleteFramebuffers(1, &renderer->ssaoBlurFBO);
    glDeleteTextures(1, &renderer->ssaoColorBuffer);
    glDeleteTextures(1, &renderer->ssaoBlurColorBuffer);
    glDeleteFramebuffers(1, &renderer->ssrFBO);
    glDeleteTextures(1, &renderer->ssrTexture);
    glDeleteFramebuffers(1, &renderer->finalRenderFBO);
    glDeleteTextures(1, &renderer->finalRenderTexture);
    glDeleteTextures(1, &renderer->finalDepthTexture);
    glDeleteFramebuffers(1, &renderer->postProcessFBO);
    glDeleteTextures(1, &renderer->postProcessTexture);
    glDeleteVertexArrays(1, &renderer->quadVAO);
    glDeleteBuffers(1, &renderer->quadVBO);
    glDeleteVertexArrays(1, &renderer->skyboxVAO);
    glDeleteBuffers(1, &renderer->skyboxVBO);
    glDeleteProgram(renderer->spriteShader);
    glDeleteVertexArrays(1, &renderer->spriteVAO);
    glDeleteBuffers(1, &renderer->spriteVBO);
    glDeleteFramebuffers(1, &renderer->sunShadowFBO);
    glDeleteTextures(1, &renderer->sunShadowMap);
    glDeleteVertexArrays(1, &renderer->parallaxRoomVAO); glDeleteBuffers(1, &renderer->parallaxRoomVBO);
    glDeleteFramebuffers(1, &renderer->bloomFBO); glDeleteTextures(1, &renderer->bloomBrightnessTexture);
    glDeleteFramebuffers(2, renderer->pingpongFBO); glDeleteTextures(2, renderer->pingpongColorbuffers);
    glDeleteFramebuffers(1, &renderer->volumetricFBO);
    glDeleteTextures(1, &renderer->volumetricTexture);
    glDeleteFramebuffers(2, renderer->volPingpongFBO);
    glDeleteTextures(2, renderer->volPingpongTextures);
    glDeleteBuffers(1, &renderer->lightSSBO);
    glDeleteBuffers(1, &renderer->histogramSSBO);
    glDeleteBuffers(1, &renderer->exposureSSBO);
    Beams_Shutdown();
    Cable_Shutdown();
    Overlay_Shutdown();
    Glow_Shutdown();
    Decals_Shutdown(&renderer);
    Skybox_Shutdown(renderer);
    VideoPlayer_ShutdownSystem();
}