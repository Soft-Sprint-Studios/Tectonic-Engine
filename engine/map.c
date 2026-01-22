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
#include "map.h"
#include "map_lighting.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "math_lib.h"
#include "physics_wrapper.h"
#include "cvar.h"
#include "sound_system.h"
#include "io_system.h"
#include "gl_video_player.h"
#include "gl_console.h"
#include "gl_render_misc.h"
#include "water_manager.h"
#include "mikktspace.h"
#include <float.h>
#include <time.h>
#include <sys/stat.h>

typedef struct {
    Brush* brush;
    int currentFaceIndex;
    int* faceTriangles;
    int numTriangles;
    Vec3* vertexNormals;
} MikkTSpaceUserdata;

static MikkTSpaceUserdata g_mikk_userdata;

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

void Brush_FreeData(Brush* b) {
    if (!b) return;
    if (b->vao) { glDeleteVertexArrays(1, &b->vao); b->vao = 0; }
    if (b->vbo) { glDeleteBuffers(1, &b->vbo); b->vbo = 0; }
    if (b->lightmapAtlas) {
        if (b->lightmapAtlasHandle) {
            glMakeTextureHandleNonResidentARB(b->lightmapAtlasHandle);
            b->lightmapAtlasHandle = 0;
        } glDeleteTextures(1, &b->lightmapAtlas); b->lightmapAtlas = 0; }
    if (b->directionalLightmapAtlas) {
        if (b->directionalLightmapAtlasHandle) {
            glMakeTextureHandleNonResidentARB(b->directionalLightmapAtlasHandle);
            b->directionalLightmapAtlasHandle = 0;
        } glDeleteTextures(1, &b->directionalLightmapAtlas); b->directionalLightmapAtlas = 0; }
    if (b->vertices) { free(b->vertices); b->vertices = NULL; }
    if (b->faces) {
        for (int i = 0; i < b->numFaces; i++) {
            if (b->faces[i].vertexIndices) {
                free(b->faces[i].vertexIndices);
                b->faces[i].vertexIndices = NULL;
            }
        }
        free(b->faces);
        b->faces = NULL;
    }
    if (b->bakedVertexColors) { free(b->bakedVertexColors); b->bakedVertexColors = NULL; }
    if (b->bakedVertexDirections) { free(b->bakedVertexDirections); b->bakedVertexDirections = NULL; }
    b->numVertices = 0;
    b->numFaces = 0;
}

void Brush_DeepCopy(Brush* dest, const Brush* src) {
    dest->pos = src->pos;
    dest->rot = src->rot;
    dest->scale = src->scale;
    dest->modelMatrix = src->modelMatrix;
    strncpy(dest->targetname, src->targetname, sizeof(dest->targetname) - 1);
    dest->targetname[sizeof(dest->targetname) - 1] = '\0';
    dest->cubemapTexture = src->cubemapTexture;
    strncpy(dest->name, src->name, sizeof(dest->name) - 1);
    dest->name[sizeof(dest->name) - 1] = '\0';
    strncpy(dest->classname, src->classname, sizeof(dest->classname) - 1);
    dest->classname[sizeof(dest->classname) - 1] = '\0';
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
            if (src->faces[i].numVertexIndices > 0) {
                dest->faces[i].vertexIndices = malloc(src->faces[i].numVertexIndices * sizeof(int));
                memcpy(dest->faces[i].vertexIndices, src->faces[i].vertexIndices, src->faces[i].numVertexIndices * sizeof(int));
            }
            else {
                dest->faces[i].vertexIndices = NULL;
            }
        }
    }
    else {
        dest->faces = NULL;
    }

    dest->vao = 0;
    dest->vbo = 0;
    dest->lightmapAtlas = 0;
    dest->directionalLightmapAtlas = 0;
    dest->lightmapAtlasHandle = 0;
    dest->directionalLightmapAtlasHandle = 0;
    dest->lightmap_atlas_size = (Vec2){ 0.0, 0.0 };
    dest->totalRenderVertexCount = 0;
    dest->physicsBody = NULL;
    dest->mass = src->mass;
    dest->isPhysicsEnabled = src->isPhysicsEnabled;
    dest->isGrouped = src->isGrouped;
    dest->useVertexLighting = src->useVertexLighting;
    dest->bakedVertexColors = NULL;
    dest->bakedVertexDirections = NULL;
    strncpy(dest->groupName, src->groupName, sizeof(dest->groupName) - 1);
    dest->groupName[sizeof(dest->groupName) - 1] = '\0';

    dest->runtime_playerIsTouching = src->runtime_playerIsTouching;
    dest->runtime_hasFired = src->runtime_hasFired;
    dest->runtime_active = src->runtime_active;
    dest->current_angular_velocity = src->current_angular_velocity;
    dest->target_angular_velocity = src->target_angular_velocity;
    dest->start_pos = src->start_pos;
    dest->end_pos = src->end_pos;
    dest->move_dir = src->move_dir;
    dest->plat_state = src->plat_state;
    dest->wait_timer = src->wait_timer;
    dest->door_state = src->door_state;
    dest->door_start_pos = src->door_start_pos;
    dest->door_end_pos = src->door_end_pos;
    dest->door_move_dir = src->door_move_dir;
    dest->runtime_is_visible = src->runtime_is_visible;
}

bool Brush_IsSolid(const Brush* b) {
    if (!b) return false;

    if (strlen(b->classname) > 0) {
        if (strcmp(b->classname, "func_clip") == 0) {
            return true;
        }
        if (strcmp(b->classname, "func_rotating") == 0) {
            return true;
        }
        if (strcmp(b->classname, "func_plat") == 0) {
            return true;
        }
        if (strcmp(b->classname, "func_door") == 0) {
            return true;
        }
        if (strcmp(b->classname, "func_wall_toggle") == 0) {
            return true;
        }
        if (strcmp(b->classname, "func_pendulum") == 0) {
            return true;
        }
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

    const int stride_floats = 32;
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

            if (b->useVertexLighting && b->bakedVertexColors) {
                memcpy(&final_vbo_data[vbo_idx + 12], &b->bakedVertexColors[vertex_index], sizeof(Vec4));
            }
            else {
                memset(&final_vbo_data[vbo_idx + 12], 0, sizeof(Vec4));
                final_vbo_data[vbo_idx + 15] = 0.0f;
            }

            memcpy(&final_vbo_data[vbo_idx + 16], uv2, sizeof(Vec2));
            memcpy(&final_vbo_data[vbo_idx + 18], uv3, sizeof(Vec2));
            memcpy(&final_vbo_data[vbo_idx + 20], uv4, sizeof(Vec2));
            memcpy(&final_vbo_data[vbo_idx + 22], &vert.lightmap_uv, sizeof(Vec2));

            if (b->useVertexLighting && b->bakedVertexDirections) {
                memcpy(&final_vbo_data[vbo_idx + 24], &b->bakedVertexDirections[vertex_index], sizeof(Vec4));
            }
            else {
                memset(&final_vbo_data[vbo_idx + 24], 0, sizeof(Vec4));
                final_vbo_data[vbo_idx + 27] = 0.0f;
            }

            memcpy(&final_vbo_data[vbo_idx + 28], &vert.color, sizeof(Vec4));
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
    glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(8); offset += 2 * sizeof(float);
    glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(9); offset += 4 * sizeof(float);
    glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride_floats * sizeof(float), (void*)offset); glEnableVertexAttribArray(12);

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
            if (scene->objects[i].lightmapHandle) {
                glMakeTextureHandleNonResidentARB(scene->objects[i].lightmapHandle);
                glDeleteTextures(1, &scene->objects[i].lightmapTexture);
            }
            if (scene->objects[i].dirLightmapHandle) {
                glMakeTextureHandleNonResidentARB(scene->objects[i].dirLightmapHandle);
                glDeleteTextures(1, &scene->objects[i].dirLightmapTexture);
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

    for (int i = 0; i < scene->numLogicEntities; ++i) {
        if (scene->logicEntities[i].monitor_fbo != 0) {
            glDeleteFramebuffers(1, &scene->logicEntities[i].monitor_fbo);
            glDeleteTextures(1, &scene->logicEntities[i].monitor_texture);
            glDeleteRenderbuffers(1, &scene->logicEntities[i].monitor_depth);
            scene->logicEntities[i].monitor_fbo = 0;
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
    scene->originalMapPath[0] = '\0';
    scene->static_shadows_generated = false;
    scene->playerStart.position = (Vec3){ 0, 5, 0 };
    if (engine) {
        engine->camera.health = 100.0f;
        engine->camera.radiation_level = 0.0f;
        engine->flashlight_on = false;
    }
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

bool Scene_LoadMap(Scene* scene, Renderer* renderer, const char* mapPath, Engine* engine) {
    FILE* file = fopen(mapPath, "r");
    if (!file) {
        Console_Printf_Error("Could not find map file: %s", mapPath);
        return false;
    }

    char version_line[256];
    int map_file_version = 0;

    if (fgets(version_line, sizeof(version_line), file) &&
        sscanf(version_line, "MAP_VERSION %d", &map_file_version) == 1) {

        if (map_file_version < MIN_MAP_VERSION || map_file_version > MAP_VERSION) {
            Console_Printf_Error("Map version unsupported! Map is v%d, supported range is v%d–v%d.",
                map_file_version, MIN_MAP_VERSION, MAP_VERSION);
            fclose(file);
            return false;
        }
    }
    else {
        Console_Printf_Error("Invalid or missing map version.");
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
        else if (strcmp(keyword, "player_status") == 0) {
            int flashlight_on_int = 0;
            if (sscanf(line, "%*s %f %f %d", &engine->camera.health, &engine->camera.radiation_level, &flashlight_on_int) == 3) {
                engine->flashlight_on = (bool)flashlight_on_int;
            }
        }
        else if (strcmp(keyword, "original_map_path") == 0) {
            sscanf(line, "%*s \"%255[^\"]\"", scene->originalMapPath);
        }
        else if (strcmp(keyword, "lightmap_resolution") == 0) {
            sscanf(line, "%*s %d", &scene->lightmapResolution);
        }
        else if (strcmp(keyword, "post_settings") == 0) {
            int enabled_int, flare_int, dof_enabled_int, ca_enabled_int, sharpen_enabled_int, bw_enabled_int, invert_enabled_int;
            sscanf(line, "%*s %d %f %f %f %d %f %f %f %d %f %f %d %f %d %f %d %f %d %f", &enabled_int, &scene->post.crtCurvature, &scene->post.vignetteStrength,
                &scene->post.vignetteRadius, &flare_int, &scene->post.lensFlareStrength, &scene->post.scanlineStrength, &scene->post.grainIntensity,
                &dof_enabled_int, &scene->post.dofFocusDistance, &scene->post.dofAperture, &ca_enabled_int, &scene->post.chromaticAberrationStrength, &sharpen_enabled_int, &scene->post.sharpenAmount,
                &bw_enabled_int, &scene->post.bwStrength, &invert_enabled_int, &scene->post.invertStrength);
            scene->post.enabled = (bool)enabled_int;
            scene->post.lensFlareEnabled = (bool)flare_int;
            scene->post.dofEnabled = (bool)dof_enabled_int;
            scene->post.chromaticAberrationEnabled = (bool)ca_enabled_int;
            scene->post.sharpenEnabled = (bool)sharpen_enabled_int;
            scene->post.bwEnabled = (bool)bw_enabled_int;
            scene->post.invertEnabled = (bool)invert_enabled_int;
        }
        else if (strcmp(keyword, "skybox") == 0) {
            int use_cubemap_int = 0;
            sscanf(line, "%*s %d \"%127[^\"]\"", &use_cubemap_int, scene->skybox_path);
            scene->use_cubemap_skybox = (bool)use_cubemap_int;
        }
        else if (strcmp(keyword, "sun") == 0) {
            int enabled_int;
            int args_count = sscanf(line, "%*s %d %f %f %f %f %f %f %f %f %f %f %f %f",
                &enabled_int,
                &scene->sun.direction.x, &scene->sun.direction.y, &scene->sun.direction.z,
                &scene->sun.color.x, &scene->sun.color.y, &scene->sun.color.z,
                &scene->sun.intensity,
                &scene->sun.windDirection.x, &scene->sun.windDirection.y, &scene->sun.windDirection.z, &scene->sun.windStrength,
                &scene->sun.volumetricIntensity);

            scene->sun.enabled = (bool)enabled_int;
            vec3_normalize(&scene->sun.direction);

            if (args_count < 13) {
                scene->sun.volumetricIntensity = 0.0f;
            }
        }
        else if (strcmp(keyword, "color_correction") == 0) {
            int enabled_int = 0;
            sscanf(line, "%*s %d \"%127[^\"]\"", &enabled_int, scene->colorCorrection.lutPath);
            scene->colorCorrection.enabled = (bool)enabled_int;
            if (scene->colorCorrection.enabled && strlen(scene->colorCorrection.lutPath) > 0) {
                scene->colorCorrection.lutTexture = loadTexture(scene->colorCorrection.lutPath, false, TEXTURE_LOAD_CONTEXT_WORLD);
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
            b->casts_shadows = true;
            char water_def_name[64] = "";
            sscanf(line, "%*s %f %f %f %f %f %f %f %f %f", &b->pos.x, &b->pos.y, &b->pos.z, &b->rot.x, &b->rot.y, &b->rot.z, &b->scale.x, &b->scale.y, &b->scale.z);
            while (fgets(line, sizeof(line), file) && strncmp(line, "brush_end", 9) != 0) {
                int dummy_int;
                int active_int, has_fired_int, touching_int, door_state_int, plat_state_int;
                if (sscanf(line, " runtime_states %d %d %d %f %d %d", &active_int, &has_fired_int, &touching_int, &b->wait_timer, &door_state_int, &plat_state_int) == 6) {
                    b->runtime_active = (bool)active_int;
                    b->runtime_hasFired = (bool)has_fired_int;
                    b->runtime_playerIsTouching = (bool)touching_int;
                    b->door_state = (DoorState)door_state_int;
                    b->plat_state = (PlatState)plat_state_int;
                    continue;
                }
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
                        }
                        else {
                            b->faces[i].isGrouped = false;
                            b->faces[i].groupName[0] = '\0';
                        }

                        sscanf(line, " f %*d %63s %63s %63s %63s %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d",
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
                else if (sscanf(line, " useVertexLighting %d", &dummy_int) == 1) { b->useVertexLighting = (bool)dummy_int; }
                else if (sscanf(line, " casts_shadows %d", &dummy_int) == 1) { b->casts_shadows = (bool)dummy_int; }
                else if (sscanf(line, " classname \"%63[^\"]\"", b->classname) == 1) {}
                else if (strstr(line, "properties")) {
                    b->numProperties = 0;
                    while (fgets(line, sizeof(line), file) && !strstr(line, "}")) {
                        if (b->numProperties < MAX_ENTITY_PROPERTIES) {
                            if (sscanf(line, " \"%63[^\"]\" \"%1023[^\"]\"", b->properties[b->numProperties].key, b->properties[b->numProperties].value) == 2) {
                                b->numProperties++;
                            }
                        }
                    }
                }
                else if (sscanf(line, " is_grouped %d \"%63[^\"]\"", &dummy_int, b->groupName) == 2) {
                    b->isGrouped = (bool)dummy_int;
                }
                else {
                    b->groupName[0] = '\0';
                }
            }
            char map_name_sanitized[128];
            const char* m_last_slash = strrchr(scene->originalMapPath, '/');
            const char* m_last_bslash = strrchr(scene->originalMapPath, '\\');
            const char* m_filename = (m_last_slash > m_last_bslash) ? m_last_slash + 1 : (m_last_bslash ? m_last_bslash + 1 : scene->originalMapPath);
            const char* m_dot = strrchr(m_filename, '.');
            if (m_dot) {
                size_t len = m_dot - m_filename;
                strncpy(map_name_sanitized, m_filename, len);
                map_name_sanitized[len] = '\0';
            }
            else {
                strcpy(map_name_sanitized, m_filename);
            }
            if (strcmp(b->classname, "env_reflectionprobe") == 0) {
                const char* faces_suffixes[] = { "px", "nx", "py", "ny", "pz", "nz" };
                char face_paths[6][256];
                for (int i = 0; i < 6; ++i)  sprintf(face_paths[i], "cubemaps/%s/%s_%s.png", map_name_sanitized, b->name, faces_suffixes[i]);
                const char* face_pointers[6];
                for (int i = 0; i < 6; ++i) face_pointers[i] = face_paths[i];
                b->cubemapTexture = loadCubemap(face_pointers);
            }
            Brush_UpdateMatrix(b);
            Brush_GenerateLightmapAtlas(b, map_name_sanitized, scene->numBrushes, scene->lightmapResolution);
            if (b->useVertexLighting) {
                Brush_LoadVertexLighting(b, scene->numBrushes, scene->originalMapPath);
                Brush_LoadVertexDirectionalLighting(b, scene->numBrushes, scene->originalMapPath);
            }
            Brush_CreateRenderData(b);
            if (Brush_IsSolid(b) && b->numVertices > 0) {
                if (strcmp(b->classname, "func_plat") == 0) {
                    b->start_pos = b->pos;
                    float height = atof(Brush_GetProperty(b, "height", "0"));
                    b->end_pos = (Vec3){ b->pos.x, b->pos.y + height, b->pos.z };
                    b->move_dir = vec3_sub(b->end_pos, b->start_pos);
                    vec3_normalize(&b->move_dir);
                    b->physicsBody = Physics_CreateKinematicBrush(engine->physicsWorld, (const float*)b->vertices, b->numVertices, b->modelMatrix);
                }
                else if (b->mass > 0.0f) {
                    b->physicsBody = Physics_CreateDynamicBrush(engine->physicsWorld, (const float*)&b->vertices->pos, b->numVertices, sizeof(BrushVertex), b->mass, b->modelMatrix);
                    if (!b->isPhysicsEnabled) Physics_ToggleCollision(engine->physicsWorld, b->physicsBody, false);
                }
                else {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int i = 0; i < b->numVertices; i++) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
            }
            b->current_angular_velocity = 0.0f;
            b->target_angular_velocity = 0.0f;
            b->plat_state = PLAT_STATE_BOTTOM;
            b->wait_timer = 0.0f;
            if (strcmp(b->classname, "func_rotating") == 0) {
                if (atoi(Brush_GetProperty(b, "StartON", "1")) != 0) {
                    b->target_angular_velocity = atof(Brush_GetProperty(b, "speed", "10"));
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
            int use_lightmap_int = 0;
            float lmap_scale = 1.0f;
            sscanf(p, "%f %f %f %f %f %f %f %f %f %f %d %d %f %f %d %d %f",
                &newObj->pos.x, &newObj->pos.y, &newObj->pos.z,
                &newObj->rot.x, &newObj->rot.y, &newObj->rot.z,
                &newObj->scale.x, &newObj->scale.y, &newObj->scale.z,
                &newObj->mass, &phys_enabled_int, &sway_enabled_int,
                &newObj->fadeStartDist, &newObj->fadeEndDist, &casts_shadows_int,
                &use_lightmap_int, &lmap_scale);
            newObj->casts_shadows = (bool)casts_shadows_int;
            newObj->useLightmap = (bool)use_lightmap_int;
            newObj->lightmapScale = (lmap_scale > 0.0f) ? lmap_scale : 1.0f;
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
            if (newObj->useLightmap) {
                SceneObject_LoadLightmaps(newObj, scene->numObjects - 1, scene->originalMapPath);
            }
            else {
                SceneObject_LoadVertexLighting(newObj, scene->numObjects - 1, scene->originalMapPath);
                SceneObject_LoadVertexDirectionalLighting(newObj, scene->numObjects - 1, scene->originalMapPath);
            }
            if (!newObj->model) { scene->numObjects--; continue; }
            if (newObj->mass > 0.0f) { newObj->physicsBody = Physics_CreateDynamicConvexHull(engine->physicsWorld, newObj->model->combinedVertexData, newObj->model->totalVertexCount, newObj->mass, newObj->modelMatrix); if (!newObj->isPhysicsEnabled) Physics_ToggleCollision(engine->physicsWorld, newObj->physicsBody, false); }
            else if (newObj->model && newObj->model->combinedVertexData && newObj->model->totalIndexCount > 0) { Mat4 physics_transform = create_trs_matrix(newObj->pos, newObj->rot, (Vec3) { 1.0f, 1.0f, 1.0f }); newObj->physicsBody = Physics_CreateStaticTriangleMesh(engine->physicsWorld, newObj->model->combinedVertexData, newObj->model->totalVertexCount, newObj->model->combinedIndexData, newObj->model->totalIndexCount, physics_transform, newObj->scale); }
        }
        else if (strcmp(keyword, "light") == 0) {
            if (scene->numActiveLights >= MAX_LIGHTS) continue;
            Light* light = &scene->lights[scene->numActiveLights];
            memset(light, 0, sizeof(Light));

            int type_int = 0, preset_int = 0, is_static_int = 0, is_static_shadow_int = 0, is_on_int = 1;
            char* p = line + strlen(keyword);

            while (*p && isspace(*p)) p++;
            if (*p == '"') {
                p++;
                char* end = strchr(p, '"');
                if (end) {
                    size_t len = end - p;
                    if (len < sizeof(light->targetname)) strncpy(light->targetname, p, len);
                    light->targetname[len] = '\0';
                    p = end + 1;
                }
            }

            sscanf(p, "%d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d %d %d %f %f",
                &type_int,
                &light->position.x, &light->position.y, &light->position.z,
                &light->rot.x, &light->rot.y, &light->rot.z,
                &light->color.x, &light->color.y, &light->color.z,
                &light->base_intensity,
                &light->radius,
                &light->cutOff, &light->outerCutOff,
                &light->shadowFarPlane, &light->shadowBias,
                &light->volumetricIntensity,
                &preset_int, &is_static_int, &is_static_shadow_int,
                &light->width, &light->height
            );

            char* cookie_start = strchr(p, '"');
            if (cookie_start) {
                p = cookie_start + 1;
                char* end = strchr(p, '"');
                if (end) {
                    size_t len = end - p;
                    if (len < sizeof(light->cookiePath)) strncpy(light->cookiePath, p, len);
                    light->cookiePath[len] = '\0';
                    p = end + 1;
                }
            }

            char* style_start = (p) ? strchr(p, '"') : NULL;
            if (style_start) {
                p = style_start + 1;
                char* end = strchr(p, '"');
                if (end) {
                    size_t len = end - p;
                    if (len < sizeof(light->custom_style_string)) strncpy(light->custom_style_string, p, len);
                    light->custom_style_string[len] = '\0';
                    p = end + 1;
                }
            }

            if (p) sscanf(p, "%d", &is_on_int);

            light->preset = preset_int;
            light->type = (LightType)type_int;
            light->is_on = (bool)is_on_int;
            light->is_static = is_static_int;
            light->is_static_shadow = (bool)is_static_shadow_int;
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
                d->lightmap_scale = 1.0f;
                d->uv_scale = (Vec2){ 1.0f, 1.0f };
                d->uv_offset = (Vec2){ 0.0f, 0.0f };
                d->uv_rotation = 0.0f;
                char* p = line + strlen(keyword);
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; char* end = strchr(p, '"'); if (end) { strncpy(mat_name, p, end - p); mat_name[end - p] = '\0'; p = end + 1; } }
                else { char* end = p; while (*end && !isspace(*end)) end++; strncpy(mat_name, p, end - p); mat_name[end - p] = '\0'; p = end; }
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; char* end = strchr(p, '"'); if (end) { strncpy(d->targetname, p, end - p); d->targetname[end - p] = '\0'; p = end + 1; } }
                sscanf(p, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                    &d->pos.x, &d->pos.y, &d->pos.z,
                    &d->rot.x, &d->rot.y, &d->rot.z,
                    &d->size.x, &d->size.y, &d->size.z,
                    &d->lightmap_scale,
                    &d->uv_offset.x, &d->uv_offset.y,
                    &d->uv_scale.x, &d->uv_scale.y,
                    &d->uv_rotation
                );
                if (d->lightmap_scale <= 0.0f) d->lightmap_scale = 1.0f;
                if (d->uv_scale.x == 0.0f) d->uv_scale.x = 1.0f;
                if (d->uv_scale.y == 0.0f) d->uv_scale.y = 1.0f;
                d->material = TextureManager_FindMaterial(mat_name);
                long current_pos = ftell(file); char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, d->groupName); d->isGrouped = (bool)grouped_int;
                } else {
                    d->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                Decal_UpdateMatrix(d);
                char map_name_sanitized[128];
                const char* last_slash = strrchr(scene->mapPath, '/');
                const char* last_bslash = strrchr(scene->mapPath, '\\');
                const char* map_filename = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : scene->mapPath);
                const char* dot = strrchr(map_filename, '.');
                if (dot) {
                    size_t len = dot - map_filename;
                    strncpy(map_name_sanitized, scene->originalMapPath, len);
                    map_name_sanitized[len] = '\0';
                }
                else {
                    strcpy(map_name_sanitized, scene->originalMapPath);
                }
                Decal_LoadLightmaps(d, map_name_sanitized, scene->numDecals);
                scene->numDecals++;
            }
        }
        else if (strcmp(keyword, "sound_entity") == 0) {
            if (scene->numSoundEntities < MAX_SOUNDS) {
                SoundEntity* s = &scene->soundEntities[scene->numSoundEntities];
                memset(s, 0, sizeof(SoundEntity));
                int is_looping_int = 0, play_on_start_int = 0, is_global_int = 0;
                char* p = line + strlen(keyword); while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; char* end = strchr(p, '"'); if (end) { size_t len = end - p; if (len < sizeof(s->targetname)) { strncpy(s->targetname, p, len); s->targetname[len] = '\0'; } p = end + 1; } }
                while (*p && isspace(*p)) p++;
                char* path_end = p; while (*path_end && !isspace(*path_end)) path_end++; size_t path_len = path_end - p;
                if (path_len < sizeof(s->soundPath)) { strncpy(s->soundPath, p, path_len); s->soundPath[path_len] = '\0'; } p = path_end;
                if (map_file_version >= 18) {
                    sscanf(p, "%f %f %f %f %f %f %d %d %d", &s->pos.x, &s->pos.y, &s->pos.z, &s->volume, &s->pitch, &s->maxDistance, &is_looping_int, &play_on_start_int, &is_global_int);
                }
                else {
                    sscanf(p, "%f %f %f %f %f %f %d %d", &s->pos.x, &s->pos.y, &s->pos.z, &s->volume, &s->pitch, &s->maxDistance, &is_looping_int, &play_on_start_int);
                    is_global_int = 0;
                }
                s->isGlobal = (bool)is_global_int;
                s->is_looping = (bool)is_looping_int; s->play_on_start = (bool)play_on_start_int;
                long current_pos = ftell(file); char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, s->groupName); s->isGrouped = (bool)grouped_int;
                } else {
                    s->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                s->bufferID = SoundSystem_LoadSound(s->soundPath);
                if (s->play_on_start) {
                    s->sourceID = SoundSystem_PlaySound(s->bufferID, s->pos, s->volume, s->pitch, s->maxDistance, s->is_looping);
                    SoundSystem_SetSourceIsGlobal(s->sourceID, s->isGlobal);
                }
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
                        if (ent->numProperties < MAX_ENTITY_PROPERTIES) {
                            if (sscanf(line, " \"%63[^\"]\" \"%1023[^\"]\"", ent->properties[ent->numProperties].key, ent->properties[ent->numProperties].value) == 2) {
                                ent->numProperties++;
                            }
                        }
                    }
                }
            }
            if (strcmp(ent->classname, "env_beam") == 0) {
                const char* starton_val = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_val) != 0);
            }
            if (strcmp(ent->classname, "env_fog") == 0 || strcmp(ent->classname, "env_blackhole") == 0) {
                const char* starton_val = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_val) != 0);
            }
            if (strcmp(ent->classname, "logic_random") == 0) { if (strcmp(LogicEntity_GetProperty(ent, "is_default_enabled", "0"), "1") == 0) { ent->runtime_active = true; } }
            else if (strcmp(ent->classname, "env_blackhole") == 0) {
                const char* starton_str = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_str) == 1);
            }
            else if (strcmp(ent->classname, "env_glow") == 0) {
                const char* starton_val = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_val) != 0);
            }
            if (strcmp(ent->classname, "logic_repeat") == 0) {
                if (atoi(LogicEntity_GetProperty(ent, "StartON", "0")) != 0) {
                    ent->runtime_active = true;
                    ent->runtime_float_a = atof(LogicEntity_GetProperty(ent, "delay", "1.0"));
                    ent->runtime_int_a = atoi(LogicEntity_GetProperty(ent, "repeats", "-1"));
                }
            }
            else if (strcmp(ent->classname, "logic_branch") == 0) {
                ent->runtime_int_a = atoi(LogicEntity_GetProperty(ent, "InitialValue", "0"));
            }
            if (strcmp(ent->classname, "env_overlay") == 0) {
                const char* starton_val = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_val) != 0);
            }
            if (strcmp(ent->classname, "point_radiation_source") == 0) {
                const char* starton_val = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_val) != 0);
            }
            scene->numLogicEntities++;
        }
        else if (strcmp(keyword, "io_connection") == 0) {
            if (g_num_io_connections < MAX_IO_CONNECTIONS) {
                IOConnection* conn = &g_io_connections[g_num_io_connections];
                conn->active = true; conn->parameter[0] = '\0';
                int type_int, fire_once_int, has_fired_int = 0;
                int items_scanned = sscanf(line, "%*s %d %d \"%63[^\"]\" \"%63[^\"]\" \"%63[^\"]\" %f %d %d \"%63[^\"]\"", &type_int, &conn->sourceIndex, conn->outputName, conn->targetName, conn->inputName, &conn->delay, &fire_once_int, &has_fired_int, conn->parameter);
                conn->sourceType = (EntityType)type_int; conn->fireOnce = (bool)fire_once_int; conn->hasFired = (bool)has_fired_int;
                g_num_io_connections++;
            }
        }
    }
    fclose(file);
    if (scene->originalMapPath[0] == '\0') {
        strncpy(scene->originalMapPath, scene->mapPath, sizeof(scene->originalMapPath) - 1);
        scene->originalMapPath[sizeof(scene->originalMapPath) - 1] = '\0';
    }
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
        strncpy(map_name_sanitized, scene->originalMapPath, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, scene->originalMapPath);
    }
    Scene_LoadAmbientProbes(scene);

    for (int i = 0; i < scene->numLogicEntities; ++i) {
        if (strcmp(scene->logicEntities[i].classname, "logic_auto") == 0) {
            IO_FireOutput(ENTITY_LOGIC, i, "OnMapSpawn", 0.0f, NULL);
        }
    }

    return true;
}

static void CreateMapBackup(const char* originalPath) {
    const char* backup_dir = Cvar_GetString("g_map_backup_path");
    if (!backup_dir || strlen(backup_dir) == 0) {
        return;
    }

    time_t now_for_folder = time(NULL);
    struct tm* t_folder = localtime(&now_for_folder);
    char month_folder_name[32];
    strftime(month_folder_name, sizeof(month_folder_name), "%Y_%B", t_folder);

    char monthly_backup_path[512];
    snprintf(monthly_backup_path, sizeof(monthly_backup_path), "%s/%s", backup_dir, month_folder_name);

    struct stat st = { 0 };
    if (stat(monthly_backup_path, &st) == -1) {
        if (_mkdir(monthly_backup_path) != 0) {
            if (stat(backup_dir, &st) == -1) {
                _mkdir(backup_dir);
            }
            if (_mkdir(monthly_backup_path) != 0) {
                Console_Printf_Error("Failed to create map backup directory: %s", monthly_backup_path);
                return;
            }
        }
    }

    const char* filename_start = strrchr(originalPath, '/');
    if (!filename_start) filename_start = strrchr(originalPath, '\\');
    if (!filename_start) filename_start = originalPath;
    else filename_start++;

    char base_name[128];
    strncpy(base_name, filename_start, sizeof(base_name) - 1);
    base_name[sizeof(base_name) - 1] = '\0';
    char* dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", t);

    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s/%s_%s.map", monthly_backup_path, base_name, timestamp);

    FILE* source = fopen(originalPath, "rb");
    if (!source) {
        Console_Printf_Error("Failed to open source map for backup: %s", originalPath);
        return;
    }

    FILE* dest = fopen(backup_path, "wb");
    if (!dest) {
        Console_Printf_Error("Failed to create backup map file: %s", backup_path);
        fclose(source);
        return;
    }

    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        fwrite(buffer, 1, bytes_read, dest);
    }

    fclose(source);
    fclose(dest);
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
    fprintf(file, "original_map_path \"%s\"\n\n", scene->originalMapPath);
    fprintf(file, "lightmap_resolution %d\n", scene->lightmapResolution);
    if (engine) {
        fprintf(file, "player_start %.4f %.4f %.4f %.4f %.4f\n\n", engine->camera.position.x, engine->camera.position.y, engine->camera.position.z, engine->camera.yaw, engine->camera.pitch);
        fprintf(file, "player_status %.2f %.2f %d\n\n", engine->camera.health, engine->camera.radiation_level, (int)engine->flashlight_on);
    }
    else {
        fprintf(file, "player_start %.4f %.4f %.4f %.4f %.4f\n\n", scene->playerStart.position.x, scene->playerStart.position.y, scene->playerStart.position.z, scene->playerStart.yaw, scene->playerStart.pitch);
    }
    fprintf(file, "post_settings %d %.4f %.4f %.4f %d %.4f %.4f %.4f %d %.4f %.4f %d %.4f %d %.4f %d %.4f\n\n",
        (int)scene->post.enabled, scene->post.crtCurvature, scene->post.vignetteStrength, scene->post.vignetteRadius,
        (int)scene->post.lensFlareEnabled, scene->post.lensFlareStrength, scene->post.scanlineStrength, scene->post.grainIntensity,
        (int)scene->post.dofEnabled, scene->post.dofFocusDistance, scene->post.dofAperture,
        (int)scene->post.chromaticAberrationEnabled, scene->post.chromaticAberrationStrength,
        (int)scene->post.sharpenEnabled, scene->post.sharpenAmount,
        (int)scene->post.bwEnabled, scene->post.bwStrength,
        (int)scene->post.invertEnabled, scene->post.invertStrength
    );
    fprintf(file, "skybox %d \"%s\"\n\n", (int)scene->use_cubemap_skybox, scene->skybox_path);
    fprintf(file, "sun %d %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f   %.4f %.4f %.4f %.4f %.4f\n\n",
        (int)scene->sun.enabled,
        scene->sun.direction.x, scene->sun.direction.y, scene->sun.direction.z,
        scene->sun.color.x, scene->sun.color.y, scene->sun.color.z,
        scene->sun.intensity,
        scene->sun.windDirection.x, scene->sun.windDirection.y, scene->sun.windDirection.z,
        scene->sun.windStrength,
        scene->sun.volumetricIntensity);
    fprintf(file, "color_correction %d \"%s\"\n\n", (int)scene->colorCorrection.enabled, scene->colorCorrection.lutPath);
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        fprintf(file, "brush_begin %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", b->pos.x, b->pos.y, b->pos.z, b->rot.x, b->rot.y, b->rot.z, b->scale.x, b->scale.y, b->scale.z);
        if (engine) {
            fprintf(file, "  runtime_states %d %d %d %f %d %d\n", (int)b->runtime_active, (int)b->runtime_hasFired, (int)b->runtime_playerIsTouching, b->wait_timer, (int)b->door_state, (int)b->plat_state);
        }
        if (strlen(b->targetname) > 0) fprintf(file, "  targetname \"%s\"\n", b->targetname);
        if (strlen(b->classname) > 0) fprintf(file, "  classname \"%s\"\n", b->classname);
        if (b->isGrouped && b->groupName[0] != '\0') fprintf(file, "  is_grouped 1 \"%s\"\n", b->groupName);
        fprintf(file, "  mass %.4f\n", b->mass);
        fprintf(file, "  isPhysicsEnabled %d\n", (int)b->isPhysicsEnabled);
        fprintf(file, "  useVertexLighting %d\n", (int)b->useVertexLighting);
        fprintf(file, "  casts_shadows %d\n", (int)b->casts_shadows);
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
        fprintf(file, "gltf_model %s \"%s\" %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f %.4f %.4f %d %d %.4f %.4f %d %d %.4f\n",
            obj->modelPath, obj->targetname, obj->pos.x, obj->pos.y, obj->pos.z,
            obj->rot.x, obj->rot.y, obj->rot.z, obj->scale.x, obj->scale.y, obj->scale.z,
            obj->mass, (int)obj->isPhysicsEnabled, (int)obj->swayEnabled, obj->fadeStartDist, obj->fadeEndDist, (int)obj->casts_shadows,
            (int)obj->useLightmap, obj->lightmapScale);
        if (obj->isGrouped && obj->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", obj->groupName);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numActiveLights; ++i) {
        Light* light = &scene->lights[i];
        const char* cookiePathStr = (strlen(light->cookiePath) > 0) ? light->cookiePath : "none";
        fprintf(file, "light \"%s\" %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %d %d %d %.4f %.4f \"%s\" \"%s\" %d\n",
            light->targetname, (int)light->type, light->position.x, light->position.y, light->position.z, light->rot.x, light->rot.y, light->rot.z,
            light->color.x, light->color.y, light->color.z, light->base_intensity, light->radius,
            light->cutOff, light->outerCutOff, light->shadowFarPlane, light->shadowBias, light->volumetricIntensity,
            light->preset, (int)light->is_static, (int)light->is_static_shadow, light->width, light->height, cookiePathStr, light->custom_style_string, (int)light->is_on);
        if (light->isGrouped && light->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", light->groupName);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numDecals; ++i) {
        Decal* d = &scene->decals[i];
        const char* mat_name = d->material ? d->material->name : "___MISSING___";
        fprintf(file, "decal \"%s\" \"%s\" %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f %.4f %.4f   %.4f %.4f   %.4f %.4f   %.4f\n",
            mat_name, d->targetname,
            d->pos.x, d->pos.y, d->pos.z,
            d->rot.x, d->rot.y, d->rot.z,
            d->size.x, d->size.y, d->size.z,
            d->lightmap_scale,
            d->uv_offset.x, d->uv_offset.y,
            d->uv_scale.x, d->uv_scale.y,
            d->uv_rotation
        );
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
        fprintf(file, "sound_entity \"%s\" %s %.4f %.4f %.4f %.4f %.4f %.4f %d %d %d\n", s->targetname, s->soundPath, s->pos.x, s->pos.y, s->pos.z, s->volume, s->pitch, s->maxDistance, (int)s->is_looping, (int)s->play_on_start, (int)s->isGlobal);
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
    if (!engine) {
        CreateMapBackup(mapPath);
    }
    return true;
}