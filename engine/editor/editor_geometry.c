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
#include "editor_geometry.h"
#include "editor_selection.h"
#include "editor_math.h"
#include "editor_undo.h"
#include "gl_console.h"
#include <float.h>

void Editor_SubdivideBrushFace(Scene* scene, Engine* engine, int brush_index, int face_index, int u_divs, int v_divs) {
    if (brush_index < 0 || brush_index >= scene->numBrushes) return;
    Brush* b = &scene->brushes[brush_index];
    if (face_index < 0 || face_index >= b->numFaces) return;

    BrushFace* old_face = &b->faces[face_index];
    if (old_face->numVertexIndices != 4) {
        Console_Printf_Error("[error] Can only subdivide 4-sided faces for now.");
        return;
    }

    Undo_BeginEntityModification(scene, ENTITY_BRUSH, brush_index);

    BrushVertex p00 = b->vertices[old_face->vertexIndices[0]];
    BrushVertex p10 = b->vertices[old_face->vertexIndices[1]];
    BrushVertex p11 = b->vertices[old_face->vertexIndices[2]];
    BrushVertex p01 = b->vertices[old_face->vertexIndices[3]];

    int num_new_verts = (u_divs + 1) * (v_divs + 1);
    BrushVertex* new_grid_verts = malloc(num_new_verts * sizeof(BrushVertex));

    for (int v = 0; v <= v_divs; ++v) {
        for (int u = 0; u <= u_divs; ++u) {
            float u_t = (float)u / u_divs;
            float v_t = (float)v / v_divs;

            BrushVertex p_u0 = {
                .pos = vec3_add(vec3_muls(p00.pos, 1.0f - u_t), vec3_muls(p10.pos, u_t)),
                .color = {
                    p00.color.x * (1.0f - u_t) + p10.color.x * u_t,
                    p00.color.y * (1.0f - u_t) + p10.color.y * u_t,
                    p00.color.z * (1.0f - u_t) + p10.color.z * u_t,
                    p00.color.w * (1.0f - u_t) + p10.color.w * u_t
                }
            };
            BrushVertex p_u1 = {
                .pos = vec3_add(vec3_muls(p01.pos, 1.0f - u_t), vec3_muls(p11.pos, u_t)),
                .color = {
                    p01.color.x * (1.0f - u_t) + p11.color.x * u_t,
                    p01.color.y * (1.0f - u_t) + p11.color.y * u_t,
                    p01.color.z * (1.0f - u_t) + p11.color.z * u_t,
                    p01.color.w * (1.0f - u_t) + p11.color.w * u_t
                }
            };

            int index = v * (u_divs + 1) + u;
            new_grid_verts[index].pos = vec3_add(vec3_muls(p_u0.pos, 1.0f - v_t), vec3_muls(p_u1.pos, v_t));
            new_grid_verts[index].color.x = p_u0.color.x * (1.0f - v_t) + p_u1.color.x * v_t;
            new_grid_verts[index].color.y = p_u0.color.y * (1.0f - v_t) + p_u1.color.y * v_t;
            new_grid_verts[index].color.z = p_u0.color.z * (1.0f - v_t) + p_u1.color.z * v_t;
            new_grid_verts[index].color.w = p_u0.color.w * (1.0f - v_t) + p_u1.color.w * v_t;
        }
    }

    int num_new_faces = u_divs * v_divs;
    BrushFace* new_faces = malloc(num_new_faces * sizeof(BrushFace));

    if (b->lightmapAtlas != 0) {
        glDeleteTextures(1, &b->lightmapAtlas);
        b->lightmapAtlas = 0;
    }
    if (b->directionalLightmapAtlas != 0) {
        glDeleteTextures(1, &b->directionalLightmapAtlas);
        b->directionalLightmapAtlas = 0;
    }

    for (int v = 0; v < v_divs; ++v) {
        for (int u = 0; u < u_divs; ++u) {
            int face_idx = v * u_divs + u;
            new_faces[face_idx] = *old_face;

            new_faces[face_idx].atlas_coords = (Vec4){ 0.0f, 0.0f, 0.0f, 0.0f };
            new_faces[face_idx].numVertexIndices = 4;
            new_faces[face_idx].vertexIndices = malloc(4 * sizeof(int));

            new_faces[face_idx].vertexIndices[0] = v * (u_divs + 1) + u;
            new_faces[face_idx].vertexIndices[1] = v * (u_divs + 1) + (u + 1);
            new_faces[face_idx].vertexIndices[2] = (v + 1) * (u_divs + 1) + (u + 1);
            new_faces[face_idx].vertexIndices[3] = (v + 1) * (u_divs + 1) + u;
        }
    }

    free(b->faces[face_index].vertexIndices);
    for (int i = face_index; i < b->numFaces - 1; ++i) {
        b->faces[i] = b->faces[i + 1];
    }
    b->numFaces--;

    int old_vert_count = b->numVertices;
    b->vertices = realloc(b->vertices, (old_vert_count + num_new_verts) * sizeof(BrushVertex));
    int old_face_count = b->numFaces;
    b->faces = realloc(b->faces, (old_face_count + num_new_faces) * sizeof(BrushFace));

    for (int i = 0; i < num_new_verts; ++i) {
        b->vertices[old_vert_count + i] = new_grid_verts[i];
    }
    for (int i = 0; i < num_new_faces; ++i) {
        for (int j = 0; j < 4; ++j) {
            new_faces[i].vertexIndices[j] += old_vert_count;
        }
        b->faces[old_face_count + i] = new_faces[i];
    }

    b->numVertices += num_new_verts;
    b->numFaces += num_new_faces;

    free(new_grid_verts);
    char group_name[64];
    snprintf(group_name, sizeof(group_name), "subdiv_group_%d", g_EditorState.next_group_id++);
    for (int i = old_face_count; i < b->numFaces; ++i) {
        b->faces[i].isGrouped = true;
        strncpy(b->faces[i].groupName, group_name, sizeof(b->faces[i].groupName) - 1);
        b->faces[i].groupName[sizeof(b->faces[i].groupName) - 1] = '\0';
    }
    free(new_faces);

    Brush_CreateRenderData(b);
    if (b->physicsBody) {
        Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
        for (int i = 0; i < b->numVertices; ++i) {
            world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
        }
        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
        free(world_verts);
    }

    Undo_EndEntityModification(scene, ENTITY_BRUSH, brush_index, "Subdivide Face");
    Console_Printf("Subdivided face %d of brush %d.", face_index, brush_index);
}
 
void Editor_CreateBrushFromPreview(Scene* scene, Engine* engine, Brush* preview) {
    if (scene->numBrushes >= MAX_BRUSHES) { return; }
    Brush* b = &scene->brushes[scene->numBrushes];
    memset(b, 0, sizeof(Brush));
    Brush_DeepCopy(b, preview);
    for (int i = 0; i < b->numFaces; i++) {
        b->faces[i].isGrouped = false;
        b->faces[i].groupName[0] = '\0';
    }
    b->vao = 0; b->vbo = 0;
    b->mass = 0.0f;
    b->isPhysicsEnabled = true;
    b->physicsBody = NULL;
    Brush_UpdateMatrix(b); Brush_CreateRenderData(b);
    if (Brush_IsSolid(b) && b->numVertices > 0) {
        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
        for (int i = 0; i < b->numVertices; i++) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
        free(world_verts);
    }
    int new_brush_index = scene->numBrushes;
    scene->numBrushes++;
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_BRUSH, new_brush_index, 0, 0);
    Undo_PushCreateEntity(scene, ENTITY_BRUSH, new_brush_index, "Create Brush");
}

void Editor_UpdatePreviewBrushFromWorldMinMax() {
    Brush* b = &g_EditorState.preview_brush;

    Vec3 world_min = g_EditorState.preview_brush_world_min;
    Vec3 world_max = g_EditorState.preview_brush_world_max;

    if (world_min.x > world_max.x) { float t = world_min.x; world_min.x = world_max.x; world_max.x = t; }
    if (world_min.y > world_max.y) { float t = world_min.y; world_min.y = world_max.y; world_max.y = t; }
    if (world_min.z > world_max.z) { float t = world_min.z; world_min.z = world_max.z; world_max.z = t; }

    Vec3 size = vec3_sub(world_max, world_min);
    const float min_dim = 0.01f;
    if (size.x < min_dim) size.x = min_dim;
    if (size.y < min_dim) size.y = min_dim;
    if (size.z < min_dim) size.z = min_dim;

    g_EditorState.preview_brush_world_min = world_min;
    g_EditorState.preview_brush_world_max = vec3_add(world_min, size);

    b->pos = vec3_muls(vec3_add(g_EditorState.preview_brush_world_min, g_EditorState.preview_brush_world_max), 0.5f);
    b->rot = (Vec3){ 0,0,0 };
    b->scale = (Vec3){ 1,1,1 };

    Vec3 local_size = vec3_sub(g_EditorState.preview_brush_world_max, g_EditorState.preview_brush_world_min);
    switch (g_EditorState.current_brush_shape) {
    case BRUSH_SHAPE_BLOCK:
        Brush_SetVerticesFromBox(b, local_size);
        break;
    case BRUSH_SHAPE_CYLINDER:
        Brush_SetVerticesFromCylinder(b, local_size, g_EditorState.cylinder_creation_steps);
        break;
    case BRUSH_SHAPE_TUBE:
        Brush_SetVerticesFromTube(b, local_size, g_EditorState.cylinder_creation_steps, g_EditorState.tube_wall_thickness);
        break;
    case BRUSH_SHAPE_WEDGE:
        Brush_SetVerticesFromWedge(b, local_size);
        break;
    case BRUSH_SHAPE_SPIKE:
        Brush_SetVerticesFromSpike(b, local_size, g_EditorState.cylinder_creation_steps);
        break;
    case BRUSH_SHAPE_SPHERE:
        Brush_SetVerticesFromSphere(b, local_size, g_EditorState.cylinder_creation_steps);
        break;
    case BRUSH_SHAPE_SEMI_SPHERE:
        Brush_SetVerticesFromSemiSphere(b, local_size, g_EditorState.cylinder_creation_steps);
        break;
    }
    Brush_UpdateMatrix(b);
    Brush_CreateRenderData(b);
}

void Editor_UpdatePreviewBrushForInitialDrag(Vec3 p1_world_drag, Vec3 p2_world_drag, ViewportType creation_view) {
    Vec3 world_min, world_max;

    if (creation_view == VIEW_TOP_XZ) {
        world_min.x = fminf(p1_world_drag.x, p2_world_drag.x);
        world_max.x = fmaxf(p1_world_drag.x, p2_world_drag.x);
        world_min.z = fminf(p1_world_drag.z, p2_world_drag.z);
        world_max.z = fmaxf(p1_world_drag.z, p2_world_drag.z);
        float half_depth = g_EditorState.grid_size * 0.5f;
        float center_y = g_EditorState.brush_creation_start_point_2d_drag.y;
        world_min.y = center_y;
        world_max.y = center_y + g_EditorState.grid_size;
        if (g_EditorState.snap_to_grid) {
            world_min.y = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.y, g_EditorState.grid_size);
            world_max.y = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.y + g_EditorState.grid_size, g_EditorState.grid_size);
        }
        else {
            world_min.y = g_EditorState.brush_creation_start_point_2d_drag.y;
            world_max.y = g_EditorState.brush_creation_start_point_2d_drag.y + g_EditorState.grid_size;
        }
    }
    else if (creation_view == VIEW_FRONT_XY) {
        world_min.x = fminf(p1_world_drag.x, p2_world_drag.x);
        world_max.x = fmaxf(p1_world_drag.x, p2_world_drag.x);
        world_min.y = fminf(p1_world_drag.y, p2_world_drag.y);
        world_max.y = fmaxf(p1_world_drag.y, p2_world_drag.y);

        if (g_EditorState.snap_to_grid) {
            world_min.z = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.z, g_EditorState.grid_size);
            world_max.z = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.z + g_EditorState.grid_size, g_EditorState.grid_size);
        }
        else {
            world_min.z = g_EditorState.brush_creation_start_point_2d_drag.z;
            world_max.z = g_EditorState.brush_creation_start_point_2d_drag.z + g_EditorState.grid_size;
        }

    }
    else if (creation_view == VIEW_SIDE_YZ) {
        world_min.y = fminf(p1_world_drag.y, p2_world_drag.y);
        world_max.y = fmaxf(p1_world_drag.y, p2_world_drag.y);
        world_min.z = fminf(p1_world_drag.z, p2_world_drag.z);
        world_max.z = fmaxf(p1_world_drag.z, p2_world_drag.z);

        if (g_EditorState.snap_to_grid) {
            world_min.x = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.x, g_EditorState.grid_size);
            world_max.x = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.x + g_EditorState.grid_size, g_EditorState.grid_size);
        }
        else {
            world_min.x = g_EditorState.brush_creation_start_point_2d_drag.x;
            world_max.x = g_EditorState.brush_creation_start_point_2d_drag.x + g_EditorState.grid_size;
        }
    }

    g_EditorState.preview_brush_world_min = world_min;
    g_EditorState.preview_brush_world_max = world_max;
    Editor_UpdatePreviewBrushFromWorldMinMax();
}

void Editor_AdjustPreviewBrushByHandle(Vec2 mouse_pos_in_viewport, ViewportType current_view) {
    if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_NONE) return;
    if (current_view != g_EditorState.preview_brush_drag_handle_view) return;

    Vec3 mouse_world_raw = ScreenToWorld_Unsnapped_ForOrthoPicking(mouse_pos_in_viewport, current_view);
    Vec3 mouse_world_snapped = mouse_world_raw;

    if (g_EditorState.snap_to_grid) {
        mouse_world_snapped.x = SnapValue(mouse_world_raw.x, g_EditorState.grid_size);
        mouse_world_snapped.y = SnapValue(mouse_world_raw.y, g_EditorState.grid_size);
        mouse_world_snapped.z = SnapValue(mouse_world_raw.z, g_EditorState.grid_size);
    }

    switch (g_EditorState.preview_brush_active_handle) {
    case PREVIEW_BRUSH_HANDLE_MIN_X:
        if (current_view == VIEW_TOP_XZ || current_view == VIEW_FRONT_XY) {
            g_EditorState.preview_brush_world_min.x = mouse_world_snapped.x;
        }
        break;
    case PREVIEW_BRUSH_HANDLE_MAX_X:
        if (current_view == VIEW_TOP_XZ || current_view == VIEW_FRONT_XY) {
            g_EditorState.preview_brush_world_max.x = mouse_world_snapped.x;
        }
        break;
    case PREVIEW_BRUSH_HANDLE_MIN_Y:
        if (current_view == VIEW_FRONT_XY || current_view == VIEW_SIDE_YZ) {
            g_EditorState.preview_brush_world_min.y = mouse_world_snapped.y;
        }
        break;
    case PREVIEW_BRUSH_HANDLE_MAX_Y:
        if (current_view == VIEW_FRONT_XY || current_view == VIEW_SIDE_YZ) {
            g_EditorState.preview_brush_world_max.y = mouse_world_snapped.y;
        }
        break;
    case PREVIEW_BRUSH_HANDLE_MIN_Z:
        if (current_view == VIEW_TOP_XZ || current_view == VIEW_SIDE_YZ) {
            g_EditorState.preview_brush_world_min.z = mouse_world_snapped.z;
        }
        break;
    case PREVIEW_BRUSH_HANDLE_MAX_Z:
        if (current_view == VIEW_TOP_XZ || current_view == VIEW_SIDE_YZ) {
            g_EditorState.preview_brush_world_max.z = mouse_world_snapped.z;
        }
        break;
    default:
        break;
    }

    Vec3 temp_min = g_EditorState.preview_brush_world_min;
    Vec3 temp_max = g_EditorState.preview_brush_world_max;

    if (temp_min.x > temp_max.x) { float t = temp_min.x; temp_min.x = temp_max.x; temp_max.x = t; }
    if (temp_min.y > temp_max.y) { float t = temp_min.y; temp_min.y = temp_max.y; temp_max.y = t; }
    if (temp_min.z > temp_max.z) { float t = temp_min.z; temp_min.z = temp_max.z; temp_max.z = t; }

    const float min_brush_dim = 0.01f;
    if (temp_max.x - temp_min.x < min_brush_dim) {
        if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MIN_X) temp_min.x = temp_max.x - min_brush_dim;
        else if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MAX_X) temp_max.x = temp_min.x + min_brush_dim;
        else if (temp_max.x - temp_min.x < min_brush_dim) temp_max.x = temp_min.x + min_brush_dim;
    }
    if (temp_max.y - temp_min.y < min_brush_dim) {
        if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MIN_Y) temp_min.y = temp_max.y - min_brush_dim;
        else if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MAX_Y) temp_max.y = temp_min.y + min_brush_dim;
        else if (temp_max.y - temp_min.y < min_brush_dim) temp_max.y = temp_min.y + min_brush_dim;
    }
    if (temp_max.z - temp_min.z < min_brush_dim) {
        if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MIN_Z) temp_min.z = temp_max.z - min_brush_dim;
        else if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MAX_Z) temp_max.z = temp_min.z + min_brush_dim;
        else if (temp_max.z - temp_min.z < min_brush_dim) temp_max.z = temp_min.z + min_brush_dim;
    }

    g_EditorState.preview_brush_world_min = temp_min;
    g_EditorState.preview_brush_world_max = temp_max;

    Editor_UpdatePreviewBrushFromWorldMinMax();
}

void Editor_AdjustPreviewBrush(Vec2 mouse_pos, ViewportType adjust_view) {
    Brush* b = &g_EditorState.preview_brush;
    Vec3 p_current = ScreenToWorld(mouse_pos, adjust_view);
    Vec3 min_v = { FLT_MAX, FLT_MAX, FLT_MAX }; Vec3 max_v = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (int i = 0; i < 8; ++i) {
        min_v.x = fminf(min_v.x, b->vertices[i].pos.x); min_v.y = fminf(min_v.y, b->vertices[i].pos.y); min_v.z = fminf(min_v.z, b->vertices[i].pos.z);
        max_v.x = fmaxf(max_v.x, b->vertices[i].pos.x); max_v.y = fmaxf(max_v.y, b->vertices[i].pos.y); max_v.z = fmaxf(max_v.z, b->vertices[i].pos.z);
    }
    Vec3 size = { max_v.x - min_v.x, max_v.y - min_v.y, max_v.z - min_v.z };
    if (g_EditorState.brush_creation_view == VIEW_TOP_XZ) {
        if (adjust_view == VIEW_FRONT_XY || adjust_view == VIEW_SIDE_YZ) { size.y = fabsf(p_current.y); b->pos.y = p_current.y / 2.0f; }
    }
    else if (g_EditorState.brush_creation_view == VIEW_FRONT_XY) {
        if (adjust_view == VIEW_TOP_XZ || adjust_view == VIEW_SIDE_YZ) { size.z = fabsf(p_current.z); b->pos.z = p_current.z / 2.0f; }
    }
    else if (g_EditorState.brush_creation_view == VIEW_SIDE_YZ) {
        if (adjust_view == VIEW_TOP_XZ || adjust_view == VIEW_FRONT_XY) { size.x = fabsf(p_current.x); b->pos.x = p_current.x / 2.0f; }
    }

    if (g_EditorState.snap_to_grid) {
        size.x = SnapValue(size.x, g_EditorState.grid_size);
        size.y = SnapValue(size.y, g_EditorState.grid_size);
        size.z = SnapValue(size.z, g_EditorState.grid_size);
        b->pos.x = SnapValue(b->pos.x, g_EditorState.grid_size * 0.5f);
        b->pos.y = SnapValue(b->pos.y, g_EditorState.grid_size * 0.5f);
        b->pos.z = SnapValue(b->pos.z, g_EditorState.grid_size * 0.5f);
    }

    if (size.x < 0.01f) size.x = 0.01f;
    if (size.y < 0.01f) size.y = 0.01f;
    if (size.z < 0.01f) size.z = 0.01f;
    Brush_SetVerticesFromBox(b, size); Brush_UpdateMatrix(b); Brush_CreateRenderData(b);
}

void Editor_AdjustSelectedBrushByHandle(Scene* scene, Engine* engine, Vec2 mouse_pos, ViewportType view) {
    if (g_EditorState.selected_brush_active_handle == PREVIEW_BRUSH_HANDLE_NONE) return;
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (!primary || primary->type != ENTITY_BRUSH) return;

    Brush* b = &scene->brushes[primary->index];
    if (!b) return;

    Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(mouse_pos, view);
    if (g_EditorState.snap_to_grid) {
        mouse_world.x = SnapValue(mouse_world.x, g_EditorState.grid_size);
        mouse_world.y = SnapValue(mouse_world.y, g_EditorState.grid_size);
        mouse_world.z = SnapValue(mouse_world.z, g_EditorState.grid_size);
    }

    Mat4 inv_model_matrix;
    if (!mat4_inverse(&b->modelMatrix, &inv_model_matrix)) {
        return;
    }
    Vec3 new_local_pos = mat4_mul_vec3(&inv_model_matrix, mouse_world);

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

    const float min_brush_dim = 0.1f;
    switch (g_EditorState.selected_brush_active_handle) {
    case PREVIEW_BRUSH_HANDLE_MIN_X: {
        float clamped_x = (new_local_pos.x > local_max.x - min_brush_dim) ? (local_max.x - min_brush_dim) : new_local_pos.x;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.x - local_min.x) < 0.001f) {
                b->vertices[i].pos.x = clamped_x;
            }
        }
        break;
    }
    case PREVIEW_BRUSH_HANDLE_MAX_X: {
        float clamped_x = (new_local_pos.x < local_min.x + min_brush_dim) ? (local_min.x + min_brush_dim) : new_local_pos.x;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.x - local_max.x) < 0.001f) {
                b->vertices[i].pos.x = clamped_x;
            }
        }
        break;
    }
    case PREVIEW_BRUSH_HANDLE_MIN_Y: {
        float clamped_y = (new_local_pos.y > local_max.y - min_brush_dim) ? (local_max.y - min_brush_dim) : new_local_pos.y;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.y - local_min.y) < 0.001f) {
                b->vertices[i].pos.y = clamped_y;
            }
        }
        break;
    }
    case PREVIEW_BRUSH_HANDLE_MAX_Y: {
        float clamped_y = (new_local_pos.y < local_min.y + min_brush_dim) ? (local_min.y + min_brush_dim) : new_local_pos.y;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.y - local_max.y) < 0.001f) {
                b->vertices[i].pos.y = clamped_y;
            }
        }
        break;
    }
    case PREVIEW_BRUSH_HANDLE_MIN_Z: {
        float clamped_z = (new_local_pos.z > local_max.z - min_brush_dim) ? (local_max.z - min_brush_dim) : new_local_pos.z;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.z - local_min.z) < 0.001f) {
                b->vertices[i].pos.z = clamped_z;
            }
        }
        break;
    }
    case PREVIEW_BRUSH_HANDLE_MAX_Z: {
        float clamped_z = (new_local_pos.z < local_min.z + min_brush_dim) ? (local_min.z + min_brush_dim) : new_local_pos.z;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.z - local_max.z) < 0.001f) {
                b->vertices[i].pos.z = clamped_z;
            }
        }
        break;
    }
    default: break;
    }

    Brush_CreateRenderData(b);
    if (b->physicsBody) {
        Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
        for (int i = 0; i < b->numVertices; i++) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
        free(world_verts);
    }
}