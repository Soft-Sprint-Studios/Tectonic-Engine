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
#include "editor_selection.h"
#include "editor_math.h"
#include <float.h>

void Editor_AddToSelection(EntityType type, Int index, Int face_index, Int vertex_index) {
    EditorSelection* new_selections = new EditorSelection[g_EditorState.num_selections + 1];

    for (Int i = 0; i < g_EditorState.num_selections; ++i)
        new_selections[i] = g_EditorState.selections[i];

    delete[] g_EditorState.selections;
    g_EditorState.selections = new_selections;

    g_EditorState.selections[g_EditorState.num_selections] = EditorSelection{ type, index, face_index, vertex_index };
    g_EditorState.num_selections++;
}

void Editor_RemoveFromSelection(EntityType type, Int index) {
    Int found_at = -1;
    for (Int i = 0; i < g_EditorState.num_selections; ++i) {
        if (g_EditorState.selections[i].type == type && g_EditorState.selections[i].index == index) {
            found_at = i;
            break;
        }
    }
    if (found_at != -1) {
        for (Int i = found_at; i < g_EditorState.num_selections - 1; ++i) {
            g_EditorState.selections[i] = g_EditorState.selections[i + 1];
        }
        g_EditorState.num_selections--;
    }
}

void Editor_RemoveFaceFromSelection(Int brush_index, Int face_index) {
    Int found_at = -1;
    for (Int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];
        if (sel->type == ENTITY_BRUSH && sel->index == brush_index && sel->face_index == face_index) {
            found_at = i;
            break;
        }
    }
    if (found_at != -1) {
        for (Int i = found_at; i < g_EditorState.num_selections - 1; ++i) {
            g_EditorState.selections[i] = g_EditorState.selections[i + 1];
        }
        g_EditorState.num_selections--;
    }
}

Bool Editor_IsSelected(EntityType type, Int index) {
    for (Int i = 0; i < g_EditorState.num_selections; ++i) {
        if (g_EditorState.selections[i].type == type && g_EditorState.selections[i].index == index) {
            return true;
        }
    }
    return false;
}

Bool Editor_IsFaceSelected(Int brush_index, Int face_index) {
    for (Int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];
        if (sel->type == ENTITY_BRUSH && sel->index == brush_index && sel->face_index == face_index) {
            return true;
        }
    }
    return false;
}

void Editor_PickObjectAtScreenPos(Vec2 screen_pos, ViewportType viewport) {
    if (viewport != VIEW_PERSPECTIVE) return;

    Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[viewport]) * 2.0f - 1.0f;
    Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[viewport]) * 2.0f;
    Mat4 inv_proj, inv_view;
    Math::mat4_inverse(&g_proj_matrix[viewport], &inv_proj);
    Math::mat4_inverse(&g_view_matrix[viewport], &inv_view);
    Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
    Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip);
    ray_eye.z = -1.0f; ray_eye.w = 0.0f;
    Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye);
    Vec3 ray_dir_world = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
    Math::vec3_normalize(&ray_dir_world);
    Vec3 ray_origin_world = g_EditorState.editor_camera.position;

    Float closest_t = FLT_MAX;
    EntityType selected_type = ENTITY_NONE;
    Int selected_index = -1;
    Int hit_face_index = -1;

    for (Int i = 0; i < g_CurrentScene->numObjects; ++i) {
        SceneObject* obj = &g_CurrentScene->objects[i];
        if (!obj->model) continue;

        Float t;
        if (Math::RayIntersectsOBB(ray_origin_world, ray_dir_world,
            &obj->modelMatrix,
            obj->model->aabb_min,
            obj->model->aabb_max,
            &t) && t < closest_t) {
            closest_t = t;
            selected_type = ENTITY_MODEL;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (Int i = 0; i < g_CurrentScene->numBrushes; ++i) {
        Brush* brush = &g_CurrentScene->brushes[i];
        if (strcmp(brush->classname, "env_reflectionprobe") == 0) {
            continue;
        }

        Vec3 brush_local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 brush_local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        if (brush->numVertices > 0) {
            for (Int v_idx = 0; v_idx < brush->numVertices; ++v_idx) {
                brush_local_min.x = fminf(brush_local_min.x, brush->vertices[v_idx].pos.x);
                brush_local_min.y = fminf(brush_local_min.y, brush->vertices[v_idx].pos.y);
                brush_local_min.z = fminf(brush_local_min.z, brush->vertices[v_idx].pos.z);
                brush_local_max.x = fmaxf(brush_local_max.x, brush->vertices[v_idx].pos.x);
                brush_local_max.y = fmaxf(brush_local_max.y, brush->vertices[v_idx].pos.y);
                brush_local_max.z = fmaxf(brush_local_max.z, brush->vertices[v_idx].pos.z);
            }
        }
        else {
            brush_local_min = Vec3{ 0,0,0 };
            brush_local_max = Vec3{ 0,0,0 };
        }

        Float t_obb_dummy;
        if (!Math::RayIntersectsOBB(ray_origin_world, ray_dir_world,
            &brush->modelMatrix,
            brush_local_min,
            brush_local_max,
            &t_obb_dummy)) {
            continue;
        }

        Mat4 inv_brush_model_matrix;
        if (!Math::mat4_inverse(&brush->modelMatrix, &inv_brush_model_matrix)) {
            continue;
        }
        Vec3 ray_origin_local = Math::mat4_mul_vec3(&inv_brush_model_matrix, ray_origin_world);
        Vec3 ray_dir_local = Math::mat4_mul_vec3_dir(&inv_brush_model_matrix, ray_dir_world);

        for (Int face_idx = 0; face_idx < brush->numFaces; ++face_idx) {
            BrushFace* face = &brush->faces[face_idx];
            if (face->numVertexIndices < 3) continue;

            for (Int k = 0; k < face->numVertexIndices - 2; ++k) {
                Vec3 v0_local = brush->vertices[face->vertexIndices[0]].pos;
                Vec3 v1_local = brush->vertices[face->vertexIndices[k + 1]].pos;
                Vec3 v2_local = brush->vertices[face->vertexIndices[k + 2]].pos;

                Float t_triangle_local;
                if (Math::RayIntersectsTriangle(ray_origin_local, ray_dir_local, v0_local, v1_local, v2_local, &t_triangle_local)) {
                    Vec3 hit_point_local = Math::vec3_add(ray_origin_local, Math::vec3_muls(ray_dir_local, t_triangle_local));
                    Vec3 hit_point_world = Math::mat4_mul_vec3(&brush->modelMatrix, hit_point_local);
                    Float dist_to_hit_world = Math::vec3_length(Math::vec3_sub(hit_point_world, ray_origin_world));

                    if (t_triangle_local > 0.0f && dist_to_hit_world < closest_t) {
                        closest_t = dist_to_hit_world;
                        selected_type = ENTITY_BRUSH;
                        selected_index = i;
                        hit_face_index = face_idx;
                    }
                }
            }
        }
    }

    for (Int i = 0; i < g_CurrentScene->numActiveLights; ++i) {
        Light* light = &g_CurrentScene->lights[i];
        Float light_gizmo_radius = 0.5f;
        Vec3 P = Math::vec3_sub(light->pos, ray_origin_world);
        Float b_dot = Math::vec3_dot(P, ray_dir_world);
        Float det = b_dot * b_dot - Math::vec3_dot(P, P) + light_gizmo_radius * light_gizmo_radius;
        if (det < 0) continue;
        Float t_light = b_dot - sqrtf(det);
        if (t_light > 0 && t_light < closest_t) {
            closest_t = t_light;
            selected_type = ENTITY_LIGHT;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (Int i = 0; i < g_CurrentScene->numDecals; ++i) {
        Decal* decal = &g_CurrentScene->decals[i];

        Vec3 decal_local_min = { -0.5f, -0.5f, -0.5f };
        Vec3 decal_local_max = { 0.5f, 0.5f, 0.5f };

        Float t;
        if (Math::RayIntersectsOBB(ray_origin_world, ray_dir_world,
            &decal->modelMatrix,
            decal_local_min,
            decal_local_max,
            &t) && t < closest_t) {
            closest_t = t;
            selected_type = ENTITY_DECAL;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (Int i = 0; i < g_CurrentScene->numParticleEmitters; ++i) {
        ParticleEmitter* emitter = &g_CurrentScene->particleEmitters[i];
        Float emitter_gizmo_radius = 0.5f;
        Vec3 P = Math::vec3_sub(emitter->pos, ray_origin_world);
        Float b_dot = Math::vec3_dot(P, ray_dir_world);
        Float det = b_dot * b_dot - Math::vec3_dot(P, P) + emitter_gizmo_radius * emitter_gizmo_radius;
        if (det < 0) continue;
        Float t_emitter = b_dot - sqrtf(det);
        if (t_emitter > 0 && t_emitter < closest_t) {
            closest_t = t_emitter;
            selected_type = ENTITY_PARTICLE_EMITTER;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (Int i = 0; i < g_CurrentScene->numSoundEntities; ++i) {
        SoundEntity* sound = &g_CurrentScene->soundEntities[i];
        Float sound_gizmo_radius = 0.5f;
        Vec3 P = Math::vec3_sub(sound->pos, ray_origin_world);
        Float b_dot = Math::vec3_dot(P, ray_dir_world);
        Float det = b_dot * b_dot - Math::vec3_dot(P, P) + sound_gizmo_radius * sound_gizmo_radius;
        if (det < 0) continue;
        Float t_sound = b_dot - sqrtf(det);
        if (t_sound > 0 && t_sound < closest_t) {
            closest_t = t_sound;
            selected_type = ENTITY_SOUND;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (Int i = 0; i < g_CurrentScene->numLogicEntities; ++i) {
        LogicEntity* ent = &g_CurrentScene->logicEntities[i];
        Float logic_gizmo_radius = 0.5f;
        Vec3 P = Math::vec3_sub(ent->pos, ray_origin_world);
        Float b_dot = Math::vec3_dot(P, ray_dir_world);
        Float det = b_dot * b_dot - Math::vec3_dot(P, P) + logic_gizmo_radius * logic_gizmo_radius;
        if (det < 0) continue;
        Float t_logic = b_dot - sqrtf(det);
        if (t_logic > 0 && t_logic < closest_t) {
            closest_t = t_logic;
            selected_type = ENTITY_LOGIC;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    Float player_start_radius = 1.0f;
    Vec3 P = Math::vec3_sub(g_CurrentScene->playerStart.pos, ray_origin_world);
    Float b_dot = Math::vec3_dot(P, ray_dir_world);
    Float det = b_dot * b_dot - Math::vec3_dot(P, P) + player_start_radius * player_start_radius;
    if (det >= 0) {
        Float t_player = b_dot - sqrtf(det);
        if (t_player > 0 && t_player < closest_t) {
            closest_t = t_player;
            selected_type = ENTITY_PLAYERSTART;
            selected_index = 0;
            hit_face_index = -1;
        }
    }

    for (Int i = 0; i < g_CurrentScene->numVideoPlayers; ++i) {
        VideoPlayer* vp = &g_CurrentScene->videoPlayers[i];

        Vec3 vp_local_min = { -0.5f, -0.5f, -0.5f };
        Vec3 vp_local_max = { 0.5f, 0.5f, 0.5f };
        vp->modelMatrix = Math::create_trs_matrix(vp->pos, vp->rot, Vec3{ vp->size.x, vp->size.y, 0.01f });

        Float t;
        if (Math::RayIntersectsOBB(ray_origin_world, ray_dir_world,
            &vp->modelMatrix,
            vp_local_min,
            vp_local_max,
            &t) && t < closest_t) {
            closest_t = t;
            selected_type = ENTITY_VIDEO_PLAYER;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (Int i = 0; i < g_CurrentScene->numParallaxRooms; ++i) {
        ParallaxRoom* p = &g_CurrentScene->parallaxRooms[i];
        p->modelMatrix = Math::create_trs_matrix(p->pos, p->rot, Vec3{ p->size.x, p->size.y, 0.01f });
        Vec3 local_min = { -0.5f, -0.5f, -0.5f };
        Vec3 local_max = { 0.5f, 0.5f, 0.5f };
        Float t;
        if (Math::RayIntersectsOBB(ray_origin_world, ray_dir_world, &p->modelMatrix, local_min, local_max, &t) && t < closest_t) {
            closest_t = t;
            selected_type = ENTITY_PARALLAX_ROOM;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (Int i = 0; i < g_CurrentScene->numSprites; ++i) {
        Sprite* s = &g_CurrentScene->sprites[i];
        Float sprite_gizmo_radius = s->scale * 0.5f;
        Vec3 P = Math::vec3_sub(s->pos, ray_origin_world);
        Float b_dot = Math::vec3_dot(P, ray_dir_world);
        Float det = b_dot * b_dot - Math::vec3_dot(P, P) + sprite_gizmo_radius * sprite_gizmo_radius;
        if (det < 0) continue;
        Float t_sprite = b_dot - sqrtf(det);
        if (t_sprite > 0 && t_sprite < closest_t) {
            closest_t = t_sprite;
            selected_type = ENTITY_SPRITE;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    Bool ctrl_held = (SDL_GetModState() & KMOD_CTRL);

    if (selected_type != ENTITY_NONE) {
        if (selected_type == ENTITY_BRUSH) {
            if (ctrl_held) {
                if (Editor_IsFaceSelected(selected_index, hit_face_index)) {
                    Editor_RemoveFaceFromSelection(selected_index, hit_face_index);
                }
                else {
                    Editor_AddToSelection(selected_type, selected_index, hit_face_index, -1);
                }
            }
            else {
                Editor_ClearSelection();
                Editor_AddToSelection(selected_type, selected_index, hit_face_index, -1);
            }
        }
        else {
            if (ctrl_held) {
                if (Editor_IsSelected(selected_type, selected_index)) {
                    Editor_RemoveFromSelection(selected_type, selected_index);
                }
                else {
                    Editor_AddToSelection(selected_type, selected_index, -1, -1);
                }
            }
            else {
                Editor_ClearSelection();
                Editor_AddToSelection(selected_type, selected_index, -1, -1);
            }
        }
    }
    else {
        if (!ctrl_held) {
            Editor_ClearSelection();
        }
    }
    if (selected_type != ENTITY_NONE) {
        const Char* group_name = nullptr;
        Bool is_grouped = false;

        if (selected_type == ENTITY_BRUSH && hit_face_index != -1) {
            is_grouped = g_CurrentScene->brushes[selected_index].faces[hit_face_index].isGrouped;
            group_name = g_CurrentScene->brushes[selected_index].faces[hit_face_index].groupName;
        }
        else {
            switch (selected_type) {
            case ENTITY_MODEL: is_grouped = g_CurrentScene->objects[selected_index].isGrouped; group_name = g_CurrentScene->objects[selected_index].groupName; break;
            case ENTITY_BRUSH: is_grouped = g_CurrentScene->brushes[selected_index].isGrouped; group_name = g_CurrentScene->brushes[selected_index].groupName; break;
            case ENTITY_LIGHT: is_grouped = g_CurrentScene->lights[selected_index].isGrouped; group_name = g_CurrentScene->lights[selected_index].groupName; break;
            case ENTITY_DECAL: is_grouped = g_CurrentScene->decals[selected_index].isGrouped; group_name = g_CurrentScene->decals[selected_index].groupName; break;
            case ENTITY_SOUND: is_grouped = g_CurrentScene->soundEntities[selected_index].isGrouped; group_name = g_CurrentScene->soundEntities[selected_index].groupName; break;
            case ENTITY_PARTICLE_EMITTER: is_grouped = g_CurrentScene->particleEmitters[selected_index].isGrouped; group_name = g_CurrentScene->particleEmitters[selected_index].groupName; break;
            case ENTITY_SPRITE: is_grouped = g_CurrentScene->sprites[selected_index].isGrouped; group_name = g_CurrentScene->sprites[selected_index].groupName; break;
            case ENTITY_VIDEO_PLAYER: is_grouped = g_CurrentScene->videoPlayers[selected_index].isGrouped; group_name = g_CurrentScene->videoPlayers[selected_index].groupName; break;
            case ENTITY_PARALLAX_ROOM: is_grouped = g_CurrentScene->parallaxRooms[selected_index].isGrouped; group_name = g_CurrentScene->parallaxRooms[selected_index].groupName; break;
            case ENTITY_LOGIC: is_grouped = g_CurrentScene->logicEntities[selected_index].isGrouped; group_name = g_CurrentScene->logicEntities[selected_index].groupName; break;
            default: UNREACHABLE(); break;
            }
        }

        if (is_grouped && group_name && strlen(group_name) > 0) {
            if (selected_type == ENTITY_BRUSH && hit_face_index != -1) {
                for (Int i = 0; i < g_CurrentScene->brushes[selected_index].numFaces; ++i) {
                    if (g_CurrentScene->brushes[selected_index].faces[i].isGrouped && strcmp(g_CurrentScene->brushes[selected_index].faces[i].groupName, group_name) == 0) {
                        Editor_AddToSelection(ENTITY_BRUSH, selected_index, i, -1);
                    }
                }
            }
            else {
                for (Int i = 0; i < g_CurrentScene->numObjects; ++i) if (g_CurrentScene->objects[i].isGrouped && strcmp(g_CurrentScene->objects[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_MODEL, i, -1, -1);
                for (Int i = 0; i < g_CurrentScene->numBrushes; ++i) if (g_CurrentScene->brushes[i].isGrouped && strcmp(g_CurrentScene->brushes[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_BRUSH, i, -1, -1);
                for (Int i = 0; i < g_CurrentScene->numActiveLights; ++i) if (g_CurrentScene->lights[i].isGrouped && strcmp(g_CurrentScene->lights[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_LIGHT, i, -1, -1);
                for (Int i = 0; i < g_CurrentScene->numDecals; ++i) if (g_CurrentScene->decals[i].isGrouped && strcmp(g_CurrentScene->decals[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_DECAL, i, -1, -1);
                for (Int i = 0; i < g_CurrentScene->numSoundEntities; ++i) if (g_CurrentScene->soundEntities[i].isGrouped && strcmp(g_CurrentScene->soundEntities[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_SOUND, i, -1, -1);
                for (Int i = 0; i < g_CurrentScene->numParticleEmitters; ++i) if (g_CurrentScene->particleEmitters[i].isGrouped && strcmp(g_CurrentScene->particleEmitters[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_PARTICLE_EMITTER, i, -1, -1);
                for (Int i = 0; i < g_CurrentScene->numSprites; ++i) if (g_CurrentScene->sprites[i].isGrouped && strcmp(g_CurrentScene->sprites[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_SPRITE, i, -1, -1);
                for (Int i = 0; i < g_CurrentScene->numVideoPlayers; ++i) if (g_CurrentScene->videoPlayers[i].isGrouped && strcmp(g_CurrentScene->videoPlayers[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_VIDEO_PLAYER, i, -1, -1);
                for (Int i = 0; i < g_CurrentScene->numParallaxRooms; ++i) if (g_CurrentScene->parallaxRooms[i].isGrouped && strcmp(g_CurrentScene->parallaxRooms[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_PARALLAX_ROOM, i, -1, -1);
                for (Int i = 0; i < g_CurrentScene->numLogicEntities; ++i) if (g_CurrentScene->logicEntities[i].isGrouped && strcmp(g_CurrentScene->logicEntities[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_LOGIC, i, -1, -1);
            }
        }
    }
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (primary && primary->type == ENTITY_BRUSH) {
        primary->face_index = hit_face_index;
        if (hit_face_index != -1) {
            Brush* brush_ptr = &g_CurrentScene->brushes[primary->index];
            BrushFace* face_ptr = &brush_ptr->faces[hit_face_index];
            if (face_ptr->numVertexIndices > 0) {
                primary->vertex_index = face_ptr->vertexIndices[0];
            }
            else {
                primary->vertex_index = -1;
            }
        }
        else {
            primary->face_index = 0;
            Brush* brush_ptr = &g_CurrentScene->brushes[primary->index];
            if (brush_ptr->numFaces > 0 && brush_ptr->faces[0].numVertexIndices > 0) {
                primary->vertex_index = brush_ptr->faces[0].vertexIndices[0];
            }
            else {
                primary->vertex_index = -1;
            }
        }
    }
}

Int Editor_PickVertexAtScreenPos(Scene* scene, Vec2 screen_pos, ViewportType viewport) {
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (!primary || primary->type != ENTITY_BRUSH) {
        return -1;
    }

    Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[viewport]) * 2.0f - 1.0f;
    Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[viewport]) * 2.0f;
    Mat4 inv_proj, inv_view;
    Math::mat4_inverse(&g_proj_matrix[viewport], &inv_proj);
    Math::mat4_inverse(&g_view_matrix[viewport], &inv_view);
    Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
    Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip);
    ray_eye.z = -1.0f; ray_eye.w = 0.0f;
    Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye);
    Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
    Math::vec3_normalize(&ray_dir);
    Vec3 ray_origin = g_EditorState.editor_camera.position;

    Brush* b = &scene->brushes[primary->index];
    Float closest_t = FLT_MAX;
    Int picked_vertex = -1;
    const Float pick_radius = 0.1f;

    for (Int i = 0; i < b->numVertices; ++i) {
        Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);

        Vec3 oc = Math::vec3_sub(ray_origin, vert_world_pos);
        Float b_dot = Math::vec3_dot(oc, ray_dir);
        Float c = Math::vec3_dot(oc, oc) - pick_radius * pick_radius;
        Float discriminant = b_dot * b_dot - c;
        if (discriminant > 0) {
            Float t = -b_dot - sqrtf(discriminant);
            if (t > 0 && t < closest_t) {
                closest_t = t;
                picked_vertex = i;
            }
        }
    }

    return picked_vertex;
}

EditorSelection* Editor_GetPrimarySelection() {
    if (g_EditorState.num_selections == 0) return nullptr;
    return &g_EditorState.selections[g_EditorState.num_selections - 1];
}

void Editor_ClearSelection() {
    g_EditorState.num_selections = 0;
}

Bool FindEntityInScene(Scene* scene, const Char* name, EntityType* out_type, Int* out_index) {
    if (name == nullptr || *name == '\0') return false;
    for (Int i = 0; i < scene->numObjects; ++i) if (strcmp(scene->objects[i].targetname, name) == 0) { *out_type = ENTITY_MODEL; *out_index = i; return true; }
    for (Int i = 0; i < scene->numBrushes; ++i) if (strcmp(scene->brushes[i].targetname, name) == 0) { *out_type = ENTITY_BRUSH; *out_index = i; return true; }
    for (Int i = 0; i < scene->numActiveLights; ++i) if (strcmp(scene->lights[i].targetname, name) == 0) { *out_type = ENTITY_LIGHT; *out_index = i; return true; }
    for (Int i = 0; i < scene->numSoundEntities; ++i) if (strcmp(scene->soundEntities[i].targetname, name) == 0) { *out_type = ENTITY_SOUND; *out_index = i; return true; }
    for (Int i = 0; i < scene->numParticleEmitters; ++i) if (strcmp(scene->particleEmitters[i].targetname, name) == 0) { *out_type = ENTITY_PARTICLE_EMITTER; *out_index = i; return true; }
    for (Int i = 0; i < scene->numVideoPlayers; ++i) if (strcmp(scene->videoPlayers[i].targetname, name) == 0) { *out_type = ENTITY_VIDEO_PLAYER; *out_index = i; return true; }
    for (Int i = 0; i < scene->numSprites; ++i) if (strcmp(scene->sprites[i].targetname, name) == 0) { *out_type = ENTITY_SPRITE; *out_index = i; return true; }
    for (Int i = 0; i < scene->numLogicEntities; ++i) if (strcmp(scene->logicEntities[i].targetname, name) == 0) { *out_type = ENTITY_LOGIC; *out_index = i; return true; }
    return false;
}

Bool Editor_FindNamedEntityPosition(Scene* scene, const Char* name, Vec3* out_pos) {
    if (name == nullptr || *name == '\0') return false;
    EntityType type;
    Int index;
    if (FindEntityInScene(scene, name, &type, &index)) {
        switch (type) {
        case ENTITY_MODEL: *out_pos = scene->objects[index].pos; return true;
        case ENTITY_BRUSH: *out_pos = scene->brushes[index].pos; return true;
        case ENTITY_LIGHT: *out_pos = scene->lights[index].pos; return true;
        case ENTITY_SOUND: *out_pos = scene->soundEntities[index].pos; return true;
        case ENTITY_PARTICLE_EMITTER: *out_pos = scene->particleEmitters[index].pos; return true;
        case ENTITY_VIDEO_PLAYER: *out_pos = scene->videoPlayers[index].pos; return true;
        case ENTITY_SPRITE: *out_pos = scene->sprites[index].pos; return true;
        case ENTITY_LOGIC: *out_pos = scene->logicEntities[index].pos; return true;
        default: return false;
        }
    }
    return false;
}