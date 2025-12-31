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
#include "gl_misc.h"
#include "editor_misc.h"

void Editor_SetMapDirty(bool is_dirty) {
    g_is_map_dirty = is_dirty;
}

void Editor_SaveRecentFiles() {
    FILE* file = fopen("editor_prefs.cfg", "w");
    if (!file) return;
    for (int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
        fprintf(file, "%s\n", g_EditorState.recent_map_files[i]);
    }
    fclose(file);
}

void Editor_LoadRecentFiles() {
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

void Editor_AddRecentFile(const char* path) {
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

void Editor_InitGizmo() {
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