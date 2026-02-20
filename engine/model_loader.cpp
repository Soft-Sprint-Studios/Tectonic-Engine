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
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "model_loader.h"
#include "gl_console.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cfloat>

constexpr int MODEL_VERTEX_STRIDE_FLOATS = 24;

static LoadedModel* g_ErrorModel = nullptr;

static void EnsureErrorModelLoaded() {
    if (g_ErrorModel) return;
    g_ErrorModel = Model_Load("models/error.glb");

    if (g_ErrorModel) {
        constexpr Float scale_factor = 0.05f;

        g_ErrorModel->aabb_min = Math::vec3_muls(g_ErrorModel->aabb_min, scale_factor);
        g_ErrorModel->aabb_max = Math::vec3_muls(g_ErrorModel->aabb_max, scale_factor);

        for (Int i = 0; i < g_ErrorModel->meshCount; ++i) {
            Mesh* mesh = &g_ErrorModel->meshes[i];

            for (Uint v = 0; v < mesh->vertexCount; ++v) {
                Int base_idx = v * MODEL_VERTEX_STRIDE_FLOATS;

                mesh->final_vbo_data[base_idx + 0] *= scale_factor;
                mesh->final_vbo_data[base_idx + 1] *= scale_factor;
                mesh->final_vbo_data[base_idx + 2] *= scale_factor;
            }

            glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, mesh->final_vbo_data_size, mesh->final_vbo_data);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
}

static void Model_CombineMeshData(LoadedModel* model) {
    if (!model || model->meshCount == 0) {
        return;
    }

    model->totalVertexCount = 0;
    model->totalIndexCount = 0;
    for (Int i = 0; i < model->meshCount; ++i) {
        model->totalVertexCount += model->meshes[i].vertexCount;
        model->totalIndexCount += model->meshes[i].indexCount;
    }

    if (model->totalVertexCount == 0 || model->totalIndexCount == 0) {
        return;
    }

    model->combinedVertexData = new Float[model->totalVertexCount * 3];
    model->combinedNormalData = new Float[model->totalVertexCount * 3];
    model->combinedIndexData = new Uint[model->totalIndexCount];

    if (!model->combinedVertexData || !model->combinedIndexData || !model->combinedNormalData) {
        delete[] model->combinedVertexData;
        delete[] model->combinedNormalData;
        delete[] model->combinedIndexData;
        model->combinedVertexData = nullptr;
        model->combinedNormalData = nullptr;
        model->combinedIndexData = nullptr;
        return;
    }

    Uint vertexOffset = 0;
    Uint indexOffset = 0;
    for (Int i = 0; i < model->meshCount; ++i) {
        Mesh* mesh = &model->meshes[i];
        for (Uint v = 0; v < mesh->vertexCount; ++v) {
            memcpy(&model->combinedVertexData[(vertexOffset + v) * 3], &mesh->final_vbo_data[v * MODEL_VERTEX_STRIDE_FLOATS + 0], 3 * sizeof(Float));
            memcpy(&model->combinedNormalData[(vertexOffset + v) * 3], &mesh->final_vbo_data[v * MODEL_VERTEX_STRIDE_FLOATS + 3], 3 * sizeof(Float));
        }
        for (Uint j = 0; j < mesh->indexCount; ++j) {
            model->combinedIndexData[indexOffset + j] = mesh->indexData[j] + vertexOffset;
        }
        vertexOffset += mesh->vertexCount;
        indexOffset += mesh->indexCount;
    }
}

LoadedModel* Model_Load(const Char* path) {
    Bool is_loading_error_asset = (path && strstr(path, "error.glb") != nullptr);

    Bool is_glb = false;
    const Char* ext = strrchr(path, '.');
    if (ext && _stricmp(ext, ".glb") == 0) {
        is_glb = true;
    }

    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path, &data) != cgltf_result_success) {
        Console_Printf_Error("Failed to load model: %s", path);
        if (is_loading_error_asset) return nullptr;
        EnsureErrorModelLoaded();
        return g_ErrorModel;
    }

    if (cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
        Console_Printf_Error("Failed to load buffers for model: %s", path);
        cgltf_free(data);
        if (is_loading_error_asset) return nullptr;
        EnsureErrorModelLoaded();
        return g_ErrorModel;
    }

    LoadedModel* loadedModel = new LoadedModel;
    if (!loadedModel) {
        cgltf_free(data);
        if (is_loading_error_asset) return nullptr;
        EnsureErrorModelLoaded();
        return g_ErrorModel;
    }
    memset(loadedModel, 0, sizeof(LoadedModel));

    loadedModel->nodes = (void*)data->nodes;
    loadedModel->num_nodes = data->nodes_count;

    if (data->skins_count > 0) {
        loadedModel->num_skins = data->skins_count;
        loadedModel->skins = new Skin[loadedModel->num_skins]{};
        for (Usize s = 0; s < data->skins_count; ++s) {
            cgltf_skin* skin_data = &data->skins[s];
            Skin* skin = &loadedModel->skins[s];
            strncpy(skin->name, skin_data->name ? skin_data->name : "", sizeof(skin->name) - 1);
            skin->num_joints = skin_data->joints_count;
            skin->joints = new SkinJoint[skin->num_joints]{};

            for (Usize j = 0; j < skin_data->joints_count; ++j) {
                skin->joints[j].joint_index = skin_data->joints[j] - data->nodes;
                cgltf_accessor_read_float(skin_data->inverse_bind_matrices, j, skin->joints[j].inverse_bind_matrix.m, 16);
            }
        }
    }

    if (data->animations_count > 0) {
        loadedModel->num_animations = data->animations_count;
        loadedModel->animations = new AnimationClip[loadedModel->num_animations]{};
        for (Usize a = 0; a < data->animations_count; ++a) {
            cgltf_animation* anim_data = &data->animations[a];
            AnimationClip* clip = &loadedModel->animations[a];
            strncpy(clip->name, anim_data->name ? anim_data->name : "", sizeof(clip->name) - 1);
            clip->num_channels = anim_data->channels_count;
            clip->channels = new AnimationChannel[clip->num_channels]{};
            clip->duration = 0.0f;

            for (Usize c = 0; c < anim_data->channels_count; ++c) {
                cgltf_animation_channel* chan_data = &anim_data->channels[c];
                AnimationChannel* channel = &clip->channels[c];
                channel->target_joint = chan_data->target_node - data->nodes;

                cgltf_animation_sampler* sampler_data = chan_data->sampler;
                channel->sampler.num_keyframes = sampler_data->input->count;

                channel->sampler.timestamps = new Float[channel->sampler.num_keyframes];
                cgltf_accessor_unpack_floats(sampler_data->input, channel->sampler.timestamps, channel->sampler.num_keyframes);

                if (clip->duration < channel->sampler.timestamps[channel->sampler.num_keyframes - 1]) {
                    clip->duration = channel->sampler.timestamps[channel->sampler.num_keyframes - 1];
                }

                if (chan_data->target_path == cgltf_animation_path_type_translation) {
                    channel->sampler.translations = new Vec3[channel->sampler.num_keyframes];
                    cgltf_accessor_unpack_floats(sampler_data->output, (Float*)channel->sampler.translations, channel->sampler.num_keyframes * 3);
                }
                else if (chan_data->target_path == cgltf_animation_path_type_rotation) {
                    channel->sampler.rotations = new Vec4[channel->sampler.num_keyframes];
                    cgltf_accessor_unpack_floats(sampler_data->output, (Float*)channel->sampler.rotations, channel->sampler.num_keyframes * 4);
                }
                else if (chan_data->target_path == cgltf_animation_path_type_scale) {
                    channel->sampler.scales = new Vec3[channel->sampler.num_keyframes];
                    cgltf_accessor_unpack_floats(sampler_data->output, (Float*)channel->sampler.scales, channel->sampler.num_keyframes * 3);
                }
            }
        }
    }

    loadedModel->aabb_min = Vec3{ FLT_MAX, FLT_MAX, FLT_MAX };
    loadedModel->aabb_max = Vec3{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
    loadedModel->meshCount = 0;
    for (Usize i = 0; i < data->meshes_count; ++i) {
        loadedModel->meshCount += data->meshes[i].primitives_count;
    }

    loadedModel->meshes = new Mesh[loadedModel->meshCount]{};
    if (!loadedModel->meshes) {
        delete loadedModel;
        cgltf_free(data);
        return g_ErrorModel;
    }

    Int currentMeshIndex = 0;
    for (Usize i = 0; i < data->meshes_count; ++i) {
        cgltf_mesh* mesh = &data->meshes[i];
        for (Usize j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* primitive = &mesh->primitives[j];
            Mesh* newMesh = &loadedModel->meshes[currentMeshIndex];
            memset(newMesh, 0, sizeof(Mesh));

            if (is_glb) {
                if (primitive->material) {
                    newMesh->material = new Material{};
                    newMesh->material->roughness = -1.0f;
                    newMesh->material->metalness = -1.0f;
                    if (primitive->material->name) {
                        strncpy(newMesh->material->name, primitive->material->name, sizeof(newMesh->material->name) - 1);
                    }

                    newMesh->material->normalMap = defaultNormalMapID;
                    newMesh->material->rmaMap = defaultRmaMapID;
                    newMesh->material->isLoaded = true;

                    TextureLoadContext context = g_is_thumbnail_mode ? TEXTURE_LOAD_CONTEXT_UI_THUMBNAIL : TEXTURE_LOAD_CONTEXT_WORLD;

                    if (primitive->material->has_pbr_metallic_roughness) {
                        cgltf_texture_view* base_color_tex = &primitive->material->pbr_metallic_roughness.base_color_texture;
                        if (base_color_tex->texture && base_color_tex->texture->image && base_color_tex->texture->image->buffer_view) {
                            cgltf_buffer_view* bv = base_color_tex->texture->image->buffer_view;
                            newMesh->material->diffuseMap = TextureManager_LoadFromMemory((Char*)bv->buffer->data + bv->offset, bv->size, true, context);
                        }
                        else {
                            newMesh->material->diffuseMap = missingTextureID;
                        }

                        cgltf_texture_view* metallic_roughness_tex = &primitive->material->pbr_metallic_roughness.metallic_roughness_texture;
                        if (metallic_roughness_tex->texture && metallic_roughness_tex->texture->image && metallic_roughness_tex->texture->image->buffer_view) {
                            cgltf_buffer_view* bv = metallic_roughness_tex->texture->image->buffer_view;
                            newMesh->material->rmaMap = TextureManager_LoadFromMemory((Char*)bv->buffer->data + bv->offset, bv->size, false, context);
                        }
                    }

                    cgltf_texture_view* normal_tex = &primitive->material->normal_texture;
                    if (normal_tex->texture && normal_tex->texture->image && normal_tex->texture->image->buffer_view) {
                        cgltf_buffer_view* bv = normal_tex->texture->image->buffer_view;
                        newMesh->material->normalMap = TextureManager_LoadFromMemory((Char*)bv->buffer->data + bv->offset, bv->size, false, context);
                    }
                }
                else {
                    newMesh->material = &g_MissingMaterial;
                }
            }
            else {
                newMesh->material = (primitive->material && primitive->material->name) ? TextureManager_FindMaterial(primitive->material->name) : &g_MissingMaterial;
            }

            Float* positions = nullptr, * normals = nullptr, * texcoords = nullptr, * tangents = nullptr;
            cgltf_accessor* joints_accessor = nullptr;
            cgltf_accessor* weights_accessor = nullptr;
            cgltf_size vertexCount = 0;

            if (primitive->attributes_count == 0) {
                continue;
            }

            for (Usize k = 0; k < primitive->attributes_count; ++k) {
                cgltf_attribute* attr = &primitive->attributes[k];
                vertexCount = attr->data->count;
                if (attr->type == cgltf_attribute_type_position) {
                    positions = new Float[vertexCount * 3];
                    cgltf_accessor_unpack_floats(attr->data, positions, vertexCount * 3);
                    for (cgltf_size v_idx = 0; v_idx < vertexCount; ++v_idx) {
                        loadedModel->aabb_min.x = fminf(loadedModel->aabb_min.x, positions[v_idx * 3 + 0]);
                        loadedModel->aabb_min.y = fminf(loadedModel->aabb_min.y, positions[v_idx * 3 + 1]);
                        loadedModel->aabb_min.z = fminf(loadedModel->aabb_min.z, positions[v_idx * 3 + 2]);
                        loadedModel->aabb_max.x = fmaxf(loadedModel->aabb_max.x, positions[v_idx * 3 + 0]);
                        loadedModel->aabb_max.y = fmaxf(loadedModel->aabb_max.y, positions[v_idx * 3 + 1]);
                        loadedModel->aabb_max.z = fmaxf(loadedModel->aabb_max.z, positions[v_idx * 3 + 2]);
                    }
                }
                else if (attr->type == cgltf_attribute_type_normal) {
                    normals = new Float[vertexCount * 3];
                    cgltf_accessor_unpack_floats(attr->data, normals, vertexCount * 3);
                }
                else if (attr->type == cgltf_attribute_type_texcoord) {
                    texcoords = new Float[vertexCount * 2];
                    cgltf_accessor_unpack_floats(attr->data, texcoords, vertexCount * 2);
                }
                else if (attr->type == cgltf_attribute_type_tangent) {
                    tangents = new Float[vertexCount * 4];
                    cgltf_accessor_unpack_floats(attr->data, tangents, vertexCount * 4);
                }
                else if (attr->type == cgltf_attribute_type_joints) {
                    joints_accessor = attr->data;
                }
                else if (attr->type == cgltf_attribute_type_weights) {
                    weights_accessor = attr->data;
                }
            }

            newMesh->vertexCount = (Uint)vertexCount;
            if (!positions) {
                delete[] positions;
                delete[] normals;
                delete[] texcoords;
                delete[] tangents;
                continue;
            }

            if (!normals) normals = new Float[vertexCount * 3]{};
            if (!texcoords) texcoords = new Float[vertexCount * 2]{};
            if (!tangents) tangents = new Float[vertexCount * 4]{};

            SkinningVertexData* skinning_data = nullptr;
            if (joints_accessor && weights_accessor) {
                skinning_data = new SkinningVertexData[vertexCount]{};
                for (cgltf_size v = 0; v < vertexCount; v++) {
                    cgltf_uint joint_indices_u16[4] = { 0 };
                    cgltf_accessor_read_uint(joints_accessor, v, joint_indices_u16, 4);
                    skinning_data[v].bone_indices[0] = (Int)joint_indices_u16[0];
                    skinning_data[v].bone_indices[1] = (Int)joint_indices_u16[1];
                    skinning_data[v].bone_indices[2] = (Int)joint_indices_u16[2];
                    skinning_data[v].bone_indices[3] = (Int)joint_indices_u16[3];
                    cgltf_accessor_read_float(weights_accessor, v, skinning_data[v].bone_weights, 4);
                }
            }

            newMesh->final_vbo_data_size = vertexCount * MODEL_VERTEX_STRIDE_FLOATS * sizeof(Float);
            newMesh->final_vbo_data = new Float[vertexCount * MODEL_VERTEX_STRIDE_FLOATS];

            for (cgltf_size v = 0; v < vertexCount; v++) {
                Int base_idx = v * MODEL_VERTEX_STRIDE_FLOATS;
                memcpy(&newMesh->final_vbo_data[base_idx + 0], &positions[v * 3], 3 * sizeof(Float));
                memcpy(&newMesh->final_vbo_data[base_idx + 3], &normals[v * 3], 3 * sizeof(Float));
                memcpy(&newMesh->final_vbo_data[base_idx + 6], &texcoords[v * 2], 2 * sizeof(Float));
                memcpy(&newMesh->final_vbo_data[base_idx + 8], &tangents[v * 4], 4 * sizeof(Float));

                newMesh->final_vbo_data[base_idx + 12] = 1.0f;
                newMesh->final_vbo_data[base_idx + 13] = 1.0f;
                newMesh->final_vbo_data[base_idx + 14] = 1.0f;
                newMesh->final_vbo_data[base_idx + 15] = 0.0f;

                newMesh->final_vbo_data[base_idx + 16] = 0.0f;
                newMesh->final_vbo_data[base_idx + 17] = 0.0f;
                newMesh->final_vbo_data[base_idx + 18] = 0.0f;
                newMesh->final_vbo_data[base_idx + 19] = 0.0f;

                memset(&newMesh->final_vbo_data[base_idx + 20], 0, 4 * sizeof(Float));
            }
            delete[] positions;
            delete[] normals;
            delete[] texcoords;
            delete[] tangents;

            if (primitive->indices) {
                newMesh->indexCount = (Int)primitive->indices->count;
                newMesh->indexData = new Uint[newMesh->indexCount];
                cgltf_accessor_unpack_indices(primitive->indices, newMesh->indexData, sizeof(Uint), newMesh->indexCount);
                newMesh->useEBO = true;
            }
            else {
                newMesh->indexCount = (Int)vertexCount;
                newMesh->useEBO = false;
                newMesh->indexData = new Uint[vertexCount];
                for (Uint v = 0; v < vertexCount; v++) {
                    newMesh->indexData[v] = v;
                }
            }

            if (newMesh->indexCount == 0) {
                continue;
            }

            glGenVertexArrays(1, &newMesh->VAO);
            glGenBuffers(1, &newMesh->VBO);
            if (skinning_data) {
                glGenBuffers(1, &newMesh->skinningVBO);
            }
            if (newMesh->useEBO) {
                glGenBuffers(1, &newMesh->EBO);
            }

            glBindVertexArray(newMesh->VAO);
            glBindBuffer(GL_ARRAY_BUFFER, newMesh->VBO);
            glBufferData(GL_ARRAY_BUFFER, newMesh->final_vbo_data_size, newMesh->final_vbo_data, GL_DYNAMIC_DRAW);
            if (skinning_data) {
                glBindBuffer(GL_ARRAY_BUFFER, newMesh->skinningVBO);
                glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(SkinningVertexData), skinning_data, GL_STATIC_DRAW);
            }

            if (newMesh->useEBO) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newMesh->EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, newMesh->indexCount * sizeof(Uint), newMesh->indexData, GL_STATIC_DRAW);
            }

            Usize offset = 0;
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, MODEL_VERTEX_STRIDE_FLOATS * sizeof(Float), (void*)offset);
            glEnableVertexAttribArray(0);
            offset += 3 * sizeof(Float);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, MODEL_VERTEX_STRIDE_FLOATS * sizeof(Float), (void*)offset);
            glEnableVertexAttribArray(1);
            offset += 3 * sizeof(Float);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, MODEL_VERTEX_STRIDE_FLOATS * sizeof(Float), (void*)offset);
            glEnableVertexAttribArray(2);
            offset += 2 * sizeof(Float);
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, MODEL_VERTEX_STRIDE_FLOATS * sizeof(Float), (void*)offset);
            glEnableVertexAttribArray(3);
            offset += 4 * sizeof(Float);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, MODEL_VERTEX_STRIDE_FLOATS * sizeof(Float), (void*)offset);
            glEnableVertexAttribArray(4);
            offset += 4 * sizeof(Float);
            glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, MODEL_VERTEX_STRIDE_FLOATS * sizeof(Float), (void*)offset);
            glEnableVertexAttribArray(9);
            if (skinning_data) {
                glBindBuffer(GL_ARRAY_BUFFER, newMesh->skinningVBO);
                glEnableVertexAttribArray(10);
                glVertexAttribIPointer(10, 4, GL_INT, sizeof(SkinningVertexData), (void*)offsetof(SkinningVertexData, bone_indices));
                glEnableVertexAttribArray(11);
                glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, sizeof(SkinningVertexData), (void*)offsetof(SkinningVertexData, bone_weights));
                delete[] skinning_data;
            }
            glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, MODEL_VERTEX_STRIDE_FLOATS * sizeof(Float), (void*)(22 * sizeof(Float)));
            glEnableVertexAttribArray(8);

            currentMeshIndex++;
        }
    }
    loadedModel->meshCount = currentMeshIndex;
    Model_CombineMeshData(loadedModel);
    glBindVertexArray(0);
    cgltf_free(data);
    return loadedModel;
}

void Model_Free(LoadedModel* model) {
    if (!model) return;
    if (model == g_ErrorModel && g_ErrorModel != nullptr) return;

    if (model->animations) {
        for (Int i = 0; i < model->num_animations; ++i) {
            for (Int j = 0; j < model->animations[i].num_channels; ++j) {
                if (model->animations[i].channels[j].sampler.timestamps) delete[] model->animations[i].channels[j].sampler.timestamps;
                if (model->animations[i].channels[j].sampler.translations) delete[] model->animations[i].channels[j].sampler.translations;
                if (model->animations[i].channels[j].sampler.rotations) delete[] model->animations[i].channels[j].sampler.rotations;
                if (model->animations[i].channels[j].sampler.scales) delete[] model->animations[i].channels[j].sampler.scales;
            }
            delete[] model->animations[i].channels;
        }
        delete[] model->animations;
    }
    if (model->skins) {
        for (Int i = 0; i < model->num_skins; ++i) {
            delete[] model->skins[i].joints;
        }
        delete[] model->skins;
    }
    for (Int i = 0; i < model->meshCount; ++i) {
        glDeleteVertexArrays(1, &model->meshes[i].VAO);
        glDeleteBuffers(1, &model->meshes[i].VBO);
        if (model->meshes[i].skinningVBO) {
            glDeleteBuffers(1, &model->meshes[i].skinningVBO);
        }
        if (model->meshes[i].material && model->meshes[i].material != &g_MissingMaterial && model->meshes[i].material != &g_NodrawMaterial) {
            Bool is_from_manager = false;
            for (Int m = 0; m < TextureManager_GetMaterialCount(); ++m) {
                if (TextureManager_GetMaterial(m) == model->meshes[i].material) {
                    is_from_manager = true;
                    break;
                }
            }
            if (!is_from_manager) {
                delete model->meshes[i].material;
            }
        }
        if (model->meshes[i].useEBO) {
            glDeleteBuffers(1, &model->meshes[i].EBO);
        }
        delete[] model->meshes[i].indexData;
        delete[] model->meshes[i].final_vbo_data;
    }
    if (model->combinedVertexData) delete[] model->combinedVertexData;
    if (model->combinedNormalData) delete[] model->combinedNormalData;
    if (model->combinedIndexData) delete[] model->combinedIndexData;
    delete[] model->meshes;
    delete model;
}

Bool Model_ApplyLMUV(LoadedModel* model, const Char* lmuv_path) {
    FILE* f = fopen(lmuv_path, "rb");
    if (!f) return false;

    Char magic[4];
    if (fread(magic, 1, 4, f) != 4 || strncmp(magic, "LMUV", 4) != 0) {
        fclose(f); return false;
    }

    uint32_t num_meshes = 0;
    fread(&num_meshes, sizeof(uint32_t), 1, f);

    if (num_meshes != model->meshCount) { fclose(f); return false; }

    for (uint32_t i = 0; i < num_meshes; ++i) {
        Mesh* mesh = &model->meshes[i];

        uint32_t num_new_verts = 0;
        uint32_t num_new_indices = 0;
        fread(&num_new_verts, sizeof(uint32_t), 1, f);
        fread(&num_new_indices, sizeof(uint32_t), 1, f);

        uint32_t* new_indices = new uint32_t[num_new_indices];
        fread(new_indices, sizeof(uint32_t), num_new_indices, f);

        constexpr Usize stride_floats = 24;
        Usize stride_bytes = stride_floats * sizeof(Float);
        Float* new_vbo_data = new Float[num_new_verts * stride_floats];

        for (uint32_t v = 0; v < num_new_verts; ++v) {
            uint32_t original_index = 0;
            Float lu = 0.0f, lv = 0.0f;
            fread(&original_index, sizeof(uint32_t), 1, f);
            fread(&lu, sizeof(Float), 1, f);
            fread(&lv, sizeof(Float), 1, f);

            Float* dst = &new_vbo_data[v * stride_floats];
            if (original_index < mesh->vertexCount) {
                Float* src = &mesh->final_vbo_data[original_index * stride_floats];
                memcpy(dst, src, stride_bytes);
            }
            else {
                memset(dst, 0, stride_bytes);
            }

            dst[22] = lu;
            dst[23] = lv;
        }

        delete[] mesh->final_vbo_data;
        mesh->final_vbo_data = new_vbo_data;
        mesh->final_vbo_data_size = num_new_verts * stride_bytes;
        mesh->vertexCount = num_new_verts;

        if (mesh->indexData) delete[] mesh->indexData;
        mesh->indexData = new_indices;
        mesh->indexCount = num_new_indices;
        mesh->useEBO = true;

        glBindVertexArray(mesh->VAO);

        glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
        glBufferData(GL_ARRAY_BUFFER, mesh->final_vbo_data_size, mesh->final_vbo_data, GL_STATIC_DRAW);

        if (mesh->EBO == 0) glGenBuffers(1, &mesh->EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCount * sizeof(Uint), mesh->indexData, GL_STATIC_DRAW);

        glBindVertexArray(0);
    }

    fclose(f);
    return true;
}

void ModelLoader_Shutdown() {
    if (g_ErrorModel) {
        LoadedModel* temp = g_ErrorModel;
        g_ErrorModel = nullptr;
        Model_Free(temp);
    }
}