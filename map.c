/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
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
#include "mikktspace/mikktspace.h"

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

void Light_InitShadowMap(Light* light) {
    Light_DestroyShadowMap(light);
    glGenFramebuffers(1, &light->shadowFBO);
    glGenTextures(1, &light->shadowMapTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO);
    if (light->type == LIGHT_POINT) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, light->shadowMapTexture);
        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
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
        printf("Shadow Framebuffer not complete! Light Type: %d\n", light->type);

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
    free(b->vertices);
    b->vertices = NULL;
    b->numVertices = 0;
    if (b->faces) {
        for (int i = 0; i < b->numFaces; ++i) {
            free(b->faces[i].vertexIndices);
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
    dest->isTrigger = src->isTrigger;
    dest->isReflectionProbe = src->isReflectionProbe;
    dest->isDSP = src->isDSP;
    dest->cubemapTexture = src->cubemapTexture;
    strcpy(dest->name, src->name);
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
        b->faces[i].numVertexIndices = 4;
        b->faces[i].vertexIndices = malloc(4 * sizeof(int));
        for (int j = 0; j < 4; ++j) b->faces[i].vertexIndices[j] = face_defs[i][j];
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

    BrushVertex* old_verts = b->vertices;
    BrushFace* old_faces = b->faces;
    int old_vert_count = b->numVertices;
    int old_face_count = b->numFaces;

    BrushVertex new_verts[MAX_BRUSH_VERTS * 2];
    int new_vert_count = 0;
    int vert_map[MAX_BRUSH_VERTS];
    memset(vert_map, -1, sizeof(vert_map));

    BrushVertex cap_verts[MAX_BRUSH_FACES + 1];
    int cap_vert_count = 0;

    BrushFace new_faces[MAX_BRUSH_FACES];
    int new_face_count = 0;

    for (int i = 0; i < old_vert_count; ++i) {
        if (side[i] >= 0) {
            vert_map[i] = new_vert_count;
            new_verts[new_vert_count++] = old_verts[i];
        }
    }

    for (int i = 0; i < old_face_count; ++i) {
        BrushFace* face = &old_faces[i];
        int face_verts_idx[MAX_BRUSH_VERTS];
        int face_vert_count = 0;

        for (int j = 0; j < face->numVertexIndices; ++j) {
            int p1_idx = face->vertexIndices[j];
            int p2_idx = face->vertexIndices[(j + 1) % face->numVertexIndices];

            if (side[p1_idx] >= 0) {
                face_verts_idx[face_vert_count++] = vert_map[p1_idx];
            }

            if (side[p1_idx] * side[p2_idx] < 0) {
                float t = dists[p1_idx] / (dists[p1_idx] - dists[p2_idx]);
                Vec3 intersect_pos = vec3_add(old_verts[p1_idx].pos, vec3_muls(vec3_sub(old_verts[p2_idx].pos, old_verts[p1_idx].pos), t));

                Vec4 p1_color = old_verts[p1_idx].color;
                Vec4 p2_color = old_verts[p2_idx].color;
                Vec4 intersect_color;
                intersect_color.x = p1_color.x + (p2_color.x - p1_color.x) * t;
                intersect_color.y = p1_color.y + (p2_color.y - p1_color.y) * t;
                intersect_color.z = p1_color.z + (p2_color.z - p1_color.z) * t;
                intersect_color.w = p1_color.w + (p2_color.w - p1_color.w) * t;

                face_verts_idx[face_vert_count++] = new_vert_count;
                cap_verts[cap_vert_count].pos = intersect_pos;
                cap_verts[cap_vert_count].color = intersect_color;
                cap_vert_count++;
                new_verts[new_vert_count].pos = intersect_pos;
                new_verts[new_vert_count].color = intersect_color;
                new_vert_count++;
            }
        }

        if (face_vert_count >= 3) {
            new_faces[new_face_count] = *face;
            new_faces[new_face_count].vertexIndices = (int*)malloc(face_vert_count * sizeof(int));
            memcpy(new_faces[new_face_count].vertexIndices, face_verts_idx, face_vert_count * sizeof(int));
            new_face_count++;
        }
    }

    if (cap_vert_count >= 3) {
        Vec3 centroid = { 0,0,0 };
        for (int i = 0; i < cap_vert_count; ++i) centroid = vec3_add(centroid, cap_verts[i].pos);
        centroid = vec3_muls(centroid, 1.0f / cap_vert_count);

        g_sort_normal = plane_normal;
        g_sort_centroid = centroid;
        qsort(cap_verts, cap_vert_count, sizeof(BrushVertex), compare_cap_verts);

        BrushFace cap_face;
        cap_face.material = TextureManager_GetMaterial(0);
        cap_face.material2 = NULL;
        cap_face.uv_offset = (Vec2){ 0, 0 };
        cap_face.uv_scale = (Vec2){ 1, 1 };
        cap_face.uv_rotation = 0;
        cap_face.numVertexIndices = cap_vert_count;
        cap_face.vertexIndices = (int*)malloc(cap_vert_count * sizeof(int));

        for (int i = 0; i < cap_vert_count; ++i) {
            int vert_idx = -1;
            for (int k = 0; k < new_vert_count; ++k) {
                if (vec3_length_sq(vec3_sub(new_verts[k].pos, cap_verts[i].pos)) < 1e-6) {
                    vert_idx = k;
                    break;
                }
            }
            cap_face.vertexIndices[i] = vert_idx;
        }

        new_faces[new_face_count++] = cap_face;
    }

    Brush_FreeData(b);
    b->numVertices = new_vert_count;
    b->vertices = (BrushVertex*)malloc(new_vert_count * sizeof(BrushVertex));
    memcpy(b->vertices, new_verts, new_vert_count * sizeof(BrushVertex));

    b->numFaces = new_face_count;
    b->faces = (BrushFace*)malloc(new_face_count * sizeof(BrushFace));
    memcpy(b->faces, new_faces, new_face_count * sizeof(BrushFace));

    free(dists);
    free(side);
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

    float rad = face->uv_rotation * (3.14159f / 180.0f);
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

    float* final_vbo_data = calloc(total_render_verts * 22, sizeof(float));
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

    int vbo_vertex_offset = 0;
    for (int i = 0; i < b->numFaces; ++i) {
        BrushFace* face = &b->faces[i];
        if (face->numVertexIndices < 3) continue;

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
        mikk_context.m_pUserData = (void*)(final_vbo_data + vbo_vertex_offset * 22);

        genTangSpaceDefault(&mikk_context);

        for (int j = 0; j < num_verts_in_face; ++j) {
            int vbo_idx = (vbo_vertex_offset + j) * 22;
            int vertex_index = face_tri_indices[j];
            BrushVertex vert = b->vertices[vertex_index];
            Vec3 norm = temp_normals[vertex_index];
            float uv1[2], uv2[2], uv3[2], uv4[2];
            getTexCoord(NULL, uv1, j / 3, j % 3);
            Vec3 p0 = b->vertices[face_tri_indices[j - (j % 3) + 0]].pos;
            Vec3 p1 = b->vertices[face_tri_indices[j - (j % 3) + 1]].pos;
            Vec3 p2 = b->vertices[face_tri_indices[j - (j % 3) + 2]].pos;
            Vec3 normal_vec = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
            vec3_normalize(&normal_vec);
            float absX = fabsf(normal_vec.x), absY = fabsf(normal_vec.y), absZ = fabsf(normal_vec.z);
            int dominant_axis = (absY > absX && absY > absZ) ? 1 : ((absX > absZ) ? 0 : 2);
            float u, v;
            if (dominant_axis == 0) { u = vert.pos.y; v = vert.pos.z; }
            else if (dominant_axis == 1) { u = vert.pos.x; v = vert.pos.z; }
            else { u = vert.pos.x; v = vert.pos.y; }
            float rad2 = face->uv_rotation2 * (3.14159f / 180.0f); float cos_r2 = cosf(rad2); float sin_r2 = sinf(rad2);
            uv2[0] = ((u * cos_r2 - v * sin_r2) / face->uv_scale2.x) + face->uv_offset2.x; uv2[1] = ((u * sin_r2 + v * cos_r2) / face->uv_scale2.y) + face->uv_offset2.y;
            float rad3 = face->uv_rotation3 * (3.14159f / 180.0f); float cos_r3 = cosf(rad3); float sin_r3 = sinf(rad3);
            uv3[0] = ((u * cos_r3 - v * sin_r3) / face->uv_scale3.x) + face->uv_offset3.x; uv3[1] = ((u * sin_r3 + v * cos_r3) / face->uv_scale3.y) + face->uv_offset3.y;
            float rad4 = face->uv_rotation4 * (3.14159f / 180.0f); float cos_r4 = cosf(rad4); float sin_r4 = sinf(rad4);
            uv4[0] = ((u * cos_r4 - v * sin_r4) / face->uv_scale4.x) + face->uv_offset4.x; uv4[1] = ((u * sin_r4 + v * cos_r4) / face->uv_scale4.y) + face->uv_offset4.y;

            final_vbo_data[vbo_idx + 0] = vert.pos.x;
            final_vbo_data[vbo_idx + 1] = vert.pos.y;
            final_vbo_data[vbo_idx + 2] = vert.pos.z;
            final_vbo_data[vbo_idx + 3] = norm.x;
            final_vbo_data[vbo_idx + 4] = norm.y;
            final_vbo_data[vbo_idx + 5] = norm.z;
            final_vbo_data[vbo_idx + 6] = uv1[0];
            final_vbo_data[vbo_idx + 7] = uv1[1];
            final_vbo_data[vbo_idx + 12] = vert.color.x;
            final_vbo_data[vbo_idx + 13] = vert.color.y;
            final_vbo_data[vbo_idx + 14] = vert.color.z;
            final_vbo_data[vbo_idx + 15] = vert.color.w;
            final_vbo_data[vbo_idx + 16] = uv2[0];
            final_vbo_data[vbo_idx + 17] = uv2[1];
            final_vbo_data[vbo_idx + 18] = uv3[0];
            final_vbo_data[vbo_idx + 19] = uv3[1];
            final_vbo_data[vbo_idx + 20] = uv4[0];
            final_vbo_data[vbo_idx + 21] = uv4[1];
        }

        free(face_tri_indices);
        vbo_vertex_offset += num_verts_in_face;
    }

    if (b->vao == 0) { glGenVertexArrays(1, &b->vao); glGenBuffers(1, &b->vbo); }
    glBindVertexArray(b->vao);
    glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
    glBufferData(GL_ARRAY_BUFFER, total_render_verts * 22 * sizeof(float), final_vbo_data, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 22 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 22 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 22 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 22 * sizeof(float), (void*)(8 * sizeof(float))); glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 22 * sizeof(float), (void*)(12 * sizeof(float))); glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, 22 * sizeof(float), (void*)(16 * sizeof(float))); glEnableVertexAttribArray(5);
    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, 22 * sizeof(float), (void*)(18 * sizeof(float))); glEnableVertexAttribArray(6);
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, 22 * sizeof(float), (void*)(20 * sizeof(float))); glEnableVertexAttribArray(7);
    glBindVertexArray(0);

    free(final_vbo_data);
    free(temp_normals);
}

void Scene_Clear(Scene* scene, Engine* engine) {
    IO_Clear();
    if (scene->objects) { for (int i = 0; i < scene->numObjects; ++i) { if (scene->objects[i].model) Model_Free(scene->objects[i].model); } free(scene->objects); }
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush_FreeData(&scene->brushes[i]);
        if (scene->brushes[i].physicsBody) Physics_RemoveRigidBody(engine->physicsWorld, scene->brushes[i].physicsBody);
    }
    for (int i = 0; i < scene->numActiveLights; ++i) Light_DestroyShadowMap(&scene->lights[i]);
    for (int i = 0; i < scene->numSoundEntities; ++i) {
        SoundSystem_DeleteSource(scene->soundEntities[i].sourceID);
        SoundSystem_DeleteBuffer(scene->soundEntities[i].bufferID);
    }
    for (int i = 0; i < scene->numParticleEmitters; ++i) {
        ParticleEmitter_Free(&scene->particleEmitters[i]);
        ParticleSystem_Free(scene->particleEmitters[i].system);
    }
    if (engine->physicsWorld) { Physics_DestroyWorld(engine->physicsWorld); }

    memset(scene, 0, sizeof(Scene));
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
    scene->sun.enabled = true;
    scene->sun.direction = (Vec3){ -0.5f, -1.0f, -0.5f };
    vec3_normalize(&scene->sun.direction);
    scene->sun.color = (Vec3){ 1.0f, 0.95f, 0.85f };
    scene->sun.intensity = 1.0f;
    engine->physicsWorld = Physics_CreateWorld(Cvar_GetFloat("gravity") * -1.0f);
    engine->camera.physicsBody = Physics_CreatePlayerCapsule(engine->physicsWorld, 0.4f, PLAYER_HEIGHT_NORMAL, 80.0f, scene->playerStart.position);
}

bool Scene_LoadMap(Scene* scene, Renderer* renderer, const char* mapPath, Engine* engine) {
    FILE* file = fopen(mapPath, "r");
    if (!file) { return false; }
    strncpy(scene->mapPath, mapPath, sizeof(scene->mapPath) - 1);
    scene->mapPath[sizeof(scene->mapPath) - 1] = '\0';
    IO_Clear();
    if (scene->objects) { for (int i = 0; i < scene->numObjects; ++i) { if (scene->objects[i].model) Model_Free(scene->objects[i].model); } free(scene->objects); }
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush_FreeData(&scene->brushes[i]);
        if (scene->brushes[i].physicsBody) Physics_RemoveRigidBody(engine->physicsWorld, scene->brushes[i].physicsBody);
    }
    for (int i = 0; i < scene->numActiveLights; ++i) Light_DestroyShadowMap(&scene->lights[i]);
    for (int i = 0; i < scene->numSoundEntities; ++i) {
        SoundSystem_DeleteSource(scene->soundEntities[i].sourceID);
        SoundSystem_DeleteBuffer(scene->soundEntities[i].bufferID);
    }
    for (int i = 0; i < scene->numParticleEmitters; ++i) {
        ParticleEmitter_Free(&scene->particleEmitters[i]);
        ParticleSystem_Free(scene->particleEmitters[i].system);
    }
    if (engine->physicsWorld) { Physics_DestroyWorld(engine->physicsWorld); }

    scene->objects = NULL; scene->numObjects = 0; scene->numBrushes = 0; scene->numActiveLights = 0; scene->numDecals = 0; scene->numSoundEntities = 0;
    scene->numParticleEmitters = 0;
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
    scene->sun.enabled = true;
    scene->sun.direction = (Vec3){ -0.5f, -1.0f, -0.5f };
    vec3_normalize(&scene->sun.direction);
    scene->sun.color = (Vec3){ 1.0f, 0.95f, 0.85f };
    scene->sun.intensity = 1.0f;
    engine->physicsWorld = Physics_CreateWorld(Cvar_GetFloat("gravity") * -1.0f);
    char line[2048];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char keyword[64]; sscanf(line, "%s", keyword);
        if (strcmp(keyword, "player_start") == 0) { sscanf(line, "%*s %f %f %f", &scene->playerStart.position.x, &scene->playerStart.position.y, &scene->playerStart.position.z); }
        else if (strcmp(keyword, "fog_settings") == 0) {
            int enabled_int;
            int items_read = sscanf(line, "%*s %d %f %f %f %f %f %f %f %f",
                &enabled_int,
                &scene->sun.direction.x, &scene->sun.direction.y, &scene->sun.direction.z,
                &scene->sun.color.x, &scene->sun.color.y, &scene->sun.color.z,
                &scene->sun.intensity, &scene->sun.volumetricIntensity);
            scene->fog.enabled = (bool)enabled_int;
            vec3_normalize(&scene->sun.direction);
        }
        else if (strcmp(keyword, "post_settings") == 0) {
            int enabled_int, flare_int, dof_enabled_int, ca_enabled_int, sharpen_enabled_int;
            sscanf(line, "%*s %d %f %f %f %d %f %f %f %d %f %f %d %f", &enabled_int, &scene->post.crtCurvature, &scene->post.vignetteStrength, &scene->post.vignetteRadius, &flare_int, &scene->post.lensFlareStrength, &scene->post.scanlineStrength, &scene->post.grainIntensity, &dof_enabled_int, &scene->post.dofFocusDistance, &scene->post.dofAperture, &ca_enabled_int, &scene->post.chromaticAberrationStrength, &sharpen_enabled_int, &scene->post.sharpenAmount);
            scene->post.enabled = (bool)enabled_int;
            scene->post.lensFlareEnabled = (bool)flare_int;
            scene->post.dofEnabled = (bool)dof_enabled_int;
            scene->post.chromaticAberrationEnabled = (bool)ca_enabled_int;
            scene->post.sharpenEnabled = (bool)sharpen_enabled_int;
        }
        else if (strcmp(keyword, "sun") == 0) {
            int enabled_int;
            sscanf(line, "%*s %d %f %f %f %f %f %f %f",
                &enabled_int,
                &scene->sun.direction.x, &scene->sun.direction.y, &scene->sun.direction.z,
                &scene->sun.color.x, &scene->sun.color.y, &scene->sun.color.z,
                &scene->sun.intensity);
            scene->sun.enabled = (bool)enabled_int;
            vec3_normalize(&scene->sun.direction);
        }
        else if (strcmp(keyword, "brush_begin") == 0) {
            if (scene->numBrushes >= MAX_BRUSHES) continue;
            Brush* b = &scene->brushes[scene->numBrushes];
            memset(b, 0, sizeof(Brush));
            sscanf(line, "%*s %f %f %f %f %f %f %f %f %f", &b->pos.x, &b->pos.y, &b->pos.z, &b->rot.x, &b->rot.y, &b->rot.z, &b->scale.x, &b->scale.y, &b->scale.z);

            while (fgets(line, sizeof(line), file) && strncmp(line, "brush_end", 9) != 0) {
                int dummy_int; char face_keyword[64]; sscanf(line, "%s", face_keyword);
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
                    b->faces = malloc(b->numFaces * sizeof(BrushFace));
                    for (int i = 0; i < b->numFaces; ++i) {
                        fgets(line, sizeof(line), file);
                        char mat_name[64], mat2_name[64], mat3_name[64], mat4_name[64];

                        sscanf(line, " f %*d %s %s %s %s %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d",
                            mat_name, mat2_name, mat3_name, mat4_name,
                            &b->faces[i].uv_offset.x, &b->faces[i].uv_offset.y, &b->faces[i].uv_rotation, &b->faces[i].uv_scale.x, &b->faces[i].uv_scale.y,
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
                else if (sscanf(line, " is_reflection_probe %d", &dummy_int) == 1) { b->isReflectionProbe = (bool)dummy_int; }
                else if (sscanf(line, " name \"%63[^\"]\"", b->name) == 1) {}
                else if (sscanf(line, " targetname \"%63[^\"]\"", b->targetname) == 1) {}
                else if (sscanf(line, " is_trigger %d", &dummy_int) == 1) { b->isTrigger = (bool)dummy_int; }
                else if (sscanf(line, " is_dsp %d", &dummy_int) == 1) { b->isDSP = (bool)dummy_int; }
                else if (sscanf(line, " reverb_preset %d", &dummy_int) == 1) { b->reverbPreset = (ReverbPreset)dummy_int; }
                else if (sscanf(line, " is_water %d", &dummy_int) == 1) { b->isWater = (bool)dummy_int; }
            }
            if (b->isReflectionProbe) {
                const char* faces_suffixes[] = { "px", "nx", "py", "ny", "pz", "nz" };
                char face_paths[6][256];
                for (int i = 0; i < 6; ++i) sprintf(face_paths[i], "cubemaps/%s_%s.png", b->name, faces_suffixes[i]);
                const char* face_pointers[6];
                for (int i = 0; i < 6; ++i) {
                    face_pointers[i] = face_paths[i];
                }
                b->cubemapTexture = loadCubemap(face_pointers);
            }
            Brush_UpdateMatrix(b);
            Brush_CreateRenderData(b);
            if (!b->isReflectionProbe && !b->isTrigger && !b->isWater && !b->isDSP && b->numVertices > 0) {
                Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                for (int i = 0; i < b->numVertices; i++) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                free(world_verts);
            }
            scene->numBrushes++;
        }
        else if (strcmp(keyword, "gltf_model") == 0) {
            char modelPath[270];
            Vec3 pos, rot, scale;
            char targetname[64] = "";
            float mass = 0.0f;
            int is_phys_on = 0;

            char* p_line = line;
            char temp_keyword[64];

            if (sscanf(p_line, "%63s", temp_keyword) != 1) {
                printf("Scene_LoadMap Error: Could not read keyword from: %s\n", line);
                continue;
            }
            p_line += strlen(temp_keyword);
            while (*p_line && isspace((unsigned char)*p_line)) p_line++;

            char* modelPath_start = p_line;
            while (*p_line && !isspace((unsigned char)*p_line)) p_line++;
            size_t modelPath_len = p_line - modelPath_start;
            if (modelPath_len > 0 && modelPath_len < sizeof(modelPath)) {
                strncpy(modelPath, modelPath_start, modelPath_len);
                modelPath[modelPath_len] = '\0';
            }
            else if (modelPath_len == 0) {
                printf("Scene_LoadMap Error: modelPath is empty in line: %s\n", line);
                continue;
            }
            else {
                printf("Scene_LoadMap Error: modelPath too long in line: %s\n", line);
                continue;
            }
            while (*p_line && isspace((unsigned char)*p_line)) p_line++;

            if (*p_line == '"') {
                p_line++;
                char* targetname_start = p_line;
                while (*p_line && *p_line != '"') p_line++;
                size_t targetname_len = p_line - targetname_start;
                if (targetname_len < sizeof(targetname)) {
                    strncpy(targetname, targetname_start, targetname_len);
                    targetname[targetname_len] = '\0';
                }
                else {
                    printf("Scene_LoadMap Warning: targetname too long, truncated from: %.*s\n", (int)targetname_len, targetname_start);
                    strncpy(targetname, targetname_start, sizeof(targetname) - 1);
                    targetname[sizeof(targetname) - 1] = '\0';
                }
                if (*p_line == '"') p_line++;
            }
            else {
                targetname[0] = '\0';
            }

            int items_read_floats = sscanf(p_line, "%f %f %f %f %f %f %f %f %f %f %d",
                &pos.x, &pos.y, &pos.z,
                &rot.x, &rot.y, &rot.z,
                &scale.x, &scale.y, &scale.z,
                &mass, &is_phys_on);

            if (items_read_floats != 11) {
                printf("Scene_LoadMap Error: Malformed gltf_model numerical data in line: %sExpected 11 numbers, got %d. Parsed path: '%s', targetname: '%s'\n", line, items_read_floats, modelPath, targetname);
                continue;
            }

            scene->numObjects++;
            scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
            if (!scene->objects) {
                fclose(file);
                printf("FATAL: realloc failed in Scene_LoadMap for objects\n");
                return false;
            }
            SceneObject* newObj = &scene->objects[scene->numObjects - 1];
            memset(newObj, 0, sizeof(SceneObject));

            strncpy(newObj->targetname, targetname, sizeof(newObj->targetname) - 1);
            newObj->targetname[sizeof(newObj->targetname) - 1] = '\0';
            newObj->mass = mass;
            newObj->isPhysicsEnabled = (bool)is_phys_on;

            strncpy(newObj->modelPath, modelPath, sizeof(newObj->modelPath) - 1);
            newObj->modelPath[sizeof(newObj->modelPath) - 1] = '\0';

            newObj->pos = pos;
            newObj->rot = rot;
            newObj->scale = scale;
            SceneObject_UpdateMatrix(newObj);

            newObj->model = Model_Load(newObj->modelPath);

            if (!newObj->model) {
                printf("Scene_LoadMap: Failed to load model '%s'. Skipping object.\n", newObj->modelPath);
                scene->numObjects--;
                continue;
            }

            if (newObj->mass > 0.0f) {
                newObj->physicsBody = Physics_CreateDynamicConvexHull(engine->physicsWorld, newObj->model->combinedVertexData, newObj->model->totalVertexCount, newObj->mass, newObj->modelMatrix);
                if (!newObj->isPhysicsEnabled) {
                    Physics_ToggleCollision(engine->physicsWorld, newObj->physicsBody, false);
                }
            }
            else {
                Mat4 physics_transform = create_trs_matrix(newObj->pos, newObj->rot, (Vec3) { 1.0f, 1.0f, 1.0f });
                if (newObj->model->combinedVertexData && newObj->model->totalIndexCount > 0) {
                    newObj->physicsBody = Physics_CreateStaticTriangleMesh(engine->physicsWorld, newObj->model->combinedVertexData, newObj->model->totalVertexCount, newObj->model->combinedIndexData, newObj->model->totalIndexCount, physics_transform, scale);
                }
            }
        }
        else if (strcmp(keyword, "light") == 0) {
            if (scene->numActiveLights >= MAX_LIGHTS) continue;
            Light* light = &scene->lights[scene->numActiveLights];
            memset(light, 0, sizeof(Light)); int type_int = 0;
            int items_scanned = sscanf(line, "%*s %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f", &type_int, &light->position.x, &light->position.y, &light->position.z, &light->rot.x, &light->rot.y, &light->rot.z, &light->color.x, &light->color.y, &light->color.z, &light->base_intensity, &light->radius, &light->cutOff, &light->outerCutOff, &light->shadowFarPlane, &light->shadowBias, &light->volumetricIntensity);
            light->type = (LightType)type_int; light->is_on = (light->base_intensity > 0.0f); light->intensity = light->base_intensity;
            Light_InitShadowMap(light);
            scene->numActiveLights++;
        }
        else if (strcmp(keyword, "decal") == 0) {
            if (scene->numDecals < MAX_DECALS) {
                Decal* d = &scene->decals[scene->numDecals]; char mat_name[64];
                sscanf(line, "%*s %s %f %f %f %f %f %f %f %f %f", mat_name, &d->pos.x, &d->pos.y, &d->pos.z, &d->rot.x, &d->rot.y, &d->rot.z, &d->size.x, &d->size.y, &d->size.z);
                d->material = TextureManager_FindMaterial(mat_name); Decal_UpdateMatrix(d); scene->numDecals++;
            }
        }
        else if (strcmp(keyword, "particle_emitter") == 0) {
            if (scene->numParticleEmitters < MAX_PARTICLE_EMITTERS) {
                ParticleEmitter* emitter = &scene->particleEmitters[scene->numParticleEmitters]; memset(emitter, 0, sizeof(ParticleEmitter)); int on_default_int = 1;
                sscanf(line, "%*s \"%127[^\"]\" \"%63[^\"]\" %d %f %f %f", emitter->parFile, emitter->targetname, &on_default_int, &emitter->pos.x, &emitter->pos.y, &emitter->pos.z);
                emitter->on_by_default = (bool)on_default_int;
                ParticleSystem* ps = ParticleSystem_Load(emitter->parFile);
                if (ps) { ParticleEmitter_Init(emitter, ps, emitter->pos); scene->numParticleEmitters++; }
            }
        }
        else if (strcmp(keyword, "sound_entity") == 0) {
            if (scene->numSoundEntities < MAX_SOUNDS) {
                SoundEntity* s = &scene->soundEntities[scene->numSoundEntities]; memset(s, 0, sizeof(SoundEntity));
                int is_looping_int = 0;
                int play_on_start_int = 0;
                int items_scanned = sscanf(line, "%*s \"%63[^\"]\" %s %f %f %f %f %f %f %d %d", s->targetname, s->soundPath, &s->pos.x, &s->pos.y, &s->pos.z, &s->volume, &s->pitch, &s->maxDistance, &is_looping_int, &play_on_start_int);
                if (items_scanned < 8) { s->volume = 1.0f; s->pitch = 1.0f; s->maxDistance = 50.0f; }
                s->is_looping = (bool)is_looping_int;
                s->play_on_start = (bool)play_on_start_int;
                s->bufferID = SoundSystem_LoadWAV(s->soundPath);
                if (s->play_on_start) {
                    s->sourceID = SoundSystem_PlaySound(s->bufferID, s->pos, s->volume, s->pitch, s->maxDistance, s->is_looping);
                }
                scene->numSoundEntities++;
            }
        }
        else if (strcmp(keyword, "io_connection") == 0) {
            if (g_num_io_connections < MAX_IO_CONNECTIONS) {
                IOConnection* conn = &g_io_connections[g_num_io_connections];
                conn->active = true; conn->hasFired = false; int type_int; int fire_once_int;
                sscanf(line, "%*s %d %d \"%63[^\"]\" \"%63[^\"]\" \"%63[^\"]\" %f %d", &type_int, &conn->sourceIndex, conn->outputName, conn->targetName, conn->inputName, &conn->delay, &fire_once_int);
                conn->sourceType = (EntityType)type_int; conn->fireOnce = (bool)fire_once_int;
                g_num_io_connections++;
            }
        }
        else if (strcmp(keyword, "targetname") == 0) {
            if (scene->numActiveLights > 0) { Light* last_light = &scene->lights[scene->numActiveLights - 1]; sscanf(line, "%*s \"%63[^\"]\"", last_light->targetname); }
        }
    }
    fclose(file);
    engine->camera.physicsBody = Physics_CreatePlayerCapsule(engine->physicsWorld, 0.4f, PLAYER_HEIGHT_NORMAL, 80.0f, scene->playerStart.position);
    engine->camera.position = scene->playerStart.position;
    return true;
}

void Scene_SaveMap(Scene* scene, const char* mapPath) {
    FILE* file = fopen(mapPath, "w");
    if (!file) { return; }
    fprintf(file, "player_start %.4f %.4f %.4f\n\n", scene->playerStart.position.x, scene->playerStart.position.y, scene->playerStart.position.z);
    fprintf(file, "fog_settings %d %.4f %.4f %.4f %.4f %.4f\n\n", (int)scene->fog.enabled, scene->fog.color.x, scene->fog.color.y, scene->fog.color.z, scene->fog.start, scene->fog.end);
    fprintf(file, "post_settings %d %.4f %.4f %.4f %d %.4f %.4f %.4f %d %.4f %.4f %d %.4f%d %.4f\n\n",
        (int)scene->post.enabled, scene->post.crtCurvature, scene->post.vignetteStrength, scene->post.vignetteRadius,
        (int)scene->post.lensFlareEnabled, scene->post.lensFlareStrength, scene->post.scanlineStrength, scene->post.grainIntensity,
        (int)scene->post.dofEnabled, scene->post.dofFocusDistance, scene->post.dofAperture,
        (int)scene->post.chromaticAberrationEnabled, scene->post.chromaticAberrationStrength,
        (int)scene->post.sharpenEnabled, scene->post.sharpenAmount
    );
    fprintf(file, "sun %d %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f\n\n",
        (int)scene->sun.enabled,
        scene->sun.direction.x, scene->sun.direction.y, scene->sun.direction.z,
        scene->sun.color.x, scene->sun.color.y, scene->sun.color.z,
        scene->sun.intensity, scene->sun.volumetricIntensity);

    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        fprintf(file, "brush_begin %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", b->pos.x, b->pos.y, b->pos.z, b->rot.x, b->rot.y, b->rot.z, b->scale.x, b->scale.y, b->scale.z);
        if (strlen(b->targetname) > 0) fprintf(file, "  targetname \"%s\"\n", b->targetname);
        if (b->isTrigger) fprintf(file, "  is_trigger 1\n");
        if (b->isDSP) {
            fprintf(file, " is_dsp 1\n");
            fprintf(file, " reverb_preset %d\n", (int)b->reverbPreset);
        }
        if (b->isReflectionProbe) {
            fprintf(file, "  is_reflection_probe 1\n");
            fprintf(file, "  name \"%s\"\n", b->name);
        }
        if (b->isWater) fprintf(file, "  is_water 1\n");
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
            fprintf(file, "\n");
        }
        fprintf(file, "brush_end\n\n");
    }

    for (int i = 0; i < scene->numObjects; ++i) {
        SceneObject* obj = &scene->objects[i];
        fprintf(file, "gltf_model %s \"%s\" %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f %.4f %.4f %d\n",
            obj->modelPath, obj->targetname, obj->pos.x, obj->pos.y, obj->pos.z,
            obj->rot.x, obj->rot.y, obj->rot.z, obj->scale.x, obj->scale.y, obj->scale.z,
            obj->mass, (int)obj->isPhysicsEnabled);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numActiveLights; ++i) {
        Light* light = &scene->lights[i];
        fprintf(file, "light %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", (int)light->type, light->position.x, light->position.y, light->position.z, light->rot.x, light->rot.y, light->rot.z, light->color.x, light->color.y, light->color.z, light->base_intensity, light->radius, light->cutOff, light->outerCutOff, light->shadowFarPlane, light->shadowBias, light->volumetricIntensity);
        if (strlen(light->targetname) > 0) fprintf(file, "  targetname \"%s\"\n", light->targetname);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numDecals; ++i) {
        Decal* d = &scene->decals[i];
        const char* mat_name = d->material ? d->material->name : "___MISSING___";
        fprintf(file, "decal %s %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f %.4f\n", mat_name, d->pos.x, d->pos.y, d->pos.z, d->rot.x, d->rot.y, d->rot.z, d->size.x, d->size.y, d->size.z);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numParticleEmitters; ++i) {
        ParticleEmitter* emitter = &scene->particleEmitters[i];
        fprintf(file, "particle_emitter \"%s\" \"%s\" %d %.4f %.4f %.4f\n", emitter->parFile, emitter->targetname, (int)emitter->on_by_default, emitter->pos.x, emitter->pos.y, emitter->pos.z);
    }
    fprintf(file, "\n");
    for (int i = 0; i < scene->numSoundEntities; ++i) {
        SoundEntity* s = &scene->soundEntities[i];
        fprintf(file, "sound_entity \"%s\" %s %.4f %.4f %.4f %.4f %.4f %.4f %d %d\n", s->targetname, s->soundPath, s->pos.x, s->pos.y, s->pos.z, s->volume, s->pitch, s->maxDistance, (int)s->is_looping, (int)s->play_on_start);
    }
    for (int i = 0; i < g_num_io_connections; ++i) {
        IOConnection* conn = &g_io_connections[i];
        if (conn->active) {
            fprintf(file, "io_connection %d %d \"%s\" \"%s\" \"%s\" %.4f %d\n", (int)conn->sourceType, conn->sourceIndex, conn->outputName, conn->targetName, conn->inputName, conn->delay, (int)conn->fireOnce);
        }
    }
    fclose(file);
}