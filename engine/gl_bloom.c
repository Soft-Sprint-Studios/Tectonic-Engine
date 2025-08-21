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
#include "gl_bloom.h"
#include "gl_renderer.h"

void Bloom_RenderPass(Renderer* renderer, Engine* engine) {
    glUseProgram(renderer->bloomShader); 
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->bloomFBO);
    glViewport(0, 0, engine->width / BLOOM_DOWNSAMPLE, engine->height / BLOOM_DOWNSAMPLE);
    glActiveTexture(GL_TEXTURE0); 
    glBindTexture(GL_TEXTURE_2D, renderer->gLitColor);
    glBindVertexArray(renderer->quadVAO); 
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    bool horizontal = true, first_iteration = true; 
    unsigned int amount = 10; 
    glUseProgram(renderer->bloomBlurShader);
    
    for (unsigned int i = 0; i < amount; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->pingpongFBO[horizontal]); 
        glUniform1i(glGetUniformLocation(renderer->bloomBlurShader, "horizontal"), horizontal);
        glActiveTexture(GL_TEXTURE0); 
        glBindTexture(GL_TEXTURE_2D, first_iteration ? renderer->bloomBrightnessTexture : renderer->pingpongColorbuffers[!horizontal]);
        glBindVertexArray(renderer->quadVAO); 
        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal; 
        if (first_iteration) first_iteration = false;
    }
    glViewport(0, 0, engine->width, engine->height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}