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
#include "gl_overlay.h"
#include "gl_misc.h"
#include "io_system.h"

static GLuint g_overlay_shader = 0;
static GLuint g_overlay_vao = 0;
static GLuint g_overlay_vbo = 0;

void Overlay_Init(void) {
    g_overlay_shader = createShaderProgram("shaders/overlay.vert", "shaders/overlay.frag");
    
    float quadVertices[] = { -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    glGenVertexArrays(1, &g_overlay_vao);
    glGenBuffers(1, &g_overlay_vbo);
    glBindVertexArray(g_overlay_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_overlay_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void Overlay_Shutdown(void) {
    if (g_overlay_shader) glDeleteProgram(g_overlay_shader);
    if (g_overlay_vao) glDeleteVertexArrays(1, &g_overlay_vao);
    if (g_overlay_vbo) glDeleteBuffers(1, &g_overlay_vbo);
}

void Overlay_Render(Scene* scene, Engine* engine) {
    for (int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        if (strcmp(ent->classname, "env_overlay") == 0 && ent->runtime_active) {
            const char* material_name = LogicEntity_GetProperty(ent, "material", "");
            Material* mat = TextureManager_FindMaterial(material_name);
            if (!mat || mat == &g_MissingMaterial) {
                continue;
            }

            glUseProgram(g_overlay_shader);
            
            int render_mode = atoi(LogicEntity_GetProperty(ent, "rendermode", "0"));

            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);

            if (render_mode == 0) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            } else {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mat->diffuseMap);
            glUniform1i(glGetUniformLocation(g_overlay_shader, "overlayTexture"), 0);
            glUniform1i(glGetUniformLocation(g_overlay_shader, "u_rendermode"), render_mode);

            glBindVertexArray(g_overlay_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            return; 
        }
    }
}