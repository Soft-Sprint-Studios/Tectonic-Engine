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
#include "gl_volumetrics.h"
#include "gl_renderer.h"

void Volumetrics_RenderPass(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix) {
    bool should_render_volumetrics = false;
    if (scene->sun.enabled && scene->sun.volumetricIntensity > 0.001f) {
        should_render_volumetrics = true;
    }
    if (!should_render_volumetrics) {
        for (int i = 0; i < scene->numActiveLights; ++i) {
            if (scene->lights[i].intensity > 0.001f && scene->lights[i].volumetricIntensity > 0.001f) {
                should_render_volumetrics = true;
                break;
            }
        }
    }

    if (!should_render_volumetrics) {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->volumetricFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->volPingpongFBO[0]);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->volumetricFBO);
    glViewport(0, 0, engine->width / VOLUMETRIC_DOWNSAMPLE, engine->height / VOLUMETRIC_DOWNSAMPLE);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(renderer->volumetricShader);
    glUniform3fv(glGetUniformLocation(renderer->volumetricShader, "viewPos"), 1, &engine->camera.position.x);

    Mat4 invView, invProj;
    mat4_inverse(view, &invView);
    mat4_inverse(projection, &invProj);
    glUniformMatrix4fv(glGetUniformLocation(renderer->volumetricShader, "invView"), 1, GL_FALSE, invView.m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->volumetricShader, "invProjection"), 1, GL_FALSE, invProj.m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->volumetricShader, "projection"), 1, GL_FALSE, projection->m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->volumetricShader, "view"), 1, GL_FALSE, view->m);

    glUniform1i(glGetUniformLocation(renderer->volumetricShader, "numActiveLights"), scene->numActiveLights);

    glUniform1i(glGetUniformLocation(renderer->volumetricShader, "sun.enabled"), scene->sun.enabled);
    if (scene->sun.enabled) {
        glActiveTexture(GL_TEXTURE15);
        glBindTexture(GL_TEXTURE_2D, renderer->sunShadowMap);
        glUniform1i(glGetUniformLocation(renderer->volumetricShader, "sunShadowMap"), 15);
        glUniformMatrix4fv(glGetUniformLocation(renderer->volumetricShader, "sunLightSpaceMatrix"), 1, GL_FALSE, sunLightSpaceMatrix->m);
        glUniform3fv(glGetUniformLocation(renderer->volumetricShader, "sun.direction"), 1, &scene->sun.direction.x);
        glUniform3fv(glGetUniformLocation(renderer->volumetricShader, "sun.color"), 1, &scene->sun.color.x);
        glUniform1f(glGetUniformLocation(renderer->volumetricShader, "sun.intensity"), scene->sun.intensity);
        glUniform1f(glGetUniformLocation(renderer->volumetricShader, "sun.volumetricIntensity"), scene->sun.volumetricIntensity / 100.0f);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->gPosition);

    glBindVertexArray(renderer->quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    bool horizontal = true, first_iteration = true;
    unsigned int amount = 4;
    glUseProgram(renderer->volumetricBlurShader);
    for (unsigned int i = 0; i < amount; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->volPingpongFBO[horizontal]);
        glUniform1i(glGetUniformLocation(renderer->volumetricBlurShader, "horizontal"), horizontal);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, first_iteration ? renderer->volumetricTexture : renderer->volPingpongTextures[!horizontal]);
        glBindVertexArray(renderer->quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, engine->width, engine->height);
}