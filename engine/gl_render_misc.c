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