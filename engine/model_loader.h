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
#pragma once
#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

//----------------------------------------//
// Brief: GLTF Model loader
//----------------------------------------//

#include <GL/glew.h>
#include "texturemanager.h" 
#include "math_lib.h"


constexpr int MAX_BONES_PER_VERTEX = 4;
constexpr int MAX_BONES_PER_MODEL = 128;

    typedef struct {
        Float* timestamps;
        Vec3* translations;
        Vec4* rotations;
        Vec3* scales;
        Usize num_keyframes;
    } AnimationSampler;

    typedef struct {
        Int target_joint;
        AnimationSampler sampler;
    } AnimationChannel;

    typedef struct {
        Char name[64];
        Float duration;
        AnimationChannel* channels;
        Int num_channels;
    } AnimationClip;

    typedef struct {
        Int joint_index;
        Mat4 inverse_bind_matrix;
    } SkinJoint;

    typedef struct {
        Char name[64];
        SkinJoint* joints;
        Int num_joints;
    } Skin;

    typedef struct {
        Int bone_indices[MAX_BONES_PER_VERTEX];
        Float bone_weights[MAX_BONES_PER_VERTEX];
    } SkinningVertexData;

    typedef struct {
        GLuint VAO;
        GLuint VBO;
        GLuint skinningVBO;
        GLuint EBO;
        Int indexCount;
        Bool useEBO;
        Material* material;
        Float* vertexData;
        Uint* indexData;
        Uint vertexCount;
        Float* final_vbo_data;
        Usize final_vbo_data_size;
    } Mesh;

    typedef struct {
        Vec3 aabb_min;
        Vec3 aabb_max;
        Mesh* meshes;
        Int meshCount;
        Float* combinedVertexData;
        Float* combinedNormalData;
        Float* combinedWorldVertexData;
        Uint* combinedIndexData;
        Uint totalVertexCount;
        Uint totalIndexCount;
        AnimationClip* animations;
        Int num_animations;
        Skin* skins;
        Int num_skins;
        void* nodes;
        Usize num_nodes;
    } LoadedModel;

    LoadedModel* Model_Load(const Char* path);
    Bool Model_ApplyLMUV(LoadedModel* model, const Char* lmuv_path);
    void Model_Free(LoadedModel* model);
    void ModelLoader_Shutdown();


#endif // MODEL_LOADER_H