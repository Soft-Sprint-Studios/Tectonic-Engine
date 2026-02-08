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
#ifndef EDITOR_INTERNAL_H
#define EDITOR_INTERNAL_H

#include "editor.h"


constexpr Int MAX_RECENT_FILES = 10;

    typedef enum {
        BRUSH_SHAPE_BLOCK, BRUSH_SHAPE_CYLINDER, BRUSH_SHAPE_TUBE,
        BRUSH_SHAPE_WEDGE, BRUSH_SHAPE_SPIKE, BRUSH_SHAPE_SPHERE,
        BRUSH_SHAPE_SEMI_SPHERE, BRUSH_SHAPE_ARCH
    } BrushCreationShapeType;

    typedef enum { TRANSFORM_MODE_MOVE, TRANSFORM_MODE_ROTATE, TRANSFORM_MODE_SCALE } TransformWindowMode;
    typedef enum { VIEW_PERSPECTIVE, VIEW_TOP_XZ, VIEW_FRONT_XY, VIEW_SIDE_YZ, VIEW_COUNT } ViewportType;
    typedef enum { GIZMO_AXIS_NONE, GIZMO_AXIS_X, GIZMO_AXIS_Y, GIZMO_AXIS_Z } GizmoAxis;
    typedef enum { GIZMO_OP_TRANSLATE, GIZMO_OP_ROTATE, GIZMO_OP_SCALE } GizmoOperation;
    typedef enum { PENDING_ACTION_NONE, PENDING_ACTION_NEW_MAP, PENDING_ACTION_LOAD_MAP, PENDING_ACTION_EXIT_EDITOR } PendingEditorAction;

    typedef enum {
        PREVIEW_BRUSH_HANDLE_NONE = -1,
        PREVIEW_BRUSH_HANDLE_MIN_X = 0, PREVIEW_BRUSH_HANDLE_MAX_X = 1,
        PREVIEW_BRUSH_HANDLE_MIN_Y = 2, PREVIEW_BRUSH_HANDLE_MAX_Y = 3,
        PREVIEW_BRUSH_HANDLE_MIN_Z = 4, PREVIEW_BRUSH_HANDLE_MAX_Z = 5,
        PREVIEW_BRUSH_HANDLE_COUNT = 6
    } PreviewBrushHandleType;

    typedef struct {
        Char* file_path;
        GLuint thumbnail_texture;
    } ModelBrowserEntry;

    typedef struct {
        Bool initialized; Camera editor_camera;
        Bool is_in_z_mode;
        BrushCreationShapeType current_brush_shape;
        Int cylinder_creation_steps;
        Float tube_wall_thickness;
        ViewportType captured_viewport;
        GLuint viewport_fbo[VIEW_COUNT], viewport_texture[VIEW_COUNT], viewport_rbo[VIEW_COUNT];
        Int viewport_width[VIEW_COUNT], viewport_height[VIEW_COUNT];
        Bool is_viewport_focused[VIEW_COUNT], is_viewport_hovered[VIEW_COUNT];
        Vec2 mouse_pos_in_viewport[VIEW_COUNT];
        Vec3 ortho_cam_pos[3]; Float ortho_cam_zoom[3];
        EditorSelection* selections;
        Int num_selections;
        GizmoOperation current_gizmo_operation;
        Bool is_in_brush_creation_mode;
        Bool is_dragging_for_creation;
        ViewportType brush_creation_view;
        Vec3 brush_creation_start_point_2d_drag;
        Brush preview_brush;
        Vec3 preview_brush_world_min;
        Vec3 preview_brush_world_max;
        PreviewBrushHandleType preview_brush_hovered_handle;
        PreviewBrushHandleType preview_brush_active_handle;
        Bool is_dragging_preview_brush_handle;
        ViewportType preview_brush_drag_handle_view;
        Bool is_hovering_preview_brush_body;
        Bool is_dragging_preview_brush_body;
        ViewportType preview_brush_drag_body_view;
        Vec3 preview_brush_drag_body_start_mouse_world;
        Vec3 preview_brush_drag_body_start_brush_pos;
        Bool is_dragging_selected_brush_handle;
        Bool is_hovering_selected_brush_body;
        Bool is_dragging_selected_brush_body;
        ViewportType selected_brush_drag_body_view;
        Vec3 selected_brush_drag_body_start_mouse_world;
        Vec3 selected_brush_drag_body_start_brush_pos;
        PreviewBrushHandleType selected_brush_hovered_handle;
        PreviewBrushHandleType selected_brush_active_handle;
        Vec3 preview_brush_drag_body_start_brush_world_min_at_drag_start;
        GLuint vertex_points_vao, vertex_points_vbo;
        GLuint debug_shader; GLuint light_gizmo_vao; Int light_gizmo_vertex_count;
        Float grid_size; Bool snap_to_grid;
        GLuint grid_shader, grid_vao, grid_vbo;
        Bool show_add_model_popup; Char add_model_path[128];
        GLuint decal_box_vao, decal_box_vbo;
        Int decal_box_vertex_count;
        GLuint selected_face_vao, selected_face_vbo;
        GLuint model_preview_fbo, model_preview_texture, model_preview_rbo;
        Int model_preview_width, model_preview_height;
        Float model_preview_cam_dist;
        Vec2 model_preview_cam_angles;
        LoadedModel* preview_model;
        Char model_search_filter[64];
        ModelBrowserEntry* model_browser_entries;
        Int num_model_files;
        Int selected_model_file_index;
        Int preview_animation_index;
        Float preview_animation_time;
        Bool preview_animation_playing;
        Bool is_manipulating_gizmo;
        GLuint model_thumb_fbo, model_thumb_texture, model_thumb_rbo;
        GLuint gizmo_shader;
        GLuint gizmo_vao;
        GLuint gizmo_vbo;
        GizmoAxis gizmo_hovered_axis;
        GizmoAxis gizmo_active_axis;
        Vec3 gizmo_drag_start_world;
        Vec3 gizmo_drag_object_start_pos;
        Vec3 gizmo_drag_object_start_rot;
        Vec3 gizmo_drag_object_start_scale;
        Vec3 gizmo_rotation_start_vec;
        Float gizmo_drag_plane_d;
        Vec3 gizmo_drag_plane_normal;
        ViewportType gizmo_drag_view;
        Bool is_vertex_manipulating;
        Int manipulated_vertex_index;
        ViewportType vertex_manipulation_view;
        Vec3 vertex_manipulation_start_pos;
        Bool is_manipulating_vertex_gizmo;
        GizmoAxis vertex_gizmo_hovered_axis;
        GizmoAxis vertex_gizmo_active_axis;
        Vec3 vertex_gizmo_drag_start_world;
        Vec3 vertex_drag_start_pos_world;
        Vec3 vertex_gizmo_drag_plane_normal;
        Float vertex_gizmo_drag_plane_d;
        Bool is_clipping;
        Int clip_point_count;
        Vec3 clip_points[2];
        Vec3 clip_side_point;
        ViewportType clip_view;
        Float clip_plane_depth;
        Char currentMapPath[256];
        Bool show_load_map_popup;
        Bool show_save_map_popup;
        Char save_map_path[128];
        Char** map_file_list;
        Int num_map_files;
        Int selected_map_file_index;
        GLuint player_start_gizmo_vao, player_start_gizmo_vbo;
        Int player_start_gizmo_vertex_count;
        Bool is_painting;
        Bool is_painting_mode_enabled;
        Float paint_brush_radius;
        Float paint_brush_strength;
        Bool show_texture_browser;
        Char texture_search_filter[64];
        Int texture_browser_target;
        Int paint_channel;
        Bool is_sculpting;
        Bool is_sculpting_mode_enabled;
        Float sculpt_brush_radius;
        Float sculpt_brush_strength;
        Bool show_sound_browser_popup;
        Char** sound_file_list;
        Int num_sound_files;
        Int selected_sound_file_index;
        Char sound_search_filter[64];
        Uint preview_sound_buffer;
        Uint preview_sound_source;
        Bool paint_brush_hit_surface;
        Vec3 paint_brush_world_pos;
        Vec3 paint_brush_world_normal;
        Bool show_replace_textures_popup;
        Int find_material_index;
        Int replace_material_index;
        Bool show_vertex_tools_window;
        Bool show_sculpt_noise_popup;
        Bool show_about_window;
        Bool show_sprinkle_tool_window;
        Char sprinkle_model_path[128];
        Float sprinkle_density;
        Float sprinkle_radius;
        Int sprinkle_mode;
        Float sprinkle_scale_min;
        Float sprinkle_scale_max;
        Bool sprinkle_align_to_normal;
        Bool sprinkle_random_yaw;
        Bool is_sprinkling;
        Float sprinkle_timer;
        Bool sprinkle_brush_hit_surface;
        Vec3 sprinkle_brush_world_pos;
        ViewportType last_active_2d_view;
        Float editor_camera_speed;
        Bool texture_lock_enabled;
        Char** doc_files;
        Int num_doc_files;
        Int selected_doc_index;
        Char* current_doc_content;
        Char** recent_map_files;
        Int num_recent_map_files;
        Vec3 gizmo_selection_centroid;
        Vec3* gizmo_drag_start_positions;
        Vec3* gizmo_drag_start_rotations;
        Vec3* gizmo_drag_start_scales;
        Int next_group_id;
        Bool gizmo_drag_has_cloned;
        Bool show_bake_lighting_popup;
        Int bake_resolution;
        Int bake_bounces;
        Bool show_arch_properties_popup;
        Bool show_build_cubemaps_popup;
        Int cubemap_resolution_index;
        Float arch_wall_width;
        Int arch_num_sides;
        Float autosave_timer;
        Float arch_arc_degrees;
        Float arch_start_angle_degrees;
        Float arch_add_height;
        Vec3 arch_creation_start_point;
        Vec3 arch_creation_end_point;
        ViewportType arch_creation_view;
        GLuint arch_preview_fbo, arch_preview_texture, arch_preview_rbo;
        Int arch_preview_width, arch_preview_height;
        Bool show_map_info_window;
        Bool show_transform_window;
        TransformWindowMode transform_window_mode;
        Vec3 transform_window_values;
        Bool show_goto_coord_window;
        Char goto_coord_input[64];
        Bool show_particle_browser_popup;
        Char** particle_file_list;
        Int num_particle_files;
        Int selected_particle_file_index;
        Char particle_search_filter[64];
#define TEXTURE_TARGET_REPLACE_FIND (10)
#define TEXTURE_TARGET_REPLACE_WITH (11)
#define MODEL_BROWSER_TARGET_SPRINKLE (1)
    } EditorState;

    extern EditorState g_EditorState;
    extern Scene* g_CurrentScene;
    extern Mat4 g_view_matrix[VIEW_COUNT];
    extern Mat4 g_proj_matrix[VIEW_COUNT];
    extern BrushFace g_copiedFaceProperties;
    extern Bool g_hasCopiedFace;
    extern Bool g_is_map_dirty;
    extern PendingEditorAction g_pending_action;
    extern Camera g_last_editor_camera_state;
    extern Bool g_has_last_camera_state;


#endif // EDITOR_INTERNAL_H