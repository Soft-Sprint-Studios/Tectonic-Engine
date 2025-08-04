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
#pragma once
#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

//----------------------------------------//
// Brief: GLTF Model loader
//----------------------------------------//

#include <GL/glew.h>
#include <stdbool.h>
#include "texturemanager.h" 
#include "math_lib.h"
#include "models_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BONES_PER_VERTEX 4
#define MAX_BONES_PER_MODEL 128

    typedef struct {
        float* timestamps;
        Vec3* translations;
        Vec4* rotations;
        Vec3* scales;
        size_t num_keyframes;
    } AnimationSampler;

    typedef struct {
        int target_joint;
        AnimationSampler sampler;
    } AnimationChannel;

    typedef struct {
        char name[64];
        float duration;
        AnimationChannel* channels;
        int num_channels;
    } AnimationClip;

    typedef struct {
        int joint_index;
        Mat4 inverse_bind_matrix;
    } SkinJoint;

    typedef struct {
        char name[64];
        SkinJoint* joints;
        int num_joints;
    } Skin;

    typedef struct {
        int bone_indices[MAX_BONES_PER_VERTEX];
        float bone_weights[MAX_BONES_PER_VERTEX];
    } SkinningVertexData;

    typedef struct {
        GLuint VAO;
        GLuint VBO;
        GLuint skinningVBO;
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
        float* combinedNormalData;
        float* combinedWorldVertexData;
        unsigned int* combinedIndexData;
        unsigned int totalVertexCount;
        unsigned int totalIndexCount;
        AnimationClip* animations;
        int num_animations;
        Skin* skins;
        int num_skins;
        void* nodes;
        size_t num_nodes;
    } LoadedModel;

    MODELS_API LoadedModel* Model_Load(const char* path);
    MODELS_API void Model_Free(LoadedModel* model);
    MODELS_API void ModelLoader_Shutdown();

#ifdef __cplusplus
}
#endif

#endif