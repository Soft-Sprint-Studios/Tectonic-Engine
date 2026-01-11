/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
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
#include "gl_monitor.h"
#include "gl_misc.h"
#include "gl_geometry.h"
#include "gl_skybox.h"
#include "gl_planar.h"
#include "gl_console.h"
#include "io_system.h"
#include "cvar.h"
#include <stdlib.h>
#include <string.h>

// function based on CreateScanlineTexture from pathos engine
static void CreateScanlineTexture(Renderer* renderer) {
    unsigned int dataSize = 64 * 64 * 4;
    unsigned char* pscanlinetexture = (unsigned char*)malloc(dataSize * sizeof(unsigned char));
    if (!pscanlinetexture) return;

    for (int y = 0; y < 64; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            unsigned char* pdata = pscanlinetexture + (y * 64 + x) * 4;

            pdata[0] = 0;
            pdata[1] = 0;
            pdata[2] = 0;

            pdata[3] = ((y % 2) == 1) ? 64 : 0;
        }
    }

    glGenTextures(1, &renderer->scanlineTexture);
    glBindTexture(GL_TEXTURE_2D, renderer->scanlineTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, pscanlinetexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    free(pscanlinetexture);
}

void Monitor_Init(Renderer* renderer) {
    renderer->monitorShader = createShaderProgram("shaders/monitor.vert", "shaders/monitor.frag");
    CreateScanlineTexture(renderer);
}

void Monitor_Shutdown(Renderer* renderer) {
    glDeleteProgram(renderer->monitorShader);
    if (renderer->scanlineTexture) glDeleteTextures(1, &renderer->scanlineTexture);
}

static void InitCameraFBO(LogicEntity* cam) {
    if (cam->monitor_fbo != 0) return;

    const char* resStr = LogicEntity_GetProperty(cam, "resolution", "512");
    cam->monitor_resolution = atoi(resStr);
    if (cam->monitor_resolution <= 0) cam->monitor_resolution = 512;

    const char* onceStr = LogicEntity_GetProperty(cam, "render_once", "0");
    cam->monitor_render_once = (atoi(onceStr) == 1);
    cam->monitor_has_rendered = false;

    const char* fovStr = LogicEntity_GetProperty(cam, "fov", "90");
    cam->monitor_fov = atof(fovStr);
    if (cam->monitor_fov <= 0.0f) cam->monitor_fov = 90.0f;

    glGenFramebuffers(1, &cam->monitor_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, cam->monitor_fbo);

    glGenTextures(1, &cam->monitor_texture);
    glBindTexture(GL_TEXTURE_2D, cam->monitor_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, cam->monitor_resolution, cam->monitor_resolution, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cam->monitor_texture, 0);

    glGenRenderbuffers(1, &cam->monitor_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, cam->monitor_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, cam->monitor_resolution, cam->monitor_resolution);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, cam->monitor_depth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Console_Printf_Error("Monitor FBO incomplete for entity %s", cam->targetname);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Monitor_RenderCameras(Scene* scene, Renderer* renderer, Engine* engine, const Mat4* sunLightSpaceMatrix) {
    GLint prev_viewport[4];
    glGetIntegerv(GL_VIEWPORT, prev_viewport);

    for (int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        if (strcmp(ent->classname, "info_monitorcamera") != 0) continue;

        if (ent->monitor_fbo == 0) InitCameraFBO(ent);
        if (ent->monitor_render_once && ent->monitor_has_rendered) continue;

        float pitch = ent->rot.x * (float)(M_PI / 180.0f);
        float yaw = ent->rot.y * (float)(M_PI / 180.0f);

        Vec3 forward;
        forward.x = cosf(pitch) * sinf(yaw);
        forward.y = sinf(pitch);
        forward.z = -cosf(pitch) * cosf(yaw);
        vec3_normalize(&forward);

        Vec3 target = vec3_add(ent->pos, forward);
        Vec3 up = { 0.0f, 1.0f, 0.0f };
        if (fabs(forward.y) > 0.99f) up = (Vec3){ 1.0f, 0.0f, 0.0f };

        Mat4 view = mat4_lookAt(ent->pos, target, up);
        Mat4 projection = mat4_perspective(ent->monitor_fov * (float)(M_PI / 180.0f), 1.0f, 0.1f, 1000.0f);

        Geometry_RenderPass(renderer, scene, engine, &view, &projection, sunLightSpaceMatrix, ent->pos, false, true);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->gBufferFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ent->monitor_fbo);

        glBlitFramebuffer(0, 0, engine->width, engine->height,
            0, 0, ent->monitor_resolution, ent->monitor_resolution,
            GL_COLOR_BUFFER_BIT, GL_LINEAR);

        glBlitFramebuffer(0, 0, engine->width, engine->height,
            0, 0, ent->monitor_resolution, ent->monitor_resolution,
            GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, ent->monitor_fbo);
        glViewport(0, 0, ent->monitor_resolution, ent->monitor_resolution);

        if (Cvar_GetInt("r_water")) {
            Planar_RenderWater(renderer, scene, engine, &view, &projection, sunLightSpaceMatrix);
        }

        Skybox_Render(renderer, scene, engine, &view, &projection);

        ent->monitor_has_rendered = true;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
}

void Monitor_RenderBrushes(Scene* scene, Renderer* renderer, Engine* engine, Mat4* view, Mat4* projection) {
    glUseProgram(renderer->monitorShader);

    glDepthFunc(GL_LEQUAL);

    glUniformMatrix4fv(glGetUniformLocation(renderer->monitorShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->monitorShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform3fv(glGetUniformLocation(renderer->monitorShader, "u_viewPos"), 1, &engine->camera.position.x);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, renderer->scanlineTexture);
    glUniform1i(glGetUniformLocation(renderer->monitorShader, "u_scanlineTexture"), 1);

    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        if (strcmp(b->classname, "func_monitor") != 0 || !b->runtime_active) continue;

        const char* targetName = Brush_GetProperty(b, "target", "");
        LogicEntity* camEnt = NULL;

        for (int k = 0; k < scene->numLogicEntities; ++k) {
            if (strcmp(scene->logicEntities[k].targetname, targetName) == 0 &&
                strcmp(scene->logicEntities[k].classname, "info_monitorcamera") == 0) {
                camEnt = &scene->logicEntities[k];
                break;
            }
        }

        if (!camEnt || camEnt->monitor_texture == 0) continue;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, camEnt->monitor_texture);
        glUniform1i(glGetUniformLocation(renderer->monitorShader, "u_renderTexture"), 0);

        bool grayscale = (atoi(Brush_GetProperty(b, "grayscale", "0")) == 1);
        glUniform1i(glGetUniformLocation(renderer->monitorShader, "u_grayscale"), grayscale);

        glUniformMatrix4fv(glGetUniformLocation(renderer->monitorShader, "model"), 1, GL_FALSE, b->modelMatrix.m);

        glBindVertexArray(b->vao);
        glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
    }
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}