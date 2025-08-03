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
#include "editor.h"
#include <stdlib.h>
#include "gl_console.h"
#include "lightmapper.h"
#include <GL/glew.h>
#include <SDL.h>
#include "gl_misc.h"
#include <math.h>
#include <float.h>
#include <sys/stat.h>
#ifdef PLATFORM_WINDOWS
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#endif
#include <SDL_image.h>
#include "sound_system.h"
#include "texturemanager.h"
#include "water_manager.h"
#include "io_system.h"
#include "video_player.h"
#include "cvar.h"

typedef enum {
    BRUSH_SHAPE_BLOCK,
    BRUSH_SHAPE_CYLINDER,
    BRUSH_SHAPE_WEDGE,
    BRUSH_SHAPE_SPIKE,
    BRUSH_SHAPE_SPHERE,
    BRUSH_SHAPE_SEMI_SPHERE,
    BRUSH_SHAPE_ARCH
} BrushCreationShapeType;

typedef enum { VIEW_PERSPECTIVE, VIEW_TOP_XZ, VIEW_FRONT_XY, VIEW_SIDE_YZ, VIEW_COUNT } ViewportType;
typedef enum { GIZMO_AXIS_NONE, GIZMO_AXIS_X, GIZMO_AXIS_Y, GIZMO_AXIS_Z } GizmoAxis;
typedef enum { GIZMO_OP_TRANSLATE, GIZMO_OP_ROTATE, GIZMO_OP_SCALE } GizmoOperation;

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
    bool show_bake_lighting_popup;
    int bake_resolution;
    int bake_bounces;
    bool show_arch_properties_popup;
    float arch_wall_width;
    int arch_num_sides;
    float arch_arc_degrees;
    float arch_start_angle_degrees;
    float arch_add_height;
    Vec3 arch_creation_start_point;
    Vec3 arch_creation_end_point;
    ViewportType arch_creation_view;
    GLuint arch_preview_fbo, arch_preview_texture, arch_preview_rbo;
    int arch_preview_width, arch_preview_height;
#define TEXTURE_TARGET_REPLACE_FIND (10)
#define TEXTURE_TARGET_REPLACE_WITH (11)
#define MODEL_BROWSER_TARGET_SPRINKLE (1)
} EditorState;

#define MAX_RECENT_FILES 10

static void Editor_GroupSelection();
static void Editor_UngroupSelection();
static EditorState g_EditorState;
static Scene* g_CurrentScene;
static Mat4 g_view_matrix[VIEW_COUNT];
static Mat4 g_proj_matrix[VIEW_COUNT];

static BrushFace g_copiedFaceProperties;
static bool g_hasCopiedFace = false;

static const char* logic_entity_classnames[] = { "logic_timer", "math_counter", "logic_random", "logic_relay", "point_servercommand", "logic_compare", "env_blackhole" };
static const int num_logic_entity_classnames = sizeof(logic_entity_classnames) / sizeof(logic_entity_classnames[0]);

static const char* g_env_blackhole_inputs[] = { "Enable", "Disable" };
static const int g_num_env_blackhole_inputs = sizeof(g_env_blackhole_inputs) / sizeof(g_env_blackhole_inputs[0]);

static const char* g_logic_relay_inputs[] = { "Trigger", "Enable", "Disable", "Toggle" };
static const int g_num_logic_relay_inputs = sizeof(g_logic_relay_inputs) / sizeof(g_logic_relay_inputs[0]);
static const char* g_logic_relay_outputs[] = { "OnTrigger" };
static const int g_num_logic_relay_outputs = sizeof(g_logic_relay_outputs) / sizeof(g_logic_relay_outputs[0]);

static const char* g_point_servercommand_inputs[] = { "Command" };
static const int g_num_point_servercommand_inputs = sizeof(g_point_servercommand_inputs) / sizeof(g_point_servercommand_inputs[0]);

static const char* g_logic_compare_inputs[] = { "SetValue", "SetValueCompare", "SetCompareValue", "Compare" };
static const int g_num_logic_compare_inputs = sizeof(g_logic_compare_inputs) / sizeof(g_logic_compare_inputs[0]);
static const char* g_logic_compare_outputs[] = { "OnLessThan", "OnEqualTo", "OnNotEqualTo", "OnGreaterThan" };
static const int g_num_logic_compare_outputs = sizeof(g_logic_compare_outputs) / sizeof(g_logic_compare_outputs[0]);

static const char* g_model_inputs[] = { "EnablePhysics", "DisablePhysics" };
static const int g_num_model_inputs = sizeof(g_model_inputs) / sizeof(g_model_inputs[0]);

static const char* g_brush_trigger_inputs[] = { "Enable", "Disable", "Toggle" };
static const int g_num_brush_trigger_inputs = sizeof(g_brush_trigger_inputs) / sizeof(g_brush_trigger_inputs[0]);

static const char* g_light_inputs[] = { "TurnOn", "TurnOff", "Toggle" };
static const int g_num_light_inputs = sizeof(g_light_inputs) / sizeof(g_light_inputs[0]);

static const char* g_sound_inputs[] = { "PlaySound", "StopSound", "EnableLoop", "DisableLoop", "ToggleLoop" };
static const int g_num_sound_inputs = sizeof(g_sound_inputs) / sizeof(g_sound_inputs[0]);

static const char* g_particle_inputs[] = { "TurnOn", "TurnOff", "Toggle" };
static const int g_num_particle_inputs = sizeof(g_particle_inputs) / sizeof(g_particle_inputs[0]);

static const char* g_video_inputs[] = { "startvideo", "stopvideo", "restartvideo" };
static const int g_num_video_inputs = sizeof(g_video_inputs) / sizeof(g_video_inputs[0]);

static const char* g_sprite_inputs[] = { "TurnOn", "TurnOff", "Toggle" };
static const int g_num_sprite_inputs = sizeof(g_sprite_inputs) / sizeof(g_sprite_inputs[0]);

static const char* g_logic_timer_inputs[] = { "StartTimer", "StopTimer", "ToggleTimer" };
static const int g_num_logic_timer_inputs = sizeof(g_logic_timer_inputs) / sizeof(g_logic_timer_inputs[0]);

static const char* g_math_counter_inputs[] = { "Add", "Subtract", "Multiply", "Divide" };
static const int g_num_math_counter_inputs = sizeof(g_math_counter_inputs) / sizeof(g_math_counter_inputs[0]);

static const char* g_logic_random_inputs[] = { "Enable", "Disable" };
static const int g_num_logic_random_inputs = sizeof(g_logic_random_inputs) / sizeof(g_logic_random_inputs[0]);

static bool FindEntityInScene(Scene* scene, const char* name, EntityType* out_type, int* out_index) {
    if (name == NULL || *name == '\0') return false;
    for (int i = 0; i < scene->numObjects; ++i) if (strcmp(scene->objects[i].targetname, name) == 0) { *out_type = ENTITY_MODEL; *out_index = i; return true; }
    for (int i = 0; i < scene->numBrushes; ++i) if (strcmp(scene->brushes[i].targetname, name) == 0) { *out_type = ENTITY_BRUSH; *out_index = i; return true; }
    for (int i = 0; i < scene->numActiveLights; ++i) if (strcmp(scene->lights[i].targetname, name) == 0) { *out_type = ENTITY_LIGHT; *out_index = i; return true; }
    for (int i = 0; i < scene->numSoundEntities; ++i) if (strcmp(scene->soundEntities[i].targetname, name) == 0) { *out_type = ENTITY_SOUND; *out_index = i; return true; }
    for (int i = 0; i < scene->numParticleEmitters; ++i) if (strcmp(scene->particleEmitters[i].targetname, name) == 0) { *out_type = ENTITY_PARTICLE_EMITTER; *out_index = i; return true; }
    for (int i = 0; i < scene->numVideoPlayers; ++i) if (strcmp(scene->videoPlayers[i].targetname, name) == 0) { *out_type = ENTITY_VIDEO_PLAYER; *out_index = i; return true; }
    for (int i = 0; i < scene->numSprites; ++i) if (strcmp(scene->sprites[i].targetname, name) == 0) { *out_type = ENTITY_SPRITE; *out_index = i; return true; }
    for (int i = 0; i < scene->numLogicEntities; ++i) if (strcmp(scene->logicEntities[i].targetname, name) == 0) { *out_type = ENTITY_LOGIC; *out_index = i; return true; }
    return false;
}

static EditorSelection* Editor_GetPrimarySelection() {
    if (g_EditorState.num_selections == 0) return NULL;
    return &g_EditorState.selections[g_EditorState.num_selections - 1];
}

static void Editor_ClearSelection() {
    g_EditorState.num_selections = 0;
}

static bool Editor_IsSelected(EntityType type, int index) {
    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        if (g_EditorState.selections[i].type == type && g_EditorState.selections[i].index == index) {
            return true;
        }
    }
    return false;
}

static void Editor_RemoveFromSelection(EntityType type, int index) {
    int found_at = -1;
    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        if (g_EditorState.selections[i].type == type && g_EditorState.selections[i].index == index) {
            found_at = i;
            break;
        }
    }
    if (found_at != -1) {
        for (int i = found_at; i < g_EditorState.num_selections - 1; ++i) {
            g_EditorState.selections[i] = g_EditorState.selections[i + 1];
        }
        g_EditorState.num_selections--;
    }
}

static bool Editor_IsFaceSelected(int brush_index, int face_index) {
    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];
        if (sel->type == ENTITY_BRUSH && sel->index == brush_index && sel->face_index == face_index) {
            return true;
        }
    }
    return false;
}

static void Editor_RemoveFaceFromSelection(int brush_index, int face_index) {
    int found_at = -1;
    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];
        if (sel->type == ENTITY_BRUSH && sel->index == brush_index && sel->face_index == face_index) {
            found_at = i;
            break;
        }
    }
    if (found_at != -1) {
        for (int i = found_at; i < g_EditorState.num_selections - 1; ++i) {
            g_EditorState.selections[i] = g_EditorState.selections[i + 1];
        }
        g_EditorState.num_selections--;
    }
}

static void Editor_AddToSelection(EntityType type, int index, int face_index, int vertex_index) {
    g_EditorState.num_selections++;
    g_EditorState.selections = realloc(g_EditorState.selections, g_EditorState.num_selections * sizeof(EditorSelection));
    g_EditorState.selections[g_EditorState.num_selections - 1] = (EditorSelection){ type, index, face_index, vertex_index };
}

static Vec3 ScreenToWorld(Vec2 screen_pos, ViewportType viewport);
static Vec3 ScreenToWorld_Unsnapped_ForOrthoPicking(Vec2 screen_pos, ViewportType viewport);
static Vec3 ScreenToWorld_Clip(Vec2 screen_pos, ViewportType viewport);
float SnapValue(float value, float snap_interval) { if (snap_interval == 0.0f) return value; return roundf(value / snap_interval) * snap_interval; }
float SnapAngle(float value, float snap_interval) { if (snap_interval == 0.0f) return value; return roundf(value / snap_interval) * snap_interval; }
static void Editor_AdjustSelectedBrushByHandle(Scene* scene, Engine* engine, Vec2 mouse_pos, ViewportType view);

void Editor_SubdivideBrushFace(Scene* scene, Engine* engine, int brush_index, int face_index, int u_divs, int v_divs) {
    if (brush_index < 0 || brush_index >= scene->numBrushes) return;
    Brush* b = &scene->brushes[brush_index];
    if (face_index < 0 || face_index >= b->numFaces) return;

    BrushFace* old_face = &b->faces[face_index];
    if (old_face->numVertexIndices != 4) {
        Console_Printf_Error("[error] Can only subdivide 4-sided faces for now.");
        return;
    }

    Undo_BeginEntityModification(scene, ENTITY_BRUSH, brush_index);

    BrushVertex p00 = b->vertices[old_face->vertexIndices[0]];
    BrushVertex p10 = b->vertices[old_face->vertexIndices[1]];
    BrushVertex p11 = b->vertices[old_face->vertexIndices[2]];
    BrushVertex p01 = b->vertices[old_face->vertexIndices[3]];

    int num_new_verts = (u_divs + 1) * (v_divs + 1);
    BrushVertex* new_grid_verts = malloc(num_new_verts * sizeof(BrushVertex));

    for (int v = 0; v <= v_divs; ++v) {
        for (int u = 0; u <= u_divs; ++u) {
            float u_t = (float)u / u_divs;
            float v_t = (float)v / v_divs;

            BrushVertex p_u0 = {
                .pos = vec3_add(vec3_muls(p00.pos, 1.0f - u_t), vec3_muls(p10.pos, u_t)),
                .color = {
                    p00.color.x * (1.0f - u_t) + p10.color.x * u_t,
                    p00.color.y * (1.0f - u_t) + p10.color.y * u_t,
                    p00.color.z * (1.0f - u_t) + p10.color.z * u_t,
                    p00.color.w * (1.0f - u_t) + p10.color.w * u_t
                }
            };
            BrushVertex p_u1 = {
                .pos = vec3_add(vec3_muls(p01.pos, 1.0f - u_t), vec3_muls(p11.pos, u_t)),
                .color = {
                    p01.color.x * (1.0f - u_t) + p11.color.x * u_t,
                    p01.color.y * (1.0f - u_t) + p11.color.y * u_t,
                    p01.color.z * (1.0f - u_t) + p11.color.z * u_t,
                    p01.color.w * (1.0f - u_t) + p11.color.w * u_t
                }
            };

            int index = v * (u_divs + 1) + u;
            new_grid_verts[index].pos = vec3_add(vec3_muls(p_u0.pos, 1.0f - v_t), vec3_muls(p_u1.pos, v_t));
            new_grid_verts[index].color.x = p_u0.color.x * (1.0f - v_t) + p_u1.color.x * v_t;
            new_grid_verts[index].color.y = p_u0.color.y * (1.0f - v_t) + p_u1.color.y * v_t;
            new_grid_verts[index].color.z = p_u0.color.z * (1.0f - v_t) + p_u1.color.z * v_t;
            new_grid_verts[index].color.w = p_u0.color.w * (1.0f - v_t) + p_u1.color.w * v_t;
        }
    }

    int num_new_faces = u_divs * v_divs;
    BrushFace* new_faces = malloc(num_new_faces * sizeof(BrushFace));

    if (b->lightmapAtlas != 0) {
        glDeleteTextures(1, &b->lightmapAtlas);
        b->lightmapAtlas = 0;
    }
    if (b->directionalLightmapAtlas != 0) {
        glDeleteTextures(1, &b->directionalLightmapAtlas);
        b->directionalLightmapAtlas = 0;
    }

    for (int v = 0; v < v_divs; ++v) {
        for (int u = 0; u < u_divs; ++u) {
            int face_idx = v * u_divs + u;
            new_faces[face_idx] = *old_face;

            new_faces[face_idx].atlas_coords = (Vec4){ 0.0f, 0.0f, 0.0f, 0.0f };
            new_faces[face_idx].numVertexIndices = 4;
            new_faces[face_idx].vertexIndices = malloc(4 * sizeof(int));

            new_faces[face_idx].vertexIndices[0] = v * (u_divs + 1) + u;
            new_faces[face_idx].vertexIndices[1] = v * (u_divs + 1) + (u + 1);
            new_faces[face_idx].vertexIndices[2] = (v + 1) * (u_divs + 1) + (u + 1);
            new_faces[face_idx].vertexIndices[3] = (v + 1) * (u_divs + 1) + u;
        }
    }

    free(b->faces[face_index].vertexIndices);
    for (int i = face_index; i < b->numFaces - 1; ++i) {
        b->faces[i] = b->faces[i + 1];
    }
    b->numFaces--;

    int old_vert_count = b->numVertices;
    b->vertices = realloc(b->vertices, (old_vert_count + num_new_verts) * sizeof(BrushVertex));
    int old_face_count = b->numFaces;
    b->faces = realloc(b->faces, (old_face_count + num_new_faces) * sizeof(BrushFace));

    for (int i = 0; i < num_new_verts; ++i) {
        b->vertices[old_vert_count + i] = new_grid_verts[i];
    }
    for (int i = 0; i < num_new_faces; ++i) {
        for (int j = 0; j < 4; ++j) {
            new_faces[i].vertexIndices[j] += old_vert_count;
        }
        b->faces[old_face_count + i] = new_faces[i];
    }

    b->numVertices += num_new_verts;
    b->numFaces += num_new_faces;

    free(new_grid_verts);
    char group_name[64];
    snprintf(group_name, sizeof(group_name), "subdiv_group_%d", g_EditorState.next_group_id++);
    for (int i = old_face_count; i < b->numFaces; ++i) {
        b->faces[i].isGrouped = true;
        strncpy(b->faces[i].groupName, group_name, sizeof(b->faces[i].groupName) - 1);
        b->faces[i].groupName[sizeof(b->faces[i].groupName) - 1] = '\0';
    }
    free(new_faces);

    Brush_CreateRenderData(b);
    if (b->physicsBody) {
        Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
        for (int i = 0; i < b->numVertices; ++i) {
            world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
        }
        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
        free(world_verts);
    }

    Undo_EndEntityModification(scene, ENTITY_BRUSH, brush_index, "Subdivide Face");
    Console_Printf("Subdivided face %d of brush %d.", face_index, brush_index);
}

static void Editor_SaveRecentFiles() {
    FILE* file = fopen("editor_prefs.cfg", "w");
    if (!file) return;
    for (int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
        fprintf(file, "%s\n", g_EditorState.recent_map_files[i]);
    }
    fclose(file);
}

static void Editor_LoadRecentFiles() {
    FILE* file = fopen("editor_prefs.cfg", "r");
    if (!file) return;

    char line[256];
    while (fgets(line, sizeof(line), file) && g_EditorState.num_recent_map_files < MAX_RECENT_FILES) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) > 0) {
            g_EditorState.recent_map_files = realloc(g_EditorState.recent_map_files, (g_EditorState.num_recent_map_files + 1) * sizeof(char*));
            g_EditorState.recent_map_files[g_EditorState.num_recent_map_files] = _strdup(line);
            g_EditorState.num_recent_map_files++;
        }
    }
    fclose(file);
}

static void Editor_AddRecentFile(const char* path) {
    for (int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
        if (strcmp(g_EditorState.recent_map_files[i], path) == 0) {
            free(g_EditorState.recent_map_files[i]);
            for (int j = i; j < g_EditorState.num_recent_map_files - 1; ++j) {
                g_EditorState.recent_map_files[j] = g_EditorState.recent_map_files[j + 1];
            }
            g_EditorState.num_recent_map_files--;
            break;
        }
    }

    if (g_EditorState.num_recent_map_files >= MAX_RECENT_FILES) {
        free(g_EditorState.recent_map_files[MAX_RECENT_FILES - 1]);
        g_EditorState.num_recent_map_files = MAX_RECENT_FILES - 1;
    }
    g_EditorState.recent_map_files = realloc(g_EditorState.recent_map_files, (g_EditorState.num_recent_map_files + 1) * sizeof(char*));
    for (int i = g_EditorState.num_recent_map_files; i > 0; --i) {
        g_EditorState.recent_map_files[i] = g_EditorState.recent_map_files[i - 1];
    }

    g_EditorState.recent_map_files[0] = _strdup(path);
    g_EditorState.num_recent_map_files++;

    Editor_SaveRecentFiles();
}

static void FreeModelBrowserEntries() {
    if (g_EditorState.model_browser_entries) {
        for (int i = 0; i < g_EditorState.num_model_files; ++i) {
            free(g_EditorState.model_browser_entries[i].file_path);
            if (g_EditorState.model_browser_entries[i].thumbnail_texture != 0) {
                glDeleteTextures(1, &g_EditorState.model_browser_entries[i].thumbnail_texture);
            }
        }
        free(g_EditorState.model_browser_entries);
        g_EditorState.model_browser_entries = NULL;
        g_EditorState.num_model_files = 0;
    }
}
static void ScanModelFiles() {
    FreeModelBrowserEntries();
    const char* dir_path = "models/";
#ifdef PLATFORM_WINDOWS
    char search_path[256];
    sprintf(search_path, "%s*.gltf", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            g_EditorState.model_browser_entries = realloc(g_EditorState.model_browser_entries, (g_EditorState.num_model_files + 1) * sizeof(ModelBrowserEntry));
            g_EditorState.model_browser_entries[g_EditorState.num_model_files].file_path = _strdup(find_data.cFileName);
            g_EditorState.model_browser_entries[g_EditorState.num_model_files].thumbnail_texture = 0;
            g_EditorState.num_model_files++;
        }
    } while (FindNextFileA(h_find, &find_data) != 0);
    FindClose(h_find);
#else
    DIR* d = opendir(dir_path);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        const char* ext = strrchr(dir->d_name, '.');
        if (ext && (_stricmp(ext, ".gltf") == 0 || _stricmp(ext, ".glb") == 0)) {
            g_EditorState.model_browser_entries = realloc(g_EditorState.model_browser_entries, (g_EditorState.num_model_files + 1) * sizeof(ModelBrowserEntry));
            g_EditorState.model_browser_entries[g_EditorState.num_model_files].file_path = strdup(dir->d_name);
            g_EditorState.model_browser_entries[g_EditorState.num_model_files].thumbnail_texture = 0;
            g_EditorState.num_model_files++;
        }
    }
    closedir(d);
#endif
}
static void FreeDocFileList() {
    if (g_EditorState.doc_files) {
        for (int i = 0; i < g_EditorState.num_doc_files; ++i) {
            free(g_EditorState.doc_files[i]);
        }
        free(g_EditorState.doc_files);
        g_EditorState.doc_files = NULL;
        g_EditorState.num_doc_files = 0;
    }
}

static void ScanDocFiles() {
    FreeDocFileList();
    const char* dir_path = "docs/";
#ifdef PLATFORM_WINDOWS
    char search_path[256];
    sprintf(search_path, "%s*.md", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            g_EditorState.doc_files = realloc(g_EditorState.doc_files, (g_EditorState.num_doc_files + 1) * sizeof(char*));
            g_EditorState.doc_files[g_EditorState.num_doc_files] = _strdup(find_data.cFileName);
            g_EditorState.num_doc_files++;
        }
    } while (FindNextFileA(h_find, &find_data) != 0);
    FindClose(h_find);
#else
    DIR* d = opendir(dir_path);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        const char* ext = strrchr(dir->d_name, '.');
        if (ext && (_stricmp(ext, ".md") == 0)) {
            g_EditorState.doc_files = realloc(g_EditorState.doc_files, (g_EditorState.num_doc_files + 1) * sizeof(char*));
            g_EditorState.doc_files[g_EditorState.num_doc_files] = strdup(dir->d_name);
            g_EditorState.num_doc_files++;
        }
    }
    closedir(d);
#endif
}
static void FreeSoundFileList() {
    if (g_EditorState.sound_file_list) {
        for (int i = 0; i < g_EditorState.num_sound_files; ++i) {
            free(g_EditorState.sound_file_list[i]);
        }
        free(g_EditorState.sound_file_list);
        g_EditorState.sound_file_list = NULL;
        g_EditorState.num_sound_files = 0;
    }
}

static void ScanSoundFiles() {
    FreeSoundFileList();
    const char* dir_path = "sounds/";
#ifdef PLATFORM_WINDOWS
    char search_path[256];
    sprintf(search_path, "%s*.*", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            const char* ext = strrchr(find_data.cFileName, '.');
            if (ext && (_stricmp(ext, ".wav") == 0 || _stricmp(ext, ".mp3") == 0 || _stricmp(ext, ".ogg") == 0)) {
                g_EditorState.sound_file_list = realloc(g_EditorState.sound_file_list, (g_EditorState.num_sound_files + 1) * sizeof(char*));
                g_EditorState.sound_file_list[g_EditorState.num_sound_files] = _strdup(find_data.cFileName);
                g_EditorState.num_sound_files++;
            }
        }
    } while (FindNextFileA(h_find, &find_data) != 0);
    FindClose(h_find);
#else
    DIR* d = opendir(dir_path);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        const char* ext = strrchr(dir->d_name, '.');
        if (ext && (_stricmp(ext, ".wav") == 0 || _stricmp(ext, ".mp3") == 0 || _stricmp(ext, ".ogg") == 0)) {
            g_EditorState.sound_file_list = realloc(g_EditorState.sound_file_list, (g_EditorState.num_sound_files + 1) * sizeof(char*));
            g_EditorState.sound_file_list[g_EditorState.num_sound_files] = strdup(dir->d_name);
            g_EditorState.num_sound_files++;
        }
    }
    closedir(d);
#endif
}

static void FreeMapFileList() {
    if (g_EditorState.map_file_list) {
        for (int i = 0; i < g_EditorState.num_map_files; ++i) {
            free(g_EditorState.map_file_list[i]);
        }
        free(g_EditorState.map_file_list);
        g_EditorState.map_file_list = NULL;
        g_EditorState.num_map_files = 0;
    }
}

static void ScanMapFiles() {
    FreeMapFileList();
    const char* dir_path = "./";
#ifdef PLATFORM_WINDOWS
    char search_path[256];
    sprintf(search_path, "%s*.map", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            g_EditorState.map_file_list = realloc(g_EditorState.map_file_list, (g_EditorState.num_map_files + 1) * sizeof(char*));
            g_EditorState.map_file_list[g_EditorState.num_map_files] = _strdup(find_data.cFileName);
            g_EditorState.num_map_files++;
        }
    } while (FindNextFileA(h_find, &find_data) != 0);
    FindClose(h_find);
#else
    DIR* d = opendir(dir_path);
    if (!d) return;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        const char* ext = strrchr(dir->d_name, '.');
        if (ext && (_stricmp(ext, ".map") == 0)) {
            g_EditorState.map_file_list = realloc(g_EditorState.map_file_list, (g_EditorState.num_map_files + 1) * sizeof(char*));
            g_EditorState.map_file_list[g_EditorState.num_map_files] = strdup(dir->d_name);
            g_EditorState.num_map_files++;
        }
    }
    closedir(d);
#endif
}

static void Editor_CreateBrushFromPreview(Scene* scene, Engine* engine, Brush* preview) {
    if (scene->numBrushes >= MAX_BRUSHES) { return; }
    Brush* b = &scene->brushes[scene->numBrushes];
    memset(b, 0, sizeof(Brush));
    Brush_DeepCopy(b, preview);
    b->vao = 0; b->vbo = 0;
    b->mass = 0.0f;
    b->isPhysicsEnabled = true;
    b->isReflectionProbe = false; b->isTrigger = false; b->physicsBody = NULL;
    Brush_UpdateMatrix(b); Brush_CreateRenderData(b);
    if (!b->isTrigger && !b->isWater && b->numVertices > 0) {
        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
        for (int i = 0; i < b->numVertices; i++) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
        free(world_verts);
    }
    int new_brush_index = scene->numBrushes;
    scene->numBrushes++;
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_BRUSH, new_brush_index, 0, 0);
    Undo_PushCreateEntity(scene, ENTITY_BRUSH, new_brush_index, "Create Brush");
}

void Editor_DuplicateModel(Scene* scene, Engine* engine, int index) {
    if (index < 0 || index >= scene->numObjects) return;
    if (scene->numObjects >= MAX_MODELS) {
        return;
    }
    SceneObject* src_obj = &scene->objects[index];
    scene->numObjects++;
    scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
    SceneObject* new_obj = &scene->objects[scene->numObjects - 1];
    memcpy(new_obj, src_obj, sizeof(SceneObject));
    sprintf(new_obj->targetname, "Model_%d", scene->numObjects - 1);
    new_obj->physicsBody = NULL;
    new_obj->pos.x += 1.0f;
    SceneObject_UpdateMatrix(new_obj);
    new_obj->model = Model_Load(new_obj->modelPath);
    if (new_obj->model && new_obj->model->combinedVertexData && new_obj->model->totalIndexCount > 0) {
        Mat4 physics_transform = create_trs_matrix(new_obj->pos, new_obj->rot, (Vec3) { 1, 1, 1 });
        new_obj->physicsBody = Physics_CreateStaticTriangleMesh(engine->physicsWorld, new_obj->model->combinedVertexData, new_obj->model->totalVertexCount, new_obj->model->combinedIndexData, new_obj->model->totalIndexCount, physics_transform, new_obj->scale);
    }
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_MODEL, scene->numObjects - 1, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_MODEL, scene->numObjects - 1, "Duplicate Model");
}

void Editor_DuplicateBrush(Scene* scene, Engine* engine, int index) {
    if (index < 0 || index >= scene->numBrushes || scene->numBrushes >= MAX_BRUSHES) return;
    Brush* src_brush = &scene->brushes[index];
    Brush* new_brush = &scene->brushes[scene->numBrushes];
    Brush_DeepCopy(new_brush, src_brush);
    sprintf(new_brush->targetname, "Brush_%d", scene->numBrushes);
    new_brush->pos.x += 1.0f;
    Brush_UpdateMatrix(new_brush);
    Brush_CreateRenderData(new_brush);
    if (!new_brush->isTrigger && !new_brush->isReflectionProbe && !new_brush->isWater && new_brush->numVertices > 0) {
        if (new_brush->mass > 0.0f) {
            new_brush->physicsBody = Physics_CreateDynamicBrush(engine->physicsWorld, (const float*)new_brush->vertices, new_brush->numVertices, new_brush->mass, new_brush->modelMatrix);
            if (!new_brush->isPhysicsEnabled) {
                Physics_ToggleCollision(engine->physicsWorld, new_brush->physicsBody, false);
            }
        }
        else {
            Vec3* world_verts = malloc(new_brush->numVertices * sizeof(Vec3));
            for (int i = 0; i < new_brush->numVertices; i++) world_verts[i] = mat4_mul_vec3(&new_brush->modelMatrix, new_brush->vertices[i].pos);
            new_brush->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, new_brush->numVertices);
            free(world_verts);
        }
    }
    int new_brush_index = scene->numBrushes;
    scene->numBrushes++;
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_BRUSH, new_brush_index, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_BRUSH, new_brush_index, "Duplicate Brush");
}

void Editor_DuplicateLight(Scene* scene, int index) {
    if (index < 0 || index >= scene->numActiveLights || scene->numActiveLights >= MAX_LIGHTS) return;
    Light* src_light = &scene->lights[index];
    Light* new_light = &scene->lights[scene->numActiveLights];
    memcpy(new_light, src_light, sizeof(Light));
    sprintf(new_light->targetname, "Light_%d", scene->numActiveLights);
    new_light->shadowFBO = 0; new_light->shadowMapTexture = 0;
    new_light->position.x += 1.0f;
    Light_InitShadowMap(new_light);
    int new_light_index = scene->numActiveLights;
    scene->numActiveLights++;
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_LIGHT, new_light_index, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_LIGHT, new_light_index, "Duplicate Light");
}

void Editor_DuplicateDecal(Scene* scene, int index) {
    if (index < 0 || index >= scene->numDecals || scene->numDecals >= MAX_DECALS) return;
    Decal* src_decal = &scene->decals[index];
    Decal* new_decal = &scene->decals[scene->numDecals];
    memcpy(new_decal, src_decal, sizeof(Decal));
    sprintf(new_decal->targetname, "Decal_%d", scene->numDecals);
    new_decal->pos.x += 1.0f;
    Decal_UpdateMatrix(new_decal);
    int new_decal_index = scene->numDecals;
    scene->numDecals++;
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_DECAL, new_decal_index, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_DECAL, new_decal_index, "Duplicate Decal");
}

void Editor_DuplicateSoundEntity(Scene* scene, int index) {
    if (index < 0 || index >= scene->numSoundEntities || scene->numSoundEntities >= MAX_SOUNDS) return;
    SoundEntity* src_sound = &scene->soundEntities[index];
    SoundEntity* new_sound = &scene->soundEntities[scene->numSoundEntities];
    memcpy(new_sound, src_sound, sizeof(SoundEntity));
    sprintf(new_sound->targetname, "Sound_%d", scene->numSoundEntities);
    new_sound->sourceID = 0; new_sound->bufferID = 0;
    new_sound->pos.x += 1.0f;
    new_sound->bufferID = SoundSystem_LoadSound(new_sound->soundPath);
    int new_sound_index = scene->numSoundEntities;
    scene->numSoundEntities++;
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_SOUND, new_sound_index, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_SOUND, new_sound_index, "Duplicate Sound");
}

void Editor_DuplicateParticleEmitter(Scene* scene, int index) {
    if (index < 0 || index >= scene->numParticleEmitters || scene->numParticleEmitters >= MAX_PARTICLE_EMITTERS) return;
    ParticleEmitter* src_emitter = &scene->particleEmitters[index];
    ParticleEmitter* new_emitter = &scene->particleEmitters[scene->numParticleEmitters];
    memcpy(new_emitter, src_emitter, sizeof(ParticleEmitter));
    sprintf(new_emitter->targetname, "Emitter_%d", scene->numParticleEmitters);
    new_emitter->pos.x += 1.0f;
    ParticleSystem* ps = ParticleSystem_Load(new_emitter->parFile);
    if (ps) {
        int new_emitter_index = scene->numParticleEmitters;
        ParticleEmitter_Init(new_emitter, ps, new_emitter->pos);
        scene->numParticleEmitters++;
        Editor_ClearSelection();
        Editor_AddToSelection(ENTITY_PARTICLE_EMITTER, new_emitter_index, -1, -1);
        Undo_PushCreateEntity(scene, ENTITY_PARTICLE_EMITTER, new_emitter_index, "Duplicate Emitter");
    }
}

void Editor_DuplicateVideoPlayer(Scene* scene, int index) {
    if (index < 0 || index >= scene->numVideoPlayers || scene->numVideoPlayers >= MAX_VIDEO_PLAYERS) return;
    VideoPlayer* src_vp = &scene->videoPlayers[index];
    VideoPlayer* new_vp = &scene->videoPlayers[scene->numVideoPlayers];
    memcpy(new_vp, src_vp, sizeof(VideoPlayer));
    sprintf(new_vp->targetname, "Video_%d", scene->numVideoPlayers);
    new_vp->plm = NULL;
    new_vp->textureID = 0;
    new_vp->audioSource = 0;
    new_vp->pos.x += 1.0f;
    VideoPlayer_Load(new_vp);
    if (new_vp->playOnStart) {
        VideoPlayer_Play(new_vp);
    }
    int new_vp_index = scene->numVideoPlayers;
    scene->numVideoPlayers++;
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_VIDEO_PLAYER, new_vp_index, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_VIDEO_PLAYER, new_vp_index, "Duplicate Video Player");
}

void Editor_DuplicateParallaxRoom(Scene* scene, int index) {
    if (index < 0 || index >= scene->numParallaxRooms || scene->numParallaxRooms >= MAX_PARALLAX_ROOMS) return;
    ParallaxRoom* src_p = &scene->parallaxRooms[index];
    ParallaxRoom* new_p = &scene->parallaxRooms[scene->numParallaxRooms];
    memcpy(new_p, src_p, sizeof(ParallaxRoom));
    sprintf(new_p->targetname, "Parallax_%d", scene->numParallaxRooms);
    new_p->pos.x += 1.0f;
    ParallaxRoom_UpdateMatrix(new_p);

    const char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
    char face_paths[6][256];
    const char* face_pointers[6];
    for (int i = 0; i < 6; ++i) {
        sprintf(face_paths[i], "%s%s", new_p->cubemapPath, suffixes[i]);
        face_pointers[i] = face_paths[i];
    }
    new_p->cubemapTexture = loadCubemap(face_pointers);
    int new_p_index = scene->numParallaxRooms;
    scene->numParallaxRooms++;
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_PARALLAX_ROOM, new_p_index, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_PARALLAX_ROOM, new_p_index, "Duplicate Parallax Room");
}
void Editor_DuplicateLogicEntity(Scene* scene, Engine* engine, int index) {
    if (index < 0 || index >= scene->numLogicEntities || scene->numLogicEntities >= MAX_LOGIC_ENTITIES) return;
    LogicEntity* src_ent = &scene->logicEntities[index];
    LogicEntity* new_ent = &scene->logicEntities[scene->numLogicEntities];
    memcpy(new_ent, src_ent, sizeof(LogicEntity));
    sprintf(new_ent->targetname, "%s_%d", src_ent->classname, scene->numLogicEntities);
    new_ent->pos.x += 1.0f;
    int new_ent_index = scene->numLogicEntities;
    scene->numLogicEntities++;
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_LOGIC, new_ent_index, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_LOGIC, new_ent_index, "Duplicate Logic Entity");
}

void Editor_DuplicateSprite(Scene* scene, int index) {
    if (index < 0 || index >= scene->numSprites || scene->numSprites >= MAX_DECALS) return;
    Sprite* src_sprite = &scene->sprites[index];
    Sprite* new_sprite = &scene->sprites[scene->numSprites];
    memcpy(new_sprite, src_sprite, sizeof(Sprite));
    sprintf(new_sprite->targetname, "Sprite_%d", scene->numSprites);
    new_sprite->pos.x += 1.0f;
    int new_sprite_index = scene->numSprites;
    scene->numSprites++;
    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_SPRITE, new_sprite_index, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_SPRITE, new_sprite_index, "Duplicate Sprite");
}

static void Editor_UpdatePreviewBrushFromWorldMinMax() {
    Brush* b = &g_EditorState.preview_brush;

    Vec3 world_min = g_EditorState.preview_brush_world_min;
    Vec3 world_max = g_EditorState.preview_brush_world_max;

    if (world_min.x > world_max.x) { float t = world_min.x; world_min.x = world_max.x; world_max.x = t; }
    if (world_min.y > world_max.y) { float t = world_min.y; world_min.y = world_max.y; world_max.y = t; }
    if (world_min.z > world_max.z) { float t = world_min.z; world_min.z = world_max.z; world_max.z = t; }

    Vec3 size = vec3_sub(world_max, world_min);
    const float min_dim = 0.01f;
    if (size.x < min_dim) size.x = min_dim;
    if (size.y < min_dim) size.y = min_dim;
    if (size.z < min_dim) size.z = min_dim;

    g_EditorState.preview_brush_world_min = world_min;
    g_EditorState.preview_brush_world_max = vec3_add(world_min, size);

    b->pos = vec3_muls(vec3_add(g_EditorState.preview_brush_world_min, g_EditorState.preview_brush_world_max), 0.5f);
    b->rot = (Vec3){ 0,0,0 };
    b->scale = (Vec3){ 1,1,1 };

    Vec3 local_size = vec3_sub(g_EditorState.preview_brush_world_max, g_EditorState.preview_brush_world_min);
    switch (g_EditorState.current_brush_shape) {
    case BRUSH_SHAPE_BLOCK:
        Brush_SetVerticesFromBox(b, local_size);
        break;
    case BRUSH_SHAPE_CYLINDER:
        Brush_SetVerticesFromCylinder(b, local_size, g_EditorState.cylinder_creation_steps);
        break;
    case BRUSH_SHAPE_WEDGE:
        Brush_SetVerticesFromWedge(b, local_size);
        break;
    case BRUSH_SHAPE_SPIKE:
        Brush_SetVerticesFromSpike(b, local_size, g_EditorState.cylinder_creation_steps);
        break;
    case BRUSH_SHAPE_SPHERE:
        Brush_SetVerticesFromSphere(b, local_size, g_EditorState.cylinder_creation_steps);
        break;
    case BRUSH_SHAPE_SEMI_SPHERE:
        Brush_SetVerticesFromSemiSphere(b, local_size, g_EditorState.cylinder_creation_steps);
        break;
    }
    Brush_UpdateMatrix(b);
    Brush_CreateRenderData(b);
}

static void Editor_SetDefaultLogicProperties(LogicEntity* ent) {
    if (!ent) return;

    ent->numProperties = 0;

    if (strcmp(ent->classname, "logic_timer") == 0) {
        ent->numProperties = 1;
        strcpy(ent->properties[0].key, "delay");
        strcpy(ent->properties[0].value, "1.0");
    }
    else if (strcmp(ent->classname, "math_counter") == 0) {
        ent->numProperties = 2;
        strcpy(ent->properties[0].key, "min");
        strcpy(ent->properties[0].value, "0");
        strcpy(ent->properties[1].key, "max");
        strcpy(ent->properties[1].value, "10");
    }
    else if (strcmp(ent->classname, "logic_random") == 0) {
        ent->numProperties = 2;
        strcpy(ent->properties[0].key, "min_time");
        strcpy(ent->properties[0].value, "1.0");
        strcpy(ent->properties[1].key, "max_time");
        strcpy(ent->properties[1].value, "5.0");
    }
    else if (strcmp(ent->classname, "logic_compare") == 0) {
        ent->numProperties = 2;
        strcpy(ent->properties[0].key, "InitialValue");
        strcpy(ent->properties[0].value, "0");
        strcpy(ent->properties[1].key, "CompareValue");
        strcpy(ent->properties[1].value, "0");
    }
    else if (strcmp(ent->classname, "env_blackhole") == 0) {
        ent->numProperties = 3;
        strcpy(ent->properties[0].key, "rotationspeed");
        strcpy(ent->properties[0].value, "10.0");
        strcpy(ent->properties[1].key, "scale");
        strcpy(ent->properties[1].value, "1.0");
        strcpy(ent->properties[2].key, "starton");
        strcpy(ent->properties[2].value, "1");
    }
}

static void Editor_UpdatePreviewBrushForInitialDrag(Vec3 p1_world_drag, Vec3 p2_world_drag, ViewportType creation_view) {
    Vec3 world_min, world_max;

    if (creation_view == VIEW_TOP_XZ) {
        world_min.x = fminf(p1_world_drag.x, p2_world_drag.x);
        world_max.x = fmaxf(p1_world_drag.x, p2_world_drag.x);
        world_min.z = fminf(p1_world_drag.z, p2_world_drag.z);
        world_max.z = fmaxf(p1_world_drag.z, p2_world_drag.z);
        float half_depth = g_EditorState.grid_size * 0.5f;
        float center_y = g_EditorState.brush_creation_start_point_2d_drag.y;
        world_min.y = center_y;
        world_max.y = center_y + g_EditorState.grid_size;
        if (g_EditorState.snap_to_grid) {
            world_min.y = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.y, g_EditorState.grid_size);
            world_max.y = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.y + g_EditorState.grid_size, g_EditorState.grid_size);
        }
        else {
            world_min.y = g_EditorState.brush_creation_start_point_2d_drag.y;
            world_max.y = g_EditorState.brush_creation_start_point_2d_drag.y + g_EditorState.grid_size;
        }
    }
    else if (creation_view == VIEW_FRONT_XY) {
        world_min.x = fminf(p1_world_drag.x, p2_world_drag.x);
        world_max.x = fmaxf(p1_world_drag.x, p2_world_drag.x);
        world_min.y = fminf(p1_world_drag.y, p2_world_drag.y);
        world_max.y = fmaxf(p1_world_drag.y, p2_world_drag.y);

        if (g_EditorState.snap_to_grid) {
            world_min.z = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.z, g_EditorState.grid_size);
            world_max.z = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.z + g_EditorState.grid_size, g_EditorState.grid_size);
        }
        else {
            world_min.z = g_EditorState.brush_creation_start_point_2d_drag.z;
            world_max.z = g_EditorState.brush_creation_start_point_2d_drag.z + g_EditorState.grid_size;
        }

    }
    else if (creation_view == VIEW_SIDE_YZ) {
        world_min.y = fminf(p1_world_drag.y, p2_world_drag.y);
        world_max.y = fmaxf(p1_world_drag.y, p2_world_drag.y);
        world_min.z = fminf(p1_world_drag.z, p2_world_drag.z);
        world_max.z = fmaxf(p1_world_drag.z, p2_world_drag.z);

        if (g_EditorState.snap_to_grid) {
            world_min.x = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.x, g_EditorState.grid_size);
            world_max.x = SnapValue(g_EditorState.brush_creation_start_point_2d_drag.x + g_EditorState.grid_size, g_EditorState.grid_size);
        }
        else {
            world_min.x = g_EditorState.brush_creation_start_point_2d_drag.x;
            world_max.x = g_EditorState.brush_creation_start_point_2d_drag.x + g_EditorState.grid_size;
        }
    }

    g_EditorState.preview_brush_world_min = world_min;
    g_EditorState.preview_brush_world_max = world_max;
    Editor_UpdatePreviewBrushFromWorldMinMax();
}

static void Editor_AdjustPreviewBrushByHandle(Vec2 mouse_pos_in_viewport, ViewportType current_view) {
    if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_NONE) return;
    if (current_view != g_EditorState.preview_brush_drag_handle_view) return;

    Vec3 mouse_world_raw = ScreenToWorld_Unsnapped_ForOrthoPicking(mouse_pos_in_viewport, current_view);
    Vec3 mouse_world_snapped = mouse_world_raw;

    if (g_EditorState.snap_to_grid) {
        mouse_world_snapped.x = SnapValue(mouse_world_raw.x, g_EditorState.grid_size);
        mouse_world_snapped.y = SnapValue(mouse_world_raw.y, g_EditorState.grid_size);
        mouse_world_snapped.z = SnapValue(mouse_world_raw.z, g_EditorState.grid_size);
    }

    switch (g_EditorState.preview_brush_active_handle) {
    case PREVIEW_BRUSH_HANDLE_MIN_X:
        if (current_view == VIEW_TOP_XZ || current_view == VIEW_FRONT_XY) {
            g_EditorState.preview_brush_world_min.x = mouse_world_snapped.x;
        }
        break;
    case PREVIEW_BRUSH_HANDLE_MAX_X:
        if (current_view == VIEW_TOP_XZ || current_view == VIEW_FRONT_XY) {
            g_EditorState.preview_brush_world_max.x = mouse_world_snapped.x;
        }
        break;
    case PREVIEW_BRUSH_HANDLE_MIN_Y:
        if (current_view == VIEW_FRONT_XY || current_view == VIEW_SIDE_YZ) {
            g_EditorState.preview_brush_world_min.y = mouse_world_snapped.y;
        }
        break;
    case PREVIEW_BRUSH_HANDLE_MAX_Y:
        if (current_view == VIEW_FRONT_XY || current_view == VIEW_SIDE_YZ) {
            g_EditorState.preview_brush_world_max.y = mouse_world_snapped.y;
        }
        break;
    case PREVIEW_BRUSH_HANDLE_MIN_Z:
        if (current_view == VIEW_TOP_XZ || current_view == VIEW_SIDE_YZ) {
            g_EditorState.preview_brush_world_min.z = mouse_world_snapped.z;
        }
        break;
    case PREVIEW_BRUSH_HANDLE_MAX_Z:
        if (current_view == VIEW_TOP_XZ || current_view == VIEW_SIDE_YZ) {
            g_EditorState.preview_brush_world_max.z = mouse_world_snapped.z;
        }
        break;
    default:
        break;
    }

    Vec3 temp_min = g_EditorState.preview_brush_world_min;
    Vec3 temp_max = g_EditorState.preview_brush_world_max;

    if (temp_min.x > temp_max.x) { float t = temp_min.x; temp_min.x = temp_max.x; temp_max.x = t; }
    if (temp_min.y > temp_max.y) { float t = temp_min.y; temp_min.y = temp_max.y; temp_max.y = t; }
    if (temp_min.z > temp_max.z) { float t = temp_min.z; temp_min.z = temp_max.z; temp_max.z = t; }

    const float min_brush_dim = 0.01f;
    if (temp_max.x - temp_min.x < min_brush_dim) {
        if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MIN_X) temp_min.x = temp_max.x - min_brush_dim;
        else if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MAX_X) temp_max.x = temp_min.x + min_brush_dim;
        else if (temp_max.x - temp_min.x < min_brush_dim) temp_max.x = temp_min.x + min_brush_dim;
    }
    if (temp_max.y - temp_min.y < min_brush_dim) {
        if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MIN_Y) temp_min.y = temp_max.y - min_brush_dim;
        else if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MAX_Y) temp_max.y = temp_min.y + min_brush_dim;
        else if (temp_max.y - temp_min.y < min_brush_dim) temp_max.y = temp_min.y + min_brush_dim;
    }
    if (temp_max.z - temp_min.z < min_brush_dim) {
        if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MIN_Z) temp_min.z = temp_max.z - min_brush_dim;
        else if (g_EditorState.preview_brush_active_handle == PREVIEW_BRUSH_HANDLE_MAX_Z) temp_max.z = temp_min.z + min_brush_dim;
        else if (temp_max.z - temp_min.z < min_brush_dim) temp_max.z = temp_min.z + min_brush_dim;
    }

    g_EditorState.preview_brush_world_min = temp_min;
    g_EditorState.preview_brush_world_max = temp_max;

    Editor_UpdatePreviewBrushFromWorldMinMax();
}
static void Editor_AdjustPreviewBrush(Vec2 mouse_pos, ViewportType adjust_view) {
    Brush* b = &g_EditorState.preview_brush;
    Vec3 p_current = ScreenToWorld(mouse_pos, adjust_view);
    Vec3 min_v = { FLT_MAX, FLT_MAX, FLT_MAX }; Vec3 max_v = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (int i = 0; i < 8; ++i) {
        min_v.x = fminf(min_v.x, b->vertices[i].pos.x); min_v.y = fminf(min_v.y, b->vertices[i].pos.y); min_v.z = fminf(min_v.z, b->vertices[i].pos.z);
        max_v.x = fmaxf(max_v.x, b->vertices[i].pos.x); max_v.y = fmaxf(max_v.y, b->vertices[i].pos.y); max_v.z = fmaxf(max_v.z, b->vertices[i].pos.z);
    }
    Vec3 size = { max_v.x - min_v.x, max_v.y - min_v.y, max_v.z - min_v.z };
    if (g_EditorState.brush_creation_view == VIEW_TOP_XZ) {
        if (adjust_view == VIEW_FRONT_XY || adjust_view == VIEW_SIDE_YZ) { size.y = fabsf(p_current.y); b->pos.y = p_current.y / 2.0f; }
    }
    else if (g_EditorState.brush_creation_view == VIEW_FRONT_XY) {
        if (adjust_view == VIEW_TOP_XZ || adjust_view == VIEW_SIDE_YZ) { size.z = fabsf(p_current.z); b->pos.z = p_current.z / 2.0f; }
    }
    else if (g_EditorState.brush_creation_view == VIEW_SIDE_YZ) {
        if (adjust_view == VIEW_TOP_XZ || adjust_view == VIEW_FRONT_XY) { size.x = fabsf(p_current.x); b->pos.x = p_current.x / 2.0f; }
    }

    if (g_EditorState.snap_to_grid) {
        size.x = SnapValue(size.x, g_EditorState.grid_size);
        size.y = SnapValue(size.y, g_EditorState.grid_size);
        size.z = SnapValue(size.z, g_EditorState.grid_size);
        b->pos.x = SnapValue(b->pos.x, g_EditorState.grid_size * 0.5f);
        b->pos.y = SnapValue(b->pos.y, g_EditorState.grid_size * 0.5f);
        b->pos.z = SnapValue(b->pos.z, g_EditorState.grid_size * 0.5f);
    }

    if (size.x < 0.01f) size.x = 0.01f;
    if (size.y < 0.01f) size.y = 0.01f;
    if (size.z < 0.01f) size.z = 0.01f;
    Brush_SetVerticesFromBox(b, size); Brush_UpdateMatrix(b); Brush_CreateRenderData(b);
}
static void Editor_InitGizmo() {
    g_EditorState.gizmo_shader = createShaderProgram("shaders/gizmo.vert", "shaders/gizmo.frag");
    const float gizmo_arrow_length = 1.0f;
    const float gizmo_vertices[] = {
        0.0f, 0.0f, 0.0f, gizmo_arrow_length, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, gizmo_arrow_length, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, gizmo_arrow_length,
    };
    glGenVertexArrays(1, &g_EditorState.gizmo_vao);
    glGenBuffers(1, &g_EditorState.gizmo_vbo);
    glBindVertexArray(g_EditorState.gizmo_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.gizmo_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gizmo_vertices), gizmo_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}
void Editor_InitDebugRenderer() {
    g_EditorState.debug_shader = createShaderProgram("shaders/debug.vert", "shaders/debug.frag");
    float radius = 0.25f; float sphere_lines[24 * 3 * 2 * 3]; int index = 0;
    for (int i = 0; i < 24; ++i) { float a1 = (i / 24.0f) * 2.0f * M_PI; float a2 = ((i + 1) / 24.0f) * 2.0f * M_PI; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = radius * sinf(a2); sphere_lines[index++] = 0.0f; }
    for (int i = 0; i < 24; ++i) { float a1 = (i / 24.0f) * 2.0f * M_PI; float a2 = ((i + 1) / 24.0f) * 2.0f * M_PI; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * sinf(a2); }
    for (int i = 0; i < 24; ++i) { float a1 = (i / 24.0f) * 2.0f * M_PI; float a2 = ((i + 1) / 24.0f) * 2.0f * M_PI; sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = radius * sinf(a2); }
    g_EditorState.light_gizmo_vertex_count = index / 3; GLuint vbo;
    glGenVertexArrays(1, &g_EditorState.light_gizmo_vao); glGenBuffers(1, &vbo);
    glBindVertexArray(g_EditorState.light_gizmo_vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sphere_lines), sphere_lines, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    float lines[] = { -0.5,-0.5,-0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,-0.5,-0.5,-0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,-0.5,-0.5,-0.5,-0.5,0.5,0.5,-0.5,-0.5,0.5,-0.5,0.5,-0.5,0.5,-0.5,-0.5,0.5,0.5,0.5,0.5,-0.5,0.5,0.5,0.5 };
    g_EditorState.decal_box_vertex_count = 24;
    glGenVertexArrays(1, &g_EditorState.decal_box_vao); glGenBuffers(1, &g_EditorState.decal_box_vbo);
    glBindVertexArray(g_EditorState.decal_box_vao); glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.decal_box_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lines), lines, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glBindVertexArray(0);
#define PLAYER_HEIGHT_NORMAL_EDITOR 1.83f
#define PLAYER_RADIUS_EDITOR 0.4f
    Vec3 p_verts[500];
    int p_vert_count = 0;
    float p_radius = PLAYER_RADIUS_EDITOR;
    float p_height = PLAYER_HEIGHT_NORMAL_EDITOR;
    float p_cylinder_height = p_height - (2.0f * p_radius);

    Vec3 bottom_center = { 0, p_radius, 0 };
    Vec3 top_center = { 0, p_radius + p_cylinder_height, 0 };
    int segments = 16;

    for (int i = 0; i < segments; ++i) {
        float angle1 = (i / (float)segments) * 2.0f * M_PI;
        float angle2 = ((i + 1) / (float)segments) * 2.0f * M_PI;

        float x1 = p_radius * cosf(angle1);
        float z1 = p_radius * sinf(angle1);
        float x2 = p_radius * cosf(angle2);
        float z2 = p_radius * sinf(angle2);

        p_verts[p_vert_count++] = (Vec3){ x1, bottom_center.y, z1 };
        p_verts[p_vert_count++] = (Vec3){ x2, bottom_center.y, z2 };

        p_verts[p_vert_count++] = (Vec3){ x1, top_center.y, z1 };
        p_verts[p_vert_count++] = (Vec3){ x2, top_center.y, z2 };

        if (i % (segments / 4) == 0) {
            p_verts[p_vert_count++] = (Vec3){ x1, bottom_center.y, z1 };
            p_verts[p_vert_count++] = (Vec3){ x1, top_center.y, z1 };
        }
    }

    int arc_segments = 8;
    for (int i = 0; i < arc_segments; ++i) {
        float angle1 = (i / (float)arc_segments) * 0.5f * M_PI;
        float angle2 = ((i + 1) / (float)arc_segments) * 0.5f * M_PI;

        p_verts[p_vert_count++] = (Vec3){ top_center.x, top_center.y + p_radius * sinf(angle1), top_center.z + p_radius * cosf(angle1) };
        p_verts[p_vert_count++] = (Vec3){ top_center.x, top_center.y + p_radius * sinf(angle2), top_center.z + p_radius * cosf(angle2) };
        p_verts[p_vert_count++] = (Vec3){ top_center.x + p_radius * cosf(angle1), top_center.y + p_radius * sinf(angle1), top_center.z };
        p_verts[p_vert_count++] = (Vec3){ top_center.x + p_radius * cosf(angle2), top_center.y + p_radius * sinf(angle2), top_center.z };
        p_verts[p_vert_count++] = (Vec3){ bottom_center.x, bottom_center.y - p_radius * sinf(angle1), bottom_center.z + p_radius * cosf(angle1) };
        p_verts[p_vert_count++] = (Vec3){ bottom_center.x, bottom_center.y - p_radius * sinf(angle2), bottom_center.z + p_radius * cosf(angle2) };
        p_verts[p_vert_count++] = (Vec3){ bottom_center.x + p_radius * cosf(angle1), bottom_center.y - p_radius * sinf(angle1), bottom_center.z };
        p_verts[p_vert_count++] = (Vec3){ bottom_center.x + p_radius * cosf(angle2), bottom_center.y - p_radius * sinf(angle1), bottom_center.z };
    }

    g_EditorState.player_start_gizmo_vertex_count = p_vert_count;
    glGenVertexArrays(1, &g_EditorState.player_start_gizmo_vao);
    glGenBuffers(1, &g_EditorState.player_start_gizmo_vbo);
    glBindVertexArray(g_EditorState.player_start_gizmo_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.player_start_gizmo_vbo);
    glBufferData(GL_ARRAY_BUFFER, p_vert_count * sizeof(Vec3), p_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Editor_Init(Engine* engine, Renderer* renderer, Scene* scene) {
    if (g_EditorState.initialized) return;
    g_is_editor_mode = true;
    g_CurrentScene = scene;
    memset(&g_EditorState, 0, sizeof(EditorState));
    g_EditorState.selections = NULL;
    g_EditorState.num_selections = 0;
    g_EditorState.preview_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
    g_EditorState.preview_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
    g_EditorState.current_brush_shape = BRUSH_SHAPE_BLOCK;
    g_EditorState.cylinder_creation_steps = 16;
    g_EditorState.is_dragging_preview_brush_handle = false;
    g_EditorState.is_hovering_preview_brush_body = false;
    g_EditorState.is_dragging_preview_brush_body = false;
    g_EditorState.is_in_z_mode = false;
    g_is_unlit_mode = false;
    g_EditorState.is_dragging_selected_brush_handle = false;
    g_EditorState.selected_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
    g_EditorState.captured_viewport = VIEW_COUNT;
    g_EditorState.current_gizmo_operation = GIZMO_OP_TRANSLATE;
    Editor_InitGizmo();
    g_EditorState.editor_camera.position = (Vec3){ 0, 5, 15 }; g_EditorState.editor_camera.yaw = -M_PI / 2.0f; g_EditorState.editor_camera.pitch = -0.4f;
    for (int i = 0; i < VIEW_COUNT; i++) {
        g_EditorState.viewport_width[i] = 800; g_EditorState.viewport_height[i] = 600;
        glGenFramebuffers(1, &g_EditorState.viewport_fbo[i]); glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[i]);
        glGenTextures(1, &g_EditorState.viewport_texture[i]); glBindTexture(GL_TEXTURE_2D, g_EditorState.viewport_texture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, g_EditorState.viewport_width[i], g_EditorState.viewport_height[i], 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.viewport_texture[i], 0);
        glGenRenderbuffers(1, &g_EditorState.viewport_rbo[i]); glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.viewport_rbo[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, g_EditorState.viewport_width[i], g_EditorState.viewport_height[i]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_EditorState.viewport_rbo[i]);
    }
    g_EditorState.model_preview_width = 512; g_EditorState.model_preview_height = 512;
    glGenFramebuffers(1, &g_EditorState.model_preview_fbo); glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.model_preview_fbo);
    glGenTextures(1, &g_EditorState.model_preview_texture); glBindTexture(GL_TEXTURE_2D, g_EditorState.model_preview_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, g_EditorState.model_preview_width, g_EditorState.model_preview_height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.model_preview_texture, 0);
    glGenRenderbuffers(1, &g_EditorState.model_preview_rbo); glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.model_preview_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, g_EditorState.model_preview_width, g_EditorState.model_preview_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_EditorState.model_preview_rbo);
    int thumb_size = 128;
    glGenFramebuffers(1, &g_EditorState.model_thumb_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.model_thumb_fbo);
    glGenTextures(1, &g_EditorState.model_thumb_texture);
    glBindTexture(GL_TEXTURE_2D, g_EditorState.model_thumb_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, thumb_size, thumb_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.model_thumb_texture, 0);
    glGenRenderbuffers(1, &g_EditorState.model_thumb_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.model_thumb_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, thumb_size, thumb_size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_EditorState.model_thumb_rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_EditorState.arch_preview_width = 200; g_EditorState.arch_preview_height = 150;
    glGenFramebuffers(1, &g_EditorState.arch_preview_fbo); glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.arch_preview_fbo);
    glGenTextures(1, &g_EditorState.arch_preview_texture); glBindTexture(GL_TEXTURE_2D, g_EditorState.arch_preview_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_EditorState.arch_preview_width, g_EditorState.arch_preview_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_EditorState.arch_preview_texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_EditorState.model_preview_cam_dist = 5.0f; g_EditorState.model_preview_cam_angles = (Vec2){ 0.f, -0.5f };
    for (int i = 0; i < 3; i++) { g_EditorState.ortho_cam_pos[i] = (Vec3){ 0,0,0 }; g_EditorState.ortho_cam_zoom[i] = 10.0f; }
    Editor_InitDebugRenderer();
    glGenVertexArrays(1, &g_EditorState.vertex_points_vao); glGenBuffers(1, &g_EditorState.vertex_points_vbo);
    glGenVertexArrays(1, &g_EditorState.selected_face_vao); glGenBuffers(1, &g_EditorState.selected_face_vbo);
    g_EditorState.grid_size = 1.0f; g_EditorState.snap_to_grid = true;
    g_EditorState.grid_shader = createShaderProgram("shaders/grid.vert", "shaders/grid.frag");
    Undo_Init();
    g_EditorState.initialized = true;
    g_EditorState.is_clipping = false;
    g_EditorState.clip_point_count = 0;
    if (strlen(scene->mapPath) > 0) {
        strcpy(g_EditorState.currentMapPath, scene->mapPath);
    }
    else {
        strcpy(g_EditorState.currentMapPath, "untitled.map");
    }
    g_EditorState.show_load_map_popup = false;
    g_EditorState.show_save_map_popup = false;
    strcpy(g_EditorState.save_map_path, "new_map.map");
    g_EditorState.map_file_list = NULL;
    g_EditorState.num_map_files = 0;
    g_EditorState.selected_map_file_index = -1;
    g_EditorState.is_painting = false;
    g_EditorState.is_painting_mode_enabled = false;
    g_EditorState.paint_brush_radius = 2.0f;
    g_EditorState.paint_brush_strength = 1.0f;
    g_EditorState.show_texture_browser = false;
    memset(g_EditorState.texture_search_filter, 0, sizeof(g_EditorState.texture_search_filter));
    g_EditorState.paint_channel = 0;
    g_EditorState.is_sculpting = false;
    g_EditorState.is_sculpting_mode_enabled = false;
    g_EditorState.sculpt_brush_radius = 2.0f;
    g_EditorState.sculpt_brush_strength = 0.5f;
    g_EditorState.show_sound_browser_popup = false;
    g_EditorState.sound_file_list = NULL;
    g_EditorState.num_sound_files = 0;
    g_EditorState.selected_sound_file_index = -1;
    memset(g_EditorState.sound_search_filter, 0, sizeof(g_EditorState.sound_search_filter));
    g_EditorState.preview_sound_buffer = 0;
    g_EditorState.preview_sound_source = 0;
    g_EditorState.paint_brush_hit_surface = false;
    g_EditorState.show_replace_textures_popup = false;
    g_EditorState.find_material_index = -1;
    g_EditorState.replace_material_index = -1;
    g_EditorState.show_vertex_tools_window = false;
    g_EditorState.show_sculpt_noise_popup = false;
    g_EditorState.show_about_window = false;
    g_EditorState.show_sprinkle_tool_window = false;
    strcpy(g_EditorState.sprinkle_model_path, "");
    g_EditorState.sprinkle_density = 5.0f;
    g_EditorState.sprinkle_radius = 5.0f;
    g_EditorState.sprinkle_mode = 0;
    g_EditorState.sprinkle_scale_min = 0.8f;
    g_EditorState.sprinkle_scale_max = 1.2f;
    g_EditorState.sprinkle_align_to_normal = true;
    g_EditorState.sprinkle_random_yaw = true;
    g_EditorState.is_sprinkling = false;
    g_EditorState.sprinkle_timer = 0.0f;
    g_EditorState.sprinkle_brush_hit_surface = false;
    g_EditorState.last_active_2d_view = VIEW_TOP_XZ;
    g_EditorState.editor_camera_speed = 10.0f;
    g_EditorState.texture_lock_enabled = true;
    g_EditorState.show_help_window = false;
    g_EditorState.doc_files = NULL;
    g_EditorState.num_doc_files = 0;
    g_EditorState.selected_doc_index = -1;
    g_EditorState.current_doc_content = NULL;
    g_EditorState.recent_map_files = NULL;
    g_EditorState.num_recent_map_files = 0;
    g_EditorState.next_group_id = 1;
    g_EditorState.show_arch_properties_popup = false;
    g_EditorState.arch_wall_width = 0.1f;
    g_EditorState.arch_num_sides = 8;
    g_EditorState.arch_arc_degrees = 180.0f;
    g_EditorState.arch_start_angle_degrees = 0.0f;
    g_EditorState.arch_add_height = 0.0f;
    Editor_LoadRecentFiles();
}
void Editor_Shutdown() {
    if (!g_EditorState.initialized) return;
    g_is_editor_mode = false;
    Undo_Shutdown();
    for (int i = 0; i < VIEW_COUNT; i++) { glDeleteFramebuffers(1, &g_EditorState.viewport_fbo[i]); glDeleteTextures(1, &g_EditorState.viewport_texture[i]); glDeleteRenderbuffers(1, &g_EditorState.viewport_rbo[i]); }
    glDeleteFramebuffers(1, &g_EditorState.model_preview_fbo); glDeleteTextures(1, &g_EditorState.model_preview_texture); glDeleteRenderbuffers(1, &g_EditorState.model_preview_rbo);
    glDeleteFramebuffers(1, &g_EditorState.model_thumb_fbo); glDeleteTextures(1, &g_EditorState.model_thumb_texture); glDeleteRenderbuffers(1, &g_EditorState.model_thumb_rbo);
    if (g_EditorState.preview_model) Model_Free(g_EditorState.preview_model);
    if (g_EditorState.preview_sound_source != 0) SoundSystem_DeleteSource(g_EditorState.preview_sound_source);
    if (g_EditorState.preview_sound_buffer != 0) SoundSystem_DeleteBuffer(g_EditorState.preview_sound_buffer);
    if (g_EditorState.sound_file_list) {
        for (int i = 0; i < g_EditorState.num_sound_files; ++i) {
            free(g_EditorState.sound_file_list[i]);
        }
        free(g_EditorState.sound_file_list);
    }
    FreeModelBrowserEntries();
    FreeMapFileList();
    glDeleteProgram(g_EditorState.debug_shader); glDeleteVertexArrays(1, &g_EditorState.light_gizmo_vao);
    Brush_FreeData(&g_EditorState.preview_brush);
    glDeleteVertexArrays(1, &g_EditorState.vertex_points_vao); glDeleteBuffers(1, &g_EditorState.vertex_points_vbo);
    glDeleteVertexArrays(1, &g_EditorState.selected_face_vao); glDeleteBuffers(1, &g_EditorState.selected_face_vbo);
    glDeleteVertexArrays(1, &g_EditorState.decal_box_vao); glDeleteBuffers(1, &g_EditorState.decal_box_vbo);
    glDeleteProgram(g_EditorState.grid_shader);
    glDeleteProgram(g_EditorState.gizmo_shader);
    glDeleteVertexArrays(1, &g_EditorState.gizmo_vao);
    glDeleteBuffers(1, &g_EditorState.gizmo_vbo);
    glDeleteVertexArrays(1, &g_EditorState.player_start_gizmo_vao);
    glDeleteBuffers(1, &g_EditorState.player_start_gizmo_vbo);
    glDeleteFramebuffers(1, &g_EditorState.arch_preview_fbo);
    glDeleteTextures(1, &g_EditorState.arch_preview_texture);
    if (g_EditorState.recent_map_files) {
        for (int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
            free(g_EditorState.recent_map_files[i]);
        }
        free(g_EditorState.recent_map_files);
    }
    FreeDocFileList();
    if (g_EditorState.current_doc_content) {
        free(g_EditorState.current_doc_content);
        g_EditorState.current_doc_content = NULL;
    }
    if (g_EditorState.selections) free(g_EditorState.selections);
    if (g_EditorState.gizmo_drag_start_positions) free(g_EditorState.gizmo_drag_start_positions);
    if (g_EditorState.gizmo_drag_start_rotations) free(g_EditorState.gizmo_drag_start_rotations);
    if (g_EditorState.gizmo_drag_start_scales) free(g_EditorState.gizmo_drag_start_scales);
    if (g_EditorState.grid_vao != 0) { glDeleteVertexArrays(1, &g_EditorState.grid_vao); glDeleteBuffers(1, &g_EditorState.grid_vbo); }
    g_EditorState.initialized = false;
}

static void Editor_PickObjectAtScreenPos(Vec2 screen_pos, ViewportType viewport) {
    if (viewport != VIEW_PERSPECTIVE) return;

    float ndc_x = (screen_pos.x / g_EditorState.viewport_width[viewport]) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[viewport]) * 2.0f;
    Mat4 inv_proj, inv_view;
    mat4_inverse(&g_proj_matrix[viewport], &inv_proj);
    mat4_inverse(&g_view_matrix[viewport], &inv_view);
    Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
    Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
    ray_eye.z = -1.0f; ray_eye.w = 0.0f;
    Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
    Vec3 ray_dir_world = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
    vec3_normalize(&ray_dir_world);
    Vec3 ray_origin_world = g_EditorState.editor_camera.position;

    float closest_t = FLT_MAX;
    EntityType selected_type = ENTITY_NONE;
    int selected_index = -1;
    int hit_face_index = -1;

    for (int i = 0; i < g_CurrentScene->numObjects; ++i) {
        SceneObject* obj = &g_CurrentScene->objects[i];
        if (!obj->model) continue;

        float t;
        if (RayIntersectsOBB(ray_origin_world, ray_dir_world,
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

    for (int i = 0; i < g_CurrentScene->numBrushes; ++i) {
        Brush* brush = &g_CurrentScene->brushes[i];
        if (brush->isReflectionProbe) continue;

        Vec3 brush_local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 brush_local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        if (brush->numVertices > 0) {
            for (int v_idx = 0; v_idx < brush->numVertices; ++v_idx) {
                brush_local_min.x = fminf(brush_local_min.x, brush->vertices[v_idx].pos.x);
                brush_local_min.y = fminf(brush_local_min.y, brush->vertices[v_idx].pos.y);
                brush_local_min.z = fminf(brush_local_min.z, brush->vertices[v_idx].pos.z);
                brush_local_max.x = fmaxf(brush_local_max.x, brush->vertices[v_idx].pos.x);
                brush_local_max.y = fmaxf(brush_local_max.y, brush->vertices[v_idx].pos.y);
                brush_local_max.z = fmaxf(brush_local_max.z, brush->vertices[v_idx].pos.z);
            }
        }
        else {
            brush_local_min = (Vec3){ 0,0,0 };
            brush_local_max = (Vec3){ 0,0,0 };
        }

        float t_obb_dummy;
        if (!RayIntersectsOBB(ray_origin_world, ray_dir_world,
            &brush->modelMatrix,
            brush_local_min,
            brush_local_max,
            &t_obb_dummy)) {
            continue;
        }

        Mat4 inv_brush_model_matrix;
        if (!mat4_inverse(&brush->modelMatrix, &inv_brush_model_matrix)) {
            continue;
        }
        Vec3 ray_origin_local = mat4_mul_vec3(&inv_brush_model_matrix, ray_origin_world);
        Vec3 ray_dir_local = mat4_mul_vec3_dir(&inv_brush_model_matrix, ray_dir_world);

        for (int face_idx = 0; face_idx < brush->numFaces; ++face_idx) {
            BrushFace* face = &brush->faces[face_idx];
            if (face->numVertexIndices < 3) continue;

            for (int k = 0; k < face->numVertexIndices - 2; ++k) {
                Vec3 v0_local = brush->vertices[face->vertexIndices[0]].pos;
                Vec3 v1_local = brush->vertices[face->vertexIndices[k + 1]].pos;
                Vec3 v2_local = brush->vertices[face->vertexIndices[k + 2]].pos;

                float t_triangle_local;
                if (RayIntersectsTriangle(ray_origin_local, ray_dir_local, v0_local, v1_local, v2_local, &t_triangle_local)) {
                    Vec3 hit_point_local = vec3_add(ray_origin_local, vec3_muls(ray_dir_local, t_triangle_local));
                    Vec3 hit_point_world = mat4_mul_vec3(&brush->modelMatrix, hit_point_local);
                    float dist_to_hit_world = vec3_length(vec3_sub(hit_point_world, ray_origin_world));

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

    for (int i = 0; i < g_CurrentScene->numActiveLights; ++i) {
        Light* light = &g_CurrentScene->lights[i];
        float light_gizmo_radius = 0.5f;
        Vec3 P = vec3_sub(light->position, ray_origin_world);
        float b_dot = vec3_dot(P, ray_dir_world);
        float det = b_dot * b_dot - vec3_dot(P, P) + light_gizmo_radius * light_gizmo_radius;
        if (det < 0) continue;
        float t_light = b_dot - sqrtf(det);
        if (t_light > 0 && t_light < closest_t) {
            closest_t = t_light;
            selected_type = ENTITY_LIGHT;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (int i = 0; i < g_CurrentScene->numDecals; ++i) {
        Decal* decal = &g_CurrentScene->decals[i];

        Vec3 decal_local_min = { -0.5f, -0.5f, -0.5f };
        Vec3 decal_local_max = { 0.5f, 0.5f, 0.5f };

        float t;
        if (RayIntersectsOBB(ray_origin_world, ray_dir_world,
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

    for (int i = 0; i < g_CurrentScene->numParticleEmitters; ++i) {
        ParticleEmitter* emitter = &g_CurrentScene->particleEmitters[i];
        float emitter_gizmo_radius = 0.5f;
        Vec3 P = vec3_sub(emitter->pos, ray_origin_world);
        float b_dot = vec3_dot(P, ray_dir_world);
        float det = b_dot * b_dot - vec3_dot(P, P) + emitter_gizmo_radius * emitter_gizmo_radius;
        if (det < 0) continue;
        float t_emitter = b_dot - sqrtf(det);
        if (t_emitter > 0 && t_emitter < closest_t) {
            closest_t = t_emitter;
            selected_type = ENTITY_PARTICLE_EMITTER;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (int i = 0; i < g_CurrentScene->numSoundEntities; ++i) {
        SoundEntity* sound = &g_CurrentScene->soundEntities[i];
        float sound_gizmo_radius = 0.5f;
        Vec3 P = vec3_sub(sound->pos, ray_origin_world);
        float b_dot = vec3_dot(P, ray_dir_world);
        float det = b_dot * b_dot - vec3_dot(P, P) + sound_gizmo_radius * sound_gizmo_radius;
        if (det < 0) continue;
        float t_sound = b_dot - sqrtf(det);
        if (t_sound > 0 && t_sound < closest_t) {
            closest_t = t_sound;
            selected_type = ENTITY_SOUND;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (int i = 0; i < g_CurrentScene->numLogicEntities; ++i) {
        LogicEntity* ent = &g_CurrentScene->logicEntities[i];
        float logic_gizmo_radius = 0.5f;
        Vec3 P = vec3_sub(ent->pos, ray_origin_world);
        float b_dot = vec3_dot(P, ray_dir_world);
        float det = b_dot * b_dot - vec3_dot(P, P) + logic_gizmo_radius * logic_gizmo_radius;
        if (det < 0) continue;
        float t_logic = b_dot - sqrtf(det);
        if (t_logic > 0 && t_logic < closest_t) {
            closest_t = t_logic;
            selected_type = ENTITY_LOGIC;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    float player_start_radius = 1.0f;
    Vec3 P = vec3_sub(g_CurrentScene->playerStart.position, ray_origin_world);
    float b_dot = vec3_dot(P, ray_dir_world);
    float det = b_dot * b_dot - vec3_dot(P, P) + player_start_radius * player_start_radius;
    if (det >= 0) {
        float t_player = b_dot - sqrtf(det);
        if (t_player > 0 && t_player < closest_t) {
            closest_t = t_player;
            selected_type = ENTITY_PLAYERSTART;
            selected_index = 0;
            hit_face_index = -1;
        }
    }

    for (int i = 0; i < g_CurrentScene->numVideoPlayers; ++i) {
        VideoPlayer* vp = &g_CurrentScene->videoPlayers[i];

        Vec3 vp_local_min = { -0.5f, -0.5f, -0.5f };
        Vec3 vp_local_max = { 0.5f, 0.5f, 0.5f };
        vp->modelMatrix = create_trs_matrix(vp->pos, vp->rot, (Vec3) { vp->size.x, vp->size.y, 0.01f });

        float t;
        if (RayIntersectsOBB(ray_origin_world, ray_dir_world,
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

    for (int i = 0; i < g_CurrentScene->numParallaxRooms; ++i) {
        ParallaxRoom* p = &g_CurrentScene->parallaxRooms[i];
        p->modelMatrix = create_trs_matrix(p->pos, p->rot, (Vec3) { p->size.x, p->size.y, 0.01f });
        Vec3 local_min = { -0.5f, -0.5f, -0.5f };
        Vec3 local_max = { 0.5f, 0.5f, 0.5f };
        float t;
        if (RayIntersectsOBB(ray_origin_world, ray_dir_world, &p->modelMatrix, local_min, local_max, &t) && t < closest_t) {
            closest_t = t;
            selected_type = ENTITY_PARALLAX_ROOM;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    for (int i = 0; i < g_CurrentScene->numSprites; ++i) {
        Sprite* s = &g_CurrentScene->sprites[i];
        float sprite_gizmo_radius = s->scale * 0.5f;
        Vec3 P = vec3_sub(s->pos, ray_origin_world);
        float b_dot = vec3_dot(P, ray_dir_world);
        float det = b_dot * b_dot - vec3_dot(P, P) + sprite_gizmo_radius * sprite_gizmo_radius;
        if (det < 0) continue;
        float t_sprite = b_dot - sqrtf(det);
        if (t_sprite > 0 && t_sprite < closest_t) {
            closest_t = t_sprite;
            selected_type = ENTITY_SPRITE;
            selected_index = i;
            hit_face_index = -1;
        }
    }

    bool ctrl_held = (SDL_GetModState() & KMOD_CTRL);

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
        const char* group_name = NULL;
        bool is_grouped = false;

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
            default: break;
            }
        }

        if (is_grouped && group_name && strlen(group_name) > 0) {
            if (selected_type == ENTITY_BRUSH && hit_face_index != -1) {
                for (int i = 0; i < g_CurrentScene->brushes[selected_index].numFaces; ++i) {
                    if (g_CurrentScene->brushes[selected_index].faces[i].isGrouped && strcmp(g_CurrentScene->brushes[selected_index].faces[i].groupName, group_name) == 0) {
                        Editor_AddToSelection(ENTITY_BRUSH, selected_index, i, -1);
                    }
                }
            }
            else {
                for (int i = 0; i < g_CurrentScene->numObjects; ++i) if (g_CurrentScene->objects[i].isGrouped && strcmp(g_CurrentScene->objects[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_MODEL, i, -1, -1);
                for (int i = 0; i < g_CurrentScene->numBrushes; ++i) if (g_CurrentScene->brushes[i].isGrouped && strcmp(g_CurrentScene->brushes[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_BRUSH, i, -1, -1);
                for (int i = 0; i < g_CurrentScene->numActiveLights; ++i) if (g_CurrentScene->lights[i].isGrouped && strcmp(g_CurrentScene->lights[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_LIGHT, i, -1, -1);
                for (int i = 0; i < g_CurrentScene->numDecals; ++i) if (g_CurrentScene->decals[i].isGrouped && strcmp(g_CurrentScene->decals[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_DECAL, i, -1, -1);
                for (int i = 0; i < g_CurrentScene->numSoundEntities; ++i) if (g_CurrentScene->soundEntities[i].isGrouped && strcmp(g_CurrentScene->soundEntities[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_SOUND, i, -1, -1);
                for (int i = 0; i < g_CurrentScene->numParticleEmitters; ++i) if (g_CurrentScene->particleEmitters[i].isGrouped && strcmp(g_CurrentScene->particleEmitters[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_PARTICLE_EMITTER, i, -1, -1);
                for (int i = 0; i < g_CurrentScene->numSprites; ++i) if (g_CurrentScene->sprites[i].isGrouped && strcmp(g_CurrentScene->sprites[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_SPRITE, i, -1, -1);
                for (int i = 0; i < g_CurrentScene->numVideoPlayers; ++i) if (g_CurrentScene->videoPlayers[i].isGrouped && strcmp(g_CurrentScene->videoPlayers[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_VIDEO_PLAYER, i, -1, -1);
                for (int i = 0; i < g_CurrentScene->numParallaxRooms; ++i) if (g_CurrentScene->parallaxRooms[i].isGrouped && strcmp(g_CurrentScene->parallaxRooms[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_PARALLAX_ROOM, i, -1, -1);
                for (int i = 0; i < g_CurrentScene->numLogicEntities; ++i) if (g_CurrentScene->logicEntities[i].isGrouped && strcmp(g_CurrentScene->logicEntities[i].groupName, group_name) == 0) Editor_AddToSelection(ENTITY_LOGIC, i, -1, -1);
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
static int Editor_PickVertexAtScreenPos(Scene* scene, Vec2 screen_pos, ViewportType viewport) {
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (!primary || primary->type != ENTITY_BRUSH) {
        return -1;
    }

    float ndc_x = (screen_pos.x / g_EditorState.viewport_width[viewport]) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[viewport]) * 2.0f;
    Mat4 inv_proj, inv_view;
    mat4_inverse(&g_proj_matrix[viewport], &inv_proj);
    mat4_inverse(&g_view_matrix[viewport], &inv_view);
    Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
    Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
    ray_eye.z = -1.0f; ray_eye.w = 0.0f;
    Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
    Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
    vec3_normalize(&ray_dir);
    Vec3 ray_origin = g_EditorState.editor_camera.position;

    Brush* b = &scene->brushes[primary->index];
    float closest_t = FLT_MAX;
    int picked_vertex = -1;
    const float pick_radius = 0.1f;

    for (int i = 0; i < b->numVertices; ++i) {
        Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);

        Vec3 oc = vec3_sub(ray_origin, vert_world_pos);
        float b_dot = vec3_dot(oc, ray_dir);
        float c = vec3_dot(oc, oc) - pick_radius * pick_radius;
        float discriminant = b_dot * b_dot - c;
        if (discriminant > 0) {
            float t = -b_dot - sqrtf(discriminant);
            if (t > 0 && t < closest_t) {
                closest_t = t;
                picked_vertex = i;
            }
        }
    }

    return picked_vertex;
}
static float dist_RaySegment(Vec3 ray_origin, Vec3 ray_dir, Vec3 seg_p0, Vec3 seg_p1, float* t_ray, float* t_seg) {
    Vec3 seg_dir = vec3_sub(seg_p1, seg_p0);
    Vec3 w0 = vec3_sub(ray_origin, seg_p0);
    float a = vec3_dot(ray_dir, ray_dir);
    float b = vec3_dot(ray_dir, seg_dir);
    float c = vec3_dot(seg_dir, seg_dir);
    float d = vec3_dot(ray_dir, w0);
    float e = vec3_dot(seg_dir, w0);
    float det = a * c - b * b;
    float s, t;
    if (det < 1e-5f) { s = 0.0f; t = e / c; }
    else { s = (b * e - c * d) / det; t = (a * e - b * d) / det; }
    *t_ray = s; *t_seg = fmaxf(0.0f, fminf(1.0f, t));
    Vec3 closest_point_on_ray = vec3_add(ray_origin, vec3_muls(ray_dir, *t_ray));
    Vec3 closest_point_on_seg = vec3_add(seg_p0, vec3_muls(seg_dir, *t_seg));
    return vec3_length(vec3_sub(closest_point_on_ray, closest_point_on_seg));
}
static bool ray_plane_intersect(Vec3 ray_origin, Vec3 ray_dir, Vec3 plane_normal, float plane_d, Vec3* intersect_point) {
    float denom = vec3_dot(plane_normal, ray_dir);
    if (fabs(denom) > 1e-6) {
        float t = -(vec3_dot(plane_normal, ray_origin) + plane_d) / denom;
        if (t >= 0) { *intersect_point = vec3_add(ray_origin, vec3_muls(ray_dir, t)); return true; }
    }
    return false;
}
static void Editor_GroupSelection() {
    if (g_EditorState.num_selections < 2) return;

    char group_name[64];
    snprintf(group_name, sizeof(group_name), "group_%d", g_EditorState.next_group_id++);

    Undo_BeginMultiEntityModification(g_CurrentScene, g_EditorState.selections, g_EditorState.num_selections);

    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];
        switch (sel->type) {
        case ENTITY_MODEL: g_CurrentScene->objects[sel->index].isGrouped = true; strncpy(g_CurrentScene->objects[sel->index].groupName, group_name, 63); break;
        case ENTITY_BRUSH:
            if (sel->face_index != -1) {
                g_CurrentScene->brushes[sel->index].faces[sel->face_index].isGrouped = true;
                strncpy(g_CurrentScene->brushes[sel->index].faces[sel->face_index].groupName, group_name, 63);
            }
            else {
                g_CurrentScene->brushes[sel->index].isGrouped = true;
                strncpy(g_CurrentScene->brushes[sel->index].groupName, group_name, 63);
            }
            break;
        case ENTITY_LIGHT: g_CurrentScene->lights[sel->index].isGrouped = true; strncpy(g_CurrentScene->lights[sel->index].groupName, group_name, 63); break;
        case ENTITY_DECAL: g_CurrentScene->decals[sel->index].isGrouped = true; strncpy(g_CurrentScene->decals[sel->index].groupName, group_name, 63); break;
        case ENTITY_SOUND: g_CurrentScene->soundEntities[sel->index].isGrouped = true; strncpy(g_CurrentScene->soundEntities[sel->index].groupName, group_name, 63); break;
        case ENTITY_PARTICLE_EMITTER: g_CurrentScene->particleEmitters[sel->index].isGrouped = true; strncpy(g_CurrentScene->particleEmitters[sel->index].groupName, group_name, 63); break;
        case ENTITY_SPRITE: g_CurrentScene->sprites[sel->index].isGrouped = true; strncpy(g_CurrentScene->sprites[sel->index].groupName, group_name, 63); break;
        case ENTITY_VIDEO_PLAYER: g_CurrentScene->videoPlayers[sel->index].isGrouped = true; strncpy(g_CurrentScene->videoPlayers[sel->index].groupName, group_name, 63); break;
        case ENTITY_PARALLAX_ROOM: g_CurrentScene->parallaxRooms[sel->index].isGrouped = true; strncpy(g_CurrentScene->parallaxRooms[sel->index].groupName, group_name, 63); break;
        case ENTITY_LOGIC: g_CurrentScene->logicEntities[sel->index].isGrouped = true; strncpy(g_CurrentScene->logicEntities[sel->index].groupName, group_name, 63); break;
        default: break;
        }
    }

    Undo_EndMultiEntityModification(g_CurrentScene, g_EditorState.selections, g_EditorState.num_selections, "Group Selection");
}

static void Editor_UngroupSelection() {
    if (g_EditorState.num_selections == 0) return;

    Undo_BeginMultiEntityModification(g_CurrentScene, g_EditorState.selections, g_EditorState.num_selections);

    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];
        switch (sel->type) {
        case ENTITY_MODEL: g_CurrentScene->objects[sel->index].isGrouped = false; g_CurrentScene->objects[sel->index].groupName[0] = '\0'; break;
        case ENTITY_BRUSH:
            if (sel->face_index != -1) {
                g_CurrentScene->brushes[sel->index].faces[sel->face_index].isGrouped = false;
                g_CurrentScene->brushes[sel->index].faces[sel->face_index].groupName[0] = '\0';
            }
            else {
                g_CurrentScene->brushes[sel->index].isGrouped = false;
                g_CurrentScene->brushes[sel->index].groupName[0] = '\0';
            }
            break;
        case ENTITY_LIGHT: g_CurrentScene->lights[sel->index].isGrouped = false; g_CurrentScene->lights[sel->index].groupName[0] = '\0'; break;
        case ENTITY_DECAL: g_CurrentScene->decals[sel->index].isGrouped = false; g_CurrentScene->decals[sel->index].groupName[0] = '\0'; break;
        case ENTITY_SOUND: g_CurrentScene->soundEntities[sel->index].isGrouped = false; g_CurrentScene->soundEntities[sel->index].groupName[0] = '\0'; break;
        case ENTITY_PARTICLE_EMITTER: g_CurrentScene->particleEmitters[sel->index].isGrouped = false; g_CurrentScene->particleEmitters[sel->index].groupName[0] = '\0'; break;
        case ENTITY_SPRITE: g_CurrentScene->sprites[sel->index].isGrouped = false; g_CurrentScene->sprites[sel->index].groupName[0] = '\0'; break;
        case ENTITY_VIDEO_PLAYER: g_CurrentScene->videoPlayers[sel->index].isGrouped = false; g_CurrentScene->videoPlayers[sel->index].groupName[0] = '\0'; break;
        case ENTITY_PARALLAX_ROOM: g_CurrentScene->parallaxRooms[sel->index].isGrouped = false; g_CurrentScene->parallaxRooms[sel->index].groupName[0] = '\0'; break;
        case ENTITY_LOGIC: g_CurrentScene->logicEntities[sel->index].isGrouped = false; g_CurrentScene->logicEntities[sel->index].groupName[0] = '\0'; break;
        default: break;
        }
    }
    Undo_EndMultiEntityModification(g_CurrentScene, g_EditorState.selections, g_EditorState.num_selections, "Ungroup Selection");
}
static void Editor_UpdateGizmoHover(Scene* scene, Vec3 ray_origin, Vec3 ray_dir) {
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (!primary) {
        g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
        return;
    }

    if (primary->type == ENTITY_BRUSH && primary->face_index != -1 && g_EditorState.current_gizmo_operation == GIZMO_OP_ROTATE) {
        g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
        return;
    }
    if (g_EditorState.num_selections == 0) {
        g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
        return;
    }
    Vec3 object_pos = g_EditorState.gizmo_selection_centroid;

    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
    float min_dist = FLT_MAX;

    switch (g_EditorState.current_gizmo_operation) {
    case GIZMO_OP_TRANSLATE:
    case GIZMO_OP_SCALE: {
        const float pick_threshold = 0.1f;
        float t_ray, t_seg;
        Vec3 x_p1 = { object_pos.x + 1.0f, object_pos.y, object_pos.z };
        float dist_x = dist_RaySegment(ray_origin, ray_dir, object_pos, x_p1, &t_ray, &t_seg);
        if (dist_x < pick_threshold && dist_x < min_dist) { min_dist = dist_x; g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X; }

        Vec3 y_p1 = { object_pos.x, object_pos.y + 1.0f, object_pos.z };
        float dist_y = dist_RaySegment(ray_origin, ray_dir, object_pos, y_p1, &t_ray, &t_seg);
        if (dist_y < pick_threshold && dist_y < min_dist) { min_dist = dist_y; g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y; }

        Vec3 z_p1 = { object_pos.x, object_pos.y, object_pos.z + 1.0f };
        float dist_z = dist_RaySegment(ray_origin, ray_dir, object_pos, z_p1, &t_ray, &t_seg);
        if (dist_z < pick_threshold && dist_z < min_dist) { g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z; }
        break;
    }
    case GIZMO_OP_ROTATE: {
        const float radius = 1.0f;
        const float pick_threshold = 0.1f;
        Vec3 intersect_point;
        float closest_dist = FLT_MAX;

        if (ray_plane_intersect(ray_origin, ray_dir, (Vec3) { 0, 1, 0 }, -object_pos.y, & intersect_point)) {
            float dist_to_intersection = vec3_length(vec3_sub(intersect_point, ray_origin));
            if (fabs(vec3_length(vec3_sub(intersect_point, object_pos)) - radius) < pick_threshold) {
                if (dist_to_intersection < closest_dist) {
                    closest_dist = dist_to_intersection;
                    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y;
                }
            }
        }

        if (ray_plane_intersect(ray_origin, ray_dir, (Vec3) { 1, 0, 0 }, -object_pos.x, & intersect_point)) {
            float dist_to_intersection = vec3_length(vec3_sub(intersect_point, ray_origin));
            if (fabs(vec3_length(vec3_sub(intersect_point, object_pos)) - radius) < pick_threshold) {
                if (dist_to_intersection < closest_dist) {
                    closest_dist = dist_to_intersection;
                    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X;
                }
            }
        }

        if (ray_plane_intersect(ray_origin, ray_dir, (Vec3) { 0, 0, 1 }, -object_pos.z, & intersect_point)) {
            float dist_to_intersection = vec3_length(vec3_sub(intersect_point, ray_origin));
            if (fabs(vec3_length(vec3_sub(intersect_point, object_pos)) - radius) < pick_threshold) {
                if (dist_to_intersection < closest_dist) {
                    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z;
                }
            }
        }
        break;
    }
    }
}
void Editor_ProcessEvent(SDL_Event* event, Scene* scene, Engine* engine) {
    if (event->type == SDL_MOUSEMOTION) {
        bool can_look = g_EditorState.is_in_z_mode || (g_EditorState.is_viewport_focused[VIEW_PERSPECTIVE] && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)));
        if (can_look) {
            g_EditorState.editor_camera.yaw += event->motion.xrel * 0.005f;
            g_EditorState.editor_camera.pitch -= event->motion.yrel * 0.005f;
        }
    }
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (event->type == SDL_KEYUP && event->key.keysym.sym == SDLK_c) {
        if (g_EditorState.is_clipping) {
            if (primary && primary->type == ENTITY_BRUSH && g_EditorState.clip_point_count >= 2) {
                if (scene->numBrushes >= MAX_BRUSHES - 1) {
                    Console_Printf_Error("[error] Cannot clip brush, MAX_BRUSHES limit reached.");
                    g_EditorState.is_clipping = false;
                    return;
                }

                int original_brush_index = primary->index;
                Brush* original_brush = &scene->brushes[original_brush_index];

                Undo_BeginEntityModification(scene, ENTITY_BRUSH, original_brush_index);

                Brush brush_b_storage = { 0 };
                Brush_DeepCopy(&brush_b_storage, original_brush);

                Vec3 p1 = g_EditorState.clip_points[0];
                Vec3 p2 = g_EditorState.clip_points[1];
                Vec3 plane_normal;
                Vec3 dir = vec3_sub(p2, p1);

                if (g_EditorState.clip_view == VIEW_TOP_XZ) { plane_normal = vec3_cross(dir, (Vec3) { 0, 1, 0 }); }
                else if (g_EditorState.clip_view == VIEW_FRONT_XY) { plane_normal = vec3_cross(dir, (Vec3) { 0, 0, 1 }); }
                else { plane_normal = vec3_cross(dir, (Vec3) { 1, 0, 0 }); }
                vec3_normalize(&plane_normal);

                float side_check = vec3_dot(plane_normal, vec3_sub(g_EditorState.clip_side_point, p1));
                if (side_check < 0.0f) {
                    plane_normal = vec3_muls(plane_normal, -1.0f);
                }

                float plane_d_a = -vec3_dot(plane_normal, p1);
                float plane_d_b = -plane_d_a;
                Vec3 plane_normal_b = vec3_muls(plane_normal, -1.0f);

                Brush_Clip(original_brush, plane_normal, plane_d_a);
                Brush_CreateRenderData(original_brush);
                if (original_brush->physicsBody) Physics_RemoveRigidBody(engine->physicsWorld, original_brush->physicsBody);
                if (!original_brush->isTrigger && original_brush->numVertices > 0) {
                    Vec3* world_verts = malloc(original_brush->numVertices * sizeof(Vec3));
                    for (int k = 0; k < original_brush->numVertices; ++k) world_verts[k] = mat4_mul_vec3(&original_brush->modelMatrix, original_brush->vertices[k].pos);
                    original_brush->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, original_brush->numVertices);
                    free(world_verts);
                }
                else {
                    original_brush->physicsBody = NULL;
                }

                Brush_Clip(&brush_b_storage, plane_normal_b, plane_d_b);

                if (brush_b_storage.numVertices > 0) {
                    int new_brush_index = scene->numBrushes;
                    scene->brushes[new_brush_index] = brush_b_storage;
                    scene->numBrushes++;

                    Brush* new_b_ptr = &scene->brushes[new_brush_index];
                    Brush_CreateRenderData(new_b_ptr);
                    if (!new_b_ptr->isTrigger && new_b_ptr->isWater && new_b_ptr->numVertices > 0) {
                        Vec3* world_verts = malloc(new_b_ptr->numVertices * sizeof(Vec3));
                        for (int k = 0; k < new_b_ptr->numVertices; ++k) world_verts[k] = mat4_mul_vec3(&new_b_ptr->modelMatrix, new_b_ptr->vertices[k].pos);
                        new_b_ptr->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, new_b_ptr->numVertices);
                        free(world_verts);
                    }
                    else {
                        new_b_ptr->physicsBody = NULL;
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
            bool is_hovering_paint_viewport = false;
            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
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
            bool is_hovering_sculpt_viewport = false;
            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
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
            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
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
                                default: g_EditorState.clip_plane_depth = 0.0f; break;
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
        for (int i = 0; i < VIEW_COUNT; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                active_viewport = (ViewportType)i;
                break;
            }
        }
        if (g_EditorState.selected_brush_hovered_handle != PREVIEW_BRUSH_HANDLE_NONE) {
            g_EditorState.is_dragging_selected_brush_handle = true;
            g_EditorState.selected_brush_active_handle = g_EditorState.selected_brush_hovered_handle;
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            return;
        }
        else if (g_EditorState.is_hovering_selected_brush_body && active_viewport >= VIEW_TOP_XZ && active_viewport <= VIEW_SIDE_YZ) {
            g_EditorState.is_dragging_selected_brush_body = true;
            g_EditorState.selected_brush_drag_body_view = active_viewport;
            Vec3 raw_start_mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
            if (g_EditorState.snap_to_grid) {
                raw_start_mouse_world.x = SnapValue(raw_start_mouse_world.x, g_EditorState.grid_size);
                raw_start_mouse_world.y = SnapValue(raw_start_mouse_world.y, g_EditorState.grid_size);
                raw_start_mouse_world.z = SnapValue(raw_start_mouse_world.z, g_EditorState.grid_size);
            }
            g_EditorState.selected_brush_drag_body_start_mouse_world = raw_start_mouse_world;
            g_EditorState.selected_brush_drag_body_start_brush_pos = scene->brushes[primary->index].pos;
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            return;
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
            g_EditorState.vertex_drag_start_pos_world = mat4_mul_vec3(&b->modelMatrix, b->vertices[primary->vertex_index].pos);

            Vec3 cam_forward = { g_view_matrix[VIEW_PERSPECTIVE].m[2], g_view_matrix[VIEW_PERSPECTIVE].m[6], g_view_matrix[VIEW_PERSPECTIVE].m[10] };
            Vec3 axis_dir = { 0 };
            if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
            if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
            if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
            float dot_product = fabsf(vec3_dot(axis_dir, cam_forward));
            if (dot_product > 0.99f) { if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) { g_EditorState.vertex_gizmo_drag_plane_normal = (Vec3){ 0, 1, 0 }; } else { g_EditorState.vertex_gizmo_drag_plane_normal = (Vec3){ 1, 0, 0 }; } }
            else { g_EditorState.vertex_gizmo_drag_plane_normal = vec3_cross(axis_dir, cam_forward); vec3_normalize(&g_EditorState.vertex_gizmo_drag_plane_normal); }
            g_EditorState.vertex_gizmo_drag_plane_d = -vec3_dot(g_EditorState.vertex_gizmo_drag_plane_normal, g_EditorState.vertex_drag_start_pos_world);

            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
            float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
            Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
            Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
            Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
            ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.vertex_gizmo_drag_plane_normal, g_EditorState.vertex_gizmo_drag_plane_d, &g_EditorState.vertex_gizmo_drag_start_world);
            return;
        }
        else if (g_EditorState.gizmo_hovered_axis != GIZMO_AXIS_NONE && active_viewport != VIEW_COUNT) {
            Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);
            g_EditorState.is_manipulating_gizmo = true;
            if (g_EditorState.gizmo_drag_start_positions) free(g_EditorState.gizmo_drag_start_positions);
            if (g_EditorState.gizmo_drag_start_rotations) free(g_EditorState.gizmo_drag_start_rotations);
            if (g_EditorState.gizmo_drag_start_scales) free(g_EditorState.gizmo_drag_start_scales);

            g_EditorState.gizmo_drag_start_positions = (Vec3*)malloc(g_EditorState.num_selections * sizeof(Vec3));
            g_EditorState.gizmo_drag_start_rotations = (Vec3*)malloc(g_EditorState.num_selections * sizeof(Vec3));
            g_EditorState.gizmo_drag_start_scales = (Vec3*)malloc(g_EditorState.num_selections * sizeof(Vec3));

            for (int i = 0; i < g_EditorState.num_selections; ++i) {
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
                    g_EditorState.gizmo_drag_start_positions[i] = scene->lights[sel->index].position;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->lights[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                case ENTITY_DECAL:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->decals[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->decals[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = scene->decals[sel->index].size;
                    break;
                case ENTITY_SOUND:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->soundEntities[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                case ENTITY_PARTICLE_EMITTER:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->particleEmitters[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                case ENTITY_SPRITE:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->sprites[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ scene->sprites[sel->index].scale, scene->sprites[sel->index].scale, scene->sprites[sel->index].scale };
                    break;
                case ENTITY_VIDEO_PLAYER:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->videoPlayers[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->videoPlayers[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ scene->videoPlayers[sel->index].size.x, scene->videoPlayers[sel->index].size.y, 1.0f };
                    break;
                case ENTITY_PARALLAX_ROOM:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->parallaxRooms[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->parallaxRooms[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ scene->parallaxRooms[sel->index].size.x, scene->parallaxRooms[sel->index].size.y, 1.0f };
                    break;
                case ENTITY_LOGIC:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->logicEntities[sel->index].pos;
                    g_EditorState.gizmo_drag_start_rotations[i] = scene->logicEntities[sel->index].rot;
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                case ENTITY_PLAYERSTART:
                    g_EditorState.gizmo_drag_start_positions[i] = scene->playerStart.position;
                    g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
                default:
                    g_EditorState.gizmo_drag_start_positions[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_rotations[i] = (Vec3){ 0,0,0 };
                    g_EditorState.gizmo_drag_start_scales[i] = (Vec3){ 1,1,1 };
                    break;
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
                        g_EditorState.gizmo_drag_object_start_pos = scene->lights[primary->index].position;
                        g_EditorState.gizmo_drag_object_start_rot = scene->lights[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ 1,1,1 };
                        break;
                    case ENTITY_DECAL:
                        g_EditorState.gizmo_drag_object_start_pos = scene->decals[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->decals[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = scene->decals[primary->index].size;
                        break;
                    case ENTITY_SOUND:
                        g_EditorState.gizmo_drag_object_start_pos = scene->soundEntities[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = (Vec3){ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ 1,1,1 };
                        break;
                    case ENTITY_PARTICLE_EMITTER:
                        g_EditorState.gizmo_drag_object_start_pos = scene->particleEmitters[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = (Vec3){ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ 1,1,1 };
                        break;
                    case ENTITY_SPRITE:
                        g_EditorState.gizmo_drag_object_start_pos = scene->sprites[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = (Vec3){ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ scene->sprites[primary->index].scale, 1, 1 };
                        break;
                    case ENTITY_PLAYERSTART:
                        g_EditorState.gizmo_drag_object_start_pos = scene->playerStart.position;
                        g_EditorState.gizmo_drag_object_start_rot = (Vec3){ 0,0,0 };
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ 1,1,1 };
                        break;
                    case ENTITY_VIDEO_PLAYER:
                        g_EditorState.gizmo_drag_object_start_pos = scene->videoPlayers[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->videoPlayers[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ scene->videoPlayers[primary->index].size.x, scene->videoPlayers[primary->index].size.y, 1.0f };
                        break;
                    case ENTITY_PARALLAX_ROOM:
                        g_EditorState.gizmo_drag_object_start_pos = scene->parallaxRooms[primary->index].pos;
                        g_EditorState.gizmo_drag_object_start_rot = scene->parallaxRooms[primary->index].rot;
                        g_EditorState.gizmo_drag_object_start_scale = (Vec3){ scene->parallaxRooms[primary->index].size.x, scene->parallaxRooms[primary->index].size.y, 1.0f };
                        break;
                    default: break;
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
                    float dot_product = fabsf(vec3_dot(axis_dir, cam_forward));
                    if (dot_product > 0.99f) { if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) { g_EditorState.gizmo_drag_plane_normal = (Vec3){ 0, 1, 0 }; } else { g_EditorState.gizmo_drag_plane_normal = (Vec3){ 1, 0, 0 }; } }
                    else { g_EditorState.gizmo_drag_plane_normal = vec3_cross(axis_dir, cam_forward); vec3_normalize(&g_EditorState.gizmo_drag_plane_normal); }
                    g_EditorState.gizmo_drag_plane_d = -vec3_dot(g_EditorState.gizmo_drag_plane_normal, drag_object_anchor_pos);
                    Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                    float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                    float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                    Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                    Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                    Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
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
                if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) g_EditorState.gizmo_drag_plane_normal = (Vec3){ 1,0,0 };
                if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) g_EditorState.gizmo_drag_plane_normal = (Vec3){ 0,1,0 };
                if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) g_EditorState.gizmo_drag_plane_normal = (Vec3){ 0,0,1 };
                Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
                Vec3 intersect_point;
                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, -vec3_dot(g_EditorState.gizmo_drag_plane_normal, object_pos_for_rotate_plane), &intersect_point)) {
                    g_EditorState.gizmo_rotation_start_vec = vec3_sub(intersect_point, object_pos_for_rotate_plane);
                    vec3_normalize(&g_EditorState.gizmo_rotation_start_vec);
                }
                break;
            }
            }
            return;
        }
        else if (active_viewport >= VIEW_TOP_XZ && !g_EditorState.is_manipulating_gizmo && primary && primary->type == ENTITY_BRUSH) {
            Brush* b = &scene->brushes[primary->index];
            Vec3 mouse_world_pos = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
            float pick_dist_sq = (g_EditorState.ortho_cam_zoom[active_viewport - 1] * 0.05f);
            pick_dist_sq *= pick_dist_sq;
            for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                float dist_sq = 0;
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
            int picked_vertex = Editor_PickVertexAtScreenPos(scene, g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE], VIEW_PERSPECTIVE);
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
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Move Brush");
        }
        if (g_EditorState.is_dragging_preview_brush_handle) {
            g_EditorState.is_dragging_preview_brush_handle = false;
            g_EditorState.preview_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
        }
        else if (g_EditorState.is_dragging_preview_brush_body) {
            Vec3 current_mouse_world_unprojected = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[g_EditorState.preview_brush_drag_body_view], g_EditorState.preview_brush_drag_body_view);
            Vec3 delta = vec3_sub(current_mouse_world_unprojected, g_EditorState.preview_brush_drag_body_start_mouse_world);

            Vec3 current_brush_min_before_move = g_EditorState.preview_brush_world_min;
            Vec3 current_brush_max_before_move = g_EditorState.preview_brush_world_max;
            Vec3 brush_size = vec3_sub(current_brush_max_before_move, current_brush_min_before_move);

            Vec3 new_world_min = vec3_add(g_EditorState.preview_brush_drag_body_start_brush_world_min_at_drag_start, delta);
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
            new_world_max = vec3_add(new_world_min, brush_size);

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
        for (int i = 0; i < VIEW_COUNT; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                active_viewport = (ViewportType)i;
                break;
            }
        }
        if (g_EditorState.is_painting) {
            Brush* b = &scene->brushes[primary->index];
            bool needs_update = false;

            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    Vec3 mouse_world_pos = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                    float radius_sq = g_EditorState.paint_brush_radius * g_EditorState.paint_brush_radius;

                    for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                        float dist_sq = 0;
                        if (i == VIEW_TOP_XZ) dist_sq = (vert_world_pos.x - mouse_world_pos.x) * (vert_world_pos.x - mouse_world_pos.x) + (vert_world_pos.z - mouse_world_pos.z) * (vert_world_pos.z - mouse_world_pos.z);
                        if (i == VIEW_FRONT_XY) dist_sq = (vert_world_pos.x - mouse_world_pos.x) * (vert_world_pos.x - mouse_world_pos.x) + (vert_world_pos.y - mouse_world_pos.y) * (vert_world_pos.y - mouse_world_pos.y);
                        if (i == VIEW_SIDE_YZ) dist_sq = (vert_world_pos.z - mouse_world_pos.z) * (vert_world_pos.z - mouse_world_pos.z) + (vert_world_pos.y - mouse_world_pos.y) * (vert_world_pos.y - mouse_world_pos.y);

                        if (dist_sq < radius_sq) {
                            float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.paint_brush_radius;
                            float blend_amount = g_EditorState.paint_brush_strength * falloff * engine->deltaTime * 10.0f;
                            float* channel_to_paint = NULL;
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
            bool needs_update = false;

            if (SDL_GetModState() & KMOD_SHIFT) {
                Vec3* average_positions = (Vec3*)calloc(b->numVertices, sizeof(Vec3));
                if (average_positions) {
                    Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
                    Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                    for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        local_min.x = fminf(local_min.x, b->vertices[v_idx].pos.x);
                        local_min.y = fminf(local_min.y, b->vertices[v_idx].pos.y);
                        local_min.z = fminf(local_min.z, b->vertices[v_idx].pos.z);
                        local_max.x = fmaxf(local_max.x, b->vertices[v_idx].pos.x);
                        local_max.y = fmaxf(local_max.y, b->vertices[v_idx].pos.y);
                        local_max.z = fmaxf(local_max.z, b->vertices[v_idx].pos.z);
                    }

                    for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                        float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
                        float dist_sq_from_brush = vec3_length_sq(vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                        if (dist_sq_from_brush < radius_sq) {
                            Vec3 neighbor_sum = { 0,0,0 };
                            int neighbor_count = 0;
                            for (int n_idx = 0; n_idx < b->numVertices; ++n_idx) {
                                if (v_idx == n_idx) continue;
                                float dist_sq_verts = vec3_length_sq(vec3_sub(b->vertices[v_idx].pos, b->vertices[n_idx].pos));
                                if (dist_sq_verts < (g_EditorState.grid_size * g_EditorState.grid_size * 2.0f)) {
                                    neighbor_sum = vec3_add(neighbor_sum, b->vertices[n_idx].pos);
                                    neighbor_count++;
                                }
                            }
                            if (neighbor_count > 0) average_positions[v_idx] = vec3_muls(neighbor_sum, 1.0f / neighbor_count);
                            else average_positions[v_idx] = b->vertices[v_idx].pos;
                        }
                        else {
                            average_positions[v_idx] = b->vertices[v_idx].pos;
                        }
                    }

                    for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                        Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                        float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
                        float dist_sq_from_brush = vec3_length_sq(vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                        if (dist_sq_from_brush < radius_sq) {
                            float falloff = 1.0f - sqrtf(dist_sq_from_brush) / g_EditorState.sculpt_brush_radius;
                            float smooth_strength = g_EditorState.sculpt_brush_strength * falloff * engine->unscaledDeltaTime * 1.5f;

                            Vec3 new_pos = vec3_add(vec3_muls(b->vertices[v_idx].pos, 1.0f - smooth_strength), vec3_muls(average_positions[v_idx], smooth_strength));

                            new_pos.x = fmaxf(local_min.x, fminf(local_max.x, new_pos.x));
                            new_pos.y = fmaxf(local_min.y, fminf(local_max.y, new_pos.y));
                            new_pos.z = fmaxf(local_min.z, fminf(local_max.z, new_pos.z));

                            b->vertices[v_idx].pos = new_pos;
                            needs_update = true;
                        }
                    }
                    free(average_positions);
                }
            }
            else {
                float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
                for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                    Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                    float dist_sq = vec3_length_sq(vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                    if (dist_sq < radius_sq) {
                        float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.sculpt_brush_radius;
                        float sculpt_amount = g_EditorState.sculpt_brush_strength * falloff * engine->unscaledDeltaTime * 10.0f;
                        if (SDL_GetModState() & KMOD_CTRL) sculpt_amount = -sculpt_amount;

                        b->vertices[v_idx].pos = vec3_add(b->vertices[v_idx].pos, vec3_muls(g_EditorState.paint_brush_world_normal, sculpt_amount));
                        needs_update = true;
                    }
                }
            }

            if (needs_update) {
                Brush_CreateRenderData(b);
                if (b->physicsBody) {
                    Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                    if (!b->isTrigger && !b->isWater && b->numVertices > 0) {
                        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                        for (int k = 0; k < b->numVertices; ++k) world_verts[k] = mat4_mul_vec3(&b->modelMatrix, b->vertices[k].pos);
                        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                        free(world_verts);
                    }
                    else {
                        b->physicsBody = NULL;
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
            Brush* b = &scene->brushes[primary->index];
            Vec3 current_mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[g_EditorState.selected_brush_drag_body_view], g_EditorState.selected_brush_drag_body_view);
            Vec3 delta = vec3_sub(current_mouse_world, g_EditorState.selected_brush_drag_body_start_mouse_world);

            b->pos = vec3_add(g_EditorState.selected_brush_drag_body_start_brush_pos, delta);

            if (g_EditorState.snap_to_grid) {
                current_mouse_world.x = SnapValue(current_mouse_world.x, g_EditorState.grid_size);
                current_mouse_world.y = SnapValue(current_mouse_world.y, g_EditorState.grid_size);
                current_mouse_world.z = SnapValue(current_mouse_world.z, g_EditorState.grid_size);
            }

            delta = vec3_sub(current_mouse_world, g_EditorState.selected_brush_drag_body_start_mouse_world);
            b->pos = vec3_add(g_EditorState.selected_brush_drag_body_start_brush_pos, delta);

            if (g_EditorState.texture_lock_enabled) {
                for (int i = 0; i < b->numFaces; ++i) {
                    b->faces[i].uv_offset.x -= delta.x / b->faces[i].uv_scale.x;
                    b->faces[i].uv_offset.y -= delta.z / b->faces[i].uv_scale.y;
                }
                Brush_CreateRenderData(b);
            }

            Brush_UpdateMatrix(b);
            if (b->physicsBody) {
                Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
            }
        }
        else if (g_EditorState.is_manipulating_vertex_gizmo) {
            Brush* b = &scene->brushes[primary->index];
            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
            float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
            Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
            Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
            Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
            Vec3 current_intersect_point;
            if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.vertex_gizmo_drag_plane_normal, g_EditorState.vertex_gizmo_drag_plane_d, &current_intersect_point)) {
                Vec3 delta = vec3_sub(current_intersect_point, g_EditorState.vertex_gizmo_drag_start_world);
                Vec3 axis_dir = { 0 };
                if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
                if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
                if (g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
                float projection_len = vec3_dot(delta, axis_dir);
                Vec3 projected_delta = vec3_muls(axis_dir, projection_len);
                Vec3 new_world_pos = vec3_add(g_EditorState.vertex_drag_start_pos_world, projected_delta);
                if (g_EditorState.snap_to_grid) { new_world_pos.x = SnapValue(new_world_pos.x, g_EditorState.grid_size); new_world_pos.y = SnapValue(new_world_pos.y, g_EditorState.grid_size); new_world_pos.z = SnapValue(new_world_pos.z, g_EditorState.grid_size); }
                Mat4 inv_model;
                mat4_inverse(&b->modelMatrix, &inv_model);
                b->vertices[primary->vertex_index].pos = mat4_mul_vec3(&inv_model, new_world_pos);
                Brush_CreateRenderData(b);
                if (b->physicsBody) {
                    Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                    if (!b->isTrigger && b->numVertices > 0) {
                        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                        for (int i = 0; i < b->numVertices; ++i) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                        free(world_verts);
                    }
                    else {
                        b->physicsBody = NULL;
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
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
                Vec3 current_intersect_point;
                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &current_intersect_point)) {
                    delta = vec3_sub(current_intersect_point, g_EditorState.gizmo_drag_start_world);
                }
            }
            else {
                Vec3 current_point = ScreenToWorld(screen_pos, g_EditorState.gizmo_drag_view);
                delta = vec3_sub(current_point, g_EditorState.gizmo_drag_start_world);
            }

            Vec3 axis_dir = { 0 };
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
            float projection_len = vec3_dot(delta, axis_dir);
            Vec3 projected_delta = vec3_muls(axis_dir, projection_len);

            if (g_EditorState.snap_to_grid) {
                projected_delta.x = SnapValue(projected_delta.x, g_EditorState.grid_size);
                projected_delta.y = SnapValue(projected_delta.y, g_EditorState.grid_size);
                projected_delta.z = SnapValue(projected_delta.z, g_EditorState.grid_size);
            }

            Mat4 inv_model; mat4_inverse(&b->modelMatrix, &inv_model);
            for (int i = 0; i < face->numVertexIndices; ++i) {
                int vert_idx = face->vertexIndices[i];
                Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[vert_idx].pos);
                Vec3 new_world_pos = vec3_add(vert_world_pos, projected_delta);
                b->vertices[vert_idx].pos = mat4_mul_vec3(&inv_model, new_world_pos);
            }

            Brush_CreateRenderData(b);
            if (b->physicsBody) {
                Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                if (!b->isTrigger && b->numVertices > 0) {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int j = 0; j < b->numVertices; ++j) world_verts[j] = mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
                else { b->physicsBody = NULL; }
            }

            g_EditorState.gizmo_drag_start_world = vec3_add(g_EditorState.gizmo_drag_start_world, projected_delta);

            return;
        }
        else if (g_EditorState.is_vertex_manipulating) {
            Brush* b = &scene->brushes[primary->index];
            Vec3 current_mouse_world = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.vertex_manipulation_view], g_EditorState.vertex_manipulation_view);
            Vec3* vert_local_pos = &b->vertices[g_EditorState.manipulated_vertex_index].pos;
            Mat4 inv_model;
            mat4_inverse(&b->modelMatrix, &inv_model);
            Vec3 vert_world = mat4_mul_vec3(&b->modelMatrix, *vert_local_pos);
            if (g_EditorState.vertex_manipulation_view == VIEW_TOP_XZ) { vert_world.x = current_mouse_world.x; vert_world.z = current_mouse_world.z; }
            if (g_EditorState.vertex_manipulation_view == VIEW_FRONT_XY) { vert_world.x = current_mouse_world.x; vert_world.y = current_mouse_world.y; }
            if (g_EditorState.vertex_manipulation_view == VIEW_SIDE_YZ) { vert_world.y = current_mouse_world.y; vert_world.z = current_mouse_world.z; }
            *vert_local_pos = mat4_mul_vec3(&inv_model, vert_world);
            Brush_CreateRenderData(b);
            if (b->physicsBody) {
                Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                if (!b->isTrigger && b->numVertices > 0) {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int i = 0; i < b->numVertices; ++i) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
                else {
                    b->physicsBody = NULL;
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
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);
                Vec3 current_intersect_point;
                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &current_intersect_point)) {
                    delta = vec3_sub(current_intersect_point, g_EditorState.gizmo_drag_start_world);
                }
            }
            else {
                Vec3 current_point = ScreenToWorld(screen_pos, g_EditorState.gizmo_drag_view);
                delta = vec3_sub(current_point, g_EditorState.gizmo_drag_start_world);
            }

            Vec3 axis_dir = { 0 };
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
            if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
            float projection_len = vec3_dot(delta, axis_dir);
            Vec3 projected_delta = vec3_muls(axis_dir, projection_len);

            if (g_EditorState.snap_to_grid) {
                projected_delta.x = SnapValue(projected_delta.x, g_EditorState.grid_size);
                projected_delta.y = SnapValue(projected_delta.y, g_EditorState.grid_size);
                projected_delta.z = SnapValue(projected_delta.z, g_EditorState.grid_size);
            }

            Mat4 inv_model; mat4_inverse(&b->modelMatrix, &inv_model);
            for (int i = 0; i < face->numVertexIndices; ++i) {
                int vert_idx = face->vertexIndices[i];
                Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[vert_idx].pos);
                Vec3 new_world_pos = vec3_add(vert_world_pos, projected_delta);
                b->vertices[vert_idx].pos = mat4_mul_vec3(&inv_model, new_world_pos);
            }

            Brush_CreateRenderData(b);
            if (b->physicsBody) {
                Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                if (!b->isTrigger && b->numVertices > 0) {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int j = 0; j < b->numVertices; ++j) world_verts[j] = mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
                else { b->physicsBody = NULL; }
            }

            g_EditorState.gizmo_drag_start_world = vec3_add(g_EditorState.gizmo_drag_start_world, projected_delta);

            return;
        }
        else if (g_EditorState.is_manipulating_gizmo) {
            Vec3 pos_delta = { 0 };
            Vec3 scale_delta = { 0 };
            float rot_angle_delta = 0.0f;
            Mat4 delta_rot_matrix;
            mat4_identity(&delta_rot_matrix);

            Vec3 current_intersect_point;
            bool intersection_found = false;

            if (g_EditorState.gizmo_drag_view == VIEW_PERSPECTIVE) {
                Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);

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
                    Vec3 current_vec = vec3_sub(current_intersect_point, object_pos_for_rotate);
                    vec3_normalize(&current_vec);
                    Vec3 u_axis = g_EditorState.gizmo_rotation_start_vec;
                    Vec3 v_axis = vec3_cross(g_EditorState.gizmo_drag_plane_normal, u_axis);

                    float u_coord = vec3_dot(current_vec, u_axis);
                    float v_coord = vec3_dot(current_vec, v_axis);

                    float angle = atan2f(v_coord, u_coord) * (180.0f / M_PI);

                    if (g_EditorState.snap_to_grid) {
                        angle = SnapAngle(angle, 15.0f);
                    }
                    rot_angle_delta = angle;

                    float angle_rad = rot_angle_delta * (M_PI / 180.0f);
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) delta_rot_matrix = mat4_rotate_x(angle_rad);
                    else if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) delta_rot_matrix = mat4_rotate_y(angle_rad);
                    else delta_rot_matrix = mat4_rotate_z(angle_rad);
                }
                else {
                    Vec3 delta = vec3_sub(current_intersect_point, g_EditorState.gizmo_drag_start_world);
                    Vec3 axis_dir = { 0 };
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
                    float projection_len = vec3_dot(delta, axis_dir);

                    if (g_EditorState.current_gizmo_operation == GIZMO_OP_TRANSLATE) {
                        if (g_EditorState.snap_to_grid) projection_len = SnapValue(projection_len, g_EditorState.grid_size);
                        pos_delta = vec3_muls(axis_dir, projection_len);
                    }
                    else {
                        if (g_EditorState.snap_to_grid) projection_len = SnapValue(projection_len, 0.25f);
                        scale_delta = vec3_muls(axis_dir, projection_len);
                    }
                }
            }

            Vec3 centroid = g_EditorState.gizmo_selection_centroid;
            for (int i = 0; i < g_EditorState.num_selections; ++i) {
                EditorSelection* sel = &g_EditorState.selections[i];

                Vec3 start_pos = g_EditorState.gizmo_drag_start_positions[i];
                Vec3 start_rot_eulers = g_EditorState.gizmo_drag_start_rotations[i];
                Vec3 start_scale = g_EditorState.gizmo_drag_start_scales[i];

                Vec3 new_pos = start_pos;
                Vec3 new_rot = start_rot_eulers;
                Vec3 new_scale = start_scale;

                if (g_EditorState.current_gizmo_operation == GIZMO_OP_TRANSLATE) {
                    new_pos = vec3_add(start_pos, pos_delta);
                }
                else if (g_EditorState.current_gizmo_operation == GIZMO_OP_SCALE) {
                    new_scale = vec3_add(start_scale, scale_delta);
                }
                else if (g_EditorState.current_gizmo_operation == GIZMO_OP_ROTATE) {
                    new_rot = start_rot_eulers;

                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) {
                        new_rot.x = start_rot_eulers.x + rot_angle_delta;
                    }
                    else if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) {
                        new_rot.y = start_rot_eulers.y + rot_angle_delta;
                    }
                    else if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) {
                        new_rot.z = start_rot_eulers.z + rot_angle_delta;
                    }
                }

                switch (sel->type) {
                case ENTITY_MODEL: {
                    SceneObject* obj = &scene->objects[sel->index];
                    obj->pos = new_pos; obj->rot = new_rot; obj->scale = new_scale;
                    SceneObject_UpdateMatrix(obj); if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); break;
                }
                case ENTITY_BRUSH: {
                    Brush* b = &scene->brushes[sel->index];
                    b->pos = new_pos; b->rot = new_rot; b->scale = new_scale;
                    Brush_UpdateMatrix(b); if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix); break;
                }
                case ENTITY_LIGHT: {
                    Light* l = &scene->lights[sel->index];
                    l->position = new_pos; l->rot = new_rot; break;
                }
                case ENTITY_DECAL: {
                    Decal* d = &scene->decals[sel->index];
                    d->pos = new_pos; d->rot = new_rot; d->size = new_scale;
                    Decal_UpdateMatrix(d); break;
                }
                case ENTITY_SOUND: {
                    SoundEntity* s = &scene->soundEntities[sel->index];
                    s->pos = new_pos; SoundSystem_SetSourcePosition(s->sourceID, s->pos); break;
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
                    scene->playerStart.position = new_pos; break;
                }
                default: break;
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
        else if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
                    if (g_EditorState.is_viewport_focused[VIEW_TOP_XZ]) { float ms = g_EditorState.ortho_cam_zoom[0] * 0.002f; g_EditorState.ortho_cam_pos[0].x -= event->motion.xrel * ms; g_EditorState.ortho_cam_pos[0].z -= event->motion.yrel * ms; }
                    if (g_EditorState.is_viewport_focused[VIEW_FRONT_XY]) { float ms = g_EditorState.ortho_cam_zoom[1] * 0.002f; g_EditorState.ortho_cam_pos[1].x -= event->motion.xrel * ms; g_EditorState.ortho_cam_pos[1].y += event->motion.yrel * ms; }
                    if (g_EditorState.is_viewport_focused[VIEW_SIDE_YZ]) { float ms = g_EditorState.ortho_cam_zoom[2] * 0.002f; g_EditorState.ortho_cam_pos[2].z += event->motion.xrel * ms; g_EditorState.ortho_cam_pos[2].y += event->motion.yrel * ms; }
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
        bool hovered_any_viewport = false;
        for (int i = 1; i < VIEW_COUNT; i++) {
            if (g_EditorState.is_viewport_hovered[i]) { g_EditorState.ortho_cam_zoom[i - 1] -= event->wheel.y * g_EditorState.ortho_cam_zoom[i - 1] * 0.1f; hovered_any_viewport = true; }
        }
    }
    if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        EditorSelection* primary = Editor_GetPrimarySelection();
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_z) { Undo_PerformUndo(scene, engine); return; }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_y) { Undo_PerformRedo(scene, engine); return; }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_s) {
            if (strcmp(g_EditorState.currentMapPath, "untitled.map") == 0) {
                g_EditorState.show_save_map_popup = true;
            }
            else {
                Scene_SaveMap(scene, NULL, g_EditorState.currentMapPath);
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
            if (primary) {
                switch (primary->type) {
                case ENTITY_MODEL: Editor_DuplicateModel(scene, engine, primary->index); break;
                case ENTITY_BRUSH: Editor_DuplicateBrush(scene, engine, primary->index); break;
                case ENTITY_LIGHT: Editor_DuplicateLight(scene, primary->index); break;
                case ENTITY_DECAL: Editor_DuplicateDecal(scene, primary->index); break;
                case ENTITY_SOUND: Editor_DuplicateSoundEntity(scene, primary->index); break;
                case ENTITY_PARTICLE_EMITTER: Editor_DuplicateParticleEmitter(scene, primary->index); break;
                case ENTITY_VIDEO_PLAYER: Editor_DuplicateVideoPlayer(scene, primary->index); break;
                case ENTITY_PARALLAX_ROOM: Editor_DuplicateParallaxRoom(scene, primary->index); break;
                case ENTITY_LOGIC: Editor_DuplicateLogicEntity(scene, engine, primary->index); break;
                default: Console_Printf("Duplication not implemented for this entity type yet."); break;
                }
            }
            return;
        }
        if (event->key.keysym.sym == SDLK_z) {
            if (g_EditorState.is_in_z_mode) {
                g_EditorState.is_in_z_mode = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
            }
            else {
                for (int i = 0; i < VIEW_COUNT; ++i) {
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
                float target_size = 1.0f;

                switch (primary->type) {
                case ENTITY_MODEL: {
                    SceneObject* obj = &scene->objects[primary->index];
                    target_pos = obj->pos;
                    Vec3 size_vec = vec3_sub(obj->model->aabb_max, obj->model->aabb_min);
                    target_size = fmaxf(fmaxf(size_vec.x * obj->scale.x, size_vec.y * obj->scale.y), size_vec.z * obj->scale.z);
                    break;
                }
                case ENTITY_BRUSH: {
                    Brush* b = &scene->brushes[primary->index];
                    target_pos = b->pos;
                    if (b->numVertices > 0) {
                        Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
                        Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                        for (int i = 0; i < b->numVertices; ++i) {
                            local_min.x = fminf(local_min.x, b->vertices[i].pos.x);
                            local_min.y = fminf(local_min.y, b->vertices[i].pos.y);
                            local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
                            local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x);
                            local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y);
                            local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
                        }
                        Vec3 size_vec = vec3_sub(local_max, local_min);
                        target_size = fmaxf(fmaxf(size_vec.x * b->scale.x, size_vec.y * b->scale.y), size_vec.z * b->scale.z);
                    }
                    break;
                }
                case ENTITY_LIGHT:              target_pos = scene->lights[primary->index].position; break;
                case ENTITY_PLAYERSTART:        target_pos = scene->playerStart.position; break;
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
                vec3_normalize(&cam_forward);

                float distance_away = target_size * 2.0f;
                if (distance_away < 2.0f) distance_away = 2.0f;

                Vec3 new_cam_pos = vec3_sub(target_pos, vec3_muls(cam_forward, distance_away));
                g_EditorState.editor_camera.position = new_cam_pos;

                Vec3 new_forward = vec3_sub(target_pos, new_cam_pos);
                vec3_normalize(&new_forward);

                g_EditorState.editor_camera.pitch = asinf(new_forward.y);
                g_EditorState.editor_camera.yaw = atan2f(new_forward.x, -new_forward.z);
            }
            if (primary && primary->type == ENTITY_BRUSH && primary->vertex_index != -1) {
                bool moved = false;
                Vec3 move_delta = { 0 };
                float grid_size = g_EditorState.grid_size;

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
                    Mat4 rot_mat_x = mat4_rotate_x(b->rot.x * (M_PI / 180.0f));
                    Mat4 rot_mat_y = mat4_rotate_y(b->rot.y * (M_PI / 180.0f));
                    Mat4 rot_mat_z = mat4_rotate_z(b->rot.z * (M_PI / 180.0f));
                    Mat4 scale_mat = mat4_scale(b->scale);
                    mat4_multiply(&inv_rot_scale, &rot_mat_y, &rot_mat_x);
                    mat4_multiply(&inv_rot_scale, &rot_mat_z, &inv_rot_scale);
                    mat4_multiply(&inv_rot_scale, &inv_rot_scale, &scale_mat);
                    mat4_inverse(&inv_rot_scale, &inv_rot_scale);

                    Vec3 local_move_delta = mat4_mul_vec3_dir(&inv_rot_scale, move_delta);

                    b->vertices[primary->vertex_index].pos = vec3_add(b->vertices[primary->vertex_index].pos, local_move_delta);
                    Brush_CreateRenderData(b);
                    if (b->physicsBody) {
                        Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                        if (!b->isTrigger && !b->isWater && b->numVertices > 0) {
                            Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                            for (int i = 0; i < b->numVertices; ++i) {
                                world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                            }
                            b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                            free(world_verts);
                        }
                        else {
                            b->physicsBody = NULL;
                        }
                    }
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Nudge Vertex");
                }
            }
            if (event->key.keysym.sym == SDLK_1) g_EditorState.current_gizmo_operation = GIZMO_OP_TRANSLATE;
            if (event->key.keysym.sym == SDLK_2) g_EditorState.current_gizmo_operation = GIZMO_OP_ROTATE;
            if (event->key.keysym.sym == SDLK_3) g_EditorState.current_gizmo_operation = GIZMO_OP_SCALE;
            if (event->key.keysym.sym == SDLK_LEFTBRACKET) {
                g_EditorState.grid_size /= 2.0f;
                if (g_EditorState.grid_size < 0.125f) g_EditorState.grid_size = 0.125f;
            }
            if (event->key.keysym.sym == SDLK_RIGHTBRACKET) {
                g_EditorState.grid_size *= 2.0f;
                if (g_EditorState.grid_size > 64.0f) g_EditorState.grid_size = 64.0f;
            }
            if (event->key.keysym.sym == SDLK_DELETE) {
                if (g_EditorState.num_selections > 0) {
                    EntityState* deleted_states = calloc(g_EditorState.num_selections, sizeof(EntityState));
                    int num_deleted = 0;
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        capture_state(&deleted_states[num_deleted++], scene, g_EditorState.selections[i].type, g_EditorState.selections[i].index);
                    }
                    Undo_PushDeleteMultipleEntities(scene, deleted_states, num_deleted, "Delete Selection");
                    for (int i = g_EditorState.num_selections - 1; i >= 0; --i) {
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
                        default: break;
                        }
                    }
                }
                Editor_ClearSelection();
            }
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_RIGHT && !g_EditorState.is_in_z_mode) {
        EditorSelection* primary = Editor_GetPrimarySelection();
        for (int i = 0; i < VIEW_COUNT; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                if (primary && primary->type == ENTITY_BRUSH && primary->face_index != -1) {
                    g_EditorState.show_texture_browser = true;
                }
                break;
            }
        }
    }
}
void Editor_RenderGrid(ViewportType type, float aspect) {
    glUseProgram(g_EditorState.grid_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.grid_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.grid_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
    Mat4 model_ident; mat4_identity(&model_ident);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.grid_shader, "model"), 1, GL_FALSE, model_ident.m);
    float grid_lines[2412]; int line_count = 0;
    if (type == VIEW_PERSPECTIVE) {
        float spacing = g_EditorState.grid_size; int num_lines = 200; float extent = (num_lines / 2.0f) * spacing;
        Vec3 cam_pos = g_EditorState.editor_camera.position;
        float center_x = roundf(cam_pos.x / (spacing * 10.0f)) * (spacing * 10.0f); float center_z = roundf(cam_pos.z / (spacing * 10.0f)) * (spacing * 10.0f);
        for (int i = 0; i <= num_lines; ++i) {
            float p = -extent + i * spacing;
            grid_lines[line_count++] = center_x + p; grid_lines[line_count++] = 0.0f; grid_lines[line_count++] = center_z - extent; grid_lines[line_count++] = center_x + p; grid_lines[line_count++] = 0.0f; grid_lines[line_count++] = center_z + extent;
            grid_lines[line_count++] = center_x - extent; grid_lines[line_count++] = 0.0f; grid_lines[line_count++] = center_z + p; grid_lines[line_count++] = center_x + extent; grid_lines[line_count++] = 0.0f; grid_lines[line_count++] = center_z + p;
        }
    }
    else {
        float zoom = g_EditorState.ortho_cam_zoom[type - 1]; float spacing = g_EditorState.grid_size; Vec3 center = g_EditorState.ortho_cam_pos[type - 1];
        float left, right, bottom, top;
        if (type == VIEW_TOP_XZ) { left = center.x - zoom * aspect; right = center.x + zoom * aspect; bottom = center.z - zoom; top = center.z + zoom; }
        else if (type == VIEW_FRONT_XY) { left = center.x - zoom * aspect; right = center.x + zoom * aspect; bottom = center.y - zoom; top = center.y + zoom; }
        else { left = center.z - zoom * aspect; right = center.z + zoom * aspect; bottom = center.y - zoom; top = center.y + zoom; }
        float start_x = floorf(left / spacing) * spacing;
        for (float x = start_x; x <= right && line_count < 2400; x += spacing) {
            if (type == VIEW_TOP_XZ) { grid_lines[line_count++] = x; grid_lines[line_count++] = 0; grid_lines[line_count++] = bottom; grid_lines[line_count++] = x; grid_lines[line_count++] = 0; grid_lines[line_count++] = top; }
            else if (type == VIEW_FRONT_XY) { grid_lines[line_count++] = x; grid_lines[line_count++] = bottom; grid_lines[line_count++] = 0; grid_lines[line_count++] = x; grid_lines[line_count++] = top; grid_lines[line_count++] = 0; }
            else { grid_lines[line_count++] = 0; grid_lines[line_count++] = bottom; grid_lines[line_count++] = x; grid_lines[line_count++] = 0; grid_lines[line_count++] = top; grid_lines[line_count++] = x; }
        }
        float start_y = floorf(bottom / spacing) * spacing;
        for (float y = start_y; y <= top && line_count < 2400; y += spacing) {
            if (type == VIEW_TOP_XZ) { grid_lines[line_count++] = left; grid_lines[line_count++] = 0; grid_lines[line_count++] = y; grid_lines[line_count++] = right; grid_lines[line_count++] = 0; grid_lines[line_count++] = y; }
            else if (type == VIEW_FRONT_XY) { grid_lines[line_count++] = left; grid_lines[line_count++] = y; grid_lines[line_count++] = 0; grid_lines[line_count++] = right; grid_lines[line_count++] = y; grid_lines[line_count++] = 0; }
            else { grid_lines[line_count++] = 0; grid_lines[line_count++] = y; grid_lines[line_count++] = left; grid_lines[line_count++] = 0; grid_lines[line_count++] = y; grid_lines[line_count++] = right; }
        }
    }
    if (line_count == 0) return;
    if (g_EditorState.grid_vao == 0) { glGenVertexArrays(1, &g_EditorState.grid_vao); glGenBuffers(1, &g_EditorState.grid_vbo); }
    glBindVertexArray(g_EditorState.grid_vao); glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, line_count * sizeof(float), grid_lines, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0); float color[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    glUniform4fv(glGetUniformLocation(g_EditorState.grid_shader, "grid_color"), 1, color);
    glDrawArrays(GL_LINES, 0, line_count / 3); glBindVertexArray(0);
}
static float rand_float_range(float min, float max) {
    if (min >= max) return min;
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}
void Editor_Update(Engine* engine, Scene* scene) {
    bool can_move = g_EditorState.is_in_z_mode || (g_EditorState.is_viewport_focused[VIEW_PERSPECTIVE] && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)));
    if (can_move) {
        const Uint8* state = SDL_GetKeyboardState(NULL); float speed = g_EditorState.editor_camera_speed * engine->deltaTime * (state[SDL_SCANCODE_LSHIFT] ? 2.5f : 1.0f);
        Vec3 forward = { cosf(g_EditorState.editor_camera.pitch) * sinf(g_EditorState.editor_camera.yaw), sinf(g_EditorState.editor_camera.pitch), -cosf(g_EditorState.editor_camera.pitch) * cosf(g_EditorState.editor_camera.yaw) };
        vec3_normalize(&forward); Vec3 right = vec3_cross(forward, (Vec3) { 0, 1, 0 }); vec3_normalize(&right);
        if (state[SDL_SCANCODE_W]) g_EditorState.editor_camera.position = vec3_add(g_EditorState.editor_camera.position, vec3_muls(forward, speed));
        if (state[SDL_SCANCODE_S]) g_EditorState.editor_camera.position = vec3_sub(g_EditorState.editor_camera.position, vec3_muls(forward, speed));
        if (state[SDL_SCANCODE_D]) g_EditorState.editor_camera.position = vec3_add(g_EditorState.editor_camera.position, vec3_muls(right, speed));
        if (state[SDL_SCANCODE_A]) g_EditorState.editor_camera.position = vec3_sub(g_EditorState.editor_camera.position, vec3_muls(right, speed));
        if (state[SDL_SCANCODE_E]) g_EditorState.editor_camera.position.y += speed;
        if (state[SDL_SCANCODE_Q]) g_EditorState.editor_camera.position.y -= speed;
    }

    for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
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

        float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
        Mat4 inv_proj, inv_view;
        mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
        mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);

        Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
        Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
        ray_eye.z = -1.0f; ray_eye.w = 0.0f;

        Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
        Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
        vec3_normalize(&ray_dir);
        Vec3 ray_origin = g_EditorState.editor_camera.position;

        Mat4 inv_brush_model_matrix;
        if (mat4_inverse(&b->modelMatrix, &inv_brush_model_matrix)) {
            Vec3 ray_origin_local = mat4_mul_vec3(&inv_brush_model_matrix, ray_origin);
            Vec3 ray_dir_local = mat4_mul_vec3_dir(&inv_brush_model_matrix, ray_dir);
            float closest_t = FLT_MAX;

            for (int face_idx = 0; face_idx < b->numFaces; ++face_idx) {
                BrushFace* face = &b->faces[face_idx];
                if (face->numVertexIndices < 3) continue;

                for (int k = 0; k < face->numVertexIndices - 2; ++k) {
                    Vec3 v0_local = b->vertices[face->vertexIndices[0]].pos;
                    Vec3 v1_local = b->vertices[face->vertexIndices[k + 1]].pos;
                    Vec3 v2_local = b->vertices[face->vertexIndices[k + 2]].pos;

                    float t_triangle_local;
                    if (RayIntersectsTriangle(ray_origin_local, ray_dir_local, v0_local, v1_local, v2_local, &t_triangle_local)) {
                        if (t_triangle_local > 0.0f && t_triangle_local < closest_t) {
                            closest_t = t_triangle_local;
                            g_EditorState.paint_brush_hit_surface = true;
                            g_EditorState.paint_brush_world_pos = vec3_add(ray_origin, vec3_muls(ray_dir, t_triangle_local));
                            Vec3 face_normal_local = vec3_cross(vec3_sub(v1_local, v0_local), vec3_sub(v2_local, v0_local));
                            g_EditorState.paint_brush_world_normal = mat4_mul_vec3_dir(&b->modelMatrix, face_normal_local);
                            vec3_normalize(&g_EditorState.paint_brush_world_normal);
                        }
                    }
                }
            }
        }

        if (g_EditorState.is_painting) {
            bool needs_update = false;
            float radius_sq = g_EditorState.paint_brush_radius * g_EditorState.paint_brush_radius;
            for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                float dist_sq = vec3_length_sq(vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                if (dist_sq < radius_sq) {
                    float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.paint_brush_radius;
                    float blend_amount = g_EditorState.paint_brush_strength * falloff * engine->unscaledDeltaTime * 10.0f;
                    float* channel_to_paint = NULL;
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
            bool needs_update = false;
            float radius_sq = g_EditorState.sculpt_brush_radius * g_EditorState.sculpt_brush_radius;
            for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                float dist_sq = vec3_length_sq(vec3_sub(vert_world_pos, g_EditorState.paint_brush_world_pos));

                if (dist_sq < radius_sq) {
                    float falloff = 1.0f - sqrtf(dist_sq) / g_EditorState.sculpt_brush_radius;
                    float sculpt_amount = g_EditorState.sculpt_brush_strength * falloff * engine->unscaledDeltaTime * 10.0f;
                    if (SDL_GetModState() & KMOD_SHIFT) sculpt_amount = -sculpt_amount;

                    b->vertices[v_idx].pos = vec3_add(b->vertices[v_idx].pos, vec3_muls(g_EditorState.paint_brush_world_normal, sculpt_amount));
                    needs_update = true;
                }
            }
            if (needs_update) {
                Brush_CreateRenderData(b);
                if (b->physicsBody) {
                    Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                    if (!b->isTrigger && !b->isWater && b->numVertices > 0) {
                        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                        for (int k = 0; k < b->numVertices; ++k) world_verts[k] = mat4_mul_vec3(&b->modelMatrix, b->vertices[k].pos);
                        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                        free(world_verts);
                    }
                    else {
                        b->physicsBody = NULL;
                    }
                }
            }
        }
    }
    if (primary && primary->type == ENTITY_BRUSH && primary->vertex_index != -1 && !g_EditorState.is_manipulating_gizmo && !g_EditorState.is_manipulating_vertex_gizmo) {
        if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
            Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
            float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
            Mat4 inv_proj, inv_view;
            mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
            mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
            Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
            Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
            ray_eye.z = -1.0f; ray_eye.w = 0.0f;
            Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
            Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
            vec3_normalize(&ray_dir);
            Vec3 ray_origin = g_EditorState.editor_camera.position;

            Brush* b = &scene->brushes[primary->index];
            Vec3 vert_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[primary->vertex_index].pos);

            const float pick_threshold = 0.1f;
            float min_dist = FLT_MAX;
            float t_ray, t_seg;

            float GIZMO_AXIS_LENGTH = 0.5f;

            Vec3 x_p1 = { vert_world_pos.x + GIZMO_AXIS_LENGTH, vert_world_pos.y, vert_world_pos.z };
            float dist_x = dist_RaySegment(ray_origin, ray_dir, vert_world_pos, x_p1, &t_ray, &t_seg);
            if (dist_x < pick_threshold && dist_x < min_dist) { min_dist = dist_x; g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_X; }

            Vec3 y_p1 = { vert_world_pos.x, vert_world_pos.y + GIZMO_AXIS_LENGTH, vert_world_pos.z };
            float dist_y = dist_RaySegment(ray_origin, ray_dir, vert_world_pos, y_p1, &t_ray, &t_seg);
            if (dist_y < pick_threshold && dist_y < min_dist) { min_dist = dist_y; g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_Y; }

            Vec3 z_p1 = { vert_world_pos.x, vert_world_pos.y, vert_world_pos.z + GIZMO_AXIS_LENGTH };
            float dist_z = dist_RaySegment(ray_origin, ray_dir, vert_world_pos, z_p1, &t_ray, &t_seg);
            if (dist_z < pick_threshold && dist_z < min_dist) { g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_Z; }
        }
    }
    g_EditorState.sprinkle_brush_hit_surface = false;
    if (g_EditorState.show_sprinkle_tool_window && g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
        Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
        float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
        float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
        Mat4 inv_proj, inv_view;
        mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
        mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
        Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
        Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
        ray_eye.z = -1.0f; ray_eye.w = 0.0f;
        Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
        Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
        vec3_normalize(&ray_dir);
        Vec3 ray_origin = g_EditorState.editor_camera.position;

        RaycastHitInfo hit_info;
        if (Physics_Raycast(engine->physicsWorld, ray_origin, vec3_add(ray_origin, vec3_muls(ray_dir, 1000.0f)), &hit_info)) {
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

                        Vec3 tangent = vec3_cross(surface_normal, (Vec3) { 0.0f, 1.0f, 0.0f });
                        if (vec3_length_sq(tangent) < 0.001f) {
                            tangent = vec3_cross(surface_normal, (Vec3) { 1.0f, 0.0f, 0.0f });
                        }
                        vec3_normalize(&tangent);
                        Vec3 bitangent = vec3_cross(surface_normal, tangent);

                        float rand_angle = rand_float_range(0, 2.0f * M_PI);
                        float rand_dist = sqrtf(rand_float_range(0, 1)) * g_EditorState.sprinkle_radius;

                        Vec3 offset_on_plane = vec3_add(vec3_muls(tangent, cosf(rand_angle) * rand_dist), vec3_muls(bitangent, sinf(rand_angle) * rand_dist));
                        Vec3 final_pos = vec3_add(g_EditorState.sprinkle_brush_world_pos, offset_on_plane);

                        if (scene->numObjects < 8192) {
                            scene->numObjects++;
                            scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
                            if (!scene->objects) {
                                Console_Printf_Error("[ERROR] Failed to reallocate memory for scene objects!");
                                scene->numObjects--;
                                return;
                            }

                            SceneObject* newObj = &scene->objects[scene->numObjects - 1];
                            memset(newObj, 0, sizeof(SceneObject));

                            strncpy(newObj->modelPath, g_EditorState.sprinkle_model_path, sizeof(newObj->modelPath) - 1);
                            newObj->pos = final_pos;
                            float scale = rand_float_range(g_EditorState.sprinkle_scale_min, g_EditorState.sprinkle_scale_max);
                            newObj->scale = (Vec3){ scale, scale, scale };
                            newObj->rot = (Vec3){ 0,0,0 };

                            if (g_EditorState.sprinkle_align_to_normal) {
                                Vec3 obj_forward = surface_normal;
                                Vec3 obj_up = (fabs(obj_forward.y) > 0.99f) ? (Vec3) { 1, 0, 0 } : (Vec3) { 0, 1, 0 };
                                Vec3 obj_right = vec3_cross(obj_up, obj_forward);
                                vec3_normalize(&obj_right);
                                obj_up = vec3_cross(obj_forward, obj_right);

                                Mat4 rot_matrix;
                                rot_matrix.m[0] = obj_right.x;  rot_matrix.m[4] = obj_up.x;  rot_matrix.m[8] = obj_forward.x;  rot_matrix.m[12] = 0;
                                rot_matrix.m[1] = obj_right.y;  rot_matrix.m[5] = obj_up.y;  rot_matrix.m[9] = obj_forward.y;  rot_matrix.m[13] = 0;
                                rot_matrix.m[2] = obj_right.z;  rot_matrix.m[6] = obj_up.z;  rot_matrix.m[10] = obj_forward.z; rot_matrix.m[14] = 0;
                                rot_matrix.m[3] = 0;            rot_matrix.m[7] = 0;         rot_matrix.m[11] = 0;              rot_matrix.m[15] = 1;

                                mat4_decompose(&rot_matrix, &(Vec3){0}, & newObj->rot, & (Vec3){0});
                            }

                            if (g_EditorState.sprinkle_random_yaw) {
                                newObj->rot.y = rand_float_range(0, 360.0f);
                            }

                            SceneObject_UpdateMatrix(newObj);
                            newObj->model = Model_Load(newObj->modelPath);
                            Undo_PushCreateEntity(scene, ENTITY_MODEL, scene->numObjects - 1, "Sprinkle Object");
                        }
                    }
                    else {
                        for (int i = scene->numObjects - 1; i >= 0; --i) {
                            if (strcmp(scene->objects[i].modelPath, g_EditorState.sprinkle_model_path) == 0) {
                                float dist_sq = vec3_length_sq(vec3_sub(scene->objects[i].pos, g_EditorState.sprinkle_brush_world_pos));
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
        for (int i = 0; i < b->numVertices; ++i) {
            local_min.x = fminf(local_min.x, b->vertices[i].pos.x);
            local_min.y = fminf(local_min.y, b->vertices[i].pos.y);
            local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
            local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x);
            local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y);
            local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
        }
        Vec3 local_center = vec3_muls(vec3_add(local_min, local_max), 0.5f);

        for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                float handle_pick_dist_sq = powf(g_EditorState.ortho_cam_zoom[i - 1] * 0.055f, 2.0f);

                Vec3 handle_local_positions[6] = {
                    {local_min.x, local_center.y, local_center.z}, {local_max.x, local_center.y, local_center.z},
                    {local_center.x, local_min.y, local_center.z}, {local_center.x, local_max.y, local_center.z},
                    {local_center.x, local_center.y, local_min.z}, {local_center.x, local_center.y, local_max.z}
                };

                for (int h_idx = 0; h_idx < 6; ++h_idx) {
                    Vec3 handle_world_pos = mat4_mul_vec3(&b->modelMatrix, handle_local_positions[h_idx]);
                    float dist_sq = 0.0f;

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
    if (g_EditorState.is_in_brush_creation_mode && !g_EditorState.is_dragging_preview_brush_handle && !g_EditorState.is_manipulating_gizmo) {
        for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);

                float pick_radius_factor = 0.055f;
                float handle_pick_dist_sq = powf(g_EditorState.ortho_cam_zoom[i - 1] * pick_radius_factor, 2.0f);

                Vec3 handle_centers_world[PREVIEW_BRUSH_HANDLE_COUNT];
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MIN_X] = (Vec3){ g_EditorState.preview_brush_world_min.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MAX_X] = (Vec3){ g_EditorState.preview_brush_world_max.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MIN_Y] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush_world_min.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MAX_Y] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush_world_max.y, g_EditorState.preview_brush.pos.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MIN_Z] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush_world_min.z };
                handle_centers_world[PREVIEW_BRUSH_HANDLE_MAX_Z] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush_world_max.z };

                for (int h_idx = 0; h_idx < PREVIEW_BRUSH_HANDLE_COUNT; ++h_idx) {
                    bool is_handle_relevant_to_view = false;
                    float dist_sq = FLT_MAX;

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
        for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
            if (g_EditorState.is_viewport_hovered[i]) {
                Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                Vec3 b_min = g_EditorState.preview_brush_world_min;
                Vec3 b_max = g_EditorState.preview_brush_world_max;

                bool hovered_this_view = false;
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
            for (int i = 0; i < b->numVertices; ++i) {
                local_min.x = fminf(local_min.x, b->vertices[i].pos.x);
                local_min.y = fminf(local_min.y, b->vertices[i].pos.y);
                local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
                local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x);
                local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y);
                local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
            }
            Vec3 world_min = mat4_mul_vec3(&b->modelMatrix, local_min);
            Vec3 world_max = mat4_mul_vec3(&b->modelMatrix, local_max);

            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                    bool hovered_this_view = false;
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
        bool use_gizmo = false;
        if (g_EditorState.is_in_brush_creation_mode) {
            gizmo_target_pos = g_EditorState.preview_brush.pos;
            use_gizmo = true;
        }
        else if (g_EditorState.num_selections > 0) {
            g_EditorState.gizmo_selection_centroid = (Vec3){ 0 };
            for (int i = 0; i < g_EditorState.num_selections; ++i) {
                Vec3 pos;
                switch (g_EditorState.selections[i].type) {
                case ENTITY_MODEL: pos = scene->objects[g_EditorState.selections[i].index].pos; break;
                case ENTITY_BRUSH: pos = scene->brushes[g_EditorState.selections[i].index].pos; break;
                case ENTITY_LIGHT: pos = scene->lights[g_EditorState.selections[i].index].position; break;
                case ENTITY_DECAL: pos = scene->decals[g_EditorState.selections[i].index].pos; break;
                case ENTITY_SOUND: pos = scene->soundEntities[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PARTICLE_EMITTER: pos = scene->particleEmitters[g_EditorState.selections[i].index].pos; break;
                case ENTITY_SPRITE: pos = scene->sprites[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PLAYERSTART: pos = scene->playerStart.position; break;
                case ENTITY_VIDEO_PLAYER: pos = scene->videoPlayers[g_EditorState.selections[i].index].pos; break;
                case ENTITY_PARALLAX_ROOM: pos = scene->parallaxRooms[g_EditorState.selections[i].index].pos; break;
                default: pos = (Vec3){ 0 }; break;
                }
                g_EditorState.gizmo_selection_centroid = vec3_add(g_EditorState.gizmo_selection_centroid, pos);
            }
            g_EditorState.gizmo_selection_centroid = vec3_muls(g_EditorState.gizmo_selection_centroid, 1.0f / g_EditorState.num_selections);
            gizmo_target_pos = g_EditorState.gizmo_selection_centroid;
            use_gizmo = true;
        }

        if (use_gizmo) {
            if (g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
                Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view;
                mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj);
                mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f };
                Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip);
                ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye);
                Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z };
                vec3_normalize(&ray_dir);
                Editor_UpdateGizmoHover(scene, g_EditorState.editor_camera.position, ray_dir);
            }
            if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_NONE) {
                for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                    if (g_EditorState.is_viewport_hovered[i]) {
                        if (primary && primary->type == ENTITY_BRUSH) {
                            continue;
                        }
                        Vec3 mouse_world = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[i], (ViewportType)i);
                        float threshold = g_EditorState.ortho_cam_zoom[i - 1] * 0.05f;
                        float GIZMO_SIZE = 1.0f;

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
    for (int i = 0; i < scene->numParticleEmitters; ++i) { ParticleEmitter_Update(&scene->particleEmitters[i], engine->deltaTime); }
}

static void Editor_RenderGizmo(Mat4 view, Mat4 projection, ViewportType type) {
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (!primary) return;

    if (primary->type == ENTITY_BRUSH && primary->face_index != -1 && g_EditorState.current_gizmo_operation == GIZMO_OP_ROTATE) {
        return;
    }
    if (g_EditorState.num_selections == 0) {
        return;
    }
    if (primary->type == ENTITY_BRUSH && type != VIEW_PERSPECTIVE) {
        return;
    }
    Vec3 object_pos = g_EditorState.gizmo_selection_centroid;

    glUseProgram(g_EditorState.gizmo_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "view"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "projection"), 1, GL_FALSE, projection.m);

    glDisable(GL_DEPTH_TEST);
    glLineWidth(4.0f);

    glBindVertexArray(g_EditorState.gizmo_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.gizmo_vbo);

    switch (g_EditorState.current_gizmo_operation) {
    case GIZMO_OP_TRANSLATE:
    case GIZMO_OP_SCALE: {
        const float gizmo_arrow_length = 1.0f;
        const float gizmo_vertices[] = {
            0.0f, 0.0f, 0.0f, gizmo_arrow_length, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, gizmo_arrow_length, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, gizmo_arrow_length,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(gizmo_vertices), gizmo_vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        Mat4 model = mat4_translate(object_pos);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "model"), 1, GL_FALSE, model.m);

        Vec3 color_x = { 1.0f, 0.2f, 0.2f }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_X || g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) color_x = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_x.x);
        glDrawArrays(GL_LINES, 0, 2);

        Vec3 color_y = { 0.2f, 1.0f, 0.2f }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_Y || g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) color_y = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_y.x);
        glDrawArrays(GL_LINES, 2, 2);

        Vec3 color_z = { 0.2f, 0.2f, 1.0f }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_Z || g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) color_z = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_z.x);
        glDrawArrays(GL_LINES, 4, 2);
        break;
    }
    case GIZMO_OP_ROTATE: {
        if (type != VIEW_PERSPECTIVE) break;
        Mat4 model; mat4_identity(&model);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "model"), 1, GL_FALSE, model.m);

#define SEGMENTS 32
        const float radius = 1.0f;
        Vec3 points[SEGMENTS + 1];

        Vec3 color_y = { 0,1,0 }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_Y || g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) color_y = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_y.x);
        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = (i / (float)SEGMENTS) * 2.0f * M_PI;
            points[i] = vec3_add(object_pos, (Vec3) { cosf(angle)* radius, 0.0f, sinf(angle)* radius });
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINE_STRIP, 0, SEGMENTS + 1);

        Vec3 color_x = { 1,0,0 }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_X || g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) color_x = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_x.x);
        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = (i / (float)SEGMENTS) * 2.0f * M_PI;
            points[i] = vec3_add(object_pos, (Vec3) { 0.0f, cosf(angle)* radius, sinf(angle)* radius });
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINE_STRIP, 0, SEGMENTS + 1);

        Vec3 color_z = { 0,0,1 }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_Z || g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) color_z = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_z.x);
        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = (i / (float)SEGMENTS) * 2.0f * M_PI;
            points[i] = vec3_add(object_pos, (Vec3) { cosf(angle)* radius, sinf(angle)* radius, 0.0f });
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINE_STRIP, 0, SEGMENTS + 1);
        break;
    }
    }

    glBindVertexArray(0);
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}
static void Editor_RenderSceneInternal(ViewportType type, Engine* engine, Renderer* renderer, Scene* scene, const Mat4* sunLightSpaceMatrix) {
    float aspect = (float)g_EditorState.viewport_width[type] / (float)g_EditorState.viewport_height[type];
    if (aspect <= 0) aspect = 1.0;

    switch (type) {
    case VIEW_PERSPECTIVE: {
        Vec3 f = { cosf(g_EditorState.editor_camera.pitch) * sinf(g_EditorState.editor_camera.yaw), sinf(g_EditorState.editor_camera.pitch), -cosf(g_EditorState.editor_camera.pitch) * cosf(g_EditorState.editor_camera.yaw) };
        vec3_normalize(&f);
        Vec3 t = vec3_add(g_EditorState.editor_camera.position, f);
        g_view_matrix[type] = mat4_lookAt(g_EditorState.editor_camera.position, t, (Vec3) { 0, 1, 0 });
        g_proj_matrix[type] = mat4_perspective(45.0f * (M_PI / 180.0f), aspect, 0.1f, 10000.0f);

        render_geometry_pass(&g_view_matrix[type], &g_proj_matrix[type], sunLightSpaceMatrix, g_EditorState.editor_camera.position, g_is_unlit_mode);
        if (Cvar_GetInt("r_ssao")) {
            render_ssao_pass(&g_proj_matrix[type]);
        }

        if (Cvar_GetInt("r_bloom")) {
            render_bloom_pass();
        }
        render_autoexposure_pass();

        glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[type]);
        glViewport(0, 0, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(renderer->postProcessShader);

        glUniform2f(glGetUniformLocation(renderer->postProcessShader, "resolution"), g_EditorState.viewport_width[type], g_EditorState.viewport_height[type]);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "time"), (float)SDL_GetTicks() / 1000.0f);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_exposure"), renderer->currentExposure);
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_fogEnabled"), scene->fog.enabled);
        glUniform3fv(glGetUniformLocation(renderer->postProcessShader, "u_fogColor"), 1, &scene->fog.color.x);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_fogStart"), scene->fog.start);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_fogEnd"), scene->fog.end);
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_postEnabled"), scene->post.enabled);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_crtCurvature"), scene->post.crtCurvature);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_vignetteStrength"), scene->post.vignetteStrength);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_vignetteRadius"), scene->post.vignetteRadius);
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_lensFlareEnabled"), scene->post.lensFlareEnabled);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_lensFlareStrength"), scene->post.lensFlareStrength);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_scanlineStrength"), scene->post.scanlineStrength);
        glUniform1f(glGetUniformLocation(renderer->postProcessShader, "u_grainIntensity"), scene->post.grainIntensity);
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_bloomEnabled"), Cvar_GetInt("r_bloom"));
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "u_volumetricsEnabled"), Cvar_GetInt("r_volumetrics"));
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, renderer->gLitColor);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, renderer->pingpongColorbuffers[0]);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, renderer->gPosition);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, renderer->volPingpongTextures[0]);
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "sceneTexture"), 0);
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "bloomBlur"), 1);
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "gPosition"), 2);
        glUniform1i(glGetUniformLocation(renderer->postProcessShader, "volumetricTexture"), 3);

        glBindVertexArray(renderer->quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->gBufferFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_EditorState.viewport_fbo[type]);
        const int LOW_RES_WIDTH = WINDOW_WIDTH / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
        const int LOW_RES_HEIGHT = WINDOW_HEIGHT / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
        glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT,
            0, 0, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type],
            GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[type]);
        glDepthFunc(GL_LEQUAL);
        glUseProgram(renderer->skyboxShader);
        Mat4 skyboxView = g_view_matrix[type];
        skyboxView.m[12] = skyboxView.m[13] = skyboxView.m[14] = 0;
        glUniformMatrix4fv(glGetUniformLocation(renderer->skyboxShader, "view"), 1, GL_FALSE, skyboxView.m);
        glUniformMatrix4fv(glGetUniformLocation(renderer->skyboxShader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
        glBindVertexArray(renderer->skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);

        for (int i = 0; i < scene->numParticleEmitters; ++i) {
            ParticleEmitter_Render(&scene->particleEmitters[i], g_view_matrix[type], g_proj_matrix[type]);
        }

        render_sprites(&g_view_matrix[type], &g_proj_matrix[type]);

        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        break;
    }
    case VIEW_TOP_XZ: { Vec3 p = g_EditorState.ortho_cam_pos[type - 1]; float z = g_EditorState.ortho_cam_zoom[type - 1]; g_view_matrix[type] = mat4_lookAt((Vec3) { p.x, 1000.0f, p.z }, (Vec3) { p.x, 0.0f, p.z }, (Vec3) { 0, 0, -1 }); g_proj_matrix[type] = mat4_ortho(-z * aspect, z * aspect, -z, z, 0.1f, 2000.0f); break; }
    case VIEW_FRONT_XY: { Vec3 p = g_EditorState.ortho_cam_pos[type - 1]; float z = g_EditorState.ortho_cam_zoom[type - 1]; g_view_matrix[type] = mat4_lookAt((Vec3) { p.x, p.y, 1000.0f }, (Vec3) { p.x, p.y, 0.0f }, (Vec3) { 0, 1, 0 }); g_proj_matrix[type] = mat4_ortho(-z * aspect, z * aspect, -z, z, 0.1f, 2000.0f); break; }
    case VIEW_SIDE_YZ: { Vec3 p = g_EditorState.ortho_cam_pos[type - 1]; float z = g_EditorState.ortho_cam_zoom[type - 1]; g_view_matrix[type] = mat4_lookAt((Vec3) { 1000.0f, p.y, p.z }, (Vec3) { 0.0f, p.y, p.z }, (Vec3) { 0, 1, 0 }); g_proj_matrix[type] = mat4_ortho(-z * aspect, z * aspect, -z, z, 0.1f, 2000.0f); break; }
    }

    if (type != VIEW_PERSPECTIVE) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[type]);
        glViewport(0, 0, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type]);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Editor_RenderGrid(type, aspect);
        if (g_EditorState.is_painting_mode_enabled && g_EditorState.is_viewport_hovered[type]) {
            Vec3 mouse_world_pos = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[type], type);
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
            Mat4 identity_mat; mat4_identity(&identity_mat);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);
            float color[] = { 1.0f, 1.0f, 0.0f, 0.8f };
            glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

            const int segments = 32;
            Vec3 circle_verts[64];
            for (int i = 0; i < segments; ++i) {
                float angle1 = (i / (float)segments) * 2.0f * M_PI;
                float angle2 = ((i + 1) / (float)segments) * 2.0f * M_PI;
                float x1 = g_EditorState.paint_brush_radius * cosf(angle1);
                float y1 = g_EditorState.paint_brush_radius * sinf(angle1);
                float x2 = g_EditorState.paint_brush_radius * cosf(angle2);
                float y2 = g_EditorState.paint_brush_radius * sinf(angle2);

                if (type == VIEW_TOP_XZ) {
                    circle_verts[i * 2] = (Vec3){ mouse_world_pos.x + x1, mouse_world_pos.y, mouse_world_pos.z + y1 };
                    circle_verts[i * 2 + 1] = (Vec3){ mouse_world_pos.x + x2, mouse_world_pos.y, mouse_world_pos.z + y2 };
                }
                else if (type == VIEW_FRONT_XY) {
                    circle_verts[i * 2] = (Vec3){ mouse_world_pos.x + x1, mouse_world_pos.y + y1, mouse_world_pos.z };
                    circle_verts[i * 2 + 1] = (Vec3){ mouse_world_pos.x + x2, mouse_world_pos.y + y2, mouse_world_pos.z };
                }
                else {
                    circle_verts[i * 2] = (Vec3){ mouse_world_pos.x, mouse_world_pos.y + y1, mouse_world_pos.z + x1 };
                    circle_verts[i * 2 + 1] = (Vec3){ mouse_world_pos.x, mouse_world_pos.y + y2, mouse_world_pos.z + x2 };
                }
            }

            glDisable(GL_DEPTH_TEST);
            glLineWidth(1.0f);
            glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(circle_verts), circle_verts, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_LINES, 0, segments * 2);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }
        if (g_EditorState.is_sculpting_mode_enabled && g_EditorState.is_viewport_hovered[type]) {
            Vec3 mouse_world_pos = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[type], type);
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
            Mat4 identity_mat; mat4_identity(&identity_mat);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);
            float color[] = { 0.0f, 1.0f, 1.0f, 0.8f };
            glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

            const int segments = 32;
            Vec3 circle_verts[64];
            for (int i = 0; i < segments; ++i) {
                float angle1 = (i / (float)segments) * 2.0f * M_PI;
                float angle2 = ((i + 1) / (float)segments) * 2.0f * M_PI;
                float x1 = g_EditorState.sculpt_brush_radius * cosf(angle1);
                float y1 = g_EditorState.sculpt_brush_radius * sinf(angle1);
                float x2 = g_EditorState.sculpt_brush_radius * cosf(angle2);
                float y2 = g_EditorState.sculpt_brush_radius * sinf(angle2);

                if (type == VIEW_TOP_XZ) {
                    circle_verts[i * 2] = (Vec3){ mouse_world_pos.x + x1, mouse_world_pos.y, mouse_world_pos.z + y1 };
                    circle_verts[i * 2 + 1] = (Vec3){ mouse_world_pos.x + x2, mouse_world_pos.y, mouse_world_pos.z + y2 };
                }
                else if (type == VIEW_FRONT_XY) {
                    circle_verts[i * 2] = (Vec3){ mouse_world_pos.x + x1, mouse_world_pos.y + y1, mouse_world_pos.z };
                    circle_verts[i * 2 + 1] = (Vec3){ mouse_world_pos.x + x2, mouse_world_pos.y + y2, mouse_world_pos.z };
                }
                else {
                    circle_verts[i * 2] = (Vec3){ mouse_world_pos.x, mouse_world_pos.y + y1, mouse_world_pos.z + x1 };
                    circle_verts[i * 2 + 1] = (Vec3){ mouse_world_pos.x, mouse_world_pos.y + y2, mouse_world_pos.z + x2 };
                }
            }

            glDisable(GL_DEPTH_TEST);
            glLineWidth(1.0f);
            glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(circle_verts), circle_verts, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
            glEnableVertexAttribArray(0);
            glDrawArrays(GL_LINES, 0, segments * 2);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glEnable(GL_LINE_SMOOTH); glEnable(GL_POLYGON_OFFSET_LINE); glPolygonOffset(1.0, 1.0); glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); float color[] = { 0.8f, 0.8f, 0.8f, 1.0f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        for (int i = 0; i < scene->numObjects; i++) { render_object(g_EditorState.debug_shader, &scene->objects[i], false, NULL); }
        for (int i = 0; i < scene->numBrushes; i++) { if (!scene->brushes[i].isTrigger) render_brush(g_EditorState.debug_shader, &scene->brushes[i], false, NULL); }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glDisable(GL_LINE_SMOOTH); glDisable(GL_POLYGON_OFFSET_LINE);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[type]);
    glViewport(0, 0, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type]);

    glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
    for (int i = 0; i < scene->numDecals; i++) {
        Decal* d = &scene->decals[i]; glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, d->modelMatrix.m); bool is_selected = Editor_IsSelected(ENTITY_DECAL, i); float color[] = { 0.2f, 1.0f, 0.2f, 1.0f }; if (!is_selected) { color[3] = 0.5f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.decal_box_vao); glLineWidth(is_selected ? 2.0f : 1.0f); glDrawArrays(GL_LINES, 0, g_EditorState.decal_box_vertex_count); glLineWidth(1.0f);
    }
    for (int i = 0; i < scene->numVideoPlayers; i++) {
        VideoPlayer* vp = &scene->videoPlayers[i];
        vp->modelMatrix = create_trs_matrix(vp->pos, vp->rot, (Vec3) { vp->size.x, vp->size.y, 1.0f });
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, vp->modelMatrix.m);
        bool is_selected = Editor_IsSelected(ENTITY_VIDEO_PLAYER, i);
        float color[] = { 1.0f, 0.0f, 1.0f, 1.0f };
        if (!is_selected) { color[3] = 0.5f; }
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        glBindVertexArray(g_EditorState.decal_box_vao);
        glLineWidth(is_selected ? 2.0f : 1.0f);
        glDrawArrays(GL_LINES, 0, g_EditorState.decal_box_vertex_count);
        glLineWidth(1.0f);
    }
    for (int i = 0; i < scene->numParallaxRooms; i++) {
        ParallaxRoom* p = &scene->parallaxRooms[i];
        p->modelMatrix = create_trs_matrix(p->pos, p->rot, (Vec3) { p->size.x, p->size.y, p->roomDepth });
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, p->modelMatrix.m);
        bool is_selected = Editor_IsSelected(ENTITY_PARALLAX_ROOM, i);
        float color[] = { 0.5f, 0.0f, 1.0f, 1.0f };
        if (!is_selected) { color[3] = 0.5f; }
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        glBindVertexArray(g_EditorState.decal_box_vao);
        glLineWidth(is_selected ? 2.0f : 1.0f);
        glDrawArrays(GL_LINES, 0, g_EditorState.decal_box_vertex_count);
        glLineWidth(1.0f);
    }
    for (int i = 0; i < scene->numSprites; ++i) {
        Sprite* s = &scene->sprites[i];
        bool is_selected = Editor_IsSelected(ENTITY_SPRITE, i);
        if (!s->visible && !is_selected) continue;

        glUseProgram(g_EditorState.debug_shader);
        Mat4 modelMatrix = mat4_translate(s->pos);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m);
        float color[] = { 0.8f, 0.2f, 1.0f, 1.0f };
        if (is_selected) { color[0] = 1.0f; color[1] = 0.5f; color[2] = 0.0f; }
        else if (!s->visible) { color[3] = 0.3f; }
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        glBindVertexArray(g_EditorState.light_gizmo_vao);
        glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);
    }
    glDisable(GL_DEPTH_TEST); glLineWidth(2.0f);
    if (g_EditorState.is_in_brush_creation_mode || g_EditorState.is_dragging_for_creation) {
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, g_EditorState.preview_brush.modelMatrix.m); float color[] = { 1.0f, 1.0f, 0.0f, 0.5f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.preview_brush.vao); glDrawArrays(GL_TRIANGLES, 0, g_EditorState.preview_brush.totalRenderVertexCount); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); color[3] = 1.0f; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glDrawArrays(GL_TRIANGLES, 0, g_EditorState.preview_brush.totalRenderVertexCount); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glDisable(GL_BLEND);
        if (type != VIEW_PERSPECTIVE && g_EditorState.preview_brush.numVertices > 0) {
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
            Mat4 identity_mat; mat4_identity(&identity_mat);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);

            float handle_screen_size = 8.0f;
            float handle_world_size = handle_screen_size * (g_EditorState.ortho_cam_zoom[type - 1] / (float)g_EditorState.viewport_height[type]);

            Vec3 handle_positions_world[PREVIEW_BRUSH_HANDLE_COUNT];
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MIN_X] = (Vec3){ g_EditorState.preview_brush_world_min.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush.pos.z };
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MAX_X] = (Vec3){ g_EditorState.preview_brush_world_max.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush.pos.z };
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MIN_Y] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush_world_min.y, g_EditorState.preview_brush.pos.z };
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MAX_Y] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush_world_max.y, g_EditorState.preview_brush.pos.z };
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MIN_Z] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush_world_min.z };
            handle_positions_world[PREVIEW_BRUSH_HANDLE_MAX_Z] = (Vec3){ g_EditorState.preview_brush.pos.x, g_EditorState.preview_brush.pos.y, g_EditorState.preview_brush_world_max.z };

            glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glEnableVertexAttribArray(0);
            glPointSize(handle_screen_size);

            for (int i = 0; i < PREVIEW_BRUSH_HANDLE_COUNT; ++i) {
                bool show_handle = false;
                if (type == VIEW_TOP_XZ) {
                    if (i == PREVIEW_BRUSH_HANDLE_MIN_X || i == PREVIEW_BRUSH_HANDLE_MAX_X || i == PREVIEW_BRUSH_HANDLE_MIN_Z || i == PREVIEW_BRUSH_HANDLE_MAX_Z) show_handle = true;
                }
                else if (type == VIEW_FRONT_XY) {
                    if (i == PREVIEW_BRUSH_HANDLE_MIN_X || i == PREVIEW_BRUSH_HANDLE_MAX_X || i == PREVIEW_BRUSH_HANDLE_MIN_Y || i == PREVIEW_BRUSH_HANDLE_MAX_Y) show_handle = true;
                }
                else if (type == VIEW_SIDE_YZ) {
                    if (i == PREVIEW_BRUSH_HANDLE_MIN_Y || i == PREVIEW_BRUSH_HANDLE_MAX_Y || i == PREVIEW_BRUSH_HANDLE_MIN_Z || i == PREVIEW_BRUSH_HANDLE_MAX_Z) show_handle = true;
                }

                if (show_handle) {
                    float color_arr[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
                    if ((PreviewBrushHandleType)i == g_EditorState.preview_brush_hovered_handle || (PreviewBrushHandleType)i == g_EditorState.preview_brush_active_handle) {
                        color_arr[0] = 1.0f; color_arr[1] = 1.0f; color_arr[2] = 0.0f;
                    }
                    glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color_arr);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(Vec3), &handle_positions_world[i], GL_DYNAMIC_DRAW);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
                    glDrawArrays(GL_POINTS, 0, 1);
                }
            }
            glPointSize(1.0f);
            glBindVertexArray(0);
        }
    }
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (primary && primary->type == ENTITY_BRUSH && type != VIEW_PERSPECTIVE) {
        Brush* b = &scene->brushes[primary->index];
        if (b->numVertices > 0) {
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, b->modelMatrix.m);

            glPointSize(8.0f);
            glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glEnableVertexAttribArray(0);

            Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (int i = 0; i < b->numVertices; ++i) {
                local_min.x = fminf(local_min.x, b->vertices[i].pos.x); local_min.y = fminf(local_min.y, b->vertices[i].pos.y); local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
                local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x); local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y); local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
            }
            Vec3 local_center = vec3_muls(vec3_add(local_min, local_max), 0.5f);

            Vec3 handle_positions_local[6] = {
                {local_min.x, local_center.y, local_center.z}, {local_max.x, local_center.y, local_center.z},
                {local_center.x, local_min.y, local_center.z}, {local_center.x, local_max.y, local_center.z},
                {local_center.x, local_center.y, local_min.z}, {local_center.x, local_center.y, local_max.z}
            };

            for (int i = 0; i < 6; ++i) {
                bool is_hovered = ((PreviewBrushHandleType)i == g_EditorState.selected_brush_hovered_handle);
                bool is_active = ((PreviewBrushHandleType)i == g_EditorState.selected_brush_active_handle);
                float color[] = { is_hovered || is_active ? 1.0f : 0.0f, 1.0f, is_hovered || is_active ? 0.0f : 0.0f, 1.0f };
                glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
                glBufferData(GL_ARRAY_BUFFER, sizeof(Vec3), &handle_positions_local[i], GL_DYNAMIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
                glDrawArrays(GL_POINTS, 0, 1);
            }
            glPointSize(1.0f);
            glBindVertexArray(0);
        }
    }
    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];
        if (sel->type == ENTITY_MODEL) { SceneObject* obj = &scene->objects[sel->index]; glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); float color[] = { 1.0f, 0.5f, 0.0f, 1.0f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); render_object(g_EditorState.debug_shader, obj, false, NULL); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); }
    }
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        if (b->isReflectionProbe || b->isTrigger || b->isWater) {
            bool is_selected = Editor_IsSelected(ENTITY_BRUSH, i);
            if (!is_selected && !b->isWater) continue;
            glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, b->modelMatrix.m); float color[] = { 1.0f, 0.5f, 0.0f, 1.0f }; if (b->isTrigger) { color[0] = 1.0f; color[1] = 0.8f; color[2] = 0.2f; } if (b->isReflectionProbe) { color[0] = 0.2f; color[1] = 0.8f; color[2] = 1.0f; } if (b->isWater) { color[0] = 0.2f; color[1] = 0.2f; color[2] = 1.0f; if (!is_selected) color[3] = 0.3f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glBindVertexArray(b->vao); glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount); glBindVertexArray(0); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];

        if (sel->type == ENTITY_BRUSH) {
            Brush* b = &scene->brushes[sel->index];
            if (!b->isReflectionProbe && !b->isTrigger && sel->face_index >= 0 && sel->face_index < b->numFaces) {
                BrushFace* face = &b->faces[sel->face_index];
                if (face->numVertexIndices >= 3) {
                    int num_tris = face->numVertexIndices - 2;
                    int num_verts = num_tris * 3;
                    float* face_verts = malloc(num_verts * 3 * sizeof(float));
                    for (int tri = 0; tri < num_tris; ++tri) {
                        int tri_indices[3] = { face->vertexIndices[0], face->vertexIndices[tri + 1], face->vertexIndices[tri + 2] };
                        for (int j = 0; j < 3; ++j) {
                            Vec3 v = b->vertices[tri_indices[j]].pos;
                            face_verts[(tri * 3 + j) * 3 + 0] = v.x;
                            face_verts[(tri * 3 + j) * 3 + 1] = v.y;
                            face_verts[(tri * 3 + j) * 3 + 2] = v.z;
                        }
                    }
                    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE); glUseProgram(g_EditorState.debug_shader);
                    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
                    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
                    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, b->modelMatrix.m);
                    float color[] = { 1.0f, 0.5f, 0.0f, 0.4f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
                    glBindVertexArray(g_EditorState.selected_face_vao); glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.selected_face_vbo);
                    glBufferData(GL_ARRAY_BUFFER, num_verts * 3 * sizeof(float), face_verts, GL_DYNAMIC_DRAW);
                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                    glEnableVertexAttribArray(0); glDrawArrays(GL_TRIANGLES, 0, num_verts);
                    glBindVertexArray(0); glDisable(GL_BLEND); glDepthMask(GL_TRUE);
                    free(face_verts);
                }
            }
        }
    }
    glUseProgram(g_EditorState.debug_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
    for (int i = 0; i < scene->numActiveLights; ++i) {
        Light* light = &scene->lights[i];
        bool is_selected = Editor_IsSelected(ENTITY_LIGHT, i);

        Mat4 modelMatrix = mat4_translate(light->position);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m);

        float color[] = { light->color.x, light->color.y, light->color.z, 1.0f };
        if (is_selected) {
            color[0] = 1.0f; color[1] = 1.0f; color[2] = 0.0f;
        }
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

        glBindVertexArray(g_EditorState.light_gizmo_vao);
        glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);

        if (is_selected) {
            if (light->type == LIGHT_POINT) {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                Mat4 scaleMatrix = mat4_scale((Vec3) { light->radius, light->radius, light->radius });
                Mat4 scaledModelMatrix;
                mat4_multiply(&scaledModelMatrix, &modelMatrix, &scaleMatrix);
                glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, scaledModelMatrix.m);
                float radius_color[] = { 1.0f, 1.0f, 0.0f, 0.5f };
                glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, radius_color);
                glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            if (light->type == LIGHT_SPOT) {
                float far_plane = light->shadowFarPlane > 0 ? light->shadowFarPlane : 25.0f;
                float angle = acosf(fmaxf(-1.0f, fminf(1.0f, light->cutOff)));
                float radius = tanf(angle) * far_plane;
                Vec3 dir = light->direction; vec3_normalize(&dir);
                Vec3 up_ish = (fabsf(vec3_dot(dir, (Vec3) { 0, 1, 0 })) > 0.99f) ? (Vec3) { 1, 0, 0 } : (Vec3) { 0, 1, 0 };
                Vec3 right = vec3_cross(dir, up_ish); vec3_normalize(&right);
                Vec3 up = vec3_cross(right, dir);
                int segments = 16;
                Vec3 cone_verts[40]; int vert_count = 0;
                for (int k = 0; k < 4; ++k) {
                    float theta = (k / 4.0f) * 2.0f * M_PI;
                    Vec3 p_on_circle = vec3_add(vec3_muls(right, cosf(theta) * radius), vec3_muls(up, sinf(theta) * radius));
                    Vec3 world_p = vec3_add(light->position, vec3_add(vec3_muls(dir, far_plane), p_on_circle));
                    cone_verts[vert_count++] = light->position;
                    cone_verts[vert_count++] = world_p;
                }
                for (int k = 0; k < segments; ++k) {
                    float theta1 = (k / (float)segments) * 2.0f * M_PI;
                    float theta2 = ((k + 1) / (float)segments) * 2.0f * M_PI;
                    Vec3 p1_on_circle = vec3_add(vec3_muls(right, cosf(theta1) * radius), vec3_muls(up, sinf(theta1) * radius));
                    Vec3 p2_on_circle = vec3_add(vec3_muls(right, cosf(theta2) * radius), vec3_muls(up, sinf(theta2) * radius));
                    cone_verts[vert_count++] = vec3_add(light->position, vec3_add(vec3_muls(dir, far_plane), p1_on_circle));
                    cone_verts[vert_count++] = vec3_add(light->position, vec3_add(vec3_muls(dir, far_plane), p2_on_circle));
                }
                Mat4 identity_mat; mat4_identity(&identity_mat);
                glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);
                glBindVertexArray(g_EditorState.vertex_points_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
                glBufferData(GL_ARRAY_BUFFER, vert_count * sizeof(Vec3), cone_verts, GL_DYNAMIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
                glEnableVertexAttribArray(0);
                glDrawArrays(GL_LINES, 0, vert_count);
            }
        }
    }
    glUseProgram(g_EditorState.debug_shader);
    for (int i = 0; i < scene->numSoundEntities; ++i) { bool is_selected = Editor_IsSelected(ENTITY_SOUND, i); Mat4 modelMatrix = mat4_translate(scene->soundEntities[i].pos); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m); float color[] = { 0.1f, 0.9f, 0.6f, 1.0f }; if (is_selected) { color[0] = 1.0f; color[1] = 0.5f; color[2] = 0.0f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.light_gizmo_vao); glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count); }
    for (int i = 0; i < scene->numParticleEmitters; ++i) { bool is_selected = Editor_IsSelected(ENTITY_PARTICLE_EMITTER, i); Mat4 modelMatrix = mat4_translate(scene->particleEmitters[i].pos); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m); float color[] = { 1.0f, 0.2f, 0.8f, 1.0f }; if (is_selected) { color[0] = 1.0f; color[1] = 0.5f; color[2] = 0.0f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.light_gizmo_vao); glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count); }
    glUseProgram(g_EditorState.debug_shader);
    for (int i = 0; i < scene->numLogicEntities; ++i) {
        bool is_selected = Editor_IsSelected(ENTITY_LOGIC, i);
        Mat4 modelMatrix = mat4_translate(scene->logicEntities[i].pos);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m);
        float color[] = { 1.0f, 0.5f, 0.0f, 1.0f };
        if (!is_selected) {
            color[3] = 0.5f;
        }
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        glBindVertexArray(g_EditorState.light_gizmo_vao);
        glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);
    }
    if (primary && primary->type == ENTITY_BRUSH && primary->vertex_index >= 0) {
        Brush* b = &scene->brushes[primary->index];
        if (primary->vertex_index < b->numVertices) {
            Vec3 vertex_local_pos = b->vertices[primary->vertex_index].pos;
            Vec3 vertex_world_pos = mat4_mul_vec3(&b->modelMatrix, vertex_local_pos);
            glUseProgram(g_EditorState.debug_shader);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
            Mat4 identity_mat; mat4_identity(&identity_mat);
            glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity_mat.m);
            float color[] = { 1.0f, 0.0f, 1.0f, 1.0f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
            glPointSize(10.0f); glBindVertexArray(g_EditorState.vertex_points_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(Vec3), &vertex_world_pos, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
            glEnableVertexAttribArray(0); glDrawArrays(GL_POINTS, 0, 1);
            glBindVertexArray(0); glPointSize(1.0f);
        }
    }
    glLineWidth(1.0f); glEnable(GL_DEPTH_TEST);
    if (g_EditorState.sprinkle_brush_hit_surface && g_EditorState.show_sprinkle_tool_window) {
        glUseProgram(g_EditorState.debug_shader);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);

        Mat4 model_mat = mat4_translate(g_EditorState.sprinkle_brush_world_pos);
        Mat4 scale_mat = mat4_scale((Vec3) { g_EditorState.sprinkle_radius, g_EditorState.sprinkle_radius, g_EditorState.sprinkle_radius });
        mat4_multiply(&model_mat, &model_mat, &scale_mat);

        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, model_mat.m);

        float color[] = { 1.0f, 0.0f, 1.0f, 0.5f };
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(g_EditorState.light_gizmo_vao);
        glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (g_EditorState.paint_brush_hit_surface && (g_EditorState.is_painting_mode_enabled || g_EditorState.is_sculpting_mode_enabled)) {
        glUseProgram(g_EditorState.debug_shader);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);

        float radius = g_EditorState.is_painting_mode_enabled ? g_EditorState.paint_brush_radius : g_EditorState.sculpt_brush_radius;
        Mat4 model_mat = mat4_translate(g_EditorState.paint_brush_world_pos);
        Mat4 scale_mat = mat4_scale((Vec3) { radius, radius, radius });
        mat4_multiply(&model_mat, &model_mat, &scale_mat);

        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, model_mat.m);

        float color[] = { 1.0f, 1.0f, 0.0f, 0.5f };
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(g_EditorState.light_gizmo_vao);
        glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (g_EditorState.is_clipping && g_EditorState.clip_point_count > 0 && primary && primary->type == ENTITY_BRUSH) {
        glUseProgram(g_EditorState.debug_shader);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
        Mat4 identity; mat4_identity(&identity);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, identity.m);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(2.0f);

        Vec3 line_verts[2];
        line_verts[0] = g_EditorState.clip_points[0];

        if (g_EditorState.clip_point_count == 1) {
            if (type == g_EditorState.clip_view) {
                line_verts[1] = ScreenToWorld_Clip(g_EditorState.mouse_pos_in_viewport[type], type);
            }
            else {
                line_verts[1] = line_verts[0];
            }
        }
        else {
            line_verts[1] = g_EditorState.clip_points[1];
        }

        float color_yellow[] = { 1.0f, 1.0f, 0.0f, 1.0f };
        glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color_yellow);

        glBindVertexArray(g_EditorState.vertex_points_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(line_verts), line_verts, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, 2);

        if (g_EditorState.clip_point_count >= 2) {
            Vec3 p1 = g_EditorState.clip_points[0];
            Vec3 p2 = g_EditorState.clip_points[1];
            Vec3 mid = vec3_muls(vec3_add(p1, p2), 0.5f);
            Vec3 plane_normal;
            Vec3 dir = vec3_sub(p2, p1);

            if (g_EditorState.clip_view == VIEW_TOP_XZ) { plane_normal = vec3_cross(dir, (Vec3) { 0, 1, 0 }); }
            else if (g_EditorState.clip_view == VIEW_FRONT_XY) { plane_normal = vec3_cross(dir, (Vec3) { 0, 0, 1 }); }
            else { plane_normal = vec3_cross(dir, (Vec3) { 1, 0, 0 }); }
            vec3_normalize(&plane_normal);

            if (g_EditorState.clip_side_point.x != 0 || g_EditorState.clip_side_point.y != 0 || g_EditorState.clip_side_point.z != 0) {
                float side_check = vec3_dot(plane_normal, vec3_sub(g_EditorState.clip_side_point, p1));
                if (side_check < 0) plane_normal = vec3_muls(plane_normal, -1.0f);
            }

            Vec3 indicator_verts[] = { mid, vec3_add(mid, plane_normal) };
            glBufferData(GL_ARRAY_BUFFER, sizeof(indicator_verts), indicator_verts, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINES, 0, 2);
        }

        glLineWidth(1.0f);
        glEnable(GL_DEPTH_TEST);
        glBindVertexArray(0);
    }
    glUseProgram(g_EditorState.debug_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
    Mat4 player_model_matrix = mat4_translate(scene->playerStart.position);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, player_model_matrix.m);

    bool is_selected = Editor_IsSelected(ENTITY_PLAYERSTART, 0);
    float player_color[] = { 0.2f, 0.2f, 1.0f, 1.0f };
    if (is_selected) {
        player_color[0] = 1.0f; player_color[1] = 0.5f; player_color[2] = 0.0f;
    }
    glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, player_color);

    glBindVertexArray(g_EditorState.player_start_gizmo_vao);
    glLineWidth(is_selected ? 2.0f : 1.0f);
    glDrawArrays(GL_LINES, 0, g_EditorState.player_start_gizmo_vertex_count);
    glLineWidth(1.0f);
    glBindVertexArray(0);

    Editor_RenderGizmo(g_view_matrix[type], g_proj_matrix[type], type);
    if (type == VIEW_PERSPECTIVE && primary && primary->type == ENTITY_BRUSH &&
        primary->vertex_index != -1 &&
        !g_EditorState.is_manipulating_gizmo) {

        Brush* b = &scene->brushes[primary->index];
        Vec3 vertex_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[primary->vertex_index].pos);

        glUseProgram(g_EditorState.gizmo_shader);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(2.0f);
        glBindVertexArray(g_EditorState.gizmo_vao);

        Mat4 scale = mat4_scale((Vec3) { 0.5f, 0.5f, 0.5f });
        Mat4 trans = mat4_translate(vertex_world_pos);
        Mat4 model;
        mat4_multiply(&model, &trans, &scale);
        glUniformMatrix4fv(glGetUniformLocation(g_EditorState.gizmo_shader, "model"), 1, GL_FALSE, model.m);

        Vec3 color_x = { 1,0,0 }; if (g_EditorState.vertex_gizmo_hovered_axis == GIZMO_AXIS_X || g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_X) color_x = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_x.x);
        glDrawArrays(GL_LINES, 0, 2);

        Vec3 color_y = { 0,1,0 }; if (g_EditorState.vertex_gizmo_hovered_axis == GIZMO_AXIS_Y || g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Y) color_y = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_y.x);
        glDrawArrays(GL_LINES, 2, 2);

        Vec3 color_z = { 0,0,1 }; if (g_EditorState.vertex_gizmo_hovered_axis == GIZMO_AXIS_Z || g_EditorState.vertex_gizmo_active_axis == GIZMO_AXIS_Z) color_z = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_z.x);
        glDrawArrays(GL_LINES, 4, 2);

        glBindVertexArray(0);
        glLineWidth(1.0f);
        glEnable(GL_DEPTH_TEST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
static void Editor_RenderModelPreviewerScene(Renderer* renderer) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.model_preview_fbo);
    glViewport(0, 0, g_EditorState.model_preview_width, g_EditorState.model_preview_height);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    if (g_EditorState.preview_model) {
        float aspect = (float)g_EditorState.model_preview_width / (float)g_EditorState.model_preview_height;
        if (aspect <= 0) aspect = 1.0f;
        Vec3 cam_pos;
        cam_pos.x = g_EditorState.model_preview_cam_dist * sinf(g_EditorState.model_preview_cam_angles.y) * cosf(g_EditorState.model_preview_cam_angles.x);
        cam_pos.y = g_EditorState.model_preview_cam_dist * cosf(g_EditorState.model_preview_cam_angles.y);
        cam_pos.z = g_EditorState.model_preview_cam_dist * sinf(g_EditorState.model_preview_cam_angles.y) * sinf(g_EditorState.model_preview_cam_angles.x);
        Mat4 view = mat4_lookAt(cam_pos, (Vec3) { 0, 0, 0 }, (Vec3) { 0, 1, 0 });
        Mat4 proj = mat4_perspective(45.0f * (M_PI / 180.0f), aspect, 0.1f, 1000.0f);
        glUseProgram(renderer->mainShader);
        glUniform1i(glGetUniformLocation(renderer->mainShader, "is_unlit"), 1);
        glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "view"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "projection"), 1, GL_FALSE, proj.m);
        glUniform1i(glGetUniformLocation(renderer->mainShader, "useEnvironmentMap"), 0);
        SceneObject temp_obj;
        memset(&temp_obj, 0, sizeof(SceneObject));
        temp_obj.model = g_EditorState.preview_model;
        mat4_identity(&temp_obj.modelMatrix);
        render_object(renderer->mainShader, &temp_obj, false, NULL);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void Editor_RenderAllViewports(Engine* engine, Renderer* renderer, Scene* scene) {
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->volumetricFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->volPingpongFBO[0]);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    render_shadows();

    Mat4 sunLightSpaceMatrix;
    mat4_identity(&sunLightSpaceMatrix);
    if (scene->sun.enabled) {
        Calculate_Sun_Light_Space_Matrix(&sunLightSpaceMatrix, &scene->sun, g_EditorState.editor_camera.position);
        render_sun_shadows(&sunLightSpaceMatrix);
    }

    for (int i = 0; i < VIEW_COUNT; i++) {
        Editor_RenderSceneInternal((ViewportType)i, engine, renderer, scene, &sunLightSpaceMatrix);
    }
    if (g_EditorState.show_add_model_popup) {
        Editor_RenderModelPreviewerScene(renderer);
    }
}
static void RenderIOEditor(EntityType type, int index, const char** valid_outputs, int num_valid_outputs) {
    if (num_valid_outputs == 0) return;
    UI_Separator();
    UI_Text("Outputs");

    int total_target_names = 0;
    char** all_target_names = NULL;

    for (int i = 0; i < g_CurrentScene->numObjects; i++) if (strlen(g_CurrentScene->objects[i].targetname) > 0) { all_target_names = realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->objects[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numBrushes; i++) if (strlen(g_CurrentScene->brushes[i].targetname) > 0) { all_target_names = realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->brushes[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numActiveLights; i++) if (strlen(g_CurrentScene->lights[i].targetname) > 0) { all_target_names = realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->lights[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numSoundEntities; i++) if (strlen(g_CurrentScene->soundEntities[i].targetname) > 0) { all_target_names = realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->soundEntities[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numParticleEmitters; i++) if (strlen(g_CurrentScene->particleEmitters[i].targetname) > 0) { all_target_names = realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->particleEmitters[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numVideoPlayers; i++) if (strlen(g_CurrentScene->videoPlayers[i].targetname) > 0) { all_target_names = realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->videoPlayers[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numSprites; i++) if (strlen(g_CurrentScene->sprites[i].targetname) > 0) { all_target_names = realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->sprites[i].targetname; }
    for (int i = 0; i < g_CurrentScene->numLogicEntities; i++) if (strlen(g_CurrentScene->logicEntities[i].targetname) > 0) { all_target_names = realloc(all_target_names, ++total_target_names * sizeof(char*)); all_target_names[total_target_names - 1] = g_CurrentScene->logicEntities[i].targetname; }

    for (int i = 0; i < num_valid_outputs; ++i) {
        if (UI_CollapsingHeader(valid_outputs[i], 1)) {
            int conn_to_delete = -1;
            for (int k = 0; k < g_num_io_connections; k++) {
                IOConnection* conn = &g_io_connections[k];
                if (conn->sourceType == type && conn->sourceIndex == index && strcmp(conn->outputName, valid_outputs[i]) == 0) {
                    UI_PushID(k);
                    char header_label[128];
                    sprintf(header_label, "To '%s' -> '%s'", conn->targetName, conn->inputName);
                    if (UI_CollapsingHeader(header_label, 1)) {
                        int current_target_idx = -1;
                        for (int j = 0; j < total_target_names; j++) {
                            if (strcmp(all_target_names[j], conn->targetName) == 0) {
                                current_target_idx = j;
                                break;
                            }
                        }
                        if (UI_Combo("Target", &current_target_idx, (const char* const*)all_target_names, total_target_names, -1)) {
                            if (current_target_idx >= 0) {
                                strncpy(conn->targetName, all_target_names[current_target_idx], sizeof(conn->targetName) - 1);
                                conn->inputName[0] = '\0';
                            }
                        }

                        EntityType target_type;
                        int target_index;
                        if (FindEntityInScene(g_CurrentScene, conn->targetName, &target_type, &target_index)) {
                            const char** valid_inputs = NULL;
                            int num_valid_inputs = 0;

                            switch (target_type) {
                            case ENTITY_MODEL: valid_inputs = g_model_inputs; num_valid_inputs = g_num_model_inputs; break;
                            case ENTITY_BRUSH: if (g_CurrentScene->brushes[target_index].isTrigger) { valid_inputs = g_brush_trigger_inputs; num_valid_inputs = g_num_brush_trigger_inputs; } break;
                            case ENTITY_LIGHT: valid_inputs = g_light_inputs; num_valid_inputs = g_num_light_inputs; break;
                            case ENTITY_SOUND: valid_inputs = g_sound_inputs; num_valid_inputs = g_num_sound_inputs; break;
                            case ENTITY_PARTICLE_EMITTER: valid_inputs = g_particle_inputs; num_valid_inputs = g_num_particle_inputs; break;
                            case ENTITY_VIDEO_PLAYER: valid_inputs = g_video_inputs; num_valid_inputs = g_num_video_inputs; break;
                            case ENTITY_SPRITE: valid_inputs = g_sprite_inputs; num_valid_inputs = g_num_sprite_inputs; break;
                            case ENTITY_LOGIC: {
                                LogicEntity* ent = &g_CurrentScene->logicEntities[target_index];
                                if (strcmp(ent->classname, "logic_timer") == 0) { valid_inputs = g_logic_timer_inputs; num_valid_inputs = g_num_logic_timer_inputs; }
                                else if (strcmp(ent->classname, "math_counter") == 0) { valid_inputs = g_math_counter_inputs; num_valid_inputs = g_num_math_counter_inputs; }
                                else if (strcmp(ent->classname, "logic_random") == 0) { valid_inputs = g_logic_random_inputs; num_valid_inputs = g_num_logic_random_inputs; }
                                else if (strcmp(ent->classname, "logic_relay") == 0) { valid_inputs = g_logic_relay_inputs; num_valid_inputs = g_num_logic_relay_inputs; }
                                else if (strcmp(ent->classname, "point_servercommand") == 0) { valid_inputs = g_point_servercommand_inputs; num_valid_inputs = g_num_point_servercommand_inputs; }
                                else if (strcmp(ent->classname, "logic_compare") == 0) { valid_inputs = g_logic_compare_inputs; num_valid_inputs = g_num_logic_compare_inputs; }
                                else if (strcmp(ent->classname, "env_blackhole") == 0) { valid_inputs = g_env_blackhole_inputs; num_valid_inputs = g_num_env_blackhole_inputs; }
                                break;
                            }
                            default: break;
                            }

                            if (valid_inputs && num_valid_inputs > 0) {
                                int current_input_idx = -1;
                                for (int j = 0; j < num_valid_inputs; j++) {
                                    if (strcmp(valid_inputs[j], conn->inputName) == 0) {
                                        current_input_idx = j;
                                        break;
                                    }
                                }
                                if (UI_Combo("Input", &current_input_idx, valid_inputs, num_valid_inputs, -1)) {
                                    if (current_input_idx >= 0) {
                                        strncpy(conn->inputName, valid_inputs[current_input_idx], sizeof(conn->inputName) - 1);
                                    }
                                }
                            }
                            else {
                                UI_InputText("Input", conn->inputName, sizeof(conn->inputName));
                            }
                        }
                        else {
                            UI_InputText("Input (Unknown Target)", conn->inputName, sizeof(conn->inputName));
                        }

                        UI_InputText("Parameter", conn->parameter, 64);
                        UI_DragFloat("Delay", &conn->delay, 0.1f, 0.0f, 300.0f);
                        UI_Selectable("Fire Once", &conn->fireOnce);
                        if (UI_Button("Delete Connection")) {
                            conn_to_delete = k;
                        }
                    }
                    UI_PopID();
                }
            }
            if (conn_to_delete != -1) { IO_RemoveConnection(conn_to_delete); }
            char add_label[64];
            sprintf(add_label, "Add Connection##%s", valid_outputs[i]);
            if (UI_Button(add_label)) { IO_AddConnection(type, index, valid_outputs[i]); }
        }
    }

    if (all_target_names) {
        free(all_target_names);
    }
}
static void Editor_RenderModelBrowser(Scene* scene, Engine* engine, Renderer* renderer) {
    if (!g_EditorState.show_add_model_popup) return;

    UI_SetNextWindowSize(700, 500);
    if (UI_Begin("Model Browser", &g_EditorState.show_add_model_popup)) {
        UI_InputText("Search", g_EditorState.model_search_filter, sizeof(g_EditorState.model_search_filter));
        UI_SameLine();
        if (UI_Button("Refresh List")) {
            ScanModelFiles();
        }
        UI_Separator();

        if (UI_BeginChild("model_grid_child", 0, 0, false, 0)) {
            float window_visible_x2 = UI_GetWindowPos_X() + UI_GetWindowContentRegionMax_X();
            float style_spacing_x = UI_GetStyle_ItemSpacing_X();
            float item_size = 96.0f;

            for (int i = 0; i < g_EditorState.num_model_files; ++i) {
                ModelBrowserEntry* entry = &g_EditorState.model_browser_entries[i];
                if (g_EditorState.model_search_filter[0] != '\0' && _stristr(entry->file_path, g_EditorState.model_search_filter) == NULL) {
                    continue;
                }

                if (entry->thumbnail_texture == 0) {
                    char path_buffer[256];
                    sprintf(path_buffer, "models/%s", entry->file_path);
                    LoadedModel* temp_model = Model_Load(path_buffer);

                    glGenTextures(1, &entry->thumbnail_texture);
                    glBindTexture(GL_TEXTURE_2D, entry->thumbnail_texture);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                    if (temp_model) {
                        glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.model_thumb_fbo);
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, entry->thumbnail_texture, 0);
                        glViewport(0, 0, 128, 128);
                        glClearColor(0.2f, 0.2f, 0.25f, 1.0f);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                        Mat4 view = mat4_lookAt((Vec3) { 1, 1, 1 }, (Vec3) { 0, 0, 0 }, (Vec3) { 0, 1, 0 });
                        Mat4 proj = mat4_perspective(45.0f * (M_PI / 180.0f), 1.0f, 0.1f, 100.0f);

                        glUseProgram(renderer->mainShader);
                        glUniform1i(glGetUniformLocation(renderer->mainShader, "is_unlit"), 1);
                        glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "view"), 1, GL_FALSE, view.m);
                        glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "projection"), 1, GL_FALSE, proj.m);

                        SceneObject temp_obj;
                        memset(&temp_obj, 0, sizeof(SceneObject));
                        temp_obj.model = temp_model;
                        mat4_identity(&temp_obj.modelMatrix);
                        render_object(renderer->mainShader, &temp_obj, false, NULL);

                        Model_Free(temp_model);
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    }
                }

                UI_PushID(i);
                UI_BeginGroup();

                if (UI_ImageButton_Flip("##thumb", (void*)(intptr_t)entry->thumbnail_texture, item_size, item_size)) {
                    if (g_EditorState.texture_browser_target == MODEL_BROWSER_TARGET_SPRINKLE) {
                        char full_path[256];
                        sprintf(full_path, "models/%s", g_EditorState.model_browser_entries[i].file_path);
                        strncpy(g_EditorState.sprinkle_model_path, full_path, sizeof(g_EditorState.sprinkle_model_path) - 1);
                        g_EditorState.show_add_model_popup = false;
                    }
                    else {
                        if (scene->numObjects < MAX_MODELS) {
                            scene->numObjects++;
                            scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
                            SceneObject* newObj = &scene->objects[scene->numObjects - 1];
                            memset(newObj, 0, sizeof(SceneObject));

                            char full_model_path[256];
                            sprintf(full_model_path, "models/%s", g_EditorState.model_browser_entries[i].file_path);
                            strncpy(newObj->modelPath, full_model_path, sizeof(newObj->modelPath) - 1);

                            Vec3 forward = { cosf(g_EditorState.editor_camera.pitch) * sinf(g_EditorState.editor_camera.yaw), sinf(g_EditorState.editor_camera.pitch), -cosf(g_EditorState.editor_camera.pitch) * cosf(g_EditorState.editor_camera.yaw) };
                            vec3_normalize(&forward);
                            newObj->pos = vec3_add(g_EditorState.editor_camera.position, vec3_muls(forward, 10.0f));
                            newObj->scale = (Vec3){ 1,1,1 };
                            SceneObject_UpdateMatrix(newObj);

                            newObj->model = Model_Load(newObj->modelPath);

                            if (newObj->model && newObj->model->combinedVertexData && newObj->model->totalIndexCount > 0) {
                                Mat4 physics_transform = create_trs_matrix(newObj->pos, newObj->rot, (Vec3) { 1, 1, 1 });
                                newObj->physicsBody = Physics_CreateStaticTriangleMesh(engine->physicsWorld, newObj->model->combinedVertexData, newObj->model->totalVertexCount, newObj->model->combinedIndexData, newObj->model->totalIndexCount, physics_transform, newObj->scale);
                            }
                            Undo_PushCreateEntity(scene, ENTITY_MODEL, scene->numObjects - 1, "Create Model");
                            g_EditorState.show_add_model_popup = false;
                        }
                        else {
                            Console_Printf_Error("Cannot add model, MAX_MODELS limit reached.");
                        }
                    }
                }

                UI_TextWrapped(entry->file_path);
                UI_EndGroup();

                float last_button_x2 = UI_GetItemRectMax_X();
                float next_button_x2 = last_button_x2 + style_spacing_x + item_size;
                if (i + 1 < g_EditorState.num_model_files && next_button_x2 < window_visible_x2) {
                    UI_SameLine();
                }
                UI_PopID();
            }
        }
        UI_EndChild();
    }
    UI_End();
}
static void Editor_RenderSoundBrowser(Scene* scene) {
    if (!g_EditorState.show_sound_browser_popup) return;

    UI_SetNextWindowSize(400, 500);
    if (UI_Begin("Sound Browser", &g_EditorState.show_sound_browser_popup)) {
        UI_InputText("Search", g_EditorState.sound_search_filter, sizeof(g_EditorState.sound_search_filter));
        UI_Separator();

        if (UI_BeginChild("sound_list_child", 0, -40, true, 0)) {
            if (g_EditorState.num_sound_files > 0) {
                for (int i = 0; i < g_EditorState.num_sound_files; ++i) {
                    const char* sound_name = g_EditorState.sound_file_list[i];
                    if (g_EditorState.sound_search_filter[0] == '\0' || _stristr(sound_name, g_EditorState.sound_search_filter) != NULL) {
                        if (UI_Selectable(sound_name, g_EditorState.selected_sound_file_index == i)) {
                            g_EditorState.selected_sound_file_index = i;
                            if (g_EditorState.preview_sound_source) SoundSystem_DeleteSource(g_EditorState.preview_sound_source);
                            if (g_EditorState.preview_sound_buffer) SoundSystem_DeleteBuffer(g_EditorState.preview_sound_buffer);
                            char path_buffer[256];
                            sprintf(path_buffer, "sounds/%s", g_EditorState.sound_file_list[i]);
                            g_EditorState.preview_sound_buffer = SoundSystem_LoadSound(path_buffer);
                            if (g_EditorState.preview_sound_buffer != 0) {
                                g_EditorState.preview_sound_source = SoundSystem_PlaySound(g_EditorState.preview_sound_buffer, g_EditorState.editor_camera.position, 10.0f, 1.0f, 1000.0f, false);
                            }
                        }
                    }
                }
            }
        }
        UI_EndChild();

        UI_Separator();

        if (g_EditorState.selected_sound_file_index != -1) {
            if (UI_Button("Add to Scene")) {
                if (scene->numSoundEntities < MAX_SOUNDS) {
                    SoundEntity* s = &scene->soundEntities[scene->numSoundEntities];
                    memset(s, 0, sizeof(SoundEntity));
                    sprintf(s->targetname, "Sound_%d", scene->numSoundEntities);
                    char full_path[256];
                    sprintf(full_path, "sounds/%s", g_EditorState.sound_file_list[g_EditorState.selected_sound_file_index]);
                    strncpy(s->soundPath, full_path, sizeof(s->soundPath) - 1);
                    s->pos = g_EditorState.editor_camera.position;
                    s->volume = 1.0f;
                    s->pitch = 1.0f;
                    s->maxDistance = 50.0f;
                    s->bufferID = SoundSystem_LoadSound(s->soundPath);
                    scene->numSoundEntities++;
                    Undo_PushCreateEntity(scene, ENTITY_SOUND, scene->numSoundEntities - 1, "Create Sound");
                    g_EditorState.show_sound_browser_popup = false;
                }
                else {
                    Console_Printf_Error("[error] Max sound entities reached.");
                }
            }
            UI_SameLine();
            if (UI_Button("Preview")) {
                if (g_EditorState.preview_sound_source) SoundSystem_DeleteSource(g_EditorState.preview_sound_source);
                if (g_EditorState.preview_sound_buffer) {
                    g_EditorState.preview_sound_source = SoundSystem_PlaySound(g_EditorState.preview_sound_buffer, g_EditorState.editor_camera.position, 10.0f, 1.0f, 1000.0f, false);
                }
            }
        }
    }
    if (!g_EditorState.show_sound_browser_popup) {
        if (g_EditorState.preview_sound_source) {
            SoundSystem_DeleteSource(g_EditorState.preview_sound_source);
            g_EditorState.preview_sound_source = 0;
        }
        if (g_EditorState.preview_sound_buffer) {
            SoundSystem_DeleteBuffer(g_EditorState.preview_sound_buffer);
            g_EditorState.preview_sound_buffer = 0;
        }
    }
    UI_End();
}
static void render_markdown_line(const char* line) {
    if (strncmp(line, "## ", 3) == 0) {
        UI_TextColored((Vec4) { 0.6f, 0.8f, 1.0f, 1.0f }, "%s", line + 3);
        return;
    }
    if (strncmp(line, "# ", 2) == 0) {
        UI_TextColored((Vec4) { 0.8f, 1.0f, 0.8f, 1.0f }, "%s", line + 2);
        return;
    }
    if (strcmp(line, "---") == 0) {
        UI_Separator();
        return;
    }
    if (strncmp(line, "* ", 2) == 0) {
        UI_BulletText("%s", line + 2);
        return;
    }
    if (strncmp(line, "|", 1) == 0) {
        UI_TextWrapped("%s", line);
        return;
    }

    const char* p = line;
    while (*p) {
        const char* bold_start = strstr(p, "**");

        if (!bold_start) {
            UI_TextWrapped("%s", p);
            break;
        }

        if (bold_start > p) {
            char buffer[1024];
            size_t len = bold_start - p;
            if (len > sizeof(buffer) - 1) len = sizeof(buffer) - 1;
            strncpy(buffer, p, len);
            buffer[len] = '\0';
            UI_TextWrapped("%s", buffer);
            UI_SameLine(0, 0);
        }

        const char* bold_end = strstr(bold_start + 2, "**");

        if (!bold_end) {
            UI_TextWrapped("%s", bold_start);
            break;
        }

        char bold_text[1024];
        size_t bold_len = bold_end - (bold_start + 2);
        if (bold_len > sizeof(bold_text) - 1) bold_len = sizeof(bold_text) - 1;
        strncpy(bold_text, bold_start + 2, bold_len);
        bold_text[bold_len] = '\0';

        UI_TextColored((Vec4) { 1.0f, 1.0f, 0.5f, 1.0f }, "%s", bold_text);

        p = bold_end + 2;

        if (*p) {
            UI_SameLine(0, 0);
        }
    }
}
static void Editor_RenderHelpWindow() {
    if (!g_EditorState.show_help_window) return;

    UI_SetNextWindowSize(800, 600);
    if (UI_Begin("Help & Documentation", &g_EditorState.show_help_window)) {
        UI_BeginChild("doc_list_child", 200, 0, true, 0);
        if (UI_Button("Refresh List")) {
            ScanDocFiles();
        }
        UI_Separator();
        if (g_EditorState.num_doc_files > 0) {
            for (int i = 0; i < g_EditorState.num_doc_files; ++i) {
                if (UI_Selectable(g_EditorState.doc_files[i], g_EditorState.selected_doc_index == i)) {
                    g_EditorState.selected_doc_index = i;
                    char path_buffer[256];
                    sprintf(path_buffer, "docs/%s", g_EditorState.doc_files[i]);

                    FILE* f = fopen(path_buffer, "rb");
                    if (f) {
                        fseek(f, 0, SEEK_END);
                        long length = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        if (g_EditorState.current_doc_content) {
                            free(g_EditorState.current_doc_content);
                        }
                        g_EditorState.current_doc_content = malloc(length + 1);
                        if (g_EditorState.current_doc_content) {
                            fread(g_EditorState.current_doc_content, 1, length, f);
                            g_EditorState.current_doc_content[length] = '\0';
                        }
                        fclose(f);
                    }
                }
            }
        }
        UI_EndChild();
        UI_SameLine();

        UI_BeginChild("doc_preview_child", 0, 0, true, 0);
        if (g_EditorState.current_doc_content) {
            char* content_copy = strdup(g_EditorState.current_doc_content);
            char* line = strtok(content_copy, "\n");
            bool in_table = false;
            bool in_code_block = false;

            while (line) {
                if (strncmp(line, "```", 3) == 0) {
                    in_code_block = !in_code_block;
                    line = strtok(NULL, "\n");
                    continue;
                }

                if (in_code_block) {
                    UI_TextColored((Vec4) { 0.8f, 0.9f, 1.0f, 1.0f }, "%s", line);
                    line = strtok(NULL, "\n");
                    continue;
                }
                if (strncmp(line, "|", 1) == 0) {
                    if (!in_table) {
                        int columns = 0;
                        for (const char* p = line; *p; p++) if (*p == '|') columns++;
                        if (columns > 1) {
                            if (UI_BeginTable("md_table", columns - 1, 1 | (1 << 6), 0, 0)) {
                                in_table = true;
                            }
                        }
                    }

                    char next_line_peek = "";
                    char* next_line_ptr = strtok(NULL, "\n");
                    if (next_line_ptr) strcpy(next_line_peek, next_line_ptr);

                    if (in_table && strncmp(next_line_peek, "|:---", 5) == 0) {
                        UI_TableHeadersRow();
                        render_markdown_line(line);
                        line = strtok(NULL, "\n");
                        line = strtok(NULL, "\n");
                    }
                    else if (in_table) {
                        UI_TableNextRow();
                        render_markdown_line(line);
                    }
                    line = next_line_ptr;
                }
                else {
                    if (in_table) {
                        UI_EndTable();
                        in_table = false;
                    }
                    render_markdown_line(line);
                    line = strtok(NULL, "\n");
                }
            }
            if (in_table) {
                UI_EndTable();
            }
            free(content_copy);
        }
        else {
            UI_Text("Select a document to view.");
        }
        UI_EndChild();
    }
    UI_End();
}

static void Editor_RenderVertexToolsWindow(Scene* scene) {
    if (!g_EditorState.show_vertex_tools_window) {
        return;
    }

    UI_SetNextWindowSize(250, 0);
    if (UI_Begin("Vertex Tools", &g_EditorState.show_vertex_tools_window)) {
        if (g_EditorState.is_sculpting_mode_enabled) {
            UI_Text("Sculpting");
            UI_Text("Hold Shift to Smooth");
            UI_Text("Hold Ctrl to Lower");
            UI_Separator();
            UI_DragFloat("Radius##Sculpt", &g_EditorState.sculpt_brush_radius, 0.1f, 0.1f, 50.0f);
            UI_DragFloat("Strength##Sculpt", &g_EditorState.sculpt_brush_strength, 0.05f, 0.01f, 5.0f);

            if (UI_Button("Apply Noise...")) {
                g_EditorState.show_sculpt_noise_popup = true;
            }
        }
        else if (g_EditorState.is_painting_mode_enabled) {
            UI_Text("Vertex Painting");
            UI_Separator();
            UI_DragFloat("Radius##Paint", &g_EditorState.paint_brush_radius, 0.1f, 0.1f, 50.0f);
            UI_DragFloat("Strength##Paint", &g_EditorState.paint_brush_strength, 0.05f, 0.1f, 5.0f);

            UI_Separator();
            UI_Text("Paint Channel:");
            if (UI_RadioButton("R (Tex 2)", g_EditorState.paint_channel == 0)) { g_EditorState.paint_channel = 0; }
            if (UI_RadioButton("G (Tex 3)", g_EditorState.paint_channel == 1)) { g_EditorState.paint_channel = 1; }
            if (UI_RadioButton("B (Tex 4)", g_EditorState.paint_channel == 2)) { g_EditorState.paint_channel = 2; }

            UI_Separator();
            if (UI_Button("Erase All Paint")) {
                EditorSelection* primary = Editor_GetPrimarySelection();
                if (primary && primary->type == ENTITY_BRUSH) {
                    Brush* b = &scene->brushes[primary->index];
                    if (b->numVertices > 0) {
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);

                        for (int i = 0; i < b->numVertices; ++i) {
                            b->vertices[i].color.x = 0.0f;
                            b->vertices[i].color.y = 0.0f;
                            b->vertices[i].color.z = 0.0f;
                        }

                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Erase All Vertex Paint");
                    }
                }
            }
            UI_SameLine();
            if (UI_Button("Invert Channel")) {
                if (g_EditorState.num_selections > 0) {
                    Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);

                    bool modified_brushes[MAX_BRUSHES] = { false };

                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        if (sel->type == ENTITY_BRUSH && sel->face_index != -1) {
                            Brush* b = &scene->brushes[sel->index];
                            BrushFace* face = &b->faces[sel->face_index];

                            for (int j = 0; j < face->numVertexIndices; j++) {
                                int vert_idx = face->vertexIndices[j];
                                BrushVertex* vert = &b->vertices[vert_idx];

                                switch (g_EditorState.paint_channel) {
                                case 0: vert->color.x = 1.0f - vert->color.x; break;
                                case 1: vert->color.y = 1.0f - vert->color.y; break;
                                case 2: vert->color.z = 1.0f - vert->color.z; break;
                                }
                            }
                            modified_brushes[sel->index] = true;
                        }
                    }

                    for (int i = 0; i < scene->numBrushes; i++) {
                        if (modified_brushes[i]) {
                            Brush_CreateRenderData(&scene->brushes[i]);
                        }
                    }

                    Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Invert Vertex Paint");
                }
            }
        }
    }
    UI_End();

    if (!g_EditorState.show_vertex_tools_window) {
        g_EditorState.is_painting_mode_enabled = false;
        g_EditorState.is_sculpting_mode_enabled = false;
    }
}

static void Editor_RenderSculptNoisePopup(Scene* scene) {
    if (g_EditorState.show_sculpt_noise_popup) {
        UI_OpenPopup("Apply Noise");
        g_EditorState.show_sculpt_noise_popup = false;
    }

    if (UI_BeginPopupModal("Apply Noise", NULL, 0)) {
        static float min_noise = -0.5f;
        static float max_noise = 0.5f;
        static float frequency = 0.2f;
        static int octaves = 4;
        static float lacunarity = 2.0f;
        static float persistence = 0.5f;

        UI_Text("Apply smooth procedural noise to all vertices.");
        UI_Separator();
        UI_DragFloat("Min Displacement", &min_noise, 0.05f, -10.0f, 10.0f);
        UI_DragFloat("Max Displacement", &max_noise, 0.05f, -10.0f, 10.0f);
        UI_Separator();
        UI_DragFloat("Frequency", &frequency, 0.01f, 0.01f, 2.0f);
        UI_DragInt("Octaves", &octaves, 1, 1, 8);
        UI_DragFloat("Lacunarity", &lacunarity, 0.1f, 1.5f, 4.0f);
        UI_DragFloat("Persistence", &persistence, 0.05f, 0.1f, 1.0f);
        UI_Separator();

        if (UI_Button("Apply")) {
            EditorSelection* primary = Editor_GetPrimarySelection();
            if (primary && primary->type == ENTITY_BRUSH) {
                Brush* b = &scene->brushes[primary->index];
                if (b->numVertices > 0) {
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);

                    for (int i = 0; i < b->numVertices; ++i) {
                        float total = 0.0f;
                        float freq = frequency;
                        float amp = 1.0f;
                        float maxAmp = 0.0f;

                        for (int j = 0; j < octaves; ++j) {
                            float n = sin(b->vertices[i].pos.x * freq) * cos(b->vertices[i].pos.z * freq);
                            total += n * amp;
                            maxAmp += amp;
                            amp *= persistence;
                            freq *= lacunarity;
                        }

                        if (maxAmp > 0.0) {
                            total /= maxAmp;
                        }

                        float noise_val = min_noise + (total * 0.5f + 0.5f) * (max_noise - min_noise);
                        b->vertices[i].pos.y += noise_val;
                    }

                    Brush_CreateRenderData(b);
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Apply Smooth Noise to Brush");
                }
            }
            UI_CloseCurrentPopup();
        }
        UI_SameLine();
        if (UI_Button("Cancel")) {
            UI_CloseCurrentPopup();
        }
        UI_EndPopup();
    }
}

static void Editor_RenderSprinkleToolWindow(void) {
    if (!g_EditorState.show_sprinkle_tool_window) {
        return;
    }

    UI_SetNextWindowSize(300, 0);
    if (UI_Begin("Sprinkle Tool", &g_EditorState.show_sprinkle_tool_window)) {
        UI_Text("Entity to Sprinkle");
        char model_button_label[256];
        sprintf(model_button_label, "Model: %s", g_EditorState.sprinkle_model_path);
        if (UI_Button(model_button_label)) {
            g_EditorState.texture_browser_target = MODEL_BROWSER_TARGET_SPRINKLE;
            g_EditorState.show_add_model_popup = true;
            ScanModelFiles();
        }

        UI_Separator();
        UI_Text("Brush Settings");
        UI_DragFloat("Radius", &g_EditorState.sprinkle_radius, 0.1f, 0.1f, 50.0f);
        UI_DragFloat("Density (obj/sec)", &g_EditorState.sprinkle_density, 0.1f, 0.1f, 100.0f);

        UI_Separator();
        UI_Text("Placement Settings");
        UI_Checkbox("Align to Surface Normal", &g_EditorState.sprinkle_align_to_normal);
        UI_Checkbox("Randomize Yaw", &g_EditorState.sprinkle_random_yaw);
        UI_DragFloat("Min Scale", &g_EditorState.sprinkle_scale_min, 0.01f, 0.1f, 10.0f);
        UI_DragFloat("Max Scale", &g_EditorState.sprinkle_scale_max, 0.01f, 0.1f, 10.0f);

        UI_Separator();
        UI_Text("Mode");
        UI_RadioButton_Int("Additive", &g_EditorState.sprinkle_mode, 0);
        UI_SameLine();
        UI_RadioButton_Int("Subtractive", &g_EditorState.sprinkle_mode, 1);
    }
    UI_End();
}

static void Editor_RenderBakeLightingWindow(Scene* scene, Engine* engine) {
    if (g_EditorState.show_bake_lighting_popup) {
        UI_Begin("Bake Lighting", &g_EditorState.show_bake_lighting_popup);
        UI_Text("Baking will save the current map file first.");
        UI_Separator();

        const char* resolutions[] = { "16", "32", "64", "128", "256", "512" };
        UI_Combo("Resolution", &g_EditorState.bake_resolution, resolutions, 6, -1);

        UI_DragInt("Bounces", &g_EditorState.bake_bounces, 1, 0, 4);

        UI_Separator();

        if (UI_Button("Bake")) {
            Scene_SaveMap(scene, NULL, g_EditorState.currentMapPath);

            int resolution_values[] = { 16, 32, 64, 128, 256, 512 };
            int resolution = resolution_values[g_EditorState.bake_resolution];

            Lightmapper_Generate(scene, engine, resolution, g_EditorState.bake_bounces);

            char map_name_sanitized[128];
            const char* last_slash = strrchr(scene->mapPath, '/');
            const char* last_bslash = strrchr(scene->mapPath, '\\');
            const char* map_filename = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : scene->mapPath);
            const char* dot = strrchr(map_filename, '.');
            if (dot) {
                size_t len = dot - map_filename;
                strncpy(map_name_sanitized, map_filename, len);
                map_name_sanitized[len] = '\0';
            }
            else {
                strcpy(map_name_sanitized, map_filename);
            }

            for (int i = 0; i < scene->numBrushes; ++i) {
                Brush* b = &scene->brushes[i];
                if (b->lightmapAtlas != 0) { glDeleteTextures(1, &b->lightmapAtlas); b->lightmapAtlas = 0; }
                if (b->directionalLightmapAtlas != 0) { glDeleteTextures(1, &b->directionalLightmapAtlas); b->directionalLightmapAtlas = 0; }
                Brush_GenerateLightmapAtlas(b, map_name_sanitized, i, scene->lightmapResolution);
                Brush_CreateRenderData(b);
            }

            for (int i = 0; i < scene->numDecals; ++i) {
                Decal* d = &scene->decals[i];
                if (d->lightmapAtlas != 0) { glDeleteTextures(1, &d->lightmapAtlas); d->lightmapAtlas = 0; }
                if (d->directionalLightmapAtlas != 0) { glDeleteTextures(1, &d->directionalLightmapAtlas); d->directionalLightmapAtlas = 0; }
                Decal_LoadLightmaps(d, map_name_sanitized, i);
            }

            for (int i = 0; i < scene->numObjects; ++i) {
                SceneObject* obj = &scene->objects[i];
                if (obj->bakedVertexColors) {
                    free(obj->bakedVertexColors);
                    obj->bakedVertexColors = NULL;
                }
                if (obj->bakedVertexDirections) {
                    free(obj->bakedVertexDirections);
                    obj->bakedVertexDirections = NULL;
                }
                SceneObject_LoadVertexLighting(obj, i, scene->mapPath);
                SceneObject_LoadVertexDirectionalLighting(obj, i, scene->mapPath);
            }

            scene->static_shadows_generated = true;
            Console_Printf("Lightmap reload complete.");

            g_EditorState.show_bake_lighting_popup = false;
        }
        UI_SameLine();
        if (UI_Button("Cancel")) {
            g_EditorState.show_bake_lighting_popup = false;
        }
        UI_End();
    }
}

static void Editor_RenderAboutWindow() {
    if (!g_EditorState.show_about_window) {
        return;
    }

    UI_SetNextWindowSize(320, 180);
    if (UI_Begin("About Tectonic Editor", &g_EditorState.show_about_window)) {
        UI_Text("Tectonic Editor");
        UI_Separator();
        UI_Text("Version: D.E.V. (Build %d)", Compat_GetBuildNumber());
        UI_Text("Build Date: %s, %s", __DATE__, __TIME__);
        UI_Text("Architecture: %s", ARCH_STRING);
        UI_Separator();
        UI_Text("Copyright (c) 2025 Soft Sprint Studios");
        UI_Text("All rights reserved.");
        UI_Separator();

        if (UI_Button("OK")) {
            g_EditorState.show_about_window = false;
        }
    }
    UI_End();
}
static void Editor_RenderTextureBrowser(Scene* scene) {
    if (!g_EditorState.show_texture_browser) return;
    EditorSelection* primary = Editor_GetPrimarySelection();

    UI_SetNextWindowSize(600, 500);
    if (UI_Begin("Texture Browser", &g_EditorState.show_texture_browser)) {
        UI_InputText("Search", g_EditorState.texture_search_filter, sizeof(g_EditorState.texture_search_filter));
        UI_Separator();

        float window_visible_x2 = UI_GetWindowPos_X() + UI_GetWindowContentRegionMax_X();
        float style_spacing_x = UI_GetStyle_ItemSpacing_X();
        int mat_count = TextureManager_GetMaterialCount();

        for (int i = 0; i < mat_count; ++i) {
            Material* mat = TextureManager_GetMaterial(i);

            if (g_EditorState.texture_search_filter[0] != '\0' &&
                _stristr(mat->name, g_EditorState.texture_search_filter) == NULL) {
                continue;
            }

            if (strncmp(mat->diffusePath, "models\\", strlen("models\\")) == 0 ||
                strncmp(mat->normalPath, "models\\", strlen("models\\")) == 0 ||
                strncmp(mat->rmaPath, "models\\", strlen("models\\")) == 0) {
                continue;
            }

            if (!mat->isLoaded) {
                TextureManager_LoadMaterialTextures(mat);
            }

            UI_PushID(i);
            char btn_id[32];
            sprintf(btn_id, "##mat_btn_%d", i);
            if (UI_ImageButton(btn_id, (void*)(intptr_t)mat->diffuseMap, 64, 64)) {
                bool is_face_material_target = (g_EditorState.texture_browser_target >= 0 && g_EditorState.texture_browser_target <= 3);

                if (g_EditorState.num_selections > 0 && is_face_material_target) {
                    Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);

                    int* modified_brushes = malloc(g_EditorState.num_selections * sizeof(int));
                    int num_modified_brushes = 0;

                    for (int sel_idx = 0; sel_idx < g_EditorState.num_selections; ++sel_idx) {
                        EditorSelection* sel = &g_EditorState.selections[sel_idx];
                        if (sel->type == ENTITY_BRUSH && sel->face_index != -1) {
                            Brush* b = &scene->brushes[sel->index];
                            BrushFace* face = &b->faces[sel->face_index];

                            switch (g_EditorState.texture_browser_target) {
                            case 0: face->material = mat; break;
                            case 1: face->material2 = mat; break;
                            case 2: face->material3 = mat; break;
                            case 3: face->material4 = mat; break;
                            }

                            bool already_added = false;
                            for (int k = 0; k < num_modified_brushes; ++k) {
                                if (modified_brushes[k] == sel->index) {
                                    already_added = true;
                                    break;
                                }
                            }
                            if (!already_added) {
                                modified_brushes[num_modified_brushes++] = sel->index;
                            }
                        }
                    }

                    for (int k = 0; k < num_modified_brushes; ++k) {
                        Brush_CreateRenderData(&scene->brushes[modified_brushes[k]]);
                    }

                    free(modified_brushes);

                    Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Change Face Materials");
                    g_EditorState.show_texture_browser = false;
                }
                else if (primary && primary->type == ENTITY_BRUSH && g_EditorState.texture_browser_target == 5) {
                    Brush* b = &scene->brushes[primary->index];
                    b->glassNormalMap = mat;
                    g_EditorState.show_texture_browser = false;
                }
                else if (primary && primary->type == ENTITY_DECAL) {
                    Decal* d = &scene->decals[primary->index];
                    Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
                    d->material = mat;
                    Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Change Decal Material");
                    g_EditorState.show_texture_browser = false;
                }
                else if (primary && primary->type == ENTITY_LIGHT && g_EditorState.texture_browser_target == 4) {
                    Light* light = &scene->lights[primary->index];
                    Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                    strncpy(light->cookiePath, mat->name, sizeof(light->cookiePath) - 1);
                    light->cookiePath[sizeof(light->cookiePath) - 1] = '\0';
                    light->cookieMap = mat->diffuseMap;
                    if (light->cookieMapHandle != 0) { glMakeTextureHandleNonResidentARB(light->cookieMapHandle); }
                    light->cookieMapHandle = glGetTextureHandleARB(light->cookieMap);
                    glMakeTextureHandleResidentARB(light->cookieMapHandle);
                    Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Set Light Cookie");
                    g_EditorState.show_texture_browser = false;
                }
                else if (primary && primary->type == ENTITY_SPRITE && g_EditorState.texture_browser_target == 6) {
                    Undo_BeginEntityModification(scene, ENTITY_SPRITE, primary->index);
                    scene->sprites[primary->index].material = mat;
                    Undo_EndEntityModification(scene, ENTITY_SPRITE, primary->index, "Change Sprite Material");
                    g_EditorState.show_texture_browser = false;
                }
                else if (g_EditorState.texture_browser_target == TEXTURE_TARGET_REPLACE_FIND) {
                    g_EditorState.find_material_index = i;
                    g_EditorState.show_texture_browser = false;
                }
                else if (g_EditorState.texture_browser_target == TEXTURE_TARGET_REPLACE_WITH) {
                    g_EditorState.replace_material_index = i;
                    g_EditorState.show_texture_browser = false;
                }
            }

            if (UI_IsItemHovered()) {
                UI_BeginTooltip();
                UI_Text(mat->name);
                UI_Image((void*)(intptr_t)mat->diffuseMap, 256, 256);
                UI_EndTooltip();
            }

            float last_button_x2 = UI_GetItemRectMax_X();
            float next_button_x2 = last_button_x2 + style_spacing_x + 64;
            if (i + 1 < mat_count && next_button_x2 < window_visible_x2) {
                UI_SameLine();
            }
            UI_PopID();
        }
    }
    UI_End();
}
static void Editor_RenderReplaceTexturesUI(Scene* scene) {
    if (!g_EditorState.show_replace_textures_popup) {
        return;
    }

    UI_SetNextWindowSize(350, 400);
    if (UI_Begin("Replace Textures", &g_EditorState.show_replace_textures_popup)) {
        UI_Text("Find Material:");
        Material* find_mat = (g_EditorState.find_material_index != -1) ? TextureManager_GetMaterial(g_EditorState.find_material_index) : NULL;
        char find_button_label[128];
        sprintf(find_button_label, "%s##Find", find_mat ? find_mat->name : "None");
        if (UI_Button(find_button_label)) {
            g_EditorState.texture_browser_target = TEXTURE_TARGET_REPLACE_FIND;
            g_EditorState.show_texture_browser = true;
        }
        if (find_mat) {
            UI_Image((void*)(intptr_t)find_mat->diffuseMap, 64, 64);
        }

        UI_Separator();

        UI_Text("Replace With:");
        Material* replace_mat = (g_EditorState.replace_material_index != -1) ? TextureManager_GetMaterial(g_EditorState.replace_material_index) : NULL;
        char replace_button_label[128];
        sprintf(replace_button_label, "%s##Replace", replace_mat ? replace_mat->name : "None");
        if (UI_Button(replace_button_label)) {
            g_EditorState.texture_browser_target = TEXTURE_TARGET_REPLACE_WITH;
            g_EditorState.show_texture_browser = true;
        }
        if (replace_mat) {
            UI_Image((void*)(intptr_t)replace_mat->diffuseMap, 64, 64);
        }

        UI_Separator();

        if (UI_Button("Replace All in Scene")) {
            if (g_EditorState.find_material_index != -1 && g_EditorState.replace_material_index != -1 && g_EditorState.find_material_index != g_EditorState.replace_material_index) {
                Material* find_mat_ptr = TextureManager_GetMaterial(g_EditorState.find_material_index);
                Material* replace_mat_ptr = TextureManager_GetMaterial(g_EditorState.replace_material_index);
                int replaced_count = 0;

                for (int i = 0; i < scene->numBrushes; ++i) {
                    Brush* b = &scene->brushes[i];
                    bool brush_modified = false;

                    for (int j = 0; j < b->numFaces; ++j) {
                        BrushFace* face = &b->faces[j];
                        if (face->material == find_mat_ptr) { face->material = replace_mat_ptr; brush_modified = true; replaced_count++; }
                        if (face->material2 == find_mat_ptr) { face->material2 = replace_mat_ptr; brush_modified = true; replaced_count++; }
                        if (face->material3 == find_mat_ptr) { face->material3 = replace_mat_ptr; brush_modified = true; replaced_count++; }
                        if (face->material4 == find_mat_ptr) { face->material4 = replace_mat_ptr; brush_modified = true; replaced_count++; }
                    }

                    if (brush_modified) {
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, i);
                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, i, "Replace Textures");
                    }
                }
                g_EditorState.show_replace_textures_popup = false;
            }
        }
    }
    UI_End();
}
static void Editor_RenderFaceEditSheet(Scene* scene, Engine* engine) {
    UI_SetNextWindowSize(320, 520);
    if (UI_Begin_NoClose("Face Edit Sheet")) {
        if (g_EditorState.num_selections == 0) {
            UI_Text("No face selected.");
            UI_End();
            return;
        }

        bool all_are_brush_faces = true;
        for (int i = 0; i < g_EditorState.num_selections; ++i) {
            if (g_EditorState.selections[i].type != ENTITY_BRUSH || g_EditorState.selections[i].face_index == -1) {
                all_are_brush_faces = false;
                break;
            }
        }

        if (!all_are_brush_faces) {
            UI_Text("Selection must contain only brush faces.");
            UI_End();
            return;
        }

        EditorSelection* primary = Editor_GetPrimarySelection();
        Brush* primary_brush = &scene->brushes[primary->index];
        BrushFace* primary_face = &primary_brush->faces[primary->face_index];

        static int selected_material_layer = 0;

        if (UI_BeginTabBar("FaceEditTabs", 0)) {
            if (UI_BeginTabItem("Material")) {
                UI_Text("Texture Layer");
                UI_RadioButton_Int("Base", &selected_material_layer, 0); UI_SameLine();
                UI_RadioButton_Int("Blend R", &selected_material_layer, 1); UI_SameLine();
                UI_RadioButton_Int("Blend G", &selected_material_layer, 2); UI_SameLine();
                UI_RadioButton_Int("Blend B", &selected_material_layer, 3);
                UI_Separator();

                Material* target_material = NULL;
                Vec2* target_scale = NULL;
                Vec2* target_offset = NULL;
                float* target_rotation = NULL;

                switch (selected_material_layer) {
                case 0:
                    target_material = primary_face->material;
                    target_scale = &primary_face->uv_scale;
                    target_offset = &primary_face->uv_offset;
                    target_rotation = &primary_face->uv_rotation;
                    break;
                case 1:
                    target_material = primary_face->material2;
                    target_scale = &primary_face->uv_scale2;
                    target_offset = &primary_face->uv_offset2;
                    target_rotation = &primary_face->uv_rotation2;
                    break;
                case 2:
                    target_material = primary_face->material3;
                    target_scale = &primary_face->uv_scale3;
                    target_offset = &primary_face->uv_offset3;
                    target_rotation = &primary_face->uv_rotation3;
                    break;
                case 3:
                    target_material = primary_face->material4;
                    target_scale = &primary_face->uv_scale4;
                    target_offset = &primary_face->uv_offset4;
                    target_rotation = &primary_face->uv_rotation4;
                    break;
                }

                UI_Image((void*)(intptr_t)(target_material ? target_material->diffuseMap : missingTextureID), 128, 128);
                UI_SameLine();
                UI_BeginGroup();
                UI_Text("Current Texture:");
                UI_TextWrapped(target_material ? target_material->name : "None");
                if (UI_Button("Browse...")) {
                    g_EditorState.texture_browser_target = selected_material_layer;
                    g_EditorState.show_texture_browser = true;
                }
                UI_EndGroup();
                UI_Separator();

                if (target_scale && target_offset && target_rotation) {
                    UI_Text("Texture Scale"); UI_SameLine(); UI_SetNextItemWidth(80);
                    if (UI_InputFloat("X##Scale", &target_scale->x, 0.01f, 0.1f, "%.2f")) {
                        for (int i = 0; i < g_EditorState.num_selections; ++i) {
                            EditorSelection* sel = &g_EditorState.selections[i];
                            BrushFace* face = &scene->brushes[sel->index].faces[sel->face_index];
                            switch (selected_material_layer) {
                            case 0: face->uv_scale.x = target_scale->x; break;
                            case 1: face->uv_scale2.x = target_scale->x; break;
                            case 2: face->uv_scale3.x = target_scale->x; break;
                            case 3: face->uv_scale4.x = target_scale->x; break;
                            }
                            Brush_CreateRenderData(&scene->brushes[sel->index]);
                        }
                    }
                    if (UI_IsItemActivated()) { Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections); }
                    if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Edit Face UVs"); }

                    UI_SameLine(); UI_SetNextItemWidth(80);
                    if (UI_InputFloat("Y##Scale", &target_scale->y, 0.01f, 0.1f, "%.2f")) {
                        for (int i = 0; i < g_EditorState.num_selections; ++i) {
                            EditorSelection* sel = &g_EditorState.selections[i];
                            BrushFace* face = &scene->brushes[sel->index].faces[sel->face_index];
                            switch (selected_material_layer) {
                            case 0: face->uv_scale.y = target_scale->y; break;
                            case 1: face->uv_scale2.y = target_scale->y; break;
                            case 2: face->uv_scale3.y = target_scale->y; break;
                            case 3: face->uv_scale4.y = target_scale->y; break;
                            }
                            Brush_CreateRenderData(&scene->brushes[sel->index]);
                        }
                    }
                    if (UI_IsItemActivated()) { Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections); }
                    if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Edit Face UVs"); }

                    UI_Text("Texture Shift"); UI_SameLine(); UI_SetNextItemWidth(80);
                    if (UI_InputFloat("X##Shift", &target_offset->x, 0.1f, 1.0f, "%.2f")) {
                        for (int i = 0; i < g_EditorState.num_selections; ++i) {
                            EditorSelection* sel = &g_EditorState.selections[i];
                            BrushFace* face = &scene->brushes[sel->index].faces[sel->face_index];
                            switch (selected_material_layer) {
                            case 0: face->uv_offset.x = target_offset->x; break;
                            case 1: face->uv_offset2.x = target_offset->x; break;
                            case 2: face->uv_offset3.x = target_offset->x; break;
                            case 3: face->uv_offset4.x = target_offset->x; break;
                            }
                            Brush_CreateRenderData(&scene->brushes[sel->index]);
                        }
                    }
                    if (UI_IsItemActivated()) { Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections); }
                    if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Edit Face UVs"); }

                    UI_SameLine(); UI_SetNextItemWidth(80);
                    if (UI_InputFloat("Y##Shift", &target_offset->y, 0.1f, 1.0f, "%.2f")) {
                        for (int i = 0; i < g_EditorState.num_selections; ++i) {
                            EditorSelection* sel = &g_EditorState.selections[i];
                            BrushFace* face = &scene->brushes[sel->index].faces[sel->face_index];
                            switch (selected_material_layer) {
                            case 0: face->uv_offset.y = target_offset->y; break;
                            case 1: face->uv_offset2.y = target_offset->y; break;
                            case 2: face->uv_offset3.y = target_offset->y; break;
                            case 3: face->uv_offset4.y = target_offset->y; break;
                            }
                            Brush_CreateRenderData(&scene->brushes[sel->index]);
                        }
                    }
                    if (UI_IsItemActivated()) { Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections); }
                    if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Edit Face UVs"); }

                    UI_Text("Rotation"); UI_SameLine(); UI_SetNextItemWidth(172);
                    if (UI_DragFloat("##Rotation", target_rotation, 1.0f, -360.0f, 360.0f)) {
                        for (int i = 0; i < g_EditorState.num_selections; ++i) {
                            EditorSelection* sel = &g_EditorState.selections[i];
                            BrushFace* face = &scene->brushes[sel->index].faces[sel->face_index];
                            switch (selected_material_layer) {
                            case 0: face->uv_rotation = *target_rotation; break;
                            case 1: face->uv_rotation2 = *target_rotation; break;
                            case 2: face->uv_rotation3 = *target_rotation; break;
                            case 3: face->uv_rotation4 = *target_rotation; break;
                            }
                            Brush_CreateRenderData(&scene->brushes[sel->index]);
                        }
                    }
                    if (UI_IsItemActivated()) { Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections); }
                    if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Edit Face UVs"); }
                }

                UI_Separator();
                UI_Text("Justify");
                if (UI_Button("L")) { for (int i = 0; i < g_EditorState.num_selections; ++i) { EditorSelection* sel = &g_EditorState.selections[i]; Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index); scene->brushes[sel->index].faces[sel->face_index].uv_offset.x = 0; Brush_CreateRenderData(&scene->brushes[sel->index]); Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Justify UV"); } } UI_SameLine();
                if (UI_Button("R")) { for (int i = 0; i < g_EditorState.num_selections; ++i) { EditorSelection* sel = &g_EditorState.selections[i]; Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index); BrushFace* f = &scene->brushes[sel->index].faces[sel->face_index]; f->uv_offset.x = 1.0f - (f->uv_scale.x > 0 ? fmodf(1.0f, f->uv_scale.x) : 0); Brush_CreateRenderData(&scene->brushes[sel->index]); Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Justify UV"); } } UI_SameLine();
                if (UI_Button("T")) { for (int i = 0; i < g_EditorState.num_selections; ++i) { EditorSelection* sel = &g_EditorState.selections[i]; Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index); scene->brushes[sel->index].faces[sel->face_index].uv_offset.y = 0; Brush_CreateRenderData(&scene->brushes[sel->index]); Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Justify UV"); } } UI_SameLine();
                if (UI_Button("B")) { for (int i = 0; i < g_EditorState.num_selections; ++i) { EditorSelection* sel = &g_EditorState.selections[i]; Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index); BrushFace* f = &scene->brushes[sel->index].faces[sel->face_index]; f->uv_offset.y = 1.0f - (f->uv_scale.y > 0 ? fmodf(1.0f, f->uv_scale.y) : 0); Brush_CreateRenderData(&scene->brushes[sel->index]); Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Justify UV"); } } UI_SameLine();
                if (UI_Button("C")) { for (int i = 0; i < g_EditorState.num_selections; ++i) { EditorSelection* sel = &g_EditorState.selections[i]; Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index); BrushFace* f = &scene->brushes[sel->index].faces[sel->face_index]; f->uv_offset.x = 0.5f - (f->uv_scale.x / 2.0f); f->uv_offset.y = 0.5f - (f->uv_scale.y / 2.0f); Brush_CreateRenderData(&scene->brushes[sel->index]); Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Justify UV"); } } UI_SameLine();
                if (UI_Button("Fit")) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Brush* b = &scene->brushes[sel->index];
                        BrushFace* face = &b->faces[sel->face_index];
                        if (face->numVertexIndices >= 3) {
                            Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                            Vec3 p0 = b->vertices[face->vertexIndices[0]].pos;
                            Vec3 p1 = b->vertices[face->vertexIndices[1]].pos;
                            Vec3 p2 = b->vertices[face->vertexIndices[2]].pos;
                            Vec3 face_normal = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
                            vec3_normalize(&face_normal);
                            Vec3 u_axis = vec3_sub(p1, p0);
                            vec3_normalize(&u_axis);
                            Vec3 v_axis = vec3_cross(face_normal, u_axis);
                            float min_u = FLT_MAX, max_u = -FLT_MAX;
                            float min_v = FLT_MAX, max_v = -FLT_MAX;
                            for (int j = 0; j < face->numVertexIndices; ++j) {
                                Vec3 vert_pos = b->vertices[face->vertexIndices[j]].pos;
                                float u = vec3_dot(vert_pos, u_axis);
                                float v = vec3_dot(vert_pos, v_axis);
                                if (u < min_u) min_u = u; if (u > max_u) max_u = u;
                                if (v < min_v) min_v = v; if (v > max_v) max_v = v;
                            }
                            float u_range = max_u - min_u;
                            float v_range = max_v - min_v;
                            if (u_range > 1e-6 && v_range > 1e-6) {
                                face->uv_scale.x = u_range;
                                face->uv_scale.y = v_range;
                                face->uv_offset.x = -min_u / u_range;
                                face->uv_offset.y = -min_v / v_range;
                                face->uv_rotation = 0;
                            }
                            Brush_CreateRenderData(b);
                            Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Fit Texture to Face");
                        }
                    }
                }

                UI_Separator();
                UI_Text("Lighting");
                if (UI_DragFloat("Lightmap Scale", &primary_face->lightmap_scale, 0.125f, 0.125f, 16.0f)) {}
                if (UI_IsItemDeactivatedAfterEdit()) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                        scene->brushes[sel->index].faces[sel->face_index].lightmap_scale = primary_face->lightmap_scale;
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Edit Lightmap Scale");
                    }
                }

                UI_EndTabItem();
            }

            if (UI_BeginTabItem("Properties")) {
                UI_Text("Geometry Tools");
                if (UI_Button("Flip Face Normal")) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Brush* b = &scene->brushes[sel->index];
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                        BrushFace* face_to_flip = &b->faces[sel->face_index];
                        int num_indices = face_to_flip->numVertexIndices;
                        for (int k = 0; k < num_indices / 2; ++k) {
                            int temp = face_to_flip->vertexIndices[k];
                            face_to_flip->vertexIndices[k] = face_to_flip->vertexIndices[num_indices - 1 - k];
                            face_to_flip->vertexIndices[num_indices - 1 - k] = temp;
                        }
                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Flip Brush Face");
                    }
                }
                UI_SameLine();
                if (UI_Button("Delete Face")) {
                    for (int i = g_EditorState.num_selections - 1; i >= 0; --i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Brush* b = &scene->brushes[sel->index];
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                        free(b->faces[sel->face_index].vertexIndices);
                        for (int j = sel->face_index; j < b->numFaces - 1; ++j) {
                            b->faces[j] = b->faces[j + 1];
                        }
                        b->numFaces--;
                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Delete Face");
                    }
                    Editor_ClearSelection();
                }

                static int subdivide_u = 2, subdivide_v = 2;
                UI_DragInt("Subdivisions U", &subdivide_u, 1, 1, 16);
                UI_DragInt("Subdivisions V", &subdivide_v, 1, 1, 16);
                if (UI_Button("Subdivide Selected Faces")) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Editor_SubdivideBrushFace(scene, engine, sel->index, sel->face_index, subdivide_u, subdivide_v);
                    }
                    Editor_ClearSelection();
                }
                UI_Separator();
                UI_Text("Utility");
                if (UI_Button("Apply Nodraw")) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Brush* b = &scene->brushes[sel->index];
                        BrushFace* face = &b->faces[sel->face_index];
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                        face->material = &g_NodrawMaterial;
                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Apply Nodraw");
                    }
                }
                UI_SameLine();
                if (UI_Button("Copy Props")) {
                    memcpy(&g_copiedFaceProperties, primary_face, sizeof(BrushFace));
                    g_copiedFaceProperties.vertexIndices = NULL;
                    g_copiedFaceProperties.numVertexIndices = 0;
                    g_hasCopiedFace = true;
                }
                UI_SameLine();
                if (UI_Button("Paste Props") && g_hasCopiedFace) {
                    for (int i = 0; i < g_EditorState.num_selections; ++i) {
                        EditorSelection* sel = &g_EditorState.selections[i];
                        Brush* b = &scene->brushes[sel->index];
                        BrushFace* face = &b->faces[sel->face_index];
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, sel->index);
                        face->material = g_copiedFaceProperties.material;
                        face->material2 = g_copiedFaceProperties.material2;
                        face->material3 = g_copiedFaceProperties.material3;
                        face->material4 = g_copiedFaceProperties.material4;
                        face->uv_offset = g_copiedFaceProperties.uv_offset;
                        face->uv_scale = g_copiedFaceProperties.uv_scale;
                        face->uv_rotation = g_copiedFaceProperties.uv_rotation;
                        face->lightmap_scale = g_copiedFaceProperties.lightmap_scale;
                        Brush_CreateRenderData(b);
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, sel->index, "Paste Face Properties");
                    }
                }
                UI_EndTabItem();
            }
            UI_EndTabBar();
        }
    }
    UI_End();
}
static void Editor_UpdatePreviewBrushForArch() {
    Vec3 p1 = g_EditorState.arch_creation_start_point;
    Vec3 p2 = g_EditorState.arch_creation_end_point;
    ViewportType view = g_EditorState.arch_creation_view;

    float width = 0.0f, height = g_EditorState.arch_add_height;
    Vec3 center = { 0 };

    if (view == VIEW_TOP_XZ) {
        width = fabsf(p2.x - p1.x);
        center = (Vec3){ (p1.x + p2.x) / 2.0f, p1.y, (p1.z + p2.z) / 2.0f };
    }
    else if (view == VIEW_FRONT_XY) {
        width = fabsf(p2.x - p1.x);
        center = (Vec3){ (p1.x + p2.x) / 2.0f, (p1.y + p2.y) / 2.0f, p1.z };
    }
    else if (view == VIEW_SIDE_YZ) {
        width = fabsf(p2.z - p1.z);
        center = (Vec3){ p1.x, (p1.y + p2.y) / 2.0f, (p1.z + p2.z) / 2.0f };
    }

    float outer_radius = width / 2.0f;
    float inner_radius = outer_radius - g_EditorState.arch_wall_width;
    if (inner_radius < 0.01f) inner_radius = 0.01f;

    int num_sides = g_EditorState.arch_num_sides;
    float start_angle_rad = g_EditorState.arch_start_angle_degrees * (M_PI / 180.0f);
    float arc_rad = g_EditorState.arch_arc_degrees * (M_PI / 180.0f);
    float angle_step = arc_rad / num_sides;

    Brush* b = &g_EditorState.preview_brush;
    Brush_FreeData(b);

    int verts_per_ring = num_sides + 1;
    b->numVertices = verts_per_ring * 4;
    b->vertices = calloc(b->numVertices, sizeof(BrushVertex));

    for (int i = 0; i <= num_sides; i++) {
        float angle = start_angle_rad + i * angle_step;
        float cos_a = cosf(angle);
        float sin_a = sinf(angle);

        int outer_bottom_idx = i;
        int inner_bottom_idx = i + verts_per_ring;
        int outer_top_idx = i + verts_per_ring * 2;
        int inner_top_idx = i + verts_per_ring * 3;

        b->vertices[outer_bottom_idx].pos = (Vec3){ cos_a * outer_radius, 0, sin_a * outer_radius };
        b->vertices[inner_bottom_idx].pos = (Vec3){ cos_a * inner_radius, 0, sin_a * inner_radius };
        b->vertices[outer_top_idx].pos = (Vec3){ cos_a * outer_radius, height, sin_a * outer_radius };
        b->vertices[inner_top_idx].pos = (Vec3){ cos_a * inner_radius, height, sin_a * inner_radius };
    }

    b->numFaces = (num_sides * 4) + 2;
    b->faces = calloc(b->numFaces, sizeof(BrushFace));

    for (int i = 0; i < num_sides; i++) {
        int ob = i;
        int ib = i + verts_per_ring;
        int ot = i + verts_per_ring * 2;
        int it = i + verts_per_ring * 3;

        b->faces[i].vertexIndices = malloc(4 * sizeof(int));
        b->faces[i].vertexIndices[0] = ob; b->faces[i].vertexIndices[1] = ot; b->faces[i].vertexIndices[2] = ot + 1; b->faces[i].vertexIndices[3] = ob + 1;

        b->faces[num_sides + i].vertexIndices = malloc(4 * sizeof(int));
        b->faces[num_sides + i].vertexIndices[0] = ib + 1; b->faces[num_sides + i].vertexIndices[1] = it + 1; b->faces[num_sides + i].vertexIndices[2] = it; b->faces[num_sides + i].vertexIndices[3] = ib;

        b->faces[num_sides * 2 + i].vertexIndices = malloc(4 * sizeof(int));
        b->faces[num_sides * 2 + i].vertexIndices[0] = ot; b->faces[num_sides * 2 + i].vertexIndices[1] = it; b->faces[num_sides * 2 + i].vertexIndices[2] = it + 1; b->faces[num_sides * 2 + i].vertexIndices[3] = ot + 1;

        b->faces[num_sides * 3 + i].vertexIndices = malloc(4 * sizeof(int));
        b->faces[num_sides * 3 + i].vertexIndices[0] = ob + 1; b->faces[num_sides * 3 + i].vertexIndices[1] = ib + 1; b->faces[num_sides * 3 + i].vertexIndices[2] = ib; b->faces[num_sides * 3 + i].vertexIndices[3] = ob;

        for (int j = 0; j < 4; ++j) b->faces[num_sides * j + i].numVertexIndices = 4;
    }

    b->faces[num_sides * 4].vertexIndices = malloc(4 * sizeof(int));
    b->faces[num_sides * 4].vertexIndices[0] = 0; b->faces[num_sides * 4].vertexIndices[1] = verts_per_ring; b->faces[num_sides * 4].vertexIndices[2] = verts_per_ring * 3; b->faces[num_sides * 4].vertexIndices[3] = verts_per_ring * 2;

    b->faces[num_sides * 4 + 1].vertexIndices = malloc(4 * sizeof(int));
    b->faces[num_sides * 4 + 1].vertexIndices[0] = num_sides; b->faces[num_sides * 4 + 1].vertexIndices[1] = num_sides + verts_per_ring * 2; b->faces[num_sides * 4 + 1].vertexIndices[2] = num_sides + verts_per_ring * 3; b->faces[num_sides * 4 + 1].vertexIndices[3] = num_sides + verts_per_ring;

    b->faces[num_sides * 4].numVertexIndices = 4;
    b->faces[num_sides * 4 + 1].numVertexIndices = 4;

    for (int i = 0; i < b->numFaces; i++) {
        b->faces[i].material = TextureManager_GetMaterial(0);
        b->faces[i].uv_scale = (Vec2){ 1,1 };
        b->faces[i].lightmap_scale = 1.0f;
    }

    b->pos = center;
    b->rot = (Vec3){ 0,0,0 };
    b->scale = (Vec3){ 1,1,1 };
    Brush_UpdateMatrix(b);
    Brush_CreateRenderData(b);
}

static void Editor_RenderArchPreview() {
    glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.arch_preview_fbo);
    glViewport(0, 0, g_EditorState.arch_preview_width, g_EditorState.arch_preview_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_EditorState.debug_shader);
    Mat4 projection = mat4_ortho(0, g_EditorState.arch_preview_width, 0, g_EditorState.arch_preview_height, -1, 1);
    Mat4 view; mat4_identity(&view);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, projection.m);
    Mat4 model; mat4_identity(&model);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, model.m);

    float world_width = 0.0f;
    if (g_EditorState.arch_creation_view == VIEW_TOP_XZ || g_EditorState.arch_creation_view == VIEW_FRONT_XY) {
        world_width = fabsf(g_EditorState.arch_creation_end_point.x - g_EditorState.arch_creation_start_point.x);
    }
    else if (g_EditorState.arch_creation_view == VIEW_SIDE_YZ) {
        world_width = fabsf(g_EditorState.arch_creation_end_point.z - g_EditorState.arch_creation_start_point.z);
    }
    float world_outer_radius = world_width / 2.0f;

    float color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);

    float center_x = g_EditorState.arch_preview_width / 2.0f;
    float center_y = 20.0f;
    float outer_radius = fminf(g_EditorState.arch_preview_width, g_EditorState.arch_preview_height) * 0.4f;
    float inner_radius = outer_radius;
    if (world_outer_radius > 0.01f) {
        float thickness_ratio = g_EditorState.arch_wall_width / world_outer_radius;
        inner_radius = outer_radius * (1.0f - thickness_ratio);
    }
    if (inner_radius < 0) inner_radius = 0;

    int num_sides = g_EditorState.arch_num_sides;
    float start_angle = g_EditorState.arch_start_angle_degrees * (M_PI / 180.0f);
    float arc = g_EditorState.arch_arc_degrees * (M_PI / 180.0f);
    float angle_step = arc / num_sides;

    Vec3* lines = malloc((num_sides * 4 + 4) * sizeof(Vec3));
    int line_idx = 0;

    for (int i = 0; i <= num_sides; ++i) {
        float angle = start_angle + i * angle_step;
        if (i > 0) {
            float prev_angle = start_angle + (i - 1) * angle_step;
            lines[line_idx++] = (Vec3){ center_x + cosf(prev_angle) * outer_radius, center_y + sinf(prev_angle) * outer_radius, 0 };
            lines[line_idx++] = (Vec3){ center_x + cosf(angle) * outer_radius, center_y + sinf(angle) * outer_radius, 0 };
            lines[line_idx++] = (Vec3){ center_x + cosf(prev_angle) * inner_radius, center_y + sinf(prev_angle) * inner_radius, 0 };
            lines[line_idx++] = (Vec3){ center_x + cosf(angle) * inner_radius, center_y + sinf(angle) * inner_radius, 0 };
        }
    }

    float start_cap_angle = start_angle;
    lines[line_idx++] = (Vec3){ center_x + cosf(start_cap_angle) * outer_radius, center_y + sinf(start_cap_angle) * outer_radius, 0 };
    lines[line_idx++] = (Vec3){ center_x + cosf(start_cap_angle) * inner_radius, center_y + sinf(start_cap_angle) * inner_radius, 0 };

    float end_cap_angle = start_angle + arc;
    lines[line_idx++] = (Vec3){ center_x + cosf(end_cap_angle) * outer_radius, center_y + sinf(end_cap_angle) * outer_radius, 0 };
    lines[line_idx++] = (Vec3){ center_x + cosf(end_cap_angle) * inner_radius, center_y + sinf(end_cap_angle) * inner_radius, 0 };

    glBindVertexArray(g_EditorState.vertex_points_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_EditorState.vertex_points_vbo);
    glBufferData(GL_ARRAY_BUFFER, line_idx * sizeof(Vec3), lines, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, line_idx);
    glBindVertexArray(0);

    free(lines);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void Editor_RenderArchPropertiesWindow(Scene* scene, Engine* engine) {
    if (!g_EditorState.show_arch_properties_popup) return;

    g_EditorState.is_in_brush_creation_mode = true;

    UI_SetNextWindowSize(370, 330);
    UI_Begin("Arch Properties", &g_EditorState.show_arch_properties_popup);

    Editor_UpdatePreviewBrushForArch();

    bool values_changed = false;
    values_changed |= UI_DragFloat("Wall width", &g_EditorState.arch_wall_width, 0.1f, 0.01f, 1024.0f);
    values_changed |= UI_DragInt("Number of Sides", &g_EditorState.arch_num_sides, 1, 3, 64);
    if (UI_Button("Circle")) { g_EditorState.arch_arc_degrees = 360.0f; values_changed = true; } UI_SameLine();
    values_changed |= UI_DragFloat("Arc", &g_EditorState.arch_arc_degrees, 1.0f, 1.0f, 360.0f);
    values_changed |= UI_DragFloat("Start Angle", &g_EditorState.arch_start_angle_degrees, 1.0f, -360.0f, 360.0f);
    values_changed |= UI_DragFloat("Add Height", &g_EditorState.arch_add_height, 1.0f, 0.0f, 4096.0f);

    if (values_changed) {
        Editor_UpdatePreviewBrushForArch();
    }

    Editor_RenderArchPreview();
    UI_Image((void*)(intptr_t)g_EditorState.arch_preview_texture, g_EditorState.arch_preview_width, g_EditorState.arch_preview_height);

    if (UI_Button("OK")) {
        Editor_CreateBrushFromPreview(scene, engine, &g_EditorState.preview_brush);
        g_EditorState.is_in_brush_creation_mode = false;
        g_EditorState.show_arch_properties_popup = false;
    }
    UI_SameLine();
    if (UI_Button("Cancel")) {
        Brush_FreeData(&g_EditorState.preview_brush);
        g_EditorState.is_in_brush_creation_mode = false;
        g_EditorState.show_arch_properties_popup = false;
    }

    if (!g_EditorState.show_arch_properties_popup) {
        Brush_FreeData(&g_EditorState.preview_brush);
        g_EditorState.is_in_brush_creation_mode = false;
    }

    UI_End();
}
void Editor_RenderUI(Engine* engine, Scene* scene, Renderer* renderer) {
    char window_title[512];
    sprintf(window_title, "Tectonic Editor - %s", g_EditorState.currentMapPath);
    SDL_SetWindowTitle(engine->window, window_title);
    static bool show_add_particle_popup = false;
    static char add_particle_path[128] = "particles/fire.par";
    int model_to_delete = -1, brush_to_delete = -1, light_to_delete = -1, decal_to_delete = -1, sound_to_delete = -1, particle_to_delete = -1, video_player_to_delete = -1, parallax_room_to_delete = -1;
    int sprite_to_delete = -1;
    int logic_entity_to_delete = -1;
    float right_panel_width = 300.0f; float screen_w, screen_h;
    UI_GetDisplaySize(&screen_w, &screen_h);
    UI_SetNextWindowPos(screen_w - right_panel_width, 22); UI_SetNextWindowSize(right_panel_width, screen_h * 0.5f);
    UI_Begin("Hierarchy", NULL);
    if (UI_Selectable("Player Start", Editor_IsSelected(ENTITY_PLAYERSTART, 0))) { Editor_ClearSelection(); Editor_AddToSelection(ENTITY_PLAYERSTART, 0, -1, -1); }
    if (UI_CollapsingHeader("Models", 1)) {
        for (int i = 0; i < scene->numObjects; ++i) {
            char label[128];
            const char* name = (strlen(scene->objects[i].targetname) > 0) ? scene->objects[i].targetname : scene->objects[i].modelPath;
            sprintf(label, "%s##%d", name, i);
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_MODEL, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_MODEL, i, -1, -1); }  char popup_id[64];
            sprintf(popup_id, "ModelContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateModel(scene, engine, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { model_to_delete = i; }
                UI_EndPopup();
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##model%d", i); if (UI_Button(del_label)) { model_to_delete = i; }
        }
        if (UI_Button("Add Model")) { g_EditorState.show_add_model_popup = true; ScanModelFiles(); }
    }
    if (model_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_MODEL, model_to_delete, "Delete Model"); _raw_delete_model(scene, model_to_delete, engine); Editor_RemoveFromSelection(ENTITY_MODEL, model_to_delete); }
    if (UI_CollapsingHeader("Brushes", 1)) {
        for (int i = 0; i < scene->numBrushes; ++i) {
            if (scene->brushes[i].isReflectionProbe || scene->brushes[i].isGlass || scene->brushes[i].isDSP || scene->brushes[i].isWater) continue;
            char label[128];
            if (strlen(scene->brushes[i].targetname) > 0) {
                sprintf(label, "%s %s##%d", scene->brushes[i].targetname, scene->brushes[i].isTrigger ? "[T]" : "", i);
            }
            else {
                sprintf(label, "Brush %d %s##%d", i, scene->brushes[i].isTrigger ? "[T]" : "", i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_BRUSH, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_BRUSH, i, 0, 0); }
            char popup_id[64];
            sprintf(popup_id, "BrushContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateBrush(scene, engine, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { brush_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##brush%d", i); if (UI_Button(del_label)) { brush_to_delete = i; }
        }
    }
    if (UI_CollapsingHeader("DSP Zones", 1)) {
        for (int i = 0; i < scene->numBrushes; ++i) {
            if (!scene->brushes[i].isDSP) continue;
            char label[128];
            sprintf(label, "DSP Zone %d", i);
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_BRUSH, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_BRUSH, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "DSPContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateBrush(scene, engine, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { brush_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##dspbrush%d", i);
            if (UI_Button(del_label)) { brush_to_delete = i; }
        }
    }
    if (UI_CollapsingHeader("Glass", 1)) {
        for (int i = 0; i < scene->numBrushes; ++i) {
            if (!scene->brushes[i].isGlass) continue;
            char label[128];
            if (strlen(scene->brushes[i].targetname) > 0) {
                sprintf(label, "%s##glass%d", scene->brushes[i].targetname, i);
            }
            else {
                sprintf(label, "Glass Brush %d##glass%d", i, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_BRUSH, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_BRUSH, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "GlassContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateBrush(scene, engine, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { brush_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##glassbrush%d", i);
            if (UI_Button(del_label)) { brush_to_delete = i; }
        }
    }
    if (UI_CollapsingHeader("Water", 1)) {
        for (int i = 0; i < scene->numBrushes; ++i) {
            if (!scene->brushes[i].isWater) continue;
            char label[128]; sprintf(label, "Water Brush %d", i);
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_BRUSH, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_BRUSH, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "WaterContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateBrush(scene, engine, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { brush_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##waterbrush%d", i); if (UI_Button(del_label)) { brush_to_delete = i; }
        }
    }
    if (brush_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_BRUSH, brush_to_delete, "Delete Brush"); _raw_delete_brush(scene, engine, brush_to_delete); Editor_RemoveFromSelection(ENTITY_BRUSH, brush_to_delete); }
    if (UI_CollapsingHeader("Lights", 1)) {
        for (int i = 0; i < scene->numActiveLights; ++i) {
            char label[128];
            if (strlen(scene->lights[i].targetname) > 0) {
                sprintf(label, "%s##%d", scene->lights[i].targetname, i);
            }
            else {
                sprintf(label, "Light %d##%d", i, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_LIGHT, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_LIGHT, i, -1, -1); } char popup_id[64];
            sprintf(popup_id, "LightContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateLight(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { light_to_delete = i; }
                UI_EndPopup();
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##light%d", i); if (UI_Button(del_label)) { light_to_delete = i; }
        }
        if (UI_Button("Add Light")) { if (scene->numActiveLights < MAX_LIGHTS) { Light* new_light = &scene->lights[scene->numActiveLights]; scene->numActiveLights++; memset(new_light, 0, sizeof(Light)); sprintf(new_light->targetname, "Light_%d", scene->numActiveLights - 1); new_light->type = LIGHT_POINT; new_light->position = g_EditorState.editor_camera.position; new_light->color = (Vec3){ 1,1,1 }; new_light->intensity = 1.0f; new_light->direction = (Vec3){ 0, -1, 0 }; new_light->shadowFarPlane = 25.0f; new_light->shadowBias = 0.05f; new_light->intensity = 1.0f; new_light->radius = 10.0f; new_light->base_intensity = 1.0f; new_light->is_on = true; Light_InitShadowMap(new_light); Undo_PushCreateEntity(scene, ENTITY_LIGHT, scene->numActiveLights - 1, "Create Light"); } }
    }
    if (light_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_LIGHT, light_to_delete, "Delete Light"); _raw_delete_light(scene, light_to_delete); Editor_RemoveFromSelection(ENTITY_LIGHT, light_to_delete); }
    if (UI_CollapsingHeader("Reflection Probes", 1)) {
        for (int i = 0; i < scene->numBrushes; ++i) {
            if (!scene->brushes[i].isReflectionProbe) continue;
            char label[128];
            sprintf(label, "%s##probebrush%d", scene->brushes[i].targetname, i);
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_BRUSH, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_BRUSH, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "ProbeContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateBrush(scene, engine, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { brush_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##reflprobe%d", i);
            if (UI_Button(del_label)) { brush_to_delete = i; }
        }
    }
    if (UI_CollapsingHeader("Decals", 1)) {
        for (int i = 0; i < scene->numDecals; ++i) {
            char label[128];
            if (strlen(scene->decals[i].targetname) > 0) {
                sprintf(label, "%s##decal%d", scene->decals[i].targetname, i);
            }
            else {
                sprintf(label, "Decal %d (%s)##decal%d", i, scene->decals[i].material->name, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_DECAL, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_DECAL, i, -1, -1); }  char popup_id[64];
            sprintf(popup_id, "DecalContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateDecal(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { decal_to_delete = i; }
                UI_EndPopup();
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##decal%d", i); if (UI_Button(del_label)) { decal_to_delete = i; }
        }
        if (UI_Button("Add Decal")) { if (scene->numDecals < MAX_DECALS) { Decal* d = &scene->decals[scene->numDecals]; memset(d, 0, sizeof(Decal)); sprintf(d->targetname, "Decal_%d", scene->numDecals); d->pos = g_EditorState.editor_camera.position; d->size = (Vec3){ 1, 1, 1 }; d->material = TextureManager_FindMaterial(TextureManager_GetMaterial(0)->name); Decal_UpdateMatrix(d); scene->numDecals++; Undo_PushCreateEntity(scene, ENTITY_DECAL, scene->numDecals - 1, "Create Decal"); } }
    }
    if (decal_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_DECAL, decal_to_delete, "Delete Decal"); _raw_delete_decal(scene, decal_to_delete); Editor_RemoveFromSelection(ENTITY_DECAL, decal_to_delete); }
    if (UI_CollapsingHeader("Sounds", 1)) {
        for (int i = 0; i < scene->numSoundEntities; ++i) {
            char label[128];
            if (strlen(scene->soundEntities[i].targetname) > 0) {
                sprintf(label, "%s##sound%d", scene->soundEntities[i].targetname, i);
            }
            else {
                sprintf(label, "Sound %d##sound%d", i, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_SOUND, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_SOUND, i, -1, -1); }  char popup_id[64];
            sprintf(popup_id, "SoundContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateSoundEntity(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { sound_to_delete = i; }
                UI_EndPopup();
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##sound%d", i); if (UI_Button(del_label)) { sound_to_delete = i; }
        }
        if (UI_Button("Add Sound Entity")) {
            g_EditorState.show_sound_browser_popup = true;
            ScanSoundFiles();
        }
    }
    if (sound_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_SOUND, sound_to_delete, "Delete Sound"); _raw_delete_sound_entity(scene, sound_to_delete); Editor_RemoveFromSelection(ENTITY_SOUND, sound_to_delete); }
    if (UI_CollapsingHeader("Particle Emitters", 1)) {
        for (int i = 0; i < scene->numParticleEmitters; ++i) {
            char label[128];
            if (strlen(scene->particleEmitters[i].targetname) > 0) {
                sprintf(label, "%s##particle%d", scene->particleEmitters[i].targetname, i);
            }
            else {
                sprintf(label, "%s##particle%d", scene->particleEmitters[i].parFile, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_PARTICLE_EMITTER, i))) { if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection(); Editor_AddToSelection(ENTITY_PARTICLE_EMITTER, i, -1, -1); }   char popup_id[64];
            sprintf(popup_id, "ParticleContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateParticleEmitter(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { particle_to_delete = i; }
                UI_EndPopup();
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##particle%d", i); if (UI_Button(del_label)) { particle_to_delete = i; }
        }
        if (UI_Button("Add Emitter")) { show_add_particle_popup = true; }
    }
    if (particle_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_PARTICLE_EMITTER, particle_to_delete, "Delete Emitter"); _raw_delete_particle_emitter(scene, particle_to_delete); Editor_RemoveFromSelection(ENTITY_PARTICLE_EMITTER, particle_to_delete); }
    if (UI_CollapsingHeader("Sprites", 1)) {
        for (int i = 0; i < scene->numSprites; ++i) {
            char label[128];
            sprintf(label, "%s##sprite%d", scene->sprites[i].targetname, i);
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_SPRITE, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_SPRITE, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "SpriteContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateSprite(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { sprite_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##sprite%d", i);
            if (UI_Button(del_label)) { sprite_to_delete = i; }
        }
        if (UI_Button("Add Sprite")) {
            if (scene->numSprites < MAX_SPRITES) {
                Sprite* s = &scene->sprites[scene->numSprites];
                memset(s, 0, sizeof(Sprite));
                sprintf(s->targetname, "Sprite_%d", scene->numSprites);
                s->pos = g_EditorState.editor_camera.position;
                s->scale = 1.0f;
                s->material = &g_MissingMaterial;
                s->visible = true;
                scene->numSprites++;
                Undo_PushCreateEntity(scene, ENTITY_SPRITE, scene->numSprites - 1, "Create Sprite");
            }
        }
    }
    if (sprite_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_SPRITE, sprite_to_delete, "Delete Sprite"); _raw_delete_sprite(scene, sprite_to_delete); Editor_RemoveFromSelection(ENTITY_SPRITE, sprite_to_delete); }
    if (UI_CollapsingHeader("Video Players", 1)) {
        for (int i = 0; i < scene->numVideoPlayers; ++i) {
            char label[128];
            if (strlen(scene->videoPlayers[i].targetname) > 0) {
                sprintf(label, "%s##vidplayer%d", scene->videoPlayers[i].targetname, i);
            }
            else {
                sprintf(label, "%s##vidplayer%d", scene->videoPlayers[i].videoPath, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_VIDEO_PLAYER, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_VIDEO_PLAYER, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "VideoContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateVideoPlayer(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { video_player_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##vidplayer%d", i);
            if (UI_Button(del_label)) { video_player_to_delete = i; }
        }
        if (UI_Button("Add Video Player")) {
            if (scene->numVideoPlayers < MAX_VIDEO_PLAYERS) {
                VideoPlayer* vp = &scene->videoPlayers[scene->numVideoPlayers];
                memset(vp, 0, sizeof(VideoPlayer));
                sprintf(vp->targetname, "Video_%d", scene->numVideoPlayers);
                vp->pos = g_EditorState.editor_camera.position;
                vp->size = (Vec2){ 2, 2 };
                scene->numVideoPlayers++;
                Undo_PushCreateEntity(scene, ENTITY_VIDEO_PLAYER, scene->numVideoPlayers - 1, "Create Video Player");
            }
        }
    }
    if (video_player_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_VIDEO_PLAYER, video_player_to_delete, "Delete Video Player"); _raw_delete_video_player(scene, video_player_to_delete); Editor_RemoveFromSelection(ENTITY_VIDEO_PLAYER, video_player_to_delete); }
    if (UI_CollapsingHeader("Parallax Rooms", 1)) {
        for (int i = 0; i < scene->numParallaxRooms; ++i) {
            char label[128];
            if (strlen(scene->parallaxRooms[i].targetname) > 0) {
                sprintf(label, "%s##parallax%d", scene->parallaxRooms[i].targetname, i);
            }
            else {
                sprintf(label, "%s##parallax%d", scene->parallaxRooms[i].cubemapPath, i);
            }
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_PARALLAX_ROOM, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_PARALLAX_ROOM, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "ParallaxContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateParallaxRoom(scene, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { parallax_room_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##parallax%d", i);
            if (UI_Button(del_label)) { parallax_room_to_delete = i; }
        }
        if (UI_Button("Add Parallax Room")) {
            if (scene->numParallaxRooms < MAX_PARALLAX_ROOMS) {
                ParallaxRoom* p = &scene->parallaxRooms[scene->numParallaxRooms];
                memset(p, 0, sizeof(ParallaxRoom));
                sprintf(p->targetname, "Parallax_%d", scene->numParallaxRooms);
                p->pos = g_EditorState.editor_camera.position;
                p->size = (Vec2){ 2, 2 };
                p->roomDepth = 2.0f;
                strcpy(p->cubemapPath, "cubemaps/");
                scene->numParallaxRooms++;
                Undo_PushCreateEntity(scene, ENTITY_PARALLAX_ROOM, scene->numParallaxRooms - 1, "Create Parallax Room");
            }
        }
    }
    if (parallax_room_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_PARALLAX_ROOM, parallax_room_to_delete, "Delete Parallax Room"); _raw_delete_parallax_room(scene, parallax_room_to_delete); Editor_RemoveFromSelection(ENTITY_PARALLAX_ROOM, parallax_room_to_delete); }
    if (UI_CollapsingHeader("Logic Entities", 1)) {
        for (int i = 0; i < scene->numLogicEntities; ++i) {
            char label[128];
            sprintf(label, "%s (%s)##logic%d", scene->logicEntities[i].targetname, scene->logicEntities[i].classname, i);
            if (UI_Selectable(label, Editor_IsSelected(ENTITY_LOGIC, i))) {
                if (!(SDL_GetModState() & KMOD_CTRL)) Editor_ClearSelection();
                Editor_AddToSelection(ENTITY_LOGIC, i, -1, -1);
            }
            char popup_id[64];
            sprintf(popup_id, "LogicContext_%d", i);
            if (UI_BeginPopupContextItem(popup_id)) {
                if (UI_MenuItem("Duplicate", NULL, false, true)) { Editor_DuplicateLogicEntity(scene, engine, i); }
                if (UI_MenuItem("Delete", NULL, false, true)) { logic_entity_to_delete = i; }
                UI_EndPopup();
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##logic%d", i);
            if (UI_Button(del_label)) { logic_entity_to_delete = i; }
        }
        if (UI_Button("Add Logic Entity")) {
            if (scene->numLogicEntities < MAX_LOGIC_ENTITIES) {
                LogicEntity* ent = &scene->logicEntities[scene->numLogicEntities];
                memset(ent, 0, sizeof(LogicEntity));
                strcpy(ent->classname, "logic_timer");
                sprintf(ent->targetname, "timer_%d", scene->numLogicEntities);
                ent->pos = g_EditorState.editor_camera.position;
                Editor_SetDefaultLogicProperties(ent);
                ent->numProperties = 1;
                strcpy(ent->properties[0].key, "delay");
                strcpy(ent->properties[0].value, "1.0");
                scene->numLogicEntities++;
                Undo_PushCreateEntity(scene, ENTITY_LOGIC, scene->numLogicEntities - 1, "Create Logic Entity");
            }
        }
    }
    if (logic_entity_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_LOGIC, logic_entity_to_delete, "Delete Logic Entity"); _raw_delete_logic_entity(scene, logic_entity_to_delete); Editor_RemoveFromSelection(ENTITY_LOGIC, logic_entity_to_delete); }
    if (show_add_particle_popup) { UI_Begin("Add Particle Emitter", &show_add_particle_popup); UI_InputText("Path (.par)", add_particle_path, sizeof(add_particle_path)); if (UI_Button("Create")) { if (scene->numParticleEmitters < MAX_PARTICLE_EMITTERS) { ParticleEmitter* emitter = &scene->particleEmitters[scene->numParticleEmitters]; strcpy(emitter->parFile, add_particle_path); sprintf(emitter->targetname, "Emitter_%d", scene->numParticleEmitters); ParticleSystem* ps = ParticleSystem_Load(emitter->parFile); if (ps) { ParticleEmitter_Init(emitter, ps, g_EditorState.editor_camera.position); scene->numParticleEmitters++; Undo_PushCreateEntity(scene, ENTITY_PARTICLE_EMITTER, scene->numParticleEmitters - 1, "Create Particle Emitter"); } else { Console_Printf_Error("[error] Failed to load particle system: %s", emitter->parFile); } } show_add_particle_popup = false; } UI_End(); }
    UI_End();
    UI_SetNextWindowPos(screen_w - right_panel_width, 22 + screen_h * 0.5f); UI_SetNextWindowSize(right_panel_width, screen_h * 0.5f);
    UI_Begin("Inspector & Settings", NULL);
    EditorSelection* primary = Editor_GetPrimarySelection();
    UI_RadioButton_Int("Translate (1)", (int*)&g_EditorState.current_gizmo_operation, GIZMO_OP_TRANSLATE);
    UI_SameLine();
    UI_RadioButton_Int("Rotate (2)", (int*)&g_EditorState.current_gizmo_operation, GIZMO_OP_ROTATE);
    UI_SameLine();
    UI_RadioButton_Int("Scale (3)", (int*)&g_EditorState.current_gizmo_operation, GIZMO_OP_SCALE);
    UI_Separator();
    UI_Text("Inspector"); UI_Separator();
    if (primary && primary->type == ENTITY_MODEL) {
        SceneObject* obj = &scene->objects[primary->index]; UI_Text(obj->modelPath); UI_Separator();
        UI_InputText("Name", obj->targetname, sizeof(obj->targetname));
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Edit Model Targetname"); }
        if (UI_DragFloat3("Position", &obj->pos.x, 0.1f, 0, 0)) { SceneObject_UpdateMatrix(obj); if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Move Model"); }

        if (UI_DragFloat3("Rotation", &obj->rot.x, 1.0f, 0, 0)) { SceneObject_UpdateMatrix(obj); if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Rotate Model"); }

        if (UI_DragFloat3("Scale", &obj->scale.x, 0.01f, 0, 0)) { SceneObject_UpdateMatrix(obj); if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Scale Model"); }
        UI_Separator();
        UI_Text("Physics Properties");
        UI_DragFloat("Mass", &obj->mass, 0.1f, 0.0f, 1000.0f);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Edit Model Mass"); }
        UI_Text("(Mass 0 = static, >0 = dynamic)");

        if (UI_Checkbox("Physics Enabled", &obj->isPhysicsEnabled)) {
            Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index);
            Physics_ToggleCollision(engine->physicsWorld, obj->physicsBody, obj->isPhysicsEnabled);
            Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Toggle Model Physics");
        }
        UI_Separator();
        UI_Checkbox("Enable Tree Sway", &obj->swayEnabled);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Toggle Model Sway"); }
        UI_Separator();
        UI_Text("Fading");
        UI_DragFloat("Fade Start", &obj->fadeStartDist, 1.0f, 0.0f, 1000.0f);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Edit Fade Distance"); }

        UI_DragFloat("Fade End", &obj->fadeEndDist, 1.0f, 0.0f, 1000.0f);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Edit Fade Distance"); }
    }
    else if (primary && primary->type == ENTITY_BRUSH) {
        Brush* b = &scene->brushes[primary->index];
        if (UI_Checkbox("Is Water", &b->isWater)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            if (b->isWater) {
                b->isTrigger = false;
                b->isReflectionProbe = false;
                b->isDSP = false;
                b->isGlass = false;
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Toggle Brush Water");
        }
        if (UI_Checkbox("Is Reflection Probe", &b->isReflectionProbe)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            if (b->isReflectionProbe) {
                b->isTrigger = false;
                b->isWater = false;
                b->isDSP = false;
                b->isGlass = false;
                int px = (int)roundf(b->pos.x);
                int py = (int)roundf(b->pos.y);
                int pz = (int)roundf(b->pos.z);
                sprintf(b->name, "Probe_%d_%d_%d", px, py, pz);
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Toggle Brush Reflection Probe");
        }
        if (UI_Checkbox("Is Trigger", &b->isTrigger)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            if (b->isTrigger) {
                b->isReflectionProbe = false;
                b->isWater = false;
                b->isDSP = false;
                b->isGlass = false;
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Toggle Brush Trigger");
        }
        if (UI_Checkbox("Is DSP Zone", &b->isDSP)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            if (b->isDSP) {
                b->isTrigger = false;
                b->isReflectionProbe = false;
                b->isWater = false;
                b->isGlass = false;
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Toggle Brush DSP Zone");
        }
        if (b->isWater) {
            UI_Separator();
            UI_Text("Water Definition");
            int current_item = -1;
            if (b->waterDef) {
                for (int i = 0; i < WaterManager_GetWaterDefCount(); ++i) {
                    if (b->waterDef == WaterManager_GetWaterDef(i)) {
                        current_item = i;
                        break;
                    }
                }
            }

            int waterDefCount = WaterManager_GetWaterDefCount();
            const char** items = (const char**)malloc(waterDefCount * sizeof(const char*));
            for (int i = 0; i < waterDefCount; i++) {
                items[i] = WaterManager_GetWaterDef(i)->name;
            }

            if (UI_Combo("Type", &current_item, items, waterDefCount, -1)) {
                if (current_item >= 0 && current_item < waterDefCount) {
                    b->waterDef = WaterManager_GetWaterDef(current_item);
                }
            }

            free(items);
        }
        if (UI_Checkbox("Is Glass", &b->isGlass)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            if (b->isGlass) {
                b->isTrigger = false;
                b->isReflectionProbe = false;
                b->isWater = false;
                b->isDSP = false;
                b->refractionStrength = 0.01f;
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Toggle Brush Glass");
        }
        UI_Separator();
        UI_InputText("Name", b->targetname, sizeof(b->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Edit Brush Name"); }
        UI_Separator(); bool transform_changed = false;
        UI_DragFloat3("Position", &b->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->pos.x = SnapValue(b->pos.x, g_EditorState.grid_size); b->pos.y = SnapValue(b->pos.y, g_EditorState.grid_size); b->pos.z = SnapValue(b->pos.z, g_EditorState.grid_size); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Move Brush"); }
        UI_DragFloat3("Rotation", &b->rot.x, 1.0f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->rot.x = SnapAngle(b->rot.x, 15.0f); b->rot.y = SnapAngle(b->rot.y, 15.0f); b->rot.z = SnapAngle(b->rot.z, 15.0f); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Rotate Brush"); }
        UI_DragFloat3("Scale", &b->scale.x, 0.01f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->scale.x = SnapValue(b->scale.x, 0.25f); b->scale.y = SnapValue(b->scale.y, 0.25f); b->scale.z = SnapValue(b->scale.z, 0.25f); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Scale Brush"); }
        if (transform_changed) { Brush_UpdateMatrix(b); if (b->physicsBody) { Physics_SetWorldTransform(b->physicsBody, b->modelMatrix); } }
        UI_Separator();
        UI_Text("Physics Properties");
        if (UI_DragFloat("Mass", &b->mass, 0.1f, 0.0f, 10000.0f)) {}
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) {
            if (b->physicsBody) {
                Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                b->physicsBody = NULL;
            }
            if (!b->isTrigger && !b->isWater && b->numVertices > 0) {
                if (b->mass > 0.0f) {
                    b->physicsBody = Physics_CreateDynamicBrush(engine->physicsWorld, (const float*)b->vertices, b->numVertices, b->mass, b->modelMatrix);
                }
                else {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int i = 0; i < b->numVertices; i++) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Edit Brush Mass");
        }
        UI_Separator();
        UI_Text("Vertex Tools");
        if (UI_Checkbox("Sculpt Mode", &g_EditorState.is_sculpting_mode_enabled)) {
            if (g_EditorState.is_sculpting_mode_enabled) {
                g_EditorState.is_painting_mode_enabled = false;
                g_EditorState.show_vertex_tools_window = true;
            }
            else {
                g_EditorState.show_vertex_tools_window = false;
            }
        }
        UI_SameLine();
        if (UI_Checkbox("Paint Mode", &g_EditorState.is_painting_mode_enabled)) {
            if (g_EditorState.is_painting_mode_enabled) {
                g_EditorState.is_sculpting_mode_enabled = false;
                g_EditorState.show_vertex_tools_window = true;
            }
            else {
                g_EditorState.show_vertex_tools_window = false;
            }
        }
        UI_Separator();
        if (b->isReflectionProbe) {
            UI_Text("Probe Name: %s", b->targetname);
        }
        else if (b->isTrigger) {
            const char* brush_outputs[] = { "OnTouch", "OnEndTouch", "OnUse" };
            RenderIOEditor(ENTITY_BRUSH, primary->index, brush_outputs, 3);
        }
        else if (b->isDSP) {
            UI_Separator();
            UI_Text("DSP Zone Settings");
            const char* reverb_names[] = { "None", "Small Room", "Medium Room", "Large Room", "Hall", "Cave" };
            int current_preset = (int)b->reverbPreset;
            if (UI_Combo("Reverb Preset", &current_preset, reverb_names, REVERB_PRESET_COUNT, REVERB_PRESET_COUNT)) {
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                b->reverbPreset = (ReverbPreset)current_preset;
                Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Set Reverb Preset");
            }
        }
        else if (b->isGlass) {
            UI_Separator();
            UI_Text("Glass Settings");
            UI_DragFloat("Refraction Strength", &b->refractionStrength, 0.001f, 0.0f, 0.1f);
            char mat_label[128];
            sprintf(mat_label, "Normal Map: %s", b->glassNormalMap ? b->glassNormalMap->name : "None");
            if (UI_Button(mat_label)) {
                g_EditorState.texture_browser_target = 5;
                g_EditorState.show_texture_browser = true;
            }
            if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); }
            if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Edit Glass Strength"); }
        }
        else {
            UI_Separator();
            UI_Text("Vertex Properties"); UI_DragInt("Selected Vertex", &primary->vertex_index, 1, 0, b->numVertices - 1);
            if (primary->vertex_index >= 0 && primary->vertex_index < b->numVertices) {
                BrushVertex* vert = &b->vertices[primary->vertex_index];

                UI_DragFloat3("Local Position", &vert->pos.x, 0.1f, 0, 0);
                if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); }
                if (UI_IsItemDeactivatedAfterEdit()) {
                    Brush_CreateRenderData(b);
                    if (b->physicsBody) {
                        Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                        if (!b->isTrigger && b->numVertices > 0) {
                            Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                            for (int i = 0; i < b->numVertices; ++i) { world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos); }
                            b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                            free(world_verts);
                        }
                        else {
                            b->physicsBody = NULL;
                        }
                    }
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Edit Brush Vertex");
                }

                if (UI_IsItemActivated()) {
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
                }
                if (UI_IsItemDeactivatedAfterEdit()) {
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Paint Vertex Color");
                }
            }
        }
    }
    else if (primary && primary->type == ENTITY_PLAYERSTART) {
        UI_Text("Player Start"); UI_Separator(); UI_DragFloat3("Position", &scene->playerStart.position.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PLAYERSTART, 0); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { scene->playerStart.position.x = SnapValue(scene->playerStart.position.x, g_EditorState.grid_size); scene->playerStart.position.y = SnapValue(scene->playerStart.position.y, g_EditorState.grid_size); scene->playerStart.position.z = SnapValue(scene->playerStart.position.z, g_EditorState.grid_size); } Undo_EndEntityModification(scene, ENTITY_PLAYERSTART, 0, "Move Player Start"); }
    }
    else if (primary && primary->type == ENTITY_SPRITE) {
        Sprite* s = &scene->sprites[primary->index];
        UI_Text("Sprite Properties");
        UI_Separator();
        UI_InputText("Name", s->targetname, sizeof(s->targetname));
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SPRITE, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SPRITE, primary->index, "Edit Sprite Name"); }

        UI_DragFloat3("Position", &s->pos.x, 0.1f, 0, 0);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SPRITE, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SPRITE, primary->index, "Move Sprite"); }

        UI_DragFloat("Scale", &s->scale, 0.05f, 0.01f, 100.0f);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SPRITE, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SPRITE, primary->index, "Scale Sprite"); }

        char mat_button_label[128];
        sprintf(mat_button_label, "Material: %s", s->material ? s->material->name : "None");
        if (UI_Button(mat_button_label)) {
            g_EditorState.texture_browser_target = 6;
            g_EditorState.show_texture_browser = true;
        }
    }
    else if (primary && primary->type == ENTITY_LIGHT) {
        Light* light = &scene->lights[primary->index]; UI_InputText("Name", light->targetname, sizeof(light->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Name"); } bool is_point = light->type == LIGHT_POINT; if (UI_RadioButton("Point", is_point)) { if (!is_point) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); Light_DestroyShadowMap(light); light->type = LIGHT_POINT; Light_InitShadowMap(light); Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Type"); } } UI_SameLine(); if (UI_RadioButton("Spot", !is_point)) { if (is_point) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); Light_DestroyShadowMap(light); light->type = LIGHT_SPOT; if (light->cutOff <= 0.0f) { light->cutOff = cosf(12.5f * M_PI / 180.0f); light->outerCutOff = cosf(17.5f * M_PI / 180.0f); } Light_InitShadowMap(light); Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Type"); } } UI_Separator(); UI_DragFloat3("Position", &light->position.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { light->position.x = SnapValue(light->position.x, g_EditorState.grid_size); light->position.y = SnapValue(light->position.y, g_EditorState.grid_size); light->position.z = SnapValue(light->position.z, g_EditorState.grid_size); } Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Move Light"); } if (light->type == LIGHT_SPOT) { UI_DragFloat3("Rotation", &light->rot.x, 1.0f, -360.0f, 360.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { light->rot.x = SnapAngle(light->rot.x, 15.0f); light->rot.y = SnapAngle(light->rot.y, 15.0f); light->rot.z = SnapAngle(light->rot.z, 15.0f); } Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Rotate Light"); } } UI_ColorEdit3("Color", &light->color.x); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Color"); } UI_DragFloat("Intensity", &light->base_intensity, 0.05f, 0.0f, 1000.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Intensity"); } UI_DragFloat("Radius", &light->radius, 0.1f, 0.1f, 1000.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Radius"); }UI_DragFloat("Volumetric Intensity", &light->volumetricIntensity, 0.05f, 0.0f, 20.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Volumetric Intensity"); }
        UI_Separator();
        const char* preset_names[] = {
            "0: Normal", "1: Flicker 1", "2: Slow Strong Pulse", "3: Candle 1",
            "4: Fast Strobe", "5: Gentle Pulse", "6: Flicker 2", "7: Candle 2",
            "8: Candle 3", "9: Slow Strobe", "10: Fluorescent", "11: Slow Pulse 2"
        };

        int temp_preset = light->preset;
        if (UI_Combo("Preset", &temp_preset, preset_names, 12, 12)) {
            if (temp_preset != light->preset) {
                Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                light->preset = temp_preset;
                Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Preset");
            }
        }
        if (light->type == LIGHT_SPOT) {
            char cookie_button_label[128];
            const char* cookie_name = strlen(light->cookiePath) > 0 ? light->cookiePath : "None";
            sprintf(cookie_button_label, "Cookie: %s", cookie_name);
            if (UI_Button(cookie_button_label)) {
                g_EditorState.texture_browser_target = 4;
                g_EditorState.show_texture_browser = true;
            }
            if (strlen(light->cookiePath) > 0) {
                UI_SameLine();
                if (UI_Button("[X]##clearcookie")) {
                    Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                    if (light->cookieMapHandle != 0) {
                        glMakeTextureHandleNonResidentARB(light->cookieMapHandle);
                    }
                    light->cookiePath[0] = '\0';
                    light->cookieMap = 0;
                    light->cookieMapHandle = 0;
                    Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Clear Light Cookie");
                }
            }
        }
        if (UI_Checkbox("On by default", &light->is_on)) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); light->is_on = !light->is_on; Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Toggle Light On"); }
        UI_SameLine();
        if (UI_Checkbox("Static", &light->is_static)) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Toggle Light Static");
        }
        UI_Separator(); if (light->type == LIGHT_SPOT) { UI_DragFloat("CutOff (cos)", &light->cutOff, 0.005f, 0.0f, 1.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Cutoff"); } UI_DragFloat("OuterCutOff (cos)", &light->outerCutOff, 0.005f, 0.0f, 1.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Cutoff"); } UI_Separator(); } UI_Text("Shadow Properties"); UI_DragFloat("Far Plane", &light->shadowFarPlane, 0.5f, 1.0f, 200.0f); UI_DragFloat("Bias", &light->shadowBias, 0.001f, 0.0f, 0.5f);
    }
    else if (primary && primary->type == ENTITY_DECAL) {
        Decal* d = &scene->decals[primary->index];
        UI_Text("Decal Properties");
        UI_InputText("Name", d->targetname, sizeof(d->targetname));
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal Name"); }
        UI_Separator();

        char decal_mat_button_label[128];
        const char* mat_name = d->material ? d->material->name : "___MISSING___";
        sprintf(decal_mat_button_label, "Material: %s", mat_name);
        if (UI_Button(decal_mat_button_label)) {
            g_EditorState.show_texture_browser = true;
        }

        UI_Separator();
        bool transform_changed = false;
        if (UI_DragFloat3("Position", &d->pos.x, 0.1f, 0, 0)) { transform_changed = true; }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Move Decal"); }

        if (UI_DragFloat3("Rotation", &d->rot.x, 1.0f, 0, 0)) { transform_changed = true; }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Rotate Decal"); }

        if (UI_DragFloat3("Size", &d->size.x, 0.05f, 0, 0)) { transform_changed = true; }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Scale Decal"); }

        if (transform_changed) {
            Decal_UpdateMatrix(d);
        }
    }
    else if (primary && primary->type == ENTITY_SOUND) {
        SoundEntity* s = &scene->soundEntities[primary->index]; UI_Text("Sound Entity Properties"); UI_Separator();
        UI_InputText("Name", s->targetname, sizeof(s->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Edit Sound Name"); } UI_InputText("Sound Path", s->soundPath, sizeof(s->soundPath)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Edit Sound Path"); } if (UI_Button("Load##Sound")) {
            if (s->sourceID != 0) SoundSystem_DeleteSource(s->sourceID); if (s->bufferID != 0) SoundSystem_DeleteBuffer(s->bufferID);  s->bufferID = SoundSystem_LoadSound(s->soundPath);
        } UI_DragFloat3("Position", &s->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourcePosition(s->sourceID, s->pos); Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Move Sound"); } UI_DragFloat("Volume", &s->volume, 0.05f, 0.0f, 2.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourceProperties(s->sourceID, s->volume, s->pitch, s->maxDistance); Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Edit Sound Volume"); } UI_DragFloat("Pitch", &s->pitch, 0.05f, 0.1f, 4.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourceProperties(s->sourceID, s->volume, s->pitch, s->maxDistance); Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Edit Sound Pitch"); } UI_DragFloat("Max Distance", &s->maxDistance, 1.0f, 1.0f, 1000.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourceProperties(s->sourceID, s->volume, s->pitch, s->maxDistance); Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Edit Sound Distance"); }
        if (UI_Checkbox("Looping", &s->is_looping)) {
            Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index);
            if (s->sourceID != 0) SoundSystem_SetSourceLooping(s->sourceID, s->is_looping);
            Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Toggle Sound Loop");
        }
        if (UI_Checkbox("Play on Start", &s->play_on_start)) {
            Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index);
            Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Toggle Play on Start");
        }
    }
    else if (primary && primary->type == ENTITY_PARTICLE_EMITTER) {
        ParticleEmitter* emitter = &scene->particleEmitters[primary->index]; UI_Text("Particle Emitter: %s", emitter->parFile); UI_Separator(); UI_DragFloat3("Position", &emitter->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index, "Move Emitter"); }
        UI_InputText("Name", emitter->targetname, sizeof(emitter->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index, "Edit Emitter Name"); } if (UI_Checkbox("On by default", &emitter->on_by_default)) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index); emitter->is_on = emitter->on_by_default; Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index, "Toggle Emitter On"); } if (UI_Button("Reload .par File")) { ParticleSystem_Free(emitter->system); ParticleSystem* ps = ParticleSystem_Load(emitter->parFile); if (ps) { ParticleEmitter_Init(emitter, ps, emitter->pos); } else { Console_Printf_Error("[error] Failed to reload particle system: %s", emitter->parFile); emitter->system = NULL; } }
    }
    else if (primary && primary->type == ENTITY_VIDEO_PLAYER) {
        VideoPlayer* vp = &scene->videoPlayers[primary->index];
        char oldPath[sizeof(vp->videoPath)];
        memcpy(oldPath, vp->videoPath, sizeof(vp->videoPath));

        UI_Text("Video Player Properties");
        UI_Separator();

        UI_InputText("Video Path", vp->videoPath, sizeof(vp->videoPath));
        if (strcmp(oldPath, vp->videoPath) != 0) {
            VideoPlayer_Load(vp);
        }
        UI_InputText("Name", vp->targetname, sizeof(vp->targetname));
        UI_Checkbox("Play on Start", &vp->playOnStart);
        UI_Checkbox("Loop", &vp->loop);

        UI_DragFloat3("Position", &vp->pos.x, 0.1f, 0, 0);
        UI_DragFloat3("Rotation", &vp->rot.x, 1.0f, 0, 0);
        UI_DragFloat2("Size", &vp->size.x, 0.05f, 0, 0);

        if (UI_Button("Play")) { VideoPlayer_Play(vp); }
        UI_SameLine();
        if (UI_Button("Stop")) { VideoPlayer_Stop(vp); }
        UI_SameLine();
        if (UI_Button("Restart")) { VideoPlayer_Restart(vp); }

        if (vp->textureID != 0) {
            UI_Image((void*)(intptr_t)vp->textureID, 256, 144);
        }
    }
    else if (primary && primary->type == ENTITY_PARALLAX_ROOM) {
        ParallaxRoom* p = &scene->parallaxRooms[primary->index];
        UI_Text("Parallax Room Properties");
        UI_Separator();
        UI_InputText("Name", p->targetname, sizeof(p->targetname));
        UI_InputText("Cubemap Path Base", p->cubemapPath, sizeof(p->cubemapPath));
        if (UI_Button("Reload Cubemap")) {
            if (p->cubemapTexture) glDeleteTextures(1, &p->cubemapTexture);
            const char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
            char face_paths[6][256];
            const char* face_pointers[6];
            for (int i = 0; i < 6; ++i) {
                sprintf(face_paths[i], "%s%s", p->cubemapPath, suffixes[i]);
                face_pointers[i] = face_paths[i];
            }
            p->cubemapTexture = loadCubemap(face_pointers);
        }

        UI_DragFloat3("Position", &p->pos.x, 0.1f, 0, 0);
        UI_DragFloat3("Rotation", &p->rot.x, 1.0f, 0, 0);
        UI_DragFloat2("Size", &p->size.x, 0.05f, 0, 0);
        UI_DragFloat("Room Depth", &p->roomDepth, 0.1f, 0.1f, 100.0f);
        ParallaxRoom_UpdateMatrix(p);
    }
    else if (primary && primary->type == ENTITY_LOGIC) {
        LogicEntity* ent = &scene->logicEntities[primary->index];
        UI_Text("Logic Entity Properties");
        int current_class_index = -1;
        for (int i = 0; i < num_logic_entity_classnames; ++i) {
            if (strcmp(ent->classname, logic_entity_classnames[i]) == 0) {
                current_class_index = i;
                break;
            }
        }
        if (UI_Combo("Classname", &current_class_index, logic_entity_classnames, num_logic_entity_classnames, -1)) {
            Undo_BeginEntityModification(scene, ENTITY_LOGIC, primary->index);
            strcpy(ent->classname, logic_entity_classnames[current_class_index]);
            Editor_SetDefaultLogicProperties(ent);
            Undo_EndEntityModification(scene, ENTITY_LOGIC, primary->index, "Change Logic Class");
        }
        UI_InputText("Targetname", ent->targetname, sizeof(ent->targetname));
        if (UI_DragFloat3("Position", &ent->pos.x, 0.1f, 0, 0)) {}
        if (UI_DragFloat3("Rotation", &ent->rot.x, 1.0f, 0, 0)) {}

        UI_Separator();
        UI_Text("Properties");
        int prop_to_delete = -1;
        for (int i = 0; i < ent->numProperties; ++i) {
            char key_label[32], val_label[32], del_label[32];
            sprintf(key_label, "##key%d", i);
            sprintf(val_label, "##val%d", i);
            sprintf(del_label, "[X]##prop%d", i);
            UI_PushID(i);
            UI_SetNextItemWidth(100.0f);
            UI_InputText(key_label, ent->properties[i].key, sizeof(ent->properties[i].key)); UI_SameLine();
            UI_SetNextItemWidth(120.0f);
            UI_InputText(val_label, ent->properties[i].value, sizeof(ent->properties[i].value)); UI_SameLine();
            if (UI_Button(del_label)) { prop_to_delete = i; }
            UI_PopID();
        }
        if (prop_to_delete != -1) {
            for (int i = prop_to_delete; i < ent->numProperties - 1; ++i) {
                ent->properties[i] = ent->properties[i + 1];
            }
            ent->numProperties--;
        }
        if (UI_Button("Add Property")) {
            if (ent->numProperties < MAX_ENTITY_PROPERTIES) {
                ent->numProperties++;
            }
        }

        if (strcmp(ent->classname, "logic_timer") == 0) {
            const char* timer_outputs[] = { "OnTimer" };
            RenderIOEditor(ENTITY_LOGIC, primary->index, timer_outputs, 1);
        }
        else if (strcmp(ent->classname, "math_counter") == 0) {
            const char* counter_outputs[] = { "OnHitMax", "OnHitMin" };
            RenderIOEditor(ENTITY_LOGIC, primary->index, counter_outputs, 2);
        }
        else if (strcmp(ent->classname, "logic_random") == 0) {
            const char* random_outputs[] = { "OnRandom" };
            RenderIOEditor(ENTITY_LOGIC, primary->index, random_outputs, 1);
        }
        else if (strcmp(ent->classname, "logic_relay") == 0) {
            RenderIOEditor(ENTITY_LOGIC, primary->index, g_logic_relay_outputs, g_num_logic_relay_outputs);
        }
        else if (strcmp(ent->classname, "point_servercommand") == 0) {
        }
        else if (strcmp(ent->classname, "logic_compare") == 0) {
            RenderIOEditor(ENTITY_LOGIC, primary->index, g_logic_compare_outputs, g_num_logic_compare_outputs);
        }
        else if (strcmp(ent->classname, "env_blackhole") == 0) {
        }
    }
    UI_Separator(); UI_Text("Scene Settings"); UI_Separator();
    if (UI_CollapsingHeader("Sun", 1)) {
        UI_Checkbox("Enabled##Sun", &scene->sun.enabled);
        UI_ColorEdit3("Color##Sun", &scene->sun.color.x);
        UI_DragFloat("Intensity##Sun", &scene->sun.intensity, 0.05f, 0.0f, 100.0f);
        UI_DragFloat("Volumetric Intensity##Sun", &scene->sun.volumetricIntensity, 0.05f, 0.0f, 20.0f);
        UI_DragFloat3("Direction##Sun", &scene->sun.direction.x, 0.01f, -1.0f, 1.0f);

        UI_Separator();
        UI_Text("Wind");
        UI_DragFloat3("Wind Direction", &scene->sun.windDirection.x, 0.01f, -1.0f, 1.0f);
        UI_DragFloat("Wind Strength", &scene->sun.windStrength, 0.05f, 0.0f, 10.0f);
    }
    if (UI_CollapsingHeader("Skybox", 1)) {
        UI_Checkbox("Use Cubemap Skybox", &scene->use_cubemap_skybox);
        if (scene->use_cubemap_skybox) {
            UI_InputText("Cubemap Name", scene->skybox_path, sizeof(scene->skybox_path));
            if (UI_Button("Reload Skybox")) {
                if (glIsTexture(scene->skybox_cubemap)) {
                    glDeleteTextures(1, &scene->skybox_cubemap);
                }
                const char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
                char face_paths[6][256];
                const char* face_pointers[6];
                for (int i = 0; i < 6; ++i) {
                    sprintf(face_paths[i], "skybox/%s%s", scene->skybox_path, suffixes[i]);
                    face_pointers[i] = face_paths[i];
                }
                scene->skybox_cubemap = loadCubemap(face_pointers);
            }
        }
    }
    if (UI_CollapsingHeader("Fog", 1)) { if (UI_Checkbox("Enabled", &scene->fog.enabled)) {} UI_ColorEdit3("Color", &scene->fog.color.x); UI_DragFloat("Start Distance", &scene->fog.start, 0.5f, 0.0f, 5000.0f); UI_DragFloat("End Distance", &scene->fog.end, 0.5f, 0.0f, 5000.0f); }
    if (UI_CollapsingHeader("Post-Processing", 1)) {
        if (UI_Checkbox("Enabled", &scene->post.enabled)) {} UI_Separator(); UI_Text("CRT & Vignette"); UI_DragFloat("CRT Curvature", &scene->post.crtCurvature, 0.01f, 0.0f, 1.0f); UI_DragFloat("Vignette Strength", &scene->post.vignetteStrength, 0.01f, 0.0f, 2.0f); UI_DragFloat("Vignette Radius", &scene->post.vignetteRadius, 0.01f, 0.0f, 2.0f); UI_Separator(); UI_Text("Effects"); if (UI_Checkbox("Lens Flare", &scene->post.lensFlareEnabled)) {} UI_DragFloat("Flare Strength", &scene->post.lensFlareStrength, 0.05f, 0.0f, 5.0f); UI_DragFloat("Scanline Strength", &scene->post.scanlineStrength, 0.01f, 0.0f, 1.0f); UI_DragFloat("Film Grain", &scene->post.grainIntensity, 0.005f, 0.0f, 0.5f); UI_Separator();
        UI_Separator();
        UI_Checkbox("Sharpening", &scene->post.sharpenEnabled);
        if (scene->post.sharpenEnabled)
        {
            UI_DragFloat("Sharpen Strength", &scene->post.sharpenAmount, 0.01f, 0.0f, 1.0f);
        }
        UI_Separator();
        if (UI_Checkbox("Chromatic Aberration", &scene->post.chromaticAberrationEnabled)) {}
        if (scene->post.chromaticAberrationEnabled) {
            UI_DragFloat("CA Strength", &scene->post.chromaticAberrationStrength, 0.0001f, 0.0f, 0.05f);
        }
        UI_Separator();
        if (UI_Checkbox("Black & White", &scene->post.bwEnabled)) {}
        if (scene->post.bwEnabled) {
            UI_DragFloat("Black & White Strength", &scene->post.bwStrength, 0.0001f, 0.0f, 0.05f);
        }
        UI_Separator();
        UI_Text("Depth of Field"); if (UI_Checkbox("Enabled##DOF", &scene->post.dofEnabled)) {} UI_DragFloat("Focus Distance", &scene->post.dofFocusDistance, 0.005f, 0.0f, 1.0f); UI_DragFloat("Aperture", &scene->post.dofAperture, 0.5f, 0.0f, 200.0f);
    }
    if (UI_CollapsingHeader("Color Correction", 1)) {
        UI_Checkbox("Enabled##ColorCorrection", &scene->colorCorrection.enabled);
        UI_InputText("LUT Path", scene->colorCorrection.lutPath, sizeof(scene->colorCorrection.lutPath));
        UI_SameLine();
        if (UI_Button("Reload")) {
            if (scene->colorCorrection.lutTexture) {
                glDeleteTextures(1, &scene->colorCorrection.lutTexture);
            }
            scene->colorCorrection.lutTexture = loadTexture(scene->colorCorrection.lutPath, false);
        }
        if (scene->colorCorrection.lutTexture) {
            UI_Image((void*)(intptr_t)scene->colorCorrection.lutTexture, 256, 16);
        }
    }
    UI_Separator();
    UI_Text("Creation Tools");
    UI_Separator();
    if (UI_RadioButton("Block", g_EditorState.current_brush_shape == BRUSH_SHAPE_BLOCK)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_BLOCK; }
    UI_SameLine();
    if (UI_RadioButton("Cylinder", g_EditorState.current_brush_shape == BRUSH_SHAPE_CYLINDER)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_CYLINDER; }
    UI_SameLine();
    if (UI_RadioButton("Wedge", g_EditorState.current_brush_shape == BRUSH_SHAPE_WEDGE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_WEDGE; }
    UI_SameLine();
    if (UI_RadioButton("Spike", g_EditorState.current_brush_shape == BRUSH_SHAPE_SPIKE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_SPIKE; }
    if (UI_RadioButton("Sphere", g_EditorState.current_brush_shape == BRUSH_SHAPE_SPHERE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_SPHERE; }
    UI_SameLine();
    if (UI_RadioButton("Semi-Sphere", g_EditorState.current_brush_shape == BRUSH_SHAPE_SEMI_SPHERE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_SEMI_SPHERE; }
    UI_SameLine();
    if (UI_RadioButton("Arch", g_EditorState.current_brush_shape == BRUSH_SHAPE_ARCH)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_ARCH; }
    if (g_EditorState.current_brush_shape == BRUSH_SHAPE_CYLINDER || g_EditorState.current_brush_shape == BRUSH_SHAPE_SPIKE || g_EditorState.current_brush_shape == BRUSH_SHAPE_SPHERE || g_EditorState.current_brush_shape == BRUSH_SHAPE_SEMI_SPHERE) {
        UI_DragInt("Sides", &g_EditorState.cylinder_creation_steps, 1, 4, 64);
    }
    UI_Separator(); UI_Text("Editor Settings"); UI_Separator(); if (UI_Button(g_EditorState.snap_to_grid ? "Sapping: ON" : "Snapping: OFF")) { g_EditorState.snap_to_grid = !g_EditorState.snap_to_grid; } UI_SameLine(); UI_DragFloat("Grid Size", &g_EditorState.grid_size, 0.125f, 0.125f, 64.0f);
    UI_Checkbox("Unlit Mode", &g_is_unlit_mode);
    for (int i = 0; i < 5; i++) {
        UI_Spacing();
    }
    UI_End();

    if (UI_BeginMainMenuBar()) {
        if (UI_BeginMenu("File", true)) {
            if (UI_MenuItem("New Map", NULL, false, true)) {
                Scene_Clear(scene, engine);
                strcpy(g_EditorState.currentMapPath, "untitled.map");
                Undo_Init();
            }
            if (UI_MenuItem("Load Map...", NULL, false, true)) {
                g_EditorState.show_load_map_popup = true;
                ScanMapFiles();
            }
            if (UI_MenuItem("Save", "Ctrl+S", false, true)) {
                if (strcmp(g_EditorState.currentMapPath, "untitled.map") == 0) {
                    g_EditorState.show_save_map_popup = true;
                }
                else {
                    Scene_SaveMap(scene, NULL, g_EditorState.currentMapPath);
                    Editor_AddRecentFile(g_EditorState.currentMapPath);
                }
            }
            if (UI_MenuItem("Save Map As...", NULL, false, true)) {
                g_EditorState.show_save_map_popup = true;
            }
            UI_Separator();
            if (UI_BeginMenu("Recent Files", g_EditorState.num_recent_map_files > 0)) {
                for (int i = 0; i < g_EditorState.num_recent_map_files; ++i) {
                    if (UI_MenuItem(g_EditorState.recent_map_files[i], NULL, false, true)) {
                        const char* path_to_load = g_EditorState.recent_map_files[i];

                        Scene_Clear(scene, engine);
                        if (Scene_LoadMap(scene, renderer, path_to_load, engine)) {
                            strcpy(g_EditorState.currentMapPath, path_to_load);
                            Editor_AddRecentFile(path_to_load);
                            Undo_Init();
                        }
                        else {
                            Console_Printf_Error("Failed to load recent map: %s", path_to_load);
                        }
                    }
                }
                UI_EndMenu();
            }
            UI_Separator();
            if (UI_MenuItem("Exit Editor", "F5", false, true)) { char* args[] = { "edit" }; handle_command(1, args); }
            UI_EndMenu();
        }
        if (UI_BeginMenu("Edit", true)) { if (UI_MenuItem("Undo", "Ctrl+Z", false, true)) { Undo_PerformUndo(scene, engine); } if (UI_MenuItem("Redo", "Ctrl+Y", false, true)) { Undo_PerformRedo(scene, engine); } UI_EndMenu(); }
        if (UI_BeginMenu("Tools", true)) {
            if (UI_MenuItem("Group", "Ctrl+G", false, g_EditorState.num_selections > 1)) { Editor_GroupSelection(); }
            if (UI_MenuItem("Ungroup", "Ctrl+U", false, g_EditorState.num_selections > 0)) { Editor_UngroupSelection(); }
            UI_Separator();
            if (UI_MenuItem("Replace Textures...", NULL, false, true)) {
                g_EditorState.show_replace_textures_popup = true;
            }
            if (UI_MenuItem("Sprinkle Tool...", NULL, false, true)) {
                g_EditorState.show_sprinkle_tool_window = true;
            }
            if (UI_Checkbox("Texture Lock", &g_EditorState.texture_lock_enabled)) {
            }
            if (UI_MenuItem("Bake Lighting...", NULL, false, true)) {
                g_EditorState.show_bake_lighting_popup = true;
                g_EditorState.bake_resolution = 3;
                g_EditorState.bake_bounces = 1;
            }
            UI_EndMenu();
        }
        if (UI_BeginMenu("Help", true)) {
            if (UI_MenuItem("About Tectonic Editor", NULL, false, true)) {
                g_EditorState.show_about_window = true;
            }
            if (UI_MenuItem("Documentation", NULL, false, true)) {
                g_EditorState.show_help_window = true;
                ScanDocFiles();
            }
            UI_EndMenu();
        }
        UI_EndMainMenuBar();
    }

    if (g_EditorState.show_save_map_popup) {
        UI_Begin("Save Map As", &g_EditorState.show_save_map_popup);
        UI_InputText("Filename", g_EditorState.save_map_path, sizeof(g_EditorState.save_map_path));
        if (UI_Button("Save")) {
            Scene_SaveMap(scene, NULL, g_EditorState.save_map_path);
            strcpy(g_EditorState.currentMapPath, g_EditorState.save_map_path);
            Editor_AddRecentFile(g_EditorState.currentMapPath);
            Console_Printf("Map saved to %s", g_EditorState.currentMapPath);
            g_EditorState.show_save_map_popup = false;
        }
        UI_End();
    }
    if (g_EditorState.show_load_map_popup) {
        UI_Begin("Load Map", &g_EditorState.show_load_map_popup);
        if (g_EditorState.num_map_files > 0) {
            UI_ListBox("Maps", &g_EditorState.selected_map_file_index, (const char* const*)g_EditorState.map_file_list, g_EditorState.num_map_files, 15);
            if (g_EditorState.selected_map_file_index != -1 && UI_Button("Load Selected Map")) {
                char path_buffer[256];
                sprintf(path_buffer, "%s", g_EditorState.map_file_list[g_EditorState.selected_map_file_index]);
                Scene_LoadMap(scene, renderer, path_buffer, engine);
                strcpy(g_EditorState.currentMapPath, path_buffer);
                Undo_Init();
                g_EditorState.show_load_map_popup = false;
            }
        }
        else {
            UI_Text("No .map files found in the current directory.");
        }
        if (UI_Button("Refresh List")) {
            ScanMapFiles();
        }
        UI_End();
    }

    Editor_RenderTextureBrowser(scene);
    Editor_RenderModelBrowser(scene, engine, renderer);
    Editor_RenderSoundBrowser(scene);
    Editor_RenderReplaceTexturesUI(scene);
    Editor_RenderVertexToolsWindow(scene);
    Editor_RenderSculptNoisePopup(scene);
    Editor_RenderAboutWindow();
    Editor_RenderHelpWindow();
    Editor_RenderSprinkleToolWindow();
    Editor_RenderBakeLightingWindow(scene, engine);
    Editor_RenderArchPropertiesWindow(scene, engine);

    float menu_bar_h = 22.0f; float viewports_area_w = screen_w - right_panel_width; float viewports_area_h = screen_h; float half_w = viewports_area_w / 2.0f; float half_h = viewports_area_h / 2.0f; Vec3 p[4] = { {0, menu_bar_h}, {half_w, menu_bar_h}, {0, menu_bar_h + half_h}, {half_w, menu_bar_h + half_h} }; const char* vp_names[] = { "Perspective", "Top (X/Z)","Front (X/Y)","Side (Y/Z)" };

    for (int i = 0; i < 4; i++) {
        ViewportType type = (ViewportType)i;
        UI_SetNextWindowPos(p[i].x, p[i].y);
        UI_SetNextWindowSize(half_w, half_h);
        UI_PushStyleVar_WindowPadding(0, 0);
        UI_Begin_NoBringToFront(vp_names[i], NULL);
        g_EditorState.is_viewport_focused[type] = UI_IsWindowFocused();
        g_EditorState.is_viewport_hovered[type] = UI_IsWindowHovered();
        float vp_w, vp_h;
        UI_GetContentRegionAvail(&vp_w, &vp_h);
        float win_x, win_y, content_min_x, content_min_y, mouse_x, mouse_y;
        UI_GetWindowPos(&win_x, &win_y);
        UI_GetWindowContentRegionMin(&content_min_x, &content_min_y);
        UI_GetMousePos(&mouse_x, &mouse_y);
        g_EditorState.mouse_pos_in_viewport[type].x = mouse_x - (win_x + content_min_x);
        g_EditorState.mouse_pos_in_viewport[type].y = mouse_y - (win_y + content_min_y);
        if (vp_w > 0 && vp_h > 0 && (fabs(vp_w - g_EditorState.viewport_width[type]) > 1 || fabs(vp_h - g_EditorState.viewport_height[type]) > 1)) {
            g_EditorState.viewport_width[type] = (int)vp_w;
            g_EditorState.viewport_height[type] = (int)vp_h;
            glBindTexture(GL_TEXTURE_2D, g_EditorState.viewport_texture[type]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type], 0, GL_RGBA, GL_FLOAT, NULL);
            glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.viewport_rbo[type]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type]);
        }
        UI_Image((void*)(intptr_t)g_EditorState.viewport_texture[type], vp_w, vp_h);
        UI_End();
        UI_PopStyleVar(1);
    }
    Editor_RenderFaceEditSheet(scene, engine);
}
static void Editor_AdjustSelectedBrushByHandle(Scene* scene, Engine* engine, Vec2 mouse_pos, ViewportType view) {
    if (g_EditorState.selected_brush_active_handle == PREVIEW_BRUSH_HANDLE_NONE) return;
    EditorSelection* primary = Editor_GetPrimarySelection();
    if (!primary || primary->type != ENTITY_BRUSH) return;

    Brush* b = &scene->brushes[primary->index];
    if (!b) return;

    Vec3 mouse_world = ScreenToWorld_Unsnapped_ForOrthoPicking(mouse_pos, view);
    if (g_EditorState.snap_to_grid) {
        mouse_world.x = SnapValue(mouse_world.x, g_EditorState.grid_size);
        mouse_world.y = SnapValue(mouse_world.y, g_EditorState.grid_size);
        mouse_world.z = SnapValue(mouse_world.z, g_EditorState.grid_size);
    }

    Mat4 inv_model_matrix;
    if (!mat4_inverse(&b->modelMatrix, &inv_model_matrix)) {
        return;
    }
    Vec3 new_local_pos = mat4_mul_vec3(&inv_model_matrix, mouse_world);

    Vec3 local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
    Vec3 local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (int i = 0; i < b->numVertices; ++i) {
        local_min.x = fminf(local_min.x, b->vertices[i].pos.x);
        local_min.y = fminf(local_min.y, b->vertices[i].pos.y);
        local_min.z = fminf(local_min.z, b->vertices[i].pos.z);
        local_max.x = fmaxf(local_max.x, b->vertices[i].pos.x);
        local_max.y = fmaxf(local_max.y, b->vertices[i].pos.y);
        local_max.z = fmaxf(local_max.z, b->vertices[i].pos.z);
    }

    const float min_brush_dim = 0.1f;
    switch (g_EditorState.selected_brush_active_handle) {
    case PREVIEW_BRUSH_HANDLE_MIN_X: {
        float clamped_x = (new_local_pos.x > local_max.x - min_brush_dim) ? (local_max.x - min_brush_dim) : new_local_pos.x;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.x - local_min.x) < 0.001f) {
                b->vertices[i].pos.x = clamped_x;
            }
        }
        break;
    }
    case PREVIEW_BRUSH_HANDLE_MAX_X: {
        float clamped_x = (new_local_pos.x < local_min.x + min_brush_dim) ? (local_min.x + min_brush_dim) : new_local_pos.x;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.x - local_max.x) < 0.001f) {
                b->vertices[i].pos.x = clamped_x;
            }
        }
        break;
    }
    case PREVIEW_BRUSH_HANDLE_MIN_Y: {
        float clamped_y = (new_local_pos.y > local_max.y - min_brush_dim) ? (local_max.y - min_brush_dim) : new_local_pos.y;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.y - local_min.y) < 0.001f) {
                b->vertices[i].pos.y = clamped_y;
            }
        }
        break;
    }
    case PREVIEW_BRUSH_HANDLE_MAX_Y: {
        float clamped_y = (new_local_pos.y < local_min.y + min_brush_dim) ? (local_min.y + min_brush_dim) : new_local_pos.y;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.y - local_max.y) < 0.001f) {
                b->vertices[i].pos.y = clamped_y;
            }
        }
        break;
    }
    case PREVIEW_BRUSH_HANDLE_MIN_Z: {
        float clamped_z = (new_local_pos.z > local_max.z - min_brush_dim) ? (local_max.z - min_brush_dim) : new_local_pos.z;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.z - local_min.z) < 0.001f) {
                b->vertices[i].pos.z = clamped_z;
            }
        }
        break;
    }
    case PREVIEW_BRUSH_HANDLE_MAX_Z: {
        float clamped_z = (new_local_pos.z < local_min.z + min_brush_dim) ? (local_min.z + min_brush_dim) : new_local_pos.z;
        for (int i = 0; i < b->numVertices; ++i) {
            if (fabsf(b->vertices[i].pos.z - local_max.z) < 0.001f) {
                b->vertices[i].pos.z = clamped_z;
            }
        }
        break;
    }
    default: break;
    }

    Brush_CreateRenderData(b);
    if (b->physicsBody) {
        Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
        for (int i = 0; i < b->numVertices; i++) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
        free(world_verts);
    }
}
static Vec3 ScreenToWorld(Vec2 screen_pos, ViewportType viewport) {
    float width = (float)g_EditorState.viewport_width[viewport]; float height = (float)g_EditorState.viewport_height[viewport];
    if (width <= 0 || height <= 0) return (Vec3) { 0, 0, 0 };
    float aspect = width / height; float zoom = g_EditorState.ortho_cam_zoom[viewport - 1]; Vec3 cam_pos = g_EditorState.ortho_cam_pos[viewport - 1];
    float ndc_x = (screen_pos.x / width) * 2.0f - 1.0f; float ndc_y = 1.0f - (screen_pos.y / height) * 2.0f;
    Vec3 world_pos = { 0 };
    switch (viewport) {
    case VIEW_TOP_XZ: world_pos.x = cam_pos.x + ndc_x * zoom * aspect; world_pos.z = cam_pos.z - ndc_y * zoom; world_pos.y = 0; break;
    case VIEW_FRONT_XY: world_pos.x = cam_pos.x + ndc_x * zoom * aspect; world_pos.y = cam_pos.y + ndc_y * zoom; world_pos.z = 0; break;
    case VIEW_SIDE_YZ: world_pos.z = cam_pos.z - ndc_x * zoom * aspect; world_pos.y = cam_pos.y + ndc_y * zoom; world_pos.x = 0; break;
    default: break;
    }
    if (g_EditorState.snap_to_grid) { world_pos.x = SnapValue(world_pos.x, g_EditorState.grid_size); world_pos.y = SnapValue(world_pos.y, g_EditorState.grid_size); world_pos.z = SnapValue(world_pos.z, g_EditorState.grid_size); }
    return world_pos;
}
static Vec3 ScreenToWorld_Unsnapped_ForOrthoPicking(Vec2 screen_pos, ViewportType viewport) {
    if (viewport == VIEW_PERSPECTIVE || viewport >= VIEW_COUNT) {
        return (Vec3) { 0, 0, 0 };
    }
    float width = (float)g_EditorState.viewport_width[viewport];
    float height = (float)g_EditorState.viewport_height[viewport];
    if (width <= 0 || height <= 0) return (Vec3) { 0, 0, 0 };
    float aspect = width / height;
    int ortho_array_idx = viewport - 1;
    float zoom = g_EditorState.ortho_cam_zoom[ortho_array_idx];
    Vec3 cam_center_on_plane = g_EditorState.ortho_cam_pos[ortho_array_idx];
    float ndc_x = (screen_pos.x / width) * 2.0f - 1.0f;
    float ndc_y = 1.0f - (screen_pos.y / height) * 2.0f;
    Vec3 world_pos = { 0 };
    switch (viewport) {
    case VIEW_TOP_XZ:
        world_pos.x = cam_center_on_plane.x + ndc_x * zoom * aspect;
        world_pos.z = cam_center_on_plane.z - ndc_y * zoom;
        world_pos.y = 0;
        break;
    case VIEW_FRONT_XY:
        world_pos.x = cam_center_on_plane.x + ndc_x * zoom * aspect;
        world_pos.y = cam_center_on_plane.y + ndc_y * zoom;
        world_pos.z = 0;
        break;
    case VIEW_SIDE_YZ:
        world_pos.z = cam_center_on_plane.z - ndc_x * zoom * aspect;
        world_pos.y = cam_center_on_plane.y + ndc_y * zoom;
        world_pos.x = 0;
        break;
    default:
        break;
    }
    return world_pos;
}
static Vec3 ScreenToWorld_Clip(Vec2 screen_pos, ViewportType viewport) {
    float width = (float)g_EditorState.viewport_width[viewport]; float height = (float)g_EditorState.viewport_height[viewport];
    if (width <= 0 || height <= 0) return (Vec3) { 0, 0, 0 };
    float aspect = width / height; float zoom = g_EditorState.ortho_cam_zoom[viewport - 1]; Vec3 cam_pos = g_EditorState.ortho_cam_pos[viewport - 1];
    float ndc_x = (screen_pos.x / width) * 2.0f - 1.0f; float ndc_y = 1.0f - (screen_pos.y / height) * 2.0f;
    Vec3 world_pos = { 0 };
    switch (viewport) {
    case VIEW_TOP_XZ:   world_pos.x = cam_pos.x + ndc_x * zoom * aspect; world_pos.z = cam_pos.z - ndc_y * zoom; world_pos.y = g_EditorState.clip_plane_depth; break;
    case VIEW_FRONT_XY: world_pos.x = cam_pos.x + ndc_x * zoom * aspect; world_pos.y = cam_pos.y + ndc_y * zoom; world_pos.z = g_EditorState.clip_plane_depth; break;
    case VIEW_SIDE_YZ:  world_pos.z = cam_pos.z - ndc_x * zoom * aspect; world_pos.y = cam_pos.y + ndc_y * zoom; world_pos.x = g_EditorState.clip_plane_depth; break;
    default: break;
    }
    if (g_EditorState.snap_to_grid) {
        world_pos.x = SnapValue(world_pos.x, g_EditorState.grid_size);
        world_pos.y = SnapValue(world_pos.y, g_EditorState.grid_size);
        world_pos.z = SnapValue(world_pos.z, g_EditorState.grid_size);
    }
    return world_pos;
}