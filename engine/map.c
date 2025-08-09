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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "math_lib.h"
#include "physics_wrapper.h"
#include "cvar.h"
#include "sound_system.h"
#include "io_system.h"
#include "video_player.h"
#include "gl_console.h"
#include "water_manager.h"
#include "mikktspace/mikktspace.h"
#include <float.h>
#include <SDL_image.h>
#include "stb_image.h"

typedef struct {
    Brush* brush;
    int currentFaceIndex;
    int* faceTriangles;
    int numTriangles;
    Vec3* vertexNormals;
} MikkTSpaceUserdata;

static MikkTSpaceUserdata g_mikk_userdata;
static Vec3 g_sort_normal;
static Vec3 g_sort_centroid;

void SceneObject_UpdateMatrix(SceneObject* obj) {
    obj->modelMatrix = create_trs_matrix(obj->pos, obj->rot, obj->scale);
}

void Brush_UpdateMatrix(Brush* b) {
    b->modelMatrix = create_trs_matrix(b->pos, b->rot, b->scale);
}

void Decal_UpdateMatrix(Decal* d) {
    d->modelMatrix = create_trs_matrix(d->pos, d->rot, d->size);
}

void ParallaxRoom_UpdateMatrix(ParallaxRoom* p) {
    p->modelMatrix = create_trs_matrix(p->pos, p->rot, (Vec3) { p->size.x, p->size.y, 1.0f });
}

void Light_InitShadowMap(Light* light) {
    Light_DestroyShadowMap(light);
    glGenFramebuffers(1, &light->shadowFBO);
    glGenTextures(1, &light->shadowMapTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
    int shadow_map_size = Cvar_GetInt("r_shadow_map_size");
    if (shadow_map_size <= 0) {
        shadow_map_size = 1024;
    }
    if (light->type == LIGHT_POINT) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, light->shadowMapTexture);
        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT16, shadow_map_size, shadow_map_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, light->shadowMapTexture, 0);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, light->shadowMapTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, shadow_map_size, shadow_map_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, light->shadowMapTexture, 0);
    }
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Console_Printf("Shadow Framebuffer not complete! Light Type: %d\n", light->type);

    light->shadowMapHandle = glGetTextureHandleARB(light->shadowMapTexture);
    glMakeTextureHandleResidentARB(light->shadowMapHandle);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Calculate_Sun_Light_Space_Matrix(Mat4* outMatrix, const Sun* sun, Vec3 cameraPosition) {
    const float SUN_SHADOW_MAP_SIZE_F = 4096.0f;

    float shadowOrthoSize = Cvar_GetFloat("r_sun_shadow_distance");

    float near_plane = 1.0f;
    float far_plane = shadowOrthoSize * 4.0f;

    Vec3 lightFocusPos = cameraPosition;
    Vec3 lightPos = vec3_sub(lightFocusPos, vec3_muls(sun->direction, far_plane * 0.5f));

    Mat4 lightProjection = mat4_ortho(-shadowOrthoSize, shadowOrthoSize, -shadowOrthoSize, shadowOrthoSize, near_plane, far_plane);
    Mat4 lightView = mat4_lookAt(lightPos, lightFocusPos, (Vec3) { 0.0f, 1.0f, 0.0f });

    Mat4 initialLightSpaceMatrix;
    mat4_multiply(&initialLightSpaceMatrix, &lightProjection, &lightView);

    Vec4 shadowOrigin = mat4_mul_vec4(&initialLightSpaceMatrix, (Vec4) { 0.0f, 0.0f, 0.0f, 1.0f });

    shadowOrigin.x *= (SUN_SHADOW_MAP_SIZE_F / 2.0f);
    shadowOrigin.y *= (SUN_SHADOW_MAP_SIZE_F / 2.0f);

    Vec4 roundedOrigin = { roundf(shadowOrigin.x), roundf(shadowOrigin.y), roundf(shadowOrigin.z), roundf(shadowOrigin.w) };

    Vec4 roundOffset;
    roundOffset.x = (roundedOrigin.x - shadowOrigin.x) * (2.0f / SUN_SHADOW_MAP_SIZE_F);
    roundOffset.y = (roundedOrigin.y - shadowOrigin.y) * (2.0f / SUN_SHADOW_MAP_SIZE_F);
    roundOffset.z = 0.0f;
    roundOffset.w = 0.0f;

    lightProjection.m[12] += roundOffset.x;
    lightProjection.m[13] += roundOffset.y;

    mat4_multiply(outMatrix, &lightProjection, &lightView);
}

void Light_DestroyShadowMap(Light* light) {
    if (light->shadowMapHandle) {
        glMakeTextureHandleNonResidentARB(light->shadowMapHandle);
        light->shadowMapHandle = 0;
    }
    if (light->shadowFBO) {
        glDeleteFramebuffers(1, &light->shadowFBO);
        light->shadowFBO = 0;
    }
    if (light->shadowMapTexture) {
        glDeleteTextures(1, &light->shadowMapTexture);
        light->shadowMapTexture = 0;
    }
}

void Brush_FreeData(Brush* b) {
    if (!b) return;
    if (b->vertices) {
        free(b->vertices);
        b->vertices = NULL;
    }
    b->numVertices = 0;
    if (b->faces) {
        for (int i = 0; i < b->numFaces; ++i) {
            if (b->faces[i].vertexIndices) {
                free(b->faces[i].vertexIndices);
            }
        }
        free(b->faces);
        b->faces = NULL;
    }
    b->numFaces = 0;
    if (b->vao) {
        glDeleteVertexArrays(1, &b->vao);
        b->vao = 0;
    }
    if (b->vbo) {
        glDeleteBuffers(1, &b->vbo);
        b->vbo = 0;
    }
}

void Brush_DeepCopy(Brush* dest, const Brush* src) {
    Brush_FreeData(dest);

    dest->pos = src->pos;
    dest->rot = src->rot;
    dest->scale = src->scale;
    dest->modelMatrix = src->modelMatrix;
    strcpy(dest->targetname, src->targetname);
    dest->cubemapTexture = src->cubemapTexture;
    strcpy(dest->name, src->name);
    strcpy(dest->classname, src->classname);
    dest->numProperties = src->numProperties;
    memcpy(dest->properties, src->properties, sizeof(KeyValue) * MAX_ENTITY_PROPERTIES);
    dest->numVertices = src->numVertices;
    if (src->numVertices > 0) {
        dest->vertices = malloc(src->numVertices * sizeof(BrushVertex));
        memcpy(dest->vertices, src->vertices, src->numVertices * sizeof(BrushVertex));
    }
    else {
        dest->vertices = NULL;
    }

    dest->numFaces = src->numFaces;
    if (src->numFaces > 0) {
        dest->faces = malloc(src->numFaces * sizeof(BrushFace));
        for (int i = 0; i < src->numFaces; ++i) {
            dest->faces[i] = src->faces[i];
            dest->faces[i].vertexIndices = NULL;
            if (src->faces[i].numVertexIndices > 0) {
                dest->faces[i].vertexIndices = malloc(src->faces[i].numVertexIndices * sizeof(int));
                memcpy(dest->faces[i].vertexIndices, src->faces[i].vertexIndices, src->faces[i].numVertexIndices * sizeof(int));
            }
        }
    }
    else {
        dest->faces = NULL;
    }
}

void Brush_SetVerticesFromBox(Brush* b, Vec3 size) {
    Brush_FreeData(b);
    b->numVertices = 8;
    b->vertices = malloc(b->numVertices * sizeof(BrushVertex));
    Vec3 half_size = vec3_muls(size, 0.5f);
    b->vertices[0].pos = (Vec3){ -half_size.x, -half_size.y,  half_size.z };
    b->vertices[1].pos = (Vec3){ half_size.x, -half_size.y,  half_size.z };
    b->vertices[2].pos = (Vec3){ half_size.x,  half_size.y,  half_size.z };
    b->vertices[3].pos = (Vec3){ -half_size.x,  half_size.y,  half_size.z };
    b->vertices[4].pos = (Vec3){ -half_size.x, -half_size.y, -half_size.z };
    b->vertices[5].pos = (Vec3){ half_size.x, -half_size.y, -half_size.z };
    b->vertices[6].pos = (Vec3){ half_size.x,  half_size.y, -half_size.z };
    b->vertices[7].pos = (Vec3){ -half_size.x,  half_size.y, -half_size.z };

    for (int i = 0; i < 8; ++i) {
        b->vertices[i].color = (Vec4){ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = 6;
    b->faces = malloc(b->numFaces * sizeof(BrushFace));
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
        b->faces[i].uv_offset = (Vec2){ 0,0 };
        b->faces[i].uv_scale = (Vec2){ 1,1 };
        b->faces[i].uv_rotation = 0;
        b->faces[i].uv_offset2 = (Vec2){ 0,0 };
        b->faces[i].uv_scale2 = (Vec2){ 1,1 };
        b->faces[i].uv_rotation2 = 0;
        b->faces[i].material3 = NULL;
        b->faces[i].uv_offset3 = (Vec2){ 0,0 };
        b->faces[i].uv_scale3 = (Vec2){ 1,1 };
        b->faces[i].uv_rotation3 = 0;
        b->faces[i].material4 = NULL;
        b->faces[i].uv_offset4 = (Vec2){ 0,0 };
        b->faces[i].uv_scale4 = (Vec2){ 1,1 };
        b->faces[i].uv_rotation4 = 0;
        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].numVertexIndices = 4;
        b->faces[i].vertexIndices = malloc(4 * sizeof(int));
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
    b->vertices = malloc(b->numVertices * sizeof(BrushVertex));

    for (int i = 0; i < num_sides; ++i) {
        float angle = (float)i / (float)num_sides * 2.0f * M_PI;
        float x = cosf(angle) * radius_x;
        float z = sinf(angle) * radius_z;

        b->vertices[i].pos = (Vec3){ x, height / 2.0f, z };
        b->vertices[i + num_sides].pos = (Vec3){ x, -height / 2.0f, z };
    }

    for (int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = (Vec4){ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = num_sides + 2;
    b->faces = malloc(b->numFaces * sizeof(BrushFace));

    for (int i = 0; i < num_sides; ++i) {
        b->faces[i] = (BrushFace){ .material = TextureManager_GetMaterial(0), .numVertexIndices = 4, .uv_scale = {1,1} };
        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].vertexIndices = malloc(4 * sizeof(int));
        b->faces[i].vertexIndices[0] = i;
        b->faces[i].vertexIndices[1] = (i + 1) % num_sides;
        b->faces[i].vertexIndices[2] = ((i + 1) % num_sides) + num_sides;
        b->faces[i].vertexIndices[3] = i + num_sides;
    }

    b->faces[num_sides] = (BrushFace){ .material = TextureManager_GetMaterial(0), .numVertexIndices = num_sides, .uv_scale = {1,1} };
    b->faces[num_sides].lightmap_scale = 1.0f;
    b->faces[num_sides].vertexIndices = malloc(num_sides * sizeof(int));
    for (int i = 0; i < num_sides; ++i) b->faces[num_sides].vertexIndices[i] = i;

    b->faces[num_sides + 1] = (BrushFace){ .material = TextureManager_GetMaterial(0), .numVertexIndices = num_sides, .uv_scale = {1,1} };
    b->faces[num_sides + 1].lightmap_scale = 1.0f;
    b->faces[num_sides + 1].vertexIndices = malloc(num_sides * sizeof(int));
    for (int i = 0; i < num_sides; ++i) b->faces[num_sides + 1].vertexIndices[i] = (num_sides - 1 - i) + num_sides;
}

void Brush_SetVerticesFromWedge(Brush* b, Vec3 size) {
    Brush_FreeData(b);
    Vec3 half_size = vec3_muls(size, 0.5f);

    b->numVertices = 6;
    b->vertices = malloc(b->numVertices * sizeof(BrushVertex));

    b->vertices[0].pos = (Vec3){ -half_size.x, -half_size.y, -half_size.z };
    b->vertices[1].pos = (Vec3){ half_size.x, -half_size.y, -half_size.z };
    b->vertices[2].pos = (Vec3){ half_size.x, -half_size.y,  half_size.z };
    b->vertices[3].pos = (Vec3){ -half_size.x, -half_size.y,  half_size.z };
    b->vertices[4].pos = (Vec3){ -half_size.x,  half_size.y, -half_size.z };
    b->vertices[5].pos = (Vec3){ half_size.x,  half_size.y, -half_size.z };

    for (int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = (Vec4){ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = 5;
    b->faces = malloc(b->numFaces * sizeof(BrushFace));

    int face_defs[5][4] = {
        {0, 3, 2, 1},
        {0, 1, 5, 4},
        {3, 2, 5, 4},
        {0, 4, 3, -1},
        {1, 2, 5, -1}
    };
    int num_indices_per_face[] = { 4, 4, 4, 3, 3 };

    for (int i = 0; i < 5; ++i) {
        b->faces[i] = (BrushFace){ .material = TextureManager_GetMaterial(0), .numVertexIndices = num_indices_per_face[i], .uv_scale = {1,1} };
        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].vertexIndices = malloc(b->faces[i].numVertexIndices * sizeof(int));
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
    b->vertices = malloc(b->numVertices * sizeof(BrushVertex));

    b->vertices[0].pos = (Vec3){ 0, height / 2.0f, 0 };

    for (int i = 0; i < num_sides; ++i) {
        float angle = (float)i / (float)num_sides * 2.0f * M_PI;
        float x = cosf(angle) * radius_x;
        float z = sinf(angle) * radius_z;
        b->vertices[i + 1].pos = (Vec3){ x, -height / 2.0f, z };
    }

    for (int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = (Vec4){ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = num_sides + 1;
    b->faces = malloc(b->numFaces * sizeof(BrushFace));

    for (int i = 0; i < num_sides; ++i) {
        b->faces[i] = (BrushFace){ .material = TextureManager_GetMaterial(0), .numVertexIndices = 3, .uv_scale = {1,1} };
        b->faces[i].vertexIndices = malloc(3 * sizeof(int));
        b->faces[i].lightmap_scale = 1.0f;
        b->faces[i].vertexIndices[0] = 0;
        b->faces[i].vertexIndices[1] = (i + 1) % num_sides + 1;
        b->faces[i].vertexIndices[2] = i + 1;
    }

    b->faces[num_sides] = (BrushFace){ .material = TextureManager_GetMaterial(0), .numVertexIndices = num_sides, .uv_scale = {1,1} };
    b->faces[num_sides].lightmap_scale = 1.0f;
    b->faces[num_sides].vertexIndices = malloc(num_sides * sizeof(int));
    for (int i = 0; i < num_sides; ++i) {
        b->faces[num_sides].vertexIndices[i] = (num_sides - i) + 0;
    }
}

void Brush_SetVerticesFromSphere(Brush* b, Vec3 size, int sides) {
    Brush_FreeData(b);
    int stacks = sides / 2;
    b->numVertices = (sides + 1) * (stacks + 1);
    b->vertices = calloc(b->numVertices, sizeof(BrushVertex));

    Vec3 radius = vec3_muls(size, 0.5f);

    for (int i = 0; i <= stacks; i++) {
        float stack_angle = M_PI / 2 - i * M_PI / stacks;
        float xy = radius.x * cosf(stack_angle);
        float z = radius.z * sinf(stack_angle);

        for (int j = 0; j <= sides; j++) {
            float sector_angle = j * 2 * M_PI / sides;
            float x = xy * cosf(sector_angle);
            float y = xy * sinf(sector_angle);
            b->vertices[i * (sides + 1) + j].pos = (Vec3){ x, y, z };
        }
    }

    b->numFaces = sides * stacks;
    b->faces = calloc(b->numFaces, sizeof(BrushFace));
    int face_index = 0;
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < sides; j++) {
            int p1 = i * (sides + 1) + j;
            int p2 = p1 + 1;
            int p3 = (i + 1) * (sides + 1) + j;
            int p4 = p3 + 1;

            b->faces[face_index].numVertexIndices = 4;
            b->faces[face_index].vertexIndices = malloc(4 * sizeof(int));
            b->faces[face_index].vertexIndices[0] = p1;
            b->faces[face_index].vertexIndices[1] = p3;
            b->faces[face_index].vertexIndices[2] = p4;
            b->faces[face_index].vertexIndices[3] = p2;

            b->faces[face_index].material = TextureManager_GetMaterial(0);
            b->faces[face_index].uv_scale = (Vec2){ 1,1 };
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
    b->vertices = calloc(b->numVertices, sizeof(BrushVertex));

    Vec3 radius = vec3_muls(size, 0.5f);

    for (int i = 0; i <= stacks; i++) {
        float stack_angle = M_PI / 2 - i * (M_PI / 2) / stacks;
        float xy = radius.x * cosf(stack_angle);
        float z = radius.z * sinf(stack_angle);

        for (int j = 0; j <= sides; j++) {
            float sector_angle = j * 2 * M_PI / sides;
            float x = xy * cosf(sector_angle);
            float y = xy * sinf(sector_angle);
            b->vertices[i * ring_vertices + j].pos = (Vec3){ x, y, z };
        }
    }

    int bottom_center_index = b->numVertices - 1;
    float bottom_z = b->vertices[stacks * ring_vertices].pos.z;
    b->vertices[bottom_center_index].pos = (Vec3){ 0, 0, bottom_z };

    b->numFaces = (sides * stacks) + sides;
    b->faces = calloc(b->numFaces, sizeof(BrushFace));

    int face_index = 0;

    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < sides; j++) {
            int p1 = i * ring_vertices + j;
            int p2 = p1 + 1;
            int p3 = (i + 1) * ring_vertices + j;
            int p4 = p3 + 1;

            b->faces[face_index].numVertexIndices = 4;
            b->faces[face_index].vertexIndices = malloc(4 * sizeof(int));
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
        b->faces[face_index].vertexIndices = malloc(3 * sizeof(int));
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
        b->faces[i].uv_scale = (Vec2){ 1,1 };
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
    b->vertices = malloc(b->numVertices * sizeof(BrushVertex));

    for (int i = 0; i < num_sides; ++i) {
        float angle = (float)i / (float)num_sides * 2.0f * M_PI;
        float cos_a = cosf(angle);
        float sin_a = sinf(angle);

        b->vertices[i].pos = (Vec3){ cos_a * radius_x, height / 2.0f, sin_a * radius_z };
        b->vertices[i + num_sides].pos = (Vec3){ cos_a * radius_x, -height / 2.0f, sin_a * radius_z };
        b->vertices[i + 2 * num_sides].pos = (Vec3){ cos_a * inner_radius_x, height / 2.0f, sin_a * inner_radius_z };
        b->vertices[i + 3 * num_sides].pos = (Vec3){ cos_a * inner_radius_x, -height / 2.0f, sin_a * inner_radius_z };
    }

    for (int i = 0; i < b->numVertices; ++i) {
        b->vertices[i].color = (Vec4){ 0.0f, 0.0f, 0.0f, 1.0f };
    }

    b->numFaces = num_sides * 4;
    b->faces = malloc(b->numFaces * sizeof(BrushFace));

    for (int i = 0; i < num_sides; ++i) {
        int next_i = (i + 1) % num_sides;

        int face_idx = i;
        b->faces[face_idx] = (BrushFace){ .material = TextureManager_GetMaterial(0), .numVertexIndices = 4, .uv_scale = {1,1}, .lightmap_scale = 1.0f };
        b->faces[face_idx].vertexIndices = malloc(4 * sizeof(int));
        b->faces[face_idx].vertexIndices[0] = i;
        b->faces[face_idx].vertexIndices[1] = next_i;
        b->faces[face_idx].vertexIndices[2] = next_i + num_sides;
        b->faces[face_idx].vertexIndices[3] = i + num_sides;

        face_idx = i + num_sides;
        b->faces[face_idx] = (BrushFace){ .material = TextureManager_GetMaterial(0), .numVertexIndices = 4, .uv_scale = {1,1}, .lightmap_scale = 1.0f };
        b->faces[face_idx].vertexIndices = malloc(4 * sizeof(int));
        b->faces[face_idx].vertexIndices[0] = next_i + 2 * num_sides;
        b->faces[face_idx].vertexIndices[1] = i + 2 * num_sides;
        b->faces[face_idx].vertexIndices[2] = i + 3 * num_sides;
        b->faces[face_idx].vertexIndices[3] = next_i + 3 * num_sides;

        face_idx = i + 2 * num_sides;
        b->faces[face_idx] = (BrushFace){ .material = TextureManager_GetMaterial(0), .numVertexIndices = 4, .uv_scale = {1,1}, .lightmap_scale = 1.0f };
        b->faces[face_idx].vertexIndices = malloc(4 * sizeof(int));
        b->faces[face_idx].vertexIndices[0] = next_i;
        b->faces[face_idx].vertexIndices[1] = i;
        b->faces[face_idx].vertexIndices[2] = i + 2 * num_sides;
        b->faces[face_idx].vertexIndices[3] = next_i + 2 * num_sides;

        face_idx = i + 3 * num_sides;
        b->faces[face_idx] = (BrushFace){ .material = TextureManager_GetMaterial(0), .numVertexIndices = 4, .uv_scale = {1,1}, .lightmap_scale = 1.0f };
        b->faces[face_idx].vertexIndices = malloc(4 * sizeof(int));
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

    Vec3 u_axis = vec3_cross(g_sort_normal, (Vec3) { 0, 0, 1 });
    if (vec3_length_sq(u_axis) < 1e-6) u_axis = vec3_cross(g_sort_normal, (Vec3) { 0, 1, 0 });
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

    float* dists = (float*)malloc(b->numVertices * sizeof(float));
    int* side = (int*)malloc(b->numVertices * sizeof(int));
    if (!dists || !side) { free(dists); free(side); return; }

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
        free(dists);
        free(side);
        if (positive_count == 0) {
            Brush_FreeData(b);
        }
        return;
    }

    BrushVertex* temp_new_verts = (BrushVertex*)malloc(MAX_BRUSH_VERTS * 2 * sizeof(BrushVertex));
    int* temp_face_verts_idx = (int*)malloc(MAX_BRUSH_VERTS * sizeof(int));
    BrushVertex* temp_cap_verts = (BrushVertex*)malloc((MAX_BRUSH_FACES + 1) * sizeof(BrushVertex));

    BrushFace* new_face_list_array = (BrushFace*)malloc(MAX_BRUSH_FACES * sizeof(BrushFace));

    if (!temp_new_verts || !temp_face_verts_idx || !temp_cap_verts || !new_face_list_array) {
        fprintf(stderr, "Brush_Clip: Failed to allocate temporary memory.\n");
        free(dists); free(side);
        free(temp_new_verts); free(temp_face_verts_idx); free(temp_cap_verts); free(new_face_list_array);
        return;
    }

    int new_vert_count = 0;
    int vert_map[MAX_BRUSH_VERTS];
    memset(vert_map, -1, sizeof(vert_map));

    for (int i = 0; i < b->numVertices; ++i) {
        if (side[i] >= 0) {
            if (new_vert_count >= MAX_BRUSH_VERTS * 2) {
                fprintf(stderr, "Brush_Clip: Exceeded MAX_BRUSH_VERTS * 2 for new_verts.\n");
                goto cleanup_and_return;
            }
            vert_map[i] = new_vert_count;
            temp_new_verts[new_vert_count++] = b->vertices[i];
        }
    }

    int current_new_face_count = 0;
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
                    fprintf(stderr, "Brush_Clip: Exceeded MAX_BRUSH_VERTS for temp_face_verts_idx.\n");
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
                    fprintf(stderr, "Brush_Clip: Exceeded MAX_BRUSH_VERTS for temp_face_verts_idx after adding intersection.\n");
                    goto cleanup_and_return;
                }
                if (new_vert_count >= MAX_BRUSH_VERTS * 2) {
                    fprintf(stderr, "Brush_Clip: Exceeded MAX_BRUSH_VERTS * 2 for temp_new_verts after adding intersection.\n");
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
                fprintf(stderr, "Brush_Clip: Exceeded MAX_BRUSH_FACES for new_face_list_array.\n");
                goto cleanup_and_return;
            }
            BrushFace new_face_entry = *face;
            new_face_entry.numVertexIndices = face_verts_current_idx_count;
            new_face_entry.vertexIndices = (int*)malloc(face_verts_current_idx_count * sizeof(int));
            if (!new_face_entry.vertexIndices) {
                fprintf(stderr, "Brush_Clip: Failed to allocate vertexIndices for new face.\n");
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
                        fprintf(stderr, "Brush_Clip: Exceeded MAX_BRUSH_FACES for temp_cap_verts.\n");
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
            fprintf(stderr, "Brush_Clip: Exceeded MAX_BRUSH_FACES for new_face_list_array (adding cap).\n");
            goto cleanup_and_return;
        }

        BrushFace cap_face;
        cap_face.material = TextureManager_GetMaterial(0);
        cap_face.material2 = NULL;
        cap_face.material3 = NULL;
        cap_face.material4 = NULL;
        cap_face.uv_offset = (Vec2){ 0, 0 };
        cap_face.uv_scale = (Vec2){ 1, 1 };
        cap_face.uv_rotation = 0;
        cap_face.uv_offset2 = (Vec2){ 0, 0 };
        cap_face.uv_scale2 = (Vec2){ 1, 1 };
        cap_face.uv_rotation2 = 0;
        cap_face.uv_offset3 = (Vec2){ 0, 0 };
        cap_face.uv_scale3 = (Vec2){ 1, 1 };
        cap_face.uv_rotation3 = 0;
        cap_face.uv_offset4 = (Vec2){ 0, 0 };
        cap_face.uv_scale4 = (Vec2){ 1, 1 };
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

bool Brush_IsSolid(const Brush* b) {
    if (!b) return false;

    if (strlen(b->classname) > 0) {
        if (strcmp(b->classname, "env_glass") == 0) {
            return true;
        }
        return false;
    }
    return true;
}

static int getNumFaces(const SMikkTSpaceContext* pContext) {
    return g_mikk_userdata.numTriangles;
}

static int getNumVerticesOfFace(const SMikkTSpaceContext* pContext, const int iFace) {
    return 3;
}

static void getPosition(const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert) {
    int vertex_index = g_mikk_userdata.faceTriangles[iFace * 3 + iVert];
    Vec3 pos = g_mikk_userdata.brush->vertices[vertex_index].pos;
    fvPosOut[0] = pos.x;
    fvPosOut[1] = pos.y;
    fvPosOut[2] = pos.z;
}

static void getNormal(const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert) {
    int vertex_index = g_mikk_userdata.faceTriangles[iFace * 3 + iVert];
    Vec3 n = g_mikk_userdata.vertexNormals[vertex_index];
    fvNormOut[0] = n.x;
    fvNormOut[1] = n.y;
    fvNormOut[2] = n.z;
}

static void getTexCoord(const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert) {
    int v_idx0 = g_mikk_userdata.faceTriangles[iFace * 3 + 0];
    int v_idx1 = g_mikk_userdata.faceTriangles[iFace * 3 + 1];
    int v_idx2 = g_mikk_userdata.faceTriangles[iFace * 3 + 2];
    Vec3 p0 = g_mikk_userdata.brush->vertices[v_idx0].pos;
    Vec3 p1 = g_mikk_userdata.brush->vertices[v_idx1].pos;
    Vec3 p2 = g_mikk_userdata.brush->vertices[v_idx2].pos;
    Vec3 normal_vec = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
    vec3_normalize(&normal_vec);

    int vertex_index = g_mikk_userdata.faceTriangles[iFace * 3 + iVert];
    Vec3 pos = g_mikk_userdata.brush->vertices[vertex_index].pos;
    BrushFace* face = &g_mikk_userdata.brush->faces[g_mikk_userdata.currentFaceIndex];

    float absX = fabsf(normal_vec.x), absY = fabsf(normal_vec.y), absZ = fabsf(normal_vec.z);
    int dominant_axis = (absY > absX && absY > absZ) ? 1 : ((absX > absZ) ? 0 : 2);

    float u, v;
    if (dominant_axis == 0) { u = pos.y; v = pos.z; }
    else if (dominant_axis == 1) { u = pos.x; v = pos.z; }
    else { u = pos.x; v = pos.y; }

    float rad = face->uv_rotation * (M_PI / 180.0f);
    float cos_r = cosf(rad); float sin_r = sinf(rad);
    fvTexcOut[0] = ((u * cos_r - v * sin_r) / face->uv_scale.x) + face->uv_offset.x;
    fvTexcOut[1] = ((u * sin_r + v * cos_r) / face->uv_scale.y) + face->uv_offset.y;
}

static void setTSpaceBasic(const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
    int vbo_idx = (iFace * 3 + iVert) * 22;
    float* vbo_data = (float*)pContext->m_pUserData;
    vbo_data[vbo_idx + 8] = fvTangent[0];
    vbo_data[vbo_idx + 9] = fvTangent[1];
    vbo_data[vbo_idx + 10] = fvTangent[2];
    vbo_data[vbo_idx + 11] = fSign;
}

void SceneObject_LoadVertexLighting(SceneObject* obj, int index, const char* mapPath) {
    if (!obj->model || obj->model->totalVertexCount == 0) return;

    char map_name_sanitized[128];
    char* dot = strrchr(mapPath, '.');
    if (dot) {
        size_t len = dot - mapPath;
        strncpy(map_name_sanitized, mapPath, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, mapPath);
    }

    char model_name_sanitized[128];
    if (strlen(obj->targetname) > 0) {
        sanitize_filename_map(obj->targetname, model_name_sanitized, sizeof(model_name_sanitized));
    }
    else {
        sprintf(model_name_sanitized, "Model_%d", index);
    }

    char vlm_path[512];
    snprintf(vlm_path, sizeof(vlm_path), "lightmaps/%s/%s/vertex_colors.vlm", map_name_sanitized, model_name_sanitized);

    FILE* file = fopen(vlm_path, "rb");
    if (file) {
        char header[4];
        unsigned int vertex_count;
        fread(header, 1, 4, file);
        fread(&vertex_count, sizeof(unsigned int), 1, file);

        if (strncmp(header, "VLM1", 4) == 0 && vertex_count == obj->model->totalVertexCount) {
            obj->bakedVertexColors = malloc(vertex_count * sizeof(Vec4));
            if (obj->bakedVertexColors) {
                fread(obj->bakedVertexColors, sizeof(Vec4), vertex_count, file);
            }
        }
        else {
            Console_Printf_Warning("VLM file '%s' is invalid or vertex count mismatch.", vlm_path);
        }
        fclose(file);
    }
}

void SceneObject_LoadVertexDirectionalLighting(SceneObject* obj, int index, const char* mapPath) {
    if (!obj->model || obj->model->totalVertexCount == 0) return;

    char map_name_sanitized[128];
    char* dot = strrchr(mapPath, '.');
    if (dot) {
        size_t len = dot - mapPath;
        strncpy(map_name_sanitized, mapPath, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, mapPath);
    }

    char model_name_sanitized[128];
    if (strlen(obj->targetname) > 0) {
        sanitize_filename_map(obj->targetname, model_name_sanitized, sizeof(model_name_sanitized));
    }
    else {
        sprintf(model_name_sanitized, "Model_%d", index);
    }

    char vld_path[512];
    snprintf(vld_path, sizeof(vld_path), "lightmaps/%s/%s/vertex_directions.vld", map_name_sanitized, model_name_sanitized);

    FILE* file = fopen(vld_path, "rb");
    if (file) {
        char header[4];
        unsigned int vertex_count;
        fread(header, 1, 4, file);
        fread(&vertex_count, sizeof(unsigned int), 1, file);

        if (strncmp(header, "VLD1", 4) == 0 && vertex_count == obj->model->totalVertexCount) {
            obj->bakedVertexDirections = malloc(vertex_count * sizeof(Vec4));
            if (obj->bakedVertexDirections) {
                fread(obj->bakedVertexDirections, sizeof(Vec4), vertex_count, file);
            }
        }
        else {
            Console_Printf_Warning("VLD file '%s' is invalid or vertex count mismatch.", vld_path);
        }
        fclose(file);
    }
}

static Vec2 calculate_texture_uv_for_vertex(const Brush* b, int face_index, int vertex_index) {
    BrushFace* face = &b->faces[face_index];
    Vec3 pos = b->vertices[vertex_index].pos;

    Vec3 p0 = b->vertices[face->vertexIndices[0]].pos;
    Vec3 p1 = b->vertices[face->vertexIndices[1]].pos;
    Vec3 p2 = b->vertices[face->vertexIndices[2]].pos;
    Vec3 normal_vec = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
    vec3_normalize(&normal_vec);

    float absX = fabsf(normal_vec.x), absY = fabsf(normal_vec.y), absZ = fabsf(normal_vec.z);
    int dominant_axis = (absY > absX && absY > absZ) ? 1 : ((absX > absZ) ? 0 : 2);

    float u, v;
    if (dominant_axis == 0) { u = pos.y; v = pos.z; }
    else if (dominant_axis == 1) { u = pos.x; v = pos.z; }
    else { u = pos.x; v = pos.y; }

    float rad = face->uv_rotation * (M_PI / 180.0f);
    float cos_r = cosf(rad); float sin_r = sinf(rad);

    Vec2 final_uv;
    final_uv.x = ((u * cos_r - v * sin_r) / face->uv_scale.x) + face->uv_offset.x;
    final_uv.y = ((u * sin_r + v * cos_r) / face->uv_scale.y) + face->uv_offset.y;

    return final_uv;
}

void Brush_CreateRenderData(Brush* b) {
    if (b->numFaces == 0 || b->numVertices == 0) {
        b->totalRenderVertexCount = 0;
        return;
    }

    Vec3* temp_normals = (Vec3*)calloc(b->numVertices, sizeof(Vec3));
    if (!temp_normals) return;

    for (int i = 0; i < b->numFaces; ++i) {
        BrushFace* face = &b->faces[i];
        if (face->numVertexIndices < 3) continue;
        for (int j = 0; j < face->numVertexIndices - 2; ++j) {
            int idx0 = face->vertexIndices[0];
            int idx1 = face->vertexIndices[j + 1];
            int idx2 = face->vertexIndices[j + 2];
            Vec3 p0 = b->vertices[idx0].pos;
            Vec3 p1 = b->vertices[idx1].pos;
            Vec3 p2 = b->vertices[idx2].pos;
            Vec3 face_normal = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
            temp_normals[idx0] = vec3_add(temp_normals[idx0], face_normal);
            temp_normals[idx1] = vec3_add(temp_normals[idx1], face_normal);
            temp_normals[idx2] = vec3_add(temp_normals[idx2], face_normal);
        }
    }
    for (int i = 0; i < b->numVertices; ++i) {
        vec3_normalize(&temp_normals[i]);
    }

    int total_render_verts = 0;
    for (int i = 0; i < b->numFaces; ++i) {
        if (b->faces[i].numVertexIndices >= 3) {
            total_render_verts += (b->faces[i].numVertexIndices - 2) * 3;
        }
    }
    b->totalRenderVertexCount = total_render_verts;
    if (total_render_verts == 0) {
        free(temp_normals);
        return;
    }

    const int stride_floats = 24;
    float* final_vbo_data = calloc(total_render_verts * stride_floats, sizeof(float));
    if (!final_vbo_data) {
        free(temp_normals);
        return;
    }

    SMikkTSpaceInterface mikk_interface = { 0 };
    mikk_interface.m_getNumFaces = getNumFaces;
    mikk_interface.m_getNumVerticesOfFace = getNumVerticesOfFace;
    mikk_interface.m_getPosition = getPosition;
    mikk_interface.m_getNormal = getNormal;
    mikk_interface.m_getTexCoord = getTexCoord;
    mikk_interface.m_setTSpaceBasic = setTSpaceBasic;

    int atlas_width = 1, atlas_height = 1;
    if (b->lightmapAtlas != 0) {
        glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &atlas_width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &atlas_height);
    }

    int vbo_vertex_offset = 0;
    for (int i = 0; i < b->numFaces; ++i) {
        BrushFace* face = &b->faces[i];
        if (face->numVertexIndices < 3) continue;

        Vec2 min_uv = { FLT_MAX, FLT_MAX };
        Vec2 max_uv = { -FLT_MAX, -FLT_MAX };
        for (int k = 0; k < face->numVertexIndices; k++) {
            Vec2 uv = calculate_texture_uv_for_vertex(b, i, face->vertexIndices[k]);
            min_uv.x = fminf(min_uv.x, uv.x);
            min_uv.y = fminf(min_uv.y, uv.y);
            max_uv.x = fmaxf(max_uv.x, uv.x);
            max_uv.y = fmaxf(max_uv.y, uv.y);
        }
        Vec2 uv_range = { max_uv.x - min_uv.x, max_uv.y - min_uv.y };
        if (uv_range.x < 0.001f) uv_range.x = 1.0f;
        if (uv_range.y < 0.001f) uv_range.y = 1.0f;

        int num_tris_in_face = face->numVertexIndices - 2;
        int num_verts_in_face = num_tris_in_face * 3;

        int* face_tri_indices = malloc(num_verts_in_face * sizeof(int));
        for (int j = 0; j < num_tris_in_face; ++j) {
            face_tri_indices[j * 3 + 0] = face->vertexIndices[0];
            face_tri_indices[j * 3 + 1] = face->vertexIndices[j + 1];
            face_tri_indices[j * 3 + 2] = face->vertexIndices[j + 2];
        }

        g_mikk_userdata.brush = b;
        g_mikk_userdata.currentFaceIndex = i;
        g_mikk_userdata.faceTriangles = face_tri_indices;
        g_mikk_userdata.numTriangles = num_tris_in_face;
        g_mikk_userdata.vertexNormals = temp_normals;

        SMikkTSpaceContext mikk_context = { 0 };
        mikk_context.m_pInterface = &mikk_interface;
        mikk_context.m_pUserData = (void*)(final_vbo_data + vbo_vertex_offset * stride_floats);
        genTangSpaceDefault(&mikk_context);

        for (int j = 0; j < num_verts_in_face; ++j) {
            int vbo_idx = (vbo_vertex_offset + j) * stride_floats;
            int vertex_index = face_tri_indices[j];
            BrushVertex vert = b->vertices[vertex_index];
            Vec3 norm = temp_normals[vertex_index];
            float uv1[2], uv2[2], uv3[2], uv4[2];

            getTexCoord(NULL, uv1, j / 3, j % 3);
            Vec3 p0 = b->vertices[face_tri_indices[j - (j % 3) + 0]].pos;
            Vec3 p1 = b->vertices[face_tri_indices[j - (j % 3) + 1]].pos;
            Vec3 p2 = b->vertices[face_tri_indices[j - (j % 3) + 2]].pos;
            Vec3 normal_vec = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0)); vec3_normalize(&normal_vec);
            float absX = fabsf(normal_vec.x), absY = fabsf(normal_vec.y), absZ = fabsf(normal_vec.z);
            int dominant_axis = (absY > absX && absY > absZ) ? 1 : ((absX > absZ) ? 0 : 2);
            float u, v;
            if (dominant_axis == 0) { u = vert.pos.y; v = vert.pos.z; }
            else if (dominant_axis == 1) { u = vert.pos.x; v = vert.pos.z; }
            else { u = vert.pos.x; v = vert.pos.y; }
            float rad2 = face->uv_rotation2 * (M_PI / 180.0f); float cos_r2 = cosf(rad2); float sin_r2 = sinf(rad2);
            uv2[0] = ((u * cos_r2 - v * sin_r2) / face->uv_scale2.x) + face->uv_offset2.x; uv2[1] = ((u * sin_r2 + v * cos_r2) / face->uv_scale2.y) + face->uv_offset2.y;
            float rad3 = face->uv_rotation3 * (M_PI / 180.0f); float cos_r3 = cosf(rad3); float sin_r3 = sinf(rad3);
            uv3[0] = ((u * cos_r3 - v * sin_r3) / face->uv_scale3.x) + face->uv_offset3.x; uv3[1] = ((u * sin_r3 + v * cos_r3) / face->uv_scale3.y) + face->uv_offset3.y;
            float rad4 = face->uv_rotation4 * (M_PI / 180.0f); float cos_r4 = cosf(rad4); float sin_r4 = sinf(rad4);
            uv4[0] = ((u * cos_r4 - v * sin_r4) / face->uv_scale4.x) + face->uv_offset4.x; uv4[1] = ((u * sin_r4 + v * cos_r4) / face->uv_scale4.y) + face->uv_offset4.y;

            Vec2 current_tex_uv = calculate_texture_uv_for_vertex(b, i, vertex_index);
            float local_u = (current_tex_uv.x - min_uv.x) / uv_range.x;
            float local_v = (current_tex_uv.y - min_uv.y) / uv_range.y;

            float total_padded_width_uv = face->atlas_coords.z;
            float total_padded_height_uv = face->atlas_coords.w;

            float padded_width_px = total_padded_width_uv * atlas_width;
            float padded_height_px = total_padded_height_uv * atlas_height;

            vert.lightmap_uv.x = face->atlas_coords.x + local_u * face->atlas_coords.z;
            vert.lightmap_uv.y = face->atlas_coords.y + local_v * face->atlas_coords.w;

            memcpy(&final_vbo_data[vbo_idx + 0], &vert.pos, sizeof(Vec3));
            memcpy(&final_vbo_data[vbo_idx + 3], &norm, sizeof(Vec3));
            memcpy(&final_vbo_data[vbo_idx + 6], uv1, sizeof(Vec2));
            memcpy(&final_vbo_data[vbo_idx + 12], &vert.color, sizeof(Vec4));
            memcpy(&final_vbo_data[vbo_idx + 16], uv2, sizeof(Vec2));
            memcpy(&final_vbo_data[vbo_idx + 18], uv3, sizeof(Vec2));
            memcpy(&final_vbo_data[vbo_idx + 20], uv4, sizeof(Vec2));
            memcpy(&final_vbo_data[vbo_idx + 22], &vert.lightmap_uv, sizeof(Vec2));
        }
        free(face_tri_indices);
        vbo_vertex_offset += num_verts_in_face;
    }

    if (b->vao == 0) { glGenVertexArrays(1, &b->vao); glGenBuffers(1, &b->vbo); }
    glBindVertexArray(b->vao);
    glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
    glBufferData(GL_ARRAY_BUFFER, total_render_verts * stride_floats * sizeof(float), final_vbo_data, GL_DYNAMIC_DRAW);

    size_t offset = 0;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(0); offset += 3 * sizeof(float);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(1); offset += 3 * sizeof(float);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(2); offset += 2 * sizeof(float);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(3); offset += 4 * sizeof(float);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(4); offset += 4 * sizeof(float);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(5); offset += 2 * sizeof(float);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(6); offset += 2 * sizeof(float);
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(7); offset += 2 * sizeof(float);
    glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(8);

    glBindVertexArray(0);
    free(final_vbo_data);
    free(temp_normals);
}

void Scene_Clear(Scene* scene, Engine* engine) {
    IO_Clear();

    if (scene->objects) {
        for (int i = 0; i < scene->numObjects; ++i) {
            if (scene->objects[i].model) {
                Model_Free(scene->objects[i].model);
            }
            if (scene->objects[i].bakedVertexColors) {
                free(scene->objects[i].bakedVertexColors);
            }
            if (scene->objects[i].bakedVertexDirections) {
                free(scene->objects[i].bakedVertexDirections);
            }
        }
        free(scene->objects);
        scene->objects = NULL;
    }

    for (int i = 0; i < scene->numBrushes; ++i) {
        for (int j = 0; j < scene->brushes[i].numFaces; ++j) {
            if (scene->brushes[i].lightmapAtlas != 0) {
                glDeleteTextures(1, &scene->brushes[i].lightmapAtlas);
            }
            if (scene->brushes[i].directionalLightmapAtlas != 0) {
                glDeleteTextures(1, &scene->brushes[i].directionalLightmapAtlas);
            }
        }
        Brush_FreeData(&scene->brushes[i]);
        scene->brushes[i].physicsBody = NULL;
    }

    for (int i = 0; i < scene->numActiveLights; ++i) {
        Light_DestroyShadowMap(&scene->lights[i]);
    }

    for (int i = 0; i < scene->numSoundEntities; ++i) {
        SoundSystem_DeleteSource(scene->soundEntities[i].sourceID);
        SoundSystem_DeleteBuffer(scene->soundEntities[i].bufferID);
    }

    for (int i = 0; i < scene->numParticleEmitters; ++i) {
        ParticleEmitter_Free(&scene->particleEmitters[i]);
        ParticleSystem_Free(scene->particleEmitters[i].system);
    }

    for (int i = 0; i < scene->numVideoPlayers; ++i) {
        VideoPlayer_Free(&scene->videoPlayers[i]);
    }

    for (int i = 0; i < scene->numDecals; ++i) {
        if (scene->decals[i].lightmapAtlas) {
            glDeleteTextures(1, &scene->decals[i].lightmapAtlas);
        }
        if (scene->decals[i].directionalLightmapAtlas) {
            glDeleteTextures(1, &scene->decals[i].directionalLightmapAtlas);
        }
    }

    for (int i = 0; i < scene->numParallaxRooms; ++i) {
        if (scene->parallaxRooms[i].cubemapTexture) {
            glDeleteTextures(1, &scene->parallaxRooms[i].cubemapTexture);
        }
    }

    scene->numLogicEntities = 0;

    if (engine->camera.physicsBody) {
        engine->camera.physicsBody = NULL;
    }

    if (engine->physicsWorld) {
        Physics_DestroyWorld(engine->physicsWorld);
        engine->physicsWorld = NULL;
    }

    scene->numSprites = 0;
    memset(scene, 0, sizeof(Scene));
    scene->static_shadows_generated = false;
    scene->playerStart.position = (Vec3){ 0, 5, 0 };
    scene->fog.enabled = false;
    scene->fog.color = (Vec3){ 0.5f, 0.6f, 0.7f };
    scene->fog.start = 50.0f;
    scene->fog.end = 200.0f;
    scene->post.enabled = true;
    scene->post.crtCurvature = 0.1f;
    scene->post.vignetteStrength = 0.8f;
    scene->post.vignetteRadius = 0.75f;
    scene->post.lensFlareEnabled = true;
    scene->post.lensFlareStrength = 1.0f;
    scene->post.scanlineStrength = 0.0f;
    scene->post.grainIntensity = 0.07f;
    scene->post.dofEnabled = false;
    scene->post.dofFocusDistance = 0.1f;
    scene->post.dofAperture = 10.0f;
    scene->post.chromaticAberrationEnabled = true;
    scene->post.chromaticAberrationStrength = 0.005f;
    scene->post.sharpenEnabled = false;
    scene->post.sharpenAmount = 0.15f;
    scene->post.fade_active = false;
    scene->post.fade_alpha = 0.0f;
    scene->post.fade_color = (Vec3){ 0, 0, 0 };
    scene->post.bwEnabled = false;
    scene->post.bwStrength = 1.0f;
    scene->colorCorrection.enabled = false;
    memset(scene->colorCorrection.lutPath, 0, sizeof(scene->colorCorrection.lutPath));
    scene->colorCorrection.lutTexture = 0;
    if (scene->ambient_probes) {
        free(scene->ambient_probes);
        scene->ambient_probes = NULL;
    }
    scene->num_ambient_probes = 0;
    scene->sun.enabled = true;
    scene->sun.direction = (Vec3){ -0.5f, -1.0f, -0.5f };
    vec3_normalize(&scene->sun.direction);
    scene->sun.color = (Vec3){ 1.0f, 0.95f, 0.85f };
    scene->sun.intensity = 1.0f;
    scene->lightmapResolution = 128;
}

void Decal_LoadLightmaps(Decal* decal, const char* map_name_sanitized, int decal_index) {
    if (!decal) return;

    char decal_name_sanitized[128];
    if (strlen(decal->targetname) > 0) {
        sanitize_filename_map(decal->targetname, decal_name_sanitized, sizeof(decal_name_sanitized));
    }
    else {
        sprintf(decal_name_sanitized, "decal_%d", decal_index);
    }

    char final_decal_dir[1024];
    snprintf(final_decal_dir, sizeof(final_decal_dir), "lightmaps/%s/%s", map_name_sanitized, decal_name_sanitized);

    char color_path[2048];
    snprintf(color_path, sizeof(color_path), "%s/lightmap_color.hdr", final_decal_dir);

    int width, height, channels;
    float* color_data = stbi_loadf(color_path, &width, &height, &channels, 3);

    if (color_data) {
        glGenTextures(1, &decal->lightmapAtlas);
        glBindTexture(GL_TEXTURE_2D, decal->lightmapAtlas);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, color_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(color_data);
    }
    else {
        decal->lightmapAtlas = 0;
    }

    char dir_path[2048];
    snprintf(dir_path, sizeof(dir_path), "%s/lightmap_dir.png", final_decal_dir);

    SDL_Surface* dir_surface = IMG_Load(dir_path);
    if (dir_surface) {
        SDL_Surface* dir_converted = SDL_ConvertSurfaceFormat(dir_surface, SDL_PIXELFORMAT_RGBA32, 0);
        if (dir_converted) {
            glGenTextures(1, &decal->directionalLightmapAtlas);
            glBindTexture(GL_TEXTURE_2D, decal->directionalLightmapAtlas);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dir_converted->w, dir_converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, dir_converted->pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            SDL_FreeSurface(dir_converted);
        }
        SDL_FreeSurface(dir_surface);
    }
    else {
        decal->directionalLightmapAtlas = 0;
    }
}

void Scene_LoadAmbientProbes(Scene* scene) {
    if (scene->ambient_probes) {
        free(scene->ambient_probes);
        scene->ambient_probes = NULL;
    }
    scene->num_ambient_probes = 0;

    if (strlen(scene->mapPath) == 0) {
        return;
    }

    char map_name_sanitized[128];
    const char* last_slash = strrchr(scene->mapPath, '/');
    const char* last_bslash = strrchr(scene->mapPath, '\\');
    const char* map_filename_start = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : scene->mapPath);

    char* dot = strrchr(map_filename_start, '.');
    if (dot) {
        size_t len = dot - map_filename_start;
        strncpy(map_name_sanitized, map_filename_start, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, map_filename_start);
    }

    char probe_path[512];
    snprintf(probe_path, sizeof(probe_path), "lightmaps/%s/ambient_probes.amp", map_name_sanitized);

    FILE* probe_file = fopen(probe_path, "rb");
    if (probe_file) {
        char header[4];
        if (fread(header, 1, 4, probe_file) == 4 && strncmp(header, "AMBI", 4) == 0) {
            fread(&scene->num_ambient_probes, sizeof(int), 1, probe_file);
            if (scene->num_ambient_probes > 0) {
                scene->ambient_probes = malloc(sizeof(AmbientProbe) * scene->num_ambient_probes);
                fread(scene->ambient_probes, sizeof(AmbientProbe), scene->num_ambient_probes, probe_file);
            }
        }
        else {
            Console_Printf_Error("Invalid ambient probe file header: %s", probe_path);
        }
        fclose(probe_file);
    }
}

void Brush_GenerateLightmapAtlas(Brush* b, const char* map_name_sanitized, int brush_index, int resolution) {
    if (b->numFaces == 0) return;

    typedef struct {
        float* color_data;
        SDL_Surface* dir_surface;
        int width;
        int height;
        bool is_valid;
    } FaceLightmapData;

    FaceLightmapData* face_data = calloc(b->numFaces, sizeof(FaceLightmapData));
    int valid_faces = 0;
    int max_width = 0;
    int max_height = 0;

    char brush_name_sanitized[128];
    if (strlen(b->targetname) > 0) {
        sanitize_filename_map(b->targetname, brush_name_sanitized, sizeof(brush_name_sanitized));
    }
    else {
        sprintf(brush_name_sanitized, "Brush_%d", brush_index);
    }

    char final_brush_dir[1024];
    snprintf(final_brush_dir, sizeof(final_brush_dir), "lightmaps/%s/%s", map_name_sanitized, brush_name_sanitized);

    for (int i = 0; i < b->numFaces; ++i) {
        face_data[i].is_valid = false;
        char path[512];

        snprintf(path, sizeof(path), "%s/face_%d_color.hdr", final_brush_dir, i);
        face_data[i].color_data = stbi_loadf(path, &face_data[i].width, &face_data[i].height, NULL, 3);

        snprintf(path, sizeof(path), "%s/face_%d_dir.png", final_brush_dir, i);
        face_data[i].dir_surface = IMG_Load(path);

        if (face_data[i].color_data && face_data[i].dir_surface) {
            valid_faces++;
            face_data[i].is_valid = true;
            if (face_data[i].width > max_width) max_width = face_data[i].width;
            if (face_data[i].height > max_height) max_height = face_data[i].height;
        }
    }

    if (valid_faces == 0) {
        for (int i = 0; i < b->numFaces; ++i) {
            if (face_data[i].color_data) stbi_image_free(face_data[i].color_data);
            if (face_data[i].dir_surface) SDL_FreeSurface(face_data[i].dir_surface);
        }
        free(face_data);
        b->lightmapAtlas = 0;
        b->directionalLightmapAtlas = 0;
        return;
    }

    if (max_width == 0) max_width = 4;
    if (max_height == 0) max_height = 4;
    int atlas_cols = (int)ceil(sqrt((double)valid_faces));
    int atlas_rows = (int)ceil((double)valid_faces / atlas_cols);
    int atlas_width = atlas_cols * max_width;
    int atlas_height = atlas_rows * max_height;

    glGenTextures(1, &b->lightmapAtlas);
    glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, atlas_width, atlas_height, 0, GL_RGB, GL_FLOAT, NULL);

    glGenTextures(1, &b->directionalLightmapAtlas);
    glBindTexture(GL_TEXTURE_2D, b->directionalLightmapAtlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_width, atlas_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    int current_face = 0;
    for (int i = 0; i < b->numFaces; ++i) {
        if (face_data[i].is_valid) {
            int x_pos = (current_face % atlas_cols) * max_width;
            int y_pos = (current_face / atlas_cols) * max_height;
            int w = face_data[i].width;
            int h = face_data[i].height;

            glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
            glTexSubImage2D(GL_TEXTURE_2D, 0, x_pos, y_pos, w, h, GL_RGB, GL_FLOAT, face_data[i].color_data);

            SDL_Surface* dir_converted = SDL_ConvertSurfaceFormat(face_data[i].dir_surface, SDL_PIXELFORMAT_RGBA32, 0);
            if (dir_converted) {
                glBindTexture(GL_TEXTURE_2D, b->directionalLightmapAtlas);
                glTexSubImage2D(GL_TEXTURE_2D, 0, x_pos, y_pos, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dir_converted->pixels);
                SDL_FreeSurface(dir_converted);
            }

            b->faces[i].atlas_coords.x = (float)x_pos / atlas_width;
            b->faces[i].atlas_coords.y = (float)y_pos / atlas_height;
            b->faces[i].atlas_coords.z = (float)w / atlas_width;
            b->faces[i].atlas_coords.w = (float)h / atlas_height;

            float pad_x = (float)LIGHTMAPPADDING / atlas_width;
            float pad_y = (float)LIGHTMAPPADDING / atlas_height;
            b->faces[i].atlas_coords.x += pad_x;
            b->faces[i].atlas_coords.y += pad_y;
            b->faces[i].atlas_coords.z -= pad_x * 2.0f;
            b->faces[i].atlas_coords.w -= pad_y * 2.0f;

            current_face++;
        }
    }

    glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, b->directionalLightmapAtlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    for (int i = 0; i < b->numFaces; ++i) {
        if (face_data[i].color_data) stbi_image_free(face_data[i].color_data);
        if (face_data[i].dir_surface) SDL_FreeSurface(face_data[i].dir_surface);
    }
    free(face_data);
}

bool Scene_LoadMap(Scene* scene, Renderer* renderer, const char* mapPath, Engine* engine) {
    FILE* file = fopen(mapPath, "r");
    if (!file) {
        Console_Printf_Error("[error] Could not find map file: %s", mapPath);
        return false;
    }

    char version_line[256];
    int map_file_version = 0;
    if (fgets(version_line, sizeof(version_line), file) && sscanf(version_line, "MAP_VERSION %d", &map_file_version) == 1) {
        if (map_file_version != MAP_VERSION) {
            Console_Printf_Error("[error] Map version mismatch! Map is v%d, Engine expects v%d.", map_file_version, MAP_VERSION);
            fclose(file);
            return false;
        }
    }
    else {
        Console_Printf_Error("[error] Invalid or missing map version. Could be an old map format.");
        fclose(file);
        return false;
    }

    Scene_Clear(scene, engine);

    strncpy(scene->mapPath, mapPath, sizeof(scene->mapPath) - 1);
    scene->mapPath[sizeof(scene->mapPath) - 1] = '\0';

    engine->physicsWorld = Physics_CreateWorld(Cvar_GetFloat("gravity") * -1.0f);

    char line[2048];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char keyword[64];
        sscanf(line, "%s", keyword);
        if (strcmp(keyword, "player_start") == 0) {
            if (sscanf(line, "%*s %f %f %f %f %f", &scene->playerStart.position.x, &scene->playerStart.position.y, &scene->playerStart.position.z, &scene->playerStart.yaw, &scene->playerStart.pitch) != 5) {
                sscanf(line, "%*s %f %f %f", &scene->playerStart.position.x, &scene->playerStart.position.y, &scene->playerStart.position.z);
                scene->playerStart.yaw = 0.0f;
                scene->playerStart.pitch = 0.0f;
            }
        }
        else if (strcmp(keyword, "lightmap_resolution") == 0) {
            sscanf(line, "%*s %d", &scene->lightmapResolution);
        }
        else if (strcmp(keyword, "fog_settings") == 0) {
            int enabled_int;
            sscanf(line, "%*s %d %f %f %f %f %f",
                &enabled_int, &scene->fog.color.x, &scene->fog.color.y, &scene->fog.color.z, &scene->fog.start, &scene->fog.end);
            scene->fog.enabled = (bool)enabled_int;
        }
        else if (strcmp(keyword, "post_settings") == 0) {
            int enabled_int, flare_int, dof_enabled_int, ca_enabled_int, sharpen_enabled_int, bw_enabled_int;
            sscanf(line, "%*s %d %f %f %f %d %f %f %f %d %f %f %d %f %d %f %d %f", &enabled_int, &scene->post.crtCurvature, &scene->post.vignetteStrength,
                &scene->post.vignetteRadius, &flare_int, &scene->post.lensFlareStrength, &scene->post.scanlineStrength, &scene->post.grainIntensity,
                &dof_enabled_int, &scene->post.dofFocusDistance, &scene->post.dofAperture, &ca_enabled_int, &scene->post.chromaticAberrationStrength, &sharpen_enabled_int, &scene->post.sharpenAmount, &bw_enabled_int, &scene->post.bwStrength);
            scene->post.enabled = (bool)enabled_int;
            scene->post.lensFlareEnabled = (bool)flare_int;
            scene->post.dofEnabled = (bool)dof_enabled_int;
            scene->post.chromaticAberrationEnabled = (bool)ca_enabled_int;
            scene->post.sharpenEnabled = (bool)sharpen_enabled_int;
            scene->post.bwEnabled = (bool)bw_enabled_int;
        }
        else if (strcmp(keyword, "skybox") == 0) {
            int use_cubemap_int = 0;
            sscanf(line, "%*s %d \"%127[^\"]\"", &use_cubemap_int, scene->skybox_path);
            scene->use_cubemap_skybox = (bool)use_cubemap_int;
        }
        else if (strcmp(keyword, "sun") == 0) {
            int enabled_int;
            sscanf(line, "%*s %d %f %f %f %f %f %f %f %f %f %f", &enabled_int, &scene->sun.direction.x, &scene->sun.direction.y,
                &scene->sun.direction.z, &scene->sun.color.x, &scene->sun.color.y, &scene->sun.color.z, &scene->sun.intensity,
                &scene->sun.windDirection.x, &scene->sun.windDirection.y, &scene->sun.windDirection.z, &scene->sun.windStrength);
            scene->sun.enabled = (bool)enabled_int;
            vec3_normalize(&scene->sun.direction);
        }
        else if (strcmp(keyword, "color_correction") == 0) {
            int enabled_int = 0;
            sscanf(line, "%*s %d \"%127[^\"]\"", &enabled_int, scene->colorCorrection.lutPath);
            scene->colorCorrection.enabled = (bool)enabled_int;
            if (scene->colorCorrection.enabled && strlen(scene->colorCorrection.lutPath) > 0) {
                scene->colorCorrection.lutTexture = loadTexture(scene->colorCorrection.lutPath, false);
            }
        }
        else if (strcmp(keyword, "brush_begin") == 0) {
            if (scene->numBrushes >= MAX_BRUSHES) continue;
            Brush* b = &scene->brushes[scene->numBrushes];
            memset(b, 0, sizeof(Brush));
            b->mass = 0.0f;
            b->isPhysicsEnabled = true;
            b->runtime_active = true;
            b->runtime_playerIsTouching = false;
            b->runtime_hasFired = false;
            char water_def_name[64] = "";
            sscanf(line, "%*s %f %f %f %f %f %f %f %f %f", &b->pos.x, &b->pos.y, &b->pos.z, &b->rot.x, &b->rot.y, &b->rot.z, &b->scale.x, &b->scale.y, &b->scale.z);
            while (fgets(line, sizeof(line), file) && strncmp(line, "brush_end", 9) != 0) {
                int dummy_int;
                char face_keyword[64];
                sscanf(line, "%s", face_keyword);
                if (sscanf(line, " num_verts %d", &b->numVertices) == 1) {
                    b->vertices = malloc(b->numVertices * sizeof(BrushVertex));
                    for (int i = 0; i < b->numVertices; ++i) {
                        fgets(line, sizeof(line), file);
                        if (sscanf(line, " v %*d %f %f %f %f %f %f %f", &b->vertices[i].pos.x, &b->vertices[i].pos.y, &b->vertices[i].pos.z, &b->vertices[i].color.x, &b->vertices[i].color.y, &b->vertices[i].color.z, &b->vertices[i].color.w) != 7) {
                            b->vertices[i].color = (Vec4){ 0,0,0,1 };
                        }
                    }
                }
                else if (sscanf(line, " num_faces %d", &b->numFaces) == 1) {
                    b->faces = calloc(b->numFaces, sizeof(BrushFace));
                    for (int i = 0; i < b->numFaces; ++i) {
                        fgets(line, sizeof(line), file);
                        char mat_name[64], mat2_name[64], mat3_name[64], mat4_name[64];

                        char* lightmap_scale_ptr = strstr(line, "lightmap_scale");
                        if (lightmap_scale_ptr) {
                            sscanf(lightmap_scale_ptr, "lightmap_scale %f", &b->faces[i].lightmap_scale);
                            *lightmap_scale_ptr = '\0';
                        }

                        char* grouped_ptr = strstr(line, "is_grouped");
                        if (grouped_ptr) {
                            int grouped_int;
                            if (sscanf(grouped_ptr, "is_grouped %d \"%63[^\"]\"", &grouped_int, b->faces[i].groupName) == 2) {
                                b->faces[i].isGrouped = (bool)grouped_int;
                            }
                            *grouped_ptr = '\0';
                        } else {
                            b->faces[i].isGrouped = false;
                            b->faces[i].groupName[0] = '\0';
                        }

                        sscanf(line, " f %*d %s %s %s %s %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d",
                            mat_name, mat2_name, mat3_name, mat4_name, &b->faces[i].uv_offset.x, &b->faces[i].uv_offset.y, &b->faces[i].uv_rotation, &b->faces[i].uv_scale.x, &b->faces[i].uv_scale.y,
                            &b->faces[i].uv_offset2.x, &b->faces[i].uv_offset2.y, &b->faces[i].uv_rotation2, &b->faces[i].uv_scale2.x, &b->faces[i].uv_scale2.y,
                            &b->faces[i].uv_offset3.x, &b->faces[i].uv_offset3.y, &b->faces[i].uv_rotation3, &b->faces[i].uv_scale3.x, &b->faces[i].uv_scale3.y,
                            &b->faces[i].uv_offset4.x, &b->faces[i].uv_offset4.y, &b->faces[i].uv_rotation4, &b->faces[i].uv_scale4.x, &b->faces[i].uv_scale4.y,
                            &b->faces[i].numVertexIndices);

                        b->faces[i].material = TextureManager_FindMaterial(mat_name);
                        b->faces[i].material2 = strcmp(mat2_name, "NULL") == 0 ? NULL : TextureManager_FindMaterial(mat2_name);
                        b->faces[i].material3 = strcmp(mat3_name, "NULL") == 0 ? NULL : TextureManager_FindMaterial(mat3_name);
                        b->faces[i].material4 = strcmp(mat4_name, "NULL") == 0 ? NULL : TextureManager_FindMaterial(mat4_name);

                        b->faces[i].vertexIndices = malloc(b->faces[i].numVertexIndices * sizeof(int));
                        char* p = strchr(line, ':');
                        if (p) {
                            p++;
                            int total_offset = 0;
                            for (int j = 0; j < b->faces[i].numVertexIndices; ++j) {
                                int chars_read = 0;
                                sscanf(p + total_offset, " %d %n", &b->faces[i].vertexIndices[j], &chars_read);
                                total_offset += chars_read;
                            }
                        }
                    }
                }
                else if (sscanf(line, " name \"%63[^\"]\"", b->name) == 1) {}
                else if (sscanf(line, " targetname \"%63[^\"]\"", b->targetname) == 1) {}
                else if (sscanf(line, " mass %f", &b->mass) == 1) {}
                else if (sscanf(line, " isPhysicsEnabled %d", &dummy_int) == 1) { b->isPhysicsEnabled = (bool)dummy_int; }
                else if (sscanf(line, " classname \"%63[^\"]\"", b->classname) == 1) {}
                else if (strstr(line, "properties")) {
                    b->numProperties = 0;
                    while (fgets(line, sizeof(line), file) && !strstr(line, "}")) {
                        if (b->numProperties < MAX_ENTITY_PROPERTIES) {
                            if (sscanf(line, " \"%63[^\"]\" \"%127[^\"]\"", b->properties[b->numProperties].key, b->properties[b->numProperties].value) == 2) {
                                b->numProperties++;
                            }
                        }
                    }
                }
                else if (sscanf(line, " is_grouped %d \"%63[^\"]\"", &dummy_int, b->groupName) == 2) { 
                    b->isGrouped = (bool)dummy_int; 
                } else {
                     b->groupName[0] = '\0';
                }
            }
            if (strcmp(b->classname, "env_reflectionprobe") == 0) {
                const char* faces_suffixes[] = { "px", "nx", "py", "ny", "pz", "nz" };
                char face_paths[6][256];
                for (int i = 0; i < 6; ++i) sprintf(face_paths[i], "cubemaps/%s_%s.png", b->name, faces_suffixes[i]);
                const char* face_pointers[6];
                for (int i = 0; i < 6; ++i) face_pointers[i] = face_paths[i];
                b->cubemapTexture = loadCubemap(face_pointers);
            }
            Brush_UpdateMatrix(b);
            char map_name_sanitized[128];
            char* dot = strrchr(scene->mapPath, '.');
            if (dot) {
                size_t len = dot - scene->mapPath;
                strncpy(map_name_sanitized, scene->mapPath, len);
                map_name_sanitized[len] = '\0';
            }
            else { strcpy(map_name_sanitized, scene->mapPath); }
            Brush_GenerateLightmapAtlas(b, map_name_sanitized, scene->numBrushes, scene->lightmapResolution);
            Brush_CreateRenderData(b);
            if (Brush_IsSolid(b) && b->numVertices > 0) {
                if (b->mass > 0.0f) {
                    b->physicsBody = Physics_CreateDynamicBrush(engine->physicsWorld, (const float*)b->vertices, b->numVertices, b->mass, b->modelMatrix);
                    if (!b->isPhysicsEnabled) Physics_ToggleCollision(engine->physicsWorld, b->physicsBody, false);
                }
                else {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int i = 0; i < b->numVertices; i++) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
            }
            scene->numBrushes++;
        }
        else if (strcmp(keyword, "gltf_model") == 0) {
            if (scene->numObjects >= MAX_MODELS) continue;
            scene->numObjects++;
            scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
            SceneObject* newObj = &scene->objects[scene->numObjects - 1];
            memset(newObj, 0, sizeof(SceneObject));
            char* p = line + strlen(keyword);
            while (*p && isspace(*p)) p++;
            char* path_end = p;
            while (*path_end && !isspace(*path_end)) path_end++;
            size_t path_len = path_end - p;
            if (path_len < sizeof(newObj->modelPath)) { strncpy(newObj->modelPath, p, path_len); newObj->modelPath[path_len] = '\0'; }
            p = path_end;
            while (*p && isspace(*p)) p++;
            if (*p == '"') { p++; char* quote_end = strchr(p, '"'); if (quote_end) { size_t name_len = quote_end - p; if (name_len < sizeof(newObj->targetname)) { strncpy(newObj->targetname, p, name_len); newObj->targetname[name_len] = '\0'; } p = quote_end + 1; } }
            int phys_enabled_int; int sway_enabled_int = 0;
            int casts_shadows_int = 1;
            sscanf(p, "%f %f %f %f %f %f %f %f %f %f %d %d %f %f %d", &newObj->pos.x, &newObj->pos.y, &newObj->pos.z, &newObj->rot.x, &newObj->rot.y, &newObj->rot.z, &newObj->scale.x, &newObj->scale.y, &newObj->scale.z, &newObj->mass, &phys_enabled_int, &sway_enabled_int, &newObj->fadeStartDist, &newObj->fadeEndDist, &casts_shadows_int);
            newObj->casts_shadows = (bool)casts_shadows_int;
            newObj->swayEnabled = (bool)sway_enabled_int; newObj->isPhysicsEnabled = (bool)phys_enabled_int;
            newObj->animation_playing = false;
            newObj->animation_looping = true;
            newObj->current_animation = -1;
            newObj->animation_time = 0.0f;
            newObj->bone_matrices = NULL;
            mat4_identity(&newObj->animated_local_transform);
            long current_pos = ftell(file); char next_line[256];
            if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                int grouped_int;
                sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, newObj->groupName);
                newObj->isGrouped = (bool)grouped_int;
            } else {
                newObj->groupName[0] = '\0';
                fseek(file, current_pos, SEEK_SET);
            }
            SceneObject_UpdateMatrix(newObj);
            newObj->model = Model_Load(newObj->modelPath);
            if (newObj->model && newObj->model->num_animations > 0) {
                newObj->current_animation = 0;
            }
            SceneObject_LoadVertexLighting(newObj, scene->numObjects - 1, scene->mapPath);
            SceneObject_LoadVertexDirectionalLighting(newObj, scene->numObjects - 1, scene->mapPath);
            if (!newObj->model) { scene->numObjects--; continue; }
            if (newObj->mass > 0.0f) { newObj->physicsBody = Physics_CreateDynamicConvexHull(engine->physicsWorld, newObj->model->combinedVertexData, newObj->model->totalVertexCount, newObj->mass, newObj->modelMatrix); if (!newObj->isPhysicsEnabled) Physics_ToggleCollision(engine->physicsWorld, newObj->physicsBody, false); }
            else if (newObj->model && newObj->model->combinedVertexData && newObj->model->totalIndexCount > 0) { Mat4 physics_transform = create_trs_matrix(newObj->pos, newObj->rot, (Vec3) { 1.0f, 1.0f, 1.0f }); newObj->physicsBody = Physics_CreateStaticTriangleMesh(engine->physicsWorld, newObj->model->combinedVertexData, newObj->model->totalVertexCount, newObj->model->combinedIndexData, newObj->model->totalIndexCount, physics_transform, newObj->scale); }
        }
        else if (strcmp(keyword, "light") == 0) {
            if (scene->numActiveLights >= MAX_LIGHTS) continue;
            Light* light = &scene->lights[scene->numActiveLights];
            memset(light, 0, sizeof(Light));
            int type_int = 0; int preset_int = 0; int is_static_int = 0;

            sscanf(line, "%*s \"%63[^\"]\" %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d %d \"%127[^\"]\"", light->targetname, &type_int, &light->position.x, &light->position.y, &light->position.z, &light->rot.x, &light->rot.y, &light->rot.z, &light->color.x, &light->color.y, &light->color.z, &light->base_intensity, &light->radius, &light->cutOff, &light->outerCutOff, &light->shadowFarPlane, &light->shadowBias, &light->volumetricIntensity, &preset_int, &is_static_int, light->cookiePath);

            light->custom_style_string[0] = '\0';
            const char* cookie_end = strstr(line, light->cookiePath);
            if (cookie_end) {
                const char* custom_style_start = strstr(cookie_end + strlen(light->cookiePath), "\"");
                if (custom_style_start) {
                    sscanf(custom_style_start, "\"%255[^\"]\"", light->custom_style_string);
                }
            }

            light->preset = preset_int;
            light->type = (LightType)type_int;
            light->is_on = (light->base_intensity > 0.0f);
            light->is_static = (bool)is_static_int;
            light->intensity = light->base_intensity;

            long current_pos = ftell(file);
            char next_line[256];
            if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                int grouped_int;
                sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, light->groupName);
                light->isGrouped = (bool)grouped_int;
            }
            else {
                light->groupName[0] = '\0';
                fseek(file, current_pos, SEEK_SET);
            }

            if (strlen(light->cookiePath) > 0 && strcmp(light->cookiePath, "none") != 0) {
                Material* cookieMat = TextureManager_FindMaterial(light->cookiePath);
                if (cookieMat && cookieMat != &g_MissingMaterial) {
                    light->cookieMap = cookieMat->diffuseMap;
                    light->cookieMapHandle = glGetTextureHandleARB(light->cookieMap);
                    glMakeTextureHandleResidentARB(light->cookieMapHandle);
                }
            }
            else {
                light->cookiePath[0] = '\0';
                light->cookieMap = 0;
                light->cookieMapHandle = 0;
            }

            Light_InitShadowMap(light);
            scene->numActiveLights++;
        }
        else if (strcmp(keyword, "decal") == 0) {
            if (scene->numDecals < MAX_DECALS) {
                Decal* d = &scene->decals[scene->numDecals];
                char mat_name[64];
                memset(d, 0, sizeof(Decal));
                char* p = line + strlen(keyword);
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; char* end = strchr(p, '"'); if (end) { strncpy(mat_name, p, end - p); mat_name[end - p] = '\0'; p = end + 1; } }
                else { char* end = p; while (*end && !isspace(*end)) end++; strncpy(mat_name, p, end - p); mat_name[end - p] = '\0'; p = end; }
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; char* end = strchr(p, '"'); if (end) { strncpy(d->targetname, p, end - p); d->targetname[end - p] = '\0'; p = end + 1; } }
                sscanf(p, "%f %f %f %f %f %f %f %f %f", &d->pos.x, &d->pos.y, &d->pos.z, &d->rot.x, &d->rot.y, &d->rot.z, &d->size.x, &d->size.y, &d->size.z);
                d->material = TextureManager_FindMaterial(mat_name);
                long current_pos = ftell(file); char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, d->groupName); d->isGrouped = (bool)grouped_int;
                } else {
                    d->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                Decal_UpdateMatrix(d);
                scene->numDecals++;
            }
        }
        else if (strcmp(keyword, "sound_entity") == 0) {
            if (scene->numSoundEntities < MAX_SOUNDS) {
                SoundEntity* s = &scene->soundEntities[scene->numSoundEntities];
                memset(s, 0, sizeof(SoundEntity));
                int is_looping_int = 0, play_on_start_int = 0;
                char* p = line + strlen(keyword); while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; char* end = strchr(p, '"'); if (end) { size_t len = end - p; if (len < sizeof(s->targetname)) { strncpy(s->targetname, p, len); s->targetname[len] = '\0'; } p = end + 1; } }
                while (*p && isspace(*p)) p++;
                char* path_end = p; while (*path_end && !isspace(*path_end)) path_end++; size_t path_len = path_end - p;
                if (path_len < sizeof(s->soundPath)) { strncpy(s->soundPath, p, path_len); s->soundPath[path_len] = '\0'; } p = path_end;
                sscanf(p, "%f %f %f %f %f %f %d %d", &s->pos.x, &s->pos.y, &s->pos.z, &s->volume, &s->pitch, &s->maxDistance, &is_looping_int, &play_on_start_int);
                s->is_looping = (bool)is_looping_int; s->play_on_start = (bool)play_on_start_int;
                long current_pos = ftell(file); char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, s->groupName); s->isGrouped = (bool)grouped_int;
                } else {
                    s->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                s->bufferID = SoundSystem_LoadSound(s->soundPath);
                if (s->play_on_start) s->sourceID = SoundSystem_PlaySound(s->bufferID, s->pos, s->volume, s->pitch, s->maxDistance, s->is_looping);
                scene->numSoundEntities++;
            }
        }
        else if (strcmp(keyword, "particle_emitter") == 0) {
            if (scene->numParticleEmitters < MAX_PARTICLE_EMITTERS) {
                ParticleEmitter* emitter = &scene->particleEmitters[scene->numParticleEmitters];
                memset(emitter, 0, sizeof(ParticleEmitter));
                int on_default_int = 1;
                sscanf(line, "%*s \"%127[^\"]\" \"%63[^\"]\" %d %f %f %f", emitter->parFile, emitter->targetname, &on_default_int, &emitter->pos.x, &emitter->pos.y, &emitter->pos.z);
                emitter->on_by_default = (bool)on_default_int;
                long current_pos = ftell(file); char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, emitter->groupName); emitter->isGrouped = (bool)grouped_int;
                } else {
                    emitter->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                ParticleSystem* ps = ParticleSystem_Load(emitter->parFile);
                if (ps) { ParticleEmitter_Init(emitter, ps, emitter->pos); scene->numParticleEmitters++; }
            }
        }
        else if (strcmp(keyword, "sprite") == 0) {
            if (scene->numSprites < MAX_SPRITES) {
                Sprite* s = &scene->sprites[scene->numSprites];
                memset(s, 0, sizeof(Sprite));
                char mat_name[64];
                sscanf(line, "%*s \"%63[^\"]\" %f %f %f %f \"%63[^\"]\"", s->targetname, &s->pos.x, &s->pos.y, &s->pos.z, &s->scale, mat_name);
                s->material = TextureManager_FindMaterial(mat_name);
                s->visible = true;
                long current_pos = ftell(file); char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, s->groupName); s->isGrouped = (bool)grouped_int;
                } else {
                    s->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                scene->numSprites++;
            }
        }
        else if (strcmp(keyword, "video_player") == 0) {
            if (scene->numVideoPlayers < MAX_VIDEO_PLAYERS) {
                VideoPlayer* vp = &scene->videoPlayers[scene->numVideoPlayers];
                memset(vp, 0, sizeof(VideoPlayer));
                int play_on_start_int = 0, loop_int = 0;
                char* p = line + strlen(keyword);
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; char* end = strchr(p, '"'); if (end) { size_t len = end - p; if (len < sizeof(vp->videoPath)) strncpy(vp->videoPath, p, len), vp->videoPath[len] = '\0'; p = end + 1; } }
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; char* end = strchr(p, '"'); if (end) { size_t len = end - p; if (len < sizeof(vp->targetname)) strncpy(vp->targetname, p, len), vp->targetname[len] = '\0'; p = end + 1; } }
                sscanf(p, "%d %d %f %f %f %f %f %f %f %f", &play_on_start_int, &loop_int, &vp->pos.x, &vp->pos.y, &vp->pos.z, &vp->rot.x, &vp->rot.y, &vp->rot.z, &vp->size.x, &vp->size.y);
                vp->playOnStart = (bool)play_on_start_int; vp->loop = (bool)loop_int;
                long current_pos = ftell(file); char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, vp->groupName); vp->isGrouped = (bool)grouped_int;
                } else {
                    vp->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                VideoPlayer_Load(vp); if (vp->playOnStart) VideoPlayer_Play(vp);
                scene->numVideoPlayers++;
            }
        }
        else if (strcmp(keyword, "parallax_room") == 0) {
            if (scene->numParallaxRooms < MAX_PARALLAX_ROOMS) {
                ParallaxRoom* p_room = &scene->parallaxRooms[scene->numParallaxRooms];
                memset(p_room, 0, sizeof(ParallaxRoom));
                char* p = line + strlen(keyword);
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; char* end = strchr(p, '"'); if (end) { size_t len = end - p; if (len < sizeof(p_room->cubemapPath)) strncpy(p_room->cubemapPath, p, len), p_room->cubemapPath[len] = '\0'; p = end + 1; } }
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; char* end = strchr(p, '"'); if (end) { size_t len = end - p; if (len < sizeof(p_room->targetname)) strncpy(p_room->targetname, p, len), p_room->targetname[len] = '\0'; p = end + 1; } }
                sscanf(p, "%f %f %f %f %f %f %f %f %f", &p_room->pos.x, &p_room->pos.y, &p_room->pos.z, &p_room->rot.x, &p_room->rot.y, &p_room->rot.z, &p_room->size.x, &p_room->size.y, &p_room->roomDepth);
                long current_pos = ftell(file); char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, p_room->groupName); p_room->isGrouped = (bool)grouped_int;
                } else {
                    p_room->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                const char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
                char face_paths[6][256]; const char* face_pointers[6];
                for (int i = 0; i < 6; ++i) { sprintf(face_paths[i], "%s%s", p_room->cubemapPath, suffixes[i]); face_pointers[i] = face_paths[i]; }
                p_room->cubemapTexture = loadCubemap(face_pointers); ParallaxRoom_UpdateMatrix(p_room);
                scene->numParallaxRooms++;
            }
        }
        else if (strcmp(keyword, "logic_entity_begin") == 0) {
            if (scene->numLogicEntities >= MAX_LOGIC_ENTITIES) continue;
            LogicEntity* ent = &scene->logicEntities[scene->numLogicEntities];
            memset(ent, 0, sizeof(LogicEntity));
            while (fgets(line, sizeof(line), file) && strncmp(line, "logic_entity_end", 16) != 0) {
                int dummy_int;
                if (sscanf(line, " classname \"%63[^\"]\"", ent->classname) == 1) {}
                else if (sscanf(line, " targetname \"%63[^\"]\"", ent->targetname) == 1) {}
                else if (sscanf(line, " pos %f %f %f", &ent->pos.x, &ent->pos.y, &ent->pos.z) == 3) {}
                else if (sscanf(line, " rot %f %f %f", &ent->rot.x, &ent->rot.y, &ent->rot.z) == 3) {}
                else if (sscanf(line, " is_grouped %d \"%63[^\"]\"", &dummy_int, ent->groupName) == 2) {
                    ent->isGrouped = (bool)dummy_int;
                } else {
                    ent->groupName[0] = '\0';
                }
                if (sscanf(line, " runtime_active %d", (int*)&ent->runtime_active) == 1) {}
                else if (sscanf(line, " runtime_float_a %f", &ent->runtime_float_a) == 1) {}
                else if (sscanf(line, " runtime_int_a %d", &ent->runtime_int_a) == 1) {}
                else if (sscanf(line, " runtime_float_b %f", &ent->runtime_float_b) == 1) {}
                else if (strstr(line, "properties")) {
                    while (fgets(line, sizeof(line), file) && !strstr(line, "}")) {
                        if (ent->numProperties < MAX_ENTITY_PROPERTIES) { if (sscanf(line, " \"%63[^\"]\" \"%127[^\"]\"", ent->properties[ent->numProperties].key, ent->properties[ent->numProperties].value) == 2) { ent->numProperties++; } }
                    }
                }
            }
            if (strcmp(ent->classname, "logic_random") == 0) { if (strcmp(LogicEntity_GetProperty(ent, "is_default_enabled", "0"), "1") == 0) { ent->runtime_active = true; } }
            else if (strcmp(ent->classname, "env_blackhole") == 0) {
                const char* starton_str = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_str) == 1);
            }
            scene->numLogicEntities++;
        }
        else if (strcmp(keyword, "io_connection") == 0) {
            if (g_num_io_connections < MAX_IO_CONNECTIONS) {
                IOConnection* conn = &g_io_connections[g_num_io_connections];
                conn->active = true; conn->parameter[0] = '\0';
                int type_int, fire_once_int, has_fired_int = 0;
                int items_scanned = sscanf(line, "%*s %d %d \"%63[^\"]\" \"%63[^\"]\" \"%63[^\"]\" %f %d %d \"%63[^\"]\"", &type_int, &conn->sourceIndex, conn->outputName, conn->targetName, conn->inputName, &conn->delay, &fire_once_int, &has_fired_int, conn->parameter);
                if (items_scanned < 8) has_fired_int = 0;
                conn->sourceType = (EntityType)type_int; conn->fireOnce = (bool)fire_once_int; conn->hasFired = (bool)has_fired_int;
                g_num_io_connections++;
            }
        }
    }
    fclose(file);
    if (scene->use_cubemap_skybox && strlen(scene->skybox_path) > 0) {
        const char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
        char face_paths[6][256]; const char* face_pointers[6];
        for (int i = 0; i < 6; ++i) { sprintf(face_paths[i], "skybox/%s%s", scene->skybox_path, suffixes[i]); face_pointers[i] = face_paths[i]; }
        scene->skybox_cubemap = loadCubemap(face_pointers);
    }
    else { scene->skybox_cubemap = 0; }
    engine->camera.physicsBody = Physics_CreatePlayerCapsule(engine->physicsWorld, 0.4f, PLAYER_HEIGHT_NORMAL, 80.0f, scene->playerStart.position);
    engine->camera.position = scene->playerStart.position;
    engine->camera.yaw = scene->playerStart.yaw;
    engine->camera.pitch = scene->playerStart.pitch;

    char map_name_sanitized[128];
    char* dot = strrchr(scene->mapPath, '.');
    if (dot) {
        size_t len = dot - scene->mapPath;
        strncpy(map_name_sanitized, scene->mapPath, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, scene->mapPath);
    }

    Scene_LoadAmbientProbes(scene);

    for (int i = 0; i < scene->numLogicEntities; ++i) {
        if (strcmp(scene->logicEntities[i].classname, "logic_auto") == 0) {
            IO_FireOutput(ENTITY_LOGIC, i, "OnMapSpawn", 0.0f, NULL);
        }
    }

    return true;
}

bool Scene_SaveMap(Scene* scene, Engine* engine, const char* mapPath) {
    char backup_path[256];
    sprintf(backup_path, "%s.bak", mapPath);
    rename(mapPath, backup_path);

    FILE* file = fopen(mapPath, "w");
    if (!file) {
        Console_Printf_Error("Failed to open %s for writing.", mapPath);
        return false;
    }
    fprintf(file, "MAP_VERSION %d\n\n", MAP_VERSION);
    fprintf(file, "lightmap_resolution %d\n", scene->lightmapResolution);
    if (engine) {
        fprintf(file, "player_start %.4f %.4f %.4f %.4f %.4f\n\n", engine->camera.position.x, engine->camera.position.y, engine->camera.position.z, engine->camera.yaw, engine->camera.pitch);
    }
    else {
        fprintf(file, "player_start %.4f %.4f %.4f %.4f %.4f\n\n", scene->playerStart.position.x, scene->playerStart.position.y, scene->playerStart.position.z, scene->playerStart.yaw, scene->playerStart.pitch);
    }
    fprintf(file, "fog_settings %d %.4f %.4f %.4f %.4f %.4f\n\n", (int)scene->fog.enabled, scene->fog.color.x, scene->fog.color.y, scene->fog.color.z, scene->fog.start, scene->fog.end);
    fprintf(file, "post_settings %d %.4f %.4f %.4f %d %.4f %.4f %.4f %d %.4f %.4f %d %.4f %d %.4f %d %.4f\n\n",
        (int)scene->post.enabled, scene->post.crtCurvature, scene->post.vignetteStrength, scene->post.vignetteRadius,
        (int)scene->post.lensFlareEnabled, scene->post.lensFlareStrength, scene->post.scanlineStrength, scene->post.grainIntensity,
        (int)scene->post.dofEnabled, scene->post.dofFocusDistance, scene->post.dofAperture,
        (int)scene->post.chromaticAberrationEnabled, scene->post.chromaticAberrationStrength,
        (int)scene->post.sharpenEnabled, scene->post.sharpenAmount,
        (int)scene->post.bwEnabled, scene->post.bwStrength
    );
    fprintf(file, "skybox %d \"%s\"\n\n", (int)scene->use_cubemap_skybox, scene->skybox_path);
    fprintf(file, "sun %d %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f   %.4f %.4f %.4f %.4f\n\n",
        (int)scene->sun.enabled,
        scene->sun.direction.x, scene->sun.direction.y, scene->sun.direction.z,
        scene->sun.color.x, scene->sun.color.y, scene->sun.color.z,
        scene->sun.intensity,
        scene->sun.windDirection.x, scene->sun.windDirection.y, scene->sun.windDirection.z,
        scene->sun.windStrength);
    fprintf(file, "color_correction %d \"%s\"\n\n", (int)scene->colorCorrection.enabled, scene->colorCorrection.lutPath);
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        fprintf(file, "brush_begin %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", b->pos.x, b->pos.y, b->pos.z, b->rot.x, b->rot.y, b->rot.z, b->scale.x, b->scale.y, b->scale.z);
        if (strlen(b->targetname) > 0) fprintf(file, "  targetname \"%s\"\n", b->targetname);
        if (strlen(b->classname) > 0) fprintf(file, "  classname \"%s\"\n", b->classname);
        if (b->isGrouped && b->groupName[0] != '\0') fprintf(file, "  is_grouped 1 \"%s\"\n", b->groupName);
        fprintf(file, "  mass %.4f\n", b->mass);
        fprintf(file, "  isPhysicsEnabled %d\n", (int)b->isPhysicsEnabled);
        if (strcmp(b->classname, "env_reflectionprobe") == 0) {
            fprintf(file, "  name \"%s\"\n", b->name);
        }
        fprintf(file, "  num_verts %d\n", b->numVertices);
        for (int v = 0; v < b->numVertices; ++v) {
            fprintf(file, "  v %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", v,
                b->vertices[v].pos.x, b->vertices[v].pos.y, b->vertices[v].pos.z,
                b->vertices[v].color.x, b->vertices[v].color.y, b->vertices[v].color.z, b->vertices[v].color.w);
        }
        fprintf(file, "  num_faces %d\n", b->numFaces);
        for (int j = 0; j < b->numFaces; ++j) {
            BrushFace* face = &b->faces[j];
            const char* mat_name = face->material ? face->material->name : "___MISSING___";
            const char* mat2_name = face->material2 ? face->material2->name : "NULL";
            const char* mat3_name = face->material3 ? face->material3->name : "NULL";
            const char* mat4_name = face->material4 ? face->material4->name : "NULL";

            fprintf(file, "  f %d %s %s %s %s %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %d :",
                j, mat_name, mat2_name, mat3_name, mat4_name,
                face->uv_offset.x, face->uv_offset.y, face->uv_rotation, face->uv_scale.x, face->uv_scale.y,
                face->uv_offset2.x, face->uv_offset2.y, face->uv_rotation2, face->uv_scale2.x, face->uv_scale2.y,
                face->uv_offset3.x, face->uv_offset3.y, face->uv_rotation3, face->uv_scale3.x, face->uv_scale3.y,
                face->uv_offset4.x, face->uv_offset4.y, face->uv_rotation4, face->uv_scale4.x, face->uv_scale4.y,
                face->numVertexIndices);
            for (int k = 0; k < face->numVertexIndices; ++k) fprintf(file, " %d", face->vertexIndices[k]);
            fprintf(file, " lightmap_scale %.4f", face->lightmap_scale);
            if (face->isGrouped && face->groupName[0] != '\0') fprintf(file, " is_grouped 1 \"%s\"", face->groupName);
            fprintf(file, "\n");
        }
        if (b->numProperties > 0) {
            fprintf(file, "  properties\n");
            fprintf(file, "  {\n");
            for (int j = 0; j < b->numProperties; ++j) {
                fprintf(file, "    \"%s\" \"%s\"\n", b->properties[j].key, b->properties[j].value);
            }
            fprintf(file, "  }\n");
        }
        fprintf(file, "brush_end\n\n");
    }

    for (int i = 0; i < scene->numObjects; ++i) {
        SceneObject* obj = &scene->objects[i];
        fprintf(file, "gltf_model %s \"%s\" %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f %.4f %.4f %d %d %.4f %.4f %d\n",
            obj->modelPath, obj->targetname, obj->pos.x, obj->pos.y, obj->pos.z,
            obj->rot.x, obj->rot.y, obj->rot.z, obj->scale.x, obj->scale.y, obj->scale.z,
            obj->mass, (int)obj->isPhysicsEnabled, (int)obj->swayEnabled, obj->fadeStartDist, obj->fadeEndDist, (int)obj->casts_shadows);
        if (obj->isGrouped && obj->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", obj->groupName);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numActiveLights; ++i) {
        Light* light = &scene->lights[i];
        const char* cookiePathStr = (strlen(light->cookiePath) > 0) ? light->cookiePath : "none";
        fprintf(file, "light \"%s\" %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %d %d \"%s\" \"%s\"\n",
            light->targetname, (int)light->type, light->position.x, light->position.y, light->position.z, light->rot.x, light->rot.y, light->rot.z,
            light->color.x, light->color.y, light->color.z, light->base_intensity, light->radius,
            light->cutOff, light->outerCutOff, light->shadowFarPlane, light->shadowBias, light->volumetricIntensity,
            light->preset, (int)light->is_static, cookiePathStr, light->custom_style_string);
        if (light->isGrouped && light->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", light->groupName);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numDecals; ++i) {
        Decal* d = &scene->decals[i];
        const char* mat_name = d->material ? d->material->name : "___MISSING___";
        fprintf(file, "decal \"%s\" \"%s\" %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f %.4f\n", mat_name, d->targetname, d->pos.x, d->pos.y, d->pos.z, d->rot.x, d->rot.y, d->rot.z, d->size.x, d->size.y, d->size.z);
        if (d->isGrouped && d->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", d->groupName);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numParticleEmitters; ++i) {
        ParticleEmitter* emitter = &scene->particleEmitters[i];
        fprintf(file, "particle_emitter \"%s\" \"%s\" %d %.4f %.4f %.4f\n", emitter->parFile, emitter->targetname, (int)emitter->on_by_default, emitter->pos.x, emitter->pos.y, emitter->pos.z);
        if (emitter->isGrouped && emitter->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", emitter->groupName);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numSoundEntities; ++i) {
        SoundEntity* s = &scene->soundEntities[i];
        fprintf(file, "sound_entity \"%s\" %s %.4f %.4f %.4f %.4f %.4f %.4f %d %d\n", s->targetname, s->soundPath, s->pos.x, s->pos.y, s->pos.z, s->volume, s->pitch, s->maxDistance, (int)s->is_looping, (int)s->play_on_start);
        if (s->isGrouped && s->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", s->groupName);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numVideoPlayers; ++i) {
        VideoPlayer* vp = &scene->videoPlayers[i];
        fprintf(file, "video_player \"%s\" \"%s\" %d %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n",
            vp->videoPath, vp->targetname, (int)vp->playOnStart, (int)vp->loop,
            vp->pos.x, vp->pos.y, vp->pos.z,
            vp->rot.x, vp->rot.y, vp->rot.z,
            vp->size.x, vp->size.y);
        if (vp->isGrouped && vp->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", vp->groupName);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numParallaxRooms; ++i) {
        ParallaxRoom* p = &scene->parallaxRooms[i];
        fprintf(file, "parallax_room \"%s\" \"%s\" %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n",
            p->cubemapPath, p->targetname,
            p->pos.x, p->pos.y, p->pos.z,
            p->rot.x, p->rot.y, p->rot.z,
            p->size.x, p->size.y,
            p->roomDepth);
        if (p->isGrouped && p->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", p->groupName);
    }
    for (int i = 0; i < scene->numSprites; ++i) {
        Sprite* s = &scene->sprites[i];
        fprintf(file, "sprite \"%s\" %.4f %.4f %.4f %.4f \"%s\"\n",
            s->targetname, s->pos.x, s->pos.y, s->pos.z, s->scale, s->material ? s->material->name : "___MISSING___");
        if (s->isGrouped && s->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", s->groupName);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        fprintf(file, "logic_entity_begin\n");
        fprintf(file, "  classname \"%s\"\n", ent->classname);
        fprintf(file, "  targetname \"%s\"\n", ent->targetname);
        if (ent->isGrouped && ent->groupName[0] != '\0') fprintf(file, "  is_grouped 1 \"%s\"\n", ent->groupName);
        fprintf(file, "  pos %.4f %.4f %.4f\n", ent->pos.x, ent->pos.y, ent->pos.z);
        fprintf(file, "  rot %.4f %.4f %.4f\n", ent->rot.x, ent->rot.y, ent->rot.z);
        fprintf(file, "  runtime_active %d\n", ent->runtime_active);
        fprintf(file, "  runtime_float_a %.4f\n", ent->runtime_float_a);
        fprintf(file, "  runtime_active %d\n", ent->runtime_active);
        fprintf(file, "  runtime_float_a %.4f\n", ent->runtime_float_a);
        fprintf(file, "  runtime_int_a %d\n", ent->runtime_int_a);
        fprintf(file, "  runtime_float_b %.4f\n", ent->runtime_float_b);
        fprintf(file, "  properties\n");
        fprintf(file, "  {\n");
        for (int j = 0; j < ent->numProperties; ++j) {
            fprintf(file, "    \"%s\" \"%s\"\n", ent->properties[j].key, ent->properties[j].value);
        }
        fprintf(file, "  }\n");
        fprintf(file, "logic_entity_end\n\n");
    }
    for (int i = 0; i < g_num_io_connections; ++i) {
        IOConnection* conn = &g_io_connections[i];
        if (conn->active) {
            fprintf(file, "io_connection %d %d \"%s\" \"%s\" \"%s\" %.4f %d %d \"%s\"\n", (int)conn->sourceType, conn->sourceIndex, conn->outputName, conn->targetName, conn->inputName, conn->delay, (int)conn->fireOnce, (int)conn->hasFired, conn->parameter);
        }
    }
    fclose(file);
    return true;
}