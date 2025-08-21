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
#include "gl_ssr.h"
#include "cvar.h"

void SSR_RenderPass(Renderer* renderer, Engine* engine, GLuint sourceTexture, GLuint destFBO, Mat4* view, Mat4* projection) {
    if (!Cvar_GetInt("r_ssr")) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->finalRenderFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFBO);
        glBlitFramebuffer(0, 0, engine->width, engine->height, 0, 0, engine->width, engine->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, destFBO);
    glViewport(0, 0, engine->width, engine->height);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(renderer->ssrShader);

    glUniformMatrix4fv(glGetUniformLocation(renderer->ssrShader, "projection"), 1, GL_FALSE, projection->m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->ssrShader, "view"), 1, GL_FALSE, view->m);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(renderer->ssrShader, "colorBuffer"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, renderer->gNormal);
    glUniform1i(glGetUniformLocation(renderer->ssrShader, "gNormal"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, renderer->gPBRParams);
    glUniform1i(glGetUniformLocation(renderer->ssrShader, "ssrValuesMap"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, renderer->gPosition);
    glUniform1i(glGetUniformLocation(renderer->ssrShader, "gPosition"), 3);

    glBindVertexArray(renderer->quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
}