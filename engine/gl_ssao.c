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
#include "gl_ssao.h"
#include "gl_renderer.h"
#include "cvar.h"

void SSAO_RenderPass(Renderer* renderer, Engine* engine, Mat4* projection) {
    const int ssao_width = engine->width / SSAO_DOWNSAMPLE;
    const int ssao_height = engine->height / SSAO_DOWNSAMPLE;
    glViewport(0, 0, ssao_width, ssao_height);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(renderer->ssaoShader);
    glUniformMatrix4fv(glGetUniformLocation(renderer->ssaoShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform2f(glGetUniformLocation(renderer->ssaoShader, "screenSize"), (float)ssao_width, (float)ssao_height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->gPosition);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, renderer->gGeometryNormal);
    glBindVertexArray(renderer->quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, renderer->ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(renderer->ssaoBlurShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->ssaoColorBuffer);
    glBindVertexArray(renderer->quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glViewport(0, 0, engine->width, engine->height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}