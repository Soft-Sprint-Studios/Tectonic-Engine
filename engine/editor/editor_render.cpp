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
#include <float.h>
#include "editor_render.h"
#include "editor_math.h"
#include "editor_selection.h"
#include "gl_geometry.h"
#include "gl_bloom.h"
#include "gl_render_misc.h"
#include "gl_shadows.h"
#include "gl_ssao.h"
#include "gl_skybox.h"
#include "gl_postprocess.h"
#include "game_data.h"
#include "cvar.h"
#include "io_system.h"

void Editor_RenderGrid(ViewportType type, float aspect) {
    glUseProgram(g_EditorState.grid_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.grid_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.grid_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
    Mat4 model_ident; mat4_identity(&model_ident);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.grid_shader, "model"), 1, GL_FALSE, model_ident.m);
    float grid_lines[2412]; int line_count = 0;
    if (type == VIEW_PERSPECTIVE) {
        float spacing = g_EditorState.grid_size; int num_lines = 200; float extent = (num_lines / 2.0f) * spacing;
        Vec3 cam_pos = g_EditorState.editor_camera.position;
        float center_x = roundf(cam_pos.x / (spacing * 10.0f)) * (spacing * 10.0f); float center_z = roundf(cam_pos.z / (spacing * 10.0f)) * (spacing * 10.0f);
        for (int i = 0; i <= num_lines; ++i) {
            float p = -extent + i * spacing;
            grid_lines[line_count++] = center_x + p; grid_lines[line_count++] = 0.0f; grid_lines[line_count++] = center_z - extent; grid_lines[line_count++] = center_x + p; grid_lines[line_count++] = 0.0f; grid_lines[line_count++] = center_z + extent;
            grid_lines[line_count++] = center_x - extent; grid_lines[line_count++] = 0.0f; grid_lines[line_count++] = center_z + p; grid_lines[line_count++] = center_x + extent; grid_lines[line_count++] = 0.0f; grid_lines[line_count++] = center_z + p;
        }
    }
    else {
        float zoom = g_EditorState.ortho_cam_zoom[type - 1]; float spacing = g_EditorState.grid_size; Vec3 center = g_EditorState.ortho_cam_pos[type - 1];
        float left, right, bottom, top;
        if (type == VIEW_TOP_XZ) { left = center.x - zoom * aspect; right = center.x + zoom * aspect; bottom = center.z - zoom; top = center.z + zoom; }
        else if (type == VIEW_FRONT_XY) { left = center.x - zoom * aspect; right = center.x + zoom * aspect; bottom = center.y - zoom; top = center.y + zoom; }
        else { left = center.z - zoom * aspect; right = center.z + zoom * aspect; bottom = center.y - zoom; top = center.y + zoom; }
        float start_x = floorf(left / spacing) * spacing;
        for (float x = start_x; x <= right && line_count < 2400; x += spacing) {
            if (type == VIEW_TOP_XZ) { grid_lines[line_count++] = x; grid_lines[line_count++] = 0; grid_lines[line_count++] = bottom; grid_lines[line_count++] = x; grid_lines[line_count++] = 0; grid_lines[line_count++] = top; }
            else if (type == VIEW_FRONT_XY) { grid_lines[line_count++] = x; grid_lines[line_count++] = bottom; grid_lines[line_count++] = 0; grid_lines[line_count++] = x; grid_lines[line_count++] = top; grid_lines[line_count++] = 0; }
            else { grid_lines[line_count++] = 0; grid_lines[line_count++] = bottom; grid_lines[line_count++] = x; grid_lines[line_count++] = 0; grid_lines[line_count++] = top; grid_lines[line_count++] = x; }
        }
        float start_y = floorf(bottom / spacing) * spacing;
        for (float y = start_y; y <= top && line_count < 2400; y += spacing) {
            if (type == VIEW_TOP_XZ) { grid_lines[line_count++] = left; grid_lines[line_count++] = 0; grid_lines[line_count++] = y; grid_lines[line_count++] = right; grid_lines[line_count++] = 0; grid_lines[line_count++] = y; }
            else if (type == VIEW_FRONT_XY) { grid_lines[line_count++] = left; grid_lines[line_count++] = y; grid_lines[line_count++] = 0; grid_lines[line_count++] = right; grid_lines[line_count++] = y; grid_lines[line_count++] = 0; }
            else { grid_lines[line_count++] = 0; grid_lines[line_count++] = y; grid_lines[line_count++] = left; grid_lines[line_count++] = 0; grid_lines[line_count++] = y; grid_lines[line_count++] = right; }
        }
    }
    if (line_count == 0) return;
    if (g_EditorState.grid_vao == 0) { glGenVertexArrays(1, &g_EditorState.grid_vao); glGenBuffers(1, &g_EditorState.grid_vbo); }
    glBindVertexArray(g_EditorState.grid_vao); glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, line_count * sizeof(float), grid_lines, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0); float color[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    glUniform4fv(glGetUniformLocation(g_EditorState.grid_shader, "grid_color"), 1, color);
    glDrawArrays(GL_LINES, 0, line_count / 3); glBindVertexArray(0);
}

void Editor_RenderGizmo(Mat4 view, Mat4 projection, ViewportType type) {
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (!primary) return;

    if (primary->type == ENTITY_BRUSH && primary->face_index != -1 && g_EditorState.current_gizmo_operation == GIZMO_OP_ROTATE) {
        return;
    }
    if (g_EditorState.num_selections == 0) {
        return;
    }
    if (primary->type == ENTITY_BRUSH && type != VIEW_PERSPECTIVE) {
        return;
    }
    Vec3 object_pos = g_EditorState.gizmo_selection_centroid;

    glUseProgram(g_EditorState.gizmo_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "view"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "projection"), 1, GL_FALSE, projection.m);

    glDisable(GL_DEPTH_TEST);
    glLineWidth(4.0f);

    glBindVertexArray(g_EditorState.gizmo_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.gizmo_vbo);

    switch (g_EditorState.current_gizmo_operation) {
    case GIZMO_OP_TRANSLATE:
    case GIZMO_OP_SCALE: {
        const float gizmo_arrow_length = 1.0f;
        const float gizmo_vertices[] = {
            0.0f, 0.0f, 0.0f, gizmo_arrow_length, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, gizmo_arrow_length, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, gizmo_arrow_length,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(gizmo_vertices), gizmo_vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        Mat4 model = mat4_translate(object_pos);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "model"), 1, GL_FALSE, model.m);

        Vec3 color_x = { 1.0f, 0.2f, 0.2f }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_X || g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) color_x = Vec3{ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_x.x);
        glDrawArrays(GL_LINES, 0, 2);

        Vec3 color_y = { 0.2f, 1.0f, 0.2f }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_Y || g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) color_y = Vec3{ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_y.x);
        glDrawArrays(GL_LINES, 2, 2);

        Vec3 color_z = { 0.2f, 0.2f, 1.0f }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_Z || g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) color_z = Vec3{ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_z.x);
        glDrawArrays(GL_LINES, 4, 2);
        break;
    }
    case GIZMO_OP_ROTATE: {
        if (type != VIEW_PERSPECTIVE) break;
        Mat4 model; mat4_identity(&model);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "model"), 1, GL_FALSE, model.m);

#define SEGMENTS 32
        const float radius = 1.0f;
        Vec3 points[SEGMENTS + 1];

        Vec3 color_y = { 0,1,0 }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_Y || g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) color_y = Vec3{ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_y.x);
        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = (i / (float)SEGMENTS) * 2.0f * M_PI;
            points[i] = vec3_add(object_pos, Vec3{ cosf(angle)* radius, 0.0f, sinf(angle)* radius });
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINE_STRIP, 0, SEGMENTS + 1);

        Vec3 color_x = { 1,0,0 }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_X || g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) color_x = Vec3{ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_x.x);
        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = (i / (float)SEGMENTS) * 2.0f * M_PI;
            points[i] = vec3_add(object_pos, Vec3{ 0.0f, cosf(angle)* radius, sinf(angle)* radius });
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINE_STRIP, 0, SEGMENTS + 1);

        Vec3 color_z = { 0,0,1 }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_Z || g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) color_z = Vec3{ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_z.x);
        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = (i / (float)SEGMENTS) * 2.0f * M_PI;
            points[i] = vec3_add(object_pos, Vec3{ cosf(angle)* radius, sinf(angle)* radius, 0.0f });
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINE_STRIP, 0, SEGMENTS + 1);
        break;
    }
    }

    glBindVertexArray(0);
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}
void Editor_RenderSceneInternal(ViewportType type, Engine* engine, Renderer* renderer, Scene* scene, const Mat4* sunLightSpaceMatrix) {
    float aspect = (float)g_EditorState.viewport_width[type] / (float)g_EditorState.viewport_height[type];
    if (aspect <= 0) aspect = 1.0;

    switch (type) {
    case VIEW_PERSPECTIVE: {
        glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[type]);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Vec3 f = { cosf(g_EditorState.editor_camera.pitch) * sinf(g_EditorState.editor_camera.yaw), sinf(g_EditorState.editor_camera.pitch), -cosf(g_EditorState.editor_camera.pitch) * cosf(g_EditorState.editor_camera.yaw) };
        vec3_normalize(&f);
        Vec3 t = vec3_add(g_EditorState.editor_camera.position, f);
        g_view_matrix[type] = mat4_lookAt(g_EditorState.editor_camera.position, t, Vec3{ 0, 1, 0 });
        g_proj_matrix[type] = mat4_perspective(45.0f * (M_PI / 180.0f), aspect, 0.1f, 10000.0f);

        Geometry_RenderPass(renderer, scene, engine, &g_view_matrix[type], &g_proj_matrix[type], sunLightSpaceMatrix, g_EditorState.editor_camera.position, g_is_unlit_mode, false);

        if (Cvar_GetInt("r_ssao")) {
            SSAO_RenderPass(renderer, engine, &g_proj_matrix[type]);
        }
        if (Cvar_GetInt("r_bloom")) {
            Bloom_RenderPass(renderer, engine);
        }
        MiscRender_AutoexposurePass(renderer, engine);

        const int LOW_RES_WIDTH = engine->width / Cvar_GetFloat("r_geometry_downsample");
        const int LOW_RES_HEIGHT = engine->height / Cvar_GetFloat("r_geometry_downsample");
        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->gBufferFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, renderer->finalRenderFBO);
        glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, 0, engine->width, engine->height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, 0, engine->width, engine->height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, renderer->finalRenderFBO);
        Skybox_Render(renderer, scene, engine, &g_view_matrix[type], &g_proj_matrix[type]);

        PostProcess_RenderPass(renderer, scene, engine, &g_view_matrix[type], &g_proj_matrix[type], renderer->finalRenderTexture, g_EditorState.viewport_fbo[type], g_EditorState.viewport_width[type], g_EditorState.viewport_height[type]);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->finalRenderFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_EditorState.viewport_fbo[type]);
        glBlitFramebuffer(0, 0, engine->width, engine->height, 0, 0, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type], GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        break;
    }
    case VIEW_TOP_XZ: { Vec3 p = g_EditorState.ortho_cam_pos[type - 1]; float z = g_EditorState.ortho_cam_zoom[type - 1]; g_view_matrix[type] = mat4_lookAt(Vec3{ p.x, 1000.0f, p.z }, Vec3{ p.x, 0.0f, p.z }, Vec3{ 0, 0, -1 }); g_proj_matrix[type] = mat4_ortho(-z * aspect, z * aspect, -z, z, 0.1f, 2000.0f); break; }
    case VIEW_FRONT_XY: { Vec3 p = g_EditorState.ortho_cam_pos[type - 1]; float z = g_EditorState.ortho_cam_zoom[type - 1]; g_view_matrix[type] = mat4_lookAt(Vec3{ p.x, p.y, 1000.0f }, Vec3{ p.x, p.y, 0.0f }, Vec3{ 0, 1, 0 }); g_proj_matrix[type] = mat4_ortho(-z * aspect, z * aspect, -z, z, 0.1f, 2000.0f); break; }
    case VIEW_SIDE_YZ: { Vec3 p = g_EditorState.ortho_cam_pos[type - 1]; float z = g_EditorState.ortho_cam_zoom[type - 1]; g_view_matrix[type] = mat4_lookAt(Vec3{ 1000.0f, p.y, p.z }, Vec3{ 0.0f, p.y, p.z }, Vec3{ 0, 1, 0 }); g_proj_matrix[type] = mat4_ortho(-z * aspect, z * aspect, -z, z, 0.1f, 2000.0f); break; }
    }

    if (type != VIEW_PERSPECTIVE) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[type]);
        glViewport(0, 0, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type]);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Editor_RenderGrid(type, aspect);
        if (g_EditorState.is_painting_mode_enabled && g_EditorState.is_viewport_hovered[type]) {
            Vec3 mouse_world_pos = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[type], type);
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
            Mat4 identity_mat; mat4_identity(&identity_mat);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);
            float color[] = { 1.0f, 1.0f, 0.0f, 0.8f };
            glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

            const int segments = 32;
            Vec3 circle_verts[64];
            for (int i = 0; i < segments; ++i) {
                float angle1 = (i / (float)segments) * 2.0f * M_PI;
                float angle2 = ((i + 1) / (float)segments) * 2.0f * M_PI;
                float x1 = g_EditorState.paint_brush_radius * cosf(angle1);
                float y1 = g_EditorState.paint_brush_radius * sinf(angle1);
                float x2 = g_EditorState.paint_brush_radius * cosf(angle2);
                float y2 = g_EditorState.paint_brush_radius * sinf(angle2);

                if (type == VIEW_TOP_XZ) {
                    circle_verts[i * 2] = Vec3{ mouse_world_pos.x + x1, mouse_world_pos.y, mouse_world_pos.z + y1 };
                    circle_verts[i * 2 + 1] = Vec3{ mouse_world_pos.x + x2, mouse_world_pos.y, mouse_world_pos.z + y2 };
                }
                else if (type == VIEW_FRONT_XY) {
                    circle_verts[i * 2] = Vec3{ mouse_world_pos.x + x1, mouse_world_pos.y + y1, mouse_world_pos.z };
                    circle_verts[i * 2 + 1] = Vec3{ mouse_world_pos.x + x2, mouse_world_pos.y + y2, mouse_world_pos.z };
                }
                else {
                    circle_verts[i * 2] = Vec3{ mouse_world_pos.x, mouse_world_pos.y + y1, mouse_world_pos.z + x1 };
                    circle_verts[i * 2 + 1] = Vec3{ mouse_world_pos.x, mouse_world_pos.y + y2, mouse_world_pos.z + x2 };
                }
            }

            glDisable(GL_DEPTH_TEST);
            glLineWidth(1.0f);
            glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(circle_verts), circle_verts, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_LINES, 0, segments * 2);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }
        if (g_EditorState.is_sculpting_mode_enabled && g_EditorState.is_viewport_hovered[type]) {
            Vec3 mouse_world_pos = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[type], type);
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
            Mat4 identity_mat; mat4_identity(&identity_mat);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);
            float color[] = { 0.0f, 1.0f, 1.0f, 0.8f };
            glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

            const int segments = 32;
            Vec3 circle_verts[64];
            for (int i = 0; i < segments; ++i) {
                float angle1 = (i / (float)segments) * 2.0f * M_PI;
                float angle2 = ((i + 1) / (float)segments) * 2.0f * M_PI;
                float x1 = g_EditorState.sculpt_brush_radius * cosf(angle1);
                float y1 = g_EditorState.sculpt_brush_radius * sinf(angle1);
                float x2 = g_EditorState.sculpt_brush_radius * cosf(angle2);
                float y2 = g_EditorState.sculpt_brush_radius * sinf(angle2);

                if (type == VIEW_TOP_XZ) {
                    circle_verts[i * 2] = Vec3{ mouse_world_pos.x + x1, mouse_world_pos.y, mouse_world_pos.z + y1 };
                    circle_verts[i * 2 + 1] = Vec3{ mouse_world_pos.x + x2, mouse_world_pos.y, mouse_world_pos.z + y2 };
                }
                else if (type == VIEW_FRONT_XY) {
                    circle_verts[i * 2] = Vec3{ mouse_world_pos.x + x1, mouse_world_pos.y + y1, mouse_world_pos.z };
                    circle_verts[i * 2 + 1] = Vec3{ mouse_world_pos.x + x2, mouse_world_pos.y + y2, mouse_world_pos.z };
                }
                else {
                    circle_verts[i * 2] = Vec3{ mouse_world_pos.x, mouse_world_pos.y + y1, mouse_world_pos.z + x1 };
                    circle_verts[i * 2 + 1] = Vec3{ mouse_world_pos.x, mouse_world_pos.y + y2, mouse_world_pos.z + x2 };
                }
            }

            glDisable(GL_DEPTH_TEST);
            glLineWidth(1.0f);
            glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(circle_verts), circle_verts, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_LINES, 0, segments * 2);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glEnable(GL_LINE_SMOOTH); glEnable(GL_POLYGON_OFFSET_LINE); glPolygonOffset(1.0, 1.0); glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); float color[] = { 0.8f, 0.8f, 0.8f, 1.0f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        for (int i = 0; i < scene->numObjects; i++) { render_object(renderer, scene, g_EditorState.debug_shader, &scene->objects[i], false, NULL); }
        for (int i = 0; i < scene->numBrushes; i++) { if (strlen(scene->brushes[i].classname) == 0) render_brush(renderer, scene, g_EditorState.debug_shader, &scene->brushes[i], false, NULL); }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glDisable(GL_LINE_SMOOTH); glDisable(GL_POLYGON_OFFSET_LINE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        float selected_fill_color[] = { 1.0f, 1.0f, 0.0f, 0.2f };
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, selected_fill_color);

        for (int i = 0; i < scene->numBrushes; i++) {
            if (Editor_IsSelected(ENTITY_BRUSH, i)) {
                render_brush(renderer, scene, g_EditorState.debug_shader, &scene->brushes[i], false, NULL);
            }
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[type]);
    glViewport(0, 0, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type]);

    glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
    for (int i = 0; i < scene->numDecals; i++) {
        Decal* d = &scene->decals[i]; glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, d->modelMatrix.m); bool is_selected = Editor_IsSelected(ENTITY_DECAL, i); float color[] = { 0.2f, 1.0f, 0.2f, 1.0f }; if (!is_selected) { color[3] = 0.5f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.decal_box_vao); glLineWidth(is_selected ? 2.0f : 1.0f); glDrawArrays(GL_LINES, 0, g_EditorState.decal_box_vertex_count); glLineWidth(1.0f);
    }
    for (int i = 0; i < scene->numVideoPlayers; i++) {
        VideoPlayer* vp = &scene->videoPlayers[i];
        vp->modelMatrix = create_trs_matrix(vp->pos, vp->rot, Vec3{ vp->size.x, vp->size.y, 1.0f });
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, vp->modelMatrix.m);
        bool is_selected = Editor_IsSelected(ENTITY_VIDEO_PLAYER, i);
        float color[] = { 1.0f, 0.0f, 1.0f, 1.0f };
        if (!is_selected) { color[3] = 0.5f; }
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        glBindVertexArray(g_EditorState.decal_box_vao);
        glLineWidth(is_selected ? 2.0f : 1.0f);
        glDrawArrays(GL_LINES, 0, g_EditorState.decal_box_vertex_count);
        glLineWidth(1.0f);
    }
    for (int i = 0; i < scene->numParallaxRooms; i++) {
        ParallaxRoom* p = &scene->parallaxRooms[i];
        p->modelMatrix = create_trs_matrix(p->pos, p->rot, Vec3{ p->size.x, p->size.y, p->roomDepth });
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, p->modelMatrix.m);
        bool is_selected = Editor_IsSelected(ENTITY_PARALLAX_ROOM, i);
        float color[] = { 0.5f, 0.0f, 1.0f, 1.0f };
        if (!is_selected) { color[3] = 0.5f; }
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        glBindVertexArray(g_EditorState.decal_box_vao);
        glLineWidth(is_selected ? 2.0f : 1.0f);
        glDrawArrays(GL_LINES, 0, g_EditorState.decal_box_vertex_count);
        glLineWidth(1.0f);
    }
    for (int i = 0; i < scene->numSprites; ++i) {
        Sprite* s = &scene->sprites[i];
        bool is_selected = Editor_IsSelected(ENTITY_SPRITE, i);
        if (!s->visible && !is_selected) continue;

        glUseProgram(g_EditorState.debug_shader);
        Mat4 modelMatrix = mat4_translate(s->pos);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m);
        float color[] = { 0.8f, 0.2f, 1.0f, 1.0f };
        if (is_selected) { color[0] = 1.0f; color[1] = 0.5f; color[2] = 0.0f; }
        else if (!s->visible) { color[3] = 0.3f; }
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        glBindVertexArray(g_EditorState.light_gizmo_vao);
        glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);
    }
    glDisable(GL_DEPTH_TEST); glLineWidth(2.0f);
    if (g_EditorState.is_in_brush_creation_mode || g_EditorState.is_dragging_for_creation) {
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, g_EditorState.preview_brush.modelMatrix.m); float color[] = { 1.0f, 1.0f, 0.0f, 0.5f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.preview_brush.vao); glDrawArrays(GL_TRIANGLES, 0, g_EditorState.preview_brush.totalRenderVertexCount); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); color[3] = 1.0f; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glDrawArrays(GL_TRIANGLES, 0, g_EditorState.preview_brush.totalRenderVertexCount); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glDisable(GL_BLEND);
        if (type != VIEW_PERSPECTIVE && g_EditorState.preview_brush.numVertices > 0) {
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
            Mat4 identity_mat; mat4_identity(&identity_mat);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);

            float handle_screen_size = 8.0f;
            float handle_world_size = handle_screen_size * (g_EditorState.ortho_cam_zoom[type - 1] / (float)g_EditorState.viewport_height[type]);

            Vec3 handle_positions_world[PREVIEW_BRUSH_HANDLE_COUNT];
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MIN_X] = Vec3{ g_EditorState.preview_brush_world_min.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush.pos.z };
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MAX_X] = Vec3{ g_EditorState.preview_brush_world_max.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush.pos.z };
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MIN_Y] = Vec3{ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush_world_min.y, g_EditorState.preview_brush.pos.z };
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MAX_Y] = Vec3{ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush_world_max.y, g_EditorState.preview_brush.pos.z };
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MIN_Z] = Vec3{ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush_world_min.z };
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MAX_Z] = Vec3{ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush_world_max.z };

            glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glEnableVertexAttribArray(0);
            glPointSize(handle_screen_size);

            for (int i = 0; i < PREVIEW_BRUSH_HANDLE_COUNT; ++i) {
                bool show_handle = false;
                if (type == VIEW_TOP_XZ) {
                    if (i == PREVIEW_BRUSH_HANDLE_MIN_X || i == PREVIEW_BRUSH_HANDLE_MAX_X || i == PREVIEW_BRUSH_HANDLE_MIN_Z || i == PREVIEW_BRUSH_HANDLE_MAX_Z) show_handle = true;
                }
                else if (type == VIEW_FRONT_XY) {
                    if (i == PREVIEW_BRUSH_HANDLE_MIN_X || i == PREVIEW_BRUSH_HANDLE_MAX_X || i == PREVIEW_BRUSH_HANDLE_MIN_Y || i == PREVIEW_BRUSH_HANDLE_MAX_Y) show_handle = true;
                }
                else if (type == VIEW_SIDE_YZ) {
                    if (i == PREVIEW_BRUSH_HANDLE_MIN_Y || i == PREVIEW_BRUSH_HANDLE_MAX_Y || i == PREVIEW_BRUSH_HANDLE_MIN_Z || i == PREVIEW_BRUSH_HANDLE_MAX_Z) show_handle = true;
                }

                if (show_handle) {
                    float color_arr[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
                    if ((PreviewBrushHandleType)i == g_EditorState.preview_brush_hovered_handle || (PreviewBrushHandleType)i == g_EditorState.preview_brush_active_handle) {
                        color_arr[0] = 1.0f; color_arr[1] = 1.0f; color_arr[2] = 0.0f;
                    }
                    glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color_arr);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(Vec3), &handle_positions_world[i], GL_DYNAMIC_DRAW);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
                    glDrawArrays(GL_POINTS, 0, 1);
                }
            }
            glPointSize(1.0f);
            glBindVertexArray(0);
        }
    }
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (primary && primary->type == ENTITY_BRUSH && type != VIEW_PERSPECTIVE) {
        Brush* b = &scene->brushes[primary->index];
        if (b->numVertices > 0) {
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, b->modelMatrix.m);

            glPointSize(8.0f);
            glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glEnableVertexAttribArray(0);

            Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (int i = 0; i < b->numVertices; ++i) {
                local_min.x = fminf(local_min.x, b->vertices[i].pos.x); local_min.y = fminf(local_min.y, b->vertices[i].pos.y); local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
                local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x); local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y); local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
            }
            Vec3 local_center = vec3_muls(vec3_add(local_min, local_max), 0.5f);

            Vec3 handle_positions_local[6] = {
                {local_min.x, local_center.y, local_center.z}, {local_max.x, local_center.y, local_center.z},
                {local_center.x, local_min.y, local_center.z}, {local_center.x, local_max.y, local_center.z},
                {local_center.x, local_center.y, local_min.z}, {local_center.x, local_center.y, local_max.z}
            };

            for (int i = 0; i < 6; ++i) {
                bool should_draw = false;
                if (type == VIEW_TOP_XZ) {
                    if (i == PREVIEW_BRUSH_HANDLE_MIN_X || i == PREVIEW_BRUSH_HANDLE_MAX_X || i == PREVIEW_BRUSH_HANDLE_MIN_Z || i == PREVIEW_BRUSH_HANDLE_MAX_Z) {
                        should_draw = true;
                    }
                }
                else if (type == VIEW_FRONT_XY) {
                    if (i == PREVIEW_BRUSH_HANDLE_MIN_X || i == PREVIEW_BRUSH_HANDLE_MAX_X || i == PREVIEW_BRUSH_HANDLE_MIN_Y || i == PREVIEW_BRUSH_HANDLE_MAX_Y) {
                        should_draw = true;
                    }
                }
                else if (type == VIEW_SIDE_YZ) {
                    if (i == PREVIEW_BRUSH_HANDLE_MIN_Y || i == PREVIEW_BRUSH_HANDLE_MAX_Y || i == PREVIEW_BRUSH_HANDLE_MIN_Z || i == PREVIEW_BRUSH_HANDLE_MAX_Z) {
                        should_draw = true;
                    }
                }

                if (should_draw) {
                    bool is_hovered = ((PreviewBrushHandleType)i == g_EditorState.selected_brush_hovered_handle);
                    bool is_active = ((PreviewBrushHandleType)i == g_EditorState.selected_brush_active_handle);
                    float color[] = { is_hovered || is_active ? 1.0f : 0.0f, 1.0f, is_hovered || is_active ? 0.0f : 0.0f, 1.0f };
                    glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(Vec3), &handle_positions_local[i], GL_DYNAMIC_DRAW);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
                    glDrawArrays(GL_POINTS, 0, 1);
                }
            }
            glPointSize(1.0f);
            glBindVertexArray(0);
        }
    }
    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];
        if (sel->type == ENTITY_MODEL) { SceneObject* obj = &scene->objects[sel->index]; glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); float color[] = { 1.0f, 0.5f, 0.0f, 1.0f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); render_object(renderer, scene, g_EditorState.debug_shader, obj, false, NULL); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); }
    }
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        if (strlen(b->classname) == 0) continue;

        bool is_selected = Editor_IsSelected(ENTITY_BRUSH, i);


        if (!is_selected && (strcmp(b->classname, "func_water") != 0 && strcmp(b->classname, "env_reflectionprobe") != 0 && strcmp(b->classname, "func_clip") != 0)) {
            continue;
        }

        glUseProgram(g_EditorState.debug_shader);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, b->modelMatrix.m);

        float color[] = { 1.0f, 0.5f, 0.0f, 1.0f };
        if (strncmp(b->classname, "trigger", 7) == 0) { color[0] = 1.0f; color[1] = 0.8f; color[2] = 0.2f; }
        if (strcmp(b->classname, "env_reflectionprobe") == 0) { color[0] = 0.2f; color[1] = 0.8f; color[2] = 1.0f; }
        if (strcmp(b->classname, "func_water") == 0) { color[0] = 0.2f; color[1] = 0.2f; color[2] = 1.0f; }
        if (strcmp(b->classname, "func_clip") == 0) { color[0] = 1.0f; color[1] = 0.0f; color[2] = 1.0f; }

        if (!is_selected) {
            color[3] = 0.3f;
        }

        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBindVertexArray(b->vao);
        glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
        glBindVertexArray(0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];

        if (sel->type == ENTITY_BRUSH) {
            Brush* b = &scene->brushes[sel->index];
            if (Brush_IsSolid(b) && b->numVertices > 0) {
                if (sel->face_index < 0 || sel->face_index >= b->numFaces) continue;
                BrushFace* face = &b->faces[sel->face_index];
                if (face->numVertexIndices >= 3) {
                    int num_tris = face->numVertexIndices - 2;
                    int num_verts = num_tris * 3;

                    float* face_verts = new float[num_verts * 3];

                    for (int tri = 0; tri < num_tris; ++tri) {
                        int tri_indices[3] = { face->vertexIndices[0], face->vertexIndices[tri + 1], face->vertexIndices[tri + 2] };
                        for (int j = 0; j < 3; ++j) {
                            Vec3 v = b->vertices[tri_indices[j]].pos;
                            face_verts[(tri * 3 + j) * 3 + 0] = v.x;
                            face_verts[(tri * 3 + j) * 3 + 1] = v.y;
                            face_verts[(tri * 3 + j) * 3 + 2] = v.z;
                        }
                    }

                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                    glUseProgram(g_EditorState.debug_shader);

                    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
                    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
                    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, b->modelMatrix.m);

                    float color[] = { 0.835f, 0.333f, 0.0f, 0.4f };
                    glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

                    glBindVertexArray(g_EditorState.selected_face_vao);
                    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.selected_face_vbo);
                    glBufferData(GL_ARRAY_BUFFER, num_verts * 3 * sizeof(float), face_verts, GL_DYNAMIC_DRAW);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                    glEnableVertexAttribArray(0);
                    glDrawArrays(GL_TRIANGLES, 0, num_verts);

                    glBindVertexArray(0);
                    glDisable(GL_BLEND);
                    glDepthMask(GL_TRUE);

                    delete[] face_verts;
                }
            }
        }
    }
    glUseProgram(g_EditorState.debug_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
    for (int i = 0; i < scene->numActiveLights; ++i) {
        Light* light = &scene->lights[i];
        bool is_selected = Editor_IsSelected(ENTITY_LIGHT, i);

        Mat4 modelMatrix = mat4_translate(light->pos);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m);

        float color[] = { light->color.x, light->color.y, light->color.z, 1.0f };
        if (is_selected) {
            color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.0f;
        }
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

        glBindVertexArray(g_EditorState.light_gizmo_vao);
        glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);

        if (is_selected) {
            if (light->type == LIGHT_AREA) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                Vec3 zero_vec = { 0.0f, 0.0f, 0.0f };
                Vec3 one_vec = { 1.0f, 1.0f, 1.0f };
                Vec3 scale_vec = { light->width, light->height, 0.01f };
                Mat4 rot_mat = create_trs_matrix(zero_vec, light->rot, one_vec);
                Mat4 scale_mat = mat4_scale(scale_vec);
                Mat4 area_model_mat;
                mat4_multiply(&area_model_mat, &rot_mat, &scale_mat);
                mat4_multiply(&area_model_mat, &modelMatrix, &area_model_mat);

                glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, area_model_mat.m);
                float area_color[] = { 1.0f, 1.0f, 0.0f, 0.8f };
                glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, area_color);

                glBindVertexArray(g_EditorState.decal_box_vao);
                glDrawArrays(GL_LINES, 0, g_EditorState.decal_box_vertex_count);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            if (light->type == LIGHT_POINT) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                Mat4 scaleMatrix = mat4_scale(Vec3{ light->radius, light->radius, light->radius });
                Mat4 scaledModelMatrix;
                mat4_multiply(&scaledModelMatrix, &modelMatrix, &scaleMatrix);
                glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, scaledModelMatrix.m);
                float radius_color[] = { 1.0f, 1.0f, 0.0f, 0.5f };
                glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, radius_color);
                glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            if (light->type == LIGHT_SPOT || light->type == LIGHT_AREA) {
                Mat4 rot_mat = create_trs_matrix(Vec3{ 0, 0, 0 }, light->rot, Vec3{ 1, 1, 1 });
                Vec3 forward = { 0, 0, -1 };
                Vec3 world_dir = mat4_mul_vec3_dir(&rot_mat, forward);
                vec3_normalize(&world_dir);

                Vec3 line_end = vec3_add(light->pos, vec3_muls(world_dir, 2.0f));

                Vec3 line_verts[] = { light->pos, line_end };

                Mat4 identity_mat;
                mat4_identity(&identity_mat);
                glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);

                float line_color[] = { 1.0f, 1.0f, 0.0f, 1.0f };
                glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, line_color);

                glBindVertexArray(g_EditorState.vertex_points_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(line_verts), line_verts, GL_DYNAMIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
                glEnableVertexAttribArray(0);

                glLineWidth(2.0f);
                glDrawArrays(GL_LINES, 0, 2);
                glLineWidth(1.0f);
            }
            if (light->type == LIGHT_SPOT) {
                float far_plane = light->shadowFarPlane > 0 ? light->shadowFarPlane : 25.0f;
                float angle = acosf(fmaxf(-1.0f, fminf(1.0f, light->cutOff)));
                float radius = tanf(angle) * far_plane;
                Vec3 dir = light->direction; vec3_normalize(&dir);
                Vec3 up_ish = (fabsf(vec3_dot(dir, Vec3{ 0, 1, 0 })) > 0.99f) ? Vec3{ 1, 0, 0 } : Vec3{ 0, 1, 0 };
                Vec3 right = vec3_cross(dir, up_ish); vec3_normalize(&right);
                Vec3 up = vec3_cross(right, dir);
                int segments = 16;
                Vec3 cone_verts[40]; int vert_count = 0;
                for (int k = 0; k < 4; ++k) {
                    float theta = (k / 4.0f) * 2.0f * M_PI;
                    Vec3 p_on_circle = vec3_add(vec3_muls(right, cosf(theta) * radius), vec3_muls(up, sinf(theta) * radius));
                    Vec3 world_p = vec3_add(light->pos, vec3_add(vec3_muls(dir, far_plane), p_on_circle));
                    cone_verts[vert_count++] = light->pos;
                    cone_verts[vert_count++] = world_p;
                }
                for (int k = 0; k < segments; ++k) {
                    float theta1 = (k / (float)segments) * 2.0f * M_PI;
                    float theta2 = ((k + 1) / (float)segments) * 2.0f * M_PI;
                    Vec3 p1_on_circle = vec3_add(vec3_muls(right, cosf(theta1) * radius), vec3_muls(up, sinf(theta1) * radius));
                    Vec3 p2_on_circle = vec3_add(vec3_muls(right, cosf(theta2) * radius), vec3_muls(up, sinf(theta2) * radius));
                    cone_verts[vert_count++] = vec3_add(light->pos, vec3_add(vec3_muls(dir, far_plane), p1_on_circle));
                    cone_verts[vert_count++] = vec3_add(light->pos, vec3_add(vec3_muls(dir, far_plane), p2_on_circle));
                }
                Mat4 identity_mat; mat4_identity(&identity_mat);
                glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);
                glBindVertexArray(g_EditorState.vertex_points_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
                glBufferData(GL_ARRAY_BUFFER, vert_count * sizeof(Vec3), cone_verts, GL_DYNAMIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
                glEnableVertexAttribArray(0);
                glDrawArrays(GL_LINES, 0, vert_count);
            }
        }
    }
    glUseProgram(g_EditorState.debug_shader);
    for (int i = 0; i < scene->numSoundEntities; ++i) { bool is_selected = Editor_IsSelected(ENTITY_SOUND, i); Mat4 modelMatrix = mat4_translate(scene->soundEntities[i].pos); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m); float color[] = { 0.1f, 0.9f, 0.6f, 1.0f }; if (is_selected) { color[0] = 1.0f; color[1] = 0.5f; color[2] = 0.0f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.light_gizmo_vao); glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count); }
    for (int i = 0; i < scene->numParticleEmitters; ++i) { bool is_selected = Editor_IsSelected(ENTITY_PARTICLE_EMITTER, i); Mat4 modelMatrix = mat4_translate(scene->particleEmitters[i].pos); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m); float color[] = { 1.0f, 0.2f, 0.8f, 1.0f }; if (is_selected) { color[0] = 1.0f; color[1] = 0.5f; color[2] = 0.0f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.light_gizmo_vao); glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count); }
    glUseProgram(g_EditorState.debug_shader);
    for (int i = 0; i < scene->numLogicEntities; ++i) {
        if (strcmp(scene->logicEntities[i].classname, "env_beam") == 0) {
            LogicEntity* ent = &scene->logicEntities[i];
            const char* target_name = LogicEntity_GetProperty(ent, "target", "");
            Vec3 end_pos;

            if (Editor_FindNamedEntityPosition(scene, target_name, &end_pos)) {
                Mat4 identity;
                mat4_identity(&identity);
                glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity.m);

                float line_verts[6] = { ent->pos.x, ent->pos.y, ent->pos.z, end_pos.x, end_pos.y, end_pos.z };
                float color[] = { 1.0f, 0.5f, 0.0f, 1.0f };

                glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

                glBindVertexArray(g_EditorState.vertex_points_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
                glBufferData(GL_ARRAY_BUFFER, sizeof(line_verts), line_verts, GL_DYNAMIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(0);

                glLineWidth(2.0f);
                glDrawArrays(GL_LINES, 0, 2);
                glLineWidth(1.0f);
                glBindVertexArray(0);
            }
        }
        bool is_selected = Editor_IsSelected(ENTITY_LOGIC, i);
        Mat4 modelMatrix = mat4_translate(scene->logicEntities[i].pos);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m);
        float color[] = { 1.0f, 0.5f, 0.0f, 1.0f };
        if (!is_selected) {
            color[3] = 0.5f;
        }
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        glBindVertexArray(g_EditorState.light_gizmo_vao);
        glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);
    }
    for (int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        if (strcmp(ent->classname, "info_monitorcamera") == 0) {
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);

            Mat4 modelMatrix = create_trs_matrix(ent->pos, ent->rot, Vec3{ 1, 1, 1 });
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m);

            bool is_selected = Editor_IsSelected(ENTITY_LOGIC, i);
            float color[] = { 0.0f, 1.0f, 1.0f, 1.0f };
            if (is_selected) { color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.0f; }
            glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

            glBindVertexArray(g_EditorState.light_gizmo_vao);
            glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);

            const char* fovStr = LogicEntity_GetProperty(ent, "fov", "90");
            float fov = atof(fovStr);
            if (fov <= 0.0f) fov = 90.0f;

            float fLen = 2.0f;
            float halfW = fLen * tanf((fov * 0.5f) * (float)(M_PI / 180.0f));
            float fW = halfW;

            float zTip = 0.0f;
            float zBase = -fLen;

            Vec3 frustum_lines[] = {
                {0,0,0}, {-fW,  fW, zBase},
                {0,0,0}, { fW,  fW, zBase},
                {0,0,0}, { fW, -fW, zBase},
                {0,0,0}, {-fW, -fW, zBase},

                {-fW,  fW, zBase}, { fW,  fW, zBase},
                { fW,  fW, zBase}, { fW, -fW, zBase},
                { fW, -fW, zBase}, {-fW, -fW, zBase},
                {-fW, -fW, zBase}, {-fW,  fW, zBase}
            };

            glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(frustum_lines), frustum_lines, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_LINES, 0, 16);
            glBindVertexArray(0);
        }
    }
    if (primary && primary->type == ENTITY_BRUSH && primary->vertex_index >= 0) {
        Brush* b = &scene->brushes[primary->index];
        if (primary->vertex_index < b->numVertices) {
            Vec3 vertex_local_pos = b->vertices[primary->vertex_index].pos;
            Vec3 vertex_world_pos = mat4_mul_vec3(&b->modelMatrix, vertex_local_pos);
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
            Mat4 identity_mat; mat4_identity(&identity_mat);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);
            float color[] = { 1.0f, 0.0f, 1.0f, 1.0f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
            glPointSize(10.0f); glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(Vec3), &vertex_world_pos, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
            glEnableVertexAttribArray(0); glDrawArrays(GL_POINTS, 0, 1);
            glBindVertexArray(0); glPointSize(1.0f);
        }
    }
    glLineWidth(1.0f); glEnable(GL_DEPTH_TEST);
    if (g_EditorState.sprinkle_brush_hit_surface && g_EditorState.show_sprinkle_tool_window) {
        glUseProgram(g_EditorState.debug_shader);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);

        Mat4 model_mat = mat4_translate(g_EditorState.sprinkle_brush_world_pos);
        Mat4 scale_mat = mat4_scale(Vec3{ g_EditorState.sprinkle_radius, g_EditorState.sprinkle_radius, g_EditorState.sprinkle_radius });
        mat4_multiply(&model_mat, &model_mat, &scale_mat);

        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, model_mat.m);

        float color[] = { 1.0f, 0.0f, 1.0f, 0.5f };
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(g_EditorState.light_gizmo_vao);
        glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (g_EditorState.paint_brush_hit_surface && (g_EditorState.is_painting_mode_enabled || g_EditorState.is_sculpting_mode_enabled)) {
        glUseProgram(g_EditorState.debug_shader);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);

        float radius = g_EditorState.is_painting_mode_enabled ? g_EditorState.paint_brush_radius : g_EditorState.sculpt_brush_radius;
        Mat4 model_mat = mat4_translate(g_EditorState.paint_brush_world_pos);
        Mat4 scale_mat = mat4_scale(Vec3{ radius, radius, radius });
        mat4_multiply(&model_mat, &model_mat, &scale_mat);

        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, model_mat.m);

        float color[] = { 1.0f, 1.0f, 0.0f, 0.5f };
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(g_EditorState.light_gizmo_vao);
        glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (g_EditorState.is_clipping && g_EditorState.clip_point_count > 0 && primary && primary->type == ENTITY_BRUSH) {
        glUseProgram(g_EditorState.debug_shader);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
        Mat4 identity; mat4_identity(&identity);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity.m);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(2.0f);

        Vec3 line_verts[2];
        line_verts[0] = g_EditorState.clip_points[0];

        if (g_EditorState.clip_point_count == 1) {
            if (type == g_EditorState.clip_view) {
                line_verts[1] = ScreenToWorld_Clip(g_EditorState.mouse_pos_in_viewport[type], type);
            }
            else {
                line_verts[1] = line_verts[0];
            }
        }
        else {
            line_verts[1] = g_EditorState.clip_points[1];
        }

        float color_yellow[] = { 1.0f, 1.0f, 0.0f, 1.0f };
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color_yellow);

        glBindVertexArray(g_EditorState.vertex_points_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_verts), line_verts, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, 2);

        if (g_EditorState.clip_point_count >= 2) {
            Vec3 p1 = g_EditorState.clip_points[0];
            Vec3 p2 = g_EditorState.clip_points[1];
            Vec3 mid = vec3_muls(vec3_add(p1, p2), 0.5f);
            Vec3 plane_normal;
            Vec3 dir = vec3_sub(p2, p1);

            if (g_EditorState.clip_view == VIEW_TOP_XZ) { plane_normal = vec3_cross(dir, Vec3{ 0, 1, 0 }); }
            else if (g_EditorState.clip_view == VIEW_FRONT_XY) { plane_normal = vec3_cross(dir, Vec3{ 0, 0, 1 }); }
            else { plane_normal = vec3_cross(dir, Vec3{ 1, 0, 0 }); }
            vec3_normalize(&plane_normal);

            if (g_EditorState.clip_side_point.x != 0 || g_EditorState.clip_side_point.y != 0 || g_EditorState.clip_side_point.z != 0) {
                float side_check = vec3_dot(plane_normal, vec3_sub(g_EditorState.clip_side_point, p1));
                if (side_check < 0) plane_normal = vec3_muls(plane_normal, -1.0f);
            }

            Vec3 indicator_verts[] = { mid, vec3_add(mid, plane_normal) };
            glBufferData(GL_ARRAY_BUFFER, sizeof(indicator_verts), indicator_verts, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINES, 0, 2);
        }

        glLineWidth(1.0f);
        glEnable(GL_DEPTH_TEST);
        glBindVertexArray(0);
    }
    glUseProgram(g_EditorState.debug_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
    Mat4 player_model_matrix = mat4_translate(scene->playerStart.pos);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, player_model_matrix.m);

    bool is_selected = Editor_IsSelected(ENTITY_PLAYERSTART, 0);
    float player_color[] = { 0.2f, 0.2f, 1.0f, 1.0f };
    if (is_selected) {
        player_color[0] = 1.0f; player_color[1] = 0.5f; player_color[2] = 0.0f;
    }
    glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, player_color);

    glBindVertexArray(g_EditorState.player_start_gizmo_vao);
    glLineWidth(is_selected ? 2.0f : 1.0f);
    glDrawArrays(GL_LINES, 0, g_EditorState.player_start_gizmo_vertex_count);
    glLineWidth(1.0f);
    glBindVertexArray(0);

    if (type != VIEW_PERSPECTIVE) {
#define RADIUS_GIZMO_SEGMENTS 32
        glUseProgram(g_EditorState.debug_shader);
        Mat4 model_ident;
        mat4_identity(&model_ident);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, model_ident.m);
        float radius_color[] = { 1.0f, 0.65f, 0.0f, 0.7f };
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, radius_color);
        glLineWidth(1.0f);
        glDisable(GL_DEPTH_TEST);

        for (int i = 0; i < scene->numLogicEntities; ++i) {
            LogicEntity* ent = &scene->logicEntities[i];
            const TGD_EntityDef* def = GameData_FindEntityDef(ent->classname);
            if (!def) continue;

            for (int j = 0; j < def->num_properties; ++j) {
                if (def->properties[j].type == TGD_PROP_RADIUS) {
                    const char* radius_str = LogicEntity_GetProperty(ent, def->properties[j].key, "0");
                    float radius = atof(radius_str);
                    if (radius > 0.0f) {
                        Vec3 circle_verts[RADIUS_GIZMO_SEGMENTS * 2];
                        for (int k = 0; k < RADIUS_GIZMO_SEGMENTS; ++k) {
                            float angle1 = (k / (float)RADIUS_GIZMO_SEGMENTS) * 2.0f * M_PI;
                            float angle2 = ((k + 1) / (float)RADIUS_GIZMO_SEGMENTS) * 2.0f * M_PI;
                            float x1 = radius * cosf(angle1);
                            float y1 = radius * sinf(angle1);
                            float x2 = radius * cosf(angle2);
                            float y2 = radius * sinf(angle2);

                            if (type == VIEW_TOP_XZ) {
                                circle_verts[k * 2] = Vec3{ ent->pos.x + x1, ent->pos.y, ent->pos.z + y1 };
                                circle_verts[k * 2 + 1] = Vec3{ ent->pos.x + x2, ent->pos.y, ent->pos.z + y2 };
                            }
                            else if (type == VIEW_FRONT_XY) {
                                circle_verts[k * 2] = Vec3{ ent->pos.x + x1, ent->pos.y + y1, ent->pos.z };
                                circle_verts[k * 2 + 1] = Vec3{ ent->pos.x + x2, ent->pos.y + y2, ent->pos.z };
                            }
                            else {
                                circle_verts[k * 2] = Vec3{ ent->pos.x, ent->pos.y + y1, ent->pos.z + x1 };
                                circle_verts[k * 2 + 1] = Vec3{ ent->pos.x, ent->pos.y + y2, ent->pos.z + x2 };
                            }
                        }
                        glBindVertexArray(g_EditorState.vertex_points_vao);
                        glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
                        glBufferData(GL_ARRAY_BUFFER, sizeof(circle_verts), circle_verts, GL_DYNAMIC_DRAW);
                        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
                        glEnableVertexAttribArray(0);
                        glDrawArrays(GL_LINES, 0, RADIUS_GIZMO_SEGMENTS * 2);
                    }
                    break;
                }
            }
        }
        glEnable(GL_DEPTH_TEST);
    }

    Editor_RenderGizmo(g_view_matrix[type], g_proj_matrix[type], type);
    if (type == VIEW_PERSPECTIVE && primary && primary->type == ENTITY_BRUSH &&
        primary->vertex_index != -1 &&
        !g_EditorState.is_manipulating_gizmo) {

        Brush* b = &scene->brushes[primary->index];
        Vec3 vertex_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[primary->vertex_index].pos);

        glUseProgram(g_EditorState.gizmo_shader);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(2.0f);
        glBindVertexArray(g_EditorState.gizmo_vao);

        Mat4 scale = mat4_scale(Vec3{ 0.5f, 0.5f, 0.5f });
        Mat4 trans = mat4_translate(vertex_world_pos);
        Mat4 model;
        mat4_multiply(&model, &trans, &scale);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "model"), 1, GL_FALSE, model.m);

        Vec3 color_x = { 1,0,0 }; if (g_EditorState.vertex_gizmo_hovered_axis == GIZMO_AXIS_X || g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) color_x = Vec3{ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_x.x);
        glDrawArrays(GL_LINES, 0, 2);

        Vec3 color_y = { 0,1,0 }; if (g_EditorState.vertex_gizmo_hovered_axis == GIZMO_AXIS_Y || g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Y) color_y = Vec3{ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_y.x);
        glDrawArrays(GL_LINES, 2, 2);

        Vec3 color_z = { 0,0,1 }; if (g_EditorState.vertex_gizmo_hovered_axis == GIZMO_AXIS_Z || g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Z) color_z = Vec3{ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_z.x);
        glDrawArrays(GL_LINES, 4, 2);

        glBindVertexArray(0);
        glLineWidth(1.0f);
        glEnable(GL_DEPTH_TEST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void Editor_RenderModelPreviewerScene(Renderer* renderer) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.model_preview_fbo);
    glViewport(0, 0, g_EditorState.model_preview_width, g_EditorState.model_preview_height);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    if (g_EditorState.preview_model) {
        float aspect = (float)g_EditorState.model_preview_width / (float)g_EditorState.model_preview_height;
        if (aspect <= 0) aspect = 1.0f;
        Vec3 cam_pos;
        cam_pos.x = g_EditorState.model_preview_cam_dist * sinf(g_EditorState.model_preview_cam_angles.y) * cosf(g_EditorState.model_preview_cam_angles.x);
        cam_pos.y = g_EditorState.model_preview_cam_dist * cosf(g_EditorState.model_preview_cam_angles.y);
        cam_pos.z = g_EditorState.model_preview_cam_dist * sinf(g_EditorState.model_preview_cam_angles.y) * sinf(g_EditorState.model_preview_cam_angles.x);
        Mat4 view = mat4_lookAt(cam_pos, Vec3{ 0, 0, 0 }, Vec3{ 0, 1, 0 });
        Mat4 proj = mat4_perspective(45.0f * (M_PI / 180.0f), aspect, 0.1f, 1000.0f);
        glUseProgram(renderer->mainShader);
        glUniform1i(glGetUniformLocation(renderer->mainShader, "is_unlit"), 1);
        glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "view"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "projection"), 1, GL_FALSE, proj.m);
        glUniform1i(glGetUniformLocation(renderer->mainShader, "useEnvironmentMap"), 0);
        SceneObject temp_obj;
        memset(&temp_obj, 0, sizeof(SceneObject));
        temp_obj.model = g_EditorState.preview_model;
        mat4_identity(&temp_obj.modelMatrix);
        render_object(renderer, g_CurrentScene, renderer->mainShader, &temp_obj, false, NULL);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void Editor_RenderAllViewports(Engine* engine, Renderer* renderer, Scene* scene) {
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->volumetricFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->volPingpongFBO[0]);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Shadows_RenderPointAndSpot(renderer, scene, engine);

    Mat4 sunLightSpaceMatrix;
    mat4_identity(&sunLightSpaceMatrix);
    if (scene->sun.enabled) {
        Calculate_Sun_Light_Space_Matrix(&sunLightSpaceMatrix, &scene->sun, g_EditorState.editor_camera.position);
        Shadows_RenderSun(renderer, scene, &sunLightSpaceMatrix);
    }

    for (int i = 0; i < VIEW_COUNT; i++) {
        Editor_RenderSceneInternal((ViewportType)i, engine, renderer, scene, &sunLightSpaceMatrix);
    }
    if (g_EditorState.show_add_model_popup) {
        Editor_RenderModelPreviewerScene(renderer);
    }
}