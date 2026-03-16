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
    Int currentFaceIndex;
    Int* faceTriangles;
    Int numTriangles;
    Vec3* vertexNormals;
} MikkTSpaceUserdata;

static MikkTSpaceUserdata g_mikk_userdata;

void SceneObject_UpdateMatrix(SceneObject* obj) {
    obj->modelMatrix = Math::create_trs_matrix(obj->pos, obj->rot, obj->scale);
}

void Brush_UpdateMatrix(Brush* b) {
    b->modelMatrix = Math::create_trs_matrix(b->pos, b->rot, b->scale);
}

void Decal_UpdateMatrix(Decal* d) {
    d->modelMatrix = Math::create_trs_matrix(d->pos, d->rot, d->size);
}

void ParallaxRoom_UpdateMatrix(ParallaxRoom* p) {
    p->modelMatrix = Math::create_trs_matrix(p->pos, p->rot, Vec3{ p->size.x, p->size.y, 1.0f });
}

void Brush_FreeData(Brush* b) {
    if (!b) return;
    if (b->vao) { glDeleteVertexArrays(1, &b->vao); b->vao = 0; }
    if (b->vbo) { glDeleteBuffers(1, &b->vbo); b->vbo = 0; }
    if (b->lightmapAtlas) {
        if (b->lightmapAtlasHandle) {
            glMakeTextureHandleNonResidentARB(b->lightmapAtlasHandle);
            b->lightmapAtlasHandle = 0;
        } glDeleteTextures(1, &b->lightmapAtlas); b->lightmapAtlas = 0;
    }
    if (b->directionalLightmapAtlas) {
        if (b->directionalLightmapAtlasHandle) {
            glMakeTextureHandleNonResidentARB(b->directionalLightmapAtlasHandle);
            b->directionalLightmapAtlasHandle = 0;
        } glDeleteTextures(1, &b->directionalLightmapAtlas); b->directionalLightmapAtlas = 0;
    }
    if (b->vertices) { delete[] b->vertices; b->vertices = nullptr; }
    if (b->faces) {
        for (Int i = 0; i < b->numFaces; i++) {
            if (b->faces[i].vertexIndices) {
                delete[] b->faces[i].vertexIndices;
                b->faces[i].vertexIndices = nullptr;
            }
        }
        delete[] b->faces;
        b->faces = nullptr;
    }
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
    memcpy(dest->properties, src->properties, sizeof(KeyValue) * Common::MAX_ENTITY_PROPERTIES);
    dest->numVertices = src->numVertices;
    if (src->numVertices > 0) {
        dest->vertices = new BrushVertex[src->numVertices];
        memcpy(dest->vertices, src->vertices, src->numVertices * sizeof(BrushVertex));
    }
    else {
        dest->vertices = nullptr;
    }

    dest->numFaces = src->numFaces;
    if (src->numFaces > 0) {
        dest->faces = new BrushFace[src->numFaces];
        for (Int i = 0; i < src->numFaces; ++i) {
            dest->faces[i] = src->faces[i];
            if (src->faces[i].numVertexIndices > 0) {
                dest->faces[i].vertexIndices = new Int[src->faces[i].numVertexIndices];
                memcpy(dest->faces[i].vertexIndices, src->faces[i].vertexIndices, src->faces[i].numVertexIndices * sizeof(Int));
            }
            else {
                dest->faces[i].vertexIndices = nullptr;
            }
        }
    }
    else {
        dest->faces = nullptr;
    }

    dest->vao = 0;
    dest->vbo = 0;
    dest->lightmapAtlas = 0;
    dest->directionalLightmapAtlas = 0;
    dest->lightmapAtlasHandle = 0;
    dest->directionalLightmapAtlasHandle = 0;
    dest->totalRenderVertexCount = 0;
    dest->physicsBody = nullptr;
    dest->mass = src->mass;
    dest->isPhysicsEnabled = src->isPhysicsEnabled;
    dest->isGrouped = src->isGrouped;
    dest->casts_shadows = src->casts_shadows;
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

Bool Brush_IsSolid(const Brush* b) {
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

void Brush_GetLocalAABB(const Brush* b, Vec3* out_min, Vec3* out_max) {
    *out_min = Vec3{ FLT_MAX, FLT_MAX, FLT_MAX };
    *out_max = Vec3{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

    if (b->numVertices > 0) {
        for (Int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
            out_min->x = fminf(out_min->x, b->vertices[v_idx].pos.x);
            out_min->y = fminf(out_min->y, b->vertices[v_idx].pos.y);
            out_min->z = fminf(out_min->z, b->vertices[v_idx].pos.z);
            out_max->x = fmaxf(out_max->x, b->vertices[v_idx].pos.x);
            out_max->y = fmaxf(out_max->y, b->vertices[v_idx].pos.y);
            out_max->z = fmaxf(out_max->z, b->vertices[v_idx].pos.z);
        }
    }
    else {
        *out_min = Vec3{ -0.5f, -0.5f, -0.5f };
        *out_max = Vec3{ 0.5f,  0.5f,  0.5f };
    }
}

void Brush_GetWorldAABB(const Brush* b, Vec3* out_min, Vec3* out_max) {
    *out_min = Vec3{ FLT_MAX, FLT_MAX, FLT_MAX };
    *out_max = Vec3{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

    if (b->numVertices == 0) {
        Vec3 local_min = { -0.5f, -0.5f, -0.5f };
        Vec3 local_max = { 0.5f,  0.5f,  0.5f };
        *out_min = Math::mat4_mul_vec3(&b->modelMatrix, local_min);
        *out_max = Math::mat4_mul_vec3(&b->modelMatrix, local_max);
        return;
    }

    if (b->rot.x == 0.0f && b->rot.y == 0.0f && b->rot.z == 0.0f &&
        b->scale.x == 1.0f && b->scale.y == 1.0f && b->scale.z == 1.0f) {
        Vec3 local_min, local_max;
        Brush_GetLocalAABB(b, &local_min, &local_max);
        *out_min = Math::vec3_add(local_min, b->pos);
        *out_max = Math::vec3_add(local_max, b->pos);
    }
    else {
        for (Int i = 0; i < b->numVertices; ++i) {
            Vec3 world_v = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
            out_min->x = fminf(out_min->x, world_v.x);
            out_min->y = fminf(out_min->y, world_v.y);
            out_min->z = fminf(out_min->z, world_v.z);
            out_max->x = fmaxf(out_max->x, world_v.x);
            out_max->y = fmaxf(out_max->y, world_v.y);
            out_max->z = fmaxf(out_max->z, world_v.z);
        }
    }
}

static Int getNumFaces(const SMikkTSpaceContext* pContext) {
    return g_mikk_userdata.numTriangles;
}

static Int getNumVerticesOfFace(const SMikkTSpaceContext* pContext, const Int iFace) {
    return 3;
}

static void getPosition(const SMikkTSpaceContext* pContext, Float fvPosOut[], const Int iFace, const Int iVert) {
    Int vertex_index = g_mikk_userdata.faceTriangles[iFace * 3 + iVert];
    Vec3 pos = g_mikk_userdata.brush->vertices[vertex_index].pos;
    fvPosOut[0] = pos.x;
    fvPosOut[1] = pos.y;
    fvPosOut[2] = pos.z;
}

static void getNormal(const SMikkTSpaceContext* pContext, Float fvNormOut[], const Int iFace, const Int iVert) {
    Int vertex_index = g_mikk_userdata.faceTriangles[iFace * 3 + iVert];
    Vec3 n = g_mikk_userdata.vertexNormals[vertex_index];
    fvNormOut[0] = n.x;
    fvNormOut[1] = n.y;
    fvNormOut[2] = n.z;
}

static void getTexCoord(const SMikkTSpaceContext* pContext, Float fvTexcOut[], const Int iFace, const Int iVert) {
    Int v_idx0 = g_mikk_userdata.faceTriangles[iFace * 3 + 0];
    Int v_idx1 = g_mikk_userdata.faceTriangles[iFace * 3 + 1];
    Int v_idx2 = g_mikk_userdata.faceTriangles[iFace * 3 + 2];
    Vec3 p0 = g_mikk_userdata.brush->vertices[v_idx0].pos;
    Vec3 p1 = g_mikk_userdata.brush->vertices[v_idx1].pos;
    Vec3 p2 = g_mikk_userdata.brush->vertices[v_idx2].pos;
    Vec3 normal_vec = Math::vec3_cross(Math::vec3_sub(p1, p0), Math::vec3_sub(p2, p0));
    Math::vec3_normalize(&normal_vec);

    Int vertex_index = g_mikk_userdata.faceTriangles[iFace * 3 + iVert];
    Vec3 pos = g_mikk_userdata.brush->vertices[vertex_index].pos;
    BrushFace* face = &g_mikk_userdata.brush->faces[g_mikk_userdata.currentFaceIndex];

    Float absX = fabsf(normal_vec.x), absY = fabsf(normal_vec.y), absZ = fabsf(normal_vec.z);
    Int dominant_axis = (absY > absX && absY > absZ) ? 1 : ((absX > absZ) ? 0 : 2);

    Float u, v;
    if (dominant_axis == 0) { u = pos.y; v = pos.z; }
    else if (dominant_axis == 1) { u = pos.x; v = pos.z; }
    else { u = pos.x; v = pos.y; }

    Float rad = face->uv_rotation * (Common::PI / 180.0f);
    Float cos_r = cosf(rad); Float sin_r = sinf(rad);
    fvTexcOut[0] = ((u * cos_r - v * sin_r) / face->uv_scale.x) + face->uv_offset.x;
    fvTexcOut[1] = ((u * sin_r + v * cos_r) / face->uv_scale.y) + face->uv_offset.y;
}

static void setTSpaceBasic(const SMikkTSpaceContext* pContext, const Float fvTangent[], const Float fSign, const Int iFace, const Int iVert) {
    Int vbo_idx = (iFace * 3 + iVert) * 22;
    Float* vbo_data = (Float*)pContext->m_pUserData;
    vbo_data[vbo_idx + 8] = fvTangent[0];
    vbo_data[vbo_idx + 9] = fvTangent[1];
    vbo_data[vbo_idx + 10] = fvTangent[2];
    vbo_data[vbo_idx + 11] = fSign;
}

static Vec2 calculate_texture_uv_for_vertex(const Brush* b, Int face_index, Int vertex_index) {
    BrushFace* face = &b->faces[face_index];
    Vec3 pos = b->vertices[vertex_index].pos;

    Vec3 p0 = b->vertices[face->vertexIndices[0]].pos;
    Vec3 p1 = b->vertices[face->vertexIndices[1]].pos;
    Vec3 p2 = b->vertices[face->vertexIndices[2]].pos;
    Vec3 normal_vec = Math::vec3_cross(Math::vec3_sub(p1, p0), Math::vec3_sub(p2, p0));
    Math::vec3_normalize(&normal_vec);

    Float absX = fabsf(normal_vec.x), absY = fabsf(normal_vec.y), absZ = fabsf(normal_vec.z);
    Int dominant_axis = (absY > absX && absY > absZ) ? 1 : ((absX > absZ) ? 0 : 2);

    Float u, v;
    if (dominant_axis == 0) { u = pos.y; v = pos.z; }
    else if (dominant_axis == 1) { u = pos.x; v = pos.z; }
    else { u = pos.x; v = pos.y; }

    Float rad = face->uv_rotation * (Common::PI / 180.0f);
    Float cos_r = cosf(rad); Float sin_r = sinf(rad);

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

    Vec3* temp_normals = new Vec3[b->numVertices]{};

    for (Int i = 0; i < b->numFaces; ++i) {
        BrushFace* face = &b->faces[i];
        if (face->numVertexIndices < 3) continue;
        for (Int j = 0; j < face->numVertexIndices - 2; ++j) {
            Int idx0 = face->vertexIndices[0];
            Int idx1 = face->vertexIndices[j + 1];
            Int idx2 = face->vertexIndices[j + 2];
            Vec3 p0 = b->vertices[idx0].pos;
            Vec3 p1 = b->vertices[idx1].pos;
            Vec3 p2 = b->vertices[idx2].pos;
            Vec3 face_normal = Math::vec3_cross(Math::vec3_sub(p1, p0), Math::vec3_sub(p2, p0));
            temp_normals[idx0] = Math::vec3_add(temp_normals[idx0], face_normal);
            temp_normals[idx1] = Math::vec3_add(temp_normals[idx1], face_normal);
            temp_normals[idx2] = Math::vec3_add(temp_normals[idx2], face_normal);
        }
    }
    for (Int i = 0; i < b->numVertices; ++i) {
        Math::vec3_normalize(&temp_normals[i]);
    }

    Int total_render_verts = 0;
    for (Int i = 0; i < b->numFaces; ++i) {
        if (b->faces[i].numVertexIndices >= 3) {
            total_render_verts += (b->faces[i].numVertexIndices - 2) * 3;
        }
    }
    b->totalRenderVertexCount = total_render_verts;
    if (total_render_verts == 0) {
        delete[] temp_normals;
        return;
    }

    constexpr Int stride_floats = 32;
    Float* final_vbo_data = new Float[total_render_verts * stride_floats]{};
    if (!final_vbo_data) {
        delete[] temp_normals;
        return;
    }

    SMikkTSpaceInterface mikk_interface = { 0 };
    mikk_interface.m_getNumFaces = getNumFaces;
    mikk_interface.m_getNumVerticesOfFace = getNumVerticesOfFace;
    mikk_interface.m_getPosition = getPosition;
    mikk_interface.m_getNormal = getNormal;
    mikk_interface.m_getTexCoord = getTexCoord;
    mikk_interface.m_setTSpaceBasic = setTSpaceBasic;

    Int atlas_width = 1, atlas_height = 1;
    if (b->lightmapAtlas != 0) {
        glBindTexture(GL_TEXTURE_2D, b->lightmapAtlas);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &atlas_width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &atlas_height);
    }

    Int vbo_vertex_offset = 0;
    for (Int i = 0; i < b->numFaces; ++i) {
        BrushFace* face = &b->faces[i];
        if (face->numVertexIndices < 3) continue;

        Vec2 min_uv = { FLT_MAX, FLT_MAX };
        Vec2 max_uv = { -FLT_MAX, -FLT_MAX };
        for (Int k = 0; k < face->numVertexIndices; k++) {
            Vec2 uv = calculate_texture_uv_for_vertex(b, i, face->vertexIndices[k]);
            min_uv.x = fminf(min_uv.x, uv.x);
            min_uv.y = fminf(min_uv.y, uv.y);
            max_uv.x = fmaxf(max_uv.x, uv.x);
            max_uv.y = fmaxf(max_uv.y, uv.y);
        }
        Vec2 uv_range = { max_uv.x - min_uv.x, max_uv.y - min_uv.y };
        if (uv_range.x < 0.001f) uv_range.x = 1.0f;
        if (uv_range.y < 0.001f) uv_range.y = 1.0f;

        Int num_tris_in_face = face->numVertexIndices - 2;
        Int num_verts_in_face = num_tris_in_face * 3;

        Int* face_tri_indices = new Int[num_verts_in_face];
        for (Int j = 0; j < num_tris_in_face; ++j) {
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

        for (Int j = 0; j < num_verts_in_face; ++j) {
            Int vbo_idx = (vbo_vertex_offset + j) * stride_floats;
            Int vertex_index = face_tri_indices[j];
            BrushVertex vert = b->vertices[vertex_index];
            Vec3 norm = temp_normals[vertex_index];
            Float uv1[2], uv2[2], uv3[2], uv4[2];

            getTexCoord(nullptr, uv1, j / 3, j % 3);
            Vec3 p0 = b->vertices[face_tri_indices[j - (j % 3) + 0]].pos;
            Vec3 p1 = b->vertices[face_tri_indices[j - (j % 3) + 1]].pos;
            Vec3 p2 = b->vertices[face_tri_indices[j - (j % 3) + 2]].pos;
            Vec3 normal_vec = Math::vec3_cross(Math::vec3_sub(p1, p0), Math::vec3_sub(p2, p0)); Math::vec3_normalize(&normal_vec);
            Float absX = fabsf(normal_vec.x), absY = fabsf(normal_vec.y), absZ = fabsf(normal_vec.z);
            Int dominant_axis = (absY > absX && absY > absZ) ? 1 : ((absX > absZ) ? 0 : 2);
            Float u, v;
            if (dominant_axis == 0) { u = vert.pos.y; v = vert.pos.z; }
            else if (dominant_axis == 1) { u = vert.pos.x; v = vert.pos.z; }
            else { u = vert.pos.x; v = vert.pos.y; }
            Float rad2 = face->uv_rotation2 * (Common::PI / 180.0f); Float cos_r2 = cosf(rad2); Float sin_r2 = sinf(rad2);
            uv2[0] = ((u * cos_r2 - v * sin_r2) / face->uv_scale2.x) + face->uv_offset2.x; uv2[1] = ((u * sin_r2 + v * cos_r2) / face->uv_scale2.y) + face->uv_offset2.y;
            Float rad3 = face->uv_rotation3 * (Common::PI / 180.0f); Float cos_r3 = cosf(rad3); Float sin_r3 = sinf(rad3);
            uv3[0] = ((u * cos_r3 - v * sin_r3) / face->uv_scale3.x) + face->uv_offset3.x; uv3[1] = ((u * sin_r3 + v * cos_r3) / face->uv_scale3.y) + face->uv_offset3.y;
            Float rad4 = face->uv_rotation4 * (Common::PI / 180.0f); Float cos_r4 = cosf(rad4); Float sin_r4 = sinf(rad4);
            uv4[0] = ((u * cos_r4 - v * sin_r4) / face->uv_scale4.x) + face->uv_offset4.x; uv4[1] = ((u * sin_r4 + v * cos_r4) / face->uv_scale4.y) + face->uv_offset4.y;

            Vec2 current_tex_uv = calculate_texture_uv_for_vertex(b, i, vertex_index);
            Float local_u = (current_tex_uv.x - min_uv.x) / uv_range.x;
            Float local_v = (current_tex_uv.y - min_uv.y) / uv_range.y;

            Float total_padded_width_uv = face->atlas_coords.z;
            Float total_padded_height_uv = face->atlas_coords.w;

            Float padded_width_px = total_padded_width_uv * atlas_width;
            Float padded_height_px = total_padded_height_uv * atlas_height;

            vert.lightmap_uv.x = face->atlas_coords.x + local_u * face->atlas_coords.z;
            vert.lightmap_uv.y = face->atlas_coords.y + local_v * face->atlas_coords.w;

            memcpy(&final_vbo_data[vbo_idx + 0], &vert.pos, sizeof(Vec3));
            memcpy(&final_vbo_data[vbo_idx + 3], &norm, sizeof(Vec3));
            memcpy(&final_vbo_data[vbo_idx + 6], uv1, sizeof(Vec2));

            memset(&final_vbo_data[vbo_idx + 12], 0, sizeof(Vec4));

            memcpy(&final_vbo_data[vbo_idx + 16], uv2, sizeof(Vec2));
            memcpy(&final_vbo_data[vbo_idx + 18], uv3, sizeof(Vec2));
            memcpy(&final_vbo_data[vbo_idx + 20], uv4, sizeof(Vec2));
            memcpy(&final_vbo_data[vbo_idx + 22], &vert.lightmap_uv, sizeof(Vec2));

            memset(&final_vbo_data[vbo_idx + 24], 0, sizeof(Vec4));

            memcpy(&final_vbo_data[vbo_idx + 28], &vert.color, sizeof(Vec4));
        }
        delete[] face_tri_indices;
        vbo_vertex_offset += num_verts_in_face;
    }

    if (b->vao == 0) { glGenVertexArrays(1, &b->vao); glGenBuffers(1, &b->vbo); }
    glBindVertexArray(b->vao);
    glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
    glBufferData(GL_ARRAY_BUFFER, total_render_verts * stride_floats * sizeof(Float), final_vbo_data, GL_DYNAMIC_DRAW);

    Usize offset = 0;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(0); offset += 3 * sizeof(Float);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(1); offset += 3 * sizeof(Float);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(2); offset += 2 * sizeof(Float);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(3); offset += 4 * sizeof(Float);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(4); offset += 4 * sizeof(Float);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(5); offset += 2 * sizeof(Float);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(6); offset += 2 * sizeof(Float);
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(7); offset += 2 * sizeof(Float);
    glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(8); offset += 2 * sizeof(Float);
    glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(9); offset += 4 * sizeof(Float);
    glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE, stride_floats * sizeof(Float), (void*)offset); glEnableVertexAttribArray(12);

    glBindVertexArray(0);
    delete[] final_vbo_data;
    delete[] temp_normals;
}

void CreateMapBackup(const Char* originalPath) {
    const Char* backup_dir = Cvar_GetString("g_map_backup_path");
    if (!backup_dir || strlen(backup_dir) == 0) {
        return;
    }

    time_t now_for_folder = time(nullptr);
    struct tm* t_folder = localtime(&now_for_folder);
    Char month_folder_name[32];
    strftime(month_folder_name, sizeof(month_folder_name), "%Y_%B", t_folder);

    Char monthly_backup_path[512];
    snprintf(monthly_backup_path, sizeof(monthly_backup_path), "%s/%s", backup_dir, month_folder_name);

    struct stat st = { 0 };
    if (stat(monthly_backup_path, &st) == -1) {
        if (_mkdir(monthly_backup_path) != 0) {
            if (stat(backup_dir, &st) == -1) {
                _mkdir(backup_dir);
            }
            if (_mkdir(monthly_backup_path) != 0) {
                Console::Printf_Error("Failed to create map backup directory: %s", monthly_backup_path);
                return;
            }
        }
    }

    const Char* filename_start = strrchr(originalPath, '/');
    if (!filename_start) filename_start = strrchr(originalPath, '\\');
    if (!filename_start) filename_start = originalPath;
    else filename_start++;

    Char base_name[128];
    strncpy(base_name, filename_start, sizeof(base_name) - 1);
    base_name[sizeof(base_name) - 1] = '\0';
    Char* dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    Char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", t);

    Char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s/%s_%s.map", monthly_backup_path, base_name, timestamp);

    FILE* source = fopen(originalPath, "rb");
    if (!source) {
        Console::Printf_Error("Failed to open source map for backup: %s", originalPath);
        return;
    }

    FILE* dest = fopen(backup_path, "wb");
    if (!dest) {
        Console::Printf_Error("Failed to create backup map file: %s", backup_path);
        fclose(source);
        return;
    }

    Char buffer[4096];
    Usize bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        fwrite(buffer, 1, bytes_read, dest);
    }

    fclose(source);
    fclose(dest);
}