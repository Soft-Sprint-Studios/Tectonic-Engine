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
#include "gl_skybox.h"
#include "gl_misc.h"

static float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
};

void Skybox_Init(Renderer* renderer) {
    renderer->skyboxShader = createShaderProgram("shaders/skybox.vert", "shaders/skybox.frag");
    glGenVertexArrays(1, &renderer->skyboxVAO);
    glGenBuffers(1, &renderer->skyboxVBO);
    glBindVertexArray(renderer->skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Skybox_Shutdown(Renderer* renderer) {
    glDeleteProgram(renderer->skyboxShader);
    glDeleteVertexArrays(1, &renderer->skyboxVAO);
    glDeleteBuffers(1, &renderer->skyboxVBO);
}

void Skybox_Render(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection) {
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->finalRenderFBO);
    glDepthFunc(GL_LEQUAL);
    glUseProgram(renderer->skyboxShader);
    glCullFace(GL_FRONT);
    glUniform1i(glGetUniformLocation(renderer->skyboxShader, "u_use_cubemap"), scene->use_cubemap_skybox);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, scene->skybox_cubemap);
    glUniform1i(glGetUniformLocation(renderer->skyboxShader, "u_skybox_cubemap"), 1);
    glUniformMatrix4fv(glGetUniformLocation(renderer->skyboxShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->skyboxShader, "projection"), 1, GL_FALSE, projection->m);

    Vec3 sunDirNormalized = scene->sun.direction;
    vec3_normalize(&sunDirNormalized);

    glUniform3fv(glGetUniformLocation(renderer->skyboxShader, "sunDirection"), 1, &sunDirNormalized.x);
    glUniform3fv(glGetUniformLocation(renderer->skyboxShader, "cameraPos"), 1, &engine->camera.position.x);
    glUniform1i(glGetUniformLocation(renderer->skyboxShader, "cloudMap"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->cloudTexture);

    glUniform1f(glGetUniformLocation(renderer->skyboxShader, "time"), engine->scaledTime);

    glBindVertexArray(renderer->skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glCullFace(GL_BACK);
    glDepthFunc(GL_LESS);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}