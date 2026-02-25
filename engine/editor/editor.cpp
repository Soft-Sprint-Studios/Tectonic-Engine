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
#include "engine.h"
#include "editor_internal.h"
#include "editor.h"
#include "editor_windows.h"
#include "editor_math.h"
#include "editor_selection.h"
#include "editor_actions.h"
#include "editor_geometry.h"
#include "editor_geometry_helpers.h"
#include "editor_misc.h"
#include "commands.h"
#include <stdlib.h>
#include "gl_console.h"
#include "lightmapper.h"
#include <GL/glew.h>
#include <SDL.h>
#include "gl_misc.h"
#include <math.h>
#include <float.h>
#include <sys/stat.h>
#include <SDL_image.h>
#include "sound_system.h"
#include "texturemanager.h"
#include "water_manager.h"
#include "io_system.h"
#include "gl_video_player.h"
#include "game_data.h"
#include "cvar.h"
#include "animations.h"
#include "map_misc.h"

EditorState g_EditorState;
Scene* g_CurrentScene;
Mat4 g_view_matrix[VIEW_COUNT];
Mat4 g_proj_matrix[VIEW_COUNT];
Bool g_is_map_dirty = false;
PendingEditorAction g_pending_action = PENDING_ACTION_NONE;
BrushFace g_copiedFaceProperties;
Bool g_hasCopiedFace = false;
Camera g_last_editor_camera_state;
Bool g_has_last_camera_state = false;

void Editor_ProcessEvent(SDL_Event* event, Scene* scene, Engine* engine) {
    if (event->type == SDL_MOUSEMOTION) {
        Bool can_look = g_EditorState.is_in_z_mode || (g_EditorState.is_viewport_focused[VIEW_PERSPECTIVE] && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT)));
        if (can_look) {
            g_EditorState.editor_camera.yaw += event->motion.xrel * 0.005f;
            g_EditorState.editor_camera.pitch -= event->motion.yrel * 0.005f;
        }
    }
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (event->type == SDL_KEYUP && event->key.keysym.sym == SDLK_c) {
        if (g_EditorState.is_clipping) {
            if (primary && primary->type == ENTITY_BRUSH && g_EditorState.clip_point_count >= 2) {
                if (scene->numBrushes >= Common::MAX_BRUSHES - 1) {
                    Console_Printf_Error("Cannot clip brush, MAX_BRUSHES limit reached.");
                    g_EditorState.is_clipping = false;
                    return;
                }

                Int original_brush_index = primary->index;
                Brush* original_brush = &scene->brushes[original_brush_index];

                Undo_BeginEntityModification(scene, ENTITY_BRUSH, original_brush_index);

                Brush brush_b_storage = { 0 };
                Brush_DeepCopy(&brush_b_storage, original_brush);

                Vec3 p1 = g_EditorState.clip_points[0];
                Vec3 p2 = g_EditorState.clip_points[1];
                Vec3 plane_normal;
                Vec3 dir = Math::vec3_sub(p2, p1);

                if (g_EditorState.clip_view == VIEW_TOP_XZ) { plane_normal = Math::vec3_cross(dir, Vec3{ 0, 1, 0 }); }
                else if (g_EditorState.clip_view == VIEW_FRONT_XY) { plane_normal = Math::vec3_cross(dir, Vec3{ 0, 0, 1 }); }
                else { plane_normal = Math::vec3_cross(dir, Vec3{ 1, 0, 0 }); }
                Math::vec3_normalize(&plane_normal);

                Float side_check = Math::vec3_dot(plane_normal, Math::vec3_sub(g_EditorState.clip_side_point, p1));
                if (side_check < 0.0f) {
                    plane_normal = Math::vec3_muls(plane_normal, -1.0f);
                }

                Float plane_d_a = -Math::vec3_dot(plane_normal, p1);
                Float plane_d_b = -plane_d_a;
                Vec3 plane_normal_b = Math::vec3_muls(plane_normal, -1.0f);

                Brush_Clip(original_brush, plane_normal, plane_d_a);
                Brush_CreateRenderData(original_brush);
                if (original_brush->physicsBody) Physics::RemoveRigidBody(engine->physicsWorld, original_brush->physicsBody);
                if (Brush_IsSolid(original_brush) && original_brush->numVertices > 0) {
                    Vec3* world_verts = new Vec3[original_brush->numVertices];

                    for (Int k = 0; k < original_brush->numVertices; ++k)
                        world_verts[k] = Math::mat4_mul_vec3(&original_brush->modelMatrix, original_brush->vertices[k].pos);

                    original_brush->physicsBody = Physics::CreateStaticConvexHull(engine->physicsWorld, reinterpret_cast<const Float*>(world_verts), original_brush->numVertices);

                    delete[] world_verts;
                }
                else {
                    original_brush->physicsBody = nullptr;
                }

                Brush_Clip(&brush_b_storage, plane_normal_b, plane_d_b);

                if (brush_b_storage.numVertices > 0) {
                    Int new_brush_index = scene->numBrushes;
                    scene->brushes[new_brush_index] = brush_b_storage;
                    scene->numBrushes++;

                    Brush* new_b_ptr = &scene->brushes[new_brush_index];
                    Brush_CreateRenderData(new_b_ptr);
                    if (Brush_IsSolid(new_b_ptr) && new_b_ptr->numVertices > 0) {
                        Vec3* world_verts = new Vec3[new_b_ptr->numVertices];

                        for (Int k = 0; k < new_b_ptr->numVertices; ++k)
                            world_verts[k] = Math::mat4_mul_vec3(&new_b_ptr->modelMatrix, new_b_ptr->vertices[k].pos);

                        new_b_ptr->physicsBody = Physics::CreateStaticConvexHull(engine->physicsWorld, reinterpret_cast<const Float*>(world_verts), new_b_ptr->numVertices);

                        delete[] world_verts;
                    }
                    else {
                        new_b_ptr->physicsBody = nullptr;
                    }
                    Undo_PushCreateEntity(scene, ENTITY_BRUSH, new_brush_index, "Clip Brush (Create B)");
                }
                else {
                    Brush_FreeData(&brush_b_storage);
                }

                Undo_EndEntityModification(scene, ENTITY_BRUSH, original_brush_index, "Clip Brush (Modify A)");
                Editor_ClearSelection();
            }
            g_EditorState.is_clipping = false;
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (g_EditorState.show_sprinkle_tool_window && g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
            g_EditorState.is_sprinkling = true;
            g_EditorState.sprinkle_timer = 0.0f;
            return;
        }
        if (g_EditorState.is_painting_mode_enabled && primary && primary->type == ENTITY_BRUSH) {
            if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
                g_EditorState.is_painting = true;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                return;
            }
            Bool is_hovering_paint_viewport = false;
            for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    is_hovering_paint_viewport = true;
                    break;
                }
            }
            if (is_hovering_paint_viewport) {
                g_EditorState.is_painting = true;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                return;
            }
        }
        if (g_EditorState.is_sculpting_mode_enabled && primary && primary->type == ENTITY_BRUSH) {
            if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
                g_EditorState.is_sculpting = true;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                return;
            }
            Bool is_hovering_sculpt_viewport = false;
            for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    is_hovering_sculpt_viewport = true;
                    break;
                }
            }
            if (is_hovering_sculpt_viewport) {
                g_EditorState.is_sculpting = true;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                return;
            }
        }
        if (g_EditorState.is_clipping) {
            for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    if (g_EditorState.clip_point_count < 2) {
                        if (g_EditorState.clip_point_count == 0) {
                            g_EditorState.clip_view = (ViewportType)i;
                            if (primary && primary->type == ENTITY_BRUSH) {
                                Brush* b = &scene->brushes[primary->index];
                                switch (g_EditorState.clip_view) {
                                case VIEW_TOP_XZ:   g_EditorState.clip_plane_depth = b->pos.y; break;
                                case VIEW_FRONT_XY: g_EditorState.clip_plane_depth = b->pos.z; break;
                                case VIEW_SIDE_YZ:  g_EditorState.clip_plane_depth = b->pos.x; break;
                                default: UNREACHABLE();  break;
                                }
                            }
                            else {
                                g_EditorState.clip_plane_depth = 0.0f;
                            }
                        }

                        if (g_EditorState.clip_view == (ViewportType)i) {
                            g_EditorState.clip_points[g_EditorState.clip_point_count] = ScreenToWorld_Clip(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                            g_EditorState.clip_point_count++;
                        }
                    }
                    else {
                        g_EditorState.clip_side_point = ScreenToWorld_Clip(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                    }
                    return;
                }
            }
        }

        ViewportType active_viewport = VIEW_COUNT;
        for (Int i = 0; i < VIEW_COUNT; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                active_viewport = (ViewportType)i;
                break;
            }
        }
        if (g_EditorState.selected_brush_hovered_handle != PREVIEW_BRUSH_HANDLE_NONE) {
            if (primary && primary->type == ENTITY_BRUSH) {
                g_EditorState.is_dragging_selected_brush_handle = true;
                g_EditorState.selected_brush_active_handle = g_EditorState.selected_brush_hovered_handle;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                return;
            }
        }
        else if (g_EditorState.is_hovering_selected_brush_body && active_viewport >= VIEW_TOP_XZ && active_viewport <= VIEW_SIDE_YZ) {
            if (primary && primary->type == ENTITY_BRUSH) {
                g_EditorState.is_dragging_selected_brush_body = true;
                g_EditorState.selected_brush_drag_body_view = active_viewport;
                Vec3 raw_start_mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
                if (g_EditorState.snap_to_grid) {
                    raw_start_mouse_world.x = SnapValue(raw_start_mouse_world.x, g_EditorState.grid_size);
                    raw_start_mouse_world.y = SnapValue(raw_start_mouse_world.y, g_EditorState.grid_size);
                    raw_start_mouse_world.z = SnapValue(raw_start_mouse_world.z, g_EditorState.grid_size);
                }
                g_EditorState.selected_brush_drag_body_start_mouse_world = raw_start_mouse_world;
                if (g_EditorState.gizmo_drag_start_positions)
                    delete[] g_EditorState.gizmo_drag_start_positions;

                g_EditorState.gizmo_drag_start_positions = new Vec3[g_EditorState.num_selections];

                Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);

                for (Int i = 0; i < g_EditorState.num_selections; ++i) {
                    EditorSelection* sel = &g_EditorState.selections[i];
                    Vec3 pos = { 0 };
                    switch (sel->type) {
                    case ENTITY_MODEL: pos = scene->objects[sel->index].pos; break;
                    case ENTITY_BRUSH: pos = scene->brushes[sel->index].pos; break;
                    case ENTITY_LIGHT: pos = scene->lights[sel->index].pos; break;
                    case ENTITY_DECAL: pos = scene->decals[sel->index].pos; break;
                    case ENTITY_SOUND: pos = scene->soundEntities[sel->index].pos; break;
                    case ENTITY_PARTICLE_EMITTER: pos = scene->particleEmitters[sel->index].pos; break;
                    case ENTITY_SPRITE: pos = scene->sprites[sel->index].pos; break;
                    case ENTITY_VIDEO_PLAYER: pos = scene->videoPlayers[sel->index].pos; break;
                    case ENTITY_PARALLAX_ROOM: pos = scene->parallaxRooms[sel->index].pos; break;
                    case ENTITY_LOGIC: pos = scene->logicEntities[sel->index].pos; break;
                    case ENTITY_PLAYERSTART: pos = scene->playerStart.pos; break;
                    }
                    g_EditorState.gizmo_drag_start_positions[i] = pos;
                }
                return;
            }
        }
        if (g_EditorState.is_in_brush_creation_mode && g_EditorState.preview_brush_hovered_handle != PREVIEW_BRUSH_HANDLE_NONE && active_viewport >= VIEW_TOP_XZ && active_viewport <= VIEW_SIDE_YZ) {
            g_EditorState.is_dragging_preview_brush_handle = true;
            g_EditorState.preview_brush_active_handle = g_EditorState.preview_brush_hovered_handle;
            g_EditorState.preview_brush_drag_handle_view = active_viewport;
            return;
        }
        else if (g_EditorState.is_in_brush_creation_mode && g_EditorState.is_hovering_preview_brush_body && active_viewport >= VIEW_TOP_XZ && active_viewport <= VIEW_SIDE_YZ) {
            g_EditorState.is_dragging_preview_brush_body = true;
            g_EditorState.preview_brush_drag_body_view = active_viewport;
            g_EditorState.preview_brush_drag_body_start_mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
            g_EditorState.preview_brush_drag_body_start_brush_world_min_at_drag_start = g_EditorState.preview_brush_world_min;
            return;
        }
        if (g_EditorState.vertex_gizmo_hovered_axis != GIZMO_AXIS_NONE && g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
            g_EditorState.is_manipulating_vertex_gizmo = true;
            g_EditorState.vertex_gizmo_active_axis = g_EditorState.vertex_gizmo_hovered_axis;
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);

            Brush* b = &scene->brushes[primary->index];
            g_EditorState.vertex_drag_start_pos_world = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[primary->vertex_index].pos);

            Vec3 cam_forward = { g_view_matrix[VIEW_PERSPECTIVE].m[2], g_view_matrix[VIEW_PERSPECTIVE].m[6], g_view_matrix[VIEW_PERSPECTIVE].m[10] };
            Vec3 axis_dir = { 0 };
            if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
            if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
            if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
            Float dot_product = fabsf(Math::vec3_dot(axis_dir, cam_forward));
            if (dot_product > 0.99f) { if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) { g_EditorState.vertex_gizmo_drag_plane_normal = Vec3{ 0, 1, 0 }; } else { g_EditorState.vertex_gizmo_drag_plane_normal = Vec3{ 1, 0, 0 }; } }
            else { g_EditorState.vertex_gizmo_drag_plane_normal = Math::vec3_cross(axis_dir, cam_forward); Math::vec3_normalize(&g_EditorState.vertex_gizmo_drag_plane_normal); }
            g_EditorState.vertex_gizmo_drag_plane_d = -Math::vec3_dot(g_EditorState.vertex_gizmo_drag_plane_normal, g_EditorState.vertex_drag_start_pos_world);

            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
            Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
            Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
            Mat4 inv_proj, inv_view; Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
            Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
            Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; Math::vec3_normalize(&ray_dir);
            ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.vertex_gizmo_drag_plane_normal, g_EditorState.vertex_gizmo_drag_plane_d, &g_EditorState.vertex_gizmo_drag_start_world);
            return;
        }
        else if (g_EditorState.gizmo_hovered_axis != GIZMO_AXIS_NONE && active_viewport != VIEW_COUNT) {
            Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);
            g_EditorState.is_manipulating_gizmo = true;
            g_EditorState.gizmo_drag_has_cloned = false;
            g_EditorState.gizmo_selection_centroid = Vec3{ 0 };
            for (Int i = 0; i < g_EditorState.num_selections; ++i) {
                Vec3 pos;
                switch (g_EditorState.selections[i].type) {
                case ENTITY_MODEL: pos = scene->objects[g_EditorState.selections[i].index].pos; break;
                case ENTITY_BRUSH: pos = scene->brushes[g_EditorState.selections[i].index].pos; break;
                case ENTITY_LIGHT: pos = scene->lights[g_EditorState.selections[i].index].pos; break;
                case ENTITY_DECAL: pos = scene->decals[g_EditorState.selections[i].index].pos; break;
                case ENTITY_SOUND: pos = scene->soundEntities[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PARTICLE_EMITTER: pos = scene->particleEmitters[g_EditorState.selections[i].index].pos; break;
                case ENTITY_SPRITE: pos = scene->sprites[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PLAYERSTART: pos = scene->playerStart.pos; break;
                case ENTITY_VIDEO_PLAYER: pos = scene->videoPlayers[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PARALLAX_ROOM: pos = scene->parallaxRooms[g_EditorState.selections[i].index].pos; break;
                case ENTITY_LOGIC: pos = scene->logicEntities[g_EditorState.selections[i].index].pos; break;
                default: UNREACHABLE();  break;
                }
                g_EditorState.gizmo_selection_centroid = Math::vec3_add(g_EditorState.gizmo_selection_centroid, pos);
            }
            if (g_EditorState.num_selections > 0) {
                g_EditorState.gizmo_selection_centroid = Math::vec3_muls(g_EditorState.gizmo_selection_centroid, 1.0f / g_EditorState.num_selections);
            }
            delete[] g_EditorState.gizmo_drag_start_positions;
            delete[] g_EditorState.gizmo_drag_start_rotations;
            delete[] g_EditorState.gizmo_drag_start_scales;

            g_EditorState.gizmo_drag_start_positions = new Vec3[g_EditorState.num_selections];
            g_EditorState.gizmo_drag_start_rotations = new Vec3[g_EditorState.num_selections];
            g_EditorState.gizmo_drag_start_scales = new Vec3[g_EditorState.num_selections];

            for (Int i = 0; i < g_EditorState.num_selections; ++i) {
                EditorSelection* sel = &g_EditorState.selections[i];
                switch (sel->type) {
                case ENTITY_MODEL:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->objects[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->objects[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = scene->objects[sel->index].scale;
                    break;
                case ENTITY_BRUSH:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->brushes[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->brushes[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = scene->brushes[sel->index].scale;
                    break;
                case ENTITY_LIGHT:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->lights[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->lights[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = Vec3{ 1,1,1 };
                    break;
                case ENTITY_DECAL:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->decals[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->decals[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = scene->decals[sel->index].size;
                    break;
                case ENTITY_SOUND:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->soundEntities[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = Vec3{ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = Vec3{ 1,1,1 };
                    break;
                case ENTITY_PARTICLE_EMITTER:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->particleEmitters[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = Vec3{ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = Vec3{ 1,1,1 };
                    break;
                case ENTITY_SPRITE:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->sprites[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = Vec3{ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = Vec3{ scene->sprites[sel->index].scale, scene->sprites[sel->index].scale, scene->sprites[sel->index].scale };
                    break;
                case ENTITY_VIDEO_PLAYER:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->videoPlayers[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->videoPlayers[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = Vec3{ scene->videoPlayers[sel->index].size.x, scene->videoPlayers[sel->index].size.y, 1.0f };
                    break;
                case ENTITY_PARALLAX_ROOM:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->parallaxRooms[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->parallaxRooms[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = Vec3{ scene->parallaxRooms[sel->index].size.x, scene->parallaxRooms[sel->index].size.y, 1.0f };
                    break;
                case ENTITY_LOGIC:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->logicEntities[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->logicEntities[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = Vec3{ 1,1,1 };
                    break;
                case ENTITY_PLAYERSTART:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->playerStart.pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = Vec3{ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = Vec3{ 1,1,1 };
                    break;
                default: UNREACHABLE();  break;
                }
            }
            g_EditorState.gizmo_active_axis = g_EditorState.gizmo_hovered_axis;
            g_EditorState.gizmo_drag_view = active_viewport;

            if (primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
            }
            else {
                if (g_EditorState.is_in_brush_creation_mode) {
                    g_EditorState.gizmo_drag_object_start_pos = g_EditorState.preview_brush.pos;
                    g_EditorState.gizmo_drag_object_start_rot = g_EditorState.preview_brush.rot;
                    g_EditorState.gizmo_drag_object_start_scale = g_EditorState.preview_brush.scale;
                }
                else if (primary) {
                    switch (primary->type) {
                    case ENTITY_MODEL:
                        g_EditorState.gizmo_drag_object_start_pos = scene->objects[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->objects[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = scene->objects[primary->index].scale;
                        break;
                    case ENTITY_BRUSH:
                        g_EditorState.gizmo_drag_object_start_pos = scene->brushes[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->brushes[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = scene->brushes[primary->index].scale;
                        break;
                    case ENTITY_LIGHT:
                        g_EditorState.gizmo_drag_object_start_pos = scene->lights[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->lights[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = Vec3{ 1,1,1 };
                        break;
                    case ENTITY_DECAL:
                        g_EditorState.gizmo_drag_object_start_pos = scene->decals[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->decals[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = scene->decals[primary->index].size;
                        break;
                    case ENTITY_SOUND:
                        g_EditorState.gizmo_drag_object_start_pos = scene->soundEntities[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = Vec3{ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = Vec3{ 1,1,1 };
                        break;
                    case ENTITY_PARTICLE_EMITTER:
                        g_EditorState.gizmo_drag_object_start_pos = scene->particleEmitters[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = Vec3{ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = Vec3{ 1,1,1 };
                        break;
                    case ENTITY_SPRITE:
                        g_EditorState.gizmo_drag_object_start_pos = scene->sprites[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = Vec3{ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = Vec3{ scene->sprites[primary->index].scale, 1, 1 };
                        break;
                    case ENTITY_PLAYERSTART:
                        g_EditorState.gizmo_drag_object_start_pos = scene->playerStart.pos;
                        g_EditorState.gizmo_drag_object_start_rot = Vec3{ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = Vec3{ 1,1,1 };
                        break;
                    case ENTITY_VIDEO_PLAYER:
                        g_EditorState.gizmo_drag_object_start_pos = scene->videoPlayers[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->videoPlayers[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = Vec3{ scene->videoPlayers[primary->index].size.x, scene->videoPlayers[primary->index].size.y, 1.0f };
                        break;
                    case ENTITY_PARALLAX_ROOM:
                        g_EditorState.gizmo_drag_object_start_pos = scene->parallaxRooms[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->parallaxRooms[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = Vec3{ scene->parallaxRooms[primary->index].size.x, scene->parallaxRooms[primary->index].size.y, 1.0f };
                        break;
                    default: UNREACHABLE();  break;
                    }
                }
            }

            switch (g_EditorState.current_gizmo_operation) {
            case GIZMO_OP_TRANSLATE:
            case GIZMO_OP_SCALE: {
                Vec3 drag_object_anchor_pos = g_EditorState.is_in_brush_creation_mode ? g_EditorState.preview_brush.pos : g_EditorState.gizmo_selection_centroid;
                if (active_viewport == VIEW_PERSPECTIVE) {
                    Vec3 cam_forward = { g_view_matrix[VIEW_PERSPECTIVE].m[2], g_view_matrix[VIEW_PERSPECTIVE].m[6], g_view_matrix[VIEW_PERSPECTIVE].m[10] };
                    Vec3 axis_dir = { 0 };
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
                    Float dot_product = fabsf(Math::vec3_dot(axis_dir, cam_forward));
                    if (dot_product > 0.99f) { if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) { g_EditorState.gizmo_drag_plane_normal = Vec3{ 0, 1, 0 }; } else { g_EditorState.gizmo_drag_plane_normal = Vec3{ 1, 0, 0 }; } }
                    else { g_EditorState.gizmo_drag_plane_normal = Math::vec3_cross(axis_dir, cam_forward); Math::vec3_normalize(&g_EditorState.gizmo_drag_plane_normal); }
                    g_EditorState.gizmo_drag_plane_d = -Math::vec3_dot(g_EditorState.gizmo_drag_plane_normal, drag_object_anchor_pos);
                    Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                    Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                    Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                    Mat4 inv_proj, inv_view; Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                    Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                    Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; Math::vec3_normalize(&ray_dir);
                    ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &g_EditorState.gizmo_drag_start_world);
                }
                else {
                    g_EditorState.gizmo_drag_start_world = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
                }
                break;
            }
            case GIZMO_OP_ROTATE: {
                if (active_viewport != VIEW_PERSPECTIVE) break;
                Vec3 object_pos_for_rotate_plane = g_EditorState.is_in_brush_creation_mode ? g_EditorState.preview_brush.pos : g_EditorState.gizmo_selection_centroid;
                if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) g_EditorState.gizmo_drag_plane_normal = Vec3{ 1,0,0 };
                if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) g_EditorState.gizmo_drag_plane_normal = Vec3{ 0,1,0 };
                if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) g_EditorState.gizmo_drag_plane_normal = Vec3{ 0,0,1 };
                Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; Math::vec3_normalize(&ray_dir);
                Vec3 intersect_point;
                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, -Math::vec3_dot(g_EditorState.gizmo_drag_plane_normal, object_pos_for_rotate_plane), &intersect_point)) {
                    g_EditorState.gizmo_rotation_start_vec = Math::vec3_sub(intersect_point, object_pos_for_rotate_plane);
                    Math::vec3_normalize(&g_EditorState.gizmo_rotation_start_vec);
                }
                break;
            }
            }
            return;
        }
        else if (active_viewport >= VIEW_TOP_XZ && !g_EditorState.is_manipulating_gizmo && primary && primary->type == ENTITY_BRUSH) {
            Brush* b = &scene->brushes[primary->index];
            Vec3 mouse_world_pos = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
            Float pick_dist_sq = (g_EditorState.ortho_cam_zoom[active_viewport - 1] * 0.05f);
            pick_dist_sq *= pick_dist_sq;
            for (Int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                Float dist_sq = 0;
                if (active_viewport == VIEW_TOP_XZ) dist_sq = (vert_world_pos.x - mouse_world_pos.x) * (vert_world_pos.x - mouse_world_pos.x) + (vert_world_pos.z - mouse_world_pos.z) * (vert_world_pos.z - mouse_world_pos.z);
                if (active_viewport == VIEW_FRONT_XY) dist_sq = (vert_world_pos.x - mouse_world_pos.x) * (vert_world_pos.x - mouse_world_pos.x) + (vert_world_pos.y - mouse_world_pos.y) * (vert_world_pos.y - mouse_world_pos.y);
                if (active_viewport == VIEW_SIDE_YZ) dist_sq = (vert_world_pos.z - mouse_world_pos.z) * (vert_world_pos.z - mouse_world_pos.z) + (vert_world_pos.y - mouse_world_pos.y) * (vert_world_pos.y - mouse_world_pos.y);
                if (dist_sq < pick_dist_sq) {
                    g_EditorState.is_vertex_manipulating = true;
                    g_EditorState.manipulated_vertex_index = v_idx;
                    primary->vertex_index = v_idx;
                    g_EditorState.vertex_manipulation_view = active_viewport;
                    g_EditorState.vertex_manipulation_start_pos = mouse_world_pos;
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                    return;
                }
            }
        }
        if (active_viewport == VIEW_PERSPECTIVE && primary && primary->type == ENTITY_BRUSH && !g_EditorState.is_manipulating_gizmo && !g_EditorState.is_manipulating_vertex_gizmo) {
            Int picked_vertex = Editor_PickVertexAtScreenPos(scene, g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE], VIEW_PERSPECTIVE);
            if (picked_vertex != -1) {
                primary->vertex_index = picked_vertex;
                return;
            }
        }
        if (active_viewport == VIEW_PERSPECTIVE && !g_EditorState.is_in_brush_creation_mode) {
            Editor_PickObjectAtScreenPos(g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE], VIEW_PERSPECTIVE);
        }

        if (g_EditorState.num_selections == 0 && active_viewport != VIEW_PERSPECTIVE && active_viewport != VIEW_COUNT && !g_EditorState.is_in_brush_creation_mode) {
            g_EditorState.is_dragging_for_creation = true;
            g_EditorState.brush_creation_start_point_2d_drag = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
            g_EditorState.brush_creation_view = active_viewport;
            g_EditorState.preview_brush_world_min = g_EditorState.brush_creation_start_point_2d_drag;
            g_EditorState.preview_brush_world_max = g_EditorState.brush_creation_start_point_2d_drag;
            Editor_UpdatePreviewBrushForInitialDrag(g_EditorState.preview_brush_world_min, g_EditorState.preview_brush_world_max, g_EditorState.brush_creation_view);
        }
    }
    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        if (g_EditorState.is_sprinkling) {
            g_EditorState.is_sprinkling = false;
        }
        if (g_EditorState.is_painting) {
            g_EditorState.is_painting = false;
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Vertex Paint");
        }
        if (g_EditorState.is_sculpting) {
            g_EditorState.is_sculpting = false;
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Vertex Sculpt");
            return;
        }
        if (g_EditorState.is_manipulating_vertex_gizmo) {
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Move Vertex (Gizmo)");
            g_EditorState.is_manipulating_vertex_gizmo = false;
            g_EditorState.vertex_gizmo_active_axis = GIZMO_AXIS_NONE;
        }
        if (g_EditorState.is_vertex_manipulating) {
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Move Vertex");
            g_EditorState.is_vertex_manipulating = false;
        }
        if (g_EditorState.is_dragging_selected_brush_handle) {
            g_EditorState.is_dragging_selected_brush_handle = false;
            g_EditorState.selected_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Resize Brush");
        }
        if (g_EditorState.is_dragging_selected_brush_body) {
            g_EditorState.is_dragging_selected_brush_body = false;
            Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Move Selection");
        }
        if (g_EditorState.is_dragging_preview_brush_handle) {
            g_EditorState.is_dragging_preview_brush_handle = false;
            g_EditorState.preview_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
        }
        else if (g_EditorState.is_dragging_preview_brush_body) {
            Vec3 current_mouse_world_unprojected = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[g_EditorState.preview_brush_drag_body_view], g_EditorState.preview_brush_drag_body_view);
            Vec3 delta = Math::vec3_sub(current_mouse_world_unprojected, g_EditorState.preview_brush_drag_body_start_mouse_world);

            Vec3 current_brush_min_before_move = g_EditorState.preview_brush_world_min;
            Vec3 current_brush_max_before_move = g_EditorState.preview_brush_world_max;
            Vec3 brush_size = Math::vec3_sub(current_brush_max_before_move, current_brush_min_before_move);

            Vec3 new_world_min = Math::vec3_add(g_EditorState.preview_brush_drag_body_start_brush_world_min_at_drag_start, delta);
            Vec3 new_world_max;

            if (g_EditorState.snap_to_grid) {
                ViewportType view = g_EditorState.preview_brush_drag_body_view;
                Vec3 original_min_at_drag_start_for_fixed_axes = g_EditorState.preview_brush_drag_body_start_brush_world_min_at_drag_start;

                if (view == VIEW_TOP_XZ) {
                    new_world_min.x = SnapValue(new_world_min.x, g_EditorState.grid_size);
                    new_world_min.z = SnapValue(new_world_min.z, g_EditorState.grid_size);
                    new_world_min.y = original_min_at_drag_start_for_fixed_axes.y;
                }
                else if (view == VIEW_FRONT_XY) {
                    new_world_min.x = SnapValue(new_world_min.x, g_EditorState.grid_size);
                    new_world_min.y = SnapValue(new_world_min.y, g_EditorState.grid_size);
                    new_world_min.z = original_min_at_drag_start_for_fixed_axes.z;
                }
                else if (view == VIEW_SIDE_YZ) {
                    new_world_min.y = SnapValue(new_world_min.y, g_EditorState.grid_size);
                    new_world_min.z = SnapValue(new_world_min.z, g_EditorState.grid_size);
                    new_world_min.x = original_min_at_drag_start_for_fixed_axes.x;
                }
            }
            new_world_max = Math::vec3_add(new_world_min, brush_size);

            g_EditorState.preview_brush_world_min = new_world_min;
            g_EditorState.preview_brush_world_max = new_world_max;

            Editor_UpdatePreviewBrushFromWorldMinMax();
        }
        if (g_EditorState.is_manipulating_gizmo) {
            Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Transform Selection");
            g_EditorState.is_manipulating_gizmo = false; g_EditorState.gizmo_active_axis = GIZMO_AXIS_NONE;
        }
        if (g_EditorState.is_dragging_for_creation) {
            g_EditorState.is_dragging_for_creation = false;
            Vec3 current_point = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.brush_creation_view], (ViewportType)g_EditorState.brush_creation_view);

            Editor_UpdatePreviewBrushForInitialDrag(g_EditorState.brush_creation_start_point_2d_drag, current_point, g_EditorState.brush_creation_view);
            g_EditorState.is_in_brush_creation_mode = true;
        }
    }
    if (event->type == SDL_MOUSEMOTION) {
        ViewportType active_viewport = VIEW_COUNT;
        for (Int i = 0; i < VIEW_COUNT; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                active_viewport = (ViewportType)i;
                break;
            }
        }
        if (g_EditorState.is_painting) {
            Brush* b = &scene->brushes[primary->index];
            Bool needs_update = false;

            for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    Vec3 mouse_world_pos = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                    Float radius_sq = g_EditorState.paint_brush_radius * g_EditorState.paint_brush_radius;

                    for (Int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                        Float dist_sq = 0;
                        if (i == VIEW_TOP_XZ) dist_sq = (vert_world_pos.x - mouse_world_pos.x) * (vert_world_pos.x - mouse_world_pos.x) + (vert_world_pos.z - mouse_world_pos.z) * (vert_world_pos.z - mouse_world_pos.z);
                        if (i == VIEW_FRONT_XY) dist_sq = (vert_world_pos.x - mouse_world_pos.x) * (vert_world_pos.x - mouse_world_pos.x) + (vert_world_pos.y - mouse_world_pos.y) * (vert_world_pos.y - mouse_world_pos.y);
                        if (i == VIEW_SIDE_YZ) dist_sq = (vert_world_pos.z - mouse_world_pos.z) * (vert_world_pos.z - mouse_world_pos.z) + (vert_world_pos.y - mouse_world_pos.y) * (vert_world_pos.y - mouse_world_pos.y);

                        if (dist_sq < radius_sq) {
                            Float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.paint_brush_radius;
                            Float blend_amount = g_EditorState.paint_brush_strength * falloff * engine->deltaTime * 10.0f;
                            Float* channel_to_paint = nullptr;
                            if (g_EditorState.paint_channel == 0) channel_to_paint = &b->vertices[v_idx].color.x;
                            else if (g_EditorState.paint_channel == 1) channel_to_paint = &b->vertices[v_idx].color.y;
                            else if (g_EditorState.paint_channel == 2) channel_to_paint = &b->vertices[v_idx].color.z;

                            if (channel_to_paint) {
                                if (SDL_GetModState() & KMOD_SHIFT) {
                                    *channel_to_paint -= blend_amount;
                                }
                                else {
                                    *channel_to_paint += blend_amount;
                                }
                                *channel_to_paint = fmaxf(0.0f, fminf(1.0f, *channel_to_paint));
                                needs_update = true;
                            }
                        }
                    }
                }
            }
            if (needs_update) {
                Brush_CreateRenderData(b);
            }
        }
        if (g_EditorState.is_sculpting) {
            Brush* b = &scene->brushes[primary->index];
            Bool needs_update = false;

            if (SDL_GetModState() & KMOD_SHIFT) {
                Vec3* average_positions = new Vec3[b->numVertices]();
                if (average_positions) {
                    Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
                    Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                    for (Int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        local_min.x = fminf(local_min.x, b->vertices[v_idx].pos.x);
                        local_min.y = fminf(local_min.y, b->vertices[v_idx].pos.y);
                        local_min.z = fminf(local_min.z, b->vertices[v_idx].pos.z);
                        local_max.x = fmaxf(local_max.x, b->vertices[v_idx].pos.x);
                        local_max.y = fmaxf(local_max.y, b->vertices[v_idx].pos.y);
                        local_max.z = fmaxf(local_max.z, b->vertices[v_idx].pos.z);
                    }

                    for (Int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                        Float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
                        Float dist_sq_from_brush = Math::vec3_length_sq(Math::vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                        if (dist_sq_from_brush < radius_sq) {
                            Vec3 neighbor_sum = { 0,0,0 };
                            Int neighbor_count = 0;
                            for (Int n_idx = 0; n_idx < b->numVertices; ++n_idx) {
                                if (v_idx == n_idx) continue;
                                Float dist_sq_verts = Math::vec3_length_sq(Math::vec3_sub(b->vertices[v_idx].pos, b->vertices[n_idx].pos));
                                if (dist_sq_verts < (g_EditorState.grid_size * g_EditorState.grid_size * 2.0f)) {
                                    neighbor_sum = Math::vec3_add(neighbor_sum, b->vertices[n_idx].pos);
                                    neighbor_count++;
                                }
                            }
                            if (neighbor_count > 0) average_positions[v_idx] = Math::vec3_muls(neighbor_sum, 1.0f / neighbor_count);
                            else average_positions[v_idx] = b->vertices[v_idx].pos;
                        }
                        else {
                            average_positions[v_idx] = b->vertices[v_idx].pos;
                        }
                    }

                    for (Int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                        Float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
                        Float dist_sq_from_brush = Math::vec3_length_sq(Math::vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                        if (dist_sq_from_brush < radius_sq) {
                            Float falloff = 1.0f - sqrtf(dist_sq_from_brush) / g_EditorState.sculpt_brush_radius;
                            Float smooth_strength = g_EditorState.sculpt_brush_strength * falloff * engine->unscaledDeltaTime * 1.5f;

                            Vec3 new_pos = Math::vec3_add(Math::vec3_muls(b->vertices[v_idx].pos, 1.0f - smooth_strength), Math::vec3_muls(average_positions[v_idx], smooth_strength));

                            new_pos.x = fmaxf(local_min.x, fminf(local_max.x, new_pos.x));
                            new_pos.y = fmaxf(local_min.y, fminf(local_max.y, new_pos.y));
                            new_pos.z = fmaxf(local_min.z, fminf(local_max.z, new_pos.z));

                            b->vertices[v_idx].pos = new_pos;
                            needs_update = true;
                        }
                    }
                    delete[] average_positions;
                }
            }
            else {
                Float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
                for (Int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                    Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                    Float dist_sq = Math::vec3_length_sq(Math::vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                    if (dist_sq < radius_sq) {
                        Float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.sculpt_brush_radius;
                        Float sculpt_amount = g_EditorState.sculpt_brush_strength * falloff * engine->unscaledDeltaTime * 10.0f;
                        if (SDL_GetModState() & KMOD_CTRL) sculpt_amount = -sculpt_amount;

                        b->vertices[v_idx].pos = Math::vec3_add(b->vertices[v_idx].pos, Math::vec3_muls(g_EditorState.paint_brush_world_normal, sculpt_amount));
                        needs_update = true;
                    }
                }
            }

            if (needs_update) {
                Brush_CreateRenderData(b);
                if (b->physicsBody) {
                    Physics::RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                    if (Brush_IsSolid(b) && b->numVertices > 0) {
                        Vec3* world_verts = new Vec3[b->numVertices];
                        for (Int k = 0; k < b->numVertices; ++k)
                            world_verts[k] = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[k].pos);
                        b->physicsBody = Physics::CreateStaticConvexHull(engine->physicsWorld, reinterpret_cast<const Float*>(world_verts), b->numVertices);
                        delete[] world_verts;
                    }
                    else {
                        b->physicsBody = nullptr;
                    }
                }
            }
        }
        if (g_EditorState.is_dragging_preview_brush_handle) {
            Editor_AdjustPreviewBrushByHandle(g_EditorState.mouse_pos_in_viewport[g_EditorState.preview_brush_drag_handle_view], g_EditorState.preview_brush_drag_handle_view);
        }
        else if (g_EditorState.is_dragging_selected_brush_handle) {
            Editor_AdjustSelectedBrushByHandle(scene, engine, g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
        }
        else if (g_EditorState.is_dragging_selected_brush_body) {
            Vec3 current_mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[g_EditorState.selected_brush_drag_body_view], g_EditorState.selected_brush_drag_body_view);

            if (g_EditorState.snap_to_grid) {
                current_mouse_world.x = SnapValue(current_mouse_world.x, g_EditorState.grid_size);
                current_mouse_world.y = SnapValue(current_mouse_world.y, g_EditorState.grid_size);
                current_mouse_world.z = SnapValue(current_mouse_world.z, g_EditorState.grid_size);
            }

            Vec3 delta = Math::vec3_sub(current_mouse_world, g_EditorState.selected_brush_drag_body_start_mouse_world);

            for (Int i = 0; i < g_EditorState.num_selections; ++i) {
                EditorSelection* sel = &g_EditorState.selections[i];
                Vec3 start_pos = g_EditorState.gizmo_drag_start_positions[i];
                Vec3 new_pos = Math::vec3_add(start_pos, delta);

                if (sel->type == ENTITY_BRUSH) {
                    Brush* b = &scene->brushes[sel->index];
                    Vec3 old_pos = b->pos;
                    b->pos = new_pos;

                    if (g_EditorState.texture_lock_enabled) {
                        Vec3 frame_move = Math::vec3_sub(new_pos, old_pos);
                        Float du = 0, dv = 0;
                        if (g_EditorState.selected_brush_drag_body_view == VIEW_TOP_XZ) { du = frame_move.x; dv = frame_move.z; }
                        else if (g_EditorState.selected_brush_drag_body_view == VIEW_FRONT_XY) { du = frame_move.x; dv = frame_move.y; }
                        else { du = frame_move.z; dv = frame_move.y; }

                        for (Int f = 0; f < b->numFaces; ++f) {
                            if (b->faces[f].uv_scale.x != 0) b->faces[f].uv_offset.x -= du / b->faces[f].uv_scale.x;
                            if (b->faces[f].uv_scale.y != 0) b->faces[f].uv_offset.y -= dv / b->faces[f].uv_scale.y;
                        }
                        Brush_CreateRenderData(b);
                    }
                    Brush_UpdateMatrix(b);
                    if (b->physicsBody) Physics::SetWorldTransform(b->physicsBody, b->modelMatrix);
                }
                else if (sel->type == ENTITY_MODEL) {
                    SceneObject* obj = &scene->objects[sel->index];
                    obj->pos = new_pos;
                    SceneObject_UpdateMatrix(obj);
                    if (obj->physicsBody) Physics::SetWorldTransform(obj->physicsBody, obj->modelMatrix);
                }
                else if (sel->type == ENTITY_LIGHT) scene->lights[sel->index].pos = new_pos;
                else if (sel->type == ENTITY_DECAL) { scene->decals[sel->index].pos = new_pos; Decal_UpdateMatrix(&scene->decals[sel->index]); }
                else if (sel->type == ENTITY_SOUND) { scene->soundEntities[sel->index].pos = new_pos; Sound::SoundSystem_SetSourcePosition(scene->soundEntities[sel->index].sourceID, new_pos); }
                else if (sel->type == ENTITY_PARTICLE_EMITTER) scene->particleEmitters[sel->index].pos = new_pos;
                else if (sel->type == ENTITY_SPRITE) scene->sprites[sel->index].pos = new_pos;
                else if (sel->type == ENTITY_VIDEO_PLAYER) scene->videoPlayers[sel->index].pos = new_pos;
                else if (sel->type == ENTITY_PARALLAX_ROOM) { scene->parallaxRooms[sel->index].pos = new_pos; ParallaxRoom_UpdateMatrix(&scene->parallaxRooms[sel->index]); }
                else if (sel->type == ENTITY_LOGIC) scene->logicEntities[sel->index].pos = new_pos;
                else if (sel->type == ENTITY_PLAYERSTART) scene->playerStart.pos = new_pos;
            }
        }
        else if (g_EditorState.is_manipulating_vertex_gizmo) {
            Brush* b = &scene->brushes[primary->index];
            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
            Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
            Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
            Mat4 inv_proj, inv_view; Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
            Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
            Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; Math::vec3_normalize(&ray_dir);
            Vec3 current_intersect_point;
            if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.vertex_gizmo_drag_plane_normal, g_EditorState.vertex_gizmo_drag_plane_d, &current_intersect_point)) {
                Vec3 delta = Math::vec3_sub(current_intersect_point, g_EditorState.vertex_gizmo_drag_start_world);
                Vec3 axis_dir = { 0 };
                if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
                if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
                if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
                Float projection_len = Math::vec3_dot(delta, axis_dir);
                Vec3 projected_delta = Math::vec3_muls(axis_dir, projection_len);
                Vec3 new_world_pos = Math::vec3_add(g_EditorState.vertex_drag_start_pos_world, projected_delta);
                if (g_EditorState.snap_to_grid) { new_world_pos.x = SnapValue(new_world_pos.x, g_EditorState.grid_size); new_world_pos.y = SnapValue(new_world_pos.y, g_EditorState.grid_size); new_world_pos.z = SnapValue(new_world_pos.z, g_EditorState.grid_size); }
                Mat4 inv_model;
                Math::mat4_inverse(&b->modelMatrix, &inv_model);
                b->vertices[primary->vertex_index].pos = Math::mat4_mul_vec3(&inv_model, new_world_pos);
                Brush_CreateRenderData(b);
                if (b->physicsBody) {
                    Physics::RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                    if (Brush_IsSolid(b) && b->numVertices > 0) {
                        Vec3* world_verts = new Vec3[b->numVertices];
                        for (Int i = 0; i < b->numVertices; ++i)
                            world_verts[i] = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                        b->physicsBody = Physics::CreateStaticConvexHull(engine->physicsWorld, reinterpret_cast<const Float*>(world_verts), b->numVertices);
                        delete[] world_verts;
                    }
                    else {
                        b->physicsBody = nullptr;
                    }
                }
            }
        }
        else if (g_EditorState.is_manipulating_gizmo && primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
            Brush* b = &scene->brushes[primary->index];
            BrushFace* face = &b->faces[primary->face_index];
            if (face->numVertexIndices < 3) return;

            Vec3 delta = { 0 };
            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[g_EditorState.gizmo_drag_view];

            if (g_EditorState.gizmo_drag_view == VIEW_PERSPECTIVE) {
                Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; Math::vec3_normalize(&ray_dir);
                Vec3 current_intersect_point;
                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &current_intersect_point)) {
                    delta = Math::vec3_sub(current_intersect_point, g_EditorState.gizmo_drag_start_world);
                }
            }
            else {
                Vec3 current_point = ScreenToWorld(screen_pos, g_EditorState.gizmo_drag_view);
                delta = Math::vec3_sub(current_point, g_EditorState.gizmo_drag_start_world);
            }

            Vec3 axis_dir = { 0 };
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
            Float projection_len = Math::vec3_dot(delta, axis_dir);
            Vec3 projected_delta = Math::vec3_muls(axis_dir, projection_len);

            if (g_EditorState.snap_to_grid) {
                projected_delta.x = SnapValue(projected_delta.x, g_EditorState.grid_size);
                projected_delta.y = SnapValue(projected_delta.y, g_EditorState.grid_size);
                projected_delta.z = SnapValue(projected_delta.z, g_EditorState.grid_size);
            }

            Mat4 inv_model; Math::mat4_inverse(&b->modelMatrix, &inv_model);
            for (Int i = 0; i < face->numVertexIndices; ++i) {
                Int vert_idx = face->vertexIndices[i];
                Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[vert_idx].pos);
                Vec3 new_world_pos = Math::vec3_add(vert_world_pos, projected_delta);
                b->vertices[vert_idx].pos = Math::mat4_mul_vec3(&inv_model, new_world_pos);
            }

            Brush_CreateRenderData(b);
            if (b->physicsBody) {
                Physics::RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                if (Brush_IsSolid(b) && b->numVertices > 0) {
                    Vec3* world_verts = new Vec3[b->numVertices];
                    for (Int j = 0; j < b->numVertices; ++j)
                        world_verts[j] = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
                    b->physicsBody = Physics::CreateStaticConvexHull(engine->physicsWorld, reinterpret_cast<const Float*>(world_verts), b->numVertices);
                    delete[] world_verts;
                }
                else {
                    b->physicsBody = nullptr;
                }
            }

            g_EditorState.gizmo_drag_start_world = Math::vec3_add(g_EditorState.gizmo_drag_start_world, projected_delta);

            return;
        }
        else if (g_EditorState.is_vertex_manipulating) {
            Brush* b = &scene->brushes[primary->index];
            Vec3 current_mouse_world = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.vertex_manipulation_view], g_EditorState.vertex_manipulation_view);
            Vec3* vert_local_pos = &b->vertices[g_EditorState.manipulated_vertex_index].pos;
            Mat4 inv_model;
            Math::mat4_inverse(&b->modelMatrix, &inv_model);
            Vec3 vert_world = Math::mat4_mul_vec3(&b->modelMatrix, *vert_local_pos);
            if (g_EditorState.vertex_manipulation_view == VIEW_TOP_XZ) { vert_world.x = current_mouse_world.x; vert_world.z = current_mouse_world.z; }
            if (g_EditorState.vertex_manipulation_view == VIEW_FRONT_XY) { vert_world.x = current_mouse_world.x; vert_world.y = current_mouse_world.y; }
            if (g_EditorState.vertex_manipulation_view == VIEW_SIDE_YZ) { vert_world.y = current_mouse_world.y; vert_world.z = current_mouse_world.z; }
            *vert_local_pos = Math::mat4_mul_vec3(&inv_model, vert_world);
            Brush_CreateRenderData(b);
            if (b->physicsBody) {
                Physics::RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                if (Brush_IsSolid(b) && b->numVertices > 0) {
                    Vec3* world_verts = new Vec3[b->numVertices];
                    for (Int i = 0; i < b->numVertices; ++i)
                        world_verts[i] = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                    b->physicsBody = Physics::CreateStaticConvexHull(engine->physicsWorld, reinterpret_cast<const Float*>(world_verts), b->numVertices);
                    delete[] world_verts;
                }
                else {
                    b->physicsBody = nullptr;
                }
            }
            return;
        }
        else if (g_EditorState.is_manipulating_gizmo && primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
            Brush* b = &scene->brushes[primary->index];
            BrushFace* face = &b->faces[primary->face_index];
            if (face->numVertexIndices < 3) return;

            Vec3 delta = { 0 };
            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[g_EditorState.gizmo_drag_view];

            if (g_EditorState.gizmo_drag_view == VIEW_PERSPECTIVE) {
                Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; Math::vec3_normalize(&ray_dir);
                Vec3 current_intersect_point;
                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &current_intersect_point)) {
                    delta = Math::vec3_sub(current_intersect_point, g_EditorState.gizmo_drag_start_world);
                }
            }
            else {
                Vec3 current_point = ScreenToWorld(screen_pos, g_EditorState.gizmo_drag_view);
                delta = Math::vec3_sub(current_point, g_EditorState.gizmo_drag_start_world);
            }

            Vec3 axis_dir = { 0 };
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
            Float projection_len = Math::vec3_dot(delta, axis_dir);
            Vec3 projected_delta = Math::vec3_muls(axis_dir, projection_len);

            if (g_EditorState.snap_to_grid) {
                projected_delta.x = SnapValue(projected_delta.x, g_EditorState.grid_size);
                projected_delta.y = SnapValue(projected_delta.y, g_EditorState.grid_size);
                projected_delta.z = SnapValue(projected_delta.z, g_EditorState.grid_size);
            }

            Mat4 inv_model; Math::mat4_inverse(&b->modelMatrix, &inv_model);
            for (Int i = 0; i < face->numVertexIndices; ++i) {
                Int vert_idx = face->vertexIndices[i];
                Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[vert_idx].pos);
                Vec3 new_world_pos = Math::vec3_add(vert_world_pos, projected_delta);
                b->vertices[vert_idx].pos = Math::mat4_mul_vec3(&inv_model, new_world_pos);
            }

            Brush_CreateRenderData(b);
            if (b->physicsBody) {
                Physics::RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                if (Brush_IsSolid(b) && b->numVertices > 0) {
                    Vec3* world_verts = new Vec3[b->numVertices];
                    for (Int j = 0; j < b->numVertices; ++j)
                        world_verts[j] = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
                    b->physicsBody = Physics::CreateStaticConvexHull(engine->physicsWorld, reinterpret_cast<const Float*>(world_verts), b->numVertices);
                    delete[] world_verts;
                }
                else {
                    b->physicsBody = nullptr;
                }
            }

            g_EditorState.gizmo_drag_start_world = Math::vec3_add(g_EditorState.gizmo_drag_start_world, projected_delta);

            return;
        }
        else if (g_EditorState.is_manipulating_gizmo) {
            if ((SDL_GetModState() & KMOD_SHIFT) && !g_EditorState.gizmo_drag_has_cloned) {
                g_EditorState.gizmo_drag_has_cloned = true;

                Int num_original_selections = g_EditorState.num_selections;
                EditorSelection* original_selections = new EditorSelection[num_original_selections];
                if (original_selections) {
                    memcpy(original_selections, g_EditorState.selections, num_original_selections * sizeof(EditorSelection));
                    Editor_ClearSelection();

                    for (Int i = 0; i < num_original_selections; ++i) {
                        EditorSelection* sel = &original_selections[i];
                        switch (sel->type) {
                        case ENTITY_MODEL: Editor_DuplicateModel(scene, engine, sel->index); break;
                        case ENTITY_BRUSH: Editor_DuplicateBrush(scene, engine, sel->index); break;
                        case ENTITY_LIGHT: Editor_DuplicateLight(scene, sel->index); break;
                        case ENTITY_DECAL: Editor_DuplicateDecal(scene, sel->index); break;
                        case ENTITY_SOUND: Editor_DuplicateSoundEntity(scene, sel->index); break;
                        case ENTITY_PARTICLE_EMITTER: Editor_DuplicateParticleEmitter(scene, sel->index); break;
                        case ENTITY_VIDEO_PLAYER: Editor_DuplicateVideoPlayer(scene, sel->index); break;
                        case ENTITY_PARALLAX_ROOM: Editor_DuplicateParallaxRoom(scene, sel->index); break;
                        case ENTITY_LOGIC: Editor_DuplicateLogicEntity(scene, engine, sel->index); break;
                        case ENTITY_SPRITE: Editor_DuplicateSprite(scene, sel->index); break;
                        case ENTITY_PLAYERSTART: break;
                        default: UNREACHABLE(); break;
                        }
                    }
                    delete[] original_selections;

                    delete[] g_EditorState.gizmo_drag_start_positions;
                    delete[] g_EditorState.gizmo_drag_start_rotations;
                    delete[] g_EditorState.gizmo_drag_start_scales;
                    g_EditorState.gizmo_drag_start_positions = new Vec3[g_EditorState.num_selections];
                    g_EditorState.gizmo_drag_start_rotations = new Vec3[g_EditorState.num_selections];
                    g_EditorState.gizmo_drag_start_scales = new Vec3[g_EditorState.num_selections];

                    for (Int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        switch (sel->type) {
                        case ENTITY_MODEL: g_EditorState.gizmo_drag_start_positions[i] = scene->objects[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->objects[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = scene->objects[sel->index].scale; break;
                        case ENTITY_BRUSH: g_EditorState.gizmo_drag_start_positions[i] = scene->brushes[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->brushes[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = scene->brushes[sel->index].scale; break;
                        case ENTITY_LIGHT: g_EditorState.gizmo_drag_start_positions[i] = scene->lights[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->lights[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = Vec3{ 1,1,1 }; break;
                        case ENTITY_DECAL: g_EditorState.gizmo_drag_start_positions[i] = scene->decals[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->decals[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = scene->decals[sel->index].size; break;
                        case ENTITY_SOUND: g_EditorState.gizmo_drag_start_positions[i] = scene->soundEntities[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = Vec3{ 0,0,0 }; g_EditorState.gizmo_drag_start_scales[i] = Vec3{ 1,1,1 }; break;
                        case ENTITY_PARTICLE_EMITTER: g_EditorState.gizmo_drag_start_positions[i] = scene->particleEmitters[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = Vec3{ 0,0,0 }; g_EditorState.gizmo_drag_start_scales[i] = Vec3{ 1,1,1 }; break;
                        case ENTITY_SPRITE: g_EditorState.gizmo_drag_start_positions[i] = scene->sprites[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = Vec3{ 0,0,0 }; g_EditorState.gizmo_drag_start_scales[i] = Vec3{ scene->sprites[sel->index].scale,1,1 }; break;
                        case ENTITY_VIDEO_PLAYER: g_EditorState.gizmo_drag_start_positions[i] = scene->videoPlayers[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->videoPlayers[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = Vec3{ scene->videoPlayers[sel->index].size.x, scene->videoPlayers[sel->index].size.y, 1.0f }; break;
                        case ENTITY_PARALLAX_ROOM: g_EditorState.gizmo_drag_start_positions[i] = scene->parallaxRooms[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->parallaxRooms[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = Vec3{ scene->parallaxRooms[sel->index].size.x, scene->parallaxRooms[sel->index].size.y, 1.0f }; break;
                        case ENTITY_LOGIC: g_EditorState.gizmo_drag_start_positions[i] = scene->logicEntities[sel->index].pos; g_EditorState.gizmo_drag_start_rotations[i] = scene->logicEntities[sel->index].rot; g_EditorState.gizmo_drag_start_scales[i] = Vec3{ 1,1,1 }; break;
                        case ENTITY_PLAYERSTART: g_EditorState.gizmo_drag_start_positions[i] = scene->playerStart.pos; g_EditorState.gizmo_drag_start_rotations[i] = Vec3{ 0,0,0 }; g_EditorState.gizmo_drag_start_scales[i] = Vec3{ 1,1,1 }; break;
                        default: UNREACHABLE();  break;
                        }
                    }
                }
            }
            Vec3 pos_delta = { 0 };
            Vec3 scale_delta = { 0 };
            Float rot_angle_delta = 0.0f;
            Mat4 delta_rot_matrix;
            Math::mat4_identity(&delta_rot_matrix);

            Vec3 current_intersect_point;
            Bool intersection_found = false;

            if (g_EditorState.gizmo_drag_view == VIEW_PERSPECTIVE) {
                Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; Math::vec3_normalize(&ray_dir);

                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &current_intersect_point)) {
                    intersection_found = true;
                }
            }
            else {
                current_intersect_point = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.gizmo_drag_view], g_EditorState.gizmo_drag_view);
                intersection_found = true;
            }

            if (intersection_found) {
                if (g_EditorState.current_gizmo_operation == GIZMO_OP_ROTATE) {
                    Vec3 object_pos_for_rotate = g_EditorState.gizmo_selection_centroid;
                    Vec3 current_vec = Math::vec3_sub(current_intersect_point, object_pos_for_rotate);
                    Math::vec3_normalize(&current_vec);
                    Vec3 u_axis = g_EditorState.gizmo_rotation_start_vec;
                    Vec3 v_axis = Math::vec3_cross(g_EditorState.gizmo_drag_plane_normal, u_axis);

                    Float u_coord = Math::vec3_dot(current_vec, u_axis);
                    Float v_coord = Math::vec3_dot(current_vec, v_axis);

                    Float angle = atan2f(v_coord, u_coord) * (180.0f / Common::PI);

                    if (SDL_GetModState() & KMOD_CTRL) {
                        angle = SnapValue(angle, 15.0f);
                    }
                    rot_angle_delta = angle;

                    Float angle_rad = rot_angle_delta * (Common::PI / 180.0f);
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) delta_rot_matrix = Math::mat4_rotate_x(angle_rad);
                    else if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) delta_rot_matrix = Math::mat4_rotate_y(angle_rad);
                    else delta_rot_matrix = Math::mat4_rotate_z(angle_rad);
                }
                else {
                    Vec3 delta = Math::vec3_sub(current_intersect_point, g_EditorState.gizmo_drag_start_world);

                    if (g_EditorState.gizmo_drag_view == VIEW_PERSPECTIVE) {
                        Vec3 axis_dir = { 0 };
                        if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
                        if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
                        if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
                        Float projection_len = Math::vec3_dot(delta, axis_dir);

                        if (g_EditorState.current_gizmo_operation == GIZMO_OP_TRANSLATE) {
                            if (g_EditorState.snap_to_grid) projection_len = SnapValue(projection_len, g_EditorState.grid_size);
                            pos_delta = Math::vec3_muls(axis_dir, projection_len);
                        }
                        else {
                            if (g_EditorState.snap_to_grid) projection_len = SnapValue(projection_len, 0.25f);
                            scale_delta = Math::vec3_muls(axis_dir, projection_len);
                        }
                    }
                    else {
                        pos_delta = delta;
                        scale_delta = delta;

                        switch (g_EditorState.gizmo_drag_view) {
                        case VIEW_TOP_XZ:   pos_delta.y = 0; scale_delta.y = 0; break;
                        case VIEW_FRONT_XY: pos_delta.z = 0; scale_delta.z = 0; break;
                        case VIEW_SIDE_YZ:  pos_delta.x = 0; scale_delta.x = 0; break;
                        default: UNREACHABLE();  break;
                        }

                        if (g_EditorState.snap_to_grid) {
                            if (g_EditorState.current_gizmo_operation == GIZMO_OP_TRANSLATE) {
                                pos_delta.x = SnapValue(pos_delta.x, g_EditorState.grid_size);
                                pos_delta.y = SnapValue(pos_delta.y, g_EditorState.grid_size);
                                pos_delta.z = SnapValue(pos_delta.z, g_EditorState.grid_size);
                            }
                            else {
                                scale_delta.x = SnapValue(scale_delta.x, 0.25f);
                                scale_delta.y = SnapValue(scale_delta.y, 0.25f);
                                scale_delta.z = SnapValue(scale_delta.z, 0.25f);
                            }
                        }
                    }
                }
            }

            Vec3 centroid = g_EditorState.gizmo_selection_centroid;
            for (Int i = 0; i < g_EditorState.num_selections; ++i) {
                EditorSelection* sel = &g_EditorState.selections[i];

                Vec3 start_pos = g_EditorState.gizmo_drag_start_positions[i];
                Vec3 start_rot_eulers = g_EditorState.gizmo_drag_start_rotations[i];
                Vec3 start_scale = g_EditorState.gizmo_drag_start_scales[i];

                Vec3 new_pos = start_pos;
                Vec3 new_rot = start_rot_eulers;
                Vec3 new_scale = start_scale;

                if (g_EditorState.current_gizmo_operation == GIZMO_OP_TRANSLATE) {
                    new_pos = Math::vec3_add(start_pos, pos_delta);
                }
                else if (g_EditorState.current_gizmo_operation == GIZMO_OP_SCALE) {
                    new_scale = Math::vec3_add(start_scale, scale_delta);
                }
                else if (g_EditorState.current_gizmo_operation == GIZMO_OP_ROTATE) {
                    new_rot = start_rot_eulers;

                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) {
                        new_rot.x += rot_angle_delta;
                    }
                    else if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) {
                        new_rot.y += rot_angle_delta;
                    }
                    else if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) {
                        new_rot.z += rot_angle_delta;
                    }
                    Vec3 relative_pos = Math::vec3_sub(start_pos, centroid);
                    Vec3 rotated_relative_pos = Math::mat4_mul_vec3_dir(&delta_rot_matrix, relative_pos);
                    new_pos = Math::vec3_add(centroid, rotated_relative_pos);
                }

                switch (sel->type) {
                case ENTITY_MODEL: {
                    SceneObject* obj = &scene->objects[sel->index];
                    obj->pos = new_pos; obj->rot = new_rot; obj->scale = new_scale;
                    SceneObject_UpdateMatrix(obj); if (obj->physicsBody) Physics::SetWorldTransform(obj->physicsBody, obj->modelMatrix); break;
                }
                case ENTITY_BRUSH: {
                    Brush* b = &scene->brushes[sel->index];
                    b->pos = new_pos; b->rot = new_rot; b->scale = new_scale;
                    Brush_UpdateMatrix(b); if (b->physicsBody) Physics::SetWorldTransform(b->physicsBody, b->modelMatrix); break;
                }
                case ENTITY_LIGHT: {
                    Light* l = &scene->lights[sel->index];
                    l->pos = new_pos; l->rot = new_rot; break;
                }
                case ENTITY_DECAL: {
                    Decal* d = &scene->decals[sel->index];
                    d->pos = new_pos; d->rot = new_rot; d->size = new_scale;
                    Decal_UpdateMatrix(d); break;
                }
                case ENTITY_SOUND: {
                    SoundEntity* s = &scene->soundEntities[sel->index];
                    s->pos = new_pos; Sound::SoundSystem_SetSourcePosition(s->sourceID, s->pos); break;
                }
                case ENTITY_PARTICLE_EMITTER: {
                    ParticleEmitter* p = &scene->particleEmitters[sel->index];
                    p->pos = new_pos; break;
                }
                case ENTITY_SPRITE: {
                    Sprite* s = &scene->sprites[sel->index];
                    s->pos = new_pos; s->scale = new_scale.x; break;
                }
                case ENTITY_VIDEO_PLAYER: {
                    VideoPlayer* vp = &scene->videoPlayers[sel->index];
                    vp->pos = new_pos; vp->rot = new_rot; vp->size.x = new_scale.x; vp->size.y = new_scale.y; break;
                }
                case ENTITY_PARALLAX_ROOM: {
                    ParallaxRoom* p = &scene->parallaxRooms[sel->index];
                    p->pos = new_pos; p->rot = new_rot; p->size.x = new_scale.x; p->size.y = new_scale.y;
                    ParallaxRoom_UpdateMatrix(p); break;
                }
                case ENTITY_LOGIC: {
                    LogicEntity* l = &scene->logicEntities[sel->index];
                    l->pos = new_pos; l->rot = new_rot; break;
                }
                case ENTITY_PLAYERSTART: {
                    scene->playerStart.pos = new_pos; break;
                }
                default: UNREACHABLE();  break;
                }
            }
        }
        else if (g_EditorState.is_dragging_for_creation) {
            BrushCreationShapeType original_shape = g_EditorState.current_brush_shape;
            if (original_shape == BRUSH_SHAPE_ARCH) {
                g_EditorState.current_brush_shape = BRUSH_SHAPE_BLOCK;
            }
            Vec3 current_point = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.brush_creation_view], (ViewportType)g_EditorState.brush_creation_view);
            Editor_UpdatePreviewBrushForInitialDrag(g_EditorState.brush_creation_start_point_2d_drag, current_point, g_EditorState.brush_creation_view);
            if (original_shape == BRUSH_SHAPE_ARCH) {
                g_EditorState.current_brush_shape = original_shape;
            }
        }
        else if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
            for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    for (Int j = 0; j < VIEW_COUNT; ++j) {
                        g_EditorState.is_viewport_focused[j] = (j == i);
                    }
                    break;
                }
            }

            if (g_EditorState.is_viewport_focused[VIEW_TOP_XZ]) { Float ms = g_EditorState.ortho_cam_zoom[0] * 0.002f; g_EditorState.ortho_cam_pos[0].x -= event->motion.xrel * ms; g_EditorState.ortho_cam_pos[0].z -= event->motion.yrel * ms; }
            if (g_EditorState.is_viewport_focused[VIEW_FRONT_XY]) { Float ms = g_EditorState.ortho_cam_zoom[1] * 0.002f; g_EditorState.ortho_cam_pos[1].x -= event->motion.xrel * ms; g_EditorState.ortho_cam_pos[1].y += event->motion.yrel * ms; }
            if (g_EditorState.is_viewport_focused[VIEW_SIDE_YZ]) { Float ms = g_EditorState.ortho_cam_zoom[2] * 0.002f; g_EditorState.ortho_cam_pos[2].z += event->motion.xrel * ms; g_EditorState.ortho_cam_pos[2].y += event->motion.yrel * ms; }
        }
    }
    if (event->type == SDL_MOUSEWHEEL) {
        if (g_EditorState.is_in_z_mode) {
            if (event->wheel.y > 0) {
                g_EditorState.editor_camera_speed *= 1.25f;
            }
            else if (event->wheel.y < 0) {
                g_EditorState.editor_camera_speed /= 1.25f;
            }
            if (g_EditorState.editor_camera_speed < 0.1f) g_EditorState.editor_camera_speed = 0.1f;
            if (g_EditorState.editor_camera_speed > 500.0f) g_EditorState.editor_camera_speed = 500.0f;
            return;
        }
        Bool hovered_any_viewport = false;
        for (Int i = 1; i < VIEW_COUNT; i++) {
            if (g_EditorState.is_viewport_hovered[i]) {
                g_EditorState.ortho_cam_zoom[i - 1] -= event->wheel.y * g_EditorState.ortho_cam_zoom[i - 1] * 0.1f; hovered_any_viewport = true;  if (g_EditorState.ortho_cam_zoom[i - 1] > 64.0f) {
                    g_EditorState.ortho_cam_zoom[i - 1] = 64.0f;
                }
                if (g_EditorState.ortho_cam_zoom[i - 1] < 0.5f) {
                    g_EditorState.ortho_cam_zoom[i - 1] = 0.5f;
                }
            }
        }
    }
    if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        EditorSelection* primary = Editor_GetPrimarySelection();
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_m) {
            if (g_EditorState.num_selections > 0) {
                g_EditorState.show_transform_window = true;
                if (g_EditorState.transform_window_mode == TRANSFORM_MODE_SCALE) {
                    g_EditorState.transform_window_values = Vec3{ 1, 1, 1 };
                }
                else {
                    g_EditorState.transform_window_values = Vec3{ 0, 0, 0 };
                }
            }
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_l) {
            Editor_FlipSelection(scene, engine, 1);
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_i) {
            Editor_FlipSelection(scene, engine, 0);
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_z) { Undo_PerformUndo(scene, engine); return; }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_y) { Undo_PerformRedo(scene, engine); return; }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_s) {
            if (strcmp(g_EditorState.currentMapPath, "untitled.map") == 0) {
                g_EditorState.show_save_map_popup = true;
            }
            else {
                Scene_SaveMap(scene, nullptr, g_EditorState.currentMapPath);
            }
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_g) {
            Editor_GroupSelection();
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_u) {
            Editor_UngroupSelection();
            return;
        }
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            Editor_ClearSelection();
            g_EditorState.is_in_brush_creation_mode = false;
            g_EditorState.is_clipping = false;
            return;
        }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_d) {
            if (g_EditorState.num_selections > 0) {
                Int num_to_duplicate = g_EditorState.num_selections;
                EditorSelection* original_selections = new EditorSelection[num_to_duplicate];
                memcpy(original_selections, g_EditorState.selections, num_to_duplicate * sizeof(EditorSelection));

                Editor_ClearSelection();

                for (Int i = 0; i < num_to_duplicate; ++i) {
                    EditorSelection* sel = &original_selections[i];
                    switch (sel->type) {
                    case ENTITY_MODEL: Editor_DuplicateModel(scene, engine, sel->index); break;
                    case ENTITY_BRUSH: Editor_DuplicateBrush(scene, engine, sel->index); break;
                    case ENTITY_LIGHT: Editor_DuplicateLight(scene, sel->index); break;
                    case ENTITY_DECAL: Editor_DuplicateDecal(scene, sel->index); break;
                    case ENTITY_SOUND: Editor_DuplicateSoundEntity(scene, sel->index); break;
                    case ENTITY_PARTICLE_EMITTER: Editor_DuplicateParticleEmitter(scene, sel->index); break;
                    case ENTITY_VIDEO_PLAYER: Editor_DuplicateVideoPlayer(scene, sel->index); break;
                    case ENTITY_PARALLAX_ROOM: Editor_DuplicateParallaxRoom(scene, sel->index); break;
                    case ENTITY_LOGIC: Editor_DuplicateLogicEntity(scene, engine, sel->index); break;
                    case ENTITY_SPRITE: Editor_DuplicateSprite(scene, sel->index); break;
                    case ENTITY_PLAYERSTART: break;
                    default: UNREACHABLE(); break;
                    }
                }
                delete[] original_selections;
            }
            return;
        }
        if (event->key.keysym.sym == SDLK_z) {
            if (g_EditorState.is_in_z_mode) {
                g_EditorState.is_in_z_mode = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            else {
                for (Int i = 0; i < VIEW_COUNT; ++i) {
                    if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
                        g_EditorState.is_in_z_mode = true;
                        g_EditorState.captured_viewport = (ViewportType)i;
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        break;
                    }
                }
            }
        }
        if (event->key.keysym.sym == SDLK_c && !g_EditorState.is_clipping) {
            if (primary && primary->type == ENTITY_BRUSH) {
                g_EditorState.is_clipping = true;
                g_EditorState.clip_point_count = 0;
                memset(&g_EditorState.clip_side_point, 0, sizeof(Vec3));
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            }
        }
        if (event->key.keysym.sym == SDLK_LEFTBRACKET) {
            g_EditorState.grid_size /= 2.0f;
            if (g_EditorState.grid_size < 0.015625f) g_EditorState.grid_size = 0.015625f;
        }
        if (event->key.keysym.sym == SDLK_RIGHTBRACKET) {
            g_EditorState.grid_size *= 2.0f;
            if (g_EditorState.grid_size > 64.0f) g_EditorState.grid_size = 64.0f;
        }
        if (g_EditorState.is_in_brush_creation_mode) {
            if (event->key.keysym.sym == SDLK_RETURN) {
                if (g_EditorState.current_brush_shape == BRUSH_SHAPE_ARCH) {
                    g_EditorState.arch_creation_start_point = g_EditorState.preview_brush_world_min;
                    g_EditorState.arch_creation_end_point = g_EditorState.preview_brush_world_max;
                    g_EditorState.arch_creation_view = g_EditorState.brush_creation_view;
                    g_EditorState.show_arch_properties_popup = true;
                }
                else {
                    Editor_CreateBrushFromPreview(scene, engine, &g_EditorState.preview_brush);
                    g_EditorState.is_in_brush_creation_mode = false;
                    g_EditorState.is_dragging_for_creation = false;
                    g_EditorState.is_dragging_preview_brush_handle = false;
                    g_EditorState.preview_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
                    g_EditorState.preview_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
                }
            }
        }
        else if (!g_EditorState.is_manipulating_gizmo && !g_EditorState.is_vertex_manipulating && !g_EditorState.is_manipulating_vertex_gizmo) {
            if (event->key.keysym.sym == SDLK_f && primary) {
                Vec3 target_pos = { 0 };
                Float target_size = 1.0f;

                switch (primary->type) {
                case ENTITY_MODEL: {
                    SceneObject* obj = &scene->objects[primary->index];
                    target_pos = obj->pos;
                    Vec3 size_vec = Math::vec3_sub(obj->model->aabb_max, obj->model->aabb_min);
                    target_size = fmaxf(fmaxf(size_vec.x * obj->scale.x, size_vec.y * obj->scale.y), size_vec.z * obj->scale.z);
                    break;
                }
                case ENTITY_BRUSH: {
                    Brush* b = &scene->brushes[primary->index];
                    target_pos = b->pos;
                    if (b->numVertices > 0) {
                        Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
                        Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                        for (Int i = 0; i < b->numVertices; ++i) {
                            local_min.x = fminf(local_min.x, b->vertices[i].pos.x);
                            local_min.y = fminf(local_min.y, b->vertices[i].pos.y);
                            local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
                            local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x);
                            local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y);
                            local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
                        }
                        Vec3 size_vec = Math::vec3_sub(local_max, local_min);
                        target_size = fmaxf(fmaxf(size_vec.x * b->scale.x, size_vec.y * b->scale.y), size_vec.z * b->scale.z);
                    }
                    break;
                }
                case ENTITY_LIGHT:              target_pos = scene->lights[primary->index].pos; break;
                case ENTITY_PLAYERSTART:        target_pos = scene->playerStart.pos; break;
                case ENTITY_DECAL:              target_pos = scene->decals[primary->index].pos; break;
                case ENTITY_SOUND:              target_pos = scene->soundEntities[primary->index].pos; break;
                case ENTITY_PARTICLE_EMITTER:   target_pos = scene->particleEmitters[primary->index].pos; break;
                case ENTITY_VIDEO_PLAYER:       target_pos = scene->videoPlayers[primary->index].pos; break;
                case ENTITY_PARALLAX_ROOM:      target_pos = scene->parallaxRooms[primary->index].pos; break;
                case ENTITY_LOGIC:              target_pos = scene->logicEntities[primary->index].pos; break;
                }

                Vec3 cam_forward = {
                    cosf(g_EditorState.editor_camera.pitch) * sinf(g_EditorState.editor_camera.yaw),
                    sinf(g_EditorState.editor_camera.pitch),
                    -cosf(g_EditorState.editor_camera.pitch) * cosf(g_EditorState.editor_camera.yaw)
                };
                Math::vec3_normalize(&cam_forward);

                Float distance_away = target_size * 2.0f;
                if (distance_away < 2.0f) distance_away = 2.0f;

                Vec3 new_cam_pos = Math::vec3_sub(target_pos, Math::vec3_muls(cam_forward, distance_away));
                g_EditorState.editor_camera.position = new_cam_pos;

                Vec3 new_forward = Math::vec3_sub(target_pos, new_cam_pos);
                Math::vec3_normalize(&new_forward);

                g_EditorState.editor_camera.pitch = asinf(new_forward.y);
                g_EditorState.editor_camera.yaw = atan2f(new_forward.x, -new_forward.z);
            }
            if (primary && primary->type == ENTITY_BRUSH && primary->vertex_index != -1) {
                Bool moved = false;
                Vec3 move_delta = { 0 };
                Float grid_size = g_EditorState.grid_size;

                if (event->key.keysym.sym == SDLK_UP) {
                    if (g_EditorState.last_active_2d_view == VIEW_TOP_XZ) move_delta.z = -grid_size;
                    else move_delta.y = grid_size;
                    moved = true;
                }
                else if (event->key.keysym.sym == SDLK_DOWN) {
                    if (g_EditorState.last_active_2d_view == VIEW_TOP_XZ) move_delta.z = grid_size;
                    else move_delta.y = -grid_size;
                    moved = true;
                }
                else if (event->key.keysym.sym == SDLK_LEFT) {
                    if (g_EditorState.last_active_2d_view == VIEW_SIDE_YZ) move_delta.z = -grid_size;
                    else move_delta.x = -grid_size;
                    moved = true;
                }
                else if (event->key.keysym.sym == SDLK_RIGHT) {
                    if (g_EditorState.last_active_2d_view == VIEW_SIDE_YZ) move_delta.z = grid_size;
                    else move_delta.x = grid_size;
                    moved = true;
                }

                if (moved) {
                    Brush* b = &scene->brushes[primary->index];
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);

                    Mat4 inv_rot_scale;
                    Mat4 rot_mat_x = Math::mat4_rotate_x(b->rot.x * (Common::PI / 180.0f));
                    Mat4 rot_mat_y = Math::mat4_rotate_y(b->rot.y * (Common::PI / 180.0f));
                    Mat4 rot_mat_z = Math::mat4_rotate_z(b->rot.z * (Common::PI / 180.0f));
                    Mat4 scale_mat = Math::mat4_scale(b->scale);
                    Math::mat4_multiply(&inv_rot_scale, &rot_mat_y, &rot_mat_x);
                    Math::mat4_multiply(&inv_rot_scale, &rot_mat_z, &inv_rot_scale);
                    Math::mat4_multiply(&inv_rot_scale, &inv_rot_scale, &scale_mat);
                    Math::mat4_inverse(&inv_rot_scale, &inv_rot_scale);

                    Vec3 local_move_delta = Math::mat4_mul_vec3_dir(&inv_rot_scale, move_delta);

                    b->vertices[primary->vertex_index].pos = Math::vec3_add(b->vertices[primary->vertex_index].pos, local_move_delta);
                    Brush_CreateRenderData(b);
                    if (b->physicsBody) {
                        Physics::RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                        if (Brush_IsSolid(b) && b->numVertices > 0) {
                            Vec3* world_verts = new Vec3[b->numVertices];
                            for (Int i = 0; i < b->numVertices; ++i) {
                                world_verts[i] = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                            }
                            b->physicsBody = Physics::CreateStaticConvexHull(engine->physicsWorld, reinterpret_cast<const Float*>(world_verts), b->numVertices);
                            delete[] world_verts;
                        }
                        else {
                            b->physicsBody = nullptr;
                        }
                    }
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Nudge Vertex");
                }
            }
            if (event->key.keysym.sym == SDLK_1) g_EditorState.current_gizmo_operation = GIZMO_OP_TRANSLATE;
            if (event->key.keysym.sym == SDLK_2) g_EditorState.current_gizmo_operation = GIZMO_OP_ROTATE;
            if (event->key.keysym.sym == SDLK_3) g_EditorState.current_gizmo_operation = GIZMO_OP_SCALE;
            if (event->key.keysym.sym == SDLK_DELETE) {
                if (g_EditorState.num_selections > 0) {
                    EntityState* deleted_states = new EntityState[g_EditorState.num_selections]();
                    Int num_deleted = 0;
                    for (Int i = 0; i < g_EditorState.num_selections; ++i) {
                        capture_state(&deleted_states[num_deleted++], scene, g_EditorState.selections[i].type, g_EditorState.selections[i].index);
                    }

                    Undo_PushDeleteMultipleEntities(scene, deleted_states, num_deleted, "Delete Selection");

                    for (Int i = g_EditorState.num_selections - 1; i >= 0; --i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        switch (sel->type) {
                        case ENTITY_MODEL: _raw_delete_model(scene, sel->index, engine); break;
                        case ENTITY_BRUSH: _raw_delete_brush(scene, engine, sel->index); break;
                        case ENTITY_LIGHT: _raw_delete_light(scene, sel->index); break;
                        case ENTITY_DECAL: _raw_delete_decal(scene, sel->index); break;
                        case ENTITY_SOUND: _raw_delete_sound_entity(scene, sel->index); break;
                        case ENTITY_PARTICLE_EMITTER: _raw_delete_particle_emitter(scene, sel->index); break;
                        case ENTITY_SPRITE: _raw_delete_sprite(scene, sel->index); break;
                        case ENTITY_VIDEO_PLAYER: _raw_delete_video_player(scene, sel->index); break;
                        case ENTITY_PARALLAX_ROOM: _raw_delete_parallax_room(scene, sel->index); break;
                        case ENTITY_LOGIC: _raw_delete_logic_entity(scene, sel->index); break;
                        case ENTITY_PLAYERSTART: break;
                        default: UNREACHABLE();  break;
                        }
                    }

                    Editor_ClearSelection();
                }
            }
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_RIGHT && !g_EditorState.is_in_z_mode) {
        if (UI_IsWindowOpen("Face Edit Sheet")) {
            EditorSelection* primary = Editor_GetPrimarySelection();
            if (primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
                ViewportType active_viewport = VIEW_COUNT;
                for (Int i = 0; i < VIEW_COUNT; ++i) {
                    if (g_EditorState.is_viewport_hovered[i]) {
                        active_viewport = (ViewportType)i;
                        break;
                    }
                }

                if (active_viewport == VIEW_PERSPECTIVE) {
                    Float ndc_x = (g_EditorState.mouse_pos_in_viewport[active_viewport].x / g_EditorState.viewport_width[active_viewport]) * 2.0f - 1.0f;
                    Float ndc_y = 1.0f - (g_EditorState.mouse_pos_in_viewport[active_viewport].y / g_EditorState.viewport_height[active_viewport]) * 2.0f;
                    Mat4 inv_proj, inv_view;
                    Math::mat4_inverse(&g_proj_matrix[active_viewport], &inv_proj);
                    Math::mat4_inverse(&g_view_matrix[active_viewport], &inv_view);
                    Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
                    Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip);
                    ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                    Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye);
                    Vec3 ray_dir_world = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
                    Math::vec3_normalize(&ray_dir_world);
                    Vec3 ray_origin_world = g_EditorState.editor_camera.position;

                    Float closest_t = FLT_MAX;
                    Int hit_brush_index = -1;
                    Int hit_face_index = -1;

                    for (Int i = 0; i < g_CurrentScene->numBrushes; ++i) {
                        Brush* brush = &g_CurrentScene->brushes[i];
                        Mat4 inv_brush_model_matrix;
                        if (!Math::mat4_inverse(&brush->modelMatrix, &inv_brush_model_matrix)) continue;
                        Vec3 ray_origin_local = Math::mat4_mul_vec3(&inv_brush_model_matrix, ray_origin_world);
                        Vec3 ray_dir_local = Math::mat4_mul_vec3_dir(&inv_brush_model_matrix, ray_dir_world);

                        for (Int face_idx = 0; face_idx < brush->numFaces; ++face_idx) {
                            BrushFace* face = &brush->faces[face_idx];
                            if (face->numVertexIndices < 3) continue;

                            for (Int k = 0; k < face->numVertexIndices - 2; ++k) {
                                Vec3 v0 = brush->vertices[face->vertexIndices[0]].pos;
                                Vec3 v1 = brush->vertices[face->vertexIndices[k + 1]].pos;
                                Vec3 v2 = brush->vertices[face->vertexIndices[k + 2]].pos;
                                Float t;
                                if (Math::RayIntersectsTriangle(ray_origin_local, ray_dir_local, v0, v1, v2, &t) && t < closest_t) {
                                    closest_t = t;
                                    hit_brush_index = i;
                                    hit_face_index = face_idx;
                                }
                            }
                        }
                    }

                    if (hit_brush_index != -1 && hit_face_index != -1) {
                        BrushFace* src_face = &scene->brushes[primary->index].faces[primary->face_index];
                        Brush* dest_brush = &scene->brushes[hit_brush_index];
                        BrushFace* dest_face = &dest_brush->faces[hit_face_index];

                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, hit_brush_index);

                        dest_face->material = src_face->material;
                        dest_face->material2 = src_face->material2;
                        dest_face->material3 = src_face->material3;
                        dest_face->material4 = src_face->material4;
                        dest_face->uv_scale = src_face->uv_scale;
                        dest_face->uv_offset = src_face->uv_offset;
                        dest_face->uv_rotation = src_face->uv_rotation;
                        dest_face->uv_scale2 = src_face->uv_scale2;
                        dest_face->uv_offset2 = src_face->uv_offset2;
                        dest_face->uv_rotation2 = src_face->uv_rotation2;
                        dest_face->uv_scale3 = src_face->uv_scale3;
                        dest_face->uv_offset3 = src_face->uv_offset3;
                        dest_face->uv_rotation3 = src_face->uv_rotation3;
                        dest_face->uv_scale4 = src_face->uv_scale4;
                        dest_face->uv_offset4 = src_face->uv_offset4;
                        dest_face->uv_rotation4 = src_face->uv_rotation4;
                        dest_face->lightmap_scale = src_face->lightmap_scale;

                        Brush_CreateRenderData(dest_brush);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, hit_brush_index, "Apply Face Properties");
                        return;
                    }
                }
            }
        }
        EditorSelection* primary = Editor_GetPrimarySelection();
        for (Int i = 0; i < VIEW_COUNT; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                if (primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
                    g_EditorState.show_texture_browser = true;
                }
                break;
            }
        }
    }
}
void Editor_Update(Engine* engine, Scene* scene) {
    Bool can_move = g_EditorState.is_in_z_mode || (g_EditorState.is_viewport_focused[VIEW_PERSPECTIVE] && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT)));
    if (can_move) {
        const Uint8* state = SDL_GetKeyboardState(nullptr); Float speed = g_EditorState.editor_camera_speed * engine->deltaTime * (state[SDL_SCANCODE_LSHIFT] ? 2.5f : 1.0f);
        Vec3 forward = { cosf(g_EditorState.editor_camera.pitch) * sinf(g_EditorState.editor_camera.yaw), sinf(g_EditorState.editor_camera.pitch), -cosf(g_EditorState.editor_camera.pitch) * cosf(g_EditorState.editor_camera.yaw) };
        Math::vec3_normalize(&forward); Vec3 right = Math::vec3_cross(forward, Vec3{ 0, 1, 0 }); Math::vec3_normalize(&right);
        if (state[SDL_SCANCODE_W]) g_EditorState.editor_camera.position = Math::vec3_add(g_EditorState.editor_camera.position, Math::vec3_muls(forward, speed));
        if (state[SDL_SCANCODE_S]) g_EditorState.editor_camera.position = Math::vec3_sub(g_EditorState.editor_camera.position, Math::vec3_muls(forward, speed));
        if (state[SDL_SCANCODE_D]) g_EditorState.editor_camera.position = Math::vec3_add(g_EditorState.editor_camera.position, Math::vec3_muls(right, speed));
        if (state[SDL_SCANCODE_A]) g_EditorState.editor_camera.position = Math::vec3_sub(g_EditorState.editor_camera.position, Math::vec3_muls(right, speed));
        if (state[SDL_SCANCODE_E]) g_EditorState.editor_camera.position.y += speed;
        if (state[SDL_SCANCODE_Q]) g_EditorState.editor_camera.position.y -= speed;
    }

    EditorSelection* primary_sel = Editor_GetPrimarySelection();
    if (primary_sel && primary_sel->type == ENTITY_MODEL) {
        SceneObject* obj = &scene->objects[primary_sel->index];
        if (g_EditorState.preview_animation_playing && g_EditorState.preview_animation_index != -1) {
            g_EditorState.preview_animation_time += engine->deltaTime;
            AnimationClip* clip = &obj->model->animations[g_EditorState.preview_animation_index];
            if (g_EditorState.preview_animation_time > clip->duration) {
                g_EditorState.preview_animation_time = fmod(g_EditorState.preview_animation_time, clip->duration);
            }
        }
        if (obj->model && obj->model->num_animations > 0 && g_EditorState.preview_animation_index != -1) {
            evaluate_animation(obj, g_EditorState.preview_animation_time);
        }
    }

    for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
        if (g_EditorState.is_viewport_focused[i]) {
            g_EditorState.last_active_2d_view = (ViewportType)i;
        }
    }

    EditorSelection* primary = Editor_GetPrimarySelection();

    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
    g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_NONE;
    g_EditorState.paint_brush_hit_surface = false;
    if ((g_EditorState.is_painting_mode_enabled || g_EditorState.is_sculpting_mode_enabled) &&
        primary && primary->type == ENTITY_BRUSH &&
        g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE])
    {
        Brush* b = &scene->brushes[primary->index];
        Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];

        Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
        Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
        Mat4 inv_proj, inv_view;
        Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
        Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);

        Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
        Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip);
        ray_eye.z = -1.0f; ray_eye.w = 0.0f;

        Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye);
        Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
        Math::vec3_normalize(&ray_dir);
        Vec3 ray_origin = g_EditorState.editor_camera.position;

        Mat4 inv_brush_model_matrix;
        if (Math::mat4_inverse(&b->modelMatrix, &inv_brush_model_matrix)) {
            Vec3 ray_origin_local = Math::mat4_mul_vec3(&inv_brush_model_matrix, ray_origin);
            Vec3 ray_dir_local = Math::mat4_mul_vec3_dir(&inv_brush_model_matrix, ray_dir);
            Float closest_t = FLT_MAX;

            for (Int face_idx = 0; face_idx < b->numFaces; ++face_idx) {
                BrushFace* face = &b->faces[face_idx];
                if (face->numVertexIndices < 3) continue;

                for (Int k = 0; k < face->numVertexIndices - 2; ++k) {
                    Vec3 v0_local = b->vertices[face->vertexIndices[0]].pos;
                    Vec3 v1_local = b->vertices[face->vertexIndices[k + 1]].pos;
                    Vec3 v2_local = b->vertices[face->vertexIndices[k + 2]].pos;

                    Float t_triangle_local;
                    if (Math::RayIntersectsTriangle(ray_origin_local, ray_dir_local, v0_local, v1_local, v2_local, &t_triangle_local)) {
                        if (t_triangle_local > 0.0f && t_triangle_local < closest_t) {
                            closest_t = t_triangle_local;
                            g_EditorState.paint_brush_hit_surface = true;
                            g_EditorState.paint_brush_world_pos = Math::vec3_add(ray_origin, Math::vec3_muls(ray_dir, t_triangle_local));
                            Vec3 face_normal_local = Math::vec3_cross(Math::vec3_sub(v1_local, v0_local), Math::vec3_sub(v2_local, v0_local));
                            g_EditorState.paint_brush_world_normal = Math::mat4_mul_vec3_dir(&b->modelMatrix, face_normal_local);
                            Math::vec3_normalize(&g_EditorState.paint_brush_world_normal);
                        }
                    }
                }
            }
        }
        if (g_EditorState.is_painting) {
            Bool needs_update = false;
            Float radius_sq = g_EditorState.paint_brush_radius * g_EditorState.paint_brush_radius;
            for (Int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                Float dist_sq = Math::vec3_length_sq(Math::vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                if (dist_sq < radius_sq) {
                    Float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.paint_brush_radius;
                    Float blend_amount = g_EditorState.paint_brush_strength * falloff * engine->unscaledDeltaTime * 10.0f;
                    Float* channel_to_paint = nullptr;
                    if (g_EditorState.paint_channel == 0) channel_to_paint = &b->vertices[v_idx].color.x;
                    else if (g_EditorState.paint_channel == 1) channel_to_paint = &b->vertices[v_idx].color.y;
                    else if (g_EditorState.paint_channel == 2) channel_to_paint = &b->vertices[v_idx].color.z;

                    if (channel_to_paint) {
                        if (SDL_GetModState() & KMOD_SHIFT) *channel_to_paint -= blend_amount;
                        else *channel_to_paint += blend_amount;
                        *channel_to_paint = fmaxf(0.0f, fminf(1.0f, *channel_to_paint));
                        needs_update = true;
                    }
                }
            }
            if (needs_update) Brush_CreateRenderData(b);
        }
        if (g_EditorState.is_sculpting) {
            Bool needs_update = false;
            Float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
            for (Int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                Float dist_sq = Math::vec3_length_sq(Math::vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                if (dist_sq < radius_sq) {
                    Float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.sculpt_brush_radius;
                    Float sculpt_amount = g_EditorState.sculpt_brush_strength * falloff * engine->unscaledDeltaTime * 10.0f;
                    if (SDL_GetModState() & KMOD_SHIFT) sculpt_amount = -sculpt_amount;

                    b->vertices[v_idx].pos = Math::vec3_add(b->vertices[v_idx].pos, Math::vec3_muls(g_EditorState.paint_brush_world_normal, sculpt_amount));
                    needs_update = true;
                }
            }
            if (needs_update) {
                Brush_CreateRenderData(b);
                if (b->physicsBody) {
                    Physics::RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                    if (Brush_IsSolid(b) && b->numVertices > 0) {
                        Vec3* world_verts = new Vec3[b->numVertices];
                        for (Int k = 0; k < b->numVertices; ++k)
                            world_verts[k] = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[k].pos);
                        b->physicsBody = Physics::CreateStaticConvexHull(engine->physicsWorld, reinterpret_cast<const Float*>(world_verts), b->numVertices);
                        delete[] world_verts;
                    }
                    else {
                        b->physicsBody = nullptr;
                    }
                }
            }
        }
    }
    if (primary && primary->type == ENTITY_BRUSH && primary->vertex_index != -1 && !g_EditorState.is_manipulating_gizmo && !g_EditorState.is_manipulating_vertex_gizmo) {
        if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
            Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
            Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
            Mat4 inv_proj, inv_view;
            Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
            Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
            Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
            Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip);
            ray_eye.z = -1.0f; ray_eye.w = 0.0f;
            Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye);
            Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
            Math::vec3_normalize(&ray_dir);
            Vec3 ray_origin = g_EditorState.editor_camera.position;

            Brush* b = &scene->brushes[primary->index];
            Vec3 vert_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[primary->vertex_index].pos);

            const Float pick_threshold = 0.1f;
            Float min_dist = FLT_MAX;
            Float t_ray, t_seg;

            Float GIZMO_AXIS_LENGTH = 0.5f;

            Vec3 x_p1 = { vert_world_pos.x + GIZMO_AXIS_LENGTH, vert_world_pos.y, vert_world_pos.z };
            Float dist_x = dist_RaySegment(ray_origin, ray_dir, vert_world_pos, x_p1, &t_ray, &t_seg);
            if (dist_x < pick_threshold && dist_x < min_dist) { min_dist = dist_x; g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_X; }

            Vec3 y_p1 = { vert_world_pos.x, vert_world_pos.y + GIZMO_AXIS_LENGTH, vert_world_pos.z };
            Float dist_y = dist_RaySegment(ray_origin, ray_dir, vert_world_pos, y_p1, &t_ray, &t_seg);
            if (dist_y < pick_threshold && dist_y < min_dist) { min_dist = dist_y; g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_Y; }

            Vec3 z_p1 = { vert_world_pos.x, vert_world_pos.y, vert_world_pos.z + GIZMO_AXIS_LENGTH };
            Float dist_z = dist_RaySegment(ray_origin, ray_dir, vert_world_pos, z_p1, &t_ray, &t_seg);
            if (dist_z < pick_threshold && dist_z < min_dist) { g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_Z; }
        }
    }
    g_EditorState.sprinkle_brush_hit_surface = false;
    if (g_EditorState.show_sprinkle_tool_window && g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
        Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
        Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
        Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
        Mat4 inv_proj, inv_view;
        Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
        Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
        Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
        Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip);
        ray_eye.z = -1.0f; ray_eye.w = 0.0f;
        Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye);
        Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
        Math::vec3_normalize(&ray_dir);
        Vec3 ray_origin = g_EditorState.editor_camera.position;

        RaycastHitInfo hit_info;
        if (Physics::Raycast(engine->physicsWorld, ray_origin, Math::vec3_add(ray_origin, Math::vec3_muls(ray_dir, 1000.0f)), &hit_info)) {
            g_EditorState.sprinkle_brush_hit_surface = true;
            g_EditorState.sprinkle_brush_world_pos = hit_info.point;
        }

        if (g_EditorState.is_sprinkling) {
            g_EditorState.sprinkle_timer -= engine->unscaledDeltaTime;
            if (g_EditorState.sprinkle_timer <= 0.0f) {
                g_EditorState.sprinkle_timer = 1.0f / g_EditorState.sprinkle_density;

                if (g_EditorState.sprinkle_brush_hit_surface) {
                    if (g_EditorState.sprinkle_mode == 0) {
                        Vec3 surface_normal = g_EditorState.paint_brush_world_normal;

                        Vec3 tangent = Math::vec3_cross(surface_normal, Vec3{ 0.0f, 1.0f, 0.0f });
                        if (Math::vec3_length_sq(tangent) < 0.001f) {
                            tangent = Math::vec3_cross(surface_normal, Vec3{ 1.0f, 0.0f, 0.0f });
                        }
                        Math::vec3_normalize(&tangent);
                        Vec3 bitangent = Math::vec3_cross(surface_normal, tangent);

                        Float rand_angle = Math::rand_float_range(0, 2.0f * Common::PI);
                        Float rand_dist = sqrtf(Math::rand_float_range(0, 1)) * g_EditorState.sprinkle_radius;

                        Vec3 offset_on_plane = Math::vec3_add(Math::vec3_muls(tangent, cosf(rand_angle) * rand_dist), Math::vec3_muls(bitangent, sinf(rand_angle) * rand_dist));
                        Vec3 final_pos = Math::vec3_add(g_EditorState.sprinkle_brush_world_pos, offset_on_plane);

                        if (scene->numObjects < 8192) {
                            scene->numObjects++;

                            SceneObject* new_objects = new SceneObject[scene->numObjects];

                            for (Usize i = 0; i < scene->numObjects - 1; ++i) {
                                new_objects[i] = scene->objects[i];
                            }

                            delete[] scene->objects;
                            scene->objects = new_objects;

                            SceneObject* newObj = &scene->objects[scene->numObjects - 1];

                            memset(newObj, 0, sizeof(SceneObject));
                            Math::mat4_identity(&newObj->animated_local_transform);

                            strncpy(newObj->modelPath, g_EditorState.sprinkle_model_path, sizeof(newObj->modelPath) - 1);
                            newObj->pos = final_pos;
                            Float scale = Math::rand_float_range(g_EditorState.sprinkle_scale_min, g_EditorState.sprinkle_scale_max);
                            newObj->scale = Vec3{ scale, scale, scale };
                            newObj->rot = Vec3{ 0, 0, 0 };

                            if (g_EditorState.sprinkle_align_to_normal) {
                                Vec3 obj_forward = surface_normal;
                                Vec3 obj_up = (fabs(obj_forward.y) > 0.99f) ? Vec3{ 1, 0, 0 } : Vec3{ 0, 1, 0 };
                                Vec3 obj_right = Math::vec3_cross(obj_up, obj_forward);
                                Math::vec3_normalize(&obj_right);
                                obj_up = Math::vec3_cross(obj_forward, obj_right);

                                Mat4 rot_matrix{};
                                rot_matrix.m[0] = obj_right.x;  rot_matrix.m[4] = obj_up.x;  rot_matrix.m[8] = obj_forward.x;  rot_matrix.m[12] = 0;
                                rot_matrix.m[1] = obj_right.y;  rot_matrix.m[5] = obj_up.y;  rot_matrix.m[9] = obj_forward.y;  rot_matrix.m[13] = 0;
                                rot_matrix.m[2] = obj_right.z;  rot_matrix.m[6] = obj_up.z;  rot_matrix.m[10] = obj_forward.z;  rot_matrix.m[14] = 0;
                                rot_matrix.m[3] = 0;            rot_matrix.m[7] = 0;         rot_matrix.m[11] = 0;                rot_matrix.m[15] = 1;

                                Vec3 dummyScale{};
                                Vec3 dummyTranslation{};
                                Math::mat4_decompose(&rot_matrix, &dummyTranslation, &newObj->rot, &dummyScale);
                            }

                            if (g_EditorState.sprinkle_random_yaw) {
                                newObj->rot.y = Math::rand_float_range(0, 360.0f);
                            }

                            SceneObject_UpdateMatrix(newObj);
                            newObj->model = Model_Load(newObj->modelPath);

                            Undo_PushCreateEntity(scene, ENTITY_MODEL, scene->numObjects - 1, "Sprinkle Object");
                        }
                    }
                    else {
                        for (Int i = scene->numObjects - 1; i >= 0; --i) {
                            if (strcmp(scene->objects[i].modelPath, g_EditorState.sprinkle_model_path) == 0) {
                                Float dist_sq = Math::vec3_length_sq(Math::vec3_sub(scene->objects[i].pos, g_EditorState.sprinkle_brush_world_pos));
                                if (dist_sq < g_EditorState.sprinkle_radius * g_EditorState.sprinkle_radius / 10.0) {
                                    Undo_PushDeleteEntity(scene, ENTITY_MODEL, i, "Erase Sprinkled Model");
                                    _raw_delete_model(scene, i, engine);
                                    Editor_RemoveFromSelection(ENTITY_MODEL, i);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (!g_EditorState.is_dragging_preview_brush_handle) {
        g_EditorState.preview_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
    }

    if (!g_EditorState.is_dragging_preview_brush_body) {
        g_EditorState.is_hovering_preview_brush_body = false;
    }
    if (!g_EditorState.is_in_brush_creation_mode && primary && primary->type == ENTITY_BRUSH && !g_EditorState.is_dragging_selected_brush_handle && !g_EditorState.is_manipulating_gizmo) {
        g_EditorState.selected_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
        Brush* b = &scene->brushes[primary->index];
        if (b->numVertices == 0) return;

        Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        for (Int i = 0; i < b->numVertices; ++i) {
            local_min.x = fminf(local_min.x, b->vertices[i].pos.x);
            local_min.y = fminf(local_min.y, b->vertices[i].pos.y);
            local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
            local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x);
            local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y);
            local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
        }
        Vec3 local_center = Math::vec3_muls(Math::vec3_add(local_min, local_max), 0.5f);

        for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                Float handle_pick_dist_sq = powf(g_EditorState.ortho_cam_zoom[i - 1] * 0.055f, 2.0f);

                Vec3 handle_local_positions[6] = {
                    {local_min.x, local_center.y, local_center.z}, {local_max.x, local_center.y, local_center.z},
                    {local_center.x, local_min.y, local_center.z}, {local_center.x, local_max.y, local_center.z},
                    {local_center.x, local_center.y, local_min.z}, {local_center.x, local_center.y, local_max.z}
                };

                for (Int h_idx = 0; h_idx < 6; ++h_idx) {
                    Bool is_handle_relevant_to_view = false;
                    if (i == VIEW_TOP_XZ) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_X || h_idx == PREVIEW_BRUSH_HANDLE_MAX_X || h_idx == PREVIEW_BRUSH_HANDLE_MIN_Z || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Z) {
                            is_handle_relevant_to_view = true;
                        }
                    }
                    else if (i == VIEW_FRONT_XY) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_X || h_idx == PREVIEW_BRUSH_HANDLE_MAX_X || h_idx == PREVIEW_BRUSH_HANDLE_MIN_Y || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Y) {
                            is_handle_relevant_to_view = true;
                        }
                    }
                    else if (i == VIEW_SIDE_YZ) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_Y || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Y || h_idx == PREVIEW_BRUSH_HANDLE_MIN_Z || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Z) {
                            is_handle_relevant_to_view = true;
                        }
                    }

                    if (is_handle_relevant_to_view) {
                        Vec3 handle_world_pos = Math::mat4_mul_vec3(&b->modelMatrix, handle_local_positions[h_idx]);
                        Float dist_sq = 0.0f;

                        if (i == VIEW_TOP_XZ) {
                            dist_sq = powf(mouse_world.x - handle_world_pos.x, 2) + powf(mouse_world.z - handle_world_pos.z, 2);
                        }
                        else if (i == VIEW_FRONT_XY) {
                            dist_sq = powf(mouse_world.x - handle_world_pos.x, 2) + powf(mouse_world.y - handle_world_pos.y, 2);
                        }
                        else if (i == VIEW_SIDE_YZ) {
                            dist_sq = powf(mouse_world.y - handle_world_pos.y, 2) + powf(mouse_world.z - handle_world_pos.z, 2);
                        }

                        if (dist_sq <= handle_pick_dist_sq) {
                            g_EditorState.selected_brush_hovered_handle = (PreviewBrushHandleType)h_idx;
                            return;
                        }
                    }
                }
            }
        }
    }
    if (g_EditorState.is_in_brush_creation_mode && !g_EditorState.is_dragging_preview_brush_handle && !g_EditorState.is_manipulating_gizmo) {
        for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);

                Float pick_radius_factor = 0.055f;
                Float handle_pick_dist_sq = powf(g_EditorState.ortho_cam_zoom[i - 1] * pick_radius_factor, 2.0f);

                Vec3 handle_centers_world[PREVIEW_BRUSH_HANDLE_COUNT];
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MIN_X] = Vec3{ g_EditorState.preview_brush_world_min.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MAX_X] = Vec3{ g_EditorState.preview_brush_world_max.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MIN_Y] = Vec3{ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush_world_min.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MAX_Y] = Vec3{ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush_world_max.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MIN_Z] = Vec3{ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush_world_min.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MAX_Z] = Vec3{ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush_world_max.z };

                for (Int h_idx = 0; h_idx < PREVIEW_BRUSH_HANDLE_COUNT; ++h_idx) {
                    Bool is_handle_relevant_to_view = false;
                    Float dist_sq = FLT_MAX;

                    if (i == VIEW_TOP_XZ) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_X || h_idx == PREVIEW_BRUSH_HANDLE_MAX_X) {
                            dist_sq = powf(mouse_world.x - handle_centers_world[h_idx].x, 2) + powf(mouse_world.z - handle_centers_world[h_idx].z, 2);
                            is_handle_relevant_to_view = true;
                        }
                        else if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_Z || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Z) {
                            dist_sq = powf(mouse_world.x - handle_centers_world[h_idx].x, 2) + powf(mouse_world.z - handle_centers_world[h_idx].z, 2);
                            is_handle_relevant_to_view = true;
                        }
                    }
                    else if (i == VIEW_FRONT_XY) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_X || h_idx == PREVIEW_BRUSH_HANDLE_MAX_X) {
                            dist_sq = powf(mouse_world.x - handle_centers_world[h_idx].x, 2) + powf(mouse_world.y - handle_centers_world[h_idx].y, 2);
                            is_handle_relevant_to_view = true;
                        }
                        else if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_Y || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Y) {
                            dist_sq = powf(mouse_world.x - handle_centers_world[h_idx].x, 2) + powf(mouse_world.y - handle_centers_world[h_idx].y, 2);
                            is_handle_relevant_to_view = true;
                        }
                    }
                    else if (i == VIEW_SIDE_YZ) {
                        if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_Y || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Y) {
                            dist_sq = powf(mouse_world.y - handle_centers_world[h_idx].y, 2) + powf(mouse_world.z - handle_centers_world[h_idx].z, 2);
                            is_handle_relevant_to_view = true;
                        }
                        else if (h_idx == PREVIEW_BRUSH_HANDLE_MIN_Z || h_idx == PREVIEW_BRUSH_HANDLE_MAX_Z) {
                            dist_sq = powf(mouse_world.y - handle_centers_world[h_idx].y, 2) + powf(mouse_world.z - handle_centers_world[h_idx].z, 2);
                            is_handle_relevant_to_view = true;
                        }
                    }

                    if (is_handle_relevant_to_view && dist_sq <= handle_pick_dist_sq) {
                        g_EditorState.preview_brush_hovered_handle = (PreviewBrushHandleType)h_idx;
                        goto found_hovered_handle_update;
                    }
                }
            }
        }
    found_hovered_handle_update:;
    }
    if (g_EditorState.is_in_brush_creation_mode &&
        !g_EditorState.is_dragging_preview_brush_handle &&
        !g_EditorState.is_manipulating_gizmo &&
        g_EditorState.preview_brush_hovered_handle == PREVIEW_BRUSH_HANDLE_NONE) {
        g_EditorState.is_hovering_preview_brush_body = false;
        for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                Vec3 b_min = g_EditorState.preview_brush_world_min;
                Vec3 b_max = g_EditorState.preview_brush_world_max;

                Bool hovered_this_view = false;
                if (i == VIEW_TOP_XZ) {
                    if (mouse_world.x >= b_min.x && mouse_world.x <= b_max.x &&
                        mouse_world.z >= b_min.z && mouse_world.z <= b_max.z) {
                        hovered_this_view = true;
                    }
                }
                else if (i == VIEW_FRONT_XY) {
                    if (mouse_world.x >= b_min.x && mouse_world.x <= b_max.x &&
                        mouse_world.y >= b_min.y && mouse_world.y <= b_max.y) {
                        hovered_this_view = true;
                    }
                }
                else if (i == VIEW_SIDE_YZ) {
                    if (mouse_world.y >= b_min.y && mouse_world.y <= b_max.y &&
                        mouse_world.z >= b_min.z && mouse_world.z <= b_max.z) {
                        hovered_this_view = true;
                    }
                }

                if (hovered_this_view) {
                    g_EditorState.is_hovering_preview_brush_body = true;
                    break;
                }
            }
        }
    }
    else if (g_EditorState.preview_brush_hovered_handle != PREVIEW_BRUSH_HANDLE_NONE) {
        g_EditorState.is_hovering_preview_brush_body = false;
    }
    if (primary && primary->type == ENTITY_BRUSH &&
        !g_EditorState.is_dragging_selected_brush_handle && !g_EditorState.is_dragging_selected_brush_body && !g_EditorState.is_manipulating_gizmo &&
        g_EditorState.selected_brush_hovered_handle == PREVIEW_BRUSH_HANDLE_NONE) {
        g_EditorState.is_hovering_selected_brush_body = false;
        Brush* b = &scene->brushes[primary->index];
        if (b->numVertices > 0) {
            Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (Int i = 0; i < b->numVertices; ++i) {
                local_min.x = fminf(local_min.x, b->vertices[i].pos.x);
                local_min.y = fminf(local_min.y, b->vertices[i].pos.y);
                local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
                local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x);
                local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y);
                local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
            }
            Vec3 world_min = Math::mat4_mul_vec3(&b->modelMatrix, local_min);
            Vec3 world_max = Math::mat4_mul_vec3(&b->modelMatrix, local_max);

            for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                    Bool hovered_this_view = false;
                    if (i == VIEW_TOP_XZ) {
                        if (mouse_world.x >= world_min.x && mouse_world.x <= world_max.x && mouse_world.z >= world_min.z && mouse_world.z <= world_max.z)
                            hovered_this_view = true;
                    }
                    else if (i == VIEW_FRONT_XY) {
                        if (mouse_world.x >= world_min.x && mouse_world.x <= world_max.x && mouse_world.y >= world_min.y && mouse_world.y <= world_max.y)
                            hovered_this_view = true;
                    }
                    else if (i == VIEW_SIDE_YZ) {
                        if (mouse_world.y >= world_min.y && mouse_world.y <= world_max.y && mouse_world.z >= world_min.z && mouse_world.z <= world_max.z)
                            hovered_this_view = true;
                    }
                    if (hovered_this_view) {
                        g_EditorState.is_hovering_selected_brush_body = true;
                        break;
                    }
                }
            }
        }
    }
    if (g_EditorState.vertex_gizmo_hovered_axis == GIZMO_AXIS_NONE && g_EditorState.gizmo_active_axis == GIZMO_AXIS_NONE && (g_EditorState.num_selections > 0 || g_EditorState.is_in_brush_creation_mode)) {
        Vec3 gizmo_target_pos;
        Bool use_gizmo = false;
        if (g_EditorState.is_in_brush_creation_mode) {
            gizmo_target_pos = g_EditorState.preview_brush.pos;
            use_gizmo = true;
        }
        else if (g_EditorState.num_selections > 0) {
            g_EditorState.gizmo_selection_centroid = Vec3{ 0 };
            for (Int i = 0; i < g_EditorState.num_selections; ++i) {
                Vec3 pos;
                switch (g_EditorState.selections[i].type) {
                case ENTITY_MODEL: pos = scene->objects[g_EditorState.selections[i].index].pos; break;
                case ENTITY_BRUSH: pos = scene->brushes[g_EditorState.selections[i].index].pos; break;
                case ENTITY_LIGHT: pos = scene->lights[g_EditorState.selections[i].index].pos; break;
                case ENTITY_DECAL: pos = scene->decals[g_EditorState.selections[i].index].pos; break;
                case ENTITY_SOUND: pos = scene->soundEntities[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PARTICLE_EMITTER: pos = scene->particleEmitters[g_EditorState.selections[i].index].pos; break;
                case ENTITY_SPRITE: pos = scene->sprites[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PLAYERSTART: pos = scene->playerStart.pos; break;
                case ENTITY_VIDEO_PLAYER: pos = scene->videoPlayers[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PARALLAX_ROOM: pos = scene->parallaxRooms[g_EditorState.selections[i].index].pos; break;
                case ENTITY_LOGIC: pos = scene->logicEntities[g_EditorState.selections[i].index].pos; break;
                default: UNREACHABLE();  break;
                }
                g_EditorState.gizmo_selection_centroid = Math::vec3_add(g_EditorState.gizmo_selection_centroid, pos);
            }
            g_EditorState.gizmo_selection_centroid = Math::vec3_muls(g_EditorState.gizmo_selection_centroid, 1.0f / g_EditorState.num_selections);
            gizmo_target_pos = g_EditorState.gizmo_selection_centroid;
            use_gizmo = true;
        }

        if (use_gizmo) {
            if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
                Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                Float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                Float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view;
                Math::mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
                Math::mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
                Vec4 ray_eye = Math::mat4_mul_vec4(&inv_proj, ray_clip);
                ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = Math::mat4_mul_vec4(&inv_view, ray_eye);
                Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
                Math::vec3_normalize(&ray_dir);
                Editor_UpdateGizmoHover(scene, g_EditorState.editor_camera.position, ray_dir);
            }
            if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_NONE) {
                for (Int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                    if (g_EditorState.is_viewport_hovered[i]) {
                        if (primary && primary->type == ENTITY_BRUSH) {
                            continue;
                        }
                        Vec3 mouse_world = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                        Float threshold = g_EditorState.ortho_cam_zoom[i - 1] * 0.05f;
                        Float GIZMO_SIZE = 1.0f;

                        if (i == VIEW_TOP_XZ) {
                            if (fabsf(mouse_world.z - gizmo_target_pos.z) < threshold && mouse_world.x >= gizmo_target_pos.x && mouse_world.x <= gizmo_target_pos.x + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X;
                            else if (fabsf(mouse_world.x - gizmo_target_pos.x) < threshold && mouse_world.z >= gizmo_target_pos.z && mouse_world.z <= gizmo_target_pos.z + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z;
                        }
                        else if (i == VIEW_FRONT_XY) {
                            if (fabsf(mouse_world.y - gizmo_target_pos.y) < threshold && mouse_world.x >= gizmo_target_pos.x && mouse_world.x <= gizmo_target_pos.x + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X;
                            else if (fabsf(mouse_world.x - gizmo_target_pos.x) < threshold && mouse_world.y >= gizmo_target_pos.y && mouse_world.y <= gizmo_target_pos.y + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y;
                        }
                        else if (i == VIEW_SIDE_YZ) {
                            if (fabsf(mouse_world.z - gizmo_target_pos.z) < threshold && mouse_world.y >= gizmo_target_pos.y && mouse_world.y <= gizmo_target_pos.y + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y;
                            else if (fabsf(mouse_world.y - gizmo_target_pos.y) < threshold && mouse_world.z >= gizmo_target_pos.z && mouse_world.z <= gizmo_target_pos.z + GIZMO_SIZE) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z;
                        }
                        if (g_EditorState.gizmo_hovered_axis != GIZMO_AXIS_NONE) break;
                    }
                }
            }
        }
    }
    for (Int i = 0; i < scene->numParticleEmitters; ++i) { ParticleEmitter_Update(&scene->particleEmitters[i], engine->deltaTime); }
    g_EditorState.autosave_timer += engine->unscaledDeltaTime;
    if (g_EditorState.autosave_timer >= 300.0f) {
        if (strcmp(g_EditorState.currentMapPath, "untitled.map") != 0) {
            Char autosave_path[256];
            sprintf(autosave_path, "autosaves/_autosave_%s", g_EditorState.currentMapPath);
            Scene_SaveMap(scene, nullptr, autosave_path);
        }
        g_EditorState.autosave_timer = 0.0f;
    }
}