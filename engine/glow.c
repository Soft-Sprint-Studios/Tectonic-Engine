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
#include "glow.h"
#include "gl_misc.h"
#include "io_system.h"

static GLuint g_glow_shader = 0;
static GLuint g_glow_vao = 0;
static GLuint g_glow_vbo = 0;

typedef struct {
    Vec3 pos;
    float size;
    Vec3 color;
} GlowVertex;

void Glow_Init(void) {
    g_glow_shader = createShaderProgramGeom("shaders/glow.vert", "shaders/glow.geom", "shaders/glow.frag");
    glGenVertexArrays(1, &g_glow_vao);
    glGenBuffers(1, &g_glow_vbo);
    glBindVertexArray(g_glow_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_glow_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_LOGIC_ENTITIES * sizeof(GlowVertex), NULL, GL_STREAM_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GlowVertex), (void*)offsetof(GlowVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(GlowVertex), (void*)offsetof(GlowVertex, size));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GlowVertex), (void*)offsetof(GlowVertex, color));

    glBindVertexArray(0);
}

void Glow_Shutdown(void) {
    if (g_glow_shader) glDeleteProgram(g_glow_shader);
    if (g_glow_vao) glDeleteVertexArrays(1, &g_glow_vao);
    if (g_glow_vbo) glDeleteBuffers(1, &g_glow_vbo);
}

void Glow_Render(Scene* scene, Mat4 view, Mat4 projection) {
    glUseProgram(g_glow_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_glow_shader, "view"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(g_glow_shader, "projection"), 1, GL_FALSE, projection.m);

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    GlowVertex vbo_data[MAX_LOGIC_ENTITIES];
    int glow_count = 0;

    for (int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        if (strcmp(ent->classname, "env_glow") == 0 && ent->runtime_active) {
            if (glow_count >= MAX_LOGIC_ENTITIES) break;

            vbo_data[glow_count].pos = ent->pos;
            vbo_data[glow_count].size = atof(LogicEntity_GetProperty(ent, "glow_size", "10.0"));
            sscanf(LogicEntity_GetProperty(ent, "color", "1.0 0.8 0.2"), "%f %f %f", 
                   &vbo_data[glow_count].color.x, &vbo_data[glow_count].color.y, &vbo_data[glow_count].color.z);

            glow_count++;
        }
    }

    if (glow_count > 0) {
        glBindVertexArray(g_glow_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_glow_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, glow_count * sizeof(GlowVertex), vbo_data);
        glDrawArrays(GL_POINTS, 0, glow_count);
        glBindVertexArray(0);
    }
    
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}