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
#include "gl_console.h"
#include "editor_geometry_helpers.h"
#include "map_misc.h"

static Vec3 g_sort_normal;
static Vec3 g_sort_centroid;

void Brush_SetVerticesFromBox(Brush* b, Vec3 size) {
    Brush_FreeData(b);
    b->numVertices = 8;
    b->vertices = new BrushVertex[b->numVertices];
    Vec3 half_size = vec3_muls(size, 0.5f);
    b->vertices[0].pos = Vec3{ -half_size.x, -half_size.y,  half_size.z };
    b->vertices[1].pos = Vec3{ half_size.x, -half_size.y,  half_size.z };
    b->vertices[2].pos = Vec3{ half_size.x,  half_size.y,  half_size.z };
    b->vertices[3].pos = Vec3{ -half_size.x,  half_size.y,  half_size.z };
    b->vertices[4].pos = Vec3{ -half_size.x, -half_size.y, -half_size.z };
    b->vertices[5].pos = Vec3{ half_size.x, -half_size.y, -half_size.z };
    b->vertices[6].pos = Vec3{ half_size.x,  half_size.y, -half_size.z };
    b->vertices[7].pos = Vec3{ -half_size.x,  half_size.y, -half_size.z };

    for (Int i = 0; i < 8; ++i) {
        b->vertices[i].color = Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = 6;
    b->faces = new BrushFace[b->numFaces];
    static const Int face_defs[6][4] = {
    {0, 1, 2, 3},
    {5, 4, 7, 6},
    {3, 2, 6, 7},
    {0, 4, 5, 1},
    {1, 5, 6, 2},
    {4, 0, 3, 7}
    };
    for (Int i = 0; i < 6; ++i) {
        b->faces[i].material = TextureManager_GetMaterial(0);
        b->faces[i].material2 = nullptr;
        b->faces[i].uv_offset = Vec2{ 0,0 };
        b->faces[i].uv_scale = Vec2{ 1,1 };
        b->faces[i].uv_rotation = 0;
        b->faces[i].uv_offset2 = Vec2{ 0,0 };
        b->faces[i].uv_scale2 = Vec2{ 1,1 };
        b->faces[i].uv_rotation2 = 0;
        b->faces[i].material3 = nullptr;
        b->faces[i].uv_offset3 = Vec2{ 0,0 };
        b->faces[i].uv_scale3 = Vec2{ 1,1 };
        b->faces[i].uv_rotation3 = 0;
        b->faces[i].material4 = nullptr;
        b->faces[i].uv_offset4 = Vec2{ 0,0 };
        b->faces[i].uv_scale4 = Vec2{ 1,1 };
        b->faces[i].uv_rotation4 = 0;
        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].numVertexIndices = 4;
        b->faces[i].vertexIndices = new Int[4];
        for (Int j = 0; j < 4; ++j) b->faces[i].vertexIndices[j] = face_defs[i][j];
    }
}

void Brush_SetVerticesFromCylinder(Brush* b, Vec3 size, Int num_sides) {
    if (num_sides < 3) num_sides = 3;
    Brush_FreeData(b);

    Float radius_x = size.x / 2.0f;
    Float radius_z = size.z / 2.0f;
    Float height = size.y;

    b->numVertices = num_sides * 2;
    b->vertices = new BrushVertex[b->numVertices];

    for (Int i = 0; i < num_sides; ++i) {
        Float angle = (Float)i / (Float)num_sides * 2.0f * M_PI;
        Float x = cosf(angle) * radius_x;
        Float z = sinf(angle) * radius_z;

        b->vertices[i].pos = Vec3{ x, height / 2.0f, z };
        b->vertices[i + num_sides].pos = Vec3{ x, -height / 2.0f, z };
    }

    for (Int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = num_sides + 2;
    b->faces = new BrushFace[b->numFaces];

    for (Int i = 0; i < num_sides; ++i) {
        BrushFace temp_face = {};
        temp_face.material = TextureManager_GetMaterial(0);
        temp_face.numVertexIndices = 4;
        temp_face.uv_scale = Vec2{ 1, 1 };
        b->faces[i] = temp_face;

        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].vertexIndices = new Int[4];
        b->faces[i].vertexIndices[0] = i;
        b->faces[i].vertexIndices[1] = (i + 1) % num_sides;
        b->faces[i].vertexIndices[2] = ((i + 1) % num_sides) + num_sides;
        b->faces[i].vertexIndices[3] = i + num_sides;
    }

    BrushFace temp_face_cap1 = {};
    temp_face_cap1.material = TextureManager_GetMaterial(0);
    temp_face_cap1.numVertexIndices = num_sides;
    temp_face_cap1.uv_scale = Vec2{ 1, 1 };
    b->faces[num_sides] = temp_face_cap1;
    b->faces[num_sides].lightmap_scale = 1.0f;
    b->faces[num_sides].vertexIndices = new Int[num_sides];
    for (Int i = 0; i < num_sides; ++i) b->faces[num_sides].vertexIndices[i] = i;

    BrushFace temp_face_cap2 = {};
    temp_face_cap2.material = TextureManager_GetMaterial(0);
    temp_face_cap2.numVertexIndices = num_sides;
    temp_face_cap2.uv_scale = Vec2{ 1, 1 };
    b->faces[num_sides + 1] = temp_face_cap2;
    b->faces[num_sides + 1].lightmap_scale = 1.0f;
    b->faces[num_sides + 1].vertexIndices = new Int[num_sides];
    for (Int i = 0; i < num_sides; ++i) b->faces[num_sides + 1].vertexIndices[i] = (num_sides - 1 - i) + num_sides;
}

void Brush_SetVerticesFromWedge(Brush* b, Vec3 size) {
    Brush_FreeData(b);
    Vec3 half_size = vec3_muls(size, 0.5f);

    b->numVertices = 6;
    b->vertices = new BrushVertex[b->numVertices];

    b->vertices[0].pos = Vec3{ -half_size.x, -half_size.y, -half_size.z };
    b->vertices[1].pos = Vec3{ half_size.x, -half_size.y, -half_size.z };
    b->vertices[2].pos = Vec3{ half_size.x, -half_size.y,  half_size.z };
    b->vertices[3].pos = Vec3{ -half_size.x, -half_size.y,  half_size.z };
    b->vertices[4].pos = Vec3{ -half_size.x,  half_size.y, -half_size.z };
    b->vertices[5].pos = Vec3{ half_size.x,  half_size.y, -half_size.z };

    for (Int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = 5;
    b->faces = new BrushFace[b->numFaces];

    Int face_defs[5][4] = {
        {0, 3, 2, 1},
        {0, 1, 5, 4},
        {3, 2, 5, 4},
        {0, 4, 3, -1},
        {1, 2, 5, -1}
    };
    Int num_indices_per_face[] = { 4, 4, 4, 3, 3 };

    for (Int i = 0; i < 5; ++i) {
        BrushFace temp_face = {};
        temp_face.material = TextureManager_GetMaterial(0);
        temp_face.numVertexIndices = num_indices_per_face[i];
        temp_face.uv_scale = Vec2{ 1, 1 };
        b->faces[i] = temp_face;

        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].vertexIndices = new Int[b->faces[i].numVertexIndices];
        for (Int j = 0; j < b->faces[i].numVertexIndices; ++j) {
            b->faces[i].vertexIndices[j] = face_defs[i][j];
        }
    }
}

void Brush_SetVerticesFromSpike(Brush* b, Vec3 size, Int num_sides) {
    if (num_sides < 3) num_sides = 3;
    Brush_FreeData(b);

    Float radius_x = size.x / 2.0f;
    Float radius_z = size.z / 2.0f;
    Float height = size.y;

    b->numVertices = num_sides + 1;
    b->vertices = new BrushVertex[b->numVertices];

    b->vertices[0].pos = Vec3{ 0, height / 2.0f, 0 };

    for (Int i = 0; i < num_sides; ++i) {
        Float angle = (Float)i / (Float)num_sides * 2.0f * M_PI;
        Float x = cosf(angle) * radius_x;
        Float z = sinf(angle) * radius_z;
        b->vertices[i + 1].pos = Vec3{ x, -height / 2.0f, z };
    }

    for (Int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = num_sides + 1;
    b->faces = new BrushFace[b->numFaces];

    for (Int i = 0; i < num_sides; ++i) {
        BrushFace temp_face = {};
        temp_face.material = TextureManager_GetMaterial(0);
        temp_face.numVertexIndices = 3;
        temp_face.uv_scale = Vec2{ 1, 1 };
        b->faces[i] = temp_face;

        b->faces[i].vertexIndices = new Int[3];
        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].vertexIndices[0] = 0;
        b->faces[i].vertexIndices[1] = (i + 1) % num_sides + 1;
        b->faces[i].vertexIndices[2] = i + 1;
    }

    BrushFace temp_face_cap = {};
    temp_face_cap.material = TextureManager_GetMaterial(0);
    temp_face_cap.numVertexIndices = num_sides;
    temp_face_cap.uv_scale = Vec2{ 1, 1 };
    b->faces[num_sides] = temp_face_cap;

    b->faces[num_sides].lightmap_scale = 1.0f;
    b->faces[num_sides].vertexIndices = new Int[num_sides];
    for (Int i = 0; i < num_sides; ++i) {
        b->faces[num_sides].vertexIndices[i] = (num_sides - i) + 0;
    }
}

void Brush_SetVerticesFromSphere(Brush* b, Vec3 size, Int sides) {
    Brush_FreeData(b);
    Int stacks = sides / 2;
    b->numVertices = (sides + 1) * (stacks + 1);
    b->vertices = new BrushVertex[b->numVertices]();

    Vec3 radius = vec3_muls(size, 0.5f);

    for (Int i = 0; i <= stacks; i++) {
        Float stack_angle = M_PI / 2 - i * M_PI / stacks;
        Float xy = radius.x * cosf(stack_angle);
        Float z = radius.z * sinf(stack_angle);

        for (Int j = 0; j <= sides; j++) {
            Float sector_angle = j * 2 * M_PI / sides;
            Float x = xy * cosf(sector_angle);
            Float y = xy * sinf(sector_angle);
            b->vertices[i * (sides + 1) + j].pos = Vec3{ x, y, z };
        }
    }

    b->numFaces = sides * stacks;
    b->faces = new BrushFace[b->numFaces]();
    Int face_index = 0;
    for (Int i = 0; i < stacks; i++) {
        for (Int j = 0; j < sides; j++) {
            Int p1 = i * (sides + 1) + j;
            Int p2 = p1 + 1;
            Int p3 = (i + 1) * (sides + 1) + j;
            Int p4 = p3 + 1;

            b->faces[face_index].numVertexIndices = 4;
            b->faces[face_index].vertexIndices = new Int[4];
            b->faces[face_index].vertexIndices[0] = p1;
            b->faces[face_index].vertexIndices[1] = p3;
            b->faces[face_index].vertexIndices[2] = p4;
            b->faces[face_index].vertexIndices[3] = p2;

            b->faces[face_index].material = TextureManager_GetMaterial(0);
            b->faces[face_index].uv_scale = Vec2{ 1,1 };
            b->faces[face_index].lightmap_scale = 1.0f;
            face_index++;
        }
    }
}

void Brush_SetVerticesFromSemiSphere(Brush* b, Vec3 size, Int sides) {
    Brush_FreeData(b);

    Int stacks = sides / 2;
    Int ring_vertices = (sides + 1);
    Int num_dome_verts = ring_vertices * (stacks + 1);

    b->numVertices = num_dome_verts + 1;
    b->vertices = new BrushVertex[b->numVertices]();

    Vec3 radius = vec3_muls(size, 0.5f);

    for (Int i = 0; i <= stacks; i++) {
        Float stack_angle = M_PI / 2 - i * (M_PI / 2) / stacks;
        Float xy = radius.x * cosf(stack_angle);
        Float z = radius.z * sinf(stack_angle);

        for (Int j = 0; j <= sides; j++) {
            Float sector_angle = j * 2 * M_PI / sides;
            Float x = xy * cosf(sector_angle);
            Float y = xy * sinf(sector_angle);
            b->vertices[i * ring_vertices + j].pos = Vec3{ x, y, z };
        }
    }

    Int bottom_center_index = b->numVertices - 1;
    Float bottom_z = b->vertices[stacks * ring_vertices].pos.z;
    b->vertices[bottom_center_index].pos = Vec3{ 0, 0, bottom_z };

    b->numFaces = (sides * stacks) + sides;
    b->faces = new BrushFace[b->numFaces]();

    Int face_index = 0;

    for (Int i = 0; i < stacks; i++) {
        for (Int j = 0; j < sides; j++) {
            Int p1 = i * ring_vertices + j;
            Int p2 = p1 + 1;
            Int p3 = (i + 1) * ring_vertices + j;
            Int p4 = p3 + 1;

            b->faces[face_index].numVertexIndices = 4;
            b->faces[face_index].vertexIndices = new Int[4];
            b->faces[face_index].vertexIndices[0] = p1;
            b->faces[face_index].vertexIndices[1] = p3;
            b->faces[face_index].vertexIndices[2] = p4;
            b->faces[face_index].vertexIndices[3] = p2;
            face_index++;
        }
    }

    Int base_start = stacks * ring_vertices;
    for (Int j = 0; j < sides; j++) {
        Int p1 = base_start + j;
        Int p2 = base_start + (j + 1) % ring_vertices;

        b->faces[face_index].numVertexIndices = 3;
        b->faces[face_index].vertexIndices = new Int[3];
        b->faces[face_index].vertexIndices[0] = bottom_center_index;
        b->faces[face_index].vertexIndices[1] = p1;
        b->faces[face_index].vertexIndices[2] = p2;

        BrushFace* face_to_flip = &b->faces[face_index];
        Int num_indices = face_to_flip->numVertexIndices;
        for (Int k = 0; k < num_indices / 2; ++k) {
            Int temp = face_to_flip->vertexIndices[k];
            face_to_flip->vertexIndices[k] = face_to_flip->vertexIndices[num_indices - 1 - k];
            face_to_flip->vertexIndices[num_indices - 1 - k] = temp;
        }

        face_index++;
    }

    for (Int i = 0; i < b->numFaces; i++) {
        b->faces[i].material = TextureManager_GetMaterial(0);
        b->faces[i].uv_scale = Vec2{ 1,1 };
        b->faces[i].lightmap_scale = 1.0f;
    }
}

void Brush_SetVerticesFromTube(Brush* b, Vec3 size, Int num_sides, Float wall_thickness) {
    if (num_sides < 3) num_sides = 3;
    Brush_FreeData(b);

    Float radius_x = size.x / 2.0f;
    Float radius_z = size.z / 2.0f;
    Float height = size.y;
    Float inner_radius_x = radius_x - wall_thickness;
    Float inner_radius_z = radius_z - wall_thickness;

    if (inner_radius_x < 0.01f) inner_radius_x = 0.01f;
    if (inner_radius_z < 0.01f) inner_radius_z = 0.01f;

    b->numVertices = num_sides * 4;
    b->vertices = new BrushVertex[b->numVertices];

    for (Int i = 0; i < num_sides; ++i) {
        Float angle = (Float)i / (Float)num_sides * 2.0f * M_PI;
        Float cos_a = cosf(angle);
        Float sin_a = sinf(angle);

        b->vertices[i].pos = Vec3{ cos_a * radius_x, height / 2.0f, sin_a * radius_z };
        b->vertices[i + num_sides].pos = Vec3{ cos_a * radius_x, -height / 2.0f, sin_a * radius_z };
        b->vertices[i + 2 * num_sides].pos = Vec3{ cos_a * inner_radius_x, height / 2.0f, sin_a * inner_radius_z };
        b->vertices[i + 3 * num_sides].pos = Vec3{ cos_a * inner_radius_x, -height / 2.0f, sin_a * inner_radius_z };
    }

    for (Int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = num_sides * 4;
    b->faces = new BrushFace[b->numFaces];

    for (Int i = 0; i < num_sides; ++i) {
        Int next_i = (i + 1) % num_sides;

        Int face_idx = i;
        BrushFace temp_face1 = {};
        temp_face1.material = TextureManager_GetMaterial(0);
        temp_face1.numVertexIndices = 4;
        temp_face1.uv_scale = Vec2{ 1, 1 };
        temp_face1.lightmap_scale = 1.0f;
        b->faces[face_idx] = temp_face1;

        b->faces[face_idx].vertexIndices = new Int[4];
        b->faces[face_idx].vertexIndices[0] = i;
        b->faces[face_idx].vertexIndices[1] = next_i;
        b->faces[face_idx].vertexIndices[2] = next_i + num_sides;
        b->faces[face_idx].vertexIndices[3] = i + num_sides;

        face_idx = i + num_sides;
        BrushFace temp_face2 = {};
        temp_face2.material = TextureManager_GetMaterial(0);
        temp_face2.numVertexIndices = 4;
        temp_face2.uv_scale = Vec2{ 1, 1 };
        temp_face2.lightmap_scale = 1.0f;
        b->faces[face_idx] = temp_face2;

        b->faces[face_idx].vertexIndices = new Int[4];
        b->faces[face_idx].vertexIndices[0] = next_i + 2 * num_sides;
        b->faces[face_idx].vertexIndices[1] = i + 2 * num_sides;
        b->faces[face_idx].vertexIndices[2] = i + 3 * num_sides;
        b->faces[face_idx].vertexIndices[3] = next_i + 3 * num_sides;

        face_idx = i + 2 * num_sides;
        BrushFace temp_face3 = {};
        temp_face3.material = TextureManager_GetMaterial(0);
        temp_face3.numVertexIndices = 4;
        temp_face3.uv_scale = Vec2{ 1, 1 };
        temp_face3.lightmap_scale = 1.0f;
        b->faces[face_idx] = temp_face3;

        b->faces[face_idx].vertexIndices = new Int[4];
        b->faces[face_idx].vertexIndices[0] = next_i;
        b->faces[face_idx].vertexIndices[1] = i;
        b->faces[face_idx].vertexIndices[2] = i + 2 * num_sides;
        b->faces[face_idx].vertexIndices[3] = next_i + 2 * num_sides;

        face_idx = i + 3 * num_sides;
        BrushFace temp_face4 = {};
        temp_face4.material = TextureManager_GetMaterial(0);
        temp_face4.numVertexIndices = 4;
        temp_face4.uv_scale = Vec2{ 1, 1 };
        temp_face4.lightmap_scale = 1.0f;
        b->faces[face_idx] = temp_face4;

        b->faces[face_idx].vertexIndices = new Int[4];
        b->faces[face_idx].vertexIndices[0] = i + num_sides;
        b->faces[face_idx].vertexIndices[1] = next_i + num_sides;
        b->faces[face_idx].vertexIndices[2] = next_i + 3 * num_sides;
        b->faces[face_idx].vertexIndices[3] = i + 3 * num_sides;
    }
}

static Int compare_cap_verts(const void* a, const void* b) {
    Vec3 va = *(const Vec3*)a;
    Vec3 vb = *(const Vec3*)b;

    Vec3 dir_a = vec3_sub(va, g_sort_centroid);
    Vec3 dir_b = vec3_sub(vb, g_sort_centroid);

    Vec3 u_axis = vec3_cross(g_sort_normal, Vec3{ 0, 0, 1 });
    if (vec3_length_sq(u_axis) < 1e-6) u_axis = vec3_cross(g_sort_normal, Vec3{ 0, 1, 0 });
    vec3_normalize(&u_axis);
    Vec3 v_axis = vec3_cross(g_sort_normal, u_axis);

    Float a_u = vec3_dot(dir_a, u_axis);
    Float a_v = vec3_dot(dir_a, v_axis);
    Float b_u = vec3_dot(dir_b, u_axis);
    Float b_v = vec3_dot(dir_b, v_axis);

    Float angle_a = atan2f(a_v, a_u);
    Float angle_b = atan2f(b_v, b_u);

    if (angle_a < angle_b) return -1;
    if (angle_a > angle_b) return 1;
    return 0;
}

void Brush_Clip(Brush* b, Vec3 plane_normal, Float plane_d) {
    if (!b || b->numVertices == 0 || b->numFaces == 0) return;

    Float* dists = nullptr;
    Int* side = nullptr;
    BrushVertex* temp_new_verts = nullptr;
    Int* temp_face_verts_idx = nullptr;
    BrushVertex* temp_cap_verts = nullptr;
    BrushFace* new_face_list_array = nullptr;
    Int current_new_face_count = 0;

    dists = new Float[b->numVertices];
    side = new Int[b->numVertices];

    Int positive_count = 0;
    Int negative_count = 0;
    for (Int i = 0; i < b->numVertices; ++i) {
        Vec3 world_vertex_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
        dists[i] = vec3_dot(plane_normal, world_vertex_pos) + plane_d;
        if (dists[i] > 1e-5) {
            side[i] = 1; positive_count++;
        }
        else if (dists[i] < -1e-5) {
            side[i] = -1; negative_count++;
        }
        else {
            side[i] = 0;
        }
    }

    if (positive_count == 0 || negative_count == 0) {
        if (positive_count == 0) {
            Brush_FreeData(b);
        }
        delete[] dists;
        delete[] side;
    }

    temp_new_verts = new BrushVertex[MAX_BRUSH_VERTS * 2];
    temp_face_verts_idx = new Int[MAX_BRUSH_VERTS];
    temp_cap_verts = new BrushVertex[MAX_BRUSH_FACES + 1];
    new_face_list_array = new BrushFace[MAX_BRUSH_FACES];

    Int new_vert_count = 0;
    Int vert_map[MAX_BRUSH_VERTS];
    memset(vert_map, -1, sizeof(vert_map));

    for (Int i = 0; i < b->numVertices; ++i) {
        if (side[i] >= 0) {
            if (new_vert_count >= MAX_BRUSH_VERTS * 2) {
                Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_VERTS * 2 for new_verts.\n");
                for (Int k = 0; k < current_new_face_count; ++k) delete[] new_face_list_array[k].vertexIndices;
                delete[] dists;
                delete[] side;
                delete[] temp_new_verts;
                delete[] temp_face_verts_idx;
                delete[] temp_cap_verts;
                delete[] new_face_list_array;
            }
            vert_map[i] = new_vert_count;
            temp_new_verts[new_vert_count++] = b->vertices[i];
        }
    }

    BrushFace* old_faces = b->faces;
    Int old_face_count = b->numFaces;

    for (Int i = 0; i < old_face_count; ++i) {
        BrushFace* face = &old_faces[i];
        Int face_verts_current_idx_count = 0;

        for (Int j = 0; j < face->numVertexIndices; ++j) {
            Int p1_idx = face->vertexIndices[j];
            Int p2_idx = face->vertexIndices[(j + 1) % face->numVertexIndices];

            if (side[p1_idx] >= 0) {
                if (face_verts_current_idx_count >= MAX_BRUSH_VERTS) {
                    Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_VERTS for temp_face_verts_idx.\n");
                    for (Int k = 0; k < current_new_face_count; ++k) delete[] new_face_list_array[k].vertexIndices;
                    delete[] dists;
                    delete[] side;
                    delete[] temp_new_verts;
                    delete[] temp_face_verts_idx;
                    delete[] temp_cap_verts;
                    delete[] new_face_list_array;
                }
                temp_face_verts_idx[face_verts_current_idx_count++] = vert_map[p1_idx];
            }

            if (side[p1_idx] * side[p2_idx] < 0) {
                Float t = dists[p1_idx] / (dists[p1_idx] - dists[p2_idx]);
                Vec3 intersect_pos = vec3_add(b->vertices[p1_idx].pos, vec3_muls(vec3_sub(b->vertices[p2_idx].pos, b->vertices[p1_idx].pos), t));
                Vec4 intersect_color;
                intersect_color.x = b->vertices[p1_idx].color.x + (b->vertices[p2_idx].color.x - b->vertices[p1_idx].color.x) * t;
                intersect_color.y = b->vertices[p1_idx].color.y + (b->vertices[p2_idx].color.y - b->vertices[p1_idx].color.y) * t;
                intersect_color.z = b->vertices[p1_idx].color.z + (b->vertices[p2_idx].color.z - b->vertices[p1_idx].color.z) * t;
                intersect_color.w = b->vertices[p1_idx].color.w + (b->vertices[p2_idx].color.w - b->vertices[p1_idx].color.w) * t;

                if (face_verts_current_idx_count >= MAX_BRUSH_VERTS) {
                    Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_VERTS for temp_face_verts_idx after adding intersection.\n");
                    for (Int k = 0; k < current_new_face_count; ++k) delete[] new_face_list_array[k].vertexIndices;
                    delete[] dists;
                    delete[] side;
                    delete[] temp_new_verts;
                    delete[] temp_face_verts_idx;
                    delete[] temp_cap_verts;
                    delete[] new_face_list_array;
                }
                if (new_vert_count >= MAX_BRUSH_VERTS * 2) {
                    Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_VERTS * 2 for temp_new_verts after adding intersection.\n");
                    for (Int k = 0; k < current_new_face_count; ++k) delete[] new_face_list_array[k].vertexIndices;
                    delete[] dists;
                    delete[] side;
                    delete[] temp_new_verts;
                    delete[] temp_face_verts_idx;
                    delete[] temp_cap_verts;
                    delete[] new_face_list_array;
                }

                temp_face_verts_idx[face_verts_current_idx_count++] = new_vert_count;
                temp_new_verts[new_vert_count].pos = intersect_pos;
                temp_new_verts[new_vert_count].color = intersect_color;
                new_vert_count++;
            }
        }

        if (face_verts_current_idx_count >= 3) {
            if (current_new_face_count >= MAX_BRUSH_FACES) {
                Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_FACES for new_face_list_array.\n");
                for (Int k = 0; k < current_new_face_count; ++k) delete[] new_face_list_array[k].vertexIndices;
                delete[] dists;
                delete[] side;
                delete[] temp_new_verts;
                delete[] temp_face_verts_idx;
                delete[] temp_cap_verts;
                delete[] new_face_list_array;
            }
            BrushFace new_face_entry = *face;
            new_face_entry.numVertexIndices = face_verts_current_idx_count;
            new_face_entry.vertexIndices = new Int[face_verts_current_idx_count];
            memcpy(new_face_entry.vertexIndices, temp_face_verts_idx, face_verts_current_idx_count * sizeof(Int));
            new_face_list_array[current_new_face_count++] = new_face_entry;
        }
    }

    Int cap_vert_count = 0;
    for (Int i = 0; i < b->numFaces; ++i) {
        BrushFace* face = &b->faces[i];
        for (Int j = 0; j < face->numVertexIndices; ++j) {
            Int p1_idx = face->vertexIndices[j];
            Int p2_idx = face->vertexIndices[(j + 1) % face->numVertexIndices];

            if (side[p1_idx] * side[p2_idx] < 0) {
                Float t = dists[p1_idx] / (dists[p1_idx] - dists[p2_idx]);
                Vec3 intersect_pos = vec3_add(b->vertices[p1_idx].pos, vec3_muls(vec3_sub(b->vertices[p2_idx].pos, b->vertices[p1_idx].pos), t));
                Vec4 intersect_color;
                intersect_color.x = b->vertices[p1_idx].color.x + (b->vertices[p2_idx].color.x - b->vertices[p1_idx].color.x) * t;
                intersect_color.y = b->vertices[p1_idx].color.y + (b->vertices[p2_idx].color.y - b->vertices[p1_idx].color.y) * t;
                intersect_color.z = b->vertices[p1_idx].color.z + (b->vertices[p2_idx].color.z - b->vertices[p1_idx].color.z) * t;
                intersect_color.w = b->vertices[p1_idx].color.w + (b->vertices[p2_idx].color.w - b->vertices[p1_idx].color.w) * t;

                Bool is_duplicate = false;
                for (Int k = 0; k < cap_vert_count; ++k) {
                    if (vec3_length_sq(vec3_sub(temp_cap_verts[k].pos, intersect_pos)) < 1e-6) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (!is_duplicate) {
                    if (cap_vert_count >= MAX_BRUSH_FACES + 1) {
                        Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_FACES for temp_cap_verts.\n");
                        for (Int k = 0; k < current_new_face_count; ++k) delete[] new_face_list_array[k].vertexIndices;
                        delete[] dists;
                        delete[] side;
                        delete[] temp_new_verts;
                        delete[] temp_face_verts_idx;
                        delete[] temp_cap_verts;
                        delete[] new_face_list_array;
                    }
                    temp_cap_verts[cap_vert_count].pos = intersect_pos;
                    temp_cap_verts[cap_vert_count].color = intersect_color;
                    cap_vert_count++;
                }
            }
        }
    }

    if (cap_vert_count >= 3) {
        Vec3 centroid = { 0,0,0 };
        for (Int i = 0; i < cap_vert_count; ++i) centroid = vec3_add(centroid, temp_cap_verts[i].pos);
        centroid = vec3_muls(centroid, 1.0f / cap_vert_count);

        g_sort_normal = plane_normal;
        g_sort_centroid = centroid;
        qsort(temp_cap_verts, cap_vert_count, sizeof(BrushVertex), compare_cap_verts);

        if (current_new_face_count >= MAX_BRUSH_FACES) {
            Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_FACES for new_face_list_array (adding cap).\n");
            for (Int k = 0; k < current_new_face_count; ++k) delete[] new_face_list_array[k].vertexIndices;
            delete[] dists;
            delete[] side;
            delete[] temp_new_verts;
            delete[] temp_face_verts_idx;
            delete[] temp_cap_verts;
            delete[] new_face_list_array;
        }

        BrushFace cap_face;
        cap_face.material = TextureManager_GetMaterial(0);
        cap_face.material2 = nullptr;
        cap_face.material3 = nullptr;
        cap_face.material4 = nullptr;
        cap_face.uv_offset = Vec2{ 0, 0 };
        cap_face.uv_scale = Vec2{ 1, 1 };
        cap_face.uv_rotation = 0;
        cap_face.uv_offset2 = Vec2{ 0, 0 };
        cap_face.uv_scale2 = Vec2{ 1, 1 };
        cap_face.uv_rotation2 = 0;
        cap_face.uv_offset3 = Vec2{ 0, 0 };
        cap_face.uv_scale3 = Vec2{ 1, 1 };
        cap_face.uv_rotation3 = 0;
        cap_face.uv_offset4 = Vec2{ 0, 0 };
        cap_face.uv_scale4 = Vec2{ 1, 1 };
        cap_face.uv_rotation4 = 0;

        cap_face.numVertexIndices = cap_vert_count;
        cap_face.vertexIndices = new Int[cap_vert_count];

        for (Int i = 0; i < cap_vert_count; ++i) {
            Int vert_idx = -1;
            for (Int k = 0; k < new_vert_count; ++k) {
                if (vec3_length_sq(vec3_sub(temp_new_verts[k].pos, temp_cap_verts[i].pos)) < 1e-6) {
                    vert_idx = k;
                    break;
                }
            }
            if (vert_idx == -1) {
                Console_Printf_Error("Brush_Clip: Capping vertex not found in temp_new_verts.\n");
                delete[] cap_face.vertexIndices;
                for (Int k = 0; k < current_new_face_count; ++k) delete[] new_face_list_array[k].vertexIndices;
                delete[] dists;
                delete[] side;
                delete[] temp_new_verts;
                delete[] temp_face_verts_idx;
                delete[] temp_cap_verts;
                delete[] new_face_list_array;
            }
            cap_face.vertexIndices[i] = vert_idx;
        }
        for (Int k = 0; k < cap_vert_count / 2; ++k) {
            Int temp = cap_face.vertexIndices[k];
            cap_face.vertexIndices[k] = cap_face.vertexIndices[cap_vert_count - 1 - k];
            cap_face.vertexIndices[cap_vert_count - 1 - k] = temp;
        }
        new_face_list_array[current_new_face_count++] = cap_face;
    }

    Brush_FreeData(b);

    b->numVertices = new_vert_count;
    b->vertices = temp_new_verts;
    temp_new_verts = nullptr;

    b->numFaces = current_new_face_count;
    b->faces = new_face_list_array;
    new_face_list_array = nullptr;

    delete[] dists;
    delete[] side;
    delete[] temp_face_verts_idx;
    delete[] temp_cap_verts;
}