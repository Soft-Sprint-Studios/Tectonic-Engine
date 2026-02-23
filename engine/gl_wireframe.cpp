/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
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
#include "gl_wireframe.h"
#include "gl_misc.h"
#include "cvar.h"
#include "map_misc.h"

void Wireframe_Render(Renderer* renderer, Scene* scene, Mat4* view, Mat4* projection) {
    if (!Cvar_GetInt("r_wireframe")) return;

    glUseProgram(renderer->wireframeShader);
    Shader_Set(renderer->wireframeShader, "view", view);
    Shader_Set(renderer->wireframeShader, "projection", projection);
    Shader_Set(renderer->wireframeShader, "wireframeColor", Vec4{ 0.0f, 0.5f, 1.0f, 1.0f });

    glDisable(GL_DEPTH_TEST);
    for (Int i = 0; i < scene->numObjects; i++) {
        SceneObject* obj = &scene->objects[i];
        Shader_Set(renderer->wireframeShader, "model", &obj->modelMatrix);
        if (obj->model) {
            for (Int meshIdx = 0; meshIdx < obj->model->meshCount; ++meshIdx) {
                Mesh* mesh = &obj->model->meshes[meshIdx];
                glBindVertexArray(mesh->VAO);
                if (mesh->useEBO) {
                    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
                } else {
                    glDrawArrays(GL_TRIANGLES, 0, mesh->indexCount);
                }
            }
        }
    }
    for (Int i = 0; i < scene->numBrushes; i++) {
        Brush* b = &scene->brushes[i];
        if (strlen(b->classname) > 0 && !Brush_IsSolid(b)) continue;
        Shader_Set(renderer->wireframeShader, "model", &b->modelMatrix);
        glBindVertexArray(b->vao);
        glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
    }
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
}