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
#include "gl_render_misc.h"
#include "cvar.h"
#include "io_system.h"
#include <SDL_image.h>

void MiscRender_AutoexposurePass(Renderer* renderer, Engine* engine) {
    bool auto_exposure_enabled = Cvar_GetInt("r_autoexposure");

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer->histogramSSBO);
    GLuint zero = 0;
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    glUseProgram(renderer->histogramShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->gLitColor);
    glUniform1i(glGetUniformLocation(renderer->histogramShader, "u_inputTexture"), 0);
    glDispatchCompute((GLuint)(engine->width / 16), (GLuint)(engine->height / 16), 1);

    glUseProgram(renderer->exposureShader);
    glUniform1f(glGetUniformLocation(renderer->exposureShader, "u_autoexposure_key"), Cvar_GetFloat("r_autoexposure_key"));
    glUniform1f(glGetUniformLocation(renderer->exposureShader, "u_autoexposure_speed"), Cvar_GetFloat("r_autoexposure_speed"));
    glUniform1f(glGetUniformLocation(renderer->exposureShader, "u_deltaTime"), engine->deltaTime);
    glUniform1i(glGetUniformLocation(renderer->exposureShader, "u_autoexposure_enabled"), auto_exposure_enabled);

    glDispatchCompute(1, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void MiscRender_DoFPass(Renderer* renderer, Scene* scene, GLuint sourceTexture, GLuint sourceDepthTexture, GLuint destFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, destFBO);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(renderer->dofShader);
    glUniform1f(glGetUniformLocation(renderer->dofShader, "u_focusDistance"), scene->post.dofFocusDistance);
    glUniform1f(glGetUniformLocation(renderer->dofShader, "u_aperture"), scene->post.dofAperture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(renderer->dofShader, "screenTexture"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sourceDepthTexture);
    glUniform1i(glGetUniformLocation(renderer->dofShader, "depthTexture"), 1);
    glBindVertexArray(renderer->quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MiscRender_MotionBlurPass(Renderer* renderer, GLuint sourceTexture, GLuint destFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, destFBO);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(renderer->motionBlurShader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(renderer->motionBlurShader, "sceneTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, renderer->gVelocity);
    glUniform1i(glGetUniformLocation(renderer->motionBlurShader, "velocityTexture"), 1);

    glBindVertexArray(renderer->quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MiscRender_ParallaxRooms(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection) {
    glUseProgram(renderer->parallaxInteriorShader);
    glUniformMatrix4fv(glGetUniformLocation(renderer->parallaxInteriorShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->parallaxInteriorShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform3fv(glGetUniformLocation(renderer->parallaxInteriorShader, "viewPos"), 1, &engine->camera.position.x);

    for (int i = 0; i < scene->numParallaxRooms; ++i) {
        ParallaxRoom* p = &scene->parallaxRooms[i];
        if (p->cubemapTexture == 0) continue;

        glUniformMatrix4fv(glGetUniformLocation(renderer->parallaxInteriorShader, "model"), 1, GL_FALSE, p->modelMatrix.m);
        glUniform1f(glGetUniformLocation(renderer->parallaxInteriorShader, "roomDepth"), p->roomDepth);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, p->cubemapTexture);
        glUniform1i(glGetUniformLocation(renderer->parallaxInteriorShader, "roomCubemap"), 0);

        glBindVertexArray(renderer->parallaxRoomVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    glBindVertexArray(0);
}

void MiscRender_RefractiveGlass(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection) {
    glUseProgram(renderer->glassShader);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUniformMatrix4fv(glGetUniformLocation(renderer->glassShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->glassShader, "projection"), 1, GL_FALSE, projection->m);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->finalRenderTexture);
    glUniform1i(glGetUniformLocation(renderer->glassShader, "sceneTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glUniform1i(glGetUniformLocation(renderer->glassShader, "normalMap"), 1);

    for (int i = 0; i < scene->numBrushes; i++) {
        Brush* b = &scene->brushes[i];
        if (strcmp(b->classname, "env_glass") != 0) continue;

        const char* normal_map_name = Brush_GetProperty(b, "normal_map", "NULL");
        Material* normal_mat = TextureManager_FindMaterial(normal_map_name);
        if (normal_mat && normal_mat != &g_MissingMaterial) {
            glBindTexture(GL_TEXTURE_2D, normal_mat->normalMap);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, defaultNormalMapID);
        }

        glUniform1f(glGetUniformLocation(renderer->glassShader, "refractionStrength"), atof(Brush_GetProperty(b, "refraction_strength", "0.01")));
        glUniformMatrix4fv(glGetUniformLocation(renderer->glassShader, "model"), 1, GL_FALSE, b->modelMatrix.m);
        glBindVertexArray(b->vao);
        glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

static void SaveFramebufferToPNG(GLuint fbo, int width, int height, const char* filepath) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    unsigned char* pixels = (unsigned char*)malloc(width * height * 4);
    if (!pixels) {
        Console_Printf_Error("[ERROR] Failed to allocate memory for screenshot pixels.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, width * 4,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

    if (!surface) {
        Console_Printf_Error("[ERROR] Failed to create SDL surface for screenshot.");
    }
    else {
        if (IMG_SavePNG(surface, filepath) != 0) {
            Console_Printf_Error("[ERROR] Failed to save screenshot to %s: %s", filepath, IMG_GetError());
        }
        else {
            Console_Printf("Saved cubemap face to %s", filepath);
        }
        SDL_FreeSurface(surface);
    }

    free(pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MiscRender_SaveScreenshot(Engine* engine, const char* filepath) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    unsigned char* pixels = (unsigned char*)malloc(engine->width * engine->height * 4);
    if (!pixels) {
        Console_Printf_Error("[ERROR] Failed to allocate memory for screenshot pixels.");
        return;
    }

    glReadPixels(0, 0, engine->width, engine->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    int row_size = engine->width * 4;
    unsigned char* temp_row = (unsigned char*)malloc(row_size);
    if (!temp_row) {
        Console_Printf_Error("[ERROR] Failed to allocate memory for screenshot row buffer.");
        free(pixels);
        return;
    }

    for (int y = 0; y < engine->height / 2; ++y) {
        unsigned char* top = pixels + y * row_size;
        unsigned char* bottom = pixels + (engine->height - 1 - y) * row_size;
        memcpy(temp_row, top, row_size);
        memcpy(top, bottom, row_size);
        memcpy(bottom, temp_row, row_size);
    }
    free(temp_row);

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, engine->width, engine->height, 32, row_size, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

    if (!surface) {
        Console_Printf_Error("[ERROR] Failed to create SDL surface for screenshot.");
    }
    else {
        if (IMG_SavePNG(surface, filepath) != 0) {
            Console_Printf_Error("[ERROR] Failed to save screenshot to %s: %s", filepath, IMG_GetError());
        }
        else {
            Console_Printf("Screenshot saved to %s", filepath);
        }
        SDL_FreeSurface(surface);
    }
    free(pixels);
}

void MiscRender_BuildCubemaps(Renderer* renderer, Scene* scene, Engine* engine, int resolution) {
    Console_Printf("Starting cubemap build with %dx%d resolution...", resolution, resolution);
    glFinish();

    Camera original_camera = engine->camera;

    Vec3 targets[] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    Vec3 ups[] = { {0,-1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {0,-1,0}, {0,-1,0} };
    const char* suffixes[] = { "px", "nx", "py", "ny", "pz", "nz" };

    GLuint cubemap_fbo, cubemap_texture, cubemap_rbo;
    glGenFramebuffers(1, &cubemap_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, cubemap_fbo);
    glGenTextures(1, &cubemap_texture);
    glBindTexture(GL_TEXTURE_2D, cubemap_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, resolution, resolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cubemap_texture, 0);
    glGenRenderbuffers(1, &cubemap_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, cubemap_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, resolution, resolution);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, cubemap_rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Console_Printf_Error("[ERROR] Cubemap face FBO not complete!");
        glDeleteFramebuffers(1, &cubemap_fbo);
        glDeleteTextures(1, &cubemap_texture);
        glDeleteRenderbuffers(1, &cubemap_rbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        if (strcmp(b->classname, "env_reflectionprobe") != 0) {
            continue;
        }
        if (strlen(b->name) == 0) {
            Console_Printf_Warning("[WARNING] Skipping unnamed reflection probe at index %d.", i);
            continue;
        }

        Console_Printf("Building cubemap for probe '%s'...", b->name);

        for (int face_idx = 0; face_idx < 6; ++face_idx) {
            engine->camera.position = b->pos;
            Vec3 target_pos = vec3_add(engine->camera.position, targets[face_idx]);
            Mat4 view = mat4_lookAt(engine->camera.position, target_pos, ups[face_idx]);
            Mat4 projection = mat4_perspective(90.0f * (M_PI / 180.f), 1.0f, 0.1f, 1000.f);

            Shadows_RenderPointAndSpot(renderer, scene, engine);
            Mat4 sunLightSpaceMatrix;
            mat4_identity(&sunLightSpaceMatrix);
            if (scene->sun.enabled) {
                Calculate_Sun_Light_Space_Matrix(&sunLightSpaceMatrix, &scene->sun, engine->camera.position);
                Shadows_RenderSun(renderer, scene, &sunLightSpaceMatrix);
            }

            Geometry_RenderPass(renderer, scene, engine, &view, &projection, &sunLightSpaceMatrix, engine->camera.position, false);

            glBindFramebuffer(GL_FRAMEBUFFER, cubemap_fbo);
            glViewport(0, 0, resolution, resolution);
            if (Cvar_GetInt("r_clear")) {
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }

            glEnable(GL_FRAMEBUFFER_SRGB);

            const int LOW_RES_WIDTH = engine->width / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
            const int LOW_RES_HEIGHT = engine->height / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;

            glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->gBufferFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cubemap_fbo);
            glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, 0, resolution, resolution, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, 0, resolution, resolution, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

            glBindFramebuffer(GL_FRAMEBUFFER, cubemap_fbo);
            Skybox_Render(renderer, scene, engine, &view, &projection);

            glDisable(GL_FRAMEBUFFER_SRGB);

            char filepath[256];
            sprintf(filepath, "cubemaps/%s_%s.png", b->name, suffixes[face_idx]);
            SaveFramebufferToPNG(cubemap_fbo, resolution, resolution, filepath);
        }

        const char* face_paths[6];
        char paths_storage[6][256];
        for (int k = 0; k < 6; ++k) {
            sprintf(paths_storage[k], "cubemaps/%s_%s.png", b->name, suffixes[k]);
            face_paths[k] = paths_storage[k];
        }
        b->cubemapTexture = TextureManager_ReloadCubemap(face_paths, b->cubemapTexture);
    }

    glDeleteFramebuffers(1, &cubemap_fbo);
    glDeleteTextures(1, &cubemap_texture);
    glDeleteRenderbuffers(1, &cubemap_rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    engine->camera = original_camera;
    glViewport(0, 0, engine->width, engine->height);

    Console_Printf("Cubemap build finished.");
}