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
#include "gl_sprites.h"
#include "gl_misc.h"

static float sprite_vertices[] = {
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
    -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
     0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
};

void Sprites_Init(Renderer* renderer) {
    renderer->spriteShader = createShaderProgram("shaders/sprite.vert", "shaders/sprite.frag");

    glGenVertexArrays(1, &renderer->spriteVAO);
    glGenBuffers(1, &renderer->spriteVBO);
    glBindVertexArray(renderer->spriteVAO);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->spriteVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sprite_vertices), sprite_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void Sprites_Shutdown(Renderer* renderer) {
    glDeleteProgram(renderer->spriteShader);
    glDeleteVertexArrays(1, &renderer->spriteVAO);
    glDeleteBuffers(1, &renderer->spriteVBO);
}

void Sprites_Render(Renderer* renderer, Scene* scene, Mat4* view, Mat4* projection) {
    glUseProgram(renderer->spriteShader);
    glUniformMatrix4fv(glGetUniformLocation(renderer->spriteShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->spriteShader, "projection"), 1, GL_FALSE, projection->m);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(renderer->spriteVAO);

    for (int i = 0; i < scene->numSprites; ++i) {
        Sprite* s = &scene->sprites[i];
        if (!s->visible) continue;

        glUniform3fv(glGetUniformLocation(renderer->spriteShader, "spritePos"), 1, &s->pos.x);
        glUniform1f(glGetUniformLocation(renderer->spriteShader, "spriteScale"), s->scale);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s->material->diffuseMap);
        glUniform1i(glGetUniformLocation(renderer->spriteShader, "spriteTexture"), 0);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}