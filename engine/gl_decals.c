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
#include "gl_decals.h"
#include "texturemanager.h"

static float decalQuadVertices[] = {
    -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 1.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   1.0f, 0.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 0.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,   0.0f, 1.0f,      0.0f, -1.0f,  0.0f, 1.0f,    0.0f, 0.0f, 0.0f, 1.0f,   0.0f, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f
};

void Decals_Init(Renderer* renderer) {
    glGenVertexArrays(1, &renderer->decalVAO);
    glGenBuffers(1, &renderer->decalVBO);
    glBindVertexArray(renderer->decalVAO);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->decalVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(decalQuadVertices), &decalQuadVertices, GL_STATIC_DRAW);
    size_t stride = 22 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, stride, (void*)(16 * sizeof(float)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride, (void*)(18 * sizeof(float)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, stride, (void*)(20 * sizeof(float)));
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(8);
    glBindVertexArray(0);
}

void Decals_Shutdown(Renderer* renderer) {
    if (renderer->decalVAO) glDeleteVertexArrays(1, &renderer->decalVAO);
    if (renderer->decalVBO) glDeleteBuffers(1, &renderer->decalVBO);
}

void Decals_Render(Scene* scene, Renderer* renderer, GLuint shader_program) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glUseProgram(shader_program);
    
    glUniform1i(glGetUniformLocation(shader_program, "isBrush"), 1);
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    for (int i = 0; i < scene->numDecals; ++i) {
        Decal* d = &scene->decals[i];

        glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, d->modelMatrix.m);
        glUniform1f(glGetUniformLocation(shader_program, "heightScale"), 0.0f);

        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, d->material->diffuseMap);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, d->material->normalMap);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, d->material->rmaMap);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, 0);

        bool has_lightmap = d->lightmapAtlas != 0 && d->lightmapAtlas != missingTextureID;
        glUniform1i(glGetUniformLocation(shader_program, "useLightmap"), has_lightmap);
        if (has_lightmap) {
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, d->lightmapAtlas);
            glUniform1i(glGetUniformLocation(shader_program, "lightmap"), 5);
        }

        bool has_dir_lightmap = d->directionalLightmapAtlas != 0 && d->directionalLightmapAtlas != missingTextureID;
        glUniform1i(glGetUniformLocation(shader_program, "useDirectionalLightmap"), has_dir_lightmap);
        if (has_dir_lightmap) {
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, d->directionalLightmapAtlas);
            glUniform1i(glGetUniformLocation(shader_program, "directionalLightmap"), 6);
        }

        glBindVertexArray(renderer->decalVAO);
        glDrawArrays(GL_PATCHES, 0, 6);
    }

    glUniform1i(glGetUniformLocation(shader_program, "isBrush"), 0);
    glUniform1i(glGetUniformLocation(shader_program, "useLightmap"), 0);
    glUniform1i(glGetUniformLocation(shader_program, "useDirectionalLightmap"), 0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}