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
#include "beams.h"
#include "gl_misc.h"
#include "io_system.h"
#include "cvar.h"

static GLuint g_beam_shader = 0;
static GLuint g_beam_vao = 0;
static GLuint g_beam_vbo = 0;

void Beams_Init(void) {
    g_beam_shader = createShaderProgram("shaders/beam.vert", "shaders/beam.frag");
    glGenVertexArrays(1, &g_beam_vao);
    glGenBuffers(1, &g_beam_vbo);
    glBindVertexArray(g_beam_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_beam_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 5, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void Beams_Shutdown(void) {
    if (g_beam_shader) glDeleteProgram(g_beam_shader);
    if (g_beam_vao) glDeleteVertexArrays(1, &g_beam_vao);
    if (g_beam_vbo) glDeleteBuffers(1, &g_beam_vbo);
}

void Beams_Render(Scene* scene, Mat4 view, Mat4 projection, Vec3 cameraPos, float time) {
    glUseProgram(g_beam_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_beam_shader, "view"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(g_beam_shader, "projection"), 1, GL_FALSE, projection.m);
    glUniform1f(glGetUniformLocation(g_beam_shader, "u_time"), time);

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glBindVertexArray(g_beam_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_beam_vbo);

    for (int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        if (strcmp(ent->classname, "env_beam") == 0 && ent->runtime_active) {
            const char* target_name = LogicEntity_GetProperty(ent, "target", "");
            Vec3 end_pos;
            Vec3 end_angles_dummy;

            if (IO_FindNamedEntity(scene, target_name, &end_pos, &end_angles_dummy)) {
                Vec3 start_pos = ent->pos;
                float width = atof(LogicEntity_GetProperty(ent, "width", "2.0"));
                Vec3 color;
                sscanf(LogicEntity_GetProperty(ent, "color", "1.0 1.0 1.0"), "%f %f %f", &color.x, &color.y, &color.z);

                glUniform3fv(glGetUniformLocation(g_beam_shader, "u_color"), 1, &color.x);

                Vec3 view_vec = vec3_sub(start_pos, cameraPos);
                Vec3 beam_dir = vec3_sub(end_pos, start_pos);
                Vec3 right = vec3_cross(beam_dir, view_vec);
                vec3_normalize(&right);
                right = vec3_muls(right, width * 0.5f);

                float vertices[] = {
                    start_pos.x - right.x, start_pos.y - right.y, start_pos.z - right.z, 0.0f, 0.0f,
                    start_pos.x + right.x, start_pos.y + right.y, start_pos.z + right.z, 1.0f, 0.0f,
                    end_pos.x + right.x,   end_pos.y + right.y,   end_pos.z + right.z,   1.0f, 1.0f,

                    end_pos.x + right.x,   end_pos.y + right.y,   end_pos.z + right.z,   1.0f, 1.0f,
                    end_pos.x - right.x,   end_pos.y - right.y,   end_pos.z - right.z,   0.0f, 1.0f,
                    start_pos.x - right.x, start_pos.y - right.y, start_pos.z - right.z, 0.0f, 0.0f
                };

                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}