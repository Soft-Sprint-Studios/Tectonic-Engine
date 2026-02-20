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
#include "commands.h"
#include "gl_misc.h"
#include "editor_misc.h"
#include "editor_windows.h"
#include "editor_math.h"
#include "editor_selection.h"
#include "sound_system.h"
#include "map_misc.h"

void Editor_SetMapDirty(Bool is_dirty) {
    g_is_map_dirty = is_dirty;
}

void Editor_SaveRecentFiles() {
    FILE* file = fopen("editor_prefs.cfg", "w");
    if (!file) return;
    for (Int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
        fprintf(file, "%s\n", g_EditorState.recent_map_files[i]);
    }
    fclose(file);
}

void Editor_LoadRecentFiles() {
    FILE* file = fopen("editor_prefs.cfg", "r");
    if (!file) return;

    Char line[256];
    while (fgets(line, sizeof(line), file) && g_EditorState.num_recent_map_files < MAX_RECENT_FILES) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) > 0) {
            Char** new_files = new Char* [g_EditorState.num_recent_map_files + 1];
            for (Int i = 0; i < g_EditorState.num_recent_map_files; ++i)
                new_files[i] = g_EditorState.recent_map_files[i];

            delete[] g_EditorState.recent_map_files;
            g_EditorState.recent_map_files = new_files;

            Usize len = strlen(line) + 1;
            g_EditorState.recent_map_files[g_EditorState.num_recent_map_files] = new Char[len];
            memcpy(g_EditorState.recent_map_files[g_EditorState.num_recent_map_files], line, len);

            g_EditorState.num_recent_map_files++;
        }
    }
    fclose(file);
}

void Editor_AddRecentFile(const Char* path) {
    for (Int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
        if (strcmp(g_EditorState.recent_map_files[i], path) == 0) {
            delete[] g_EditorState.recent_map_files[i];
            for (Int j = i; j < g_EditorState.num_recent_map_files - 1; ++j)
                g_EditorState.recent_map_files[j] = g_EditorState.recent_map_files[j + 1];
            g_EditorState.num_recent_map_files--;
            break;
        }
    }

    if (g_EditorState.num_recent_map_files >= MAX_RECENT_FILES) {
        delete[] g_EditorState.recent_map_files[MAX_RECENT_FILES - 1];
        g_EditorState.num_recent_map_files = MAX_RECENT_FILES - 1;
    }

    Char** new_files = new Char* [g_EditorState.num_recent_map_files + 1];
    new_files[0] = new Char[strlen(path) + 1];
    memcpy(new_files[0], path, strlen(path) + 1);

    for (Int i = 0; i < g_EditorState.num_recent_map_files; ++i)
        new_files[i + 1] = g_EditorState.recent_map_files[i];

    delete[] g_EditorState.recent_map_files;
    g_EditorState.recent_map_files = new_files;
    g_EditorState.num_recent_map_files++;

    Editor_SaveRecentFiles();
}

void Editor_ExecutePendingAction(Engine* engine, Scene* scene, Renderer* renderer) {
    switch (g_pending_action) {
    case PENDING_ACTION_NEW_MAP:
        Scene_Clear(scene, engine);
        strcpy(g_EditorState.currentMapPath, "untitled.map");
        Undo_Init();
        Editor_SetMapDirty(false);
        break;
    case PENDING_ACTION_LOAD_MAP:
        g_EditorState.show_load_map_popup = true;
        ScanMapFiles();
        Editor_SetMapDirty(false);
        break;
    case PENDING_ACTION_EXIT_EDITOR:
    {
        Char* args[] = { "edit" };
        Commands_Execute(1, args);
        break;
    }
    default:
        break;
    }
    g_pending_action = PENDING_ACTION_NONE;
}

void Editor_InitGizmo() {
    g_EditorState.gizmo_shader = createShaderProgram("shaders/gizmo.vert", "shaders/gizmo.frag");
    const Float gizmo_arrow_length = 1.0f;
    const Float gizmo_vertices[] = {
        0.0f, 0.0f, 0.0f, gizmo_arrow_length, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, gizmo_arrow_length, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, gizmo_arrow_length,
    };
    glGenVertexArrays(1, &g_EditorState.gizmo_vao);
    glGenBuffers(1, &g_EditorState.gizmo_vbo);
    glBindVertexArray(g_EditorState.gizmo_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.gizmo_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gizmo_vertices), gizmo_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(Float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Editor_UpdateGizmoHover(Scene* scene, Vec3 ray_origin, Vec3 ray_dir) {
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (!primary) {
        g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
        return;
    }

    if (primary->type == ENTITY_BRUSH && primary->face_index != -1 && g_EditorState.current_gizmo_operation == GIZMO_OP_ROTATE) {
        g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
        return;
    }
    if (g_EditorState.num_selections == 0) {
        g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
        return;
    }
    Vec3 object_pos = g_EditorState.gizmo_selection_centroid;

    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
    Float min_dist = FLT_MAX;

    switch (g_EditorState.current_gizmo_operation) {
    case GIZMO_OP_TRANSLATE:
    case GIZMO_OP_SCALE: {
        const Float pick_threshold = 0.1f;
        Float t_ray, t_seg;
        Vec3 x_p1 = { object_pos.x + 1.0f, object_pos.y, object_pos.z };
        Float dist_x = dist_RaySegment(ray_origin, ray_dir, object_pos, x_p1, &t_ray, &t_seg);
        if (dist_x < pick_threshold && dist_x < min_dist) { min_dist = dist_x; g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X; }

        Vec3 y_p1 = { object_pos.x, object_pos.y + 1.0f, object_pos.z };
        Float dist_y = dist_RaySegment(ray_origin, ray_dir, object_pos, y_p1, &t_ray, &t_seg);
        if (dist_y < pick_threshold && dist_y < min_dist) { min_dist = dist_y; g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y; }

        Vec3 z_p1 = { object_pos.x, object_pos.y, object_pos.z + 1.0f };
        Float dist_z = dist_RaySegment(ray_origin, ray_dir, object_pos, z_p1, &t_ray, &t_seg);
        if (dist_z < pick_threshold && dist_z < min_dist) { g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z; }
        break;
    }
    case GIZMO_OP_ROTATE: {
        const Float radius = 1.0f;
        const Float pick_threshold = 0.1f;
        Vec3 intersect_point;
        Float closest_dist = FLT_MAX;

        if (ray_plane_intersect(ray_origin, ray_dir, Vec3{ 0, 1, 0 }, -object_pos.y, &intersect_point)) {
            Float dist_to_intersection = Math::vec3_length(Math::vec3_sub(intersect_point, ray_origin));
            if (fabs(Math::vec3_length(Math::vec3_sub(intersect_point, object_pos)) - radius) < pick_threshold) {
                if (dist_to_intersection < closest_dist) {
                    closest_dist = dist_to_intersection;
                    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y;
                }
            }
        }

        if (ray_plane_intersect(ray_origin, ray_dir, Vec3{ 1, 0, 0 }, -object_pos.x, &intersect_point)) {
            Float dist_to_intersection = Math::vec3_length(Math::vec3_sub(intersect_point, ray_origin));
            if (fabs(Math::vec3_length(Math::vec3_sub(intersect_point, object_pos)) - radius) < pick_threshold) {
                if (dist_to_intersection < closest_dist) {
                    closest_dist = dist_to_intersection;
                    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X;
                }
            }
        }

        if (ray_plane_intersect(ray_origin, ray_dir, Vec3{ 0, 0, 1 }, -object_pos.z, &intersect_point)) {
            Float dist_to_intersection = Math::vec3_length(Math::vec3_sub(intersect_point, ray_origin));
            if (fabs(Math::vec3_length(Math::vec3_sub(intersect_point, object_pos)) - radius) < pick_threshold) {
                if (dist_to_intersection < closest_dist) {
                    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z;
                }
            }
        }
        break;
    }
    }
}

void Editor_InitDebugRenderer() {
    g_EditorState.debug_shader = createShaderProgram("shaders/debug.vert", "shaders/debug.frag");
    Float radius = 0.25f; Float sphere_lines[24 * 3 * 2 * 3]; Int index = 0;
    for (Int i = 0; i < 24; ++i) { Float a1 = (i / 24.0f) * 2.0f * Common::PI; Float a2 = ((i + 1) / 24.0f) * 2.0f * Common::PI; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = radius * sinf(a2); sphere_lines[index++] = 0.0f; }
    for (Int i = 0; i < 24; ++i) { Float a1 = (i / 24.0f) * 2.0f * Common::PI; Float a2 = ((i + 1) / 24.0f) * 2.0f * Common::PI; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * sinf(a2); }
    for (Int i = 0; i < 24; ++i) { Float a1 = (i / 24.0f) * 2.0f * Common::PI; Float a2 = ((i + 1) / 24.0f) * 2.0f * Common::PI; sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = radius * sinf(a2); }
    g_EditorState.light_gizmo_vertex_count = index / 3; GLuint vbo;
    glGenVertexArrays(1, &g_EditorState.light_gizmo_vao); glGenBuffers(1, &vbo);
    glBindVertexArray(g_EditorState.light_gizmo_vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sphere_lines), sphere_lines, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(Float), (void*)0); glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    Float lines[] = { -0.5,-0.5,-0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,-0.5,-0.5,-0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,-0.5,-0.5,-0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,0.5,-0.5,0.5,-0.5,-0.5,0.5,0.5,0.5,0.5,-0.5,0.5,0.5,0.5 };
    g_EditorState.decal_box_vertex_count = 24;
    glGenVertexArrays(1, &g_EditorState.decal_box_vao); glGenBuffers(1, &g_EditorState.decal_box_vbo);
    glBindVertexArray(g_EditorState.decal_box_vao); glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.decal_box_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lines), lines, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(Float), (void*)0); glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    constexpr float PLAYER_HEIGHT_NORMAL_EDITOR = 1.83f;
    constexpr float PLAYER_RADIUS_EDITOR = 0.4f;
    Vec3 p_verts[500];
    Int p_vert_count = 0;
    Float p_radius = PLAYER_RADIUS_EDITOR;
    Float p_height = PLAYER_HEIGHT_NORMAL_EDITOR;
    Float p_cylinder_height = p_height - (2.0f * p_radius);

    Vec3 bottom_center = { 0, p_radius, 0 };
    Vec3 top_center = { 0, p_radius + p_cylinder_height, 0 };
    Int segments = 16;

    for (Int i = 0; i < segments; ++i) {
        Float angle1 = (i / (Float)segments) * 2.0f * Common::PI;
        Float angle2 = ((i + 1) / (Float)segments) * 2.0f * Common::PI;

        Float x1 = p_radius * cosf(angle1);
        Float z1 = p_radius * sinf(angle1);
        Float x2 = p_radius * cosf(angle2);
        Float z2 = p_radius * sinf(angle2);

        p_verts[p_vert_count++] = Vec3{ x1, bottom_center.y, z1 };
        p_verts[p_vert_count++] = Vec3{ x2, bottom_center.y, z2 };

        p_verts[p_vert_count++] = Vec3{ x1, top_center.y, z1 };
        p_verts[p_vert_count++] = Vec3{ x2, top_center.y, z2 };

        if (i % (segments / 4) == 0) {
            p_verts[p_vert_count++] = Vec3{ x1, bottom_center.y, z1 };
            p_verts[p_vert_count++] = Vec3{ x1, top_center.y, z1 };
        }
    }

    Int arc_segments = 8;
    for (Int i = 0; i < arc_segments; ++i) {
        Float angle1 = (i / (Float)arc_segments) * 0.5f * Common::PI;
        Float angle2 = ((i + 1) / (Float)arc_segments) * 0.5f * Common::PI;

        p_verts[p_vert_count++] = Vec3{ top_center.x, top_center.y + p_radius * sinf(angle1), top_center.z + p_radius * cosf(angle1) };
        p_verts[p_vert_count++] = Vec3{ top_center.x, top_center.y + p_radius * sinf(angle2), top_center.z + p_radius * cosf(angle2) };
        p_verts[p_vert_count++] = Vec3{ top_center.x + p_radius * cosf(angle1), top_center.y + p_radius * sinf(angle1), top_center.z };
        p_verts[p_vert_count++] = Vec3{ top_center.x + p_radius * cosf(angle2), top_center.y + p_radius * sinf(angle2), top_center.z };
        p_verts[p_vert_count++] = Vec3{ bottom_center.x, bottom_center.y - p_radius * sinf(angle1), bottom_center.z + p_radius * cosf(angle1) };
        p_verts[p_vert_count++] = Vec3{ bottom_center.x, bottom_center.y - p_radius * sinf(angle2), bottom_center.z + p_radius * cosf(angle2) };
        p_verts[p_vert_count++] = Vec3{ bottom_center.x + p_radius * cosf(angle1), bottom_center.y - p_radius * sinf(angle1), bottom_center.z };
        p_verts[p_vert_count++] = Vec3{ bottom_center.x + p_radius * cosf(angle2), bottom_center.y - p_radius * sinf(angle1), bottom_center.z };
    }

    g_EditorState.player_start_gizmo_vertex_count = p_vert_count;
    glGenVertexArrays(1, &g_EditorState.player_start_gizmo_vao);
    glGenBuffers(1, &g_EditorState.player_start_gizmo_vbo);
    glBindVertexArray(g_EditorState.player_start_gizmo_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.player_start_gizmo_vbo);
    glBufferData(GL_ARRAY_BUFFER, p_vert_count * sizeof(Vec3), p_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(Float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Editor_Init(Engine* engine, Renderer* renderer, Scene* scene) {
    if (g_EditorState.initialized) return;
    g_is_editor_mode = true;
    g_CurrentScene = scene;
    memset(&g_EditorState, 0, sizeof(EditorState));
    g_EditorState.selections = nullptr;
    g_EditorState.num_selections = 0;
    g_EditorState.preview_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
    g_EditorState.preview_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
    g_EditorState.current_brush_shape = BRUSH_SHAPE_BLOCK;
    g_EditorState.cylinder_creation_steps = 16;
    g_EditorState.tube_wall_thickness = 0.5f;
    g_EditorState.is_dragging_preview_brush_handle = false;
    g_EditorState.is_hovering_preview_brush_body = false;
    g_EditorState.is_dragging_preview_brush_body = false;
    g_EditorState.is_in_z_mode = false;
    g_EditorState.is_dragging_selected_brush_handle = false;
    g_EditorState.selected_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
    g_EditorState.captured_viewport = VIEW_COUNT;
    g_EditorState.current_gizmo_operation = GIZMO_OP_TRANSLATE;
    Editor_InitGizmo();
    if (g_has_last_camera_state) {
        g_EditorState.editor_camera = g_last_editor_camera_state;
    }
    else {
        g_EditorState.editor_camera.position = Vec3{ 0, 5, 15 };
        g_EditorState.editor_camera.yaw = -Common::PI / 2.0f;
        g_EditorState.editor_camera.pitch = -0.4f;
    }
    for (Int i = 0; i < VIEW_COUNT; i++) {
        g_EditorState.viewport_width[i] = 800; g_EditorState.viewport_height[i] = 600;
        glGenFramebuffers(1, &g_EditorState.viewport_fbo[i]); glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[i]);
        glGenTextures(1, &g_EditorState.viewport_texture[i]); glBindTexture(GL_TEXTURE_2D, g_EditorState.viewport_texture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, g_EditorState.viewport_width[i], g_EditorState.viewport_height[i], 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.viewport_texture[i], 0);
        glGenRenderbuffers(1, &g_EditorState.viewport_rbo[i]); glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.viewport_rbo[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, g_EditorState.viewport_width[i], g_EditorState.viewport_height[i]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_EditorState.viewport_rbo[i]);
    }
    g_EditorState.model_preview_width = 512; g_EditorState.model_preview_height = 512;
    glGenFramebuffers(1, &g_EditorState.model_preview_fbo); glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.model_preview_fbo);
    glGenTextures(1, &g_EditorState.model_preview_texture); glBindTexture(GL_TEXTURE_2D, g_EditorState.model_preview_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, g_EditorState.model_preview_width, g_EditorState.model_preview_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.model_preview_texture, 0);
    glGenRenderbuffers(1, &g_EditorState.model_preview_rbo); glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.model_preview_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, g_EditorState.model_preview_width, g_EditorState.model_preview_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_EditorState.model_preview_rbo);
    Int thumb_size = 128;
    glGenFramebuffers(1, &g_EditorState.model_thumb_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.model_thumb_fbo);
    glGenTextures(1, &g_EditorState.model_thumb_texture);
    glBindTexture(GL_TEXTURE_2D, g_EditorState.model_thumb_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, thumb_size, thumb_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.model_thumb_texture, 0);
    glGenRenderbuffers(1, &g_EditorState.model_thumb_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.model_thumb_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, thumb_size, thumb_size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_EditorState.model_thumb_rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_EditorState.arch_preview_width = 200; g_EditorState.arch_preview_height = 150;
    glGenFramebuffers(1, &g_EditorState.arch_preview_fbo); glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.arch_preview_fbo);
    glGenTextures(1, &g_EditorState.arch_preview_texture); glBindTexture(GL_TEXTURE_2D, g_EditorState.arch_preview_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_EditorState.arch_preview_width, g_EditorState.arch_preview_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.arch_preview_texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_EditorState.model_preview_cam_dist = 5.0f; g_EditorState.model_preview_cam_angles = Vec2{ 0.f, -0.5f };
    for (Int i = 0; i < 3; i++) { g_EditorState.ortho_cam_pos[i] = Vec3{ 0,0,0 }; g_EditorState.ortho_cam_zoom[i] = 10.0f; }
    Editor_InitDebugRenderer();
    glGenVertexArrays(1, &g_EditorState.vertex_points_vao); glGenBuffers(1, &g_EditorState.vertex_points_vbo);
    glGenVertexArrays(1, &g_EditorState.selected_face_vao); glGenBuffers(1, &g_EditorState.selected_face_vbo);
    g_EditorState.grid_size = 1.0f; g_EditorState.snap_to_grid = true;
    g_EditorState.grid_shader = createShaderProgram("shaders/grid.vert", "shaders/grid.frag");
    Undo_Init();
    g_EditorState.initialized = true;
    g_EditorState.preview_animation_index = -1;
    g_EditorState.preview_animation_time = 0.0f;
    g_EditorState.preview_animation_playing = false;
    g_EditorState.is_clipping = false;
    g_EditorState.clip_point_count = 0;
    if (strlen(scene->mapPath) > 0) {
        strcpy(g_EditorState.currentMapPath, scene->mapPath);
    }
    else {
        strcpy(g_EditorState.currentMapPath, "untitled.map");
    }
    g_EditorState.show_load_map_popup = false;
    g_EditorState.show_save_map_popup = false;
    strcpy(g_EditorState.save_map_path, "new_map.map");
    g_EditorState.map_file_list = nullptr;
    g_EditorState.num_map_files = 0;
    g_EditorState.selected_map_file_index = -1;
    g_EditorState.is_painting = false;
    g_EditorState.is_painting_mode_enabled = false;
    g_EditorState.paint_brush_radius = 2.0f;
    g_EditorState.paint_brush_strength = 1.0f;
    g_EditorState.show_texture_browser = false;
    memset(g_EditorState.texture_search_filter, 0, sizeof(g_EditorState.texture_search_filter));
    g_EditorState.paint_channel = 0;
    g_EditorState.is_sculpting = false;
    g_EditorState.is_sculpting_mode_enabled = false;
    g_EditorState.sculpt_brush_radius = 2.0f;
    g_EditorState.sculpt_brush_strength = 0.5f;
    g_EditorState.show_sound_browser_popup = false;
    g_EditorState.sound_file_list = nullptr;
    g_EditorState.num_sound_files = 0;
    g_EditorState.selected_sound_file_index = -1;
    memset(g_EditorState.sound_search_filter, 0, sizeof(g_EditorState.sound_search_filter));
    g_EditorState.preview_sound_buffer = 0;
    g_EditorState.preview_sound_source = 0;
    g_EditorState.paint_brush_hit_surface = false;
    g_EditorState.show_replace_textures_popup = false;
    g_EditorState.find_material_index = -1;
    g_EditorState.replace_material_index = -1;
    g_EditorState.show_vertex_tools_window = false;
    g_EditorState.show_sculpt_noise_popup = false;
    g_EditorState.show_about_window = false;
    g_EditorState.show_sprinkle_tool_window = false;
    strcpy(g_EditorState.sprinkle_model_path, "");
    g_EditorState.sprinkle_density = 5.0f;
    g_EditorState.sprinkle_radius = 5.0f;
    g_EditorState.sprinkle_mode = 0;
    g_EditorState.sprinkle_scale_min = 0.8f;
    g_EditorState.sprinkle_scale_max = 1.2f;
    g_EditorState.sprinkle_align_to_normal = true;
    g_EditorState.sprinkle_random_yaw = true;
    g_EditorState.is_sprinkling = false;
    g_EditorState.sprinkle_timer = 0.0f;
    g_EditorState.sprinkle_brush_hit_surface = false;
    g_EditorState.last_active_2d_view = VIEW_TOP_XZ;
    g_EditorState.editor_camera_speed = 10.0f;
    g_EditorState.texture_lock_enabled = true;
    g_EditorState.doc_files = nullptr;
    g_EditorState.num_doc_files = 0;
    g_EditorState.selected_doc_index = -1;
    g_EditorState.recent_map_files = nullptr;
    g_EditorState.num_recent_map_files = 0;
    g_EditorState.next_group_id = 1;
    g_EditorState.show_arch_properties_popup = false;
    g_EditorState.arch_wall_width = 0.1f;
    g_EditorState.arch_num_sides = 8;
    g_EditorState.arch_arc_degrees = 180.0f;
    g_EditorState.arch_start_angle_degrees = 0.0f;
    g_EditorState.arch_add_height = 0.0f;
    g_EditorState.show_goto_coord_window = false;
    memset(g_EditorState.goto_coord_input, 0, sizeof(g_EditorState.goto_coord_input));
    g_EditorState.autosave_timer = 0.0f;
    g_EditorState.gizmo_drag_has_cloned = false;
    g_EditorState.show_map_info_window = false;
    g_EditorState.show_transform_window = false;
    g_EditorState.transform_window_mode = TRANSFORM_MODE_MOVE;
    g_EditorState.transform_window_values = Vec3{ 0,0,0 };
    g_EditorState.show_particle_browser_popup = false;
    g_EditorState.particle_file_list = nullptr;
    g_EditorState.num_particle_files = 0;
    g_EditorState.selected_particle_file_index = -1;
    memset(g_EditorState.particle_search_filter, 0, sizeof(g_EditorState.particle_search_filter));
    Editor_LoadRecentFiles();
}

void Editor_Shutdown() {
    if (!g_EditorState.initialized) return;
    g_is_editor_mode = false;
    g_last_editor_camera_state = g_EditorState.editor_camera;
    g_has_last_camera_state = true;
    Undo_Shutdown();
    for (Int i = 0; i < VIEW_COUNT; i++) { glDeleteFramebuffers(1, &g_EditorState.viewport_fbo[i]); glDeleteTextures(1, &g_EditorState.viewport_texture[i]); glDeleteRenderbuffers(1, &g_EditorState.viewport_rbo[i]); }
    glDeleteFramebuffers(1, &g_EditorState.model_preview_fbo); glDeleteTextures(1, &g_EditorState.model_preview_texture); glDeleteRenderbuffers(1, &g_EditorState.model_preview_rbo);
    glDeleteFramebuffers(1, &g_EditorState.model_thumb_fbo); glDeleteTextures(1, &g_EditorState.model_thumb_texture); glDeleteRenderbuffers(1, &g_EditorState.model_thumb_rbo);
    if (g_EditorState.preview_model) Model_Free(g_EditorState.preview_model);
    if (g_EditorState.preview_sound_source != 0) Sound::SoundSystem_DeleteSource(g_EditorState.preview_sound_source);
    if (g_EditorState.preview_sound_buffer != 0) Sound::SoundSystem_DeleteBuffer(g_EditorState.preview_sound_buffer);
    FreeSoundFileList();
    FreeModelBrowserEntries();
    FreeMapFileList();
    FreeParticleFileList();
    glDeleteProgram(g_EditorState.debug_shader); glDeleteVertexArrays(1, &g_EditorState.light_gizmo_vao);
    Brush_FreeData(&g_EditorState.preview_brush);
    glDeleteVertexArrays(1, &g_EditorState.vertex_points_vao); glDeleteBuffers(1, &g_EditorState.vertex_points_vbo);
    glDeleteVertexArrays(1, &g_EditorState.selected_face_vao); glDeleteBuffers(1, &g_EditorState.selected_face_vbo);
    glDeleteVertexArrays(1, &g_EditorState.decal_box_vao); glDeleteBuffers(1, &g_EditorState.decal_box_vbo);
    glDeleteProgram(g_EditorState.grid_shader);
    glDeleteProgram(g_EditorState.gizmo_shader);
    glDeleteVertexArrays(1, &g_EditorState.gizmo_vao);
    glDeleteBuffers(1, &g_EditorState.gizmo_vbo);
    glDeleteVertexArrays(1, &g_EditorState.player_start_gizmo_vao);
    glDeleteBuffers(1, &g_EditorState.player_start_gizmo_vbo);
    glDeleteFramebuffers(1, &g_EditorState.arch_preview_fbo);
    glDeleteTextures(1, &g_EditorState.arch_preview_texture);
    if (g_EditorState.recent_map_files) {
        for (Int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
            delete[] g_EditorState.recent_map_files[i];
        }
        delete[] g_EditorState.recent_map_files;
    }
    if (g_EditorState.selections) delete[] g_EditorState.selections;
    if (g_EditorState.gizmo_drag_start_positions) delete[] g_EditorState.gizmo_drag_start_positions;
    if (g_EditorState.gizmo_drag_start_rotations) delete[] g_EditorState.gizmo_drag_start_rotations;
    if (g_EditorState.gizmo_drag_start_scales) delete[] g_EditorState.gizmo_drag_start_scales;
    if (g_EditorState.grid_vao != 0) { glDeleteVertexArrays(1, &g_EditorState.grid_vao); glDeleteBuffers(1, &g_EditorState.grid_vbo); }
    g_EditorState.initialized = false;
}