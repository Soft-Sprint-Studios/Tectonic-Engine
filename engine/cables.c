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
#include "map.h"
#include "gl_misc.h"
#include "io_system.h"
#include <stdlib.h>

static GLuint g_cable_shader = 0;
static GLuint g_cable_vao = 0;
static GLuint g_cable_vbo = 0;

void Cable_Init(void) {
    g_cable_shader = createShaderProgram("shaders/cable.vert", "shaders/cable.frag");
    glGenVertexArrays(1, &g_cable_vao);
    glGenBuffers(1, &g_cable_vbo);
    glBindVertexArray(g_cable_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_cable_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Cable_Shutdown(void) {
    if (g_cable_shader) glDeleteProgram(g_cable_shader);
    if (g_cable_vao) glDeleteVertexArrays(1, &g_cable_vao);
    if (g_cable_vbo) glDeleteBuffers(1, &g_cable_vbo);
}

static Vec3 get_bezier_point(float t, Vec3 p0, Vec3 p1, Vec3 p2) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    Vec3 p = vec3_muls(p0, uu);
    p = vec3_add(p, vec3_muls(p1, 2.0f * u * t));
    p = vec3_add(p, vec3_muls(p2, tt));
    return p;
}

void Cable_Render(Scene* scene, Mat4 view, Mat4 projection, Vec3 cameraPos, float time) {
    glUseProgram(g_cable_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_cable_shader, "view"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(g_cable_shader, "projection"), 1, GL_FALSE, projection.m);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glBindVertexArray(g_cable_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_cable_vbo);

    for (int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        if (strcmp(ent->classname, "env_cable") == 0) {
            const char* target_name = LogicEntity_GetProperty(ent, "Target", "");
            Vec3 end_pos;
            Vec3 end_angles_dummy;

            if (IO_FindNamedEntity(scene, target_name, &end_pos, &end_angles_dummy)) {
                Vec3 start_pos = ent->pos;
                float depth = atof(LogicEntity_GetProperty(ent, "Depth", "20.0"));
                float width = atof(LogicEntity_GetProperty(ent, "Width", "0.1"));
                int segments = atoi(LogicEntity_GetProperty(ent, "Segments", "16"));
                if (segments < 2) segments = 2;

                float wind_amount = atof(LogicEntity_GetProperty(ent, "WindAmount", "5.0"));
                float wind_speed = atof(LogicEntity_GetProperty(ent, "WindSpeed", "1.0"));
                Vec3 wind_angles;
                sscanf(LogicEntity_GetProperty(ent, "WindDirection", "0 0 0"), "%f %f %f", &wind_angles.x, &wind_angles.y, &wind_angles.z);

                Vec3 control_pos = vec3_muls(vec3_add(start_pos, end_pos), 0.5f);
                control_pos.y -= depth;

                if (wind_amount > 0.0f) {
                    Mat4 rot_mat = create_trs_matrix((Vec3) { 0, 0, 0 }, wind_angles, (Vec3) { 1, 1, 1 });
                    Vec3 wind_dir = mat4_mul_vec3_dir(&rot_mat, (Vec3) { 1, 0, 0 });
                    vec3_normalize(&wind_dir);

                    float sway1 = sin(time * wind_speed * 1.0f) * wind_amount * 0.6f;
                    float sway2 = sin(time * wind_speed * 0.45f + 1.23f) * wind_amount * 0.4f;
                    Vec3 wind_offset = vec3_muls(wind_dir, sway1 + sway2);
                    control_pos = vec3_add(control_pos, wind_offset);
                }

                int num_vertices = (segments + 1) * 2;
                Vec3* vertices = (Vec3*)malloc(num_vertices * sizeof(Vec3));
                if (!vertices) continue;

                for (int j = 0; j <= segments; ++j) {
                    float t = (float)j / (float)segments;
                    Vec3 p = get_bezier_point(t, start_pos, control_pos, end_pos);

                    Vec3 next_p;
                    if (j == segments) {
                        float t_prev = (float)(j - 1) / (float)segments;
                        Vec3 prev_p = get_bezier_point(t_prev, start_pos, control_pos, end_pos);
                        next_p = vec3_add(p, vec3_sub(p, prev_p));
                    }
                    else {
                        float t_next = (float)(j + 1) / (float)segments;
                        next_p = get_bezier_point(t_next, start_pos, control_pos, end_pos);
                    }

                    Vec3 tangent = vec3_sub(next_p, p);
                    vec3_normalize(&tangent);
                    Vec3 view_vec = vec3_sub(p, cameraPos);
                    Vec3 right = vec3_cross(tangent, view_vec);
                    vec3_normalize(&right);
                    right = vec3_muls(right, width * 0.5f);

                    vertices[j * 2] = vec3_sub(p, right);
                    vertices[j * 2 + 1] = vec3_add(p, right);
                }

                glBufferData(GL_ARRAY_BUFFER, num_vertices * sizeof(Vec3), vertices, GL_STREAM_DRAW);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, num_vertices);
                free(vertices);
            }
        }
    }
    glBindVertexArray(0);
}