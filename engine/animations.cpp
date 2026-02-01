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
#include "animations.h"
#include "math_lib.h"
#include "model_loader.h"
#include "cgltf.h"
#include <stdlib.h>
#include <math.h>

void evaluate_animation(SceneObject* obj, float time) {
    if (!obj->model || obj->model->num_animations == 0 || obj->current_animation < 0) return;

    AnimationClip* clip = &obj->model->animations[obj->current_animation];
    cgltf_node* nodes = (cgltf_node*)obj->model->nodes;
    cgltf_size num_nodes = obj->model->num_nodes;
    Skin* skin = (obj->model->num_skins > 0) ? &obj->model->skins[0] : NULL;

    if (!skin) return;

    if (!obj->bone_matrices) {
        obj->bone_matrices = new Mat4[skin->num_joints]();
        if (!obj->bone_matrices) return;
    }

    Mat4* local_transforms = new Mat4[num_nodes]();
    if (!local_transforms) return;

    for (size_t i = 0; i < num_nodes; ++i) {
        cgltf_node* node = &nodes[i];
        Vec3 t = { node->translation[0], node->translation[1], node->translation[2] };
        Vec4 r = { node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3] };
        Vec3 s = { node->scale[0], node->scale[1], node->scale[2] };
        mat4_compose(&local_transforms[i], t, r, s);
    }

    for (int i = 0; i < clip->num_channels; ++i) {
        AnimationChannel* channel = &clip->channels[i];
        AnimationSampler* sampler = &channel->sampler;
        int joint_index = channel->target_joint;

        if (joint_index < 0 || joint_index >= num_nodes) continue;

        size_t frame_idx = 0;
        for (size_t k = 0; k < sampler->num_keyframes - 1; ++k) {
            if (time >= sampler->timestamps[k] && time <= sampler->timestamps[k + 1]) {
                frame_idx = k;
                break;
            }
        }
        if (frame_idx >= sampler->num_keyframes - 1) {
            frame_idx = sampler->num_keyframes > 1 ? sampler->num_keyframes - 2 : 0;
        }

        float t0 = sampler->timestamps[frame_idx];
        float t1 = sampler->timestamps[frame_idx + 1];
        float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;

        cgltf_node* node = &nodes[joint_index];
        Vec3 final_t = { node->translation[0], node->translation[1], node->translation[2] };
        Vec4 final_r = { node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3] };
        Vec3 final_s = { node->scale[0], node->scale[1], node->scale[2] };

        if (sampler->translations) final_t = vec3_lerp(sampler->translations[frame_idx], sampler->translations[frame_idx + 1], factor);
        if (sampler->rotations) final_r = quat_slerp(sampler->rotations[frame_idx], sampler->rotations[frame_idx + 1], factor);
        if (sampler->scales) final_s = vec3_lerp(sampler->scales[frame_idx], sampler->scales[frame_idx + 1], factor);

        mat4_compose(&local_transforms[joint_index], final_t, final_r, final_s);
    }

    Mat4* global_transforms = new Mat4[num_nodes]();
    if (!global_transforms) {
        delete[] local_transforms;
        return;
    }

    for (size_t i = 0; i < num_nodes; ++i) {
        cgltf_node* node = &nodes[i];
        if (node->parent) {
            int parent_idx = node->parent - nodes;
            mat4_multiply(&global_transforms[i], &global_transforms[parent_idx], &local_transforms[i]);
        }
        else {
            global_transforms[i] = local_transforms[i];
        }
    }

    for (int i = 0; i < skin->num_joints; ++i) {
        int joint_node_idx = skin->joints[i].joint_index;
        if (joint_node_idx >= 0 && joint_node_idx < num_nodes) {
            Mat4 inv_bind = skin->joints[i].inverse_bind_matrix;
            mat4_multiply(&obj->bone_matrices[i], &global_transforms[joint_node_idx], &inv_bind);
        }
    }

    delete[] local_transforms;
    delete[] global_transforms;
}

void Scene_UpdateAnimations(Scene* scene, float deltaTime) {
    for (int i = 0; i < scene->numObjects; ++i) {
        SceneObject* obj = &scene->objects[i];

        if (!obj->model || obj->model->num_animations == 0) {
            mat4_identity(&obj->animated_local_transform);
            continue;
        }

        if (obj->current_animation == -1) {
            obj->animation_playing = false;
            obj->animation_looping = true;
            obj->animation_time = 0.0f;
            mat4_identity(&obj->animated_local_transform);
            obj->current_animation = 0;
        }

        mat4_identity(&obj->animated_local_transform);

        if (obj->animation_playing) {
            AnimationClip* clip = &obj->model->animations[obj->current_animation];

            if (clip->duration <= 0.0f) {
                continue;
            }

            obj->animation_time += deltaTime;
            if (obj->animation_time > clip->duration) {
                if (obj->animation_looping) {
                    obj->animation_time = fmod(obj->animation_time, clip->duration);
                }
                else {
                    obj->animation_time = clip->duration;
                    obj->animation_playing = false;
                }
            }

            if (obj->model->num_skins > 0) {
                evaluate_animation(obj, obj->animation_time);
            }
            else {
                cgltf_node* target_node = &((cgltf_node*)obj->model->nodes)[0];

                Vec3 anim_t = { target_node->translation[0], target_node->translation[1], target_node->translation[2] };
                Vec4 anim_r = { target_node->rotation[0], target_node->rotation[1], target_node->rotation[2], target_node->rotation[3] };
                Vec3 anim_s = { target_node->scale[0], target_node->scale[1], target_node->scale[2] };

                for (int c = 0; c < clip->num_channels; ++c) {
                    AnimationChannel* channel = &clip->channels[c];
                    AnimationSampler* sampler = &channel->sampler;

                    size_t frame_idx = 0;
                    for (size_t k = 0; k < sampler->num_keyframes - 1; ++k) {
                        if (obj->animation_time >= sampler->timestamps[k] && obj->animation_time <= sampler->timestamps[k + 1]) {
                            frame_idx = k;
                            break;
                        }
                    }
                    if (frame_idx >= sampler->num_keyframes - 1) frame_idx = sampler->num_keyframes > 1 ? sampler->num_keyframes - 2 : 0;

                    float t0 = sampler->timestamps[frame_idx];
                    float t1 = sampler->timestamps[frame_idx + 1];
                    float factor = (t1 > t0) ? (obj->animation_time - t0) / (t1 - t0) : 0.0f;

                    if (sampler->translations) anim_t = vec3_lerp(sampler->translations[frame_idx], sampler->translations[frame_idx + 1], factor);
                    if (sampler->rotations) anim_r = quat_slerp(sampler->rotations[frame_idx], sampler->rotations[frame_idx + 1], factor);
                    if (sampler->scales) anim_s = vec3_lerp(sampler->scales[frame_idx], sampler->scales[frame_idx + 1], factor);
                }

                Mat4 trans_mat = mat4_translate(anim_t);
                Mat4 rot_mat = quat_to_mat4(anim_r);
                Mat4 scale_mat = mat4_scale(anim_s);

                mat4_multiply(&obj->animated_local_transform, &trans_mat, &rot_mat);
                mat4_multiply(&obj->animated_local_transform, &obj->animated_local_transform, &scale_mat);
            }
        }
        else if (obj->model->num_skins > 0) {
            if (!obj->bone_matrices) {
                obj->bone_matrices = new Mat4[obj->model->skins[0].num_joints]();
            }
            if (obj->bone_matrices) {
                for (int j = 0; j < obj->model->skins[0].num_joints; ++j) {
                    mat4_identity(&obj->bone_matrices[j]);
                }
            }
        }
    }
}