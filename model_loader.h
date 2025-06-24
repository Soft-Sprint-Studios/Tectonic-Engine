/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include <GL/glew.h>
#include <stdbool.h>
#include "texturemanager.h" 
#include "math_lib.h"

typedef struct {
    GLuint VAO;
    GLuint VBO;
    GLuint EBO;
    int indexCount;
    bool useEBO;
    Material* material;

    float* vertexData;
    unsigned int* indexData;
    unsigned int vertexCount;

    float* final_vbo_data;
    size_t final_vbo_data_size;
} Mesh;

typedef struct {
    Vec3 aabb_min;
    Vec3 aabb_max;
    Mesh* meshes;
    int meshCount;
    float* combinedVertexData;
    float* combinedWorldVertexData;
    unsigned int* combinedIndexData;
    unsigned int totalVertexCount;
    unsigned int totalIndexCount;
} LoadedModel;

LoadedModel* Model_Load(const char* path);
void Model_Free(LoadedModel* model);

#endif // MODEL_LOADER_H