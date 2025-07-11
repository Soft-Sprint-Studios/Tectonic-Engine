/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "cgltf/cgltf.h"
#include "model_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

static LoadedModel* g_ErrorModel = NULL;

static void create_error_model() {
    g_ErrorModel = malloc(sizeof(LoadedModel));
    memset(g_ErrorModel, 0, sizeof(LoadedModel));

    g_ErrorModel->meshCount = 1;
    g_ErrorModel->meshes = calloc(1, sizeof(Mesh));
    Mesh* errorMesh = &g_ErrorModel->meshes[0];

    errorMesh->material = &g_MissingMaterial;

    float size = 0.5f;
    float vertices[] = {
        -size, -size, -size,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
         size, -size, -size,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
         size,  size, -size,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        -size,  size, -size,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,

        -size, -size,  size,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
         size, -size,  size,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
         size,  size,  size,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        -size,  size,  size,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,

        -size,  size,  size, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        -size,  size, -size, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        -size, -size, -size, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        -size, -size,  size, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,

         size,  size,  size,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
         size,  size, -size,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
         size, -size, -size,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
         size, -size,  size,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,

        -size, -size, -size,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
         size, -size, -size,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
         size, -size,  size,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        -size, -size,  size,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,

        -size,  size, -size,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
         size,  size, -size,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
         size,  size,  size,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        -size,  size,  size,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f
    };

    unsigned int indices[] = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    errorMesh->vertexCount = 24;
    errorMesh->indexCount = 36;
    errorMesh->useEBO = true;

    errorMesh->final_vbo_data_size = sizeof(vertices);
    errorMesh->final_vbo_data = malloc(errorMesh->final_vbo_data_size);
    memcpy(errorMesh->final_vbo_data, vertices, errorMesh->final_vbo_data_size);

    errorMesh->indexData = malloc(sizeof(indices));
    memcpy(errorMesh->indexData, indices, sizeof(indices));

    glGenVertexArrays(1, &errorMesh->VAO);
    glGenBuffers(1, &errorMesh->VBO);
    glGenBuffers(1, &errorMesh->EBO);

    glBindVertexArray(errorMesh->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, errorMesh->VBO);
    glBufferData(GL_ARRAY_BUFFER, errorMesh->final_vbo_data_size, errorMesh->final_vbo_data, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, errorMesh->EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), errorMesh->indexData, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    g_ErrorModel->aabb_min = (Vec3){ -size, -size, -size };
    g_ErrorModel->aabb_max = (Vec3){ size, size, size };
}

static void create_combined_collision_mesh(LoadedModel* model) {
    if (!model || model->meshCount == 0) return;
    model->totalVertexCount = 0; model->totalIndexCount = 0;
    for (int i = 0; i < model->meshCount; ++i) {
        model->totalVertexCount += model->meshes[i].vertexCount;
        model->totalIndexCount += model->meshes[i].indexCount;
    }
    if (model->totalVertexCount == 0 || model->totalIndexCount == 0) return;
    model->combinedVertexData = malloc(model->totalVertexCount * 3 * sizeof(float));
    model->combinedIndexData = malloc(model->totalIndexCount * sizeof(unsigned int));
    if (!model->combinedVertexData || !model->combinedIndexData) {
        free(model->combinedVertexData); free(model->combinedIndexData);
        model->combinedVertexData = NULL; model->combinedIndexData = NULL;
        return;
    }
    unsigned int vertexOffset = 0; unsigned int indexOffset = 0;
    for (int i = 0; i < model->meshCount; ++i) {
        Mesh* mesh = &model->meshes[i];
        for (unsigned int v = 0; v < mesh->vertexCount; ++v) {
            memcpy(&model->combinedVertexData[(vertexOffset + v) * 3], &mesh->vertexData[v * 3], 3 * sizeof(float));
        }
        for (unsigned int j = 0; j < mesh->indexCount; ++j) {
            model->combinedIndexData[indexOffset + j] = mesh->indexData[j] + vertexOffset;
        }
        vertexOffset += mesh->vertexCount; indexOffset += mesh->indexCount;
    }
}

LoadedModel* Model_Load(const char* path) {
    if (!g_ErrorModel) {
        create_error_model();
    }
    cgltf_options options = { 0 }; cgltf_data* data = NULL;
    if (cgltf_parse_file(&options, path, &data) != cgltf_result_success) {
        Console_Printf("ModelLoader ERROR: Failed to parse %s. Returning error model.\n", path);
        return g_ErrorModel;
    }
    if (cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
        cgltf_free(data);
        Console_Printf("ModelLoader ERROR: Failed to load buffers for %s. Returning error model.\n", path);
        return g_ErrorModel;
    }
    LoadedModel* loadedModel = malloc(sizeof(LoadedModel));
    if (!loadedModel) { cgltf_free(data); return g_ErrorModel; }
    memset(loadedModel, 0, sizeof(LoadedModel));
    loadedModel->aabb_min = (Vec3){ FLT_MAX, FLT_MAX, FLT_MAX };
    loadedModel->aabb_max = (Vec3){ -FLT_MAX, -FLT_MAX, -FLT_MAX };
    loadedModel->meshCount = 0;
    for (size_t i = 0; i < data->meshes_count; ++i) loadedModel->meshCount += data->meshes[i].primitives_count;
    loadedModel->meshes = calloc(loadedModel->meshCount, sizeof(Mesh));
    if (!loadedModel->meshes) { free(loadedModel); cgltf_free(data); return g_ErrorModel; }
    int currentMeshIndex = 0;
    for (size_t i = 0; i < data->meshes_count; ++i) {
        cgltf_mesh* mesh = &data->meshes[i];
        for (size_t j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* primitive = &mesh->primitives[j];
            Mesh* newMesh = &loadedModel->meshes[currentMeshIndex];
            memset(newMesh, 0, sizeof(Mesh));
            newMesh->material = primitive->material && primitive->material->name ? TextureManager_FindMaterial(primitive->material->name) : &g_MissingMaterial;
            float* positions = NULL, * normals = NULL, * texcoords = NULL, * tangents = NULL;
            cgltf_size vertexCount = 0;
            if (primitive->attributes_count == 0) continue;
            for (size_t k = 0; k < primitive->attributes_count; ++k) {
                cgltf_attribute* attr = &primitive->attributes[k]; vertexCount = attr->data->count;
                if (attr->type == cgltf_attribute_type_position) {
                    positions = malloc(vertexCount * 3 * sizeof(float)); cgltf_accessor_unpack_floats(attr->data, positions, vertexCount * 3);
                    for (cgltf_size v_idx = 0; v_idx < vertexCount; ++v_idx) {
                        loadedModel->aabb_min.x = fminf(loadedModel->aabb_min.x, positions[v_idx * 3 + 0]);
                        loadedModel->aabb_min.y = fminf(loadedModel->aabb_min.y, positions[v_idx * 3 + 1]);
                        loadedModel->aabb_min.z = fminf(loadedModel->aabb_min.z, positions[v_idx * 3 + 2]);
                        loadedModel->aabb_max.x = fmaxf(loadedModel->aabb_max.x, positions[v_idx * 3 + 0]);
                        loadedModel->aabb_max.y = fmaxf(loadedModel->aabb_max.y, positions[v_idx * 3 + 1]);
                        loadedModel->aabb_max.z = fmaxf(loadedModel->aabb_max.z, positions[v_idx * 3 + 2]);
                    }
                }
                else if (attr->type == cgltf_attribute_type_normal) { normals = malloc(vertexCount * 3 * sizeof(float)); cgltf_accessor_unpack_floats(attr->data, normals, vertexCount * 3); }
                else if (attr->type == cgltf_attribute_type_texcoord) { texcoords = malloc(vertexCount * 2 * sizeof(float)); cgltf_accessor_unpack_floats(attr->data, texcoords, vertexCount * 2); }
                else if (attr->type == cgltf_attribute_type_tangent) { tangents = malloc(vertexCount * 4 * sizeof(float)); cgltf_accessor_unpack_floats(attr->data, tangents, vertexCount * 4); }
            }
            newMesh->vertexCount = (unsigned int)vertexCount;
            if (!positions) { free(positions); free(normals); free(texcoords); free(tangents); continue; }
            if (!normals) normals = calloc(vertexCount * 3, sizeof(float));
            if (!texcoords) texcoords = calloc(vertexCount * 2, sizeof(float));
            if (!tangents) tangents = calloc(vertexCount * 4, sizeof(float));
            newMesh->vertexData = positions;
            newMesh->final_vbo_data_size = vertexCount * 12 * sizeof(float);
            newMesh->final_vbo_data = malloc(newMesh->final_vbo_data_size);
            for (cgltf_size v = 0; v < vertexCount; v++) {
                memcpy(&newMesh->final_vbo_data[v * 12 + 0], &positions[v * 3], 3 * sizeof(float));
                memcpy(&newMesh->final_vbo_data[v * 12 + 3], &normals[v * 3], 3 * sizeof(float));
                memcpy(&newMesh->final_vbo_data[v * 12 + 6], &texcoords[v * 2], 2 * sizeof(float));
                memcpy(&newMesh->final_vbo_data[v * 12 + 8], &tangents[v * 4], 4 * sizeof(float));
            }
            free(normals); free(texcoords); free(tangents);
            if (primitive->indices) {
                newMesh->indexCount = (int)primitive->indices->count;
                newMesh->indexData = malloc(newMesh->indexCount * sizeof(unsigned int));
                cgltf_accessor_unpack_indices(primitive->indices, newMesh->indexData, sizeof(unsigned int), newMesh->indexCount);
                newMesh->useEBO = true;
            }
            else {
                newMesh->indexCount = (int)vertexCount; newMesh->useEBO = false;
                newMesh->indexData = malloc(vertexCount * sizeof(unsigned int));
                for (unsigned int v = 0; v < vertexCount; v++) newMesh->indexData[v] = v;
            }
            if (newMesh->indexCount == 0) continue;
            glGenVertexArrays(1, &newMesh->VAO); glGenBuffers(1, &newMesh->VBO);
            if (newMesh->useEBO) glGenBuffers(1, &newMesh->EBO);
            glBindVertexArray(newMesh->VAO);
            glBindBuffer(GL_ARRAY_BUFFER, newMesh->VBO);
            glBufferData(GL_ARRAY_BUFFER, newMesh->final_vbo_data_size, newMesh->final_vbo_data, GL_DYNAMIC_DRAW);
            if (newMesh->useEBO) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newMesh->EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, newMesh->indexCount * sizeof(unsigned int), newMesh->indexData, GL_STATIC_DRAW);
            }
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(8 * sizeof(float))); glEnableVertexAttribArray(3);
            currentMeshIndex++;
        }
    }
    loadedModel->meshCount = currentMeshIndex;
    create_combined_collision_mesh(loadedModel);
    glBindVertexArray(0); cgltf_free(data);
    return loadedModel;
}

void Model_Free(LoadedModel* model) {
    if (!model || model == g_ErrorModel) return;
    for (int i = 0; i < model->meshCount; ++i) {
        glDeleteVertexArrays(1, &model->meshes[i].VAO);
        glDeleteBuffers(1, &model->meshes[i].VBO);
        if (model->meshes[i].useEBO) glDeleteBuffers(1, &model->meshes[i].EBO);
        free(model->meshes[i].vertexData);
        free(model->meshes[i].indexData);
        free(model->meshes[i].final_vbo_data);
    }
    free(model->combinedVertexData);
    free(model->combinedIndexData);
    free(model->meshes);
    free(model);
}

void ModelLoader_Shutdown() {
    if (g_ErrorModel) {
        for (int i = 0; i < g_ErrorModel->meshCount; ++i) {
            glDeleteVertexArrays(1, &g_ErrorModel->meshes[i].VAO);
            glDeleteBuffers(1, &g_ErrorModel->meshes[i].VBO);
            if (g_ErrorModel->meshes[i].useEBO) glDeleteBuffers(1, &g_ErrorModel->meshes[i].EBO);
            free(g_ErrorModel->meshes[i].vertexData);
            free(g_ErrorModel->meshes[i].indexData);
            free(g_ErrorModel->meshes[i].final_vbo_data);
        }
        free(g_ErrorModel->meshes);
        free(g_ErrorModel);
        g_ErrorModel = NULL;
    }
}