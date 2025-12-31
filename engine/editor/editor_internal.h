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
#ifndef EDITOR_INTERNAL_H
#define EDITOR_INTERNAL_H

#include "editor.h"

#define MAX_RECENT_FILES 10

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
    char* file_path;
    GLuint thumbnail_texture;
} ModelBrowserEntry;

typedef struct {
    bool initialized; Camera editor_camera;
    bool is_in_z_mode;
    BrushCreationShapeType current_brush_shape;
    int cylinder_creation_steps;
    float tube_wall_thickness;
    ViewportType captured_viewport;
    GLuint viewport_fbo[VIEW_COUNT], viewport_texture[VIEW_COUNT], viewport_rbo[VIEW_COUNT];
    int viewport_width[VIEW_COUNT], viewport_height[VIEW_COUNT];
    bool is_viewport_focused[VIEW_COUNT], is_viewport_hovered[VIEW_COUNT];
    Vec2 mouse_pos_in_viewport[VIEW_COUNT];
    Vec3 ortho_cam_pos[3]; float ortho_cam_zoom[3];
    EditorSelection* selections;
    int num_selections;
    GizmoOperation current_gizmo_operation;
    bool is_in_brush_creation_mode;
    bool is_dragging_for_creation;
    ViewportType brush_creation_view;
    Vec3 brush_creation_start_point_2d_drag;
    Brush preview_brush;
    Vec3 preview_brush_world_min;
    Vec3 preview_brush_world_max;
    PreviewBrushHandleType preview_brush_hovered_handle;
    PreviewBrushHandleType preview_brush_active_handle;
    bool is_dragging_preview_brush_handle;
    ViewportType preview_brush_drag_handle_view;
    bool is_hovering_preview_brush_body;
    bool is_dragging_preview_brush_body;
    ViewportType preview_brush_drag_body_view;
    Vec3 preview_brush_drag_body_start_mouse_world;
    Vec3 preview_brush_drag_body_start_brush_pos;
    bool is_dragging_selected_brush_handle;
    bool is_hovering_selected_brush_body;
    bool is_dragging_selected_brush_body;
    ViewportType selected_brush_drag_body_view;
    Vec3 selected_brush_drag_body_start_mouse_world;
    Vec3 selected_brush_drag_body_start_brush_pos;
    PreviewBrushHandleType selected_brush_hovered_handle;
    PreviewBrushHandleType selected_brush_active_handle;
    Vec3 preview_brush_drag_body_start_brush_world_min_at_drag_start;
    GLuint vertex_points_vao, vertex_points_vbo;
    GLuint debug_shader; GLuint light_gizmo_vao; int light_gizmo_vertex_count;
    float grid_size; bool snap_to_grid;
    GLuint grid_shader, grid_vao, grid_vbo;
    bool show_add_model_popup; char add_model_path[128];
    GLuint decal_box_vao, decal_box_vbo;
    int decal_box_vertex_count;
    GLuint selected_face_vao, selected_face_vbo;
    GLuint model_preview_fbo, model_preview_texture, model_preview_rbo;
    int model_preview_width, model_preview_height;
    float model_preview_cam_dist;
    Vec2 model_preview_cam_angles;
    LoadedModel* preview_model;
    char model_search_filter[64];
    ModelBrowserEntry* model_browser_entries;
    int num_model_files;
    int selected_model_file_index;
    int preview_animation_index;
    float preview_animation_time;
    bool preview_animation_playing;
    bool is_manipulating_gizmo;
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
    float gizmo_drag_plane_d;
    Vec3 gizmo_drag_plane_normal;
    ViewportType gizmo_drag_view;
    bool is_vertex_manipulating;
    int manipulated_vertex_index;
    ViewportType vertex_manipulation_view;
    Vec3 vertex_manipulation_start_pos;
    bool is_manipulating_vertex_gizmo;
    GizmoAxis vertex_gizmo_hovered_axis;
    GizmoAxis vertex_gizmo_active_axis;
    Vec3 vertex_gizmo_drag_start_world;
    Vec3 vertex_drag_start_pos_world;
    Vec3 vertex_gizmo_drag_plane_normal;
    float vertex_gizmo_drag_plane_d;
    bool is_clipping;
    int clip_point_count;
    Vec3 clip_points[2];
    Vec3 clip_side_point;
    ViewportType clip_view;
    float clip_plane_depth;
    char currentMapPath[256];
    bool show_load_map_popup;
    bool show_save_map_popup;
    char save_map_path[128];
    char** map_file_list;
    int num_map_files;
    int selected_map_file_index;
    GLuint player_start_gizmo_vao, player_start_gizmo_vbo;
    int player_start_gizmo_vertex_count;
    bool is_painting;
    bool is_painting_mode_enabled;
    float paint_brush_radius;
    float paint_brush_strength;
    bool show_texture_browser;
    char texture_search_filter[64];
    int texture_browser_target;
    int paint_channel;
    bool is_sculpting;
    bool is_sculpting_mode_enabled;
    float sculpt_brush_radius;
    float sculpt_brush_strength;
    bool show_sound_browser_popup;
    char** sound_file_list;
    int num_sound_files;
    int selected_sound_file_index;
    char sound_search_filter[64];
    unsigned int preview_sound_buffer;
    unsigned int preview_sound_source;
    bool paint_brush_hit_surface;
    Vec3 paint_brush_world_pos;
    Vec3 paint_brush_world_normal;
    bool show_replace_textures_popup;
    int find_material_index;
    int replace_material_index;
    bool show_vertex_tools_window;
    bool show_sculpt_noise_popup;
    bool show_about_window;
    bool show_sprinkle_tool_window;
    char sprinkle_model_path[128];
    float sprinkle_density;
    float sprinkle_radius;
    int sprinkle_mode;
    float sprinkle_scale_min;
    float sprinkle_scale_max;
    bool sprinkle_align_to_normal;
    bool sprinkle_random_yaw;
    bool is_sprinkling;
    float sprinkle_timer;
    bool sprinkle_brush_hit_surface;
    Vec3 sprinkle_brush_world_pos;
    ViewportType last_active_2d_view;
    float editor_camera_speed;
    bool texture_lock_enabled;
    bool show_help_window;
    char** doc_files;
    int num_doc_files;
    int selected_doc_index;
    char* current_doc_content;
    char** recent_map_files;
    int num_recent_map_files;
    Vec3 gizmo_selection_centroid;
    Vec3* gizmo_drag_start_positions;
    Vec3* gizmo_drag_start_rotations;
    Vec3* gizmo_drag_start_scales;
    int next_group_id;
    bool gizmo_drag_has_cloned;
    bool show_bake_lighting_popup;
    int bake_resolution;
    int bake_bounces;
    bool show_arch_properties_popup;
    bool show_build_cubemaps_popup;
    int cubemap_resolution_index;
    float arch_wall_width;
    int arch_num_sides;
    float autosave_timer;
    float arch_arc_degrees;
    float arch_start_angle_degrees;
    float arch_add_height;
    Vec3 arch_creation_start_point;
    Vec3 arch_creation_end_point;
    ViewportType arch_creation_view;
    GLuint arch_preview_fbo, arch_preview_texture, arch_preview_rbo;
    int arch_preview_width, arch_preview_height;
    bool show_map_info_window;
    bool show_transform_window;
    TransformWindowMode transform_window_mode;
    Vec3 transform_window_values;
    bool show_goto_coord_window;
    char goto_coord_input[64];
#define TEXTURE_TARGET_REPLACE_FIND (10)
#define TEXTURE_TARGET_REPLACE_WITH (11)
#define MODEL_BROWSER_TARGET_SPRINKLE (1)
} EditorState;

extern EditorState g_EditorState;
extern Scene* g_CurrentScene;
extern Mat4 g_view_matrix[VIEW_COUNT];
extern Mat4 g_proj_matrix[VIEW_COUNT];
extern BrushFace g_copiedFaceProperties;
extern bool g_hasCopiedFace;
extern bool g_is_map_dirty;

#endif