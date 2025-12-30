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
#include "editor_internal.h"
#include "editor.h"
#include "editor_windows.h"
#include "editor_math.h"
#include "editor_selection.h"
#include "editor_actions.h"
#include "editor_geometry.h"
#include <stdlib.h>
#include "gl_console.h"
#include "lightmapper.h"
#include <GL/glew.h>
#include <SDL.h>
#include "gl_misc.h"
#include <math.h>
#include <float.h>
#include <sys/stat.h>
#ifdef PLATFORM_WINDOWS
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#endif
#include <SDL_image.h>
#include "sound_system.h"
#include "texturemanager.h"
#include "water_manager.h"
#include "io_system.h"
#include "gl_video_player.h"
#include "game_data.h"
#include "cvar.h"

EditorState g_EditorState;
Scene* g_CurrentScene;
Mat4 g_view_matrix[VIEW_COUNT];
Mat4 g_proj_matrix[VIEW_COUNT];

static bool g_is_map_dirty = false;

static PendingEditorAction g_pending_action = PENDING_ACTION_NONE;

BrushFace g_copiedFaceProperties;
bool g_hasCopiedFace = false;

void Editor_SetMapDirty(bool is_dirty) {
    g_is_map_dirty = is_dirty;
}

static void Editor_SaveRecentFiles() {
    FILE* file = fopen("editor_prefs.cfg", "w");
    if (!file) return;
    for (int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
        fprintf(file, "%s\n", g_EditorState.recent_map_files[i]);
    }
    fclose(file);
}

static void Editor_LoadRecentFiles() {
    FILE* file = fopen("editor_prefs.cfg", "r");
    if (!file) return;

    char line[256];
    while (fgets(line, sizeof(line), file) && g_EditorState.num_recent_map_files < MAX_RECENT_FILES) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) > 0) {
            g_EditorState.recent_map_files = realloc(g_EditorState.recent_map_files, (g_EditorState.num_recent_map_files + 1) * sizeof(char*));
            g_EditorState.recent_map_files[g_EditorState.num_recent_map_files] = _strdup(line);
            g_EditorState.num_recent_map_files++;
        }
    }
    fclose(file);
}

static void Editor_AddRecentFile(const char* path) {
    for (int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
        if (strcmp(g_EditorState.recent_map_files[i], path) == 0) {
            free(g_EditorState.recent_map_files[i]);
            for (int j = i; j < g_EditorState.num_recent_map_files - 1; ++j) {
                g_EditorState.recent_map_files[j] = g_EditorState.recent_map_files[j + 1];
            }
            g_EditorState.num_recent_map_files--;
            break;
        }
    }

    if (g_EditorState.num_recent_map_files >= MAX_RECENT_FILES) {
        free(g_EditorState.recent_map_files[MAX_RECENT_FILES - 1]);
        g_EditorState.num_recent_map_files = MAX_RECENT_FILES - 1;
    }
    g_EditorState.recent_map_files = realloc(g_EditorState.recent_map_files, (g_EditorState.num_recent_map_files + 1) * sizeof(char*));
    for (int i = g_EditorState.num_recent_map_files; i > 0; --i) {
        g_EditorState.recent_map_files[i] = g_EditorState.recent_map_files[i - 1];
    }

    g_EditorState.recent_map_files[0] = _strdup(path);
    g_EditorState.num_recent_map_files++;

    Editor_SaveRecentFiles();
}

static void Editor_InitGizmo() {
    g_EditorState.gizmo_shader = createShaderProgram("shaders/gizmo.vert", "shaders/gizmo.frag");
    const float gizmo_arrow_length = 1.0f;
    const float gizmo_vertices[] = {
        0.0f, 0.0f, 0.0f, gizmo_arrow_length, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, gizmo_arrow_length, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, gizmo_arrow_length,
    };
    glGenVertexArrays(1, &g_EditorState.gizmo_vao);
    glGenBuffers(1, &g_EditorState.gizmo_vbo);
    glBindVertexArray(g_EditorState.gizmo_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.gizmo_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gizmo_vertices), gizmo_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}
void Editor_InitDebugRenderer() {
    g_EditorState.debug_shader = createShaderProgram("shaders/debug.vert", "shaders/debug.frag");
    float radius = 0.25f; float sphere_lines[24 * 3 * 2 * 3]; int index = 0;
    for (int i = 0; i < 24; ++i) { float a1 = (i / 24.0f) * 2.0f * M_PI; float a2 = ((i + 1) / 24.0f) * 2.0f * M_PI; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = radius * sinf(a2); sphere_lines[index++] = 0.0f; }
    for (int i = 0; i < 24; ++i) { float a1 = (i / 24.0f) * 2.0f * M_PI; float a2 = ((i + 1) / 24.0f) * 2.0f * M_PI; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * sinf(a2); }
    for (int i = 0; i < 24; ++i) { float a1 = (i / 24.0f) * 2.0f * M_PI; float a2 = ((i + 1) / 24.0f) * 2.0f * M_PI; sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = radius * sinf(a2); }
    g_EditorState.light_gizmo_vertex_count = index / 3; GLuint vbo;
    glGenVertexArrays(1, &g_EditorState.light_gizmo_vao); glGenBuffers(1, &vbo);
    glBindVertexArray(g_EditorState.light_gizmo_vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sphere_lines), sphere_lines, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    float lines[] = { -0.5,-0.5,-0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,-0.5,-0.5,-0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,-0.5,-0.5,-0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,0.5,-0.5,0.5,-0.5,-0.5,0.5,0.5,0.5,0.5,-0.5,0.5,0.5,0.5 };
    g_EditorState.decal_box_vertex_count = 24;
    glGenVertexArrays(1, &g_EditorState.decal_box_vao); glGenBuffers(1, &g_EditorState.decal_box_vbo);
    glBindVertexArray(g_EditorState.decal_box_vao); glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.decal_box_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lines), lines, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glBindVertexArray(0);
#define PLAYER_HEIGHT_NORMAL_EDITOR 1.83f
#define PLAYER_RADIUS_EDITOR 0.4f
    Vec3 p_verts[500];
    int p_vert_count = 0;
    float p_radius = PLAYER_RADIUS_EDITOR;
    float p_height = PLAYER_HEIGHT_NORMAL_EDITOR;
    float p_cylinder_height = p_height - (2.0f * p_radius);

    Vec3 bottom_center = { 0, p_radius, 0 };
    Vec3 top_center = { 0, p_radius + p_cylinder_height, 0 };
    int segments = 16;

    for (int i = 0; i < segments; ++i) {
        float angle1 = (i / (float)segments) * 2.0f * M_PI;
        float angle2 = ((i + 1) / (float)segments) * 2.0f * M_PI;

        float x1 = p_radius * cosf(angle1);
        float z1 = p_radius * sinf(angle1);
        float x2 = p_radius * cosf(angle2);
        float z2 = p_radius * sinf(angle2);

        p_verts[p_vert_count++] = (Vec3){ x1, bottom_center.y, z1 };
        p_verts[p_vert_count++] = (Vec3){ x2, bottom_center.y, z2 };

        p_verts[p_vert_count++] = (Vec3){ x1, top_center.y, z1 };
        p_verts[p_vert_count++] = (Vec3){ x2, top_center.y, z2 };

        if (i % (segments / 4) == 0) {
            p_verts[p_vert_count++] = (Vec3){ x1, bottom_center.y, z1 };
            p_verts[p_vert_count++] = (Vec3){ x1, top_center.y, z1 };
        }
    }

    int arc_segments = 8;
    for (int i = 0; i < arc_segments; ++i) {
        float angle1 = (i / (float)arc_segments) * 0.5f * M_PI;
        float angle2 = ((i + 1) / (float)arc_segments) * 0.5f * M_PI;

        p_verts[p_vert_count++] = (Vec3){ top_center.x, top_center.y + p_radius * sinf(angle1), top_center.z + p_radius * cosf(angle1) };
        p_verts[p_vert_count++] = (Vec3){ top_center.x, top_center.y + p_radius * sinf(angle2), top_center.z + p_radius * cosf(angle2) };
        p_verts[p_vert_count++] = (Vec3){ top_center.x + p_radius * cosf(angle1), top_center.y + p_radius * sinf(angle1), top_center.z };
        p_verts[p_vert_count++] = (Vec3){ top_center.x + p_radius * cosf(angle2), top_center.y + p_radius * sinf(angle2), top_center.z };
        p_verts[p_vert_count++] = (Vec3){ bottom_center.x, bottom_center.y - p_radius * sinf(angle1), bottom_center.z + p_radius * cosf(angle1) };
        p_verts[p_vert_count++] = (Vec3){ bottom_center.x, bottom_center.y - p_radius * sinf(angle2), bottom_center.z + p_radius * cosf(angle2) };
        p_verts[p_vert_count++] = (Vec3){ bottom_center.x + p_radius * cosf(angle1), bottom_center.y - p_radius * sinf(angle1), bottom_center.z };
        p_verts[p_vert_count++] = (Vec3){ bottom_center.x + p_radius * cosf(angle2), bottom_center.y - p_radius * sinf(angle1), bottom_center.z };
    }

    g_EditorState.player_start_gizmo_vertex_count = p_vert_count;
    glGenVertexArrays(1, &g_EditorState.player_start_gizmo_vao);
    glGenBuffers(1, &g_EditorState.player_start_gizmo_vbo);
    glBindVertexArray(g_EditorState.player_start_gizmo_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.player_start_gizmo_vbo);
    glBufferData(GL_ARRAY_BUFFER, p_vert_count * sizeof(Vec3), p_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Editor_Init(Engine* engine, Renderer* renderer, Scene* scene) {
    if (g_EditorState.initialized) return;
    g_is_editor_mode = true;
    g_CurrentScene = scene;
    memset(&g_EditorState, 0, sizeof(EditorState));
    g_EditorState.selections = NULL;
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
    g_EditorState.editor_camera.position = (Vec3){ 0, 5, 15 }; g_EditorState.editor_camera.yaw = -M_PI / 2.0f; g_EditorState.editor_camera.pitch = -0.4f;
    for (int i = 0; i < VIEW_COUNT; i++) {
        g_EditorState.viewport_width[i] = 800; g_EditorState.viewport_height[i] = 600;
        glGenFramebuffers(1, &g_EditorState.viewport_fbo[i]); glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[i]);
        glGenTextures(1, &g_EditorState.viewport_texture[i]); glBindTexture(GL_TEXTURE_2D, g_EditorState.viewport_texture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, g_EditorState.viewport_width[i], g_EditorState.viewport_height[i], 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.viewport_texture[i], 0);
        glGenRenderbuffers(1, &g_EditorState.viewport_rbo[i]); glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.viewport_rbo[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, g_EditorState.viewport_width[i], g_EditorState.viewport_height[i]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_EditorState.viewport_rbo[i]);
    }
    g_EditorState.model_preview_width = 512; g_EditorState.model_preview_height = 512;
    glGenFramebuffers(1, &g_EditorState.model_preview_fbo); glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.model_preview_fbo);
    glGenTextures(1, &g_EditorState.model_preview_texture); glBindTexture(GL_TEXTURE_2D, g_EditorState.model_preview_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, g_EditorState.model_preview_width, g_EditorState.model_preview_height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.model_preview_texture, 0);
    glGenRenderbuffers(1, &g_EditorState.model_preview_rbo); glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.model_preview_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, g_EditorState.model_preview_width, g_EditorState.model_preview_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_EditorState.model_preview_rbo);
    int thumb_size = 128;
    glGenFramebuffers(1, &g_EditorState.model_thumb_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.model_thumb_fbo);
    glGenTextures(1, &g_EditorState.model_thumb_texture);
    glBindTexture(GL_TEXTURE_2D, g_EditorState.model_thumb_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, thumb_size, thumb_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_EditorState.arch_preview_width, g_EditorState.arch_preview_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.arch_preview_texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_EditorState.model_preview_cam_dist = 5.0f; g_EditorState.model_preview_cam_angles = (Vec2){ 0.f, -0.5f };
    for (int i = 0; i < 3; i++) { g_EditorState.ortho_cam_pos[i] = (Vec3){ 0,0,0 }; g_EditorState.ortho_cam_zoom[i] = 10.0f; }
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
    g_EditorState.map_file_list = NULL;
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
    g_EditorState.sound_file_list = NULL;
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
    g_EditorState.show_help_window = false;
    g_EditorState.doc_files = NULL;
    g_EditorState.num_doc_files = 0;
    g_EditorState.selected_doc_index = -1;
    g_EditorState.current_doc_content = NULL;
    g_EditorState.recent_map_files = NULL;
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
    g_EditorState.transform_window_values = (Vec3){ 0,0,0 };
    Editor_LoadRecentFiles();
}
void Editor_Shutdown() {
    if (!g_EditorState.initialized) return;
    g_is_editor_mode = false;
    Undo_Shutdown();
    for (int i = 0; i < VIEW_COUNT; i++) { glDeleteFramebuffers(1, &g_EditorState.viewport_fbo[i]); glDeleteTextures(1, &g_EditorState.viewport_texture[i]); glDeleteRenderbuffers(1, &g_EditorState.viewport_rbo[i]); }
    glDeleteFramebuffers(1, &g_EditorState.model_preview_fbo); glDeleteTextures(1, &g_EditorState.model_preview_texture); glDeleteRenderbuffers(1, &g_EditorState.model_preview_rbo);
    glDeleteFramebuffers(1, &g_EditorState.model_thumb_fbo); glDeleteTextures(1, &g_EditorState.model_thumb_texture); glDeleteRenderbuffers(1, &g_EditorState.model_thumb_rbo);
    if (g_EditorState.preview_model) Model_Free(g_EditorState.preview_model);
    if (g_EditorState.preview_sound_source != 0) SoundSystem_DeleteSource(g_EditorState.preview_sound_source);
    if (g_EditorState.preview_sound_buffer != 0) SoundSystem_DeleteBuffer(g_EditorState.preview_sound_buffer);
    if (g_EditorState.sound_file_list) {
        for (int i = 0; i < g_EditorState.num_sound_files; ++i) {
            free(g_EditorState.sound_file_list[i]);
        }
        free(g_EditorState.sound_file_list);
    }
    FreeModelBrowserEntries();
    FreeMapFileList();
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
        for (int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
            free(g_EditorState.recent_map_files[i]);
        }
        free(g_EditorState.recent_map_files);
    }
    FreeDocFileList();
    if (g_EditorState.current_doc_content) {
        free(g_EditorState.current_doc_content);
        g_EditorState.current_doc_content = NULL;
    }
    if (g_EditorState.selections) free(g_EditorState.selections);
    if (g_EditorState.gizmo_drag_start_positions) free(g_EditorState.gizmo_drag_start_positions);
    if (g_EditorState.gizmo_drag_start_rotations) free(g_EditorState.gizmo_drag_start_rotations);
    if (g_EditorState.gizmo_drag_start_scales) free(g_EditorState.gizmo_drag_start_scales);
    if (g_EditorState.grid_vao != 0) { glDeleteVertexArrays(1, &g_EditorState.grid_vao); glDeleteBuffers(1, &g_EditorState.grid_vbo); }
    g_EditorState.initialized = false;
}
static void Editor_UpdateGizmoHover(Scene* scene, Vec3 ray_origin, Vec3 ray_dir) {
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
    float min_dist = FLT_MAX;

    switch (g_EditorState.current_gizmo_operation) {
    case GIZMO_OP_TRANSLATE:
    case GIZMO_OP_SCALE: {
        const float pick_threshold = 0.1f;
        float t_ray, t_seg;
        Vec3 x_p1 = { object_pos.x + 1.0f, object_pos.y, object_pos.z };
        float dist_x = dist_RaySegment(ray_origin, ray_dir, object_pos, x_p1, &t_ray, &t_seg);
        if (dist_x < pick_threshold && dist_x < min_dist) { min_dist = dist_x; g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X; }

        Vec3 y_p1 = { object_pos.x, object_pos.y + 1.0f, object_pos.z };
        float dist_y = dist_RaySegment(ray_origin, ray_dir, object_pos, y_p1, &t_ray, &t_seg);
        if (dist_y < pick_threshold && dist_y < min_dist) { min_dist = dist_y; g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y; }

        Vec3 z_p1 = { object_pos.x, object_pos.y, object_pos.z + 1.0f };
        float dist_z = dist_RaySegment(ray_origin, ray_dir, object_pos, z_p1, &t_ray, &t_seg);
        if (dist_z < pick_threshold && dist_z < min_dist) { g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z; }
        break;
    }
    case GIZMO_OP_ROTATE: {
        const float radius = 1.0f;
        const float pick_threshold = 0.1f;
        Vec3 intersect_point;
        float closest_dist = FLT_MAX;

        if (ray_plane_intersect(ray_origin, ray_dir, (Vec3) { 0, 1, 0 }, -object_pos.y, & intersect_point)) {
            float dist_to_intersection = vec3_length(vec3_sub(intersect_point, ray_origin));
            if (fabs(vec3_length(vec3_sub(intersect_point, object_pos)) - radius) < pick_threshold) {
                if (dist_to_intersection < closest_dist) {
                    closest_dist = dist_to_intersection;
                    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y;
                }
            }
        }

        if (ray_plane_intersect(ray_origin, ray_dir, (Vec3) { 1, 0, 0 }, -object_pos.x, & intersect_point)) {
            float dist_to_intersection = vec3_length(vec3_sub(intersect_point, ray_origin));
            if (fabs(vec3_length(vec3_sub(intersect_point, object_pos)) - radius) < pick_threshold) {
                if (dist_to_intersection < closest_dist) {
                    closest_dist = dist_to_intersection;
                    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X;
                }
            }
        }

        if (ray_plane_intersect(ray_origin, ray_dir, (Vec3) { 0, 0, 1 }, -object_pos.z, & intersect_point)) {
            float dist_to_intersection = vec3_length(vec3_sub(intersect_point, ray_origin));
            if (fabs(vec3_length(vec3_sub(intersect_point, object_pos)) - radius) < pick_threshold) {
                if (dist_to_intersection < closest_dist) {
                    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z;
                }
            }
        }
        break;
    }
    }
}
void Editor_ProcessEvent(SDL_Event* event, Scene* scene, Engine* engine) {
    if (event->type == SDL_MOUSEMOTION) {
        bool can_look = g_EditorState.is_in_z_mode || (g_EditorState.is_viewport_focused[VIEW_PERSPECTIVE] && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)));
        if (can_look) {
            g_EditorState.editor_camera.yaw += event->motion.xrel * 0.005f;
            g_EditorState.editor_camera.pitch -= event->motion.yrel * 0.005f;
        }
    }
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (event->type == SDL_KEYUP && event->key.keysym.sym == SDLK_c) {
        if (g_EditorState.is_clipping) {
            if (primary && primary->type == ENTITY_BRUSH && g_EditorState.clip_point_count >= 2) {
                if (scene->numBrushes >= MAX_BRUSHES - 1) {
                    Console_Printf_Error("[error] Cannot clip brush, MAX_BRUSHES limit reached.");
                    g_EditorState.is_clipping = false;
                    return;
                }

                int original_brush_index = primary->index;
                Brush* original_brush = &scene->brushes[original_brush_index];

                Undo_BeginEntityModification(scene, ENTITY_BRUSH, original_brush_index);

                Brush brush_b_storage = { 0 };
                Brush_DeepCopy(&brush_b_storage, original_brush);

                Vec3 p1 = g_EditorState.clip_points[0];
                Vec3 p2 = g_EditorState.clip_points[1];
                Vec3 plane_normal;
                Vec3 dir = vec3_sub(p2, p1);

                if (g_EditorState.clip_view == VIEW_TOP_XZ) { plane_normal = vec3_cross(dir, (Vec3) { 0, 1, 0 }); }
                else if (g_EditorState.clip_view == VIEW_FRONT_XY) { plane_normal = vec3_cross(dir, (Vec3) { 0, 0, 1 }); }
                else { plane_normal = vec3_cross(dir, (Vec3) { 1, 0, 0 }); }
                vec3_normalize(&plane_normal);

                float side_check = vec3_dot(plane_normal, vec3_sub(g_EditorState.clip_side_point, p1));
                if (side_check < 0.0f) {
                    plane_normal = vec3_muls(plane_normal, -1.0f);
                }

                float plane_d_a = -vec3_dot(plane_normal, p1);
                float plane_d_b = -plane_d_a;
                Vec3 plane_normal_b = vec3_muls(plane_normal, -1.0f);

                Brush_Clip(original_brush, plane_normal, plane_d_a);
                Brush_CreateRenderData(original_brush);
                if (original_brush->physicsBody) Physics_RemoveRigidBody(engine->physicsWorld, original_brush->physicsBody);
                if (Brush_IsSolid(original_brush) && original_brush->numVertices > 0) {
                    Vec3* world_verts = malloc(original_brush->numVertices * sizeof(Vec3));
                    for (int k = 0; k < original_brush->numVertices; ++k) world_verts[k] = mat4_mul_vec3(&original_brush->modelMatrix, original_brush->vertices[k].pos);
                    original_brush->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, original_brush->numVertices);
                    free(world_verts);
                }
                else {
                    original_brush->physicsBody = NULL;
                }

                Brush_Clip(&brush_b_storage, plane_normal_b, plane_d_b);

                if (brush_b_storage.numVertices > 0) {
                    int new_brush_index = scene->numBrushes;
                    scene->brushes[new_brush_index] = brush_b_storage;
                    scene->numBrushes++;

                    Brush* new_b_ptr = &scene->brushes[new_brush_index];
                    Brush_CreateRenderData(new_b_ptr);
                    if (Brush_IsSolid(new_b_ptr) && new_b_ptr->numVertices > 0) {
                        Vec3* world_verts = malloc(new_b_ptr->numVertices * sizeof(Vec3));
                        for (int k = 0; k < new_b_ptr->numVertices; ++k) world_verts[k] = mat4_mul_vec3(&new_b_ptr->modelMatrix, new_b_ptr->vertices[k].pos);
                        new_b_ptr->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, new_b_ptr->numVertices);
                        free(world_verts);
                    }
                    else {
                        new_b_ptr->physicsBody = NULL;
                    }
                    Undo_PushCreateEntity(scene, ENTITY_BRUSH, new_brush_index, "Clip Brush (Create B)");
                }
                else {
                    Brush_FreeData(&brush_b_storage);
                }

                Undo_EndEntityModification(scene, ENTITY_BRUSH, original_brush_index, "Clip Brush (Modify A)");
                Editor_ClearSelection();
            }
            g_EditorState.is_clipping = false;
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (g_EditorState.show_sprinkle_tool_window && g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
            g_EditorState.is_sprinkling = true;
            g_EditorState.sprinkle_timer = 0.0f;
            return;
        }
        if (g_EditorState.is_painting_mode_enabled && primary && primary->type == ENTITY_BRUSH) {
            if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
                g_EditorState.is_painting = true;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                return;
            }
            bool is_hovering_paint_viewport = false;
            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    is_hovering_paint_viewport = true;
                    break;
                }
            }
            if (is_hovering_paint_viewport) {
                g_EditorState.is_painting = true;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                return;
            }
        }
        if (g_EditorState.is_sculpting_mode_enabled && primary && primary->type == ENTITY_BRUSH) {
            if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
                g_EditorState.is_sculpting = true;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                return;
            }
            bool is_hovering_sculpt_viewport = false;
            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    is_hovering_sculpt_viewport = true;
                    break;
                }
            }
            if (is_hovering_sculpt_viewport) {
                g_EditorState.is_sculpting = true;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                return;
            }
        }
        if (g_EditorState.is_clipping) {
            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    if (g_EditorState.clip_point_count < 2) {
                        if (g_EditorState.clip_point_count == 0) {
                            g_EditorState.clip_view = (ViewportType)i;
                            if (primary && primary->type == ENTITY_BRUSH) {
                                Brush* b = &scene->brushes[primary->index];
                                switch (g_EditorState.clip_view) {
                                case VIEW_TOP_XZ:   g_EditorState.clip_plane_depth = b->pos.y; break;
                                case VIEW_FRONT_XY: g_EditorState.clip_plane_depth = b->pos.z; break;
                                case VIEW_SIDE_YZ:  g_EditorState.clip_plane_depth = b->pos.x; break;
                                default: g_EditorState.clip_plane_depth = 0.0f; break;
                                }
                            }
                            else {
                                g_EditorState.clip_plane_depth = 0.0f;
                            }
                        }

                        if (g_EditorState.clip_view == (ViewportType)i) {
                            g_EditorState.clip_points[g_EditorState.clip_point_count] = ScreenToWorld_Clip(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                            g_EditorState.clip_point_count++;
                        }
                    }
                    else {
                        g_EditorState.clip_side_point = ScreenToWorld_Clip(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                    }
                    return;
                }
            }
        }

        ViewportType active_viewport = VIEW_COUNT;
        for (int i = 0; i < VIEW_COUNT; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                active_viewport = (ViewportType)i;
                break;
            }
        }
        if (g_EditorState.selected_brush_hovered_handle != PREVIEW_BRUSH_HANDLE_NONE) {
            if (primary && primary->type == ENTITY_BRUSH) {
                g_EditorState.is_dragging_selected_brush_handle = true;
                g_EditorState.selected_brush_active_handle = g_EditorState.selected_brush_hovered_handle;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                return;
            }
        }
        else if (g_EditorState.is_hovering_selected_brush_body && active_viewport >= VIEW_TOP_XZ && active_viewport <= VIEW_SIDE_YZ) {
            if (primary && primary->type == ENTITY_BRUSH) {
                g_EditorState.is_dragging_selected_brush_body = true;
                g_EditorState.selected_brush_drag_body_view = active_viewport;
                Vec3 raw_start_mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
                if (g_EditorState.snap_to_grid) {
                    raw_start_mouse_world.x = SnapValue(raw_start_mouse_world.x, g_EditorState.grid_size);
                    raw_start_mouse_world.y = SnapValue(raw_start_mouse_world.y, g_EditorState.grid_size);
                    raw_start_mouse_world.z = SnapValue(raw_start_mouse_world.z, g_EditorState.grid_size);
                }
                g_EditorState.selected_brush_drag_body_start_mouse_world = raw_start_mouse_world;
                if (g_EditorState.gizmo_drag_start_positions) free(g_EditorState.gizmo_drag_start_positions);
                g_EditorState.gizmo_drag_start_positions = malloc(g_EditorState.num_selections * sizeof(Vec3));

                Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);

                for (int i = 0; i < g_EditorState.num_selections; ++i) {
                    EditorSelection* sel = &g_EditorState.selections[i];
                    Vec3 pos = { 0 };
                    switch (sel->type) {
                    case ENTITY_MODEL: pos = scene->objects[sel->index].pos; break;
                    case ENTITY_BRUSH: pos = scene->brushes[sel->index].pos; break;
                    case ENTITY_LIGHT: pos = scene->lights[sel->index].position; break;
                    case ENTITY_DECAL: pos = scene->decals[sel->index].pos; break;
                    case ENTITY_SOUND: pos = scene->soundEntities[sel->index].pos; break;
                    case ENTITY_PARTICLE_EMITTER: pos = scene->particleEmitters[sel->index].pos; break;
                    case ENTITY_SPRITE: pos = scene->sprites[sel->index].pos; break;
                    case ENTITY_VIDEO_PLAYER: pos = scene->videoPlayers[sel->index].pos; break;
                    case ENTITY_PARALLAX_ROOM: pos = scene->parallaxRooms[sel->index].pos; break;
                    case ENTITY_LOGIC: pos = scene->logicEntities[sel->index].pos; break;
                    case ENTITY_PLAYERSTART: pos = scene->playerStart.position; break;
                    }
                    g_EditorState.gizmo_drag_start_positions[i] = pos;
                }
                return;
            }
        }
        if (g_EditorState.is_in_brush_creation_mode && g_EditorState.preview_brush_hovered_handle != PREVIEW_BRUSH_HANDLE_NONE && active_viewport >= VIEW_TOP_XZ && active_viewport <= VIEW_SIDE_YZ) {
            g_EditorState.is_dragging_preview_brush_handle = true;
            g_EditorState.preview_brush_active_handle = g_EditorState.preview_brush_hovered_handle;
            g_EditorState.preview_brush_drag_handle_view = active_viewport;
            return;
        }
        else if (g_EditorState.is_in_brush_creation_mode && g_EditorState.is_hovering_preview_brush_body && active_viewport >= VIEW_TOP_XZ && active_viewport <= VIEW_SIDE_YZ) {
            g_EditorState.is_dragging_preview_brush_body = true;
            g_EditorState.preview_brush_drag_body_view = active_viewport;
            g_EditorState.preview_brush_drag_body_start_mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
            g_EditorState.preview_brush_drag_body_start_brush_world_min_at_drag_start = g_EditorState.preview_brush_world_min;
            return;
        }
        if (g_EditorState.vertex_gizmo_hovered_axis != GIZMO_AXIS_NONE && g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
            g_EditorState.is_manipulating_vertex_gizmo = true;
            g_EditorState.vertex_gizmo_active_axis = g_EditorState.vertex_gizmo_hovered_axis;
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);

            Brush* b = &scene->brushes[primary->index];
            g_EditorState.vertex_drag_start_pos_world = mat4_mul_vec3(&b->modelMatrix, b->vertices[primary->vertex_index].pos);

            Vec3 cam_forward = { g_view_matrix[VIEW_PERSPECTIVE].m[2], g_view_matrix[VIEW_PERSPECTIVE].m[6], g_view_matrix[VIEW_PERSPECTIVE].m[10] };
            Vec3 axis_dir = { 0 };
            if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
            if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
            if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
            float dot_product = fabsf(vec3_dot(axis_dir, cam_forward));
            if (dot_product > 0.99f) { if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) { g_EditorState.vertex_gizmo_drag_plane_normal = (Vec3){ 0, 1, 0 }; } else { g_EditorState.vertex_gizmo_drag_plane_normal = (Vec3){ 1, 0, 0 }; } }
            else { g_EditorState.vertex_gizmo_drag_plane_normal = vec3_cross(axis_dir, cam_forward); vec3_normalize(&g_EditorState.vertex_gizmo_drag_plane_normal); }
            g_EditorState.vertex_gizmo_drag_plane_d = -vec3_dot(g_EditorState.vertex_gizmo_drag_plane_normal, g_EditorState.vertex_drag_start_pos_world);

            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
            float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
            Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
            Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
            Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
            ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.vertex_gizmo_drag_plane_normal, g_EditorState.vertex_gizmo_drag_plane_d, &g_EditorState.vertex_gizmo_drag_start_world);
            return;
        }
        else if (g_EditorState.gizmo_hovered_axis != GIZMO_AXIS_NONE && active_viewport != VIEW_COUNT) {
            Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);
            g_EditorState.is_manipulating_gizmo = true;
            g_EditorState.gizmo_drag_has_cloned = false;
            g_EditorState.gizmo_selection_centroid = (Vec3){ 0 };
            for (int i = 0; i < g_EditorState.num_selections; ++i) {
                Vec3 pos;
                switch (g_EditorState.selections[i].type) {
                case ENTITY_MODEL: pos = scene->objects[g_EditorState.selections[i].index].pos; break;
                case ENTITY_BRUSH: pos = scene->brushes[g_EditorState.selections[i].index].pos; break;
                case ENTITY_LIGHT: pos = scene->lights[g_EditorState.selections[i].index].position; break;
                case ENTITY_DECAL: pos = scene->decals[g_EditorState.selections[i].index].pos; break;
                case ENTITY_SOUND: pos = scene->soundEntities[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PARTICLE_EMITTER: pos = scene->particleEmitters[g_EditorState.selections[i].index].pos; break;
                case ENTITY_SPRITE: pos = scene->sprites[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PLAYERSTART: pos = scene->playerStart.position; break;
                case ENTITY_VIDEO_PLAYER: pos = scene->videoPlayers[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PARALLAX_ROOM: pos = scene->parallaxRooms[g_EditorState.selections[i].index].pos; break;
                case ENTITY_LOGIC: pos = scene->logicEntities[g_EditorState.selections[i].index].pos; break;
                default: pos = (Vec3){ 0 }; break;
                }
                g_EditorState.gizmo_selection_centroid = vec3_add(g_EditorState.gizmo_selection_centroid, pos);
            }
            if (g_EditorState.num_selections > 0) {
                g_EditorState.gizmo_selection_centroid = vec3_muls(g_EditorState.gizmo_selection_centroid, 1.0f / g_EditorState.num_selections);
            }
            if (g_EditorState.gizmo_drag_start_positions) free(g_EditorState.gizmo_drag_start_positions);
            if (g_EditorState.gizmo_drag_start_rotations) free(g_EditorState.gizmo_drag_start_rotations);
            if (g_EditorState.gizmo_drag_start_scales) free(g_EditorState.gizmo_drag_start_scales);

            g_EditorState.gizmo_drag_start_positions = (Vec3*)malloc(g_EditorState.num_selections * sizeof(Vec3));
            g_EditorState.gizmo_drag_start_rotations = (Vec3*)malloc(g_EditorState.num_selections * sizeof(Vec3));
            g_EditorState.gizmo_drag_start_scales = (Vec3*)malloc(g_EditorState.num_selections * sizeof(Vec3));

            for (int i = 0; i < g_EditorState.num_selections; ++i) {
                EditorSelection* sel = &g_EditorState.selections[i];
                switch (sel->type) {
                case ENTITY_MODEL:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->objects[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->objects[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = scene->objects[sel->index].scale;
                    break;
                case ENTITY_BRUSH:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->brushes[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->brushes[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = scene->brushes[sel->index].scale;
                    break;
                case ENTITY_LIGHT:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->lights[sel->index].position;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->lights[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                case ENTITY_DECAL:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->decals[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->decals[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = scene->decals[sel->index].size;
                    break;
                case ENTITY_SOUND:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->soundEntities[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                case ENTITY_PARTICLE_EMITTER:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->particleEmitters[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                case ENTITY_SPRITE:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->sprites[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ scene->sprites[sel->index].scale, scene->sprites[sel->index].scale, scene->sprites[sel->index].scale };
                    break;
                case ENTITY_VIDEO_PLAYER:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->videoPlayers[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->videoPlayers[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ scene->videoPlayers[sel->index].size.x, scene->videoPlayers[sel->index].size.y, 1.0f };
                    break;
                case ENTITY_PARALLAX_ROOM:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->parallaxRooms[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->parallaxRooms[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ scene->parallaxRooms[sel->index].size.x, scene->parallaxRooms[sel->index].size.y, 1.0f };
                    break;
                case ENTITY_LOGIC:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->logicEntities[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->logicEntities[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                case ENTITY_PLAYERSTART:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->playerStart.position;
                    g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                default:
                    g_EditorState.gizmo_drag_start_positions[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                }
            }
            g_EditorState.gizmo_active_axis = g_EditorState.gizmo_hovered_axis;
            g_EditorState.gizmo_drag_view = active_viewport;

            if (primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
            }
            else {
                if (g_EditorState.is_in_brush_creation_mode) {
                    g_EditorState.gizmo_drag_object_start_pos = g_EditorState.preview_brush.pos;
                    g_EditorState.gizmo_drag_object_start_rot = g_EditorState.preview_brush.rot;
                    g_EditorState.gizmo_drag_object_start_scale = g_EditorState.preview_brush.scale;
                }
                else if (primary) {
                    switch (primary->type) {
                    case ENTITY_MODEL:
                        g_EditorState.gizmo_drag_object_start_pos = scene->objects[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->objects[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = scene->objects[primary->index].scale;
                        break;
                    case ENTITY_BRUSH:
                        g_EditorState.gizmo_drag_object_start_pos = scene->brushes[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->brushes[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = scene->brushes[primary->index].scale;
                        break;
                    case ENTITY_LIGHT:
                        g_EditorState.gizmo_drag_object_start_pos = scene->lights[primary->index].position;
                        g_EditorState.gizmo_drag_object_start_rot = scene->lights[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ 1,1,1 };
                        break;
                    case ENTITY_DECAL:
                        g_EditorState.gizmo_drag_object_start_pos = scene->decals[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->decals[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = scene->decals[primary->index].size;
                        break;
                    case ENTITY_SOUND:
                        g_EditorState.gizmo_drag_object_start_pos = scene->soundEntities[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = (Vec3){ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ 1,1,1 };
                        break;
                    case ENTITY_PARTICLE_EMITTER:
                        g_EditorState.gizmo_drag_object_start_pos = scene->particleEmitters[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = (Vec3){ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ 1,1,1 };
                        break;
                    case ENTITY_SPRITE:
                        g_EditorState.gizmo_drag_object_start_pos = scene->sprites[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = (Vec3){ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ scene->sprites[primary->index].scale, 1, 1 };
                        break;
                    case ENTITY_PLAYERSTART:
                        g_EditorState.gizmo_drag_object_start_pos = scene->playerStart.position;
                        g_EditorState.gizmo_drag_object_start_rot = (Vec3){ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ 1,1,1 };
                        break;
                    case ENTITY_VIDEO_PLAYER:
                        g_EditorState.gizmo_drag_object_start_pos = scene->videoPlayers[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->videoPlayers[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ scene->videoPlayers[primary->index].size.x, scene->videoPlayers[primary->index].size.y, 1.0f };
                        break;
                    case ENTITY_PARALLAX_ROOM:
                        g_EditorState.gizmo_drag_object_start_pos = scene->parallaxRooms[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->parallaxRooms[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ scene->parallaxRooms[primary->index].size.x, scene->parallaxRooms[primary->index].size.y, 1.0f };
                        break;
                    default: break;
                    }
                }
            }

            switch (g_EditorState.current_gizmo_operation) {
            case GIZMO_OP_TRANSLATE:
            case GIZMO_OP_SCALE: {
                Vec3 drag_object_anchor_pos = g_EditorState.is_in_brush_creation_mode ? g_EditorState.preview_brush.pos : g_EditorState.gizmo_selection_centroid;
                if (active_viewport == VIEW_PERSPECTIVE) {
                    Vec3 cam_forward = { g_view_matrix[VIEW_PERSPECTIVE].m[2], g_view_matrix[VIEW_PERSPECTIVE].m[6], g_view_matrix[VIEW_PERSPECTIVE].m[10] };
                    Vec3 axis_dir = { 0 };
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
                    float dot_product = fabsf(vec3_dot(axis_dir, cam_forward));
                    if (dot_product > 0.99f) { if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) { g_EditorState.gizmo_drag_plane_normal = (Vec3){ 0, 1, 0 }; } else { g_EditorState.gizmo_drag_plane_normal = (Vec3){ 1, 0, 0 }; } }
                    else { g_EditorState.gizmo_drag_plane_normal = vec3_cross(axis_dir, cam_forward); vec3_normalize(&g_EditorState.gizmo_drag_plane_normal); }
                    g_EditorState.gizmo_drag_plane_d = -vec3_dot(g_EditorState.gizmo_drag_plane_normal, drag_object_anchor_pos);
                    Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                    float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                    float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                    Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                    Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                    Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
                    ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &g_EditorState.gizmo_drag_start_world);
                }
                else {
                    g_EditorState.gizmo_drag_start_world = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
                }
                break;
            }
            case GIZMO_OP_ROTATE: {
                if (active_viewport != VIEW_PERSPECTIVE) break;
                Vec3 object_pos_for_rotate_plane = g_EditorState.is_in_brush_creation_mode ? g_EditorState.preview_brush.pos : g_EditorState.gizmo_selection_centroid;
                if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) g_EditorState.gizmo_drag_plane_normal = (Vec3){ 1,0,0 };
                if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) g_EditorState.gizmo_drag_plane_normal = (Vec3){ 0,1,0 };
                if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) g_EditorState.gizmo_drag_plane_normal = (Vec3){ 0,0,1 };
                Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
                Vec3 intersect_point;
                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, -vec3_dot(g_EditorState.gizmo_drag_plane_normal, object_pos_for_rotate_plane), &intersect_point)) {
                    g_EditorState.gizmo_rotation_start_vec = vec3_sub(intersect_point, object_pos_for_rotate_plane);
                    vec3_normalize(&g_EditorState.gizmo_rotation_start_vec);
                }
                break;
            }
            }
            return;
        }
        else if (active_viewport >= VIEW_TOP_XZ && !g_EditorState.is_manipulating_gizmo && primary && primary->type == ENTITY_BRUSH) {
            Brush* b = &scene->brushes[primary->index];
            Vec3 mouse_world_pos = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
            float pick_dist_sq = (g_EditorState.ortho_cam_zoom[active_viewport - 1] * 0.05f);
            pick_dist_sq *= pick_dist_sq;
            for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                float dist_sq = 0;
                if (active_viewport == VIEW_TOP_XZ) dist_sq = (vert_world_pos.x - mouse_world_pos.x) * (vert_world_pos.x - mouse_world_pos.x) + (vert_world_pos.z - mouse_world_pos.z) * (vert_world_pos.z - mouse_world_pos.z);
                if (active_viewport == VIEW_FRONT_XY) dist_sq = (vert_world_pos.x - mouse_world_pos.x) * (vert_world_pos.x - mouse_world_pos.x) + (vert_world_pos.y - mouse_world_pos.y) * (vert_world_pos.y - mouse_world_pos.y);
                if (active_viewport == VIEW_SIDE_YZ) dist_sq = (vert_world_pos.z - mouse_world_pos.z) * (vert_world_pos.z - mouse_world_pos.z) + (vert_world_pos.y - mouse_world_pos.y) * (vert_world_pos.y - mouse_world_pos.y);
                if (dist_sq < pick_dist_sq) {
                    g_EditorState.is_vertex_manipulating = true;
                    g_EditorState.manipulated_vertex_index = v_idx;
                    primary->vertex_index = v_idx;
                    g_EditorState.vertex_manipulation_view = active_viewport;
                    g_EditorState.vertex_manipulation_start_pos = mouse_world_pos;
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                    return;
                }
            }
        }
        if (active_viewport == VIEW_PERSPECTIVE && primary && primary->type == ENTITY_BRUSH && !g_EditorState.is_manipulating_gizmo && !g_EditorState.is_manipulating_vertex_gizmo) {
            int picked_vertex = Editor_PickVertexAtScreenPos(scene, g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE], VIEW_PERSPECTIVE);
            if (picked_vertex != -1) {
                primary->vertex_index = picked_vertex;
                return;
            }
        }
        if (active_viewport == VIEW_PERSPECTIVE && !g_EditorState.is_in_brush_creation_mode) {
            Editor_PickObjectAtScreenPos(g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE], VIEW_PERSPECTIVE);
        }

        if (g_EditorState.num_selections == 0 && active_viewport != VIEW_PERSPECTIVE && active_viewport != VIEW_COUNT && !g_EditorState.is_in_brush_creation_mode) {
            g_EditorState.is_dragging_for_creation = true;
            g_EditorState.brush_creation_start_point_2d_drag = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
            g_EditorState.brush_creation_view = active_viewport;
            g_EditorState.preview_brush_world_min = g_EditorState.brush_creation_start_point_2d_drag;
            g_EditorState.preview_brush_world_max = g_EditorState.brush_creation_start_point_2d_drag;
            Editor_UpdatePreviewBrushForInitialDrag(g_EditorState.preview_brush_world_min, g_EditorState.preview_brush_world_max, g_EditorState.brush_creation_view);
        }
    }
    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        if (g_EditorState.is_sprinkling) {
            g_EditorState.is_sprinkling = false;
        }
        if (g_EditorState.is_painting) {
            g_EditorState.is_painting = false;
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Vertex Paint");
        }
        if (g_EditorState.is_sculpting) {
            g_EditorState.is_sculpting = false;
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Vertex Sculpt");
            return;
        }
        if (g_EditorState.is_manipulating_vertex_gizmo) {
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Move Vertex (Gizmo)");
            g_EditorState.is_manipulating_vertex_gizmo = false;
            g_EditorState.vertex_gizmo_active_axis = GIZMO_AXIS_NONE;
        }
        if (g_EditorState.is_vertex_manipulating) {
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Move Vertex");
            g_EditorState.is_vertex_manipulating = false;
        }
        if (g_EditorState.is_dragging_selected_brush_handle) {
            g_EditorState.is_dragging_selected_brush_handle = false;
            g_EditorState.selected_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Resize Brush");
        }
        if (g_EditorState.is_dragging_selected_brush_body) {
            g_EditorState.is_dragging_selected_brush_body = false;
            Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Move Selection");
        }
        if (g_EditorState.is_dragging_preview_brush_handle) {
            g_EditorState.is_dragging_preview_brush_handle = false;
            g_EditorState.preview_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
        }
        else if (g_EditorState.is_dragging_preview_brush_body) {
            Vec3 current_mouse_world_unprojected = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[g_EditorState.preview_brush_drag_body_view], g_EditorState.preview_brush_drag_body_view);
            Vec3 delta = vec3_sub(current_mouse_world_unprojected, g_EditorState.preview_brush_drag_body_start_mouse_world);

            Vec3 current_brush_min_before_move = g_EditorState.preview_brush_world_min;
            Vec3 current_brush_max_before_move = g_EditorState.preview_brush_world_max;
            Vec3 brush_size = vec3_sub(current_brush_max_before_move, current_brush_min_before_move);

            Vec3 new_world_min = vec3_add(g_EditorState.preview_brush_drag_body_start_brush_world_min_at_drag_start, delta);
            Vec3 new_world_max;

            if (g_EditorState.snap_to_grid) {
                ViewportType view = g_EditorState.preview_brush_drag_body_view;
                Vec3 original_min_at_drag_start_for_fixed_axes = g_EditorState.preview_brush_drag_body_start_brush_world_min_at_drag_start;

                if (view == VIEW_TOP_XZ) {
                    new_world_min.x = SnapValue(new_world_min.x, g_EditorState.grid_size);
                    new_world_min.z = SnapValue(new_world_min.z, g_EditorState.grid_size);
                    new_world_min.y = original_min_at_drag_start_for_fixed_axes.y;
                }
                else if (view == VIEW_FRONT_XY) {
                    new_world_min.x = SnapValue(new_world_min.x, g_EditorState.grid_size);
                    new_world_min.y = SnapValue(new_world_min.y, g_EditorState.grid_size);
                    new_world_min.z = original_min_at_drag_start_for_fixed_axes.z;
                }
                else if (view == VIEW_SIDE_YZ) {
                    new_world_min.y = SnapValue(new_world_min.y, g_EditorState.grid_size);
                    new_world_min.z = SnapValue(new_world_min.z, g_EditorState.grid_size);
                    new_world_min.x = original_min_at_drag_start_for_fixed_axes.x;
                }
            }
            new_world_max = vec3_add(new_world_min, brush_size);

            g_EditorState.preview_brush_world_min = new_world_min;
            g_EditorState.preview_brush_world_max = new_world_max;

            Editor_UpdatePreviewBrushFromWorldMinMax();
        }
        if (g_EditorState.is_manipulating_gizmo) {
            Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Transform Selection");
            g_EditorState.is_manipulating_gizmo = false; g_EditorState.gizmo_active_axis = GIZMO_AXIS_NONE;
        }
        if (g_EditorState.is_dragging_for_creation) {
            g_EditorState.is_dragging_for_creation = false;
            Vec3 current_point = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.brush_creation_view], (ViewportType)g_EditorState.brush_creation_view);

            Editor_UpdatePreviewBrushForInitialDrag(g_EditorState.brush_creation_start_point_2d_drag, current_point, g_EditorState.brush_creation_view);
            g_EditorState.is_in_brush_creation_mode = true;
        }
    }
    if (event->type == SDL_MOUSEMOTION) {
        ViewportType active_viewport = VIEW_COUNT;
        for (int i = 0; i < VIEW_COUNT; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                active_viewport = (ViewportType)i;
                break;
            }
        }
        if (g_EditorState.is_painting) {
            Brush* b = &scene->brushes[primary->index];
            bool needs_update = false;

            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    Vec3 mouse_world_pos = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                    float radius_sq = g_EditorState.paint_brush_radius * g_EditorState.paint_brush_radius;

                    for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                        float dist_sq = 0;
                        if (i == VIEW_TOP_XZ) dist_sq = (vert_world_pos.x - mouse_world_pos.x) * (vert_world_pos.x - mouse_world_pos.x) + (vert_world_pos.z - mouse_world_pos.z) * (vert_world_pos.z - mouse_world_pos.z);
                        if (i == VIEW_FRONT_XY) dist_sq = (vert_world_pos.x - mouse_world_pos.x) * (vert_world_pos.x - mouse_world_pos.x) + (vert_world_pos.y - mouse_world_pos.y) * (vert_world_pos.y - mouse_world_pos.y);
                        if (i == VIEW_SIDE_YZ) dist_sq = (vert_world_pos.z - mouse_world_pos.z) * (vert_world_pos.z - mouse_world_pos.z) + (vert_world_pos.y - mouse_world_pos.y) * (vert_world_pos.y - mouse_world_pos.y);

                        if (dist_sq < radius_sq) {
                            float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.paint_brush_radius;
                            float blend_amount = g_EditorState.paint_brush_strength * falloff * engine->deltaTime * 10.0f;
                            float* channel_to_paint = NULL;
                            if (g_EditorState.paint_channel == 0) channel_to_paint = &b->vertices[v_idx].color.x;
                            else if (g_EditorState.paint_channel == 1) channel_to_paint = &b->vertices[v_idx].color.y;
                            else if (g_EditorState.paint_channel == 2) channel_to_paint = &b->vertices[v_idx].color.z;

                            if (channel_to_paint) {
                                if (SDL_GetModState() & KMOD_SHIFT) {
                                    *channel_to_paint -= blend_amount;
                                }
                                else {
                                    *channel_to_paint += blend_amount;
                                }
                                *channel_to_paint = fmaxf(0.0f, fminf(1.0f, *channel_to_paint));
                                needs_update = true;
                            }
                        }
                    }
                }
            }
            if (needs_update) {
                Brush_CreateRenderData(b);
            }
        }
        if (g_EditorState.is_sculpting) {
            Brush* b = &scene->brushes[primary->index];
            bool needs_update = false;

            if (SDL_GetModState() & KMOD_SHIFT) {
                Vec3* average_positions = (Vec3*)calloc(b->numVertices, sizeof(Vec3));
                if (average_positions) {
                    Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
                    Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                    for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        local_min.x = fminf(local_min.x, b->vertices[v_idx].pos.x);
                        local_min.y = fminf(local_min.y, b->vertices[v_idx].pos.y);
                        local_min.z = fminf(local_min.z, b->vertices[v_idx].pos.z);
                        local_max.x = fmaxf(local_max.x, b->vertices[v_idx].pos.x);
                        local_max.y = fmaxf(local_max.y, b->vertices[v_idx].pos.y);
                        local_max.z = fmaxf(local_max.z, b->vertices[v_idx].pos.z);
                    }

                    for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                        float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
                        float dist_sq_from_brush = vec3_length_sq(vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                        if (dist_sq_from_brush < radius_sq) {
                            Vec3 neighbor_sum = { 0,0,0 };
                            int neighbor_count = 0;
                            for (int n_idx = 0; n_idx < b->numVertices; ++n_idx) {
                                if (v_idx == n_idx) continue;
                                float dist_sq_verts = vec3_length_sq(vec3_sub(b->vertices[v_idx].pos, b->vertices[n_idx].pos));
                                if (dist_sq_verts < (g_EditorState.grid_size * g_EditorState.grid_size * 2.0f)) {
                                    neighbor_sum = vec3_add(neighbor_sum, b->vertices[n_idx].pos);
                                    neighbor_count++;
                                }
                            }
                            if (neighbor_count > 0) average_positions[v_idx] = vec3_muls(neighbor_sum, 1.0f / neighbor_count);
                            else average_positions[v_idx] = b->vertices[v_idx].pos;
                        }
                        else {
                            average_positions[v_idx] = b->vertices[v_idx].pos;
                        }
                    }

                    for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                        float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
                        float dist_sq_from_brush = vec3_length_sq(vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                        if (dist_sq_from_brush < radius_sq) {
                            float falloff = 1.0f - sqrtf(dist_sq_from_brush) / g_EditorState.sculpt_brush_radius;
                            float smooth_strength = g_EditorState.sculpt_brush_strength * falloff * engine->unscaledDeltaTime * 1.5f;

                            Vec3 new_pos = vec3_add(vec3_muls(b->vertices[v_idx].pos, 1.0f - smooth_strength), vec3_muls(average_positions[v_idx], smooth_strength));

                            new_pos.x = fmaxf(local_min.x, fminf(local_max.x, new_pos.x));
                            new_pos.y = fmaxf(local_min.y, fminf(local_max.y, new_pos.y));
                            new_pos.z = fmaxf(local_min.z, fminf(local_max.z, new_pos.z));

                            b->vertices[v_idx].pos = new_pos;
                            needs_update = true;
                        }
                    }
                    free(average_positions);
                }
            }
            else {
                float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
                for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                    Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                    float dist_sq = vec3_length_sq(vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                    if (dist_sq < radius_sq) {
                        float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.sculpt_brush_radius;
                        float sculpt_amount = g_EditorState.sculpt_brush_strength * falloff * engine->unscaledDeltaTime * 10.0f;
                        if (SDL_GetModState() & KMOD_CTRL) sculpt_amount = -sculpt_amount;

                        b->vertices[v_idx].pos = vec3_add(b->vertices[v_idx].pos, vec3_muls(g_EditorState.paint_brush_world_normal, sculpt_amount));
                        needs_update = true;
                    }
                }
            }

            if (needs_update) {
                Brush_CreateRenderData(b);
                if (b->physicsBody) {
                    Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                    if (Brush_IsSolid(b) && b->numVertices > 0) {
                        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                        for (int k = 0; k < b->numVertices; ++k) world_verts[k] = mat4_mul_vec3(&b->modelMatrix, b->vertices[k].pos);
                        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                        free(world_verts);
                    }
                    else {
                        b->physicsBody = NULL;
                    }
                }
            }
        }
        if (g_EditorState.is_dragging_preview_brush_handle) {
            Editor_AdjustPreviewBrushByHandle(g_EditorState.mouse_pos_in_viewport[g_EditorState.preview_brush_drag_handle_view], g_EditorState.preview_brush_drag_handle_view);
        }
        else if (g_EditorState.is_dragging_selected_brush_handle) {
            Editor_AdjustSelectedBrushByHandle(scene, engine, g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
        }
        else if (g_EditorState.is_dragging_selected_brush_body) {
            Vec3 current_mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[g_EditorState.selected_brush_drag_body_view], g_EditorState.selected_brush_drag_body_view);

            if (g_EditorState.snap_to_grid) {
                current_mouse_world.x = SnapValue(current_mouse_world.x, g_EditorState.grid_size);
                current_mouse_world.y = SnapValue(current_mouse_world.y, g_EditorState.grid_size);
                current_mouse_world.z = SnapValue(current_mouse_world.z, g_EditorState.grid_size);
            }

            Vec3 delta = vec3_sub(current_mouse_world, g_EditorState.selected_brush_drag_body_start_mouse_world);

            for (int i = 0; i < g_EditorState.num_selections; ++i) {
                EditorSelection* sel = &g_EditorState.selections[i];
                Vec3 start_pos = g_EditorState.gizmo_drag_start_positions[i];
                Vec3 new_pos = vec3_add(start_pos, delta);

                if (sel->type == ENTITY_BRUSH) {
                    Brush* b = &scene->brushes[sel->index];
                    Vec3 old_pos = b->pos;
                    b->pos = new_pos;

                    if (g_EditorState.texture_lock_enabled) {
                        Vec3 frame_move = vec3_sub(new_pos, old_pos);
                        float du = 0, dv = 0;
                        if (g_EditorState.selected_brush_drag_body_view == VIEW_TOP_XZ) { du = frame_move.x; dv = frame_move.z; }
                        else if (g_EditorState.selected_brush_drag_body_view == VIEW_FRONT_XY) { du = frame_move.x; dv = frame_move.y; }
                        else { du = frame_move.z; dv = frame_move.y; }

                        for (int f = 0; f < b->numFaces; ++f) {
                            if (b->faces[f].uv_scale.x != 0) b->faces[f].uv_offset.x -= du / b->faces[f].uv_scale.x;
                            if (b->faces[f].uv_scale.y != 0) b->faces[f].uv_offset.y -= dv / b->faces[f].uv_scale.y;
                        }
                        Brush_CreateRenderData(b);
                    }
                    Brush_UpdateMatrix(b);
                    if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
                }
                else if (sel->type == ENTITY_MODEL) {
                    SceneObject* obj = &scene->objects[sel->index];
                    obj->pos = new_pos;
                    SceneObject_UpdateMatrix(obj);
                    if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix);
                }
                else if (sel->type == ENTITY_LIGHT) scene->lights[sel->index].position = new_pos;
                else if (sel->type == ENTITY_DECAL) { scene->decals[sel->index].pos = new_pos; Decal_UpdateMatrix(&scene->decals[sel->index]); }
                else if (sel->type == ENTITY_SOUND) { scene->soundEntities[sel->index].pos = new_pos; SoundSystem_SetSourcePosition(scene->soundEntities[sel->index].sourceID, new_pos); }
                else if (sel->type == ENTITY_PARTICLE_EMITTER) scene->particleEmitters[sel->index].pos = new_pos;
                else if (sel->type == ENTITY_SPRITE) scene->sprites[sel->index].pos = new_pos;
                else if (sel->type == ENTITY_VIDEO_PLAYER) scene->videoPlayers[sel->index].pos = new_pos;
                else if (sel->type == ENTITY_PARALLAX_ROOM) { scene->parallaxRooms[sel->index].pos = new_pos; ParallaxRoom_UpdateMatrix(&scene->parallaxRooms[sel->index]); }
                else if (sel->type == ENTITY_LOGIC) scene->logicEntities[sel->index].pos = new_pos;
                else if (sel->type == ENTITY_PLAYERSTART) scene->playerStart.position = new_pos;
            }
        }
        else if (g_EditorState.is_manipulating_vertex_gizmo) {
            Brush* b = &scene->brushes[primary->index];
            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
            float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
            Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
            Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
            Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
            Vec3 current_intersect_point;
            if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.vertex_gizmo_drag_plane_normal, g_EditorState.vertex_gizmo_drag_plane_d, &current_intersect_point)) {
                Vec3 delta = vec3_sub(current_intersect_point, g_EditorState.vertex_gizmo_drag_start_world);
                Vec3 axis_dir = { 0 };
                if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
                if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
                if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
                float projection_len = vec3_dot(delta, axis_dir);
                Vec3 projected_delta = vec3_muls(axis_dir, projection_len);
                Vec3 new_world_pos = vec3_add(g_EditorState.vertex_drag_start_pos_world, projected_delta);
                if (g_EditorState.snap_to_grid) { new_world_pos.x = SnapValue(new_world_pos.x, g_EditorState.grid_size); new_world_pos.y = SnapValue(new_world_pos.y, g_EditorState.grid_size); new_world_pos.z = SnapValue(new_world_pos.z, g_EditorState.grid_size); }
                Mat4 inv_model;
                mat4_inverse(&b->modelMatrix, &inv_model);
                b->vertices[primary->vertex_index].pos = mat4_mul_vec3(&inv_model, new_world_pos);
                Brush_CreateRenderData(b);
                if (b->physicsBody) {
                    Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                    if (Brush_IsSolid(b) && b->numVertices > 0) {
                        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                        for (int i = 0; i < b->numVertices; ++i) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                        free(world_verts);
                    }
                    else {
                        b->physicsBody = NULL;
                    }
                }
            }
        }
        else if (g_EditorState.is_manipulating_gizmo && primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
            Brush* b = &scene->brushes[primary->index];
            BrushFace* face = &b->faces[primary->face_index];
            if (face->numVertexIndices < 3) return;

            Vec3 delta = { 0 };
            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[g_EditorState.gizmo_drag_view];

            if (g_EditorState.gizmo_drag_view == VIEW_PERSPECTIVE) {
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
                Vec3 current_intersect_point;
                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &current_intersect_point)) {
                    delta = vec3_sub(current_intersect_point, g_EditorState.gizmo_drag_start_world);
                }
            }
            else {
                Vec3 current_point = ScreenToWorld(screen_pos, g_EditorState.gizmo_drag_view);
                delta = vec3_sub(current_point, g_EditorState.gizmo_drag_start_world);
            }

            Vec3 axis_dir = { 0 };
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
            float projection_len = vec3_dot(delta, axis_dir);
            Vec3 projected_delta = vec3_muls(axis_dir, projection_len);

            if (g_EditorState.snap_to_grid) {
                projected_delta.x = SnapValue(projected_delta.x, g_EditorState.grid_size);
                projected_delta.y = SnapValue(projected_delta.y, g_EditorState.grid_size);
                projected_delta.z = SnapValue(projected_delta.z, g_EditorState.grid_size);
            }

            Mat4 inv_model; mat4_inverse(&b->modelMatrix, &inv_model);
            for (int i = 0; i < face->numVertexIndices; ++i) {
                int vert_idx = face->vertexIndices[i];
                Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[vert_idx].pos);
                Vec3 new_world_pos = vec3_add(vert_world_pos, projected_delta);
                b->vertices[vert_idx].pos = mat4_mul_vec3(&inv_model, new_world_pos);
            }

            Brush_CreateRenderData(b);
            if (b->physicsBody) {
                Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                if (Brush_IsSolid(b) && b->numVertices > 0) {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int j = 0; j < b->numVertices; ++j) world_verts[j] = mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
                else { b->physicsBody = NULL; }
            }

            g_EditorState.gizmo_drag_start_world = vec3_add(g_EditorState.gizmo_drag_start_world, projected_delta);

            return;
        }
        else if (g_EditorState.is_vertex_manipulating) {
            Brush* b = &scene->brushes[primary->index];
            Vec3 current_mouse_world = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.vertex_manipulation_view], g_EditorState.vertex_manipulation_view);
            Vec3* vert_local_pos = &b->vertices[g_EditorState.manipulated_vertex_index].pos;
            Mat4 inv_model;
            mat4_inverse(&b->modelMatrix, &inv_model);
            Vec3 vert_world = mat4_mul_vec3(&b->modelMatrix, *vert_local_pos);
            if (g_EditorState.vertex_manipulation_view == VIEW_TOP_XZ) { vert_world.x = current_mouse_world.x; vert_world.z = current_mouse_world.z; }
            if (g_EditorState.vertex_manipulation_view == VIEW_FRONT_XY) { vert_world.x = current_mouse_world.x; vert_world.y = current_mouse_world.y; }
            if (g_EditorState.vertex_manipulation_view == VIEW_SIDE_YZ) { vert_world.y = current_mouse_world.y; vert_world.z = current_mouse_world.z; }
            *vert_local_pos = mat4_mul_vec3(&inv_model, vert_world);
            Brush_CreateRenderData(b);
            if (b->physicsBody) {
                Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                if (Brush_IsSolid(b) && b->numVertices > 0) {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int i = 0; i < b->numVertices; ++i) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
                else {
                    b->physicsBody = NULL;
                }
            }
            return;
        }
        else if (g_EditorState.is_manipulating_gizmo && primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
            Brush* b = &scene->brushes[primary->index];
            BrushFace* face = &b->faces[primary->face_index];
            if (face->numVertexIndices < 3) return;

            Vec3 delta = { 0 };
            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[g_EditorState.gizmo_drag_view];

            if (g_EditorState.gizmo_drag_view == VIEW_PERSPECTIVE) {
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
                Vec3 current_intersect_point;
                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &current_intersect_point)) {
                    delta = vec3_sub(current_intersect_point, g_EditorState.gizmo_drag_start_world);
                }
            }
            else {
                Vec3 current_point = ScreenToWorld(screen_pos, g_EditorState.gizmo_drag_view);
                delta = vec3_sub(current_point, g_EditorState.gizmo_drag_start_world);
            }

            Vec3 axis_dir = { 0 };
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
            float projection_len = vec3_dot(delta, axis_dir);
            Vec3 projected_delta = vec3_muls(axis_dir, projection_len);

            if (g_EditorState.snap_to_grid) {
                projected_delta.x = SnapValue(projected_delta.x, g_EditorState.grid_size);
                projected_delta.y = SnapValue(projected_delta.y, g_EditorState.grid_size);
                projected_delta.z = SnapValue(projected_delta.z, g_EditorState.grid_size);
            }

            Mat4 inv_model; mat4_inverse(&b->modelMatrix, &inv_model);
            for (int i = 0; i < face->numVertexIndices; ++i) {
                int vert_idx = face->vertexIndices[i];
                Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[vert_idx].pos);
                Vec3 new_world_pos = vec3_add(vert_world_pos, projected_delta);
                b->vertices[vert_idx].pos = mat4_mul_vec3(&inv_model, new_world_pos);
            }

            Brush_CreateRenderData(b);
            if (b->physicsBody) {
                Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                if (Brush_IsSolid(b) && b->numVertices > 0) {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int j = 0; j < b->numVertices; ++j) world_verts[j] = mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
                else { b->physicsBody = NULL; }
            }

            g_EditorState.gizmo_drag_start_world = vec3_add(g_EditorState.gizmo_drag_start_world, projected_delta);

            return;
        }
        else if (g_EditorState.is_manipulating_gizmo) {
            if ((SDL_GetModState() & KMOD_SHIFT) && !g_EditorState.gizmo_drag_has_cloned) {
                g_EditorState.gizmo_drag_has_cloned = true;

                int num_original_selections = g_EditorState.num_selections;
                EditorSelection* original_selections = malloc(num_original_selections * sizeof(EditorSelection));
                if (original_selections) {
                    memcpy(original_selections, g_EditorState.selections, num_original_selections * sizeof(EditorSelection));
                    Editor_ClearSelection();

                    for (int i = 0; i < num_original_selections; ++i) {
                        EditorSelection* sel = &original_selections[i];
                        switch (sel->type) {
                        case ENTITY_MODEL: Editor_DuplicateModel(scene, engine, sel->index); break;
                        case ENTITY_BRUSH: Editor_DuplicateBrush(scene, engine, sel->index); break;
                        case ENTITY_LIGHT: Editor_DuplicateLight(scene, sel->index); break;
                        case ENTITY_DECAL: Editor_DuplicateDecal(scene, sel->index); break;
                        case ENTITY_SOUND: Editor_DuplicateSoundEntity(scene, sel->index); break;
                        case ENTITY_PARTICLE_EMITTER: Editor_DuplicateParticleEmitter(scene, sel->index); break;
                        case ENTITY_VIDEO_PLAYER: Editor_DuplicateVideoPlayer(scene, sel->index); break;
                        case ENTITY_PARALLAX_ROOM: Editor_DuplicateParallaxRoom(scene, sel->index); break;
                        case ENTITY_LOGIC: Editor_DuplicateLogicEntity(scene, engine, sel->index); break;
                        case ENTITY_SPRITE: Editor_DuplicateSprite(scene, sel->index); break;
                        default: break;
                        }
                    }
                    free(original_selections);

                    free(g_EditorState.gizmo_drag_start_positions);
                    free(g_EditorState.gizmo_drag_start_rotations);
                    free(g_EditorState.gizmo_drag_start_scales);
                    g_EditorState.gizmo_drag_start_positions = malloc(g_EditorState.num_selections * sizeof(Vec3));
                    g_EditorState.gizmo_drag_start_rotations = malloc(g_EditorState.num_selections * sizeof(Vec3));
                    g_EditorState.gizmo_drag_start_scales = malloc(g_EditorState.num_selections * sizeof(Vec3));

                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        switch (sel->type) {
                        case ENTITY_MODEL: g_EditorState.gizmo_drag_start_positions[i] = scene->objects[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->objects[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = scene->objects[sel->index].scale; break;
                        case ENTITY_BRUSH: g_EditorState.gizmo_drag_start_positions[i] = scene->brushes[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->brushes[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = scene->brushes[sel->index].scale; break;
                        case ENTITY_LIGHT: g_EditorState.gizmo_drag_start_positions[i] = scene->lights[sel->index].position; g_EditorState.gizmo_drag_start_rotations[i] = scene->lights[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 }; break;
                        case ENTITY_DECAL: g_EditorState.gizmo_drag_start_positions[i] = scene->decals[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->decals[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = scene->decals[sel->index].size; break;
                        case ENTITY_SOUND: g_EditorState.gizmo_drag_start_positions[i] = scene->soundEntities[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 }; g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 }; break;
                        case ENTITY_PARTICLE_EMITTER: g_EditorState.gizmo_drag_start_positions[i] = scene->particleEmitters[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 }; g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 }; break;
                        case ENTITY_SPRITE: g_EditorState.gizmo_drag_start_positions[i] = scene->sprites[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 }; g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ scene->sprites[sel->index].scale,1,1 }; break;
                        case ENTITY_VIDEO_PLAYER: g_EditorState.gizmo_drag_start_positions[i] = scene->videoPlayers[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->videoPlayers[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ scene->videoPlayers[sel->index].size.x, scene->videoPlayers[sel->index].size.y, 1.0f }; break;
                        case ENTITY_PARALLAX_ROOM: g_EditorState.gizmo_drag_start_positions[i] = scene->parallaxRooms[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->parallaxRooms[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ scene->parallaxRooms[sel->index].size.x, scene->parallaxRooms[sel->index].size.y, 1.0f }; break;
                        case ENTITY_LOGIC: g_EditorState.gizmo_drag_start_positions[i] = scene->logicEntities[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->logicEntities[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 }; break;
                        case ENTITY_PLAYERSTART: g_EditorState.gizmo_drag_start_positions[i] = scene->playerStart.position; g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 }; g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 }; break;
                        default: g_EditorState.gizmo_drag_start_positions[i] = (Vec3){ 0,0,0 }; g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 }; g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 }; break;
                        }
                    }
                }
            }
            Vec3 pos_delta = { 0 };
            Vec3 scale_delta = { 0 };
            float rot_angle_delta = 0.0f;
            Mat4 delta_rot_matrix;
            mat4_identity(&delta_rot_matrix);

            Vec3 current_intersect_point;
            bool intersection_found = false;

            if (g_EditorState.gizmo_drag_view == VIEW_PERSPECTIVE) {
                Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);

                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &current_intersect_point)) {
                    intersection_found = true;
                }
            }
            else {
                current_intersect_point = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.gizmo_drag_view], g_EditorState.gizmo_drag_view);
                intersection_found = true;
            }

            if (intersection_found) {
                if (g_EditorState.current_gizmo_operation == GIZMO_OP_ROTATE) {
                    Vec3 object_pos_for_rotate = g_EditorState.gizmo_selection_centroid;
                    Vec3 current_vec = vec3_sub(current_intersect_point, object_pos_for_rotate);
                    vec3_normalize(&current_vec);
                    Vec3 u_axis = g_EditorState.gizmo_rotation_start_vec;
                    Vec3 v_axis = vec3_cross(g_EditorState.gizmo_drag_plane_normal, u_axis);

                    float u_coord = vec3_dot(current_vec, u_axis);
                    float v_coord = vec3_dot(current_vec, v_axis);

                    float angle = atan2f(v_coord, u_coord) * (180.0f / M_PI);

                    if (SDL_GetModState() & KMOD_CTRL) {
                        angle = SnapAngle(angle, 15.0f);
                    }
                    rot_angle_delta = angle;

                    float angle_rad = rot_angle_delta * (M_PI / 180.0f);
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) delta_rot_matrix = mat4_rotate_x(angle_rad);
                    else if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) delta_rot_matrix = mat4_rotate_y(angle_rad);
                    else delta_rot_matrix = mat4_rotate_z(angle_rad);
                }
                else {
                    Vec3 delta = vec3_sub(current_intersect_point, g_EditorState.gizmo_drag_start_world);

                    if (g_EditorState.gizmo_drag_view == VIEW_PERSPECTIVE) {
                        Vec3 axis_dir = { 0 };
                        if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
                        if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
                        if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
                        float projection_len = vec3_dot(delta, axis_dir);

                        if (g_EditorState.current_gizmo_operation == GIZMO_OP_TRANSLATE) {
                            if (g_EditorState.snap_to_grid) projection_len = SnapValue(projection_len, g_EditorState.grid_size);
                            pos_delta = vec3_muls(axis_dir, projection_len);
                        }
                        else {
                            if (g_EditorState.snap_to_grid) projection_len = SnapValue(projection_len, 0.25f);
                            scale_delta = vec3_muls(axis_dir, projection_len);
                        }
                    }
                    else {
                        pos_delta = delta;
                        scale_delta = delta;

                        switch (g_EditorState.gizmo_drag_view) {
                        case VIEW_TOP_XZ:   pos_delta.y = 0; scale_delta.y = 0; break;
                        case VIEW_FRONT_XY: pos_delta.z = 0; scale_delta.z = 0; break;
                        case VIEW_SIDE_YZ:  pos_delta.x = 0; scale_delta.x = 0; break;
                        default: break;
                        }

                        if (g_EditorState.snap_to_grid) {
                            if (g_EditorState.current_gizmo_operation == GIZMO_OP_TRANSLATE) {
                                pos_delta.x = SnapValue(pos_delta.x, g_EditorState.grid_size);
                                pos_delta.y = SnapValue(pos_delta.y, g_EditorState.grid_size);
                                pos_delta.z = SnapValue(pos_delta.z, g_EditorState.grid_size);
                            }
                            else {
                                scale_delta.x = SnapValue(scale_delta.x, 0.25f);
                                scale_delta.y = SnapValue(scale_delta.y, 0.25f);
                                scale_delta.z = SnapValue(scale_delta.z, 0.25f);
                            }
                        }
                    }
                }
            }

            Vec3 centroid = g_EditorState.gizmo_selection_centroid;
            for (int i = 0; i < g_EditorState.num_selections; ++i) {
                EditorSelection* sel = &g_EditorState.selections[i];

                Vec3 start_pos = g_EditorState.gizmo_drag_start_positions[i];
                Vec3 start_rot_eulers = g_EditorState.gizmo_drag_start_rotations[i];
                Vec3 start_scale = g_EditorState.gizmo_drag_start_scales[i];

                Vec3 new_pos = start_pos;
                Vec3 new_rot = start_rot_eulers;
                Vec3 new_scale = start_scale;

                if (g_EditorState.current_gizmo_operation == GIZMO_OP_TRANSLATE) {
                    new_pos = vec3_add(start_pos, pos_delta);
                }
                else if (g_EditorState.current_gizmo_operation == GIZMO_OP_SCALE) {
                    new_scale = vec3_add(start_scale, scale_delta);
                }
                else if (g_EditorState.current_gizmo_operation == GIZMO_OP_ROTATE) {
                    new_rot = start_rot_eulers;

                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) {
                        new_rot.x += rot_angle_delta;
                    }
                    else if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) {
                        new_rot.y += rot_angle_delta;
                    }
                    else if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) {
                        new_rot.z += rot_angle_delta;
                    }
                    Vec3 relative_pos = vec3_sub(start_pos, centroid);
                    Vec3 rotated_relative_pos = mat4_mul_vec3_dir(&delta_rot_matrix, relative_pos);
                    new_pos = vec3_add(centroid, rotated_relative_pos);
                }

                switch (sel->type) {
                case ENTITY_MODEL: {
                    SceneObject* obj = &scene->objects[sel->index];
                    obj->pos = new_pos; obj->rot = new_rot; obj->scale = new_scale;
                    SceneObject_UpdateMatrix(obj); if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); break;
                }
                case ENTITY_BRUSH: {
                    Brush* b = &scene->brushes[sel->index];
                    b->pos = new_pos; b->rot = new_rot; b->scale = new_scale;
                    Brush_UpdateMatrix(b); if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix); break;
                }
                case ENTITY_LIGHT: {
                    Light* l = &scene->lights[sel->index];
                    l->position = new_pos; l->rot = new_rot; break;
                }
                case ENTITY_DECAL: {
                    Decal* d = &scene->decals[sel->index];
                    d->pos = new_pos; d->rot = new_rot; d->size = new_scale;
                    Decal_UpdateMatrix(d); break;
                }
                case ENTITY_SOUND: {
                    SoundEntity* s = &scene->soundEntities[sel->index];
                    s->pos = new_pos; SoundSystem_SetSourcePosition(s->sourceID, s->pos); break;
                }
                case ENTITY_PARTICLE_EMITTER: {
                    ParticleEmitter* p = &scene->particleEmitters[sel->index];
                    p->pos = new_pos; break;
                }
                case ENTITY_SPRITE: {
                    Sprite* s = &scene->sprites[sel->index];
                    s->pos = new_pos; s->scale = new_scale.x; break;
                }
                case ENTITY_VIDEO_PLAYER: {
                    VideoPlayer* vp = &scene->videoPlayers[sel->index];
                    vp->pos = new_pos; vp->rot = new_rot; vp->size.x = new_scale.x; vp->size.y = new_scale.y; break;
                }
                case ENTITY_PARALLAX_ROOM: {
                    ParallaxRoom* p = &scene->parallaxRooms[sel->index];
                    p->pos = new_pos; p->rot = new_rot; p->size.x = new_scale.x; p->size.y = new_scale.y;
                    ParallaxRoom_UpdateMatrix(p); break;
                }
                case ENTITY_LOGIC: {
                    LogicEntity* l = &scene->logicEntities[sel->index];
                    l->pos = new_pos; l->rot = new_rot; break;
                }
                case ENTITY_PLAYERSTART: {
                    scene->playerStart.position = new_pos; break;
                }
                default: break;
                }
            }
        }
        else if (g_EditorState.is_dragging_for_creation) {
                BrushCreationShapeType original_shape = g_EditorState.current_brush_shape;
                if (original_shape == BRUSH_SHAPE_ARCH) {
                    g_EditorState.current_brush_shape = BRUSH_SHAPE_BLOCK;
                }
                Vec3 current_point = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.brush_creation_view], (ViewportType)g_EditorState.brush_creation_view);
                Editor_UpdatePreviewBrushForInitialDrag(g_EditorState.brush_creation_start_point_2d_drag, current_point, g_EditorState.brush_creation_view);
                if (original_shape == BRUSH_SHAPE_ARCH) {
                    g_EditorState.current_brush_shape = original_shape;
                }
                }
        else if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    for (int j = 0; j < VIEW_COUNT; ++j) {
                        g_EditorState.is_viewport_focused[j] = (j == i);
                    }
                    break;
                }
            }

            if (g_EditorState.is_viewport_focused[VIEW_TOP_XZ]) { float ms = g_EditorState.ortho_cam_zoom[0] * 0.002f; g_EditorState.ortho_cam_pos[0].x -= event->motion.xrel * ms; g_EditorState.ortho_cam_pos[0].z -= event->motion.yrel * ms; }
            if (g_EditorState.is_viewport_focused[VIEW_FRONT_XY]) { float ms = g_EditorState.ortho_cam_zoom[1] * 0.002f; g_EditorState.ortho_cam_pos[1].x -= event->motion.xrel * ms; g_EditorState.ortho_cam_pos[1].y += event->motion.yrel * ms; }
            if (g_EditorState.is_viewport_focused[VIEW_SIDE_YZ]) { float ms = g_EditorState.ortho_cam_zoom[2] * 0.002f; g_EditorState.ortho_cam_pos[2].z += event->motion.xrel * ms; g_EditorState.ortho_cam_pos[2].y += event->motion.yrel * ms; }
        }
    }
    if (event->type == SDL_MOUSEWHEEL) {
        if (g_EditorState.is_in_z_mode) {
            if (event->wheel.y > 0) {
                g_EditorState.editor_camera_speed *= 1.25f;
            }
            else if (event->wheel.y < 0) {
                g_EditorState.editor_camera_speed /= 1.25f;
            }
            if (g_EditorState.editor_camera_speed < 0.1f) g_EditorState.editor_camera_speed = 0.1f;
            if (g_EditorState.editor_camera_speed > 500.0f) g_EditorState.editor_camera_speed = 500.0f;
            return;
        }
        bool hovered_any_viewport = false;
        for (int i = 1; i < VIEW_COUNT; i++) {
            if (g_EditorState.is_viewport_hovered[i]) { g_EditorState.ortho_cam_zoom[i - 1] -= event->wheel.y * g_EditorState.ortho_cam_zoom[i - 1] * 0.1f; hovered_any_viewport = true;  if (g_EditorState.ortho_cam_zoom[i - 1] > 64.0f) {
                g_EditorState.ortho_cam_zoom[i - 1] = 64.0f;
            }
            if (g_EditorState.ortho_cam_zoom[i - 1] < 0.5f) {
                g_EditorState.ortho_cam_zoom[i - 1] = 0.5f;
            }
            }
        }
    }
    if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        EditorSelection* primary = Editor_GetPrimarySelection();
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_m) {
            if (g_EditorState.num_selections > 0) {
                g_EditorState.show_transform_window = true;
                if (g_EditorState.transform_window_mode == TRANSFORM_MODE_SCALE) {
                    g_EditorState.transform_window_values = (Vec3){ 1, 1, 1 };
                }
                else {
                    g_EditorState.transform_window_values = (Vec3){ 0, 0, 0 };
                }
            }
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_l) {
            Editor_FlipSelection(scene, engine, 1);
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_i) {
            Editor_FlipSelection(scene, engine, 0);
            return;
        }
        if (event->key.keysym.sym == SDLK_F1) {
            g_EditorState.show_help_window = !g_EditorState.show_help_window;
            if (g_EditorState.show_help_window) {
                ScanDocFiles();
            }
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_z) { Undo_PerformUndo(scene, engine); return; }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_y) { Undo_PerformRedo(scene, engine); return; }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_s) {
            if (strcmp(g_EditorState.currentMapPath, "untitled.map") == 0) {
                g_EditorState.show_save_map_popup = true;
            }
            else {
                Scene_SaveMap(scene, NULL, g_EditorState.currentMapPath);
            }
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_g) {
            Editor_GroupSelection();
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_u) {
            Editor_UngroupSelection();
            return;
        }
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            Editor_ClearSelection();
            g_EditorState.is_in_brush_creation_mode = false;
            g_EditorState.is_clipping = false;
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_d) {
            if (g_EditorState.num_selections > 0) {
                int num_to_duplicate = g_EditorState.num_selections;
                EditorSelection* original_selections = malloc(num_to_duplicate * sizeof(EditorSelection));
                memcpy(original_selections, g_EditorState.selections, num_to_duplicate * sizeof(EditorSelection));

                Editor_ClearSelection();

                for (int i = 0; i < num_to_duplicate; ++i) {
                    EditorSelection* sel = &original_selections[i];
                    switch (sel->type) {
                    case ENTITY_MODEL: Editor_DuplicateModel(scene, engine, sel->index); break;
                    case ENTITY_BRUSH: Editor_DuplicateBrush(scene, engine, sel->index); break;
                    case ENTITY_LIGHT: Editor_DuplicateLight(scene, sel->index); break;
                    case ENTITY_DECAL: Editor_DuplicateDecal(scene, sel->index); break;
                    case ENTITY_SOUND: Editor_DuplicateSoundEntity(scene, sel->index); break;
                    case ENTITY_PARTICLE_EMITTER: Editor_DuplicateParticleEmitter(scene, sel->index); break;
                    case ENTITY_VIDEO_PLAYER: Editor_DuplicateVideoPlayer(scene, sel->index); break;
                    case ENTITY_PARALLAX_ROOM: Editor_DuplicateParallaxRoom(scene, sel->index); break;
                    case ENTITY_LOGIC: Editor_DuplicateLogicEntity(scene, engine, sel->index); break;
                    default: Console_Printf("Duplication not implemented for this entity type yet."); break;
                    }
                }
                free(original_selections);
            }
            return;
        }
        if (event->key.keysym.sym == SDLK_z) {
            if (g_EditorState.is_in_z_mode) {
                g_EditorState.is_in_z_mode = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            else {
                for (int i = 0; i < VIEW_COUNT; ++i) {
                    if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
                        g_EditorState.is_in_z_mode = true;
                        g_EditorState.captured_viewport = (ViewportType)i;
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        break;
                    }
                }
            }
        }
        if (event->key.keysym.sym == SDLK_c && !g_EditorState.is_clipping) {
            if (primary && primary->type == ENTITY_BRUSH) {
                g_EditorState.is_clipping = true;
                g_EditorState.clip_point_count = 0;
                memset(&g_EditorState.clip_side_point, 0, sizeof(Vec3));
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            }
        }
        if (g_EditorState.is_in_brush_creation_mode) {
            if (event->key.keysym.sym == SDLK_RETURN) {
                if (g_EditorState.current_brush_shape == BRUSH_SHAPE_ARCH) {
                    g_EditorState.arch_creation_start_point = g_EditorState.preview_brush_world_min;
                    g_EditorState.arch_creation_end_point = g_EditorState.preview_brush_world_max;
                    g_EditorState.arch_creation_view = g_EditorState.brush_creation_view;
                    g_EditorState.show_arch_properties_popup = true;
                }
                else {
                    Editor_CreateBrushFromPreview(scene, engine, &g_EditorState.preview_brush);
                    g_EditorState.is_in_brush_creation_mode = false;
                    g_EditorState.is_dragging_for_creation = false;
                    g_EditorState.is_dragging_preview_brush_handle = false;
                    g_EditorState.preview_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
                    g_EditorState.preview_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
                }
            }
        }
        else if (!g_EditorState.is_manipulating_gizmo && !g_EditorState.is_vertex_manipulating && !g_EditorState.is_manipulating_vertex_gizmo) {
            if (event->key.keysym.sym == SDLK_f && primary) {
                Vec3 target_pos = { 0 };
                float target_size = 1.0f;

                switch (primary->type) {
                case ENTITY_MODEL: {
                    SceneObject* obj = &scene->objects[primary->index];
                    target_pos = obj->pos;
                    Vec3 size_vec = vec3_sub(obj->model->aabb_max, obj->model->aabb_min);
                    target_size = fmaxf(fmaxf(size_vec.x * obj->scale.x, size_vec.y * obj->scale.y), size_vec.z * obj->scale.z);
                    break;
                }
                case ENTITY_BRUSH: {
                    Brush* b = &scene->brushes[primary->index];
                    target_pos = b->pos;
                    if (b->numVertices > 0) {
                        Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
                        Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                        for (int i = 0; i < b->numVertices; ++i) {
                            local_min.x = fminf(local_min.x, b->vertices[i].pos.x);
                            local_min.y = fminf(local_min.y, b->vertices[i].pos.y);
                            local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
                            local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x);
                            local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y);
                            local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
                        }
                        Vec3 size_vec = vec3_sub(local_max, local_min);
                        target_size = fmaxf(fmaxf(size_vec.x * b->scale.x, size_vec.y * b->scale.y), size_vec.z * b->scale.z);
                    }
                    break;
                }
                case ENTITY_LIGHT:              target_pos = scene->lights[primary->index].position; break;
                case ENTITY_PLAYERSTART:        target_pos = scene->playerStart.position; break;
                case ENTITY_DECAL:              target_pos = scene->decals[primary->index].pos; break;
                case ENTITY_SOUND:              target_pos = scene->soundEntities[primary->index].pos; break;
                case ENTITY_PARTICLE_EMITTER:   target_pos = scene->particleEmitters[primary->index].pos; break;
                case ENTITY_VIDEO_PLAYER:       target_pos = scene->videoPlayers[primary->index].pos; break;
                case ENTITY_PARALLAX_ROOM:      target_pos = scene->parallaxRooms[primary->index].pos; break;
                case ENTITY_LOGIC:              target_pos = scene->logicEntities[primary->index].pos; break;
                }

                Vec3 cam_forward = {
                    cosf(g_EditorState.editor_camera.pitch) * sinf(g_EditorState.editor_camera.yaw),
                    sinf(g_EditorState.editor_camera.pitch),
                    -cosf(g_EditorState.editor_camera.pitch) * cosf(g_EditorState.editor_camera.yaw)
                };
                vec3_normalize(&cam_forward);

                float distance_away = target_size * 2.0f;
                if (distance_away < 2.0f) distance_away = 2.0f;

                Vec3 new_cam_pos = vec3_sub(target_pos, vec3_muls(cam_forward, distance_away));
                g_EditorState.editor_camera.position = new_cam_pos;

                Vec3 new_forward = vec3_sub(target_pos, new_cam_pos);
                vec3_normalize(&new_forward);

                g_EditorState.editor_camera.pitch = asinf(new_forward.y);
                g_EditorState.editor_camera.yaw = atan2f(new_forward.x, -new_forward.z);
            }
            if (primary && primary->type == ENTITY_BRUSH && primary->vertex_index != -1) {
                bool moved = false;
                Vec3 move_delta = { 0 };
                float grid_size = g_EditorState.grid_size;

                if (event->key.keysym.sym == SDLK_UP) {
                    if (g_EditorState.last_active_2d_view == VIEW_TOP_XZ) move_delta.z = -grid_size;
                    else move_delta.y = grid_size;
                    moved = true;
                }
                else if (event->key.keysym.sym == SDLK_DOWN) {
                    if (g_EditorState.last_active_2d_view == VIEW_TOP_XZ) move_delta.z = grid_size;
                    else move_delta.y = -grid_size;
                    moved = true;
                }
                else if (event->key.keysym.sym == SDLK_LEFT) {
                    if (g_EditorState.last_active_2d_view == VIEW_SIDE_YZ) move_delta.z = -grid_size;
                    else move_delta.x = -grid_size;
                    moved = true;
                }
                else if (event->key.keysym.sym == SDLK_RIGHT) {
                    if (g_EditorState.last_active_2d_view == VIEW_SIDE_YZ) move_delta.z = grid_size;
                    else move_delta.x = grid_size;
                    moved = true;
                }

                if (moved) {
                    Brush* b = &scene->brushes[primary->index];
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);

                    Mat4 inv_rot_scale;
                    Mat4 rot_mat_x = mat4_rotate_x(b->rot.x * (M_PI / 180.0f));
                    Mat4 rot_mat_y = mat4_rotate_y(b->rot.y * (M_PI / 180.0f));
                    Mat4 rot_mat_z = mat4_rotate_z(b->rot.z * (M_PI / 180.0f));
                    Mat4 scale_mat = mat4_scale(b->scale);
                    mat4_multiply(&inv_rot_scale, &rot_mat_y, &rot_mat_x);
                    mat4_multiply(&inv_rot_scale, &rot_mat_z, &inv_rot_scale);
                    mat4_multiply(&inv_rot_scale, &inv_rot_scale, &scale_mat);
                    mat4_inverse(&inv_rot_scale, &inv_rot_scale);

                    Vec3 local_move_delta = mat4_mul_vec3_dir(&inv_rot_scale, move_delta);

                    b->vertices[primary->vertex_index].pos = vec3_add(b->vertices[primary->vertex_index].pos, local_move_delta);
                    Brush_CreateRenderData(b);
                    if (b->physicsBody) {
                        Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                        if (Brush_IsSolid(b) && b->numVertices > 0) {
                            Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                            for (int i = 0; i < b->numVertices; ++i) {
                                world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                            }
                            b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                            free(world_verts);
                        }
                        else {
                            b->physicsBody = NULL;
                        }
                    }
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Nudge Vertex");
                }
            }
            if (event->key.keysym.sym == SDLK_1) g_EditorState.current_gizmo_operation = GIZMO_OP_TRANSLATE;
            if (event->key.keysym.sym == SDLK_2) g_EditorState.current_gizmo_operation = GIZMO_OP_ROTATE;
            if (event->key.keysym.sym == SDLK_3) g_EditorState.current_gizmo_operation = GIZMO_OP_SCALE;
            if (event->key.keysym.sym == SDLK_LEFTBRACKET) {
                g_EditorState.grid_size /= 2.0f;
                if (g_EditorState.grid_size < 0.015625f) g_EditorState.grid_size = 0.015625f;
            }
            if (event->key.keysym.sym == SDLK_RIGHTBRACKET) {
                g_EditorState.grid_size *= 2.0f;
                if (g_EditorState.grid_size > 64.0f) g_EditorState.grid_size = 64.0f;
            }
            if (event->key.keysym.sym == SDLK_DELETE) {
                if (g_EditorState.num_selections > 0) {
                    EntityState* deleted_states = calloc(g_EditorState.num_selections, sizeof(EntityState));
                    int num_deleted = 0;
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        capture_state(&deleted_states[num_deleted++], scene, g_EditorState.selections[i].type, g_EditorState.selections[i].index);
                    }
                    Undo_PushDeleteMultipleEntities(scene, deleted_states, num_deleted, "Delete Selection");
                    for (int i = g_EditorState.num_selections - 1; i >= 0; --i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        switch (sel->type) {
                        case ENTITY_MODEL: _raw_delete_model(scene, sel->index, engine); break;
                        case ENTITY_BRUSH: _raw_delete_brush(scene, engine, sel->index); break;
                        case ENTITY_LIGHT: _raw_delete_light(scene, sel->index); break;
                        case ENTITY_DECAL: _raw_delete_decal(scene, sel->index); break;
                        case ENTITY_SOUND: _raw_delete_sound_entity(scene, sel->index); break;
                        case ENTITY_PARTICLE_EMITTER: _raw_delete_particle_emitter(scene, sel->index); break;
                        case ENTITY_SPRITE: _raw_delete_sprite(scene, sel->index); break;
                        case ENTITY_VIDEO_PLAYER: _raw_delete_video_player(scene, sel->index); break;
                        case ENTITY_PARALLAX_ROOM: _raw_delete_parallax_room(scene, sel->index); break;
                        case ENTITY_LOGIC: _raw_delete_logic_entity(scene, sel->index); break;
                        default: break;
                        }
                    }
                }
                Editor_ClearSelection();
            }
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_RIGHT && !g_EditorState.is_in_z_mode) {
        if (UI_IsWindowOpen("Face Edit Sheet")) {
            EditorSelection* primary = Editor_GetPrimarySelection();
            if (primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
                ViewportType active_viewport = VIEW_COUNT;
                for (int i = 0; i < VIEW_COUNT; ++i) {
                    if (g_EditorState.is_viewport_hovered[i]) {
                        active_viewport = (ViewportType)i;
                        break;
                    }
                }

                if (active_viewport == VIEW_PERSPECTIVE) {
                    float ndc_x = (g_EditorState.mouse_pos_in_viewport[active_viewport].x / g_EditorState.viewport_width[active_viewport]) * 2.0f - 1.0f;
                    float ndc_y = 1.0f - (g_EditorState.mouse_pos_in_viewport[active_viewport].y / g_EditorState.viewport_height[active_viewport]) * 2.0f;
                    Mat4 inv_proj, inv_view;
                    mat4_inverse(&g_proj_matrix[active_viewport], &inv_proj);
                    mat4_inverse(&g_view_matrix[active_viewport], &inv_view);
                    Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
                    Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
                    ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                    Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
                    Vec3 ray_dir_world = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
                    vec3_normalize(&ray_dir_world);
                    Vec3 ray_origin_world = g_EditorState.editor_camera.position;

                    float closest_t = FLT_MAX;
                    int hit_brush_index = -1;
                    int hit_face_index = -1;

                    for (int i = 0; i < g_CurrentScene->numBrushes; ++i) {
                        Brush* brush = &g_CurrentScene->brushes[i];
                        Mat4 inv_brush_model_matrix;
                        if (!mat4_inverse(&brush->modelMatrix, &inv_brush_model_matrix)) continue;
                        Vec3 ray_origin_local = mat4_mul_vec3(&inv_brush_model_matrix, ray_origin_world);
                        Vec3 ray_dir_local = mat4_mul_vec3_dir(&inv_brush_model_matrix, ray_dir_world);

                        for (int face_idx = 0; face_idx < brush->numFaces; ++face_idx) {
                            BrushFace* face = &brush->faces[face_idx];
                            if (face->numVertexIndices < 3) continue;

                            for (int k = 0; k < face->numVertexIndices - 2; ++k) {
                                Vec3 v0 = brush->vertices[face->vertexIndices[0]].pos;
                                Vec3 v1 = brush->vertices[face->vertexIndices[k + 1]].pos;
                                Vec3 v2 = brush->vertices[face->vertexIndices[k + 2]].pos;
                                float t;
                                if (RayIntersectsTriangle(ray_origin_local, ray_dir_local, v0, v1, v2, &t) && t < closest_t) {
                                    closest_t = t;
                                    hit_brush_index = i;
                                    hit_face_index = face_idx;
                                }
                            }
                        }
                    }

                    if (hit_brush_index != -1 && hit_face_index != -1) {
                        BrushFace* src_face = &scene->brushes[primary->index].faces[primary->face_index];
                        Brush* dest_brush = &scene->brushes[hit_brush_index];
                        BrushFace* dest_face = &dest_brush->faces[hit_face_index];

                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, hit_brush_index);

                        dest_face->material = src_face->material;
                        dest_face->material2 = src_face->material2;
                        dest_face->material3 = src_face->material3;
                        dest_face->material4 = src_face->material4;
                        dest_face->uv_scale = src_face->uv_scale;
                        dest_face->uv_offset = src_face->uv_offset;
                        dest_face->uv_rotation = src_face->uv_rotation;
                        dest_face->uv_scale2 = src_face->uv_scale2;
                        dest_face->uv_offset2 = src_face->uv_offset2;
                        dest_face->uv_rotation2 = src_face->uv_rotation2;
                        dest_face->uv_scale3 = src_face->uv_scale3;
                        dest_face->uv_offset3 = src_face->uv_offset3;
                        dest_face->uv_rotation3 = src_face->uv_rotation3;
                        dest_face->uv_scale4 = src_face->uv_scale4;
                        dest_face->uv_offset4 = src_face->uv_offset4;
                        dest_face->uv_rotation4 = src_face->uv_rotation4;
                        dest_face->lightmap_scale = src_face->lightmap_scale;

                        Brush_CreateRenderData(dest_brush);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, hit_brush_index, "Apply Face Properties");
                        return;
                    }
                }
            }
        }
        EditorSelection* primary = Editor_GetPrimarySelection();
        for (int i = 0; i < VIEW_COUNT; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                if (primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
                    g_EditorState.show_texture_browser = true;
                }
                break;
            }
        }
    }
}
void Editor_Update(Engine* engine, Scene* scene) {
    bool can_move = g_EditorState.is_in_z_mode || (g_EditorState.is_viewport_focused[VIEW_PERSPECTIVE] && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)));
    if (can_move) {
        const Uint8* state = SDL_GetKeyboardState(NULL); float speed = g_EditorState.editor_camera_speed * engine->deltaTime * (state[SDL_SCANCODE_LSHIFT] ? 2.5f : 1.0f);
        Vec3 forward = { cosf(g_EditorState.editor_camera.pitch) * sinf(g_EditorState.editor_camera.yaw), sinf(g_EditorState.editor_camera.pitch), -cosf(g_EditorState.editor_camera.pitch) * cosf(g_EditorState.editor_camera.yaw) };
        vec3_normalize(&forward); Vec3 right = vec3_cross(forward, (Vec3) { 0, 1, 0 }); vec3_normalize(&right);
        if (state[SDL_SCANCODE_W]) g_EditorState.editor_camera.position = vec3_add(g_EditorState.editor_camera.position, vec3_muls(forward, speed));
        if (state[SDL_SCANCODE_S]) g_EditorState.editor_camera.position = vec3_sub(g_EditorState.editor_camera.position, vec3_muls(forward, speed));
        if (state[SDL_SCANCODE_D]) g_EditorState.editor_camera.position = vec3_add(g_EditorState.editor_camera.position, vec3_muls(right, speed));
        if (state[SDL_SCANCODE_A]) g_EditorState.editor_camera.position = vec3_sub(g_EditorState.editor_camera.position, vec3_muls(right, speed));
        if (state[SDL_SCANCODE_E]) g_EditorState.editor_camera.position.y += speed;
        if (state[SDL_SCANCODE_Q]) g_EditorState.editor_camera.position.y -= speed;
    }

    EditorSelection* primary_sel = Editor_GetPrimarySelection();
    if (primary_sel && primary_sel->type == ENTITY_MODEL) {
        SceneObject* obj = &scene->objects[primary_sel->index];
        if (g_EditorState.preview_animation_playing && g_EditorState.preview_animation_index != -1) {
            g_EditorState.preview_animation_time += engine->deltaTime;
            AnimationClip* clip = &obj->model->animations[g_EditorState.preview_animation_index];
            if (g_EditorState.preview_animation_time > clip->duration) {
                g_EditorState.preview_animation_time = fmod(g_EditorState.preview_animation_time, clip->duration);
            }
        }
        if (obj->model && obj->model->num_animations > 0 && g_EditorState.preview_animation_index != -1) {
            evaluate_animation(obj, g_EditorState.preview_animation_time);
        }
    }

    for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
        if (g_EditorState.is_viewport_focused[i]) {
            g_EditorState.last_active_2d_view = (ViewportType)i;
        }
    }

    EditorSelection* primary = Editor_GetPrimarySelection();

    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
    g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_NONE;
    g_EditorState.paint_brush_hit_surface = false;
    if ((g_EditorState.is_painting_mode_enabled || g_EditorState.is_sculpting_mode_enabled) &&
        primary && primary->type == ENTITY_BRUSH &&
        g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE])
    {
        Brush* b = &scene->brushes[primary->index];
        Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];

        float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
        Mat4 inv_proj, inv_view;
        mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
        mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);

        Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
        Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
        ray_eye.z = -1.0f; ray_eye.w = 0.0f;

        Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
        Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
        vec3_normalize(&ray_dir);
        Vec3 ray_origin = g_EditorState.editor_camera.position;

        Mat4 inv_brush_model_matrix;
        if (mat4_inverse(&b->modelMatrix, &inv_brush_model_matrix)) {
            Vec3 ray_origin_local = mat4_mul_vec3(&inv_brush_model_matrix, ray_origin);
            Vec3 ray_dir_local = mat4_mul_vec3_dir(&inv_brush_model_matrix, ray_dir);
            float closest_t = FLT_MAX;

            for (int face_idx = 0; face_idx < b->numFaces; ++face_idx) {
                BrushFace* face = &b->faces[face_idx];
                if (face->numVertexIndices < 3) continue;

                for (int k = 0; k < face->numVertexIndices - 2; ++k) {
                    Vec3 v0_local = b->vertices[face->vertexIndices[0]].pos;
                    Vec3 v1_local = b->vertices[face->vertexIndices[k + 1]].pos;
                    Vec3 v2_local = b->vertices[face->vertexIndices[k + 2]].pos;

                    float t_triangle_local;
                    if (RayIntersectsTriangle(ray_origin_local, ray_dir_local, v0_local, v1_local, v2_local, &t_triangle_local)) {
                        if (t_triangle_local > 0.0f && t_triangle_local < closest_t) {
                            closest_t = t_triangle_local;
                            g_EditorState.paint_brush_hit_surface = true;
                            g_EditorState.paint_brush_world_pos = vec3_add(ray_origin, vec3_muls(ray_dir, t_triangle_local));
                            Vec3 face_normal_local = vec3_cross(vec3_sub(v1_local, v0_local), vec3_sub(v2_local, v0_local));
                            g_EditorState.paint_brush_world_normal = mat4_mul_vec3_dir(&b->modelMatrix, face_normal_local);
                            vec3_normalize(&g_EditorState.paint_brush_world_normal);
                        }
                    }
                }
            }
        }
        if (g_EditorState.is_painting) {
            bool needs_update = false;
            float radius_sq = g_EditorState.paint_brush_radius * g_EditorState.paint_brush_radius;
            for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                float dist_sq = vec3_length_sq(vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                if (dist_sq < radius_sq) {
                    float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.paint_brush_radius;
                    float blend_amount = g_EditorState.paint_brush_strength * falloff * engine->unscaledDeltaTime * 10.0f;
                    float* channel_to_paint = NULL;
                    if (g_EditorState.paint_channel == 0) channel_to_paint = &b->vertices[v_idx].color.x;
                    else if (g_EditorState.paint_channel == 1) channel_to_paint = &b->vertices[v_idx].color.y;
                    else if (g_EditorState.paint_channel == 2) channel_to_paint = &b->vertices[v_idx].color.z;

                    if (channel_to_paint) {
                        if (SDL_GetModState() & KMOD_SHIFT) *channel_to_paint -= blend_amount;
                        else *channel_to_paint += blend_amount;
                        *channel_to_paint = fmaxf(0.0f, fminf(1.0f, *channel_to_paint));
                        needs_update = true;
                    }
                }
            }
            if (needs_update) Brush_CreateRenderData(b);
        }
        if (g_EditorState.is_sculpting) {
            bool needs_update = false;
            float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
            for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                float dist_sq = vec3_length_sq(vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                if (dist_sq < radius_sq) {
                    float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.sculpt_brush_radius;
                    float sculpt_amount = g_EditorState.sculpt_brush_strength * falloff * engine->unscaledDeltaTime * 10.0f;
                    if (SDL_GetModState() & KMOD_SHIFT) sculpt_amount = -sculpt_amount;

                    b->vertices[v_idx].pos = vec3_add(b->vertices[v_idx].pos, vec3_muls(g_EditorState.paint_brush_world_normal, sculpt_amount));
                    needs_update = true;
                }
            }
            if (needs_update) {
                Brush_CreateRenderData(b);
                if (b->physicsBody) {
                    Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                    if (Brush_IsSolid(b) && b->numVertices > 0) {
                        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                        for (int k = 0; k < b->numVertices; ++k) world_verts[k] = mat4_mul_vec3(&b->modelMatrix, b->vertices[k].pos);
                        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                        free(world_verts);
                    }
                    else {
                        b->physicsBody = NULL;
                    }
                }
            }
        }
    }
    if (primary && primary->type == ENTITY_BRUSH && primary->vertex_index != -1 && !g_EditorState.is_manipulating_gizmo && !g_EditorState.is_manipulating_vertex_gizmo) {
        if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
            float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
            Mat4 inv_proj, inv_view;
            mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
            mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
            Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
            Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
            ray_eye.z = -1.0f; ray_eye.w = 0.0f;
            Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
            Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
            vec3_normalize(&ray_dir);
            Vec3 ray_origin = g_EditorState.editor_camera.position;

            Brush* b = &scene->brushes[primary->index];
            Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[primary->vertex_index].pos);

            const float pick_threshold = 0.1f;
            float min_dist = FLT_MAX;
            float t_ray, t_seg;

            float GIZMO_AXIS_LENGTH = 0.5f;

            Vec3 x_p1 = { vert_world_pos.x + GIZMO_AXIS_LENGTH, vert_world_pos.y, vert_world_pos.z };
            float dist_x = dist_RaySegment(ray_origin, ray_dir, vert_world_pos, x_p1, &t_ray, &t_seg);
            if (dist_x < pick_threshold && dist_x < min_dist) { min_dist = dist_x; g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_X; }

            Vec3 y_p1 = { vert_world_pos.x, vert_world_pos.y + GIZMO_AXIS_LENGTH, vert_world_pos.z };
            float dist_y = dist_RaySegment(ray_origin, ray_dir, vert_world_pos, y_p1, &t_ray, &t_seg);
            if (dist_y < pick_threshold && dist_y < min_dist) { min_dist = dist_y; g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_Y; }

            Vec3 z_p1 = { vert_world_pos.x, vert_world_pos.y, vert_world_pos.z + GIZMO_AXIS_LENGTH };
            float dist_z = dist_RaySegment(ray_origin, ray_dir, vert_world_pos, z_p1, &t_ray, &t_seg);
            if (dist_z < pick_threshold && dist_z < min_dist) { g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_Z; }
        }
    }
    g_EditorState.sprinkle_brush_hit_surface = false;
    if (g_EditorState.show_sprinkle_tool_window && g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
        Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
        float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
        Mat4 inv_proj, inv_view;
        mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
        mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
        Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
        Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
        ray_eye.z = -1.0f; ray_eye.w = 0.0f;
        Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
        Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
        vec3_normalize(&ray_dir);
        Vec3 ray_origin = g_EditorState.editor_camera.position;

        RaycastHitInfo hit_info;
        if (Physics_Raycast(engine->physicsWorld, ray_origin, vec3_add(ray_origin, vec3_muls(ray_dir, 1000.0f)), &hit_info)) {
            g_EditorState.sprinkle_brush_hit_surface = true;
            g_EditorState.sprinkle_brush_world_pos = hit_info.point;
        }

        if (g_EditorState.is_sprinkling) {
            g_EditorState.sprinkle_timer -= engine->unscaledDeltaTime;
            if (g_EditorState.sprinkle_timer <= 0.0f) {
                g_EditorState.sprinkle_timer = 1.0f / g_EditorState.sprinkle_density;

                if (g_EditorState.sprinkle_brush_hit_surface) {
                    if (g_EditorState.sprinkle_mode == 0) {
                        Vec3 surface_normal = g_EditorState.paint_brush_world_normal;

                        Vec3 tangent = vec3_cross(surface_normal, (Vec3) { 0.0f, 1.0f, 0.0f });
                        if (vec3_length_sq(tangent) < 0.001f) {
                            tangent = vec3_cross(surface_normal, (Vec3) { 1.0f, 0.0f, 0.0f });
                        }
                        vec3_normalize(&tangent);
                        Vec3 bitangent = vec3_cross(surface_normal, tangent);

                        float rand_angle = rand_float_range(0, 2.0f * M_PI);
                        float rand_dist = sqrtf(rand_float_range(0, 1)) * g_EditorState.sprinkle_radius;

                        Vec3 offset_on_plane = vec3_add(vec3_muls(tangent, cosf(rand_angle) * rand_dist), vec3_muls(bitangent, sinf(rand_angle) * rand_dist));
                        Vec3 final_pos = vec3_add(g_EditorState.sprinkle_brush_world_pos, offset_on_plane);

                        if (scene->numObjects < 8192) {
                            scene->numObjects++;
                            scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
                            if (!scene->objects) {
                                Console_Printf_Error("[ERROR] Failed to reallocate memory for scene objects!");
                                scene->numObjects--;
                                return;
                            }

                            SceneObject* newObj = &scene->objects[scene->numObjects - 1];
                            memset(newObj, 0, sizeof(SceneObject));

                            mat4_identity(&newObj->animated_local_transform);

                            strncpy(newObj->modelPath, g_EditorState.sprinkle_model_path, sizeof(newObj->modelPath) - 1);
                            newObj->pos = final_pos;
                            float scale = rand_float_range(g_EditorState.sprinkle_scale_min, g_EditorState.sprinkle_scale_max);
                            newObj->scale = (Vec3){ scale, scale, scale };
                            newObj->rot = (Vec3){ 0,0,0 };

                            if (g_EditorState.sprinkle_align_to_normal) {
                                Vec3 obj_forward = surface_normal;
                                Vec3 obj_up = (fabs(obj_forward.y) > 0.99f) ? (Vec3) { 1, 0, 0 } : (Vec3) { 0, 1, 0 };
                                Vec3 obj_right = vec3_cross(obj_up, obj_forward);
                                vec3_normalize(&obj_right);
                                obj_up = vec3_cross(obj_forward, obj_right);

                                Mat4 rot_matrix;
                                rot_matrix.m[0] = obj_right.x;  rot_matrix.m[4] = obj_up.x;  rot_matrix.m[8] = obj_forward.x;  rot_matrix.m[12] = 0;
                                rot_matrix.m[1] = obj_right.y;  rot_matrix.m[5] = obj_up.y;  rot_matrix.m[9] = obj_forward.y;  rot_matrix.m[13] = 0;
                                rot_matrix.m[2] = obj_right.z;  rot_matrix.m[6] = obj_up.z;  rot_matrix.m[10] = obj_forward.z; rot_matrix.m[14] = 0;
                                rot_matrix.m[3] = 0;            rot_matrix.m[7] = 0;         rot_matrix.m[11] = 0;              rot_matrix.m[15] = 1;

                                mat4_decompose(&rot_matrix, &(Vec3){0}, & newObj->rot, & (Vec3){0});
                            }

                            if (g_EditorState.sprinkle_random_yaw) {
                                newObj->rot.y = rand_float_range(0, 360.0f);
                            }

                            SceneObject_UpdateMatrix(newObj);
                            newObj->model = Model_Load(newObj->modelPath);
                            Undo_PushCreateEntity(scene, ENTITY_MODEL, scene->numObjects - 1, "Sprinkle Object");
                        }
                    }
                    else {
                        for (int i = scene->numObjects - 1; i >= 0; --i) {
                            if (strcmp(scene->objects[i].modelPath, g_EditorState.sprinkle_model_path) == 0) {
                                float dist_sq = vec3_length_sq(vec3_sub(scene->objects[i].pos, g_EditorState.sprinkle_brush_world_pos));
                                if (dist_sq < g_EditorState.sprinkle_radius * g_EditorState.sprinkle_radius / 10.0) {
                                    Undo_PushDeleteEntity(scene, ENTITY_MODEL, i, "Erase Sprinkled Model");
                                    _raw_delete_model(scene, i, engine);
                                    Editor_RemoveFromSelection(ENTITY_MODEL, i);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (!g_EditorState.is_dragging_preview_brush_handle) {
        g_EditorState.preview_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
    }

    if (!g_EditorState.is_dragging_preview_brush_body) {
        g_EditorState.is_hovering_preview_brush_body = false;
    }
    if (!g_EditorState.is_in_brush_creation_mode && primary && primary->type == ENTITY_BRUSH && !g_EditorState.is_dragging_selected_brush_handle && !g_EditorState.is_manipulating_gizmo) {
        g_EditorState.selected_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
        Brush* b = &scene->brushes[primary->index];
        if (b->numVertices == 0) return;

        Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        for (int i = 0; i < b->numVertices; ++i) {
            local_min.x = fminf(local_min.x, b->vertices[i].pos.x);
            local_min.y = fminf(local_min.y, b->vertices[i].pos.y);
            local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
            local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x);
            local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y);
            local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
        }
        Vec3 local_center = vec3_muls(vec3_add(local_min, local_max), 0.5f);

        for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                float handle_pick_dist_sq = powf(g_EditorState.ortho_cam_zoom[i - 1] * 0.055f, 2.0f);

                Vec3 handle_local_positions[6] = {
                    {local_min.x, local_center.y, local_center.z}, {local_max.x, local_center.y, local_center.z},
                    {local_center.x, local_min.y, local_center.z}, {local_center.x, local_max.y, local_center.z},
                    {local_center.x, local_center.y, local_min.z}, {local_center.x, local_center.y, local_max.z}
                };

                for (int h_idx = 0; h_idx < 6; ++h_idx) {
                    bool is_handle_relevant_to_view = false;
                    if (i == VIEW_TOP_XZ) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_X || h_idx == PREVIEW_BRUSH_HANDLE_MAX_X || h_idx == PREVIEW_BRUSH_HANDLE_MIN_Z || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Z) {
                            is_handle_relevant_to_view = true;
                        }
                    }
                    else if (i == VIEW_FRONT_XY) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_X || h_idx == PREVIEW_BRUSH_HANDLE_MAX_X || h_idx == PREVIEW_BRUSH_HANDLE_MIN_Y || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Y) {
                            is_handle_relevant_to_view = true;
                        }
                    }
                    else if (i == VIEW_SIDE_YZ) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_Y || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Y || h_idx == PREVIEW_BRUSH_HANDLE_MIN_Z || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Z) {
                            is_handle_relevant_to_view = true;
                        }
                    }

                    if (is_handle_relevant_to_view) {
                        Vec3 handle_world_pos = mat4_mul_vec3(&b->modelMatrix, handle_local_positions[h_idx]);
                        float dist_sq = 0.0f;

                        if (i == VIEW_TOP_XZ) {
                            dist_sq = powf(mouse_world.x - handle_world_pos.x, 2) + powf(mouse_world.z - handle_world_pos.z, 2);
                        }
                        else if (i == VIEW_FRONT_XY) {
                            dist_sq = powf(mouse_world.x - handle_world_pos.x, 2) + powf(mouse_world.y - handle_world_pos.y, 2);
                        }
                        else if (i == VIEW_SIDE_YZ) {
                            dist_sq = powf(mouse_world.y - handle_world_pos.y, 2) + powf(mouse_world.z - handle_world_pos.z, 2);
                        }

                        if (dist_sq <= handle_pick_dist_sq) {
                            g_EditorState.selected_brush_hovered_handle = (PreviewBrushHandleType)h_idx;
                            return;
                        }
                    }
                }
            }
        }
    }
    if (g_EditorState.is_in_brush_creation_mode && !g_EditorState.is_dragging_preview_brush_handle && !g_EditorState.is_manipulating_gizmo) {
        for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);

                float pick_radius_factor = 0.055f;
                float handle_pick_dist_sq = powf(g_EditorState.ortho_cam_zoom[i - 1] * pick_radius_factor, 2.0f);

                Vec3 handle_centers_world[PREVIEW_BRUSH_HANDLE_COUNT];
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MIN_X] = (Vec3){ g_EditorState.preview_brush_world_min.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MAX_X] = (Vec3){ g_EditorState.preview_brush_world_max.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MIN_Y] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush_world_min.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MAX_Y] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush_world_max.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MIN_Z] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush_world_min.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MAX_Z] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush_world_max.z };

                for (int h_idx = 0; h_idx < PREVIEW_BRUSH_HANDLE_COUNT; ++h_idx) {
                    bool is_handle_relevant_to_view = false;
                    float dist_sq = FLT_MAX;

                    if (i == VIEW_TOP_XZ) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_X || h_idx == PREVIEW_BRUSH_HANDLE_MAX_X) {
                            dist_sq = powf(mouse_world.x - handle_centers_world[h_idx].x, 2) + powf(mouse_world.z - handle_centers_world[h_idx].z, 2);
                            is_handle_relevant_to_view = true;
                        }
                        else if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_Z || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Z) {
                            dist_sq = powf(mouse_world.x - handle_centers_world[h_idx].x, 2) + powf(mouse_world.z - handle_centers_world[h_idx].z, 2);
                            is_handle_relevant_to_view = true;
                        }
                    }
                    else if (i == VIEW_FRONT_XY) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_X || h_idx == PREVIEW_BRUSH_HANDLE_MAX_X) {
                            dist_sq = powf(mouse_world.x - handle_centers_world[h_idx].x, 2) + powf(mouse_world.y - handle_centers_world[h_idx].y, 2);
                            is_handle_relevant_to_view = true;
                        }
                        else if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_Y || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Y) {
                            dist_sq = powf(mouse_world.x - handle_centers_world[h_idx].x, 2) + powf(mouse_world.y - handle_centers_world[h_idx].y, 2);
                            is_handle_relevant_to_view = true;
                        }
                    }
                    else if (i == VIEW_SIDE_YZ) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_Y || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Y) {
                            dist_sq = powf(mouse_world.y - handle_centers_world[h_idx].y, 2) + powf(mouse_world.z - handle_centers_world[h_idx].z, 2);
                            is_handle_relevant_to_view = true;
                        }
                        else if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_Z || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Z) {
                            dist_sq = powf(mouse_world.y - handle_centers_world[h_idx].y, 2) + powf(mouse_world.z - handle_centers_world[h_idx].z, 2);
                            is_handle_relevant_to_view = true;
                        }
                    }

                    if (is_handle_relevant_to_view && dist_sq <= handle_pick_dist_sq) {
                        g_EditorState.preview_brush_hovered_handle = (PreviewBrushHandleType)h_idx;
                        goto found_hovered_handle_update;
                    }
                }
            }
        }
    found_hovered_handle_update:;
    }
    if (g_EditorState.is_in_brush_creation_mode &&
        !g_EditorState.is_dragging_preview_brush_handle &&
        !g_EditorState.is_manipulating_gizmo &&
        g_EditorState.preview_brush_hovered_handle == PREVIEW_BRUSH_HANDLE_NONE) {
        g_EditorState.is_hovering_preview_brush_body = false;
        for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                Vec3 b_min = g_EditorState.preview_brush_world_min;
                Vec3 b_max = g_EditorState.preview_brush_world_max;

                bool hovered_this_view = false;
                if (i == VIEW_TOP_XZ) {
                    if (mouse_world.x >= b_min.x && mouse_world.x <= b_max.x &&
                        mouse_world.z >= b_min.z && mouse_world.z <= b_max.z) {
                        hovered_this_view = true;
                    }
                }
                else if (i == VIEW_FRONT_XY) {
                    if (mouse_world.x >= b_min.x && mouse_world.x <= b_max.x &&
                        mouse_world.y >= b_min.y && mouse_world.y <= b_max.y) {
                        hovered_this_view = true;
                    }
                }
                else if (i == VIEW_SIDE_YZ) {
                    if (mouse_world.y >= b_min.y && mouse_world.y <= b_max.y &&
                        mouse_world.z >= b_min.z && mouse_world.z <= b_max.z) {
                        hovered_this_view = true;
                    }
                }

                if (hovered_this_view) {
                    g_EditorState.is_hovering_preview_brush_body = true;
                    break;
                }
            }
        }
    }
    else if (g_EditorState.preview_brush_hovered_handle != PREVIEW_BRUSH_HANDLE_NONE) {
        g_EditorState.is_hovering_preview_brush_body = false;
    }
    if (primary && primary->type == ENTITY_BRUSH &&
        !g_EditorState.is_dragging_selected_brush_handle && !g_EditorState.is_dragging_selected_brush_body && !g_EditorState.is_manipulating_gizmo &&
        g_EditorState.selected_brush_hovered_handle == PREVIEW_BRUSH_HANDLE_NONE) {
        g_EditorState.is_hovering_selected_brush_body = false;
        Brush* b = &scene->brushes[primary->index];
        if (b->numVertices > 0) {
            Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (int i = 0; i < b->numVertices; ++i) {
                local_min.x = fminf(local_min.x, b->vertices[i].pos.x);
                local_min.y = fminf(local_min.y, b->vertices[i].pos.y);
                local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
                local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x);
                local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y);
                local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
            }
            Vec3 world_min = mat4_mul_vec3(&b->modelMatrix, local_min);
            Vec3 world_max = mat4_mul_vec3(&b->modelMatrix, local_max);

            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                    bool hovered_this_view = false;
                    if (i == VIEW_TOP_XZ) {
                        if (mouse_world.x >= world_min.x && mouse_world.x <= world_max.x && mouse_world.z >= world_min.z && mouse_world.z <= world_max.z)
                            hovered_this_view = true;
                    }
                    else if (i == VIEW_FRONT_XY) {
                        if (mouse_world.x >= world_min.x && mouse_world.x <= world_max.x && mouse_world.y >= world_min.y && mouse_world.y <= world_max.y)
                            hovered_this_view = true;
                    }
                    else if (i == VIEW_SIDE_YZ) {
                        if (mouse_world.y >= world_min.y && mouse_world.y <= world_max.y && mouse_world.z >= world_min.z && mouse_world.z <= world_max.z)
                            hovered_this_view = true;
                    }
                    if (hovered_this_view) {
                        g_EditorState.is_hovering_selected_brush_body = true;
                        break;
                    }
                }
            }
        }
    }
    if (g_EditorState.vertex_gizmo_hovered_axis == GIZMO_AXIS_NONE && g_EditorState.gizmo_active_axis == GIZMO_AXIS_NONE && (g_EditorState.num_selections > 0 || g_EditorState.is_in_brush_creation_mode)) {
        Vec3 gizmo_target_pos;
        bool use_gizmo = false;
        if (g_EditorState.is_in_brush_creation_mode) {
            gizmo_target_pos = g_EditorState.preview_brush.pos;
            use_gizmo = true;
        }
        else if (g_EditorState.num_selections > 0) {
            g_EditorState.gizmo_selection_centroid = (Vec3){ 0 };
            for (int i = 0; i < g_EditorState.num_selections; ++i) {
                Vec3 pos;
                switch (g_EditorState.selections[i].type) {
                case ENTITY_MODEL: pos = scene->objects[g_EditorState.selections[i].index].pos; break;
                case ENTITY_BRUSH: pos = scene->brushes[g_EditorState.selections[i].index].pos; break;
                case ENTITY_LIGHT: pos = scene->lights[g_EditorState.selections[i].index].position; break;
                case ENTITY_DECAL: pos = scene->decals[g_EditorState.selections[i].index].pos; break;
                case ENTITY_SOUND: pos = scene->soundEntities[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PARTICLE_EMITTER: pos = scene->particleEmitters[g_EditorState.selections[i].index].pos; break;
                case ENTITY_SPRITE: pos = scene->sprites[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PLAYERSTART: pos = scene->playerStart.position; break;
                case ENTITY_VIDEO_PLAYER: pos = scene->videoPlayers[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PARALLAX_ROOM: pos = scene->parallaxRooms[g_EditorState.selections[i].index].pos; break;
                case ENTITY_LOGIC: pos = scene->logicEntities[g_EditorState.selections[i].index].pos; break;
                default: pos = (Vec3){ 0 }; break;
                }
                g_EditorState.gizmo_selection_centroid = vec3_add(g_EditorState.gizmo_selection_centroid, pos);
            }
            g_EditorState.gizmo_selection_centroid = vec3_muls(g_EditorState.gizmo_selection_centroid, 1.0f / g_EditorState.num_selections);
            gizmo_target_pos = g_EditorState.gizmo_selection_centroid;
            use_gizmo = true;
        }

        if (use_gizmo) {
            if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
                Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view;
                mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
                mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
                Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
                ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
                Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
                vec3_normalize(&ray_dir);
                Editor_UpdateGizmoHover(scene, g_EditorState.editor_camera.position, ray_dir);
            }
            if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_NONE) {
                for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                    if (g_EditorState.is_viewport_hovered[i]) {
                        if (primary && primary->type == ENTITY_BRUSH) {
                            continue;
                        }
                        Vec3 mouse_world = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                        float threshold = g_EditorState.ortho_cam_zoom[i - 1] * 0.05f;
                        float GIZMO_SIZE = 1.0f;

                        if (i == VIEW_TOP_XZ) {
                            if (fabsf(mouse_world.z - gizmo_target_pos.z) < threshold && mouse_world.x >= gizmo_target_pos.x && mouse_world.x <= gizmo_target_pos.x + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X;
                            else if (fabsf(mouse_world.x - gizmo_target_pos.x) < threshold && mouse_world.z >= gizmo_target_pos.z && mouse_world.z <= gizmo_target_pos.z + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z;
                        }
                        else if (i == VIEW_FRONT_XY) {
                            if (fabsf(mouse_world.y - gizmo_target_pos.y) < threshold && mouse_world.x >= gizmo_target_pos.x && mouse_world.x <= gizmo_target_pos.x + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X;
                            else if (fabsf(mouse_world.x - gizmo_target_pos.x) < threshold && mouse_world.y >= gizmo_target_pos.y && mouse_world.y <= gizmo_target_pos.y + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y;
                        }
                        else if (i == VIEW_SIDE_YZ) {
                            if (fabsf(mouse_world.z - gizmo_target_pos.z) < threshold && mouse_world.y >= gizmo_target_pos.y && mouse_world.y <= gizmo_target_pos.y + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y;
                            else if (fabsf(mouse_world.y - gizmo_target_pos.y) < threshold && mouse_world.z >= gizmo_target_pos.z && mouse_world.z <= gizmo_target_pos.z + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z;
                        }
                        if (g_EditorState.gizmo_hovered_axis != GIZMO_AXIS_NONE) break;
                    }
                }
            }
        }
    }
    for (int i = 0; i < scene->numParticleEmitters; ++i) { ParticleEmitter_Update(&scene->particleEmitters[i], engine->deltaTime); }
    g_EditorState.autosave_timer += engine->unscaledDeltaTime;
    if (g_EditorState.autosave_timer >= 300.0f) {
        if (strcmp(g_EditorState.currentMapPath, "untitled.map") != 0) {
            char autosave_path[256];
            sprintf(autosave_path, "autosaves/_autosave_%s", g_EditorState.currentMapPath);
            Scene_SaveMap(scene, NULL, autosave_path);
        }
        g_EditorState.autosave_timer = 0.0f;
    }
}

static void Editor_ExecutePendingAction(Engine* engine, Scene* scene, Renderer* renderer) {
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
        char* args[] = { "edit" };
        handle_command(1, args);
        break;
    }
    default:
        break;
    }
    g_pending_action = PENDING_ACTION_NONE;
}

void Editor_RenderUI(Engine* engine, Scene* scene, Renderer* renderer) {
    char window_title[512];
    sprintf(window_title, "Tectonic Editor - %s", g_EditorState.currentMapPath);
    SDL_SetWindowTitle(engine->window, window_title);
    static bool show_add_particle_popup = false;
    static char add_particle_path[128] = "particles/fire.par";
    int model_to_delete = -1, brush_to_delete = -1, light_to_delete = -1, decal_to_delete = -1, sound_to_delete = -1, particle_to_delete = -1, video_player_to_delete = -1, parallax_room_to_delete = -1;
    int sprite_to_delete = -1;
    int logic_entity_to_delete = -1;
    float right_panel_width = 300.0f; float screen_w, screen_h;
    UI_GetDisplaySize(&screen_w, &screen_h);
    UI_SetNextWindowPos(screen_w - right_panel_width, 22); UI_SetNextWindowSize(right_panel_width, screen_h * 0.5f);
    UI_Begin("Hierarchy", NULL);
    if (UI_Selectable("Player Start", Editor_IsSelected(ENTITY_PLAYERSTART, 0))) { Editor_ClearSelection(); Editor_AddToSelection(ENTITY_PLAYERSTART, 0, -1, -1); }
    if (UI_CollapsingHeader("Models", 1)) {
        for (int i = 0; i < scene->numObjects; ++i) {
            char label[128];
            const char* name = (strlen(scene->objects[i].targetname) > 0) ? scene->objects[i].targetname : scene->objects[i].modelPath;
            sprintf(label, "%s##%d", name, i);
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_MODEL, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_MODEL, i, -1, -1); }  char popup_id[64];
            sprintf(popup_id, "ModelContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateModel(scene, engine, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { model_to_delete = i; }
                UI_EndPopup();
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##model%d", i); if (UI_Button(del_label)) { model_to_delete = i; }
        }
        if (UI_Button("Add Model")) { g_EditorState.show_add_model_popup = true; }
    }
    if (model_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_MODEL, model_to_delete, "Delete Model"); _raw_delete_model(scene, model_to_delete, engine); Editor_RemoveFromSelection(ENTITY_MODEL, model_to_delete); }
    if (UI_CollapsingHeader("Brushes", 1)) {
        for (int i = 0; i < scene->numBrushes; ++i) {
            char label[128];
            const char* entity_tag = "";
            if (strlen(scene->brushes[i].classname) > 0) {
                entity_tag = "[E]";
            }
            if (strlen(scene->brushes[i].targetname) > 0) {
                sprintf(label, "%s %s##%d", scene->brushes[i].targetname, entity_tag, i);
            }
            else {
                sprintf(label, "Brush %d %s##%d", i, entity_tag, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_BRUSH, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_BRUSH, i, 0, 0); }
            char popup_id[64];
            sprintf(popup_id, "BrushContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateBrush(scene, engine, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { brush_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##brush%d", i); if (UI_Button(del_label)) { brush_to_delete = i; }
        }
    }
    if (brush_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_BRUSH, brush_to_delete, "Delete Brush"); _raw_delete_brush(scene, engine, brush_to_delete); Editor_RemoveFromSelection(ENTITY_BRUSH, brush_to_delete); }
    if (UI_CollapsingHeader("Lights", 1)) {
        for (int i = 0; i < scene->numActiveLights; ++i) {
            char label[128];
            if (strlen(scene->lights[i].targetname) > 0) {
                sprintf(label, "%s##%d", scene->lights[i].targetname, i);
            }
            else {
                sprintf(label, "Light %d##%d", i, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_LIGHT, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_LIGHT, i, -1, -1); } char popup_id[64];
            sprintf(popup_id, "LightContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateLight(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { light_to_delete = i; }
                UI_EndPopup();
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##light%d", i); if (UI_Button(del_label)) { light_to_delete = i; }
        }
        if (UI_Button("Add Light")) { if (scene->numActiveLights < MAX_LIGHTS) { Light* new_light = &scene->lights[scene->numActiveLights]; scene->numActiveLights++; memset(new_light, 0, sizeof(Light));  new_light->custom_style_string[0] = '\0'; sprintf(new_light->targetname, "Light_%d", scene->numActiveLights - 1); new_light->type = LIGHT_POINT; new_light->position = g_EditorState.editor_camera.position; new_light->color = (Vec3){ 1,1,1 }; new_light->intensity = 1.0f; new_light->direction = (Vec3){ 0, -1, 0 }; new_light->shadowFarPlane = 25.0f; new_light->shadowBias = 0.05f; new_light->intensity = 1.0f; new_light->radius = 10.0f; new_light->base_intensity = 1.0f; new_light->is_on = true; Light_InitShadowMap(new_light); Undo_PushCreateEntity(scene, ENTITY_LIGHT, scene->numActiveLights - 1, "Create Light"); } }
    }
    if (light_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_LIGHT, light_to_delete, "Delete Light"); _raw_delete_light(scene, light_to_delete); Editor_RemoveFromSelection(ENTITY_LIGHT, light_to_delete); }
    if (UI_CollapsingHeader("Decals", 1)) {
        for (int i = 0; i < scene->numDecals; ++i) {
            char label[128];
            if (strlen(scene->decals[i].targetname) > 0) {
                sprintf(label, "%s##decal%d", scene->decals[i].targetname, i);
            }
            else {
                sprintf(label, "Decal %d (%s)##decal%d", i, scene->decals[i].material->name, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_DECAL, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_DECAL, i, -1, -1); }  char popup_id[64];
            sprintf(popup_id, "DecalContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateDecal(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { decal_to_delete = i; }
                UI_EndPopup();
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##decal%d", i); if (UI_Button(del_label)) { decal_to_delete = i; }
        }
        if (UI_Button("Add Decal")) { if (scene->numDecals < MAX_DECALS) { Decal* d = &scene->decals[scene->numDecals]; memset(d, 0, sizeof(Decal)); sprintf(d->targetname, "Decal_%d", scene->numDecals); d->pos = g_EditorState.editor_camera.position; d->size = (Vec3){ 1, 1, 1 }; d->material = TextureManager_FindMaterial(TextureManager_GetMaterial(0)->name); 
        d->uv_scale = (Vec2){ 1.0f, 1.0f }; d->uv_offset = (Vec2){ 0.0f, 0.0f }; d->uv_rotation = 0.0f;
        d->lightmap_scale = 1.0f;
        Decal_UpdateMatrix(d); scene->numDecals++; Undo_PushCreateEntity(scene, ENTITY_DECAL, scene->numDecals - 1, "Create Decal"); } }
    }
    if (decal_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_DECAL, decal_to_delete, "Delete Decal"); _raw_delete_decal(scene, decal_to_delete); Editor_RemoveFromSelection(ENTITY_DECAL, decal_to_delete); }
    if (UI_CollapsingHeader("Sounds", 1)) {
        for (int i = 0; i < scene->numSoundEntities; ++i) {
            char label[128];
            if (strlen(scene->soundEntities[i].targetname) > 0) {
                sprintf(label, "%s##sound%d", scene->soundEntities[i].targetname, i);
            }
            else {
                sprintf(label, "Sound %d##sound%d", i, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_SOUND, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_SOUND, i, -1, -1); }  char popup_id[64];
            sprintf(popup_id, "SoundContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateSoundEntity(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { sound_to_delete = i; }
                UI_EndPopup();
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##sound%d", i); if (UI_Button(del_label)) { sound_to_delete = i; }
        }
        if (UI_Button("Add Sound Entity")) {
            g_EditorState.show_sound_browser_popup = true;
            ScanSoundFiles();
        }
    }
    if (sound_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_SOUND, sound_to_delete, "Delete Sound"); _raw_delete_sound_entity(scene, sound_to_delete); Editor_RemoveFromSelection(ENTITY_SOUND, sound_to_delete); }
    if (UI_CollapsingHeader("Particle Emitters", 1)) {
        for (int i = 0; i < scene->numParticleEmitters; ++i) {
            char label[128];
            if (strlen(scene->particleEmitters[i].targetname) > 0) {
                sprintf(label, "%s##particle%d", scene->particleEmitters[i].targetname, i);
            }
            else {
                sprintf(label, "%s##particle%d", scene->particleEmitters[i].parFile, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_PARTICLE_EMITTER, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_PARTICLE_EMITTER, i, -1, -1); }   char popup_id[64];
            sprintf(popup_id, "ParticleContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateParticleEmitter(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { particle_to_delete = i; }
                UI_EndPopup();
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##particle%d", i); if (UI_Button(del_label)) { particle_to_delete = i; }
        }
        if (UI_Button("Add Emitter")) { show_add_particle_popup = true; }
    }
    if (particle_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_PARTICLE_EMITTER, particle_to_delete, "Delete Emitter"); _raw_delete_particle_emitter(scene, particle_to_delete); Editor_RemoveFromSelection(ENTITY_PARTICLE_EMITTER, particle_to_delete); }
    if (UI_CollapsingHeader("Sprites", 1)) {
        for (int i = 0; i < scene->numSprites; ++i) {
            char label[128];
            sprintf(label, "%s##sprite%d", scene->sprites[i].targetname, i);
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_SPRITE, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_SPRITE, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "SpriteContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateSprite(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { sprite_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##sprite%d", i);
            if (UI_Button(del_label)) { sprite_to_delete = i; }
        }
        if (UI_Button("Add Sprite")) {
            if (scene->numSprites < MAX_SPRITES) {
                Sprite* s = &scene->sprites[scene->numSprites];
                memset(s, 0, sizeof(Sprite));
                sprintf(s->targetname, "Sprite_%d", scene->numSprites);
                s->pos = g_EditorState.editor_camera.position;
                s->scale = 1.0f;
                s->material = &g_MissingMaterial;
                s->visible = true;
                scene->numSprites++;
                Undo_PushCreateEntity(scene, ENTITY_SPRITE, scene->numSprites - 1, "Create Sprite");
            }
        }
    }
    if (sprite_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_SPRITE, sprite_to_delete, "Delete Sprite"); _raw_delete_sprite(scene, sprite_to_delete); Editor_RemoveFromSelection(ENTITY_SPRITE, sprite_to_delete); }
    if (UI_CollapsingHeader("Video Players", 1)) {
        for (int i = 0; i < scene->numVideoPlayers; ++i) {
            char label[128];
            if (strlen(scene->videoPlayers[i].targetname) > 0) {
                sprintf(label, "%s##vidplayer%d", scene->videoPlayers[i].targetname, i);
            }
            else {
                sprintf(label, "%s##vidplayer%d", scene->videoPlayers[i].videoPath, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_VIDEO_PLAYER, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_VIDEO_PLAYER, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "VideoContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateVideoPlayer(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { video_player_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##vidplayer%d", i);
            if (UI_Button(del_label)) { video_player_to_delete = i; }
        }
        if (UI_Button("Add Video Player")) {
            if (scene->numVideoPlayers < MAX_VIDEO_PLAYERS) {
                VideoPlayer* vp = &scene->videoPlayers[scene->numVideoPlayers];
                memset(vp, 0, sizeof(VideoPlayer));
                sprintf(vp->targetname, "Video_%d", scene->numVideoPlayers);
                vp->pos = g_EditorState.editor_camera.position;
                vp->size = (Vec2){ 2, 2 };
                scene->numVideoPlayers++;
                Undo_PushCreateEntity(scene, ENTITY_VIDEO_PLAYER, scene->numVideoPlayers - 1, "Create Video Player");
            }
        }
    }
    if (video_player_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_VIDEO_PLAYER, video_player_to_delete, "Delete Video Player"); _raw_delete_video_player(scene, video_player_to_delete); Editor_RemoveFromSelection(ENTITY_VIDEO_PLAYER, video_player_to_delete); }
    if (UI_CollapsingHeader("Parallax Rooms", 1)) {
        for (int i = 0; i < scene->numParallaxRooms; ++i) {
            char label[128];
            if (strlen(scene->parallaxRooms[i].targetname) > 0) {
                sprintf(label, "%s##parallax%d", scene->parallaxRooms[i].targetname, i);
            }
            else {
                sprintf(label, "%s##parallax%d", scene->parallaxRooms[i].cubemapPath, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_PARALLAX_ROOM, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_PARALLAX_ROOM, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "ParallaxContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateParallaxRoom(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { parallax_room_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##parallax%d", i);
            if (UI_Button(del_label)) { parallax_room_to_delete = i; }
        }
        if (UI_Button("Add Parallax Room")) {
            if (scene->numParallaxRooms < MAX_PARALLAX_ROOMS) {
                ParallaxRoom* p = &scene->parallaxRooms[scene->numParallaxRooms];
                memset(p, 0, sizeof(ParallaxRoom));
                sprintf(p->targetname, "Parallax_%d", scene->numParallaxRooms);
                p->pos = g_EditorState.editor_camera.position;
                p->size = (Vec2){ 2, 2 };
                p->roomDepth = 2.0f;
                strcpy(p->cubemapPath, "cubemaps/");
                scene->numParallaxRooms++;
                Undo_PushCreateEntity(scene, ENTITY_PARALLAX_ROOM, scene->numParallaxRooms - 1, "Create Parallax Room");
            }
        }
    }
    if (parallax_room_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_PARALLAX_ROOM, parallax_room_to_delete, "Delete Parallax Room"); _raw_delete_parallax_room(scene, parallax_room_to_delete); Editor_RemoveFromSelection(ENTITY_PARALLAX_ROOM, parallax_room_to_delete); }
    if (UI_CollapsingHeader("Logic Entities", 1)) {
        for (int i = 0; i < scene->numLogicEntities; ++i) {
            char label[128];
            sprintf(label, "%s (%s)##logic%d", scene->logicEntities[i].targetname, scene->logicEntities[i].classname, i);
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_LOGIC, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_LOGIC, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "LogicContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateLogicEntity(scene, engine, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { logic_entity_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##logic%d", i);
            if (UI_Button(del_label)) { logic_entity_to_delete = i; }
        }
        if (UI_Button("Add Logic Entity")) {
            if (scene->numLogicEntities < MAX_LOGIC_ENTITIES) {
                LogicEntity* ent = &scene->logicEntities[scene->numLogicEntities];
                memset(ent, 0, sizeof(LogicEntity));

                int num_logic_classes = 0;
                const char** logic_classes = GameData_GetLogicEntityClassnames(&num_logic_classes);
                if (num_logic_classes > 0) {
                    strcpy(ent->classname, logic_classes[0]);
                    const TGD_EntityDef* def = GameData_FindEntityDef(ent->classname);
                    if (def) {
                        ent->numProperties = def->num_properties;
                        for (int i = 0; i < def->num_properties; ++i) {
                            strcpy(ent->properties[i].key, def->properties[i].key);
                            strcpy(ent->properties[i].value, def->properties[i].default_value);
                        }
                    }
                }
                sprintf(ent->targetname, "%s_%d", ent->classname, scene->numLogicEntities);
                ent->pos = g_EditorState.editor_camera.position;
                scene->numLogicEntities++;
                Undo_PushCreateEntity(scene, ENTITY_LOGIC, scene->numLogicEntities - 1, "Create Logic Entity");
            }
        }
    }
    if (logic_entity_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_LOGIC, logic_entity_to_delete, "Delete Logic Entity"); _raw_delete_logic_entity(scene, logic_entity_to_delete); Editor_RemoveFromSelection(ENTITY_LOGIC, logic_entity_to_delete); }
    if (show_add_particle_popup) { UI_Begin("Add Particle Emitter", &show_add_particle_popup); UI_InputText("Path (.par)", add_particle_path, sizeof(add_particle_path)); if (UI_Button("Create")) { if (scene->numParticleEmitters < MAX_PARTICLE_EMITTERS) { ParticleEmitter* emitter = &scene->particleEmitters[scene->numParticleEmitters]; strcpy(emitter->parFile, add_particle_path); sprintf(emitter->targetname, "Emitter_%d", scene->numParticleEmitters); ParticleSystem* ps = ParticleSystem_Load(emitter->parFile); if (ps) { ParticleEmitter_Init(emitter, ps, g_EditorState.editor_camera.position); scene->numParticleEmitters++; Undo_PushCreateEntity(scene, ENTITY_PARTICLE_EMITTER, scene->numParticleEmitters - 1, "Create Particle Emitter"); } else { Console_Printf_Error("[error] Failed to load particle system: %s", emitter->parFile); } } show_add_particle_popup = false; } UI_End(); }
    UI_End();
    UI_SetNextWindowPos(screen_w - right_panel_width, 22 + screen_h * 0.5f); UI_SetNextWindowSize(right_panel_width, screen_h * 0.5f);
    UI_Begin("Inspector & Settings", NULL);
    EditorSelection* primary = Editor_GetPrimarySelection();
    UI_RadioButton_Int("Translate (1)", (int*)&g_EditorState.current_gizmo_operation, GIZMO_OP_TRANSLATE);
    UI_SameLine();
    UI_RadioButton_Int("Rotate (2)", (int*)&g_EditorState.current_gizmo_operation, GIZMO_OP_ROTATE);
    UI_SameLine();
    UI_RadioButton_Int("Scale (3)", (int*)&g_EditorState.current_gizmo_operation, GIZMO_OP_SCALE);
    UI_Separator();
    UI_Text("Inspector"); UI_Separator();
    if (primary && primary->type == ENTITY_MODEL) {
        SceneObject* obj = &scene->objects[primary->index]; UI_Text(obj->modelPath); UI_Separator();
        UI_InputText("Name", obj->targetname, sizeof(obj->targetname));
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Edit Model Targetname"); }
        if (UI_DragFloat3("Position", &obj->pos.x, 0.1f, 0, 0)) { SceneObject_UpdateMatrix(obj); if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Move Model"); }

        if (UI_DragFloat3("Rotation", &obj->rot.x, 1.0f, 0, 0)) { SceneObject_UpdateMatrix(obj); if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Rotate Model"); }

        if (UI_DragFloat3("Scale", &obj->scale.x, 0.01f, 0, 0)) { SceneObject_UpdateMatrix(obj); if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Scale Model"); }
        UI_Separator();
        UI_Text("Physics Properties");
        UI_DragFloat("Mass", &obj->mass, 0.1f, 0.0f, 1000.0f);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Edit Model Mass"); }
        UI_Text("(Mass 0 = static, >0 = dynamic)");

        if (UI_Checkbox("Physics Enabled", &obj->isPhysicsEnabled)) {
            Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index);
            Physics_ToggleCollision(engine->physicsWorld, obj->physicsBody, obj->isPhysicsEnabled);
            Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Toggle Model Physics");
        }
        if (UI_Checkbox("Casts Shadows", &obj->casts_shadows)) {
            Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index);
            Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Toggle Model Shadows");
        }
        UI_Separator();
        UI_Checkbox("Enable Tree Sway", &obj->swayEnabled);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Toggle Model Sway"); }
        UI_Separator();
        UI_Text("Fading");
        UI_DragFloat("Fade Start", &obj->fadeStartDist, 1.0f, 0.0f, 1000.0f);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Edit Fade Distance"); }

        UI_DragFloat("Fade End", &obj->fadeEndDist, 1.0f, 0.0f, 1000.0f);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Edit Fade Distance"); }

        if (obj->model && obj->model->num_animations > 0) {
            UI_Separator();
            if (UI_CollapsingHeader("Animation", 1)) {
                const char** anim_names = malloc(obj->model->num_animations * sizeof(const char*));
                if (anim_names) {
                    for (int i = 0; i < obj->model->num_animations; ++i) {
                        anim_names[i] = obj->model->animations[i].name;
                    }
                    if (UI_Combo("Clip", &g_EditorState.preview_animation_index, anim_names, obj->model->num_animations, -1)) {
                        g_EditorState.preview_animation_time = 0.0f;
                        g_EditorState.preview_animation_playing = false;
                    }
                    free(anim_names);
                }

                if (g_EditorState.preview_animation_index != -1) {
                    AnimationClip* clip = &obj->model->animations[g_EditorState.preview_animation_index];
                    if (g_EditorState.preview_animation_playing) {
                        if (UI_Button("Pause")) { g_EditorState.preview_animation_playing = false; }
                    }
                    else {
                        if (UI_Button("Play")) { g_EditorState.preview_animation_playing = true; }
                    }
                    UI_SameLine();
                    if (UI_Button("Stop")) {
                        g_EditorState.preview_animation_playing = false;
                        g_EditorState.preview_animation_time = 0.0f;
                    }
                    UI_DragFloat("Time", &g_EditorState.preview_animation_time, 0.01f, 0.0f, clip->duration);
                }
            }
        }
    }
    else if (primary && primary->type == ENTITY_BRUSH) {
        Brush* b = &scene->brushes[primary->index];
        UI_Separator();
        UI_InputText("Name", b->targetname, sizeof(b->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Edit Brush Name"); }
        UI_Separator(); bool transform_changed = false;
        UI_DragFloat3("Position", &b->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->pos.x = SnapValue(b->pos.x, g_EditorState.grid_size); b->pos.y = SnapValue(b->pos.y, g_EditorState.grid_size); b->pos.z = SnapValue(b->pos.z, g_EditorState.grid_size); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Move Brush"); }
        UI_DragFloat3("Rotation", &b->rot.x, 1.0f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->rot.x = SnapAngle(b->rot.x, 15.0f); b->rot.y = SnapAngle(b->rot.y, 15.0f); b->rot.z = SnapAngle(b->rot.z, 15.0f); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Rotate Brush"); }
        UI_DragFloat3("Scale", &b->scale.x, 0.01f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->scale.x = SnapValue(b->scale.x, 0.25f); b->scale.y = SnapValue(b->scale.y, 0.25f); b->scale.z = SnapValue(b->scale.z, 0.25f); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Scale Brush"); }
        if (UI_Checkbox("Use Vertex Lighting", &b->useVertexLighting)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            if (b->useVertexLighting) {
                if (b->lightmapAtlas) { glDeleteTextures(1, &b->lightmapAtlas); b->lightmapAtlas = 0; }
                if (b->directionalLightmapAtlas) { glDeleteTextures(1, &b->directionalLightmapAtlas); b->directionalLightmapAtlas = 0; }
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Toggle Brush Vertex Lighting");
        }
        if (transform_changed) { Brush_UpdateMatrix(b); if (b->physicsBody) { Physics_SetWorldTransform(b->physicsBody, b->modelMatrix); } }
        UI_Separator();
        UI_Text("Physics Properties");
        if (UI_DragFloat("Mass", &b->mass, 0.1f, 0.0f, 10000.0f)) {}
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) {
            if (b->physicsBody) {
                Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                b->physicsBody = NULL;
            }
            if (Brush_IsSolid(b) && b->numVertices > 0) {
                if (b->mass > 0.0f) {
                    b->physicsBody = Physics_CreateDynamicBrush(engine->physicsWorld, (const float*)&b->vertices->pos, b->numVertices, sizeof(BrushVertex), b->mass, b->modelMatrix);
                }
                else {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int i = 0; i < b->numVertices; i++) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Edit Brush Mass");
        }
        UI_Separator();
        UI_Text("Vertex Tools");
        if (UI_Checkbox("Sculpt Mode", &g_EditorState.is_sculpting_mode_enabled)) {
            if (g_EditorState.is_sculpting_mode_enabled) {
                g_EditorState.is_painting_mode_enabled = false;
                g_EditorState.show_vertex_tools_window = true;
            }
            else {
                g_EditorState.show_vertex_tools_window = false;
            }
        }
        UI_SameLine();
        if (UI_Checkbox("Paint Mode", &g_EditorState.is_painting_mode_enabled)) {
            if (g_EditorState.is_painting_mode_enabled) {
                g_EditorState.is_sculpting_mode_enabled = false;
                g_EditorState.show_vertex_tools_window = true;
            }
            else {
                g_EditorState.show_vertex_tools_window = false;
            }
        }
        UI_Separator();
        UI_Text("Brush Entity Class");
        int num_brush_classes = 0;
        const char** brush_classes = GameData_GetBrushEntityClassnames(&num_brush_classes);
        int current_class_idx = 0;
        for (int i = 1; i < num_brush_classes; ++i) {
            if (strcmp(b->classname, brush_classes[i]) == 0) {
                current_class_idx = i;
                break;
            }
        }

        if (UI_Combo("Classname", &current_class_idx, brush_classes, num_brush_classes, -1)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            if (current_class_idx == 0) {
                b->classname[0] = '\0';
                b->numProperties = 0;
            }
            else {
                strcpy(b->classname, brush_classes[current_class_idx]);
                if (strcmp(b->classname, "env_reflectionprobe") == 0) {
                    int x = (int)roundf(b->pos.x);
                    int y = (int)roundf(b->pos.y);
                    int z = (int)roundf(b->pos.z);

                    char name_buf[128];
                    snprintf(name_buf, sizeof(name_buf), "Probe_%s%d_%s%d_%s%d",
                        (x < 0 ? "n" : ""), abs(x),
                        (y < 0 ? "n" : ""), abs(y),
                        (z < 0 ? "n" : ""), abs(z));

                    strncpy(b->name, name_buf, sizeof(b->name) - 1);
                    strncpy(b->targetname, b->name, sizeof(b->targetname) - 1);
                }
                const TGD_EntityDef* def = GameData_FindEntityDef(b->classname);
                if (def) {
                    b->numProperties = def->num_properties;
                    for (int k = 0; k < def->num_properties; ++k) {
                        strcpy(b->properties[k].key, def->properties[k].key);
                        strcpy(b->properties[k].value, def->properties[k].default_value);
                    }
                }
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Change Brush Class");
        }

        const TGD_EntityDef* brush_def = GameData_FindEntityDef(b->classname);
        if (brush_def) {
            UI_Separator();
            UI_Text("Properties");

            const char** target_names = NULL;
            int num_targets = 0;
            for (int k = 0; k < scene->numObjects; ++k) if (strlen(scene->objects[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->objects[k].targetname; }
            for (int k = 0; k < scene->numBrushes; ++k) if (strlen(scene->brushes[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->brushes[k].targetname; }
            for (int k = 0; k < scene->numActiveLights; ++k) if (strlen(scene->lights[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->lights[k].targetname; }
            for (int k = 0; k < scene->numSoundEntities; ++k) if (strlen(scene->soundEntities[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->soundEntities[k].targetname; }
            for (int k = 0; k < scene->numParticleEmitters; ++k) if (strlen(scene->particleEmitters[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->particleEmitters[k].targetname; }
            for (int k = 0; k < scene->numVideoPlayers; ++k) if (strlen(scene->videoPlayers[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->videoPlayers[k].targetname; }
            for (int k = 0; k < scene->numSprites; ++k) if (strlen(scene->sprites[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->sprites[k].targetname; }
            for (int k = 0; k < scene->numLogicEntities; ++k) if (strlen(scene->logicEntities[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->logicEntities[k].targetname; }

            for (int i = 0; i < brush_def->num_properties; ++i) {
                const TGD_Property* prop = &brush_def->properties[i];
                if (i >= b->numProperties) continue;

                UI_PushID(i);
                switch (prop->type) {
                case TGD_PROP_CHECKBOX: {
                    bool is_checked = (atoi(b->properties[i].value) != 0);
                    if (UI_Checkbox(prop->display_name, &is_checked)) {
                        strcpy(b->properties[i].value, is_checked ? "1" : "0");
                    }
                    break;
                }
                case TGD_PROP_CHOICES: {
                    const char** display_items = (const char**)malloc(prop->num_choices * sizeof(const char*));
                    int current_item = -1;
                    for (int j = 0; j < prop->num_choices; ++j) {
                        display_items[j] = prop->choices[j].display_name;
                        if (strcmp(b->properties[i].value, prop->choices[j].value) == 0) {
                            current_item = j;
                        }
                    }
                    if (UI_Combo(prop->display_name, &current_item, display_items, prop->num_choices, -1)) {
                        if (current_item >= 0) {
                            strcpy(b->properties[i].value, prop->choices[current_item].value);
                        }
                    }
                    free(display_items);
                    break;
                }
                case TGD_PROP_TEXTURE: {
                    char button_label[256];
                    snprintf(button_label, sizeof(button_label), "%s: %s", prop->display_name, b->properties[i].value);
                    if (UI_Button(button_label)) {
                        g_EditorState.texture_browser_target = 100 + i;
                        g_EditorState.show_texture_browser = true;
                    }
                    break;
                }
                case TGD_PROP_ENTITIES: {
                    int current_item = -1;
                    for (int k = 0; k < num_targets; ++k) {
                        if (strcmp(b->properties[i].value, target_names[k]) == 0) {
                            current_item = k;
                            break;
                        }
                    }
                    if (UI_Combo(prop->display_name, &current_item, target_names, num_targets, -1)) {
                        if (current_item >= 0) {
                            strncpy(b->properties[i].value, target_names[current_item], sizeof(b->properties[i].value) - 1);
                        }
                    }
                    break;
                }
                default:
                    UI_InputText(prop->display_name, b->properties[i].value, sizeof(b->properties[i].value));
                    break;
                }
                if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); }
                if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Edit Brush Property"); }
                UI_PopID();
            }
            if (target_names) free(target_names);
            RenderIOEditor(ENTITY_BRUSH, primary->index);
        }
        else {
            UI_Separator();
            UI_Text("Vertex Properties"); UI_DragInt("Selected Vertex", &primary->vertex_index, 1, 0, b->numVertices - 1);
            if (primary->vertex_index >= 0 && primary->vertex_index < b->numVertices) {
                BrushVertex* vert = &b->vertices[primary->vertex_index];

                UI_DragFloat3("Local Position", &vert->pos.x, 0.1f, 0, 0);
                if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); }
                if (UI_IsItemDeactivatedAfterEdit()) {
                    Brush_CreateRenderData(b);
                    if (b->physicsBody) {
                        Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                        if (Brush_IsSolid(b) && b->numVertices > 0) {
                            Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                            for (int i = 0; i < b->numVertices; ++i) { world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos); }
                            b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                            free(world_verts);
                        }
                        else {
                            b->physicsBody = NULL;
                        }
                    }
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Edit Brush Vertex");
                }

                if (UI_IsItemActivated()) {
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                }
                if (UI_IsItemDeactivatedAfterEdit()) {
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Paint Vertex Color");
                }
            }
        }
    }
    else if (primary && primary->type == ENTITY_PLAYERSTART) {
        UI_Text("Player Start"); UI_Separator(); UI_DragFloat3("Position", &scene->playerStart.position.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PLAYERSTART, 0); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { scene->playerStart.position.x = SnapValue(scene->playerStart.position.x, g_EditorState.grid_size); scene->playerStart.position.y = SnapValue(scene->playerStart.position.y, g_EditorState.grid_size); scene->playerStart.position.z = SnapValue(scene->playerStart.position.z, g_EditorState.grid_size); } Undo_EndEntityModification(scene, ENTITY_PLAYERSTART, 0, "Move Player Start"); }
    }
    else if (primary && primary->type == ENTITY_SPRITE) {
        Sprite* s = &scene->sprites[primary->index];
        UI_Text("Sprite Properties");
        UI_Separator();
        UI_InputText("Name", s->targetname, sizeof(s->targetname));
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SPRITE, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SPRITE, primary->index, "Edit Sprite Name"); }

        UI_DragFloat3("Position", &s->pos.x, 0.1f, 0, 0);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SPRITE, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SPRITE, primary->index, "Move Sprite"); }

        UI_DragFloat("Scale", &s->scale, 0.05f, 0.01f, 100.0f);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SPRITE, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SPRITE, primary->index, "Scale Sprite"); }

        char mat_button_label[128];
        sprintf(mat_button_label, "Material: %s", s->material ? s->material->name : "None");
        if (UI_Button(mat_button_label)) {
            g_EditorState.texture_browser_target = 6;
            g_EditorState.show_texture_browser = true;
        }
    }
    else if (primary && primary->type == ENTITY_LIGHT) {
        Light* light = &scene->lights[primary->index];

        UI_InputText("Name", light->targetname, sizeof(light->targetname));
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Name");
        }

        if (UI_RadioButton("Point", light->type == LIGHT_POINT)) {
            if (light->type != LIGHT_POINT) {
                Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                Light_DestroyShadowMap(light);
                light->type = LIGHT_POINT;
                Light_InitShadowMap(light);
                Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Type");
            }
        }
        UI_SameLine();
        if (UI_RadioButton("Spot", light->type == LIGHT_SPOT)) {
            if (light->type != LIGHT_SPOT) {
                Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                Light_DestroyShadowMap(light);
                light->type = LIGHT_SPOT;
                if (light->cutOff <= 0.0f) {
                    light->cutOff = cosf(12.5f * M_PI / 180.0f);
                    light->outerCutOff = cosf(17.5f * M_PI / 180.0f);
                }
                Light_InitShadowMap(light);
                Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Type");
            }
        }
        UI_SameLine();
        if (UI_RadioButton("Area (Baked Only)", light->type == LIGHT_AREA)) {
            if (light->type != LIGHT_AREA) {
                Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                Light_DestroyShadowMap(light);
                light->type = LIGHT_AREA;
                light->is_static = true;
                Light_InitShadowMap(light);
                Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Type");
            }
        }

        UI_Separator();

        UI_DragFloat3("Position", &light->position.x, 0.1f, 0, 0);
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            if (g_EditorState.snap_to_grid) {
                light->position.x = SnapValue(light->position.x, g_EditorState.grid_size);
                light->position.y = SnapValue(light->position.y, g_EditorState.grid_size);
                light->position.z = SnapValue(light->position.z, g_EditorState.grid_size);
            }
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Move Light");
        }

        if (light->type == LIGHT_SPOT || light->type == LIGHT_AREA) {
            UI_DragFloat3("Rotation", &light->rot.x, 1.0f, -360.0f, 360.0f);
            if (UI_IsItemActivated()) {
                Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
            }
            if (UI_IsItemDeactivatedAfterEdit()) {
                if (g_EditorState.snap_to_grid) {
                    light->rot.x = SnapAngle(light->rot.x, 15.0f);
                    light->rot.y = SnapAngle(light->rot.y, 15.0f);
                    light->rot.z = SnapAngle(light->rot.z, 15.0f);
                }
                Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Rotate Light");
            }
        }

        UI_ColorEdit3("Color", &light->color.x);
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Color");
        }

        UI_DragFloat("Intensity", &light->base_intensity, 0.05f, 0.0f, 1000.0f);
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Intensity");
        }

        if (light->type == LIGHT_AREA) {
            UI_DragFloat("Width", &light->width, 0.1f, 0.1f, 1000.0f);
            if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); }
            if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Width"); }
            UI_DragFloat("Height", &light->height, 0.1f, 0.1f, 1000.0f);
            if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); }
            if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Height"); }
        }

        UI_DragFloat("Radius", &light->radius, 0.1f, 0.1f, 1000.0f);
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Radius");
        }

        UI_DragFloat("Volumetric Intensity", &light->volumetricIntensity, 0.05f, 0.0f, 10.0f);
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Volumetric Intensity");
        }

        UI_Separator();

        const char* preset_names[] = {
            "0: Normal", "1: Flicker 1", "2: Slow Strong Pulse", "3: Candle 1",
            "4: Fast Strobe", "5: Gentle Pulse", "6: Flicker 2", "7: Candle 2",
            "8: Candle 3", "9: Slow Strobe", "10: Fluorescent", "11: Slow Pulse 2",
            "12: Underwater", "13: Custom"
        };

        int temp_preset = light->preset;
        if (UI_Combo("Preset", &temp_preset, preset_names, 14, 14)) {
            if (temp_preset != light->preset) {
                Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                light->preset = temp_preset;
                Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Preset");
            }
        }

        if (light->preset == 13) {
            UI_InputText("Custom Style", light->custom_style_string, sizeof(light->custom_style_string));
            if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); }
            if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Custom Light Style"); }
        }
        if (light->type == LIGHT_SPOT) {
            char cookie_button_label[128];
            const char* cookie_name = strlen(light->cookiePath) > 0 ? light->cookiePath : "None";
            sprintf(cookie_button_label, "Cookie: %s", cookie_name);
            if (UI_Button(cookie_button_label)) {
                g_EditorState.texture_browser_target = 4;
                g_EditorState.show_texture_browser = true;
            }
            if (strlen(light->cookiePath) > 0) {
                UI_SameLine();
                if (UI_Button("[X]##clearcookie")) {
                    Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                    if (light->cookieMapHandle != 0) {
                        glMakeTextureHandleNonResidentARB(light->cookieMapHandle);
                    }
                    light->cookiePath[0] = '\0';
                    light->cookieMap = 0;
                    light->cookieMapHandle = 0;
                    Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Clear Light Cookie");
                }
            }
        }
        if (UI_Checkbox("On by default", &light->is_on)) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Toggle Light On"); }
        UI_SameLine();
        if (UI_Checkbox("Static", &light->is_static)) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Toggle Light Static");
        }
        if (UI_Checkbox("Static Shadow Map", &light->is_static_shadow)) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
            light->has_rendered_static_shadow = false;
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Toggle Static Shadow");
        }
        UI_Separator(); if (light->type == LIGHT_SPOT) { UI_DragFloat("CutOff (cos)", &light->cutOff, 0.005f, 0.0f, 1.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Cutoff"); } UI_DragFloat("OuterCutOff (cos)", &light->outerCutOff, 0.005f, 0.0f, 1.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Cutoff"); } UI_Separator(); } UI_Text("Shadow Properties"); UI_DragFloat("Far Plane", &light->shadowFarPlane, 0.5f, 1.0f, 200.0f); UI_DragFloat("Bias", &light->shadowBias, 0.001f, 0.0f, 0.5f);
    }
    else if (primary && primary->type == ENTITY_DECAL) {
        Decal* d = &scene->decals[primary->index];
        UI_Text("Decal Properties");
        UI_InputText("Name", d->targetname, sizeof(d->targetname));
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal Name"); }
        UI_Separator();

        char decal_mat_button_label[128];
        const char* mat_name = d->material ? d->material->name : "___MISSING___";
        sprintf(decal_mat_button_label, "Material: %s", mat_name);
        if (UI_Button(decal_mat_button_label)) {
            g_EditorState.texture_browser_target = 5;
            g_EditorState.show_texture_browser = true;
        }

        UI_Separator();
        bool transform_changed = false;
        if (UI_DragFloat3("Position", &d->pos.x, 0.1f, 0, 0)) { transform_changed = true; }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Move Decal"); }

        if (UI_DragFloat3("Rotation", &d->rot.x, 1.0f, 0, 0)) { transform_changed = true; }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Rotate Decal"); }

        if (UI_DragFloat3("Size", &d->size.x, 0.05f, 0, 0)) { transform_changed = true; }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Scale Decal"); }

        UI_Separator();
        if (UI_DragFloat("Lightmap Scale", &d->lightmap_scale, 0.125f, 0.125f, 16.0f)) {}
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal Lightmap Scale"); }

        UI_Separator();
        UI_Text("Texture Mapping");

        UI_Text("Scale"); UI_SameLine(); UI_SetNextItemWidth(80);
        if (UI_InputFloat("X##DScale", &d->uv_scale.x, 0.01f, 0.1f, "%.2f")) {}
        if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
        if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal UV Scale");
        UI_SameLine(); UI_SetNextItemWidth(80);
        if (UI_InputFloat("Y##DScale", &d->uv_scale.y, 0.01f, 0.1f, "%.2f")) {}
        if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
        if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal UV Scale");

        UI_Text("Shift"); UI_SameLine(); UI_SetNextItemWidth(80);
        if (UI_InputFloat("X##DShift", &d->uv_offset.x, 0.1f, 1.0f, "%.2f")) {}
        if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
        if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal UV Shift");
        UI_SameLine(); UI_SetNextItemWidth(80);
        if (UI_InputFloat("Y##DShift", &d->uv_offset.y, 0.1f, 1.0f, "%.2f")) {}
        if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
        if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal UV Shift");

        UI_Text("Rotation"); UI_SameLine(); UI_SetNextItemWidth(172);
        if (UI_DragFloat("##DRotation", &d->uv_rotation, 1.0f, -360.0f, 360.0f)) {}
        if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
        if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal UV Rotation");

        if (transform_changed) {
            Decal_UpdateMatrix(d);
        }
    }
    else if (primary && primary->type == ENTITY_SOUND) {
        SoundEntity* s = &scene->soundEntities[primary->index]; UI_Text("Sound Entity Properties"); UI_Separator();
        UI_InputText("Name", s->targetname, sizeof(s->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Edit Sound Name"); } UI_InputText("Sound Path", s->soundPath, sizeof(s->soundPath)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Edit Sound Path"); } if (UI_Button("Load##Sound")) {
            if (s->sourceID != 0) SoundSystem_DeleteSource(s->sourceID); if (s->bufferID != 0) SoundSystem_DeleteBuffer(s->bufferID);  s->bufferID = SoundSystem_LoadSound(s->soundPath);
        } UI_DragFloat3("Position", &s->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourcePosition(s->sourceID, s->pos); Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Move Sound"); } UI_DragFloat("Volume", &s->volume, 0.05f, 0.0f, 2.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourceProperties(s->sourceID, s->volume, s->pitch, s->maxDistance); Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Edit Sound Volume"); } UI_DragFloat("Pitch", &s->pitch, 0.05f, 0.1f, 4.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourceProperties(s->sourceID, s->volume, s->pitch, s->maxDistance); Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Edit Sound Pitch"); } UI_DragFloat("Max Distance", &s->maxDistance, 1.0f, 1.0f, 1000.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourceProperties(s->sourceID, s->volume, s->pitch, s->maxDistance); Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Edit Sound Distance"); }
        if (UI_Checkbox("Looping", &s->is_looping)) {
            Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index);
            if (s->sourceID != 0) SoundSystem_SetSourceLooping(s->sourceID, s->is_looping);
            Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Toggle Sound Loop");
        }
        if (UI_Checkbox("Global Sound", &s->isGlobal)) {
            Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index);
            SoundSystem_SetSourceIsGlobal(s->sourceID, s->isGlobal);
            if (!s->isGlobal) {
                SoundSystem_SetSourcePosition(s->sourceID, s->pos);
            }
            Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Toggle Sound Global");
        }
        if (UI_Checkbox("Play on Start", &s->play_on_start)) {
            Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index);
            Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Toggle Play on Start");
        }
    }
    else if (primary && primary->type == ENTITY_PARTICLE_EMITTER) {
        ParticleEmitter* emitter = &scene->particleEmitters[primary->index]; UI_Text("Particle Emitter: %s", emitter->parFile); UI_Separator(); UI_DragFloat3("Position", &emitter->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index, "Move Emitter"); }
        UI_InputText("Name", emitter->targetname, sizeof(emitter->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index, "Edit Emitter Name"); } if (UI_Checkbox("On by default", &emitter->on_by_default)) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index); emitter->is_on = emitter->on_by_default; Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index, "Toggle Emitter On"); } if (UI_Button("Reload .par File")) { ParticleSystem_Free(emitter->system); ParticleSystem* ps = ParticleSystem_Load(emitter->parFile); if (ps) { ParticleEmitter_Init(emitter, ps, emitter->pos); } else { Console_Printf_Error("[error] Failed to reload particle system: %s", emitter->parFile); emitter->system = NULL; } }
    }
    else if (primary && primary->type == ENTITY_VIDEO_PLAYER) {
        VideoPlayer* vp = &scene->videoPlayers[primary->index];
        char oldPath[sizeof(vp->videoPath)];
        memcpy(oldPath, vp->videoPath, sizeof(vp->videoPath));

        UI_Text("Video Player Properties");
        UI_Separator();

        UI_InputText("Video Path", vp->videoPath, sizeof(vp->videoPath));
        if (strcmp(oldPath, vp->videoPath) != 0) {
            VideoPlayer_Load(vp);
        }
        UI_InputText("Name", vp->targetname, sizeof(vp->targetname));
        UI_Checkbox("Play on Start", &vp->playOnStart);
        UI_Checkbox("Loop", &vp->loop);

        UI_DragFloat3("Position", &vp->pos.x, 0.1f, 0, 0);
        UI_DragFloat3("Rotation", &vp->rot.x, 1.0f, 0, 0);
        UI_DragFloat2("Size", &vp->size.x, 0.05f, 0, 0);

        if (UI_Button("Play")) { VideoPlayer_Play(vp); }
        UI_SameLine();
        if (UI_Button("Stop")) { VideoPlayer_Stop(vp); }
        UI_SameLine();
        if (UI_Button("Restart")) { VideoPlayer_Restart(vp); }
    }
    else if (primary && primary->type == ENTITY_PARALLAX_ROOM) {
        ParallaxRoom* p = &scene->parallaxRooms[primary->index];
        UI_Text("Parallax Room Properties");
        UI_Separator();
        UI_InputText("Name", p->targetname, sizeof(p->targetname));
        UI_InputText("Cubemap Path Base", p->cubemapPath, sizeof(p->cubemapPath));
        if (UI_Button("Reload Cubemap")) {
            if (p->cubemapTexture) glDeleteTextures(1, &p->cubemapTexture);
            const char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
            char face_paths[6][256];
            const char* face_pointers[6];
            for (int i = 0; i < 6; ++i) {
                sprintf(face_paths[i], "%s%s", p->cubemapPath, suffixes[i]);
                face_pointers[i] = face_paths[i];
            }
            p->cubemapTexture = loadCubemap(face_pointers);
        }

        UI_DragFloat3("Position", &p->pos.x, 0.1f, 0, 0);
        UI_DragFloat3("Rotation", &p->rot.x, 1.0f, 0, 0);
        UI_DragFloat2("Size", &p->size.x, 0.05f, 0, 0);
        UI_DragFloat("Room Depth", &p->roomDepth, 0.1f, 0.1f, 100.0f);
        ParallaxRoom_UpdateMatrix(p);
    }
    else if (primary && primary->type == ENTITY_LOGIC) {
        LogicEntity* ent = &scene->logicEntities[primary->index];
        UI_Text("Logic Entity Properties");
        int num_logic_classes = 0;
        const char** logic_classes = GameData_GetLogicEntityClassnames(&num_logic_classes);
        int current_class_index = -1;
        for (int i = 0; i < num_logic_classes; ++i) {
            if (strcmp(ent->classname, logic_classes[i]) == 0) {
                current_class_index = i;
                break;
            }
        }
        if (UI_Combo("Classname", &current_class_index, logic_classes, num_logic_classes, -1)) {
            if (current_class_index >= 0) {
                Undo_BeginEntityModification(scene, ENTITY_LOGIC, primary->index);
                strcpy(ent->classname, logic_classes[current_class_index]);
                const TGD_EntityDef* def = GameData_FindEntityDef(ent->classname);
                if (def) {
                    ent->numProperties = def->num_properties;
                    for (int k = 0; k < def->num_properties; ++k) {
                        strcpy(ent->properties[k].key, def->properties[k].key);
                        strcpy(ent->properties[k].value, def->properties[k].default_value);
                    }
                }
                else {
                    ent->numProperties = 0;
                }
                Undo_EndEntityModification(scene, ENTITY_LOGIC, primary->index, "Change Logic Class");
            }
        }
        UI_InputText("Targetname", ent->targetname, sizeof(ent->targetname));
        if (UI_DragFloat3("Position", &ent->pos.x, 0.1f, 0, 0)) {}
        if (UI_DragFloat3("Rotation", &ent->rot.x, 1.0f, 0, 0)) {}

        UI_Separator();
        const TGD_EntityDef* def = GameData_FindEntityDef(ent->classname);
        if (def) {
            UI_Text("Properties");

            const char** target_names = NULL;
            int num_targets = 0;
            for (int k = 0; k < scene->numObjects; ++k) if (strlen(scene->objects[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->objects[k].targetname; }
            for (int k = 0; k < scene->numBrushes; ++k) if (strlen(scene->brushes[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->brushes[k].targetname; }
            for (int k = 0; k < scene->numActiveLights; ++k) if (strlen(scene->lights[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->lights[k].targetname; }
            for (int k = 0; k < scene->numSoundEntities; ++k) if (strlen(scene->soundEntities[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->soundEntities[k].targetname; }
            for (int k = 0; k < scene->numParticleEmitters; ++k) if (strlen(scene->particleEmitters[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->particleEmitters[k].targetname; }
            for (int k = 0; k < scene->numVideoPlayers; ++k) if (strlen(scene->videoPlayers[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->videoPlayers[k].targetname; }
            for (int k = 0; k < scene->numSprites; ++k) if (strlen(scene->sprites[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->sprites[k].targetname; }
            for (int k = 0; k < scene->numLogicEntities; ++k) if (strlen(scene->logicEntities[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->logicEntities[k].targetname; }

            for (int i = 0; i < ent->numProperties; ++i) {
                const TGD_Property* prop = &def->properties[i];
                UI_PushID(i);
                switch (prop->type) {
                case TGD_PROP_CHECKBOX: {
                    bool is_checked = (atoi(ent->properties[i].value) != 0);
                    if (UI_Checkbox(prop->display_name, &is_checked)) {
                        strcpy(ent->properties[i].value, is_checked ? "1" : "0");
                    }
                    break;
                }
                case TGD_PROP_COLOR: {
                    Vec3 color;
                    sscanf(ent->properties[i].value, "%f %f %f", &color.x, &color.y, &color.z);
                    if (UI_ColorEdit3(prop->display_name, &color.x)) {
                        sprintf(ent->properties[i].value, "%.3f %.3f %.3f", color.x, color.y, color.z);
                    }
                    break;
                }
                case TGD_PROP_CHOICES: {
                    const char** display_items = (const char**)malloc(prop->num_choices * sizeof(const char*));
                    int current_item = -1;
                    for (int j = 0; j < prop->num_choices; ++j) {
                        display_items[j] = prop->choices[j].display_name;
                        if (strcmp(ent->properties[i].value, prop->choices[j].value) == 0) {
                            current_item = j;
                        }
                    }
                    if (UI_Combo(prop->display_name, &current_item, display_items, prop->num_choices, -1)) {
                        if (current_item >= 0) {
                            strcpy(ent->properties[i].value, prop->choices[current_item].value);
                        }
                    }
                    free(display_items);
                    break;
                }
                case TGD_PROP_TEXTURE: {
                    char button_label[256];
                    snprintf(button_label, sizeof(button_label), "%s: %s", prop->display_name, ent->properties[i].value);
                    if (UI_Button(button_label)) {
                        g_EditorState.texture_browser_target = 200 + i;
                        g_EditorState.show_texture_browser = true;
                    }
                    break;
                }
                case TGD_PROP_ENTITIES: {
                    int current_item = -1;
                    for (int k = 0; k < num_targets; ++k) {
                        if (strcmp(ent->properties[i].value, target_names[k]) == 0) {
                            current_item = k;
                            break;
                        }
                    }
                    if (UI_Combo(prop->display_name, &current_item, target_names, num_targets, -1)) {
                        if (current_item >= 0) {
                            strncpy(ent->properties[i].value, target_names[current_item], sizeof(ent->properties[i].value) - 1);
                        }
                    }
                    break;
                }
                default:
                    UI_InputText(prop->display_name, ent->properties[i].value, sizeof(ent->properties[i].value));
                    break;
                }
                if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LOGIC, primary->index); }
                if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LOGIC, primary->index, "Edit Logic Property"); }
                UI_PopID();
            }
            if (target_names) free(target_names);
            RenderIOEditor(ENTITY_LOGIC, primary->index);
        }
    }
    UI_Separator(); UI_Text("Scene Settings"); UI_Separator();
    if (UI_CollapsingHeader("Sun", 1)) {
        UI_Checkbox("Enabled##Sun", &scene->sun.enabled);
        UI_ColorEdit3("Color##Sun", &scene->sun.color.x);
        UI_DragFloat("Intensity##Sun", &scene->sun.intensity, 0.05f, 0.0f, 100.0f);
        UI_DragFloat("Volumetric Intensity##Sun", &scene->sun.volumetricIntensity, 0.05f, 0.0f, 10.0f);
        UI_DragFloat3("Direction##Sun", &scene->sun.direction.x, 0.01f, -1.0f, 1.0f);

        UI_Separator();
        UI_Text("Wind");
        UI_DragFloat3("Wind Direction", &scene->sun.windDirection.x, 0.01f, -1.0f, 1.0f);
        UI_DragFloat("Wind Strength", &scene->sun.windStrength, 0.05f, 0.0f, 10.0f);
    }
    if (UI_CollapsingHeader("Skybox", 1)) {
        UI_Checkbox("Use Cubemap Skybox", &scene->use_cubemap_skybox);
        if (scene->use_cubemap_skybox) {
            UI_InputText("Cubemap Name", scene->skybox_path, sizeof(scene->skybox_path));
            if (UI_Button("Reload Skybox")) {
                if (glIsTexture(scene->skybox_cubemap)) {
                    glDeleteTextures(1, &scene->skybox_cubemap);
                }
                const char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
                char face_paths[6][256];
                const char* face_pointers[6];
                for (int i = 0; i < 6; ++i) {
                    sprintf(face_paths[i], "skybox/%s%s", scene->skybox_path, suffixes[i]);
                    face_pointers[i] = face_paths[i];
                }
                scene->skybox_cubemap = loadCubemap(face_pointers);
            }
        }
    }
    if (UI_CollapsingHeader("Post-Processing", 1)) {
        if (UI_Checkbox("Enabled", &scene->post.enabled)) {} UI_Separator(); UI_Text("CRT & Vignette"); UI_DragFloat("CRT Curvature", &scene->post.crtCurvature, 0.01f, 0.0f, 1.0f); UI_DragFloat("Vignette Strength", &scene->post.vignetteStrength, 0.01f, 0.0f, 2.0f); UI_DragFloat("Vignette Radius", &scene->post.vignetteRadius, 0.01f, 0.0f, 2.0f); UI_Separator(); UI_Text("Effects"); if (UI_Checkbox("Lens Flare", &scene->post.lensFlareEnabled)) {} UI_DragFloat("Flare Strength", &scene->post.lensFlareStrength, 0.05f, 0.0f, 5.0f); UI_DragFloat("Scanline Strength", &scene->post.scanlineStrength, 0.01f, 0.0f, 1.0f); UI_DragFloat("Film Grain", &scene->post.grainIntensity, 0.005f, 0.0f, 0.5f); UI_Separator();
        UI_Separator();
        UI_Checkbox("Sharpening", &scene->post.sharpenEnabled);
        if (scene->post.sharpenEnabled)
        {
            UI_DragFloat("Sharpen Strength", &scene->post.sharpenAmount, 0.01f, 0.0f, 1.0f);
        }
        UI_Separator();
        if (UI_Checkbox("Chromatic Aberration", &scene->post.chromaticAberrationEnabled)) {}
        if (scene->post.chromaticAberrationEnabled) {
            UI_DragFloat("CA Strength", &scene->post.chromaticAberrationStrength, 0.0001f, 0.0f, 0.05f);
        }
        UI_Separator();
        if (UI_Checkbox("Black & White", &scene->post.bwEnabled)) {}
        if (scene->post.bwEnabled) {
            UI_DragFloat("Black & White Strength", &scene->post.bwStrength, 0.0001f, 0.0f, 0.05f);
        }
        UI_Separator();
        if (UI_Checkbox("Invert", &scene->post.invertEnabled)) {}
        if (scene->post.invertEnabled) {
            UI_DragFloat("Invert Strength", &scene->post.invertStrength, 0.01f, 0.0f, 1.0f);
        }
        UI_Separator();
        UI_Text("Depth of Field"); if (UI_Checkbox("Enabled##DOF", &scene->post.dofEnabled)) {} UI_DragFloat("Focus Distance", &scene->post.dofFocusDistance, 0.005f, 0.0f, 1.0f); UI_DragFloat("Aperture", &scene->post.dofAperture, 0.5f, 0.0f, 200.0f);
    }
    if (UI_CollapsingHeader("Color Correction", 1)) {
        UI_Checkbox("Enabled##ColorCorrection", &scene->colorCorrection.enabled);
        UI_InputText("LUT Path", scene->colorCorrection.lutPath, sizeof(scene->colorCorrection.lutPath));
        UI_SameLine();
        if (UI_Button("Reload")) {
            if (scene->colorCorrection.lutTexture) {
                glDeleteTextures(1, &scene->colorCorrection.lutTexture);
            }
            scene->colorCorrection.lutTexture = loadTexture(scene->colorCorrection.lutPath, false, TEXTURE_LOAD_CONTEXT_WORLD);
        }
        if (scene->colorCorrection.lutTexture) {
            UI_Image((void*)(intptr_t)scene->colorCorrection.lutTexture, 256, 16);
        }
    }
    UI_Separator();
    UI_Text("Creation Tools");
    UI_Separator();
    if (UI_RadioButton("Block", g_EditorState.current_brush_shape == BRUSH_SHAPE_BLOCK)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_BLOCK; }
    UI_SameLine();
    if (UI_RadioButton("Cylinder", g_EditorState.current_brush_shape == BRUSH_SHAPE_CYLINDER)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_CYLINDER; }
    if (UI_RadioButton("Tube", g_EditorState.current_brush_shape == BRUSH_SHAPE_TUBE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_TUBE; }
    UI_SameLine();
    if (UI_RadioButton("Wedge", g_EditorState.current_brush_shape == BRUSH_SHAPE_WEDGE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_WEDGE; }
    UI_SameLine();
    if (UI_RadioButton("Spike", g_EditorState.current_brush_shape == BRUSH_SHAPE_SPIKE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_SPIKE; }
    if (UI_RadioButton("Sphere", g_EditorState.current_brush_shape == BRUSH_SHAPE_SPHERE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_SPHERE; }
    UI_SameLine();
    if (UI_RadioButton("Semi-Sphere", g_EditorState.current_brush_shape == BRUSH_SHAPE_SEMI_SPHERE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_SEMI_SPHERE; }
    UI_SameLine();
    if (UI_RadioButton("Arch", g_EditorState.current_brush_shape == BRUSH_SHAPE_ARCH)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_ARCH; }
    if (g_EditorState.current_brush_shape == BRUSH_SHAPE_CYLINDER || g_EditorState.current_brush_shape == BRUSH_SHAPE_TUBE || g_EditorState.current_brush_shape == BRUSH_SHAPE_SPIKE || g_EditorState.current_brush_shape == BRUSH_SHAPE_SPHERE || g_EditorState.current_brush_shape == BRUSH_SHAPE_SEMI_SPHERE) {
        UI_DragInt("Sides", &g_EditorState.cylinder_creation_steps, 1, 4, 64);
    }
    if (g_EditorState.current_brush_shape == BRUSH_SHAPE_TUBE) {
        UI_DragFloat("Wall Thickness", &g_EditorState.tube_wall_thickness, 0.05f, 0.1f, 16.0f);
    }
    UI_Separator(); UI_Text("Editor Settings"); UI_Separator(); if (UI_Button(g_EditorState.snap_to_grid ? "Sapping: ON" : "Snapping: OFF")) { g_EditorState.snap_to_grid = !g_EditorState.snap_to_grid; } UI_SameLine(); UI_DragFloat("Grid Size", &g_EditorState.grid_size, 0.015625f, 0.015625f, 64.0f);
    bool is_unlit = Cvar_GetInt("r_fullbright");
    if (UI_Checkbox("Unlit Mode", &is_unlit)) {
        Cvar_Set("r_fullbright", is_unlit ? "1" : "0");
    }
    for (int i = 0; i < 5; i++) {
        UI_Spacing();
    }
    UI_End();

    if (UI_BeginMainMenuBar()) {
        if (UI_BeginMenu("File", true)) {
            if (UI_MenuItem("New Map", NULL, false, true)) {
                if (g_is_map_dirty) {
                    g_pending_action = PENDING_ACTION_NEW_MAP;
                }
                else {
                    Scene_Clear(scene, engine);
                    strcpy(g_EditorState.currentMapPath, "untitled.map");
                    Undo_Init();
                }
            }
            if (UI_MenuItem("Load Map...", NULL, false, true)) {
                if (g_is_map_dirty) {
                    g_pending_action = PENDING_ACTION_LOAD_MAP;
                }
                else {
                    g_EditorState.show_load_map_popup = true;
                    ScanMapFiles();
                }
            }
            if (UI_MenuItem("Save", "Ctrl+S", false, true)) {
                if (strcmp(g_EditorState.currentMapPath, "untitled.map") == 0) {
                    g_EditorState.show_save_map_popup = true;
                }
                else {
                    Scene_SaveMap(scene, NULL, g_EditorState.currentMapPath);
                    Editor_SetMapDirty(false);
                    Editor_AddRecentFile(g_EditorState.currentMapPath);
                }
            }
            if (UI_MenuItem("Save Map As...", NULL, false, true)) {
                g_EditorState.show_save_map_popup = true;
            }
            UI_Separator();
            if (UI_BeginMenu("Recent Files", g_EditorState.num_recent_map_files > 0)) {
                for (int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
                    if (UI_MenuItem(g_EditorState.recent_map_files[i], NULL, false, true)) {
                        const char* path_to_load = g_EditorState.recent_map_files[i];

                        Scene_Clear(scene, engine);
                        if (Scene_LoadMap(scene, renderer, path_to_load, engine)) {
                            strcpy(g_EditorState.currentMapPath, path_to_load);
                            Editor_AddRecentFile(path_to_load);
                            Undo_Init();
                        }
                        else {
                            Console_Printf_Error("Failed to load recent map: %s", path_to_load);
                        }
                    }
                }
                UI_EndMenu();
            }
            UI_Separator();
            if (UI_MenuItem("Exit Editor", "F5", false, true)) {
                if (g_is_map_dirty) {
                    g_pending_action = PENDING_ACTION_EXIT_EDITOR;
                }
                else {
                    char* args[] = { "edit" };
                    handle_command(1, args);
                }
            }
            UI_EndMenu();
        }
        if (UI_BeginMenu("Edit", true)) { if (UI_MenuItem("Undo", "Ctrl+Z", false, true)) { Undo_PerformUndo(scene, engine); } if (UI_MenuItem("Redo", "Ctrl+Y", false, true)) { Undo_PerformRedo(scene, engine); } UI_EndMenu(); }
        if (UI_BeginMenu("Tools", true)) {
            if (UI_MenuItem("Group", "Ctrl+G", false, g_EditorState.num_selections > 1)) { Editor_GroupSelection(); }
            if (UI_MenuItem("Ungroup", "Ctrl+U", false, g_EditorState.num_selections > 0)) { Editor_UngroupSelection(); }
            if (UI_MenuItem("Transform", "Ctrl+M", false, g_EditorState.num_selections > 0)) {
                g_EditorState.show_transform_window = true;
                if (g_EditorState.transform_window_mode == TRANSFORM_MODE_SCALE) {
                    g_EditorState.transform_window_values = (Vec3){ 1, 1, 1 };
                }
                else {
                    g_EditorState.transform_window_values = (Vec3){ 0, 0, 0 };
                }
            }
            bool can_merge = false;
            if (g_EditorState.num_selections > 1) {
                can_merge = true;
                for (int i = 0; i < g_EditorState.num_selections; ++i) {
                    if (g_EditorState.selections[i].type != ENTITY_BRUSH) {
                        can_merge = false;
                        break;
                    }
                }
            }
            if (UI_MenuItem("Merge", NULL, false, can_merge)) {
                Editor_MergeSelection(scene, engine);
            }
            UI_Separator();
            if (UI_MenuItem("Flip Horizontal", "Ctrl+L", false, g_EditorState.num_selections > 0)) {
                Editor_FlipSelection(scene, engine, 1);
            }
            if (UI_MenuItem("Flip Vertical", "Ctrl+I", false, g_EditorState.num_selections > 0)) {
                Editor_FlipSelection(scene, engine, 0);
            }
            if (UI_MenuItem("Go to Coordinates...", NULL, false, true)) {
                g_EditorState.show_goto_coord_window = true;
                g_EditorState.goto_coord_input[0] = '\0';
            }
            UI_Separator();
            if (UI_MenuItem("Map Information", NULL, false, true)) {
                g_EditorState.show_map_info_window = true;
            }
            if (UI_MenuItem("Replace Textures...", NULL, false, true)) {
                g_EditorState.show_replace_textures_popup = true;
            }
            if (UI_MenuItem("Sprinkle Tool...", NULL, false, true)) {
                g_EditorState.show_sprinkle_tool_window = true;
            }
            if (UI_Checkbox("Texture Lock", &g_EditorState.texture_lock_enabled)) {
            }
            if (UI_MenuItem("Bake Lighting...", NULL, false, true)) {
                g_EditorState.show_bake_lighting_popup = true;
                g_EditorState.bake_resolution = 3;
                g_EditorState.bake_bounces = 1;
            }
            if (UI_MenuItem("Build Environment probes...", NULL, false, true)) {
                g_EditorState.show_build_cubemaps_popup = true;
                g_EditorState.cubemap_resolution_index = 2;
            }
            UI_EndMenu();
        }
        if (UI_BeginMenu("Help", true)) {
            if (UI_MenuItem("About Tectonic Editor", NULL, false, true)) {
                g_EditorState.show_about_window = true;
            }
            if (UI_MenuItem("Documentation", NULL, false, true)) {
                g_EditorState.show_help_window = true;
                ScanDocFiles();
            }
            UI_EndMenu();
        }
        UI_EndMainMenuBar();
    }

    if (g_EditorState.show_save_map_popup) {
        UI_Begin("Save Map As", &g_EditorState.show_save_map_popup);
        UI_InputText("Filename", g_EditorState.save_map_path, sizeof(g_EditorState.save_map_path));
        if (UI_Button("Save")) {
            Scene_SaveMap(scene, NULL, g_EditorState.save_map_path);
            strcpy(g_EditorState.currentMapPath, g_EditorState.save_map_path);
            Editor_SetMapDirty(false);
            if (g_pending_action != PENDING_ACTION_NONE) {
                Editor_ExecutePendingAction(engine, scene, renderer);
            }
            Editor_AddRecentFile(g_EditorState.currentMapPath);
            Console_Printf("Map saved to %s", g_EditorState.currentMapPath);
            g_EditorState.show_save_map_popup = false;
        }
        UI_End();
    }
    if (g_EditorState.show_load_map_popup) {
        UI_Begin("Load Map", &g_EditorState.show_load_map_popup);
        if (g_EditorState.num_map_files > 0) {
            UI_ListBox("Maps", &g_EditorState.selected_map_file_index, (const char* const*)g_EditorState.map_file_list, g_EditorState.num_map_files, 15);
            if (g_EditorState.selected_map_file_index != -1 && UI_Button("Load Selected Map")) {
                char path_buffer[256];
                sprintf(path_buffer, "%s", g_EditorState.map_file_list[g_EditorState.selected_map_file_index]);
                Scene_LoadMap(scene, renderer, path_buffer, engine);
                strcpy(g_EditorState.currentMapPath, path_buffer);
                Undo_Init();
                Editor_SetMapDirty(false);
                g_EditorState.show_load_map_popup = false;
            }
        }
        else {
            UI_Text("No .map files found in the current directory.");
        }
        if (UI_Button("Refresh List")) {
            ScanMapFiles();
        }
        UI_End();
    }

    Editor_RenderTextureBrowser(scene);
    Editor_RenderModelBrowser(scene, engine, renderer);
    Editor_RenderSoundBrowser(scene);
    Editor_RenderReplaceTexturesUI(scene);
    Editor_RenderVertexToolsWindow(scene);
    Editor_RenderSculptNoisePopup(scene);
    Editor_RenderAboutWindow();
    Editor_RenderHelpWindow();
    Editor_RenderSprinkleToolWindow();
    Editor_RenderBakeLightingWindow(scene, engine);
    Editor_RenderBuildCubemapsWindow(renderer, scene, engine);
    Editor_RenderArchPropertiesWindow(scene, engine);
    Editor_RenderMapInfoWindow(scene);
    Editor_RenderTransformWindow(scene, engine);
    Editor_RenderGoToCoordinatesWindow();

    if (g_pending_action != PENDING_ACTION_NONE) {
        UI_OpenPopup("Unsaved Changes");
    }

    if (UI_BeginPopupModal("Unsaved Changes", NULL, 1 << 3)) {
        UI_Text("You have unsaved changes. Do you want to save them?");
        UI_Spacing();

        if (UI_Button("Save")) {
            if (strcmp(g_EditorState.currentMapPath, "untitled.map") == 0) {
                g_EditorState.show_save_map_popup = true;
            }
            else {
                Scene_SaveMap(scene, NULL, g_EditorState.currentMapPath);
                Editor_SetMapDirty(false);
                Editor_ExecutePendingAction(engine, scene, renderer);
            }
            UI_CloseCurrentPopup();
        }
        UI_SameLine();
        if (UI_Button("Don't Save")) {
            Editor_ExecutePendingAction(engine, scene, renderer);
            UI_CloseCurrentPopup();
        }
        UI_SameLine();
        if (UI_Button("Cancel")) {
            g_pending_action = PENDING_ACTION_NONE;
            UI_CloseCurrentPopup();
        }
        UI_EndPopup();
    }

    float menu_bar_h = 22.0f; float viewports_area_w = screen_w - right_panel_width; float viewports_area_h = screen_h; float half_w = viewports_area_w / 2.0f; float half_h = viewports_area_h / 2.0f; Vec3 p[4] = { {0, menu_bar_h}, {half_w, menu_bar_h}, {0, menu_bar_h + half_h}, {half_w, menu_bar_h + half_h} }; const char* vp_names[] = { "Perspective", "Top (X/Z)","Front (X/Y)","Side (Y/Z)" };

    for (int i = 0; i < 4; i++) {
        ViewportType type = (ViewportType)i;
        UI_SetNextWindowPos(p[i].x, p[i].y);
        UI_SetNextWindowSize(half_w, half_h);
        UI_PushStyleVar_WindowPadding(0, 0);
        UI_Begin_NoBringToFront(vp_names[i], NULL);
        g_EditorState.is_viewport_focused[type] = UI_IsWindowFocused();
        g_EditorState.is_viewport_hovered[type] = UI_IsWindowHovered();
        float vp_w, vp_h;
        UI_GetContentRegionAvail(&vp_w, &vp_h);
        float win_x, win_y, content_min_x, content_min_y, mouse_x, mouse_y;
        UI_GetWindowPos(&win_x, &win_y);
        UI_GetWindowContentRegionMin(&content_min_x, &content_min_y);
        UI_GetMousePos(&mouse_x, &mouse_y);
        g_EditorState.mouse_pos_in_viewport[type].x = mouse_x - (win_x + content_min_x);
        g_EditorState.mouse_pos_in_viewport[type].y = mouse_y - (win_y + content_min_y);
        if (vp_w > 0 && vp_h > 0 && (fabs(vp_w - g_EditorState.viewport_width[type]) > 1 || fabs(vp_h - g_EditorState.viewport_height[type]) > 1)) {
            g_EditorState.viewport_width[type] = (int)vp_w;
            g_EditorState.viewport_height[type] = (int)vp_h;
            glBindTexture(GL_TEXTURE_2D, g_EditorState.viewport_texture[type]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type], 0, GL_RGBA, GL_FLOAT, NULL);
            glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.viewport_rbo[type]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type]);
        }
        UI_Image((void*)(intptr_t)g_EditorState.viewport_texture[type], vp_w, vp_h);

        bool show_dims = false;
        Vec3 b_min, b_max;

        bool is_actively_creating = g_EditorState.is_dragging_for_creation || g_EditorState.is_in_brush_creation_mode;
        bool is_single_brush_selected = (g_EditorState.num_selections == 1 && Editor_GetPrimarySelection()->type == ENTITY_BRUSH);

        if (is_actively_creating) {
            show_dims = true;
            b_min = g_EditorState.preview_brush_world_min;
            b_max = g_EditorState.preview_brush_world_max;
        }
        else if (is_single_brush_selected) {
            show_dims = true;
            EditorSelection* sel = Editor_GetPrimarySelection();
            Brush* b = &scene->brushes[sel->index];
            if (b->numVertices > 0) {
                b_min = (Vec3){ FLT_MAX, FLT_MAX, FLT_MAX };
                b_max = (Vec3){ -FLT_MAX, -FLT_MAX, -FLT_MAX };
                for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                    Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                    b_min.x = fminf(b_min.x, world_v.x);
                    b_min.y = fminf(b_min.y, world_v.y);
                    b_min.z = fminf(b_min.z, world_v.z);
                    b_max.x = fmaxf(b_max.x, world_v.x);
                    b_max.y = fmaxf(b_max.y, world_v.y);
                    b_max.z = fmaxf(b_max.z, world_v.z);
                }
            }
            else {
                b_min = b->pos;
                b_max = b->pos;
            }
        }

        if (show_dims && type >= VIEW_TOP_XZ) {
            void* draw_list = UI_GetWindowDrawList();
            unsigned int text_color = UI_GetColorU32(255, 255, 255, 255);
            Vec3 size = vec3_sub(b_max, b_min);

            Vec3 top_mid_world, left_mid_world;
            char horizontal_text[32], vertical_text[32];

            if (type == VIEW_TOP_XZ) {
                top_mid_world = (Vec3){ (b_min.x + b_max.x) / 2.0f, b_min.y, b_max.z };
                left_mid_world = (Vec3){ b_min.x, b_min.y, (b_min.z + b_max.z) / 2.0f };
                sprintf(horizontal_text, "%.0f", fabsf(size.x));
                sprintf(vertical_text, "%.0f", fabsf(size.z));
            }
            else if (type == VIEW_FRONT_XY) {
                top_mid_world = (Vec3){ (b_min.x + b_max.x) / 2.0f, b_max.y, b_min.z };
                left_mid_world = (Vec3){ b_min.x, (b_min.y + b_max.y) / 2.0f, b_min.z };
                sprintf(horizontal_text, "%.0f", fabsf(size.x));
                sprintf(vertical_text, "%.0f", fabsf(size.y));
            }
            else {
                top_mid_world = (Vec3){ b_min.x, b_max.y, (b_min.z + b_max.z) / 2.0f };
                left_mid_world = (Vec3){ b_min.x, (b_min.y + b_max.y) / 2.0f, b_min.z };
                sprintf(horizontal_text, "%.0f", fabsf(size.z));
                sprintf(vertical_text, "%.0f", fabsf(size.y));
            }

            Vec2 top_mid_screen = WorldToScreen(top_mid_world, type);
            Vec2 left_mid_screen = WorldToScreen(left_mid_world, type);

            float h_text_offset = strlen(horizontal_text) * 4.0f;
            UI_DrawList_AddText(draw_list, win_x + top_mid_screen.x - h_text_offset, win_y + top_mid_screen.y - 20.0f, text_color, horizontal_text);

            float v_text_offset = strlen(vertical_text) * 8.0f;
            UI_DrawList_AddText(draw_list, win_x + left_mid_screen.x - v_text_offset - 10.0f, win_y + left_mid_screen.y - 8.0f, text_color, vertical_text);
        }
        UI_End();
        UI_PopStyleVar(1);
    }
    Editor_RenderFaceEditSheet(scene, engine);
    Editor_RenderStatusBar();
}