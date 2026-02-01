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

static Vec3 g_sort_normal;
static Vec3 g_sort_centroid;

void Brush_SetVerticesFromBox(Brush* b, Vec3 size) {
    Brush_FreeData(b);
    b->numVertices = 8;
    b->vertices = (BrushVertex*)malloc(b->numVertices * sizeof(BrushVertex));
    Vec3 half_size = vec3_muls(size, 0.5f);
    b->vertices[0].pos = Vec3{ -half_size.x, -half_size.y,  half_size.z };
    b->vertices[1].pos = Vec3{ half_size.x, -half_size.y,  half_size.z };
    b->vertices[2].pos = Vec3{ half_size.x,  half_size.y,  half_size.z };
    b->vertices[3].pos = Vec3{ -half_size.x,  half_size.y,  half_size.z };
    b->vertices[4].pos = Vec3{ -half_size.x, -half_size.y, -half_size.z };
    b->vertices[5].pos = Vec3{ half_size.x, -half_size.y, -half_size.z };
    b->vertices[6].pos = Vec3{ half_size.x,  half_size.y, -half_size.z };
    b->vertices[7].pos = Vec3{ -half_size.x,  half_size.y, -half_size.z };

    for (int i = 0; i < 8; ++i) {
        b->vertices[i].color = Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = 6;
    b->faces = (BrushFace*)malloc(b->numFaces * sizeof(BrushFace));
    static const int face_defs[6][4] = {
    {0, 1, 2, 3},
    {5, 4, 7, 6},
    {3, 2, 6, 7},
    {0, 4, 5, 1},
    {1, 5, 6, 2},
    {4, 0, 3, 7}
    };
    for (int i = 0; i < 6; ++i) {
        b->faces[i].material = TextureManager_GetMaterial(0);
        b->faces[i].material2 = NULL;
        b->faces[i].uv_offset = Vec2{ 0,0 };
        b->faces[i].uv_scale = Vec2{ 1,1 };
        b->faces[i].uv_rotation = 0;
        b->faces[i].uv_offset2 = Vec2{ 0,0 };
        b->faces[i].uv_scale2 = Vec2{ 1,1 };
        b->faces[i].uv_rotation2 = 0;
        b->faces[i].material3 = NULL;
        b->faces[i].uv_offset3 = Vec2{ 0,0 };
        b->faces[i].uv_scale3 = Vec2{ 1,1 };
        b->faces[i].uv_rotation3 = 0;
        b->faces[i].material4 = NULL;
        b->faces[i].uv_offset4 = Vec2{ 0,0 };
        b->faces[i].uv_scale4 = Vec2{ 1,1 };
        b->faces[i].uv_rotation4 = 0;
        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].numVertexIndices = 4;
        b->faces[i].vertexIndices = (int*)malloc(4 * sizeof(int));
        for (int j = 0; j < 4; ++j) b->faces[i].vertexIndices[j] = face_defs[i][j];
    }
}

void Brush_SetVerticesFromCylinder(Brush* b, Vec3 size, int num_sides) {
    if (num_sides < 3) num_sides = 3;
    Brush_FreeData(b);

    float radius_x = size.x / 2.0f;
    float radius_z = size.z / 2.0f;
    float height = size.y;

    b->numVertices = num_sides * 2;
    b->vertices = (BrushVertex*)malloc(b->numVertices * sizeof(BrushVertex));

    for (int i = 0; i < num_sides; ++i) {
        float angle = (float)i / (float)num_sides * 2.0f * M_PI;
        float x = cosf(angle) * radius_x;
        float z = sinf(angle) * radius_z;

        b->vertices[i].pos = Vec3{ x, height / 2.0f, z };
        b->vertices[i + num_sides].pos = Vec3{ x, -height / 2.0f, z };
    }

    for (int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = num_sides + 2;
    b->faces = (BrushFace*)malloc(b->numFaces * sizeof(BrushFace));

    for (int i = 0; i < num_sides; ++i) {
        BrushFace temp_face = {};
        temp_face.material = TextureManager_GetMaterial(0);
        temp_face.numVertexIndices = 4;
        temp_face.uv_scale = Vec2{ 1, 1 };
        b->faces[i] = temp_face;

        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].vertexIndices = (int*)malloc(4 * sizeof(int));
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
    b->faces[num_sides].vertexIndices = (int*)malloc(num_sides * sizeof(int));
    for (int i = 0; i < num_sides; ++i) b->faces[num_sides].vertexIndices[i] = i;

    BrushFace temp_face_cap2 = {};
    temp_face_cap2.material = TextureManager_GetMaterial(0);
    temp_face_cap2.numVertexIndices = num_sides;
    temp_face_cap2.uv_scale = Vec2{ 1, 1 };
    b->faces[num_sides + 1] = temp_face_cap2;
    b->faces[num_sides + 1].lightmap_scale = 1.0f;
    b->faces[num_sides + 1].vertexIndices = (int*)malloc(num_sides * sizeof(int));
    for (int i = 0; i < num_sides; ++i) b->faces[num_sides + 1].vertexIndices[i] = (num_sides - 1 - i) + num_sides;
}

void Brush_SetVerticesFromWedge(Brush* b, Vec3 size) {
    Brush_FreeData(b);
    Vec3 half_size = vec3_muls(size, 0.5f);

    b->numVertices = 6;
    b->vertices = (BrushVertex*)malloc(b->numVertices * sizeof(BrushVertex));

    b->vertices[0].pos = Vec3{ -half_size.x, -half_size.y, -half_size.z };
    b->vertices[1].pos = Vec3{ half_size.x, -half_size.y, -half_size.z };
    b->vertices[2].pos = Vec3{ half_size.x, -half_size.y,  half_size.z };
    b->vertices[3].pos = Vec3{ -half_size.x, -half_size.y,  half_size.z };
    b->vertices[4].pos = Vec3{ -half_size.x,  half_size.y, -half_size.z };
    b->vertices[5].pos = Vec3{ half_size.x,  half_size.y, -half_size.z };

    for (int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = 5;
    b->faces = (BrushFace*)malloc(b->numFaces * sizeof(BrushFace));

    int face_defs[5][4] = {
        {0, 3, 2, 1},
        {0, 1, 5, 4},
        {3, 2, 5, 4},
        {0, 4, 3, -1},
        {1, 2, 5, -1}
    };
    int num_indices_per_face[] = { 4, 4, 4, 3, 3 };

    for (int i = 0; i < 5; ++i) {
        BrushFace temp_face = {};
        temp_face.material = TextureManager_GetMaterial(0);
        temp_face.numVertexIndices = num_indices_per_face[i];
        temp_face.uv_scale = Vec2{ 1, 1 };
        b->faces[i] = temp_face;

        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].vertexIndices = (int*)malloc(b->faces[i].numVertexIndices * sizeof(int));
        for (int j = 0; j < b->faces[i].numVertexIndices; ++j) {
            b->faces[i].vertexIndices[j] = face_defs[i][j];
        }
    }
}

void Brush_SetVerticesFromSpike(Brush* b, Vec3 size, int num_sides) {
    if (num_sides < 3) num_sides = 3;
    Brush_FreeData(b);

    float radius_x = size.x / 2.0f;
    float radius_z = size.z / 2.0f;
    float height = size.y;

    b->numVertices = num_sides + 1;
    b->vertices = (BrushVertex*)malloc(b->numVertices * sizeof(BrushVertex));

    b->vertices[0].pos = Vec3{ 0, height / 2.0f, 0 };

    for (int i = 0; i < num_sides; ++i) {
        float angle = (float)i / (float)num_sides * 2.0f * M_PI;
        float x = cosf(angle) * radius_x;
        float z = sinf(angle) * radius_z;
        b->vertices[i + 1].pos = Vec3{ x, -height / 2.0f, z };
    }

    for (int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = num_sides + 1;
    b->faces = (BrushFace*)malloc(b->numFaces * sizeof(BrushFace));

    for (int i = 0; i < num_sides; ++i) {
        BrushFace temp_face = {};
        temp_face.material = TextureManager_GetMaterial(0);
        temp_face.numVertexIndices = 3;
        temp_face.uv_scale = Vec2{ 1, 1 };
        b->faces[i] = temp_face;

        b->faces[i].vertexIndices = (int*)malloc(3 * sizeof(int));
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
    b->faces[num_sides].vertexIndices = (int*)malloc(num_sides * sizeof(int));
    for (int i = 0; i < num_sides; ++i) {
        b->faces[num_sides].vertexIndices[i] = (num_sides - i) + 0;
    }
}

void Brush_SetVerticesFromSphere(Brush* b, Vec3 size, int sides) {
    Brush_FreeData(b);
    int stacks = sides / 2;
    b->numVertices = (sides + 1) * (stacks + 1);
    b->vertices = (BrushVertex*)calloc(b->numVertices, sizeof(BrushVertex));

    Vec3 radius = vec3_muls(size, 0.5f);

    for (int i = 0; i <= stacks; i++) {
        float stack_angle = M_PI / 2 - i * M_PI / stacks;
        float xy = radius.x * cosf(stack_angle);
        float z = radius.z * sinf(stack_angle);

        for (int j = 0; j <= sides; j++) {
            float sector_angle = j * 2 * M_PI / sides;
            float x = xy * cosf(sector_angle);
            float y = xy * sinf(sector_angle);
            b->vertices[i * (sides + 1) + j].pos = Vec3{ x, y, z };
        }
    }

    b->numFaces = sides * stacks;
    b->faces = (BrushFace*)calloc(b->numFaces, sizeof(BrushFace));
    int face_index = 0;
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < sides; j++) {
            int p1 = i * (sides + 1) + j;
            int p2 = p1 + 1;
            int p3 = (i + 1) * (sides + 1) + j;
            int p4 = p3 + 1;

            b->faces[face_index].numVertexIndices = 4;
            b->faces[face_index].vertexIndices = (int*)malloc(4 * sizeof(int));
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

void Brush_SetVerticesFromSemiSphere(Brush* b, Vec3 size, int sides) {
    Brush_FreeData(b);

    int stacks = sides / 2;
    int ring_vertices = (sides + 1);
    int num_dome_verts = ring_vertices * (stacks + 1);

    b->numVertices = num_dome_verts + 1;
    b->vertices = (BrushVertex*)calloc(b->numVertices, sizeof(BrushVertex));

    Vec3 radius = vec3_muls(size, 0.5f);

    for (int i = 0; i <= stacks; i++) {
        float stack_angle = M_PI / 2 - i * (M_PI / 2) / stacks;
        float xy = radius.x * cosf(stack_angle);
        float z = radius.z * sinf(stack_angle);

        for (int j = 0; j <= sides; j++) {
            float sector_angle = j * 2 * M_PI / sides;
            float x = xy * cosf(sector_angle);
            float y = xy * sinf(sector_angle);
            b->vertices[i * ring_vertices + j].pos = Vec3{ x, y, z };
        }
    }

    int bottom_center_index = b->numVertices - 1;
    float bottom_z = b->vertices[stacks * ring_vertices].pos.z;
    b->vertices[bottom_center_index].pos = Vec3{ 0, 0, bottom_z };

    b->numFaces = (sides * stacks) + sides;
    b->faces = (BrushFace*)calloc(b->numFaces, sizeof(BrushFace));

    int face_index = 0;

    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < sides; j++) {
            int p1 = i * ring_vertices + j;
            int p2 = p1 + 1;
            int p3 = (i + 1) * ring_vertices + j;
            int p4 = p3 + 1;

            b->faces[face_index].numVertexIndices = 4;
            b->faces[face_index].vertexIndices = (int*)malloc(4 * sizeof(int));
            b->faces[face_index].vertexIndices[0] = p1;
            b->faces[face_index].vertexIndices[1] = p3;
            b->faces[face_index].vertexIndices[2] = p4;
            b->faces[face_index].vertexIndices[3] = p2;
            face_index++;
        }
    }

    int base_start = stacks * ring_vertices;
    for (int j = 0; j < sides; j++) {
        int p1 = base_start + j;
        int p2 = base_start + (j + 1) % ring_vertices;

        b->faces[face_index].numVertexIndices = 3;
        b->faces[face_index].vertexIndices = (int*)malloc(3 * sizeof(int));
        b->faces[face_index].vertexIndices[0] = bottom_center_index;
        b->faces[face_index].vertexIndices[1] = p1;
        b->faces[face_index].vertexIndices[2] = p2;

        BrushFace* face_to_flip = &b->faces[face_index];
        int num_indices = face_to_flip->numVertexIndices;
        for (int k = 0; k < num_indices / 2; ++k) {
            int temp = face_to_flip->vertexIndices[k];
            face_to_flip->vertexIndices[k] = face_to_flip->vertexIndices[num_indices - 1 - k];
            face_to_flip->vertexIndices[num_indices - 1 - k] = temp;
        }

        face_index++;
    }

    for (int i = 0; i < b->numFaces; i++) {
        b->faces[i].material = TextureManager_GetMaterial(0);
        b->faces[i].uv_scale = Vec2{ 1,1 };
        b->faces[i].lightmap_scale = 1.0f;
    }
}

void Brush_SetVerticesFromTube(Brush* b, Vec3 size, int num_sides, float wall_thickness) {
    if (num_sides < 3) num_sides = 3;
    Brush_FreeData(b);

    float radius_x = size.x / 2.0f;
    float radius_z = size.z / 2.0f;
    float height = size.y;
    float inner_radius_x = radius_x - wall_thickness;
    float inner_radius_z = radius_z - wall_thickness;

    if (inner_radius_x < 0.01f) inner_radius_x = 0.01f;
    if (inner_radius_z < 0.01f) inner_radius_z = 0.01f;

    b->numVertices = num_sides * 4;
    b->vertices = (BrushVertex*)malloc(b->numVertices * sizeof(BrushVertex));

    for (int i = 0; i < num_sides; ++i) {
        float angle = (float)i / (float)num_sides * 2.0f * M_PI;
        float cos_a = cosf(angle);
        float sin_a = sinf(angle);

        b->vertices[i].pos = Vec3{ cos_a * radius_x, height / 2.0f, sin_a * radius_z };
        b->vertices[i + num_sides].pos = Vec3{ cos_a * radius_x, -height / 2.0f, sin_a * radius_z };
        b->vertices[i + 2 * num_sides].pos = Vec3{ cos_a * inner_radius_x, height / 2.0f, sin_a * inner_radius_z };
        b->vertices[i + 3 * num_sides].pos = Vec3{ cos_a * inner_radius_x, -height / 2.0f, sin_a * inner_radius_z };
    }

    for (int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = Vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = num_sides * 4;
    b->faces = (BrushFace*)malloc(b->numFaces * sizeof(BrushFace));

    for (int i = 0; i < num_sides; ++i) {
        int next_i = (i + 1) % num_sides;

        int face_idx = i;
        BrushFace temp_face1 = {};
        temp_face1.material = TextureManager_GetMaterial(0);
        temp_face1.numVertexIndices = 4;
        temp_face1.uv_scale = Vec2{ 1, 1 };
        temp_face1.lightmap_scale = 1.0f;
        b->faces[face_idx] = temp_face1;

        b->faces[face_idx].vertexIndices = (int*)malloc(4 * sizeof(int));
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

        b->faces[face_idx].vertexIndices = (int*)malloc(4 * sizeof(int));
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

        b->faces[face_idx].vertexIndices = (int*)malloc(4 * sizeof(int));
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

        b->faces[face_idx].vertexIndices = (int*)malloc(4 * sizeof(int));
        b->faces[face_idx].vertexIndices[0] = i + num_sides;
        b->faces[face_idx].vertexIndices[1] = next_i + num_sides;
        b->faces[face_idx].vertexIndices[2] = next_i + 3 * num_sides;
        b->faces[face_idx].vertexIndices[3] = i + 3 * num_sides;
    }
}

static int compare_cap_verts(const void* a, const void* b) {
    Vec3 va = *(const Vec3*)a;
    Vec3 vb = *(const Vec3*)b;

    Vec3 dir_a = vec3_sub(va, g_sort_centroid);
    Vec3 dir_b = vec3_sub(vb, g_sort_centroid);

    Vec3 u_axis = vec3_cross(g_sort_normal, Vec3{ 0, 0, 1 });
    if (vec3_length_sq(u_axis) < 1e-6) u_axis = vec3_cross(g_sort_normal, Vec3{ 0, 1, 0 });
    vec3_normalize(&u_axis);
    Vec3 v_axis = vec3_cross(g_sort_normal, u_axis);

    float a_u = vec3_dot(dir_a, u_axis);
    float a_v = vec3_dot(dir_a, v_axis);
    float b_u = vec3_dot(dir_b, u_axis);
    float b_v = vec3_dot(dir_b, v_axis);

    float angle_a = atan2f(a_v, a_u);
    float angle_b = atan2f(b_v, b_u);

    if (angle_a < angle_b) return -1;
    if (angle_a > angle_b) return 1;
    return 0;
}

void Brush_Clip(Brush* b, Vec3 plane_normal, float plane_d) {
    if (!b || b->numVertices == 0 || b->numFaces == 0) return;

    float* dists = NULL;
    int* side = NULL;
    BrushVertex* temp_new_verts = NULL;
    int* temp_face_verts_idx = NULL;
    BrushVertex* temp_cap_verts = NULL;
    BrushFace* new_face_list_array = NULL;
    int current_new_face_count = 0;

    dists = (float*)malloc(b->numVertices * sizeof(float));
    side = (int*)malloc(b->numVertices * sizeof(int));

    if (!dists || !side) {
        goto cleanup_and_return;
    }

    int positive_count = 0;
    int negative_count = 0;
    for (int i = 0; i < b->numVertices; ++i) {
        Vec3 world_vertex_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
        dists[i] = vec3_dot(plane_normal, world_vertex_pos) + plane_d;
        if (dists[i] > 1e-5) { side[i] = 1; positive_count++; }
        else if (dists[i] < -1e-5) { side[i] = -1; negative_count++; }
        else { side[i] = 0; }
    }

    if (positive_count == 0 || negative_count == 0) {
        if (positive_count == 0) {
            Brush_FreeData(b);
        }
        goto cleanup_and_return;
    }

    temp_new_verts = (BrushVertex*)malloc(MAX_BRUSH_VERTS * 2 * sizeof(BrushVertex));
    temp_face_verts_idx = (int*)malloc(MAX_BRUSH_VERTS * sizeof(int));
    temp_cap_verts = (BrushVertex*)malloc((MAX_BRUSH_FACES + 1) * sizeof(BrushVertex));
    new_face_list_array = (BrushFace*)malloc(MAX_BRUSH_FACES * sizeof(BrushFace));

    if (!temp_new_verts || !temp_face_verts_idx || !temp_cap_verts || !new_face_list_array) {
        Console_Printf_Error("Brush_Clip: Failed to allocate temporary memory.\n");
        goto cleanup_and_return;
    }

    int new_vert_count = 0;
    int vert_map[MAX_BRUSH_VERTS];
    memset(vert_map, -1, sizeof(vert_map));

    for (int i = 0; i < b->numVertices; ++i) {
        if (side[i] >= 0) {
            if (new_vert_count >= MAX_BRUSH_VERTS * 2) {
                Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_VERTS * 2 for new_verts.\n");
                goto cleanup_and_return;
            }
            vert_map[i] = new_vert_count;
            temp_new_verts[new_vert_count++] = b->vertices[i];
        }
    }

    BrushFace* old_faces = b->faces;
    int old_face_count = b->numFaces;

    for (int i = 0; i < old_face_count; ++i) {
        BrushFace* face = &old_faces[i];
        int face_verts_current_idx_count = 0;

        for (int j = 0; j < face->numVertexIndices; ++j) {
            int p1_idx = face->vertexIndices[j];
            int p2_idx = face->vertexIndices[(j + 1) % face->numVertexIndices];

            if (side[p1_idx] >= 0) {
                if (face_verts_current_idx_count >= MAX_BRUSH_VERTS) {
                    Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_VERTS for temp_face_verts_idx.\n");
                    goto cleanup_and_return;
                }
                temp_face_verts_idx[face_verts_current_idx_count++] = vert_map[p1_idx];
            }

            if (side[p1_idx] * side[p2_idx] < 0) {
                float t = dists[p1_idx] / (dists[p1_idx] - dists[p2_idx]);
                Vec3 intersect_pos = vec3_add(b->vertices[p1_idx].pos, vec3_muls(vec3_sub(b->vertices[p2_idx].pos, b->vertices[p1_idx].pos), t));
                Vec4 intersect_color;
                intersect_color.x = b->vertices[p1_idx].color.x + (b->vertices[p2_idx].color.x - b->vertices[p1_idx].color.x) * t;
                intersect_color.y = b->vertices[p1_idx].color.y + (b->vertices[p2_idx].color.y - b->vertices[p1_idx].color.y) * t;
                intersect_color.z = b->vertices[p1_idx].color.z + (b->vertices[p2_idx].color.z - b->vertices[p1_idx].color.z) * t;
                intersect_color.w = b->vertices[p1_idx].color.w + (b->vertices[p2_idx].color.w - b->vertices[p1_idx].color.w) * t;

                if (face_verts_current_idx_count >= MAX_BRUSH_VERTS) {
                    Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_VERTS for temp_face_verts_idx after adding intersection.\n");
                    goto cleanup_and_return;
                }
                if (new_vert_count >= MAX_BRUSH_VERTS * 2) {
                    Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_VERTS * 2 for temp_new_verts after adding intersection.\n");
                    goto cleanup_and_return;
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
                goto cleanup_and_return;
            }
            BrushFace new_face_entry = *face;
            new_face_entry.numVertexIndices = face_verts_current_idx_count;
            new_face_entry.vertexIndices = (int*)malloc(face_verts_current_idx_count * sizeof(int));
            if (!new_face_entry.vertexIndices) {
                Console_Printf_Error("Brush_Clip: Failed to allocate vertexIndices for new face.\n");
                for (int k = 0; k < current_new_face_count; ++k) free(new_face_list_array[k].vertexIndices);
                goto cleanup_and_return;
            }
            memcpy(new_face_entry.vertexIndices, temp_face_verts_idx, face_verts_current_idx_count * sizeof(int));
            new_face_list_array[current_new_face_count++] = new_face_entry;
        }
    }

    int cap_vert_count = 0;
    for (int i = 0; i < b->numFaces; ++i) {
        BrushFace* face = &b->faces[i];
        for (int j = 0; j < face->numVertexIndices; ++j) {
            int p1_idx = face->vertexIndices[j];
            int p2_idx = face->vertexIndices[(j + 1) % face->numVertexIndices];

            if (side[p1_idx] * side[p2_idx] < 0) {
                float t = dists[p1_idx] / (dists[p1_idx] - dists[p2_idx]);
                Vec3 intersect_pos = vec3_add(b->vertices[p1_idx].pos, vec3_muls(vec3_sub(b->vertices[p2_idx].pos, b->vertices[p1_idx].pos), t));
                Vec4 intersect_color;
                intersect_color.x = b->vertices[p1_idx].color.x + (b->vertices[p2_idx].color.x - b->vertices[p1_idx].color.x) * t;
                intersect_color.y = b->vertices[p1_idx].color.y + (b->vertices[p2_idx].color.y - b->vertices[p1_idx].color.y) * t;
                intersect_color.z = b->vertices[p1_idx].color.z + (b->vertices[p2_idx].color.z - b->vertices[p1_idx].color.z) * t;
                intersect_color.w = b->vertices[p1_idx].color.w + (b->vertices[p2_idx].color.w - b->vertices[p1_idx].color.w) * t;

                bool is_duplicate = false;
                for (int k = 0; k < cap_vert_count; ++k) {
                    if (vec3_length_sq(vec3_sub(temp_cap_verts[k].pos, intersect_pos)) < 1e-6) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (!is_duplicate) {
                    if (cap_vert_count >= MAX_BRUSH_FACES + 1) {
                        Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_FACES for temp_cap_verts.\n");
                        goto cleanup_and_return;
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
        for (int i = 0; i < cap_vert_count; ++i) centroid = vec3_add(centroid, temp_cap_verts[i].pos);
        centroid = vec3_muls(centroid, 1.0f / cap_vert_count);

        g_sort_normal = plane_normal;
        g_sort_centroid = centroid;
        qsort(temp_cap_verts, cap_vert_count, sizeof(BrushVertex), compare_cap_verts);

        if (current_new_face_count >= MAX_BRUSH_FACES) {
            Console_Printf_Error("Brush_Clip: Exceeded MAX_BRUSH_FACES for new_face_list_array (adding cap).\n");
            goto cleanup_and_return;
        }

        BrushFace cap_face;
        cap_face.material = TextureManager_GetMaterial(0);
        cap_face.material2 = NULL;
        cap_face.material3 = NULL;
        cap_face.material4 = NULL;
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
        cap_face.vertexIndices = (int*)malloc(cap_vert_count * sizeof(int));
        if (!cap_face.vertexIndices) {
            Console_Printf_Error("Brush_Clip: Failed to allocate vertexIndices for cap face.\n");
            for (int k = 0; k < current_new_face_count; ++k) free(new_face_list_array[k].vertexIndices);
            goto cleanup_and_return;
        }

        for (int i = 0; i < cap_vert_count; ++i) {
            int vert_idx = -1;
            for (int k = 0; k < new_vert_count; ++k) {
                if (vec3_length_sq(vec3_sub(temp_new_verts[k].pos, temp_cap_verts[i].pos)) < 1e-6) {
                    vert_idx = k;
                    break;
                }
            }
            if (vert_idx == -1) {
                Console_Printf_Error("Brush_Clip: Capping vertex not found in temp_new_verts.\n");
                free(cap_face.vertexIndices);
                for (int k = 0; k < current_new_face_count; ++k) free(new_face_list_array[k].vertexIndices);
                goto cleanup_and_return;
            }
            cap_face.vertexIndices[i] = vert_idx;
        }
        for (int k = 0; k < cap_vert_count / 2; ++k) {
            int temp = cap_face.vertexIndices[k];
            cap_face.vertexIndices[k] = cap_face.vertexIndices[cap_vert_count - 1 - k];
            cap_face.vertexIndices[cap_vert_count - 1 - k] = temp;
        }
        new_face_list_array[current_new_face_count++] = cap_face;
    }

    Brush_FreeData(b);

    b->numVertices = new_vert_count;
    b->vertices = (BrushVertex*)malloc(new_vert_count * sizeof(BrushVertex));
    if (!b->vertices) {
        Console_Printf_Error("Brush_Clip: Failed to allocate final brush vertices.\n");
        for (int k = 0; k < current_new_face_count; ++k) free(new_face_list_array[k].vertexIndices);
        goto cleanup_and_return;
    }
    memcpy(b->vertices, temp_new_verts, new_vert_count * sizeof(BrushVertex));

    b->numFaces = current_new_face_count;
    b->faces = (BrushFace*)malloc(b->numFaces * sizeof(BrushFace));
    if (!b->faces) {
        Console_Printf_Error("Brush_Clip: Failed to allocate final brush faces.\n");
        free(b->vertices); b->vertices = NULL;
        for (int k = 0; k < current_new_face_count; ++k) free(new_face_list_array[k].vertexIndices);
        goto cleanup_and_return;
    }
    memcpy(b->faces, new_face_list_array, current_new_face_count * sizeof(BrushFace));

cleanup_and_return:
    free(dists);
    free(side);
    free(temp_new_verts);
    free(temp_face_verts_idx);
    free(temp_cap_verts);
    free(new_face_list_array);
}