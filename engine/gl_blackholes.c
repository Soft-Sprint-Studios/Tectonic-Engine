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
#include "gl_blackholes.h"
#include "gl_misc.h"
#include "io_system.h"

void Blackhole_Init(Renderer* renderer) {
    renderer->blackholeShader = createShaderProgram("shaders/blackhole.vert", "shaders/blackhole.frag");
}

void Blackhole_Shutdown(Renderer* renderer) {
    glDeleteProgram(renderer->blackholeShader);
}

void Blackhole_Render(Renderer* renderer, Scene* scene, Engine* engine, Mat4* view, Mat4* projection) {
    bool has_blackhole = false;
    for (int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        if (strcmp(ent->classname, "env_blackhole") == 0 && ent->runtime_active) {
            has_blackhole = true;
            break;
        }
    }
    if (!has_blackhole) return;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->finalRenderFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, renderer->postProcessFBO);
    glBlitFramebuffer(0, 0, engine->width, engine->height, 0, 0, engine->width, engine->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, renderer->finalRenderFBO);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(renderer->blackholeShader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->postProcessTexture);
    glUniform1i(glGetUniformLocation(renderer->blackholeShader, "screenTexture"), 0);

    for (int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        if (strcmp(ent->classname, "env_blackhole") == 0 && ent->runtime_active) {
            glUniform2f(glGetUniformLocation(renderer->blackholeShader, "screensize"), (float)engine->width, (float)engine->height);

            Mat4 view_proj;
            mat4_multiply(&view_proj, projection, view);
            Vec4 clip_pos = mat4_mul_vec4(&view_proj, (Vec4) { ent->pos.x, ent->pos.y, ent->pos.z, 1.0f });

            Vec2 screen_pos;
            screen_pos.x = (clip_pos.x / clip_pos.w) * 0.5f + 0.5f;
            screen_pos.y = (clip_pos.y / clip_pos.w) * 0.5f + 0.5f;

            const char* scale_str = LogicEntity_GetProperty(ent, "scale", "1.0");
            float scale = atof(scale_str);

            const float deg_to_rad = 3.14159265359f / 180.0f;
            float rotation_rad = ent->rot.y * deg_to_rad;

            Vec3 offset_pos = vec3_add(ent->pos, (Vec3) { scale, 0.0f, 0.0f });
            Vec4 clip_offset = mat4_mul_vec4(&view_proj, (Vec4) { offset_pos.x, offset_pos.y, offset_pos.z, 1.0f });

            Vec2 offset_screen_pos;
            offset_screen_pos.x = (clip_offset.x / clip_offset.w) * 0.5f + 0.5f;
            offset_screen_pos.y = (clip_offset.y / clip_offset.w) * 0.5f + 0.5f;

            float screen_radius_x = fabsf(offset_screen_pos.x - screen_pos.x);
            float screen_radius_y = fabsf(offset_screen_pos.y - screen_pos.y);
            float screen_radius = fmaxf(screen_radius_x, screen_radius_y);

            bool visible =
                clip_pos.w > 0.0f &&
                screen_pos.x + screen_radius > 0.0f &&
                screen_pos.x - screen_radius < 1.0f &&
                screen_pos.y + screen_radius > 0.0f &&
                screen_pos.y - screen_radius < 1.0f;

            if (visible) {
                glUniform2fv(glGetUniformLocation(renderer->blackholeShader, "screenpos"), 1, &screen_pos.x);

                float distance_to_cam = vec3_length(vec3_sub(engine->camera.position, ent->pos));
                glUniform1f(glGetUniformLocation(renderer->blackholeShader, "distance_uniform"), distance_to_cam);
                glUniform1f(glGetUniformLocation(renderer->blackholeShader, "size"), scale);
                glUniform1f(glGetUniformLocation(renderer->blackholeShader, "rotation_angle"), rotation_rad);

                glBindVertexArray(renderer->quadVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
    }

    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
}