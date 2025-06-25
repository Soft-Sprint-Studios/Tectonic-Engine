/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "editor.h"
#include "gl_console.h"
#include <GL/glew.h>
#include <SDL.h>
#include "gl_misc.h"
#include <math.h>
#include <float.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#endif
#include <SDL_image.h>
#include "sound_system.h"
#include "texturemanager.h"
#include "io_system.h"

extern void render_object(GLuint shader, SceneObject* obj);
extern void render_brush(GLuint shader, Brush* b);
extern void render_sun_shadows(const Mat4* sunLightSpaceMatrix);
extern void SceneObject_UpdateMatrix(SceneObject* obj);
extern void Brush_UpdateMatrix(Brush* b);
extern void Decal_UpdateMatrix(Decal* d);
extern void Brush_SetVerticesFromBox(Brush* b, Vec3 size);
extern void Brush_CreateRenderData(Brush* b);
extern void handle_command(int argc, char** argv);
extern void Light_InitShadowMap(Light* light);
extern void Light_DestroyShadowMap(Light* light);
extern Vec3 mat4_mul_vec3_dir(const Mat4* m, Vec3 v);
extern void Physics_SetWorldTransform(RigidBodyHandle body, Mat4 transform);
extern void render_shadows();
extern void render_sun_shadows(const Mat4* sunLightSpaceMatrix);
extern void render_ssao_pass(Mat4* projection);
extern void render_geometry_pass(Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix);
extern void render_bloom_pass();
extern void render_lighting_composite_pass(Mat4* view, Mat4* projection);
extern void render_skybox(Mat4* view, Mat4* projection);
extern void render_autoexposure_pass();
extern void render_volumetric_pass(Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix);

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

static const char* _stristr(const char* haystack, const char* needle) {
    if (!needle || !*needle) {
        return haystack;
    }
    for (; *haystack; ++haystack) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            const char* h = haystack;
            const char* n = needle;
            while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
                h++;
                n++;
            }
            if (!*n) {
                return haystack;
            }
        }
    }
    return NULL;
}

typedef struct {
    bool initialized; Camera editor_camera;
    bool is_in_z_mode;
    ViewportType captured_viewport;
    GLuint viewport_fbo[VIEW_COUNT], viewport_texture[VIEW_COUNT], viewport_rbo[VIEW_COUNT];
    int viewport_width[VIEW_COUNT], viewport_height[VIEW_COUNT];
    bool is_viewport_focused[VIEW_COUNT], is_viewport_hovered[VIEW_COUNT];
    Vec2 mouse_pos_in_viewport[VIEW_COUNT];
    Vec3 ortho_cam_pos[3]; float ortho_cam_zoom[3];
    EntityType selected_entity_type; int selected_entity_index; int selected_face_index;
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
    Vec3 preview_brush_drag_body_start_brush_world_min_at_drag_start;
    int selected_vertex_index; GLuint vertex_points_vao, vertex_points_vbo;
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
    char** model_file_list;
    int num_model_files;
    int selected_model_file_index;
    bool is_manipulating_gizmo;
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
} EditorState;

static EditorState g_EditorState;
static Scene* g_CurrentScene;
static Mat4 g_view_matrix[VIEW_COUNT];
static Mat4 g_proj_matrix[VIEW_COUNT];

void mat4_decompose(const Mat4* matrix, Vec3* translation, Vec3* rotation, Vec3* scale);
static Vec3 ScreenToWorld(Vec2 screen_pos, ViewportType viewport);
static Vec3 ScreenToWorld_Unsnapped_ForOrthoPicking(Vec2 screen_pos, ViewportType viewport);
static Vec3 ScreenToWorld_Clip(Vec2 screen_pos, ViewportType viewport);
float SnapValue(float value, float snap_interval) { if (snap_interval == 0.0f) return value; return roundf(value / snap_interval) * snap_interval; }
float SnapAngle(float value, float snap_interval) { if (snap_interval == 0.0f) return value; return roundf(value / snap_interval) * snap_interval; }

void Editor_SubdivideBrushFace(Scene* scene, Engine* engine, int brush_index, int face_index, int u_divs, int v_divs) {
    if (brush_index < 0 || brush_index >= scene->numBrushes) return;
    Brush* b = &scene->brushes[brush_index];
    if (face_index < 0 || face_index >= b->numFaces) return;

    BrushFace* old_face = &b->faces[face_index];
    if (old_face->numVertexIndices != 4) {
        Console_Printf("[error] Can only subdivide 4-sided faces for now.");
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

    for (int v = 0; v < v_divs; ++v) {
        for (int u = 0; u < u_divs; ++u) {
            int face_idx = v * u_divs + u;
            new_faces[face_idx] = *old_face;

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

static void FreeModelFileList() {
    if (g_EditorState.model_file_list) {
        for (int i = 0; i < g_EditorState.num_model_files; ++i) {
            free(g_EditorState.model_file_list[i]);
        }
        free(g_EditorState.model_file_list);
        g_EditorState.model_file_list = NULL;
        g_EditorState.num_model_files = 0;
    }
}
static void ScanModelFiles() {
    FreeModelFileList();
    const char* dir_path = "models/";
#ifdef _WIN32
    char search_path[256];
    sprintf(search_path, "%s*.gltf", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            g_EditorState.model_file_list = realloc(g_EditorState.model_file_list, (g_EditorState.num_model_files + 1) * sizeof(char*));
            g_EditorState.model_file_list[g_EditorState.num_model_files] = _strdup(find_data.cFileName);
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
            g_EditorState.model_file_list = realloc(g_EditorState.model_file_list, (g_EditorState.num_model_files + 1) * sizeof(char*));
            g_EditorState.model_file_list[g_EditorState.num_model_files] = strdup(dir->d_name);
            g_EditorState.num_model_files++;
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
#ifdef _WIN32
    char search_path[256];
    sprintf(search_path, "%s*.map", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            g_EditorState.map_file_list = realloc(g_EditorState.map_file_list, (g_EditorState.num_model_files + 1) * sizeof(char*));
            g_EditorState.map_file_list[g_EditorState.num_model_files] = _strdup(find_data.cFileName);
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
        if (ext && (_stricmp(ext, ".map") == 0)) {
            g_EditorState.map_file_list = realloc(g_EditorState.map_file_list, (g_EditorState.num_model_files + 1) * sizeof(char*));
            g_EditorState.map_file_list[g_EditorState.num_model_files] = strdup(dir->d_name);
            g_EditorState.num_model_files++;
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
    b->isReflectionProbe = false; b->isTrigger = false; b->physicsBody = NULL;
    Brush_UpdateMatrix(b); Brush_CreateRenderData(b);
    if (!b->isTrigger && !b->isWater && b->numVertices > 0) {
        Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
        for (int i = 0; i < b->numVertices; i++) world_verts[i] = mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
        b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
        free(world_verts);
    }
    scene->numBrushes++;
    g_EditorState.selected_entity_type = ENTITY_BRUSH; g_EditorState.selected_entity_index = scene->numBrushes - 1;
    Undo_PushCreateEntity(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Create Brush");
}

void Editor_DeleteModel(Scene* scene, int index, Engine* engine) {
    if (index < 0 || index >= scene->numObjects) return;
    Undo_PushDeleteEntity(scene, ENTITY_MODEL, index, "Delete Model");
    if (scene->objects[index].model) Model_Free(scene->objects[index].model);
    if (scene->objects[index].physicsBody) Physics_RemoveRigidBody(engine->physicsWorld, scene->objects[index].physicsBody);
    for (int i = index; i < scene->numObjects - 1; ++i) scene->objects[i] = scene->objects[i + 1];
    scene->numObjects--;
    if (scene->numObjects > 0) { scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject)); }
    else { free(scene->objects); scene->objects = NULL; }
    if (g_EditorState.selected_entity_type == ENTITY_MODEL) {
        if (g_EditorState.selected_entity_index == index) { g_EditorState.selected_entity_type = ENTITY_NONE; g_EditorState.selected_entity_index = -1; }
        else if (g_EditorState.selected_entity_index > index) { g_EditorState.selected_entity_index--; }
    }
}
void Editor_DeleteBrush(Scene* scene, Engine* engine, int index) {
    if (index < 0 || index >= scene->numBrushes) return;
    Undo_PushDeleteEntity(scene, ENTITY_BRUSH, index, "Delete Brush");
    Brush_FreeData(&scene->brushes[index]);
    if (scene->brushes[index].physicsBody) { Physics_RemoveRigidBody(engine->physicsWorld, scene->brushes[index].physicsBody); scene->brushes[index].physicsBody = NULL; }
    for (int i = index; i < scene->numBrushes - 1; ++i) scene->brushes[i] = scene->brushes[i + 1];
    scene->numBrushes--;
    if (g_EditorState.selected_entity_type == ENTITY_BRUSH) {
        if (g_EditorState.selected_entity_index == index) { g_EditorState.selected_entity_type = ENTITY_NONE; g_EditorState.selected_entity_index = -1; }
        else if (g_EditorState.selected_entity_index > index) { g_EditorState.selected_entity_index--; }
    }
}
void Editor_DeleteLight(Scene* scene, int index) {
    if (index < 0 || index >= scene->numActiveLights) return;
    Undo_PushDeleteEntity(scene, ENTITY_LIGHT, index, "Delete Light");
    Light_DestroyShadowMap(&scene->lights[index]);
    for (int i = index; i < scene->numActiveLights - 1; ++i) scene->lights[i] = scene->lights[i + 1];
    scene->numActiveLights--;
    if (g_EditorState.selected_entity_type == ENTITY_LIGHT) {
        if (g_EditorState.selected_entity_index == index) { g_EditorState.selected_entity_type = ENTITY_NONE; g_EditorState.selected_entity_index = -1; }
        else if (g_EditorState.selected_entity_index > index) { g_EditorState.selected_entity_index--; }
    }
}
void Editor_DeleteDecal(Scene* scene, int index) {
    if (index < 0 || index >= scene->numDecals) return;
    Undo_PushDeleteEntity(scene, ENTITY_DECAL, index, "Delete Decal");
    for (int i = index; i < scene->numDecals - 1; ++i) scene->decals[i] = scene->decals[i + 1];
    scene->numDecals--;
    if (g_EditorState.selected_entity_type == ENTITY_DECAL) {
        if (g_EditorState.selected_entity_index == index) { g_EditorState.selected_entity_type = ENTITY_NONE; g_EditorState.selected_entity_index = -1; }
        else if (g_EditorState.selected_entity_index > index) { g_EditorState.selected_entity_index--; }
    }
}
void Editor_DeleteSoundEntity(Scene* scene, int index) {
    if (index < 0 || index >= scene->numSoundEntities) return;
    Undo_PushDeleteEntity(scene, ENTITY_SOUND, index, "Delete Sound");
    SoundSystem_DeleteSource(scene->soundEntities[index].sourceID);
    for (int i = index; i < scene->numSoundEntities - 1; ++i) scene->soundEntities[i] = scene->soundEntities[i + 1];
    scene->numSoundEntities--;
    if (g_EditorState.selected_entity_type == ENTITY_SOUND) {
        if (g_EditorState.selected_entity_index == index) { g_EditorState.selected_entity_type = ENTITY_NONE; g_EditorState.selected_entity_index = -1; }
        else if (g_EditorState.selected_entity_index > index) { g_EditorState.selected_entity_index--; }
    }
}
void Editor_DeleteParticleEmitter(Scene* scene, int index) {
    if (index < 0 || index >= scene->numParticleEmitters) return;
    Undo_PushDeleteEntity(scene, ENTITY_PARTICLE_EMITTER, index, "Delete Particle Emitter");
    ParticleEmitter_Free(&scene->particleEmitters[index]);
    ParticleSystem_Free(scene->particleEmitters[index].system);
    for (int i = index; i < scene->numParticleEmitters - 1; ++i) { scene->particleEmitters[i] = scene->particleEmitters[i + 1]; }
    scene->numParticleEmitters--;
    if (g_EditorState.selected_entity_type == ENTITY_PARTICLE_EMITTER) {
        if (g_EditorState.selected_entity_index == index) { g_EditorState.selected_entity_type = ENTITY_NONE; g_EditorState.selected_entity_index = -1; }
        else if (g_EditorState.selected_entity_index > index) { g_EditorState.selected_entity_index--; }
    }
}

void Editor_DuplicateModel(Scene* scene, Engine* engine, int index) {
    if (index < 0 || index >= scene->numObjects) return;
    SceneObject* src_obj = &scene->objects[index];
    scene->numObjects++;
    scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
    SceneObject* new_obj = &scene->objects[scene->numObjects - 1];
    memcpy(new_obj, src_obj, sizeof(SceneObject));
    new_obj->physicsBody = NULL;
    new_obj->pos.x += 1.0f;
    SceneObject_UpdateMatrix(new_obj);
    new_obj->model = Model_Load(new_obj->modelPath);
    if (new_obj->model && new_obj->model->combinedVertexData && new_obj->model->totalIndexCount > 0) {
        Mat4 physics_transform = create_trs_matrix(new_obj->pos, new_obj->rot, (Vec3) { 1, 1, 1 });
        new_obj->physicsBody = Physics_CreateStaticTriangleMesh(engine->physicsWorld, new_obj->model->combinedVertexData, new_obj->model->totalVertexCount, new_obj->model->combinedIndexData, new_obj->model->totalIndexCount, physics_transform, new_obj->scale);
    }
    g_EditorState.selected_entity_type = ENTITY_MODEL;
    g_EditorState.selected_entity_index = scene->numObjects - 1;
    Undo_PushCreateEntity(scene, ENTITY_MODEL, g_EditorState.selected_entity_index, "Duplicate Model");
}

void Editor_DuplicateBrush(Scene* scene, Engine* engine, int index) {
    if (index < 0 || index >= scene->numBrushes || scene->numBrushes >= MAX_BRUSHES) return;
    Brush* src_brush = &scene->brushes[index];
    Brush* new_brush = &scene->brushes[scene->numBrushes];
    Brush_DeepCopy(new_brush, src_brush);
    new_brush->pos.x += 1.0f;
    Brush_UpdateMatrix(new_brush);
    Brush_CreateRenderData(new_brush);
    if (!new_brush->isTrigger && !new_brush->isReflectionProbe && !new_brush->isWater && new_brush->numVertices > 0) {
        Vec3* world_verts = malloc(new_brush->numVertices * sizeof(Vec3));
        for (int i = 0; i < new_brush->numVertices; i++) world_verts[i] = mat4_mul_vec3(&new_brush->modelMatrix, new_brush->vertices[i].pos);
        new_brush->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, new_brush->numVertices);
        free(world_verts);
    }
    scene->numBrushes++;
    g_EditorState.selected_entity_type = ENTITY_BRUSH;
    g_EditorState.selected_entity_index = scene->numBrushes - 1;
    Undo_PushCreateEntity(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Duplicate Brush");
}

void Editor_DuplicateLight(Scene* scene, int index) {
    if (index < 0 || index >= scene->numActiveLights || scene->numActiveLights >= MAX_LIGHTS) return;
    Light* src_light = &scene->lights[index];
    Light* new_light = &scene->lights[scene->numActiveLights];
    memcpy(new_light, src_light, sizeof(Light));
    new_light->shadowFBO = 0; new_light->shadowMapTexture = 0;
    new_light->position.x += 1.0f;
    Light_InitShadowMap(new_light);
    scene->numActiveLights++;
    g_EditorState.selected_entity_type = ENTITY_LIGHT;
    g_EditorState.selected_entity_index = scene->numActiveLights - 1;
    Undo_PushCreateEntity(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Duplicate Light");
}

void Editor_DuplicateDecal(Scene* scene, int index) {
    if (index < 0 || index >= scene->numDecals || scene->numDecals >= MAX_DECALS) return;
    Decal* src_decal = &scene->decals[index];
    Decal* new_decal = &scene->decals[scene->numDecals];
    memcpy(new_decal, src_decal, sizeof(Decal));
    new_decal->pos.x += 1.0f;
    Decal_UpdateMatrix(new_decal);
    scene->numDecals++;
    g_EditorState.selected_entity_type = ENTITY_DECAL;
    g_EditorState.selected_entity_index = scene->numDecals - 1;
    Undo_PushCreateEntity(scene, ENTITY_DECAL, g_EditorState.selected_entity_index, "Duplicate Decal");
}

void Editor_DuplicateSoundEntity(Scene* scene, int index) {
    if (index < 0 || index >= scene->numSoundEntities || scene->numSoundEntities >= MAX_SOUNDS) return;
    SoundEntity* src_sound = &scene->soundEntities[index];
    SoundEntity* new_sound = &scene->soundEntities[scene->numSoundEntities];
    memcpy(new_sound, src_sound, sizeof(SoundEntity));
    new_sound->sourceID = 0; new_sound->bufferID = 0;
    new_sound->pos.x += 1.0f;
    new_sound->bufferID = SoundSystem_LoadWAV(new_sound->soundPath);
    scene->numSoundEntities++;
    g_EditorState.selected_entity_type = ENTITY_SOUND;
    g_EditorState.selected_entity_index = scene->numSoundEntities - 1;
    Undo_PushCreateEntity(scene, ENTITY_SOUND, g_EditorState.selected_entity_index, "Duplicate Sound");
}

void Editor_DuplicateParticleEmitter(Scene* scene, int index) {
    if (index < 0 || index >= scene->numParticleEmitters || scene->numParticleEmitters >= MAX_PARTICLE_EMITTERS) return;
    ParticleEmitter* src_emitter = &scene->particleEmitters[index];
    ParticleEmitter* new_emitter = &scene->particleEmitters[scene->numParticleEmitters];
    memcpy(new_emitter, src_emitter, sizeof(ParticleEmitter));
    new_emitter->pos.x += 1.0f;
    ParticleSystem* ps = ParticleSystem_Load(new_emitter->parFile);
    if (ps) {
        ParticleEmitter_Init(new_emitter, ps, new_emitter->pos);
        scene->numParticleEmitters++;
        g_EditorState.selected_entity_type = ENTITY_PARTICLE_EMITTER;
        g_EditorState.selected_entity_index = scene->numParticleEmitters - 1;
        Undo_PushCreateEntity(scene, ENTITY_PARTICLE_EMITTER, g_EditorState.selected_entity_index, "Duplicate Emitter");
    }
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
    Brush_SetVerticesFromBox(b, local_size);
    Brush_UpdateMatrix(b);
    Brush_CreateRenderData(b);
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
    for (int i = 0; i < 24; ++i) { float a1 = (i / 24.0f) * 2.0f * 3.14159f; float a2 = ((i + 1) / 24.0f) * 2.0f * 3.14159f; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = radius * sinf(a2); sphere_lines[index++] = 0.0f; }
    for (int i = 0; i < 24; ++i) { float a1 = (i / 24.0f) * 2.0f * 3.14159f; float a2 = ((i + 1) / 24.0f) * 2.0f * 3.14159f; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * sinf(a2); }
    for (int i = 0; i < 24; ++i) { float a1 = (i / 24.0f) * 2.0f * 3.14159f; float a2 = ((i + 1) / 24.0f) * 2.0f * 3.14159f; sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a1); sphere_lines[index++] = radius * sinf(a1); sphere_lines[index++] = 0.0f; sphere_lines[index++] = radius * cosf(a2); sphere_lines[index++] = radius * sinf(a2); }
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
        float angle1 = (i / (float)segments) * 2.0f * 3.14159f;
        float angle2 = ((i + 1) / (float)segments) * 2.0f * 3.14159f;

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
        float angle1 = (i / (float)arc_segments) * 0.5f * 3.14159f;
        float angle2 = ((i + 1) / (float)arc_segments) * 0.5f * 3.14159f;

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
    g_CurrentScene = scene;
    memset(&g_EditorState, 0, sizeof(EditorState));
    g_EditorState.preview_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
    g_EditorState.preview_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
    g_EditorState.is_dragging_preview_brush_handle = false;
    g_EditorState.is_hovering_preview_brush_body = false;
    g_EditorState.is_dragging_preview_brush_body = false;
    g_EditorState.is_in_z_mode = false;
    g_EditorState.captured_viewport = VIEW_COUNT;
    g_EditorState.current_gizmo_operation = GIZMO_OP_TRANSLATE;
    Editor_InitGizmo();
    g_EditorState.editor_camera.position = (Vec3){ 0, 5, 15 }; g_EditorState.editor_camera.yaw = -3.14159f / 2.0f; g_EditorState.editor_camera.pitch = -0.4f;
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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_EditorState.model_preview_cam_dist = 5.0f; g_EditorState.model_preview_cam_angles = (Vec2){ 0.f, -0.5f };
    for (int i = 0; i < 3; i++) { g_EditorState.ortho_cam_pos[i] = (Vec3){ 0,0,0 }; g_EditorState.ortho_cam_zoom[i] = 10.0f; }
    Editor_InitDebugRenderer();
    glGenVertexArrays(1, &g_EditorState.vertex_points_vao); glGenBuffers(1, &g_EditorState.vertex_points_vbo);
    glGenVertexArrays(1, &g_EditorState.selected_face_vao); glGenBuffers(1, &g_EditorState.selected_face_vbo);
    g_EditorState.selected_vertex_index = -1; g_EditorState.grid_size = 1.0f; g_EditorState.snap_to_grid = true;
    g_EditorState.grid_shader = createShaderProgram("shaders/grid.vert", "shaders/grid.frag");
    Undo_Init();
    g_EditorState.initialized = true;
    g_EditorState.is_clipping = false;
    g_EditorState.clip_point_count = 0;
    strcpy(g_EditorState.currentMapPath, "level1.map");
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
}
void Editor_Shutdown() {
    if (!g_EditorState.initialized) return;
    Undo_Shutdown();
    for (int i = 0; i < VIEW_COUNT; i++) { glDeleteFramebuffers(1, &g_EditorState.viewport_fbo[i]); glDeleteTextures(1, &g_EditorState.viewport_texture[i]); glDeleteRenderbuffers(1, &g_EditorState.viewport_rbo[i]); }
    glDeleteFramebuffers(1, &g_EditorState.model_preview_fbo); glDeleteTextures(1, &g_EditorState.model_preview_texture); glDeleteRenderbuffers(1, &g_EditorState.model_preview_rbo);
    if (g_EditorState.preview_model) Model_Free(g_EditorState.preview_model);
    FreeModelFileList();
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

    g_EditorState.selected_entity_type = selected_type;
    g_EditorState.selected_entity_index = selected_index;

    if (selected_type == ENTITY_BRUSH && selected_index != -1) {
        g_EditorState.selected_face_index = hit_face_index;
        if (hit_face_index != -1) {
            Brush* brush_ptr = &g_CurrentScene->brushes[selected_index];
            BrushFace* face_ptr = &brush_ptr->faces[hit_face_index];
            if (face_ptr->numVertexIndices > 0) {
                g_EditorState.selected_vertex_index = face_ptr->vertexIndices[0];
            }
            else {
                g_EditorState.selected_vertex_index = -1;
            }
        }
        else {
            g_EditorState.selected_face_index = 0;
            Brush* brush_ptr = &g_CurrentScene->brushes[selected_index];
            if (brush_ptr->numFaces > 0 && brush_ptr->faces[0].numVertexIndices > 0) {
                g_EditorState.selected_vertex_index = brush_ptr->faces[0].vertexIndices[0];
            }
            else {
                g_EditorState.selected_vertex_index = -1;
            }
        }
    }
    else {
        g_EditorState.selected_face_index = -1;
        g_EditorState.selected_vertex_index = -1;
    }
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
static void Editor_UpdateGizmoHover(Scene* scene, Vec3 ray_origin, Vec3 ray_dir) {
    if (g_EditorState.selected_entity_type == ENTITY_NONE) {
        g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
        return;
    }
    Vec3 object_pos;
    bool has_pos = true;
    switch (g_EditorState.selected_entity_type) {
    case ENTITY_MODEL: object_pos = scene->objects[g_EditorState.selected_entity_index].pos; break;
    case ENTITY_BRUSH: object_pos = scene->brushes[g_EditorState.selected_entity_index].pos; break;
    case ENTITY_LIGHT: object_pos = scene->lights[g_EditorState.selected_entity_index].position; break;
    case ENTITY_DECAL: object_pos = scene->decals[g_EditorState.selected_entity_index].pos; break;
    case ENTITY_SOUND: object_pos = scene->soundEntities[g_EditorState.selected_entity_index].pos; break;
    case ENTITY_PARTICLE_EMITTER: object_pos = scene->particleEmitters[g_EditorState.selected_entity_index].pos; break;
    case ENTITY_PLAYERSTART: object_pos = scene->playerStart.position; break;
    default: has_pos = false; break;
    }
    if (!has_pos) { g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE; return; }

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

        if (ray_plane_intersect(ray_origin, ray_dir, (Vec3) { 0, 1, 0 }, -object_pos.y, & intersect_point)) {
            float dist_from_center = vec3_length(vec3_sub(intersect_point, object_pos));
            if (fabs(dist_from_center - radius) < pick_threshold) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Y;
        }
        if (ray_plane_intersect(ray_origin, ray_dir, (Vec3) { 1, 0, 0 }, -object_pos.x, & intersect_point)) {
            float dist_from_center = vec3_length(vec3_sub(intersect_point, object_pos));
            if (fabs(dist_from_center - radius) < pick_threshold) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_X;
        }
        if (ray_plane_intersect(ray_origin, ray_dir, (Vec3) { 0, 0, 1 }, -object_pos.z, & intersect_point)) {
            float dist_from_center = vec3_length(vec3_sub(intersect_point, object_pos));
            if (fabs(dist_from_center - radius) < pick_threshold) g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_Z;
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
    if (event->type == SDL_KEYUP && event->key.keysym.sym == SDLK_c) {
        if (g_EditorState.is_clipping) {
            if (g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index != -1 && g_EditorState.clip_point_count >= 2) {
                Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
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

                float plane_d = -vec3_dot(plane_normal, p1);
                Brush_Clip(b, plane_normal, plane_d);
                Brush_CreateRenderData(b);

                if (b->physicsBody) Physics_RemoveRigidBody(engine->physicsWorld, b->physicsBody);
                if (!b->isTrigger && !b->isWater && b->numVertices > 0) {
                    Vec3* world_verts = malloc(b->numVertices * sizeof(Vec3));
                    for (int k = 0; k < b->numVertices; ++k) world_verts[k] = mat4_mul_vec3(&b->modelMatrix, b->vertices[k].pos);
                    b->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, b->numVertices);
                    free(world_verts);
                }
                else {
                    b->physicsBody = NULL;
                }

                Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Clip Brush");
            }
            g_EditorState.is_clipping = false;
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        if (g_EditorState.is_painting_mode_enabled && g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index != -1) {
            bool is_hovering_paint_viewport = false;
            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    is_hovering_paint_viewport = true;
                    break;
                }
            }
            if (is_hovering_paint_viewport) {
                g_EditorState.is_painting = true;
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);
                return;
            }
        }
        if (g_EditorState.is_clipping) {
            for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                if (g_EditorState.is_viewport_hovered[i]) {
                    if (g_EditorState.clip_point_count < 2) {
                        if (g_EditorState.clip_point_count == 0) {
                            g_EditorState.clip_view = (ViewportType)i;
                            if (g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index != -1) {
                                Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
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
        else if (g_EditorState.gizmo_hovered_axis != GIZMO_AXIS_NONE && active_viewport != VIEW_COUNT) {
            g_EditorState.is_manipulating_gizmo = true;
            g_EditorState.gizmo_active_axis = g_EditorState.gizmo_hovered_axis;
            g_EditorState.gizmo_drag_view = active_viewport;

            if (!g_EditorState.is_in_brush_creation_mode) {
                Undo_BeginEntityModification(scene, g_EditorState.selected_entity_type, g_EditorState.selected_entity_index);
            }

            switch (g_EditorState.current_gizmo_operation) {
            case GIZMO_OP_TRANSLATE:
            case GIZMO_OP_SCALE: {
                if (g_EditorState.is_in_brush_creation_mode) {
                    g_EditorState.gizmo_drag_object_start_pos = g_EditorState.preview_brush.pos;
                    g_EditorState.gizmo_drag_object_start_rot = g_EditorState.preview_brush.rot;
                    g_EditorState.gizmo_drag_object_start_scale = g_EditorState.preview_brush.scale;
                }
                else {
                    switch (g_EditorState.selected_entity_type) {
                    case ENTITY_MODEL: g_EditorState.gizmo_drag_object_start_pos = scene->objects[g_EditorState.selected_entity_index].pos; g_EditorState.gizmo_drag_object_start_scale = scene->objects[g_EditorState.selected_entity_index].scale; break;
                    case ENTITY_BRUSH: g_EditorState.gizmo_drag_object_start_pos = scene->brushes[g_EditorState.selected_entity_index].pos; g_EditorState.gizmo_drag_object_start_scale = scene->brushes[g_EditorState.selected_entity_index].scale; break;
                    case ENTITY_LIGHT: g_EditorState.gizmo_drag_object_start_pos = scene->lights[g_EditorState.selected_entity_index].position; break;
                    case ENTITY_DECAL: g_EditorState.gizmo_drag_object_start_pos = scene->decals[g_EditorState.selected_entity_index].pos; g_EditorState.gizmo_drag_object_start_scale = scene->decals[g_EditorState.selected_entity_index].size; break;
                    case ENTITY_SOUND: g_EditorState.gizmo_drag_object_start_pos = scene->soundEntities[g_EditorState.selected_entity_index].pos; break;
                    case ENTITY_PARTICLE_EMITTER: g_EditorState.gizmo_drag_object_start_pos = scene->particleEmitters[g_EditorState.selected_entity_index].pos; break;
                    case ENTITY_PLAYERSTART: g_EditorState.gizmo_drag_object_start_pos = scene->playerStart.position; break;
                    default: break;
                    }
                }
                Vec3 drag_object_anchor_pos = g_EditorState.is_in_brush_creation_mode ? g_EditorState.preview_brush.pos : g_EditorState.gizmo_drag_object_start_pos;
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
                Vec3 object_pos_for_rotate_plane;
                if (g_EditorState.is_in_brush_creation_mode) {
                    g_EditorState.gizmo_drag_object_start_rot = g_EditorState.preview_brush.rot;
                    object_pos_for_rotate_plane = g_EditorState.preview_brush.pos;
                }
                else {
                    switch (g_EditorState.selected_entity_type) {
                    case ENTITY_MODEL: g_EditorState.gizmo_drag_object_start_rot = scene->objects[g_EditorState.selected_entity_index].rot; object_pos_for_rotate_plane = scene->objects[g_EditorState.selected_entity_index].pos; break;
                    case ENTITY_BRUSH: g_EditorState.gizmo_drag_object_start_rot = scene->brushes[g_EditorState.selected_entity_index].rot; object_pos_for_rotate_plane = scene->brushes[g_EditorState.selected_entity_index].pos; break;
                    case ENTITY_LIGHT: g_EditorState.gizmo_drag_object_start_rot = scene->lights[g_EditorState.selected_entity_index].rot; object_pos_for_rotate_plane = scene->lights[g_EditorState.selected_entity_index].position; break;
                    case ENTITY_DECAL: g_EditorState.gizmo_drag_object_start_rot = scene->decals[g_EditorState.selected_entity_index].rot; object_pos_for_rotate_plane = scene->decals[g_EditorState.selected_entity_index].pos; break;
                    default: break;
                    }
                }

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
        else if (g_EditorState.vertex_gizmo_hovered_axis != GIZMO_AXIS_NONE && g_EditorState.is_viewport_hovered[VIEW_PERSPECTIVE]) {
            g_EditorState.is_manipulating_vertex_gizmo = true;
            g_EditorState.vertex_gizmo_active_axis = g_EditorState.vertex_gizmo_hovered_axis;
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);

            Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
            g_EditorState.vertex_drag_start_pos_world = mat4_mul_vec3(&b->modelMatrix, b->vertices[g_EditorState.selected_vertex_index].pos);

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
        else if (active_viewport >= VIEW_TOP_XZ && !g_EditorState.is_manipulating_gizmo && g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index != -1) {
            Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
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
                    g_EditorState.selected_vertex_index = v_idx;
                    g_EditorState.vertex_manipulation_view = active_viewport;
                    g_EditorState.vertex_manipulation_start_pos = mouse_world_pos;
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);
                    return;
                }
            }
        }

        if (active_viewport == VIEW_PERSPECTIVE && !g_EditorState.is_in_brush_creation_mode) {
            Editor_PickObjectAtScreenPos(g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE], VIEW_PERSPECTIVE);
        }

        if (g_EditorState.selected_entity_type == ENTITY_NONE && active_viewport != VIEW_PERSPECTIVE && active_viewport != VIEW_COUNT && !g_EditorState.is_in_brush_creation_mode) {
            g_EditorState.is_dragging_for_creation = true;
            g_EditorState.brush_creation_start_point_2d_drag = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[active_viewport], active_viewport);
            g_EditorState.brush_creation_view = active_viewport;
            g_EditorState.preview_brush_world_min = g_EditorState.brush_creation_start_point_2d_drag;
            g_EditorState.preview_brush_world_max = g_EditorState.brush_creation_start_point_2d_drag;
            Editor_UpdatePreviewBrushForInitialDrag(g_EditorState.preview_brush_world_min, g_EditorState.preview_brush_world_max, g_EditorState.brush_creation_view);
        }
    }
    if (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT) {
        if (g_EditorState.is_painting) {
            g_EditorState.is_painting = false;
            Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Vertex Paint");
        }
        if (g_EditorState.is_manipulating_vertex_gizmo) {
            Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Move Vertex (Gizmo)");
            g_EditorState.is_manipulating_vertex_gizmo = false;
            g_EditorState.vertex_gizmo_active_axis = GIZMO_AXIS_NONE;
        }
        if (g_EditorState.is_vertex_manipulating) {
            Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Move Vertex");
            g_EditorState.is_vertex_manipulating = false;
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
            if (!g_EditorState.is_in_brush_creation_mode) {
                if (g_EditorState.selected_entity_type == ENTITY_MODEL) {
                    SceneObject* obj = &scene->objects[g_EditorState.selected_entity_index];
                    if (obj->physicsBody) {
                        Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix);
                    }
                }
                else if (g_EditorState.selected_entity_type == ENTITY_BRUSH) {
                    Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
                    if (b->physicsBody) {
                        Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
                    }
                }
                Undo_EndEntityModification(scene, g_EditorState.selected_entity_type, g_EditorState.selected_entity_index, "Transform Entity");
            }
            g_EditorState.is_manipulating_gizmo = false; g_EditorState.gizmo_active_axis = GIZMO_AXIS_NONE;
        }
        if (g_EditorState.is_dragging_for_creation) {
            Vec3 current_point = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.brush_creation_view], g_EditorState.brush_creation_view);
            Editor_UpdatePreviewBrushForInitialDrag(g_EditorState.brush_creation_start_point_2d_drag, current_point, g_EditorState.brush_creation_view);
            g_EditorState.is_dragging_for_creation = false;
            g_EditorState.is_in_brush_creation_mode = true;
        }
    }
    if (event->type == SDL_MOUSEMOTION) {
        if (g_EditorState.is_painting) {
            Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
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
                            if (SDL_GetModState() & KMOD_SHIFT) {
                                b->vertices[v_idx].color.x -= blend_amount;
                            }
                            else {
                                b->vertices[v_idx].color.x += blend_amount;
                            }
                            b->vertices[v_idx].color.x = fmaxf(0.0f, fminf(1.0f, b->vertices[v_idx].color.x));
                            needs_update = true;
                        }
                    }
                }
            }
            if (needs_update) {
                Brush_CreateRenderData(b);
            }
        }
        if (g_EditorState.is_dragging_preview_brush_handle) {
            Editor_AdjustPreviewBrushByHandle(g_EditorState.mouse_pos_in_viewport[g_EditorState.preview_brush_drag_handle_view], g_EditorState.preview_brush_drag_handle_view);
        }
        else if (g_EditorState.is_manipulating_vertex_gizmo) {
            Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
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
                b->vertices[g_EditorState.selected_vertex_index].pos = mat4_mul_vec3(&inv_model, new_world_pos);
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
        else if (g_EditorState.is_vertex_manipulating) {
            Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
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
        else if (g_EditorState.is_manipulating_gizmo) {
            Vec3 new_pos = g_EditorState.gizmo_drag_object_start_pos;
            Vec3 new_rot = g_EditorState.gizmo_drag_object_start_rot;
            Vec3 new_scale = g_EditorState.gizmo_drag_object_start_scale;

            if (g_EditorState.gizmo_drag_view == VIEW_PERSPECTIVE) {
                Vec2 screen_pos = g_EditorState.mouse_pos_in_viewport[VIEW_PERSPECTIVE];
                float ndc_x = (screen_pos.x / g_EditorState.viewport_width[VIEW_PERSPECTIVE]) * 2.0f - 1.0f;
                float ndc_y = 1.0f - (screen_pos.y / g_EditorState.viewport_height[VIEW_PERSPECTIVE]) * 2.0f;
                Mat4 inv_proj, inv_view; mat4_inverse(&g_proj_matrix[VIEW_PERSPECTIVE], &inv_proj); mat4_inverse(&g_view_matrix[VIEW_PERSPECTIVE], &inv_view);
                Vec4 ray_clip = { ndc_x, ndc_y, -1.0f, 1.0f }; Vec4 ray_eye = mat4_mul_vec4(&inv_proj, ray_clip); ray_eye.z = -1.0f; ray_eye.w = 0.0f;
                Vec4 ray_wor4 = mat4_mul_vec4(&inv_view, ray_eye); Vec3 ray_dir = { ray_wor4.x, ray_wor4.y, ray_wor4.z }; vec3_normalize(&ray_dir);

                Vec3 current_intersect_point;
                if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, g_EditorState.gizmo_drag_plane_d, &current_intersect_point)) {
                    Vec3 delta = vec3_sub(current_intersect_point, g_EditorState.gizmo_drag_start_world);
                    Vec3 axis_dir = { 0 };
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
                    float projection_len = vec3_dot(delta, axis_dir);

                    if (g_EditorState.current_gizmo_operation == GIZMO_OP_TRANSLATE) {
                        if (g_EditorState.snap_to_grid) projection_len = SnapValue(projection_len, g_EditorState.grid_size);
                        Vec3 projected_delta = vec3_muls(axis_dir, projection_len);
                        new_pos = vec3_add(g_EditorState.gizmo_drag_object_start_pos, projected_delta);
                    }
                    else if (g_EditorState.current_gizmo_operation == GIZMO_OP_SCALE) {
                        if (g_EditorState.snap_to_grid) projection_len = SnapValue(projection_len, 0.25f);
                        new_scale.x = g_EditorState.gizmo_drag_object_start_scale.x + axis_dir.x * projection_len;
                        new_scale.y = g_EditorState.gizmo_drag_object_start_scale.y + axis_dir.y * projection_len;
                        new_scale.z = g_EditorState.gizmo_drag_object_start_scale.z + axis_dir.z * projection_len;
                    }
                }
                if (g_EditorState.current_gizmo_operation == GIZMO_OP_ROTATE) {
                    Vec3 object_pos_for_rotate = g_EditorState.is_in_brush_creation_mode ? g_EditorState.preview_brush.pos : g_EditorState.gizmo_drag_object_start_pos;
                    if (ray_plane_intersect(g_EditorState.editor_camera.position, ray_dir, g_EditorState.gizmo_drag_plane_normal, -vec3_dot(g_EditorState.gizmo_drag_plane_normal, object_pos_for_rotate), &current_intersect_point)) {
                        Vec3 current_vec = vec3_sub(current_intersect_point, object_pos_for_rotate);
                        vec3_normalize(&current_vec);
                        float dot = vec3_dot(g_EditorState.gizmo_rotation_start_vec, current_vec);
                        dot = fmaxf(-1.0f, fminf(1.0f, dot));
                        float angle = acosf(dot) * (180.0f / 3.14159f);
                        Vec3 cross_prod = vec3_cross(g_EditorState.gizmo_rotation_start_vec, current_vec);
                        if (vec3_dot(g_EditorState.gizmo_drag_plane_normal, cross_prod) < 0) { angle = -angle; }

                        new_rot = g_EditorState.gizmo_drag_object_start_rot;
                        if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) new_rot.x += angle;
                        if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) new_rot.y += angle;
                        if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) new_rot.z += angle;
                        if (g_EditorState.snap_to_grid) {
                            new_rot.x = SnapAngle(new_rot.x, 15.0f);
                            new_rot.y = SnapAngle(new_rot.y, 15.0f);
                            new_rot.z = SnapAngle(new_rot.z, 15.0f);
                        }
                    }
                }
            }
            else {
                Vec3 current_point = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.gizmo_drag_view], g_EditorState.gizmo_drag_view);
                Vec3 delta = vec3_sub(current_point, g_EditorState.gizmo_drag_start_world);
                if (g_EditorState.snap_to_grid) {
                    delta.x = SnapValue(delta.x, g_EditorState.grid_size);
                    delta.y = SnapValue(delta.y, g_EditorState.grid_size);
                    delta.z = SnapValue(delta.z, g_EditorState.grid_size);
                }
                if (g_EditorState.current_gizmo_operation == GIZMO_OP_TRANSLATE) {
                    new_pos = vec3_add(g_EditorState.gizmo_drag_object_start_pos, delta);
                }
                else if (g_EditorState.current_gizmo_operation == GIZMO_OP_SCALE) {
                    Vec3 axis_dir = { 0 };
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) axis_dir.x = 1.0f;
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Y) axis_dir.y = 1.0f;
                    if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) axis_dir.z = 1.0f;
                    float projection_len = vec3_dot(delta, axis_dir);
                    if (g_EditorState.snap_to_grid) projection_len = SnapValue(projection_len, 0.25f);
                    new_scale.x = g_EditorState.gizmo_drag_object_start_scale.x + axis_dir.x * projection_len;
                    new_scale.y = g_EditorState.gizmo_drag_object_start_scale.y + axis_dir.y * projection_len;
                    new_scale.z = g_EditorState.gizmo_drag_object_start_scale.z + axis_dir.z * projection_len;
                }
            }

            if (g_EditorState.is_in_brush_creation_mode) {
                g_EditorState.preview_brush.pos = new_pos;
                g_EditorState.preview_brush.rot = new_rot;
                g_EditorState.preview_brush.scale = new_scale;
                Brush_UpdateMatrix(&g_EditorState.preview_brush);
                Brush_CreateRenderData(&g_EditorState.preview_brush);
            }
            else {
                switch (g_EditorState.selected_entity_type) {
                case ENTITY_MODEL: scene->objects[g_EditorState.selected_entity_index].pos = new_pos; scene->objects[g_EditorState.selected_entity_index].rot = new_rot; scene->objects[g_EditorState.selected_entity_index].scale = new_scale; SceneObject_UpdateMatrix(&scene->objects[g_EditorState.selected_entity_index]); break;
                case ENTITY_BRUSH: scene->brushes[g_EditorState.selected_entity_index].pos = new_pos; scene->brushes[g_EditorState.selected_entity_index].rot = new_rot; scene->brushes[g_EditorState.selected_entity_index].scale = new_scale; Brush_UpdateMatrix(&scene->brushes[g_EditorState.selected_entity_index]); if (scene->brushes[g_EditorState.selected_entity_index].physicsBody) Physics_SetWorldTransform(scene->brushes[g_EditorState.selected_entity_index].physicsBody, scene->brushes[g_EditorState.selected_entity_index].modelMatrix); break;
                case ENTITY_LIGHT: scene->lights[g_EditorState.selected_entity_index].position = new_pos; scene->lights[g_EditorState.selected_entity_index].rot = new_rot; break;
                case ENTITY_DECAL: scene->decals[g_EditorState.selected_entity_index].pos = new_pos; scene->decals[g_EditorState.selected_entity_index].rot = new_rot; scene->decals[g_EditorState.selected_entity_index].size = new_scale; Decal_UpdateMatrix(&scene->decals[g_EditorState.selected_entity_index]); break;
                case ENTITY_SOUND: scene->soundEntities[g_EditorState.selected_entity_index].pos = new_pos; SoundSystem_SetSourcePosition(scene->soundEntities[g_EditorState.selected_entity_index].sourceID, new_pos); break;
                case ENTITY_PARTICLE_EMITTER: scene->particleEmitters[g_EditorState.selected_entity_index].pos = new_pos; break;
                case ENTITY_PLAYERSTART: scene->playerStart.position = new_pos; break;
                default: break;
                }
            }
        }
        else if (g_EditorState.is_dragging_for_creation) {
            Vec3 current_point = ScreenToWorld(g_EditorState.mouse_pos_in_viewport[g_EditorState.brush_creation_view], (ViewportType)g_EditorState.brush_creation_view);
            Editor_UpdatePreviewBrushForInitialDrag(g_EditorState.brush_creation_start_point_2d_drag, current_point, g_EditorState.brush_creation_view);
        }
        else if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_MIDDLE)) {
            if (g_EditorState.is_viewport_focused[VIEW_TOP_XZ]) { float ms = g_EditorState.ortho_cam_zoom[0] * 0.002f; g_EditorState.ortho_cam_pos[0].x -= event->motion.xrel * ms; g_EditorState.ortho_cam_pos[0].z -= event->motion.yrel * ms; }
            if (g_EditorState.is_viewport_focused[VIEW_FRONT_XY]) { float ms = g_EditorState.ortho_cam_zoom[1] * 0.002f; g_EditorState.ortho_cam_pos[1].x -= event->motion.xrel * ms; g_EditorState.ortho_cam_pos[1].y += event->motion.yrel * ms; }
            if (g_EditorState.is_viewport_focused[VIEW_SIDE_YZ]) { float ms = g_EditorState.ortho_cam_zoom[2] * 0.002f; g_EditorState.ortho_cam_pos[2].z += event->motion.xrel * ms; g_EditorState.ortho_cam_pos[2].y += event->motion.yrel * ms; }
        }
    }
    if (event->type == SDL_MOUSEWHEEL) {
        bool hovered_any_viewport = false;
        for (int i = 1; i < VIEW_COUNT; i++) {
            if (g_EditorState.is_viewport_hovered[i]) { g_EditorState.ortho_cam_zoom[i - 1] -= event->wheel.y * g_EditorState.ortho_cam_zoom[i - 1] * 0.1f; hovered_any_viewport = true; }
        }
    }
    if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_z) { Undo_PerformUndo(scene, engine); return; }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_y) { Undo_PerformRedo(scene, engine); return; }
        if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_d) {
            if (g_EditorState.selected_entity_type != ENTITY_NONE && g_EditorState.selected_entity_index != -1) {
                switch (g_EditorState.selected_entity_type) {
                case ENTITY_MODEL: Editor_DuplicateModel(scene, engine, g_EditorState.selected_entity_index); break;
                case ENTITY_BRUSH: Editor_DuplicateBrush(scene, engine, g_EditorState.selected_entity_index); break;
                case ENTITY_LIGHT: Editor_DuplicateLight(scene, g_EditorState.selected_entity_index); break;
                case ENTITY_DECAL: Editor_DuplicateDecal(scene, g_EditorState.selected_entity_index); break;
                case ENTITY_SOUND: Editor_DuplicateSoundEntity(scene, g_EditorState.selected_entity_index); break;
                case ENTITY_PARTICLE_EMITTER: Editor_DuplicateParticleEmitter(scene, g_EditorState.selected_entity_index); break;
                default: Console_Printf("Duplication not implemented for this entity type yet."); break;
                }
            }
            return;
        }

        if (event->key.keysym.sym == SDLK_ESCAPE) {
            if (g_EditorState.is_in_z_mode) {
                g_EditorState.is_in_z_mode = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
                return;
            }
            if (g_EditorState.is_in_brush_creation_mode) {
                g_EditorState.is_in_brush_creation_mode = false;
                g_EditorState.is_dragging_for_creation = false;
                g_EditorState.is_dragging_preview_brush_handle = false;
                g_EditorState.preview_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
                g_EditorState.preview_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
            }
            if (g_EditorState.is_dragging_preview_brush_body) {
                g_EditorState.is_dragging_preview_brush_body = false;
            }
            else if (g_EditorState.is_clipping) {
                g_EditorState.is_clipping = false;
                Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Cancel Clip");
                Undo_PerformUndo(scene, engine);
            }
            if (g_EditorState.is_painting_mode_enabled && g_EditorState.selected_entity_type == ENTITY_BRUSH) {
                for (int i = VIEW_TOP_XZ; i <= VIEW_SIDE_YZ; ++i) {
                    if (g_EditorState.is_viewport_hovered[i]) {
                        g_EditorState.is_painting = true;
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);
                        return;
                    }
                }
            }
            else if (g_EditorState.selected_entity_type != ENTITY_NONE) {
                g_EditorState.selected_entity_type = ENTITY_NONE;
                g_EditorState.selected_entity_index = -1;
                g_EditorState.selected_vertex_index = -1;
                g_EditorState.selected_face_index = -1;
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
                    if (g_EditorState.is_viewport_focused[i] && i == VIEW_PERSPECTIVE) {
                        g_EditorState.is_in_z_mode = true;
                        g_EditorState.captured_viewport = (ViewportType)i;
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        break;
                    }
                }
            }
        }
        if (event->key.keysym.sym == SDLK_c && !g_EditorState.is_clipping) {
            if (g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index != -1) {
                g_EditorState.is_clipping = true;
                g_EditorState.clip_point_count = 0;
                memset(&g_EditorState.clip_side_point, 0, sizeof(Vec3));
                Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);
            }
        }
        if (g_EditorState.is_in_brush_creation_mode) {
            if (event->key.keysym.sym == SDLK_RETURN) {
                Editor_CreateBrushFromPreview(scene, engine, &g_EditorState.preview_brush);
                g_EditorState.is_in_brush_creation_mode = false;
                g_EditorState.is_dragging_for_creation = false;
                g_EditorState.is_dragging_preview_brush_handle = false;
                g_EditorState.preview_brush_active_handle = PREVIEW_BRUSH_HANDLE_NONE;
                g_EditorState.preview_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
            }
        }
        else if (!g_EditorState.is_manipulating_gizmo && !g_EditorState.is_vertex_manipulating && !g_EditorState.is_manipulating_vertex_gizmo) {
            if (event->key.keysym.sym == SDLK_1) g_EditorState.current_gizmo_operation = GIZMO_OP_TRANSLATE;
            if (event->key.keysym.sym == SDLK_2) g_EditorState.current_gizmo_operation = GIZMO_OP_ROTATE;
            if (event->key.keysym.sym == SDLK_3) g_EditorState.current_gizmo_operation = GIZMO_OP_SCALE;
            if (event->key.keysym.sym == SDLK_0) {
                g_EditorState.is_painting_mode_enabled = !g_EditorState.is_painting_mode_enabled;
                if (g_EditorState.is_painting_mode_enabled) {
                    Console_Printf("Vertex Paint Mode: ON");
                }
                else {
                    Console_Printf("Vertex Paint Mode: OFF");
                }
            }
            if (event->key.keysym.sym == SDLK_DELETE) {
                switch (g_EditorState.selected_entity_type) {
                case ENTITY_MODEL: if (g_EditorState.selected_entity_index != -1) Editor_DeleteModel(scene, g_EditorState.selected_entity_index, engine); break;
                case ENTITY_BRUSH: if (g_EditorState.selected_entity_index != -1) Editor_DeleteBrush(scene, engine, g_EditorState.selected_entity_index); break;
                case ENTITY_LIGHT: if (g_EditorState.selected_entity_index != -1) Editor_DeleteLight(scene, g_EditorState.selected_entity_index); break;
                case ENTITY_DECAL: if (g_EditorState.selected_entity_index != -1) Editor_DeleteDecal(scene, g_EditorState.selected_entity_index); break;
                case ENTITY_SOUND: if (g_EditorState.selected_entity_index != -1) Editor_DeleteSoundEntity(scene, g_EditorState.selected_entity_index); break;
                case ENTITY_PARTICLE_EMITTER: if (g_EditorState.selected_entity_index != -1) Editor_DeleteParticleEmitter(scene, g_EditorState.selected_entity_index); break;
                default: break;
                }
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
void Editor_Update(Engine* engine, Scene* scene) {
    bool can_move = g_EditorState.is_in_z_mode || (g_EditorState.is_viewport_focused[VIEW_PERSPECTIVE] && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)));
    if (can_move) {
        const Uint8* state = SDL_GetKeyboardState(NULL); float speed = 10.0f * engine->deltaTime * (state[SDL_SCANCODE_LSHIFT] ? 2.5f : 1.0f);
        Vec3 forward = { cosf(g_EditorState.editor_camera.pitch) * sinf(g_EditorState.editor_camera.yaw), sinf(g_EditorState.editor_camera.pitch), -cosf(g_EditorState.editor_camera.pitch) * cosf(g_EditorState.editor_camera.yaw) };
        vec3_normalize(&forward); Vec3 right = vec3_cross(forward, (Vec3) { 0, 1, 0 }); vec3_normalize(&right);
        if (state[SDL_SCANCODE_W]) g_EditorState.editor_camera.position = vec3_add(g_EditorState.editor_camera.position, vec3_muls(forward, speed));
        if (state[SDL_SCANCODE_S]) g_EditorState.editor_camera.position = vec3_sub(g_EditorState.editor_camera.position, vec3_muls(forward, speed));
        if (state[SDL_SCANCODE_D]) g_EditorState.editor_camera.position = vec3_add(g_EditorState.editor_camera.position, vec3_muls(right, speed));
        if (state[SDL_SCANCODE_A]) g_EditorState.editor_camera.position = vec3_sub(g_EditorState.editor_camera.position, vec3_muls(right, speed));
        if (state[SDL_SCANCODE_SPACE]) g_EditorState.editor_camera.position.y += speed; if (state[SDL_SCANCODE_LCTRL]) g_EditorState.editor_camera.position.y -= speed;
    }

    g_EditorState.gizmo_hovered_axis = GIZMO_AXIS_NONE;
    g_EditorState.vertex_gizmo_hovered_axis = GIZMO_AXIS_NONE;

    if (!g_EditorState.is_dragging_preview_brush_handle) {
        g_EditorState.preview_brush_hovered_handle = PREVIEW_BRUSH_HANDLE_NONE;
    }

    if (!g_EditorState.is_dragging_preview_brush_body) {
        g_EditorState.is_hovering_preview_brush_body = false;
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
    else if (g_EditorState.gizmo_active_axis == GIZMO_AXIS_NONE && (g_EditorState.selected_entity_type != ENTITY_NONE || g_EditorState.is_in_brush_creation_mode)) {
        Vec3 gizmo_target_pos;
        bool use_gizmo = false;
        if (g_EditorState.is_in_brush_creation_mode) {
            gizmo_target_pos = g_EditorState.preview_brush.pos;
            use_gizmo = true;
        }
        else if (g_EditorState.selected_entity_type != ENTITY_NONE) {
            switch (g_EditorState.selected_entity_type) {
            case ENTITY_MODEL: gizmo_target_pos = scene->objects[g_EditorState.selected_entity_index].pos; use_gizmo = true; break;
            case ENTITY_BRUSH: gizmo_target_pos = scene->brushes[g_EditorState.selected_entity_index].pos; use_gizmo = true; break;
            case ENTITY_LIGHT: gizmo_target_pos = scene->lights[g_EditorState.selected_entity_index].position; use_gizmo = true; break;
            case ENTITY_DECAL: gizmo_target_pos = scene->decals[g_EditorState.selected_entity_index].pos; use_gizmo = true; break;
            case ENTITY_SOUND: gizmo_target_pos = scene->soundEntities[g_EditorState.selected_entity_index].pos; use_gizmo = true; break;
            case ENTITY_PARTICLE_EMITTER: gizmo_target_pos = scene->particleEmitters[g_EditorState.selected_entity_index].pos; use_gizmo = true; break;
            case ENTITY_PLAYERSTART: gizmo_target_pos = scene->playerStart.position; use_gizmo = true; break;
            default: break;
            }
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
    if (g_EditorState.selected_entity_type == ENTITY_NONE || g_EditorState.selected_entity_index == -1) {
        return;
    }
    Vec3 object_pos;
    bool has_pos = true;
    switch (g_EditorState.selected_entity_type) {
    case ENTITY_MODEL: object_pos = g_CurrentScene->objects[g_EditorState.selected_entity_index].pos; break;
    case ENTITY_BRUSH: object_pos = g_CurrentScene->brushes[g_EditorState.selected_entity_index].pos; break;
    case ENTITY_LIGHT: object_pos = g_CurrentScene->lights[g_EditorState.selected_entity_index].position; break;
    case ENTITY_DECAL: object_pos = g_CurrentScene->decals[g_EditorState.selected_entity_index].pos; break;
    case ENTITY_SOUND: object_pos = g_CurrentScene->soundEntities[g_EditorState.selected_entity_index].pos; break;
    case ENTITY_PARTICLE_EMITTER: object_pos = g_CurrentScene->particleEmitters[g_EditorState.selected_entity_index].pos; break;
    case ENTITY_PLAYERSTART: object_pos = g_CurrentScene->playerStart.position; break;
    default: has_pos = false; break;
    }
    if (!has_pos) return;

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
            float angle = (i / (float)SEGMENTS) * 2.0f * 3.14159f;
            points[i] = vec3_add(object_pos, (Vec3) { cosf(angle)* radius, 0.0f, sinf(angle)* radius });
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINE_STRIP, 0, SEGMENTS + 1);

        Vec3 color_x = { 1,0,0 }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_X || g_EditorState.gizmo_active_axis == GIZMO_AXIS_X) color_x = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_x.x);
        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = (i / (float)SEGMENTS) * 2.0f * 3.14159f;
            points[i] = vec3_add(object_pos, (Vec3) { 0.0f, cosf(angle)* radius, sinf(angle)* radius });
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINE_STRIP, 0, SEGMENTS + 1);

        Vec3 color_z = { 0,0,1 }; if (g_EditorState.gizmo_hovered_axis == GIZMO_AXIS_Z || g_EditorState.gizmo_active_axis == GIZMO_AXIS_Z) color_z = (Vec3){ 1,1,0 };
        glUniform3fv(glGetUniformLocation(g_EditorState.gizmo_shader, "gizmoColor"), 1, &color_z.x);
        for (int i = 0; i <= SEGMENTS; ++i) {
            float angle = (i / (float)SEGMENTS) * 2.0f * 3.14159f;
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
        g_proj_matrix[type] = mat4_perspective(45.0f * (3.14159f / 180.0f), aspect, 0.1f, 10000.0f);

        render_geometry_pass(&g_view_matrix[type], &g_proj_matrix[type], sunLightSpaceMatrix);
        if (Cvar_GetInt("r_ssao")) {
            render_ssao_pass(&g_proj_matrix[type]);
        }
        render_volumetric_pass(&g_view_matrix[type], &g_proj_matrix[type], sunLightSpaceMatrix);
        render_bloom_pass();
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
        glBlitFramebuffer(0, 0, 1920, 1080,
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
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, renderer->skyboxTex);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);

        for (int i = 0; i < scene->numParticleEmitters; ++i) {
            ParticleEmitter_Render(&scene->particleEmitters[i], g_view_matrix[type], g_proj_matrix[type]);
        }

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
                float angle1 = (i / (float)segments) * 2.0f * 3.14159f;
                float angle2 = ((i + 1) / (float)segments) * 2.0f * 3.14159f;
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
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glEnable(GL_LINE_SMOOTH); glEnable(GL_POLYGON_OFFSET_LINE); glPolygonOffset(1.0, 1.0); glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); float color[] = { 0.8f, 0.8f, 0.8f, 1.0f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color);
        for (int i = 0; i < scene->numObjects; i++) { render_object(g_EditorState.debug_shader, &scene->objects[i]); }
        for (int i = 0; i < scene->numBrushes; i++) { if (!scene->brushes[i].isTrigger) render_brush(g_EditorState.debug_shader, &scene->brushes[i]); }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); glDisable(GL_LINE_SMOOTH); glDisable(GL_POLYGON_OFFSET_LINE);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, g_EditorState.viewport_fbo[type]);
    glViewport(0, 0, g_EditorState.viewport_width[type], g_EditorState.viewport_height[type]);

    glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
    for (int i = 0; i < scene->numDecals; i++) {
        Decal* d = &scene->decals[i]; glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, d->modelMatrix.m); bool is_selected = (g_EditorState.selected_entity_type == ENTITY_DECAL && g_EditorState.selected_entity_index == i); float color[] = { 0.2f, 1.0f, 0.2f, 1.0f }; if (!is_selected) { color[3] = 0.5f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.decal_box_vao); glLineWidth(is_selected ? 2.0f : 1.0f); glDrawArrays(GL_LINES, 0, g_EditorState.decal_box_vertex_count); glLineWidth(1.0f);
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
    if (g_EditorState.selected_entity_type == ENTITY_MODEL && g_EditorState.selected_entity_index < scene->numObjects) { SceneObject* obj = &scene->objects[g_EditorState.selected_entity_index]; glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); float color[] = { 1.0f, 0.5f, 0.0f, 1.0f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); render_object(g_EditorState.debug_shader, obj); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); }
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        if (b->isReflectionProbe || b->isTrigger) {
            bool is_selected = (g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index == i); if (!is_selected) continue; glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, b->modelMatrix.m); float color[] = { 1.0f, 0.5f, 0.0f, 1.0f }; if (b->isTrigger) { color[0] = 1.0f; color[1] = 0.8f; color[2] = 0.2f; } if (b->isReflectionProbe) { color[0] = 0.2f; color[1] = 0.8f; color[2] = 1.0f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glBindVertexArray(b->vao); glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount); glBindVertexArray(0); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    if (g_EditorState.selected_entity_type == ENTITY_MODEL && g_EditorState.selected_entity_index < scene->numObjects) { SceneObject* obj = &scene->objects[g_EditorState.selected_entity_index]; glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); float color[] = { 1.0f, 0.5f, 0.0f, 1.0f }; glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); render_object(g_EditorState.debug_shader, obj); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); }
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        if (b->isReflectionProbe || b->isTrigger || b->isWater) {
            bool is_selected = (g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index == i);
            if (!is_selected && !b->isWater) continue;
            glUseProgram(g_EditorState.debug_shader); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, b->modelMatrix.m); float color[] = { 1.0f, 0.5f, 0.0f, 1.0f }; if (b->isTrigger) { color[0] = 1.0f; color[1] = 0.8f; color[2] = 0.2f; } if (b->isReflectionProbe) { color[0] = 0.2f; color[1] = 0.8f; color[2] = 1.0f; } if (b->isWater) { color[0] = 0.2f; color[1] = 0.2f; color[2] = 1.0f; if (!is_selected) color[3] = 0.3f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glBindVertexArray(b->vao); glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount); glBindVertexArray(0); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    if (g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index < scene->numBrushes) {
        Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
        if (!b->isReflectionProbe && !b->isTrigger && g_EditorState.selected_face_index >= 0 && g_EditorState.selected_face_index < b->numFaces) {
            BrushFace* face = &b->faces[g_EditorState.selected_face_index];
            if (face->numVertexIndices >= 3) {
                int num_tris = face->numVertexIndices - 2;
                int num_verts = num_tris * 3;
                float* face_verts = malloc(num_verts * 3 * sizeof(float));
                for (int i = 0; i < num_tris; ++i) {
                    int tri_indices[3] = { face->vertexIndices[0], face->vertexIndices[i + 1], face->vertexIndices[i + 2] };
                    for (int j = 0; j < 3; ++j) {
                        Vec3 v = b->vertices[tri_indices[j]].pos;
                        face_verts[(i * 3 + j) * 3 + 0] = v.x;
                        face_verts[(i * 3 + j) * 3 + 1] = v.y;
                        face_verts[(i * 3 + j) * 3 + 2] = v.z;
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
    glUseProgram(g_EditorState.debug_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "view"), 1, GL_FALSE, g_view_matrix[type].m);
    glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "projection"), 1, GL_FALSE, g_proj_matrix[type].m);
    for (int i = 0; i < scene->numActiveLights; ++i) {
        Light* light = &scene->lights[i];
        bool is_selected = (g_EditorState.selected_entity_type == ENTITY_LIGHT && g_EditorState.selected_entity_index == i);

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
                    float theta = (k / 4.0f) * 2.0f * 3.14159f;
                    Vec3 p_on_circle = vec3_add(vec3_muls(right, cosf(theta) * radius), vec3_muls(up, sinf(theta) * radius));
                    Vec3 world_p = vec3_add(light->position, vec3_add(vec3_muls(dir, far_plane), p_on_circle));
                    cone_verts[vert_count++] = light->position;
                    cone_verts[vert_count++] = world_p;
                }
                for (int k = 0; k < segments; ++k) {
                    float theta1 = (k / (float)segments) * 2.0f * 3.14159f;
                    float theta2 = ((k + 1) / (float)segments) * 2.0f * 3.14159f;
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
    for (int i = 0; i < scene->numSoundEntities; ++i) { bool is_selected = (g_EditorState.selected_entity_type == ENTITY_SOUND && g_EditorState.selected_entity_index == i); Mat4 modelMatrix = mat4_translate(scene->soundEntities[i].pos); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m); float color[] = { 0.1f, 0.9f, 0.6f, 1.0f }; if (is_selected) { color[0] = 1.0f; color[1] = 0.5f; color[2] = 0.0f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.light_gizmo_vao); glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count); }
    for (int i = 0; i < scene->numParticleEmitters; ++i) { bool is_selected = (g_EditorState.selected_entity_type == ENTITY_PARTICLE_EMITTER && g_EditorState.selected_entity_index == i); Mat4 modelMatrix = mat4_translate(scene->particleEmitters[i].pos); glUniformMatrix4fv(glGetUniformLocation(g_EditorState.debug_shader, "model"), 1, GL_FALSE, modelMatrix.m); float color[] = { 1.0f, 0.2f, 0.8f, 1.0f }; if (is_selected) { color[0] = 1.0f; color[1] = 0.5f; color[2] = 0.0f; } glUniform4fv(glGetUniformLocation(g_EditorState.debug_shader, "color"), 1, color); glBindVertexArray(g_EditorState.light_gizmo_vao); glDrawArrays(GL_LINES, 0, g_EditorState.light_gizmo_vertex_count); }
    if (g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index < scene->numBrushes && g_EditorState.selected_vertex_index >= 0) {
        Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
        if (g_EditorState.selected_vertex_index < b->numVertices) {
            Vec3 vertex_local_pos = b->vertices[g_EditorState.selected_vertex_index].pos;
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
    if (g_EditorState.is_clipping && g_EditorState.clip_point_count > 0 && g_EditorState.selected_entity_type == ENTITY_BRUSH) {
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

    bool is_selected = (g_EditorState.selected_entity_type == ENTITY_PLAYERSTART);
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
    if (type == VIEW_PERSPECTIVE && g_EditorState.selected_entity_type == ENTITY_BRUSH &&
        g_EditorState.selected_entity_index != -1 && g_EditorState.selected_vertex_index != -1 &&
        !g_EditorState.is_manipulating_gizmo) {

        Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
        Vec3 vertex_world_pos = mat4_mul_vec3(&b->modelMatrix, b->vertices[g_EditorState.selected_vertex_index].pos);

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
        Mat4 proj = mat4_perspective(45.0f * (3.14159f / 180.0f), aspect, 0.1f, 1000.0f);
        glUseProgram(renderer->mainShader);
        glUniform1i(glGetUniformLocation(renderer->mainShader, "is_unlit"), 1);
        glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "view"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(renderer->mainShader, "projection"), 1, GL_FALSE, proj.m);
        glUniform1i(glGetUniformLocation(renderer->mainShader, "useEnvironmentMap"), 0);
        SceneObject temp_obj;
        memset(&temp_obj, 0, sizeof(SceneObject));
        temp_obj.model = g_EditorState.preview_model;
        mat4_identity(&temp_obj.modelMatrix);
        render_object(renderer->mainShader, &temp_obj);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void Editor_RenderAllViewports(Engine* engine, Renderer* renderer, Scene* scene) {
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
static void RenderIOEditor(EntityType type, int index) {
    const char* outputs[16]; int output_count = 0;
    if (type == ENTITY_BRUSH) { outputs[0] = "OnTouch"; outputs[1] = "OnEndTouch"; outputs[2] = "OnUse"; output_count = 3; }
    if (output_count == 0) return;
    UI_Separator(); UI_Text("Outputs");
    for (int i = 0; i < output_count; ++i) {
        if (UI_CollapsingHeader(outputs[i], 1)) {
            int conn_to_delete = -1;
            for (int k = 0; k < g_num_io_connections; k++) {
                IOConnection* conn = &g_io_connections[k];
                if (conn->sourceType == type && conn->sourceIndex == index && strcmp(conn->outputName, outputs[i]) == 0) {
                    char header_label[128]; sprintf(header_label, "To '%s' -> '%s'##%d", conn->targetName, conn->inputName, k);
                    if (UI_CollapsingHeader(header_label, 1)) {
                        char target_buf[64], input_buf[64];
                        strcpy(target_buf, conn->targetName); strcpy(input_buf, conn->inputName);
                        UI_InputText("Target Name##k", target_buf, 64); strcpy(conn->targetName, target_buf);
                        UI_InputText("Input Name##k", input_buf, 64); strcpy(conn->inputName, input_buf);
                        UI_DragFloat("Delay##k", &conn->delay, 0.1f, 0.0f, 300.0f);
                        UI_Selectable("Fire Once##k", &conn->fireOnce);
                        char delete_label[32]; sprintf(delete_label, "[X]##conn%d", k);
                        if (UI_Button(delete_label)) { conn_to_delete = k; }
                    }
                }
            }
            if (conn_to_delete != -1) { IO_RemoveConnection(conn_to_delete); }
            char add_label[64]; sprintf(add_label, "Add Connection##%d", i);
            if (UI_Button(add_label)) { IO_AddConnection(type, index, outputs[i]); }
        }
    }
}
static void Editor_RenderModelBrowser(Scene* scene, Engine* engine) {
    if (!g_EditorState.show_add_model_popup) return;

    UI_SetNextWindowSize(700, 500);
    if (UI_Begin("Model Browser", &g_EditorState.show_add_model_popup)) {
        UI_BeginChild("model_list_child", 200, 0, true, 0);
        if (UI_Button("Refresh List")) { ScanModelFiles(); }
        if (g_EditorState.num_model_files > 0) {
            if (UI_ListBox("##models", &g_EditorState.selected_model_file_index, (const char* const*)g_EditorState.model_file_list, g_EditorState.num_model_files, -1)) {
                if (g_EditorState.preview_model) { Model_Free(g_EditorState.preview_model); g_EditorState.preview_model = NULL; }
                char path_buffer[256];
                sprintf(path_buffer, "models/%s", g_EditorState.model_file_list[g_EditorState.selected_model_file_index]);
                g_EditorState.preview_model = Model_Load(path_buffer);
            }
        }
        UI_EndChild();

        UI_SameLine();

        UI_BeginChild("model_preview_child", 0, 0, false, 0);

        float w, h;
        UI_GetContentRegionAvail(&w, &h);
        h -= 40;
        if (w > 0 && h > 0 && (fabs(w - g_EditorState.model_preview_width) > 1 || fabs(h - g_EditorState.model_preview_height) > 1)) {
            g_EditorState.model_preview_width = (int)w; g_EditorState.model_preview_height = (int)h;
            glBindTexture(GL_TEXTURE_2D, g_EditorState.model_preview_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, g_EditorState.model_preview_width, g_EditorState.model_preview_height, 0, GL_RGBA, GL_FLOAT, NULL);
            glBindRenderbuffer(GL_RENDERBUFFER, g_EditorState.model_preview_rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, g_EditorState.model_preview_width, g_EditorState.model_preview_height);
        }
        UI_Image((void*)(intptr_t)g_EditorState.model_preview_texture, w, h);
        if (UI_IsItemHovered()) {
            float dx, dy; UI_GetMouseDragDelta(1, 0.0f, &dx, &dy);
            if (UI_IsMouseDragging(1)) {
                g_EditorState.model_preview_cam_angles.x += dx * 0.01f;
                g_EditorState.model_preview_cam_angles.y -= dy * 0.01f;
            }
            UI_ResetMouseDragDelta(1);
            float wheel = UI_GetMouseWheel();
            g_EditorState.model_preview_cam_dist -= wheel;
            if (g_EditorState.model_preview_cam_dist < 1.0f) g_EditorState.model_preview_cam_dist = 1.0f;
        }

        if (g_EditorState.selected_model_file_index != -1) {
            if (UI_Button("Add to Scene")) {
                scene->numObjects++;
                scene->objects = realloc(scene->objects, scene->numObjects * sizeof(SceneObject));
                SceneObject* newObj = &scene->objects[scene->numObjects - 1];
                memset(newObj, 0, sizeof(SceneObject));
                strcpy(newObj->modelPath, g_EditorState.model_file_list[g_EditorState.selected_model_file_index]);
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
        }
        UI_EndChild();
    }
    UI_End();
}
static void Editor_RenderTextureBrowser(Scene* scene) {
    if (!g_EditorState.show_texture_browser) return;

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

            if (!mat->isLoaded) {
                TextureManager_LoadMaterialTextures(mat);
            }

            UI_PushID(i);
            char btn_id[32];
            sprintf(btn_id, "##mat_btn_%d", i);
            if (UI_ImageButton(btn_id, mat->diffuseMap, 64, 64)) {
                if (g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index != -1 && g_EditorState.selected_face_index != -1) {
                    Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
                    BrushFace* face = &b->faces[g_EditorState.selected_face_index];

                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);
                    if (g_EditorState.texture_browser_target == 0) {
                        face->material = mat;
                    }
                    else {
                        face->material2 = mat;
                    }
                    Brush_CreateRenderData(b);
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Change Brush Material");

                    g_EditorState.show_texture_browser = false;
                }
                else if (g_EditorState.selected_entity_type == ENTITY_DECAL && g_EditorState.selected_entity_index != -1) {
                    Decal* d = &scene->decals[g_EditorState.selected_entity_index];

                    Undo_BeginEntityModification(scene, ENTITY_DECAL, g_EditorState.selected_entity_index);
                    d->material = mat;
                    Undo_EndEntityModification(scene, ENTITY_DECAL, g_EditorState.selected_entity_index, "Change Decal Material");

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
void Editor_RenderUI(Engine* engine, Scene* scene, Renderer* renderer) {
    static bool show_add_particle_popup = false;
    static char add_particle_path[128] = "particles/fire.par";
    float right_panel_width = 300.0f; float screen_w, screen_h;
    UI_GetDisplaySize(&screen_w, &screen_h);
    UI_SetNextWindowPos(screen_w - right_panel_width, 22); UI_SetNextWindowSize(right_panel_width, screen_h * 0.5f);
    UI_Begin("Hierarchy", NULL);
    int model_to_delete = -1, brush_to_delete = -1, light_to_delete = -1, decal_to_delete = -1, sound_to_delete = -1;
    if (UI_Selectable("Player Start", g_EditorState.selected_entity_type == ENTITY_PLAYERSTART)) { g_EditorState.selected_entity_type = ENTITY_PLAYERSTART; g_EditorState.selected_vertex_index = -1; }
    if (UI_CollapsingHeader("Models", 1)) {
        for (int i = 0; i < scene->numObjects; ++i) { char label[128]; sprintf(label, "%s##%d", scene->objects[i].modelPath, i); if (UI_Selectable(label, g_EditorState.selected_entity_type == ENTITY_MODEL && g_EditorState.selected_entity_index == i)) { g_EditorState.selected_entity_type = ENTITY_MODEL; g_EditorState.selected_entity_index = i; g_EditorState.selected_vertex_index = -1; } UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##model%d", i); if (UI_Button(del_label)) { model_to_delete = i; } }
        if (UI_Button("Add Model")) { g_EditorState.show_add_model_popup = true; ScanModelFiles(); }
    }
    if (model_to_delete != -1) { Editor_DeleteModel(scene, model_to_delete, engine); }
    if (UI_CollapsingHeader("Brushes", 1)) {
        for (int i = 0; i < scene->numBrushes; ++i) {
            if (scene->brushes[i].isReflectionProbe) continue; char label[128]; sprintf(label, "Brush %d %s", i, scene->brushes[i].isTrigger ? "[T]" : ""); if (UI_Selectable(label, g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index == i)) { g_EditorState.selected_entity_type = ENTITY_BRUSH; g_EditorState.selected_entity_index = i; g_EditorState.selected_face_index = 0; g_EditorState.selected_vertex_index = 0; }
            UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##brush%d", i); if (UI_Button(del_label)) { brush_to_delete = i; }
        }
    }
    if (brush_to_delete != -1) { Editor_DeleteBrush(scene, engine, brush_to_delete); }
    if (UI_CollapsingHeader("Water", 1)) {
        for (int i = 0; i < scene->numBrushes; ++i) {
            if (!scene->brushes[i].isWater) continue;
            char label[128]; sprintf(label, "Water Brush %d", i);
            if (UI_Selectable(label, g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index == i)) {
                g_EditorState.selected_entity_type = ENTITY_BRUSH;
                g_EditorState.selected_entity_index = i;
            }
            UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##waterbrush%d", i); if (UI_Button(del_label)) { brush_to_delete = i; }
        }
    }
    if (UI_CollapsingHeader("Lights", 1)) {
        for (int i = 0; i < scene->numActiveLights; ++i) { char label[128]; sprintf(label, "Light %d", i); if (UI_Selectable(label, g_EditorState.selected_entity_type == ENTITY_LIGHT && g_EditorState.selected_entity_index == i)) { g_EditorState.selected_entity_type = ENTITY_LIGHT; g_EditorState.selected_entity_index = i; } UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##light%d", i); if (UI_Button(del_label)) { light_to_delete = i; } }
        if (UI_Button("Add Light")) { if (scene->numActiveLights < MAX_LIGHTS) { Light* new_light = &scene->lights[scene->numActiveLights]; scene->numActiveLights++; memset(new_light, 0, sizeof(Light)); new_light->type = LIGHT_POINT; new_light->position = g_EditorState.editor_camera.position; new_light->color = (Vec3){ 1,1,1 }; new_light->intensity = 1.0f; new_light->direction = (Vec3){ 0, -1, 0 }; new_light->shadowFarPlane = 25.0f; new_light->shadowBias = 0.05f; new_light->intensity = 1.0f; new_light->radius = 10.0f; new_light->base_intensity = 1.0f; new_light->is_on = true; Light_InitShadowMap(new_light); Undo_PushCreateEntity(scene, ENTITY_LIGHT, scene->numActiveLights - 1, "Create Light"); } }
    }
    if (light_to_delete != -1) { Editor_DeleteLight(scene, light_to_delete); }
    if (UI_CollapsingHeader("Reflection Probes", 1)) {
        for (int i = 0; i < scene->numBrushes; ++i) {
            if (!scene->brushes[i].isReflectionProbe) continue;
            char label[128];
            sprintf(label, "%s##probebrush%d", scene->brushes[i].name, i);
            if (UI_Selectable(label, g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index == i)) {
                g_EditorState.selected_entity_type = ENTITY_BRUSH; g_EditorState.selected_entity_index = i;
            }
            UI_SameLine(0, 20.0f);
            char del_label[32];
            sprintf(del_label, "[X]##reflprobe%d", i);
            if (UI_Button(del_label)) { brush_to_delete = i; }
        }
    }
    if (UI_CollapsingHeader("Decals", 1)) {
        for (int i = 0; i < scene->numDecals; ++i) { char label[128]; sprintf(label, "%s##decal%d", scene->decals[i].material->name, i); if (UI_Selectable(label, g_EditorState.selected_entity_type == ENTITY_DECAL && g_EditorState.selected_entity_index == i)) { g_EditorState.selected_entity_type = ENTITY_DECAL; g_EditorState.selected_entity_index = i; } UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##decal%d", i); if (UI_Button(del_label)) { decal_to_delete = i; } }
        if (UI_Button("Add Decal")) { if (scene->numDecals < MAX_DECALS) { Decal* d = &scene->decals[scene->numDecals]; memset(d, 0, sizeof(Decal)); d->pos = g_EditorState.editor_camera.position; d->size = (Vec3){ 1, 1, 1 }; d->material = TextureManager_FindMaterial(TextureManager_GetMaterial(0)->name); Decal_UpdateMatrix(d); scene->numDecals++; Undo_PushCreateEntity(scene, ENTITY_DECAL, scene->numDecals - 1, "Create Decal"); } }
    }
    if (decal_to_delete != -1) { Editor_DeleteDecal(scene, decal_to_delete); }
    if (UI_CollapsingHeader("Sounds", 1)) {
        for (int i = 0; i < scene->numSoundEntities; ++i) { char label[128]; sprintf(label, "Sound %d##sound%d", i, i); if (UI_Selectable(label, g_EditorState.selected_entity_type == ENTITY_SOUND && g_EditorState.selected_entity_index == i)) { g_EditorState.selected_entity_type = ENTITY_SOUND; g_EditorState.selected_entity_index = i; } UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##sound%d", i); if (UI_Button(del_label)) { sound_to_delete = i; } }
        if (UI_Button("Add Sound Entity")) { if (scene->numSoundEntities < MAX_SOUNDS) { SoundEntity* s = &scene->soundEntities[scene->numSoundEntities]; memset(s, 0, sizeof(SoundEntity)); s->pos = g_EditorState.editor_camera.position; s->volume = 1.0f; s->pitch = 1.0f; s->maxDistance = 50.0f; scene->numSoundEntities++; Undo_PushCreateEntity(scene, ENTITY_SOUND, scene->numSoundEntities - 1, "Create Sound"); } }
    }
    if (sound_to_delete != -1) { Editor_DeleteSoundEntity(scene, sound_to_delete); }
    int particle_to_delete = -1;
    if (UI_CollapsingHeader("Particle Emitters", 1)) {
        for (int i = 0; i < scene->numParticleEmitters; ++i) { char label[128]; sprintf(label, "%s##particle%d", scene->particleEmitters[i].parFile, i); if (UI_Selectable(label, g_EditorState.selected_entity_type == ENTITY_PARTICLE_EMITTER && g_EditorState.selected_entity_index == i)) { g_EditorState.selected_entity_type = ENTITY_PARTICLE_EMITTER; g_EditorState.selected_entity_index = i; } UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##particle%d", i); if (UI_Button(del_label)) { particle_to_delete = i; } }
        if (UI_Button("Add Emitter")) { show_add_particle_popup = true; }
    }
    if (particle_to_delete != -1) { Editor_DeleteParticleEmitter(scene, particle_to_delete); }
    if (show_add_particle_popup) { UI_Begin("Add Particle Emitter", &show_add_particle_popup); UI_InputText("Path (.par)", add_particle_path, sizeof(add_particle_path)); if (UI_Button("Create")) { if (scene->numParticleEmitters < MAX_PARTICLE_EMITTERS) { ParticleEmitter* emitter = &scene->particleEmitters[scene->numParticleEmitters]; strcpy(emitter->parFile, add_particle_path); ParticleSystem* ps = ParticleSystem_Load(emitter->parFile); if (ps) { ParticleEmitter_Init(emitter, ps, g_EditorState.editor_camera.position); scene->numParticleEmitters++; Undo_PushCreateEntity(scene, ENTITY_PARTICLE_EMITTER, scene->numParticleEmitters - 1, "Create Particle Emitter"); } else { Console_Printf("[error] Failed to load particle system: %s", emitter->parFile); } } show_add_particle_popup = false; } UI_End(); }
    UI_End();
    UI_SetNextWindowPos(screen_w - right_panel_width, 22 + screen_h * 0.5f); UI_SetNextWindowSize(right_panel_width, screen_h * 0.5f);
    UI_Begin("Inspector & Settings", NULL); UI_Text("Inspector"); UI_Separator();
    if (g_EditorState.selected_entity_type == ENTITY_MODEL && g_EditorState.selected_entity_index < scene->numObjects) {
        SceneObject* obj = &scene->objects[g_EditorState.selected_entity_index]; UI_Text(obj->modelPath); UI_Separator();
        UI_InputText("Target Name", obj->targetname, sizeof(obj->targetname));
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index, "Edit Model Targetname"); }
        UI_DragFloat3("Position", &obj->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { obj->pos.x = SnapValue(obj->pos.x, g_EditorState.grid_size); obj->pos.y = SnapValue(obj->pos.y, g_EditorState.grid_size); obj->pos.z = SnapValue(obj->pos.z, g_EditorState.grid_size); } SceneObject_UpdateMatrix(obj); if (obj->physicsBody) { Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); } Undo_EndEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index, "Move Model"); }
        UI_DragFloat3("Rotation", &obj->rot.x, 1.0f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { obj->rot.x = SnapAngle(obj->rot.x, 15.0f); obj->rot.y = SnapAngle(obj->rot.y, 15.0f); obj->rot.z = SnapAngle(obj->rot.z, 15.0f); } SceneObject_UpdateMatrix(obj); if (obj->physicsBody) { Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); } Undo_EndEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index, "Rotate Model"); }
        UI_DragFloat3("Scale", &obj->scale.x, 0.01f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { obj->scale.x = SnapValue(obj->scale.x, 0.25f); obj->scale.y = SnapValue(obj->scale.y, 0.25f); obj->scale.z = SnapValue(obj->scale.z, 0.25f); } SceneObject_UpdateMatrix(obj); if (obj->physicsBody) { Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); } Undo_EndEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index, "Scale Model"); }
        UI_Separator();
        UI_Text("Physics Properties");
        UI_DragFloat("Mass", &obj->mass, 0.1f, 0.0f, 1000.0f);
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index, "Edit Model Mass"); }
        UI_Text("(Mass 0 = static, >0 = dynamic)");

        if (UI_Selectable("Physics Enabled", obj->isPhysicsEnabled)) {
            Undo_BeginEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index);
            obj->isPhysicsEnabled = !obj->isPhysicsEnabled;
            Undo_EndEntityModification(scene, ENTITY_MODEL, g_EditorState.selected_entity_index, "Toggle Model Physics Default");
        }
    }
    else if (g_EditorState.selected_entity_type == ENTITY_BRUSH && g_EditorState.selected_entity_index < scene->numBrushes) {
        Brush* b = &scene->brushes[g_EditorState.selected_entity_index];
        if (UI_Checkbox("Is Water", &b->isWater)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);
            if (b->isWater) {
                b->isTrigger = false;
                b->isReflectionProbe = false;
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Toggle Brush Water");
        }
        if (UI_Checkbox("Is Reflection Probe", &b->isReflectionProbe)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);
            if (b->isReflectionProbe) {
                b->isTrigger = false;
                b->isWater = false;
                int px = (int)roundf(b->pos.x);
                int py = (int)roundf(b->pos.y);
                int pz = (int)roundf(b->pos.z);
                sprintf(b->name, "Probe_%d_%d_%d", px, py, pz);
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Toggle Brush Reflection Probe");
        }
        if (UI_Checkbox("Is Trigger", &b->isTrigger)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);
            if (b->isTrigger) {
                b->isReflectionProbe = false;
                b->isWater = false;
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Toggle Brush Trigger");
        }
        UI_Separator();
        UI_InputText("Target Name", b->targetname, sizeof(b->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Edit Brush Targetname"); }
        UI_Separator(); bool transform_changed = false;
        UI_DragFloat3("Position", &b->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->pos.x = SnapValue(b->pos.x, g_EditorState.grid_size); b->pos.y = SnapValue(b->pos.y, g_EditorState.grid_size); b->pos.z = SnapValue(b->pos.z, g_EditorState.grid_size); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Move Brush"); }
        UI_DragFloat3("Rotation", &b->rot.x, 1.0f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->rot.x = SnapAngle(b->rot.x, 15.0f); b->rot.y = SnapAngle(b->rot.y, 15.0f); b->rot.z = SnapAngle(b->rot.z, 15.0f); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Rotate Brush"); }
        UI_DragFloat3("Scale", &b->scale.x, 0.01f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->scale.x = SnapValue(b->scale.x, 0.25f); b->scale.y = SnapValue(b->scale.y, 0.25f); b->scale.z = SnapValue(b->scale.z, 0.25f); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Scale Brush"); }
        if (transform_changed) { Brush_UpdateMatrix(b); if (b->physicsBody) { Physics_SetWorldTransform(b->physicsBody, b->modelMatrix); } }
        UI_Text("Vertex Paint");
        UI_Checkbox("Paint Mode Active (0)", &g_EditorState.is_painting_mode_enabled);
        if (g_EditorState.is_painting_mode_enabled) {
            UI_DragFloat("Brush Radius", &g_EditorState.paint_brush_radius, 0.1f, 0.1f, 50.0f);
            UI_DragFloat("Brush Strength", &g_EditorState.paint_brush_strength, 0.05f, 0.1f, 5.0f);
        }
        UI_Separator();
        if (b->isReflectionProbe) {
            UI_Text("Probe Name: %s", b->name);
        }
        else if (b->isTrigger) {
            RenderIOEditor(ENTITY_BRUSH, g_EditorState.selected_entity_index);
        }
        else {
            UI_Text("Face Properties (Face %d)", g_EditorState.selected_face_index);
            if (UI_Button("Flip Face Normal")) {
                if (g_EditorState.selected_face_index >= 0 && g_EditorState.selected_face_index < b->numFaces) {
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);

                    BrushFace* face_to_flip = &b->faces[g_EditorState.selected_face_index];
                    int num_indices = face_to_flip->numVertexIndices;
                    for (int k = 0; k < num_indices / 2; ++k) {
                        int temp = face_to_flip->vertexIndices[k];
                        face_to_flip->vertexIndices[k] = face_to_flip->vertexIndices[num_indices - 1 - k];
                        face_to_flip->vertexIndices[num_indices - 1 - k] = temp;
                    }

                    Brush_CreateRenderData(b);
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Flip Brush Face");
                }
            }
            UI_DragInt("Selected Face", &g_EditorState.selected_face_index, 1, 0, b->numFaces - 1);
            if (g_EditorState.selected_face_index >= 0 && g_EditorState.selected_face_index < b->numFaces) {
                BrushFace* face = &b->faces[g_EditorState.selected_face_index];

                char mat_button_label[128];
                sprintf(mat_button_label, "Material: %s", face->material->name);
                if (UI_Button(mat_button_label)) {
                    g_EditorState.texture_browser_target = 0;
                    g_EditorState.show_texture_browser = true;
                }

                char mat2_button_label[128];
                const char* mat2_name = face->material2 ? face->material2->name : "NULL";
                sprintf(mat2_button_label, "Material 2: %s", mat2_name);
                if (UI_Button(mat2_button_label)) {
                    g_EditorState.texture_browser_target = 1;
                    g_EditorState.show_texture_browser = true;
                }
                if (face->material2) {
                    UI_SameLine();
                    if (UI_Button("[X]##clear_mat2")) {
                        Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);
                        face->material2 = NULL;
                        Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Clear Blend Material");
                    }
                }

                UI_DragFloat2("UV Offset", &face->uv_offset.x, 0.05f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Brush_CreateRenderData(b); Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Edit Brush UVs"); }
                UI_DragFloat2("UV Scale", &face->uv_scale.x, 0.05f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Brush_CreateRenderData(b); Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Edit Brush UVs"); }
                UI_DragFloat("UV Rotation", &face->uv_rotation, 1.0f, -360.0f, 360.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Brush_CreateRenderData(b); Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Edit Brush UVs"); } UI_Separator();

                if (face->material2) {
                    UI_Separator();
                    UI_Text("Material 2 UVs");
                    UI_DragFloat2("UV Offset 2##uv2", &face->uv_offset2.x, 0.05f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Brush_CreateRenderData(b); Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Edit Brush UVs"); }
                    UI_DragFloat2("UV Scale 2##uv2", &face->uv_scale2.x, 0.05f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Brush_CreateRenderData(b); Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Edit Brush UVs"); }
                    UI_DragFloat("UV Rotation 2##uv2", &face->uv_rotation2, 1.0f, -360.0f, 360.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Brush_CreateRenderData(b); Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Edit Brush UVs"); }
                }
            }
            UI_Separator();
            UI_Text("Face Tools");

            static int subdivide_u = 2;
            static int subdivide_v = 2;
            UI_DragInt("Subdivisions U", &subdivide_u, 1, 1, 16);
            UI_DragInt("Subdivisions V", &subdivide_v, 1, 1, 16);

            if (UI_Button("Subdivide Selected Face")) {
                if (g_EditorState.selected_face_index != -1) {
                    Editor_SubdivideBrushFace(scene, engine, g_EditorState.selected_entity_index, g_EditorState.selected_face_index, subdivide_u, subdivide_v);
                    g_EditorState.selected_face_index = -1;
                }
            }
            UI_Separator();
            UI_Text("Vertex Properties"); UI_DragInt("Selected Vertex", &g_EditorState.selected_vertex_index, 1, 0, b->numVertices - 1);
            if (g_EditorState.selected_vertex_index >= 0 && g_EditorState.selected_vertex_index < b->numVertices) {
                BrushVertex* vert = &b->vertices[g_EditorState.selected_vertex_index];

                UI_DragFloat3("Local Position", &vert->pos.x, 0.1f, 0, 0);
                if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index); }
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
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Edit Brush Vertex");
                }

                if (UI_IsItemActivated()) {
                    Undo_BeginEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index);
                }
                if (UI_IsItemDeactivatedAfterEdit()) {
                    Undo_EndEntityModification(scene, ENTITY_BRUSH, g_EditorState.selected_entity_index, "Paint Vertex Color");
                }
            }
        }
    }
    else if (g_EditorState.selected_entity_type == ENTITY_PLAYERSTART) {
        UI_Text("Player Start"); UI_Separator(); UI_DragFloat3("Position", &scene->playerStart.position.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PLAYERSTART, 0); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { scene->playerStart.position.x = SnapValue(scene->playerStart.position.x, g_EditorState.grid_size); scene->playerStart.position.y = SnapValue(scene->playerStart.position.y, g_EditorState.grid_size); scene->playerStart.position.z = SnapValue(scene->playerStart.position.z, g_EditorState.grid_size); } Undo_EndEntityModification(scene, ENTITY_PLAYERSTART, 0, "Move Player Start"); }
    }
    else if (g_EditorState.selected_entity_type == ENTITY_LIGHT && g_EditorState.selected_entity_index < scene->numActiveLights) {
        Light* light = &scene->lights[g_EditorState.selected_entity_index]; UI_InputText("Target Name", light->targetname, sizeof(light->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Edit Light Targetname"); } bool is_point = light->type == LIGHT_POINT; if (UI_RadioButton("Point", is_point)) { if (!is_point) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); Light_DestroyShadowMap(light); light->type = LIGHT_POINT; Light_InitShadowMap(light); Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Change Light Type"); } } UI_SameLine(); if (UI_RadioButton("Spot", !is_point)) { if (is_point) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); Light_DestroyShadowMap(light); light->type = LIGHT_SPOT; if (light->cutOff <= 0.0f) { light->cutOff = cosf(12.5f * 3.14159f / 180.0f); light->outerCutOff = cosf(17.5f * 3.14159f / 180.0f); } Light_InitShadowMap(light); Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Change Light Type"); } } UI_Separator(); UI_DragFloat3("Position", &light->position.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { light->position.x = SnapValue(light->position.x, g_EditorState.grid_size); light->position.y = SnapValue(light->position.y, g_EditorState.grid_size); light->position.z = SnapValue(light->position.z, g_EditorState.grid_size); } Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Move Light"); } if (light->type == LIGHT_SPOT) { UI_DragFloat3("Rotation", &light->rot.x, 1.0f, -360.0f, 360.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { light->rot.x = SnapAngle(light->rot.x, 15.0f); light->rot.y = SnapAngle(light->rot.y, 15.0f); light->rot.z = SnapAngle(light->rot.z, 15.0f); } Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Rotate Light"); } } UI_ColorEdit3("Color", &light->color.x); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Edit Light Color"); } UI_DragFloat("Intensity", &light->base_intensity, 0.05f, 0.0f, 1000.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Edit Light Intensity"); } UI_DragFloat("Radius", &light->radius, 0.1f, 0.1f, 1000.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Edit Light Radius"); }UI_DragFloat("Volumetric Intensity", &light->volumetricIntensity, 0.05f, 0.0f, 20.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Edit Volumetric Intensity"); }  if (UI_Checkbox("On by default", &light->is_on)) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); light->is_on = !light->is_on; Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Toggle Light On"); } UI_Separator(); if (light->type == LIGHT_SPOT) { UI_DragFloat("CutOff (cos)", &light->cutOff, 0.005f, 0.0f, 1.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Edit Light Cutoff"); } UI_DragFloat("OuterCutOff (cos)", &light->outerCutOff, 0.005f, 0.0f, 1.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, g_EditorState.selected_entity_index, "Edit Light Cutoff"); } UI_Separator(); } UI_Text("Shadow Properties"); UI_DragFloat("Far Plane", &light->shadowFarPlane, 0.5f, 1.0f, 200.0f); UI_DragFloat("Bias", &light->shadowBias, 0.001f, 0.0f, 0.5f);
    }
    else if (g_EditorState.selected_entity_type == ENTITY_DECAL && g_EditorState.selected_entity_index < scene->numDecals) {
        Decal* d = &scene->decals[g_EditorState.selected_entity_index];
        UI_Text("Decal Properties");
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
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, g_EditorState.selected_entity_index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, g_EditorState.selected_entity_index, "Move Decal"); }

        if (UI_DragFloat3("Rotation", &d->rot.x, 1.0f, 0, 0)) { transform_changed = true; }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, g_EditorState.selected_entity_index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, g_EditorState.selected_entity_index, "Rotate Decal"); }

        if (UI_DragFloat3("Size", &d->size.x, 0.05f, 0, 0)) { transform_changed = true; }
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, g_EditorState.selected_entity_index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, g_EditorState.selected_entity_index, "Scale Decal"); }

        if (transform_changed) {
            Decal_UpdateMatrix(d);
        }
    }
    else if (g_EditorState.selected_entity_type == ENTITY_SOUND && g_EditorState.selected_entity_index < scene->numSoundEntities) {
        SoundEntity* s = &scene->soundEntities[g_EditorState.selected_entity_index]; UI_Text("Sound Entity Properties"); UI_Separator(); UI_InputText("Target Name", s->targetname, sizeof(s->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index, "Edit Sound Targetname"); } UI_InputText("Sound Path", s->soundPath, sizeof(s->soundPath)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index, "Edit Sound Path"); } if (UI_Button("Load##Sound")) { if (s->sourceID != 0) SoundSystem_DeleteSource(s->sourceID); if (s->bufferID != 0) SoundSystem_DeleteBuffer(s->bufferID); s->bufferID = SoundSystem_LoadWAV(s->soundPath); } UI_DragFloat3("Position", &s->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourcePosition(s->sourceID, s->pos); Undo_EndEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index, "Move Sound"); } UI_DragFloat("Volume", &s->volume, 0.05f, 0.0f, 2.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourceProperties(s->sourceID, s->volume, s->pitch, s->maxDistance); Undo_EndEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index, "Edit Sound Volume"); } UI_DragFloat("Pitch", &s->pitch, 0.05f, 0.1f, 4.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourceProperties(s->sourceID, s->volume, s->pitch, s->maxDistance); Undo_EndEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index, "Edit Sound Pitch"); } UI_DragFloat("Max Distance", &s->maxDistance, 1.0f, 1.0f, 1000.0f); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { SoundSystem_SetSourceProperties(s->sourceID, s->volume, s->pitch, s->maxDistance); Undo_EndEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index, "Edit Sound Distance"); }
        if (UI_Checkbox("Looping", &s->is_looping)) {
            Undo_BeginEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index);
            if (s->sourceID != 0) SoundSystem_SetSourceLooping(s->sourceID, s->is_looping);
            Undo_EndEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index, "Toggle Sound Loop");
        }
        if (UI_Checkbox("Play on Start", &s->play_on_start)) {
            Undo_BeginEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index);
            Undo_EndEntityModification(scene, ENTITY_SOUND, g_EditorState.selected_entity_index, "Toggle Play on Start");
        }
    }
    else if (g_EditorState.selected_entity_type == ENTITY_PARTICLE_EMITTER && g_EditorState.selected_entity_index < scene->numParticleEmitters) {
        ParticleEmitter* emitter = &scene->particleEmitters[g_EditorState.selected_entity_index]; UI_Text("Particle Emitter: %s", emitter->parFile); UI_Separator(); UI_DragFloat3("Position", &emitter->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, g_EditorState.selected_entity_index, "Move Emitter"); } UI_InputText("Target Name", emitter->targetname, sizeof(emitter->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, g_EditorState.selected_entity_index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, g_EditorState.selected_entity_index, "Edit Emitter Targetname"); } if (UI_Checkbox("On by default", &emitter->on_by_default)) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, g_EditorState.selected_entity_index); emitter->on_by_default = !emitter->on_by_default; emitter->is_on = emitter->on_by_default; Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, g_EditorState.selected_entity_index, "Toggle Emitter On"); } if (UI_Button("Reload .par File")) { ParticleSystem_Free(emitter->system); ParticleSystem* ps = ParticleSystem_Load(emitter->parFile); if (ps) { ParticleEmitter_Init(emitter, ps, emitter->pos); } else { Console_Printf("[error] Failed to reload particle system: %s", emitter->parFile); emitter->system = NULL; } }
    }
    UI_Separator(); UI_Text("Scene Settings"); UI_Separator();
    if (UI_CollapsingHeader("Sun", 1)) {
        UI_Checkbox("Enabled##Sun", &scene->sun.enabled);
        UI_ColorEdit3("Color##Sun", &scene->sun.color.x);
        UI_DragFloat("Intensity##Sun", &scene->sun.intensity, 0.05f, 0.0f, 100.0f);
        UI_DragFloat("Volumetric Intensity##Sun", &scene->sun.volumetricIntensity, 0.05f, 0.0f, 20.0f);
        if (UI_DragFloat3("Direction##Sun", &scene->sun.direction.x, 0.01f, -1.0f, 1.0f)) {
            vec3_normalize(&scene->sun.direction);
        }
    }
    if (UI_CollapsingHeader("Fog", 1)) { if (UI_Checkbox("Enabled", &scene->fog.enabled)) {} UI_ColorEdit3("Color", &scene->fog.color.x); UI_DragFloat("Start Distance", &scene->fog.start, 0.5f, 0.0f, 5000.0f); UI_DragFloat("End Distance", &scene->fog.end, 0.5f, 0.0f, 5000.0f); }
    if (UI_CollapsingHeader("Post-Processing", 1)) { if (UI_Checkbox("Enabled", &scene->post.enabled)) {} UI_Separator(); UI_Text("CRT & Vignette"); UI_DragFloat("CRT Curvature", &scene->post.crtCurvature, 0.01f, 0.0f, 1.0f); UI_DragFloat("Vignette Strength", &scene->post.vignetteStrength, 0.01f, 0.0f, 2.0f); UI_DragFloat("Vignette Radius", &scene->post.vignetteRadius, 0.01f, 0.0f, 2.0f); UI_Separator(); UI_Text("Effects"); if (UI_Checkbox("Lens Flare", &scene->post.lensFlareEnabled)) {} UI_DragFloat("Flare Strength", &scene->post.lensFlareStrength, 0.05f, 0.0f, 5.0f); UI_DragFloat("Scanline Strength", &scene->post.scanlineStrength, 0.01f, 0.0f, 1.0f); UI_DragFloat("Film Grain", &scene->post.grainIntensity, 0.005f, 0.0f, 0.5f); UI_Separator(); UI_Text("Depth of Field"); if (UI_Checkbox("Enabled##DOF", &scene->post.dofEnabled)) {} UI_DragFloat("Focus Distance", &scene->post.dofFocusDistance, 0.005f, 0.0f, 1.0f); UI_DragFloat("Aperture", &scene->post.dofAperture, 0.5f, 0.0f, 200.0f); }
    UI_Separator(); UI_Text("Editor Settings"); UI_Separator(); if (UI_Button(g_EditorState.snap_to_grid ? "Sapping: ON" : "Snapping: OFF")) { g_EditorState.snap_to_grid = !g_EditorState.snap_to_grid; } UI_SameLine(); UI_DragFloat("Grid Size", &g_EditorState.grid_size, 0.125f, 0.125f, 64.0f);
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
                    Scene_SaveMap(scene, g_EditorState.currentMapPath);
                    Console_Printf("Map saved to %s", g_EditorState.currentMapPath);
                }
            }
            if (UI_MenuItem("Save Map As...", NULL, false, true)) {
                g_EditorState.show_save_map_popup = true;
            }
            if (UI_MenuItem("Exit Editor", "F5", false, true)) { char* args[] = { "edit" }; handle_command(1, args); }
            UI_EndMenu();
        }
        if (UI_BeginMenu("Edit", true)) { if (UI_MenuItem("Undo", "Ctrl+Z", false, true)) { Undo_PerformUndo(scene, engine); } if (UI_MenuItem("Redo", "Ctrl+Y", false, true)) { Undo_PerformRedo(scene, engine); } UI_EndMenu(); }
        if (UI_BeginMenu("Tools", true)) { if (UI_MenuItem("Build Cubemaps", NULL, false, true)) { Editor_BuildCubemaps(scene, renderer, engine); } UI_EndMenu(); }
        UI_EndMainMenuBar();
    }

    if (g_EditorState.show_save_map_popup) {
        UI_Begin("Save Map As", &g_EditorState.show_save_map_popup);
        UI_InputText("Filename", g_EditorState.save_map_path, sizeof(g_EditorState.save_map_path));
        if (UI_Button("Save")) {
            Scene_SaveMap(scene, g_EditorState.save_map_path);
            strcpy(g_EditorState.currentMapPath, g_EditorState.save_map_path);
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
    Editor_RenderModelBrowser(scene, engine);

    float menu_bar_h = 22.0f; float viewports_area_w = screen_w - right_panel_width; float viewports_area_h = screen_h; float half_w = viewports_area_w / 2.0f; float half_h = viewports_area_h / 2.0f; Vec3 p[4] = { {0, menu_bar_h}, {half_w, menu_bar_h}, {0, menu_bar_h + half_h}, {half_w, menu_bar_h + half_h} }; const char* vp_names[] = { "Perspective", "Top (X/Z)","Front (X/Y)","Side (Y/Z)" };
    for (int i = 0; i < 4; i++) {
        ViewportType type = (ViewportType)i;
        UI_SetNextWindowPos(p[i].x, p[i].y);
        UI_SetNextWindowSize(half_w, half_h);
        UI_PushStyleVar_WindowPadding(0, 0);
        UI_Begin(vp_names[i], NULL);
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
}
void mat4_decompose(const Mat4* matrix, Vec3* translation, Vec3* rotation, Vec3* scale) {
    translation->x = matrix->m[12];
    translation->y = matrix->m[13];
    translation->z = matrix->m[14];
    Vec3 row0 = { matrix->m[0], matrix->m[1], matrix->m[2] };
    Vec3 row1 = { matrix->m[4], matrix->m[5], matrix->m[6] };
    Vec3 row2 = { matrix->m[8], matrix->m[9], matrix->m[10] };
    scale->x = vec3_length(row0);
    scale->y = vec3_length(row1);
    scale->z = vec3_length(row2);
    Mat4 rot_mat;
    memcpy(&rot_mat, matrix, sizeof(Mat4));
    if (fabs(scale->x) < 1e-6 || fabs(scale->y) < 1e-6 || fabs(scale->z) < 1e-6) {
        rotation->x = rotation->y = rotation->z = 0.0f;
        return;
    }
    rot_mat.m[0] /= scale->x; rot_mat.m[1] /= scale->x; rot_mat.m[2] /= scale->x;
    rot_mat.m[4] /= scale->y; rot_mat.m[5] /= scale->y; rot_mat.m[6] /= scale->y;
    rot_mat.m[8] /= scale->z; rot_mat.m[9] /= scale->z; rot_mat.m[10] /= scale->z;
    float sy = sqrtf(rot_mat.m[0] * rot_mat.m[0] + rot_mat.m[4] * rot_mat.m[4]);
    bool singular = sy < 1e-6;
    float x_rad, y_rad, z_rad;
    if (!singular) {
        x_rad = atan2f(rot_mat.m[9], rot_mat.m[10]);
        y_rad = atan2f(-rot_mat.m[8], sy);
        z_rad = atan2f(rot_mat.m[4], rot_mat.m[0]);
    }
    else {
        x_rad = atan2f(-rot_mat.m[6], rot_mat.m[5]);
        y_rad = atan2f(-rot_mat.m[8], sy);
        z_rad = 0;
    }
    const float rad_to_deg = 180.0f / 3.1415926535f;
    rotation->x = x_rad * rad_to_deg;
    rotation->y = y_rad * rad_to_deg;
    rotation->z = z_rad * rad_to_deg;
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
static void RenderSceneForBaking(GLuint shader, Scene* scene, Renderer* renderer, Mat4 view, Mat4 projection) {
    glEnable(GL_DEPTH_TEST); glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, projection.m);
    glUniform1i(glGetUniformLocation(shader, "is_unlit"), 0); Mat4 invView;
    mat4_inverse(&view, &invView); Vec3 probePos = { invView.m[12], invView.m[13], invView.m[14] };
    glUniform3fv(glGetUniformLocation(shader, "viewPos"), 1, &probePos.x);
    glUniform1i(glGetUniformLocation(shader, "sun.enabled"), scene->sun.enabled);
    glUniform3fv(glGetUniformLocation(shader, "sun.direction"), 1, &scene->sun.direction.x);
    glUniform3fv(glGetUniformLocation(shader, "sun.color"), 1, &scene->sun.color.x);
    glUniform1f(glGetUniformLocation(shader, "sun.intensity"), scene->sun.intensity);
    glUniform1i(glGetUniformLocation(shader, "numLights"), scene->numActiveLights);
    char uName[64]; int point_light_shadow_idx = 0; int spot_light_shadow_idx = 0;
    for (int i = 0; i < scene->numActiveLights; ++i) {
        sprintf(uName, "lights[%d].type", i); glUniform1i(glGetUniformLocation(shader, uName), scene->lights[i].type);
        sprintf(uName, "lights[%d].position", i); glUniform3fv(glGetUniformLocation(shader, uName), 1, &scene->lights[i].position.x);
        sprintf(uName, "lights[%d].direction", i); glUniform3fv(glGetUniformLocation(shader, uName), 1, &scene->lights[i].direction.x);
        sprintf(uName, "lights[%d].color", i); glUniform3fv(glGetUniformLocation(shader, uName), 1, &scene->lights[i].color.x);
        sprintf(uName, "lights[%d].intensity", i); glUniform1f(glGetUniformLocation(shader, uName), scene->lights[i].intensity);
        sprintf(uName, "lights[%d].radius", i); glUniform1f(glGetUniformLocation(shader, uName), scene->lights[i].radius);
        sprintf(uName, "lights[%d].cutOff", i); glUniform1f(glGetUniformLocation(shader, uName), scene->lights[i].cutOff);
        sprintf(uName, "lights[%d].outerCutOff", i); glUniform1f(glGetUniformLocation(shader, uName), scene->lights[i].outerCutOff);
        sprintf(uName, "lights[%d].shadowFarPlane", i); glUniform1f(glGetUniformLocation(shader, uName), scene->lights[i].shadowFarPlane);
        sprintf(uName, "lights[%d].shadowBias", i); glUniform1f(glGetUniformLocation(shader, uName), scene->lights[i].shadowBias);
        int current_shadow_idx = -1; Mat4 lightSpaceMatrix; mat4_identity(&lightSpaceMatrix);
        if (scene->lights[i].type == LIGHT_SPOT) {
            if (spot_light_shadow_idx < MAX_LIGHTS) {
                current_shadow_idx = spot_light_shadow_idx;
                float angle_rad = acosf(fmaxf(-1.0f, fminf(1.0f, scene->lights[i].cutOff))); if (angle_rad < 0.01f) angle_rad = 0.01f;
                Mat4 lightProjection = mat4_perspective(angle_rad * 2.0f, 1.0f, 1.0f, scene->lights[i].shadowFarPlane);
                Mat4 lightView = mat4_lookAt(scene->lights[i].position, vec3_add(scene->lights[i].position, scene->lights[i].direction), (Vec3) { 0, 1, 0 });
                mat4_multiply(&lightSpaceMatrix, &lightProjection, &lightView); spot_light_shadow_idx++;
            }
        }
        else { if (point_light_shadow_idx < MAX_LIGHTS) { current_shadow_idx = point_light_shadow_idx; point_light_shadow_idx++; } }
        sprintf(uName, "lightSpaceMatrices[%d]", i); glUniformMatrix4fv(glGetUniformLocation(shader, uName), 1, GL_FALSE, lightSpaceMatrix.m);
        sprintf(uName, "lights[%d].shadowMapIndex", i); glUniform1i(glGetUniformLocation(shader, uName), current_shadow_idx);
    }
    point_light_shadow_idx = 0; spot_light_shadow_idx = 0;
    for (int i = 0; i < scene->numActiveLights; ++i) {
        if (scene->lights[i].type == LIGHT_POINT) { if (point_light_shadow_idx < MAX_LIGHTS) { glActiveTexture(GL_TEXTURE4 + point_light_shadow_idx); glBindTexture(GL_TEXTURE_CUBE_MAP, scene->lights[i].shadowMapTexture); point_light_shadow_idx++; } }
        else { if (spot_light_shadow_idx < MAX_LIGHTS) { glActiveTexture(GL_TEXTURE4 + MAX_LIGHTS + spot_light_shadow_idx); glBindTexture(GL_TEXTURE_2D, scene->lights[i].shadowMapTexture); spot_light_shadow_idx++; } }
    }
    glUniform1i(glGetUniformLocation(shader, "useEnvironmentMap"), 0); glUniform1i(glGetUniformLocation(shader, "useParallaxCorrection"), 0);
    for (int i = 0; i < scene->numObjects; i++) { render_object(shader, &scene->objects[i]); }
    for (int i = 0; i < scene->numBrushes; i++) { render_brush(shader, &scene->brushes[i]); }
    glDepthFunc(GL_LEQUAL); glUseProgram(renderer->skyboxShader);
    Mat4 skyboxView = view; skyboxView.m[12] = skyboxView.m[13] = skyboxView.m[14] = 0;
    glUniformMatrix4fv(glGetUniformLocation(renderer->skyboxShader, "view"), 1, GL_FALSE, skyboxView.m);
    glUniformMatrix4fv(glGetUniformLocation(renderer->skyboxShader, "projection"), 1, GL_FALSE, projection.m);
    glBindVertexArray(renderer->skyboxVAO); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_CUBE_MAP, renderer->skyboxTex);
    glDrawArrays(GL_TRIANGLES, 0, 36); glDepthFunc(GL_LESS);
}
void Editor_BuildCubemaps(Scene* scene, Renderer* renderer, Engine* engine) {
    Console_Printf("Starting cubemap build...");
#ifdef _WIN32
    _mkdir("cubemaps");
#else
    mkdir("cubemaps", 0777);
#endif
    const int CUBEMAP_RES = 128; GLuint fbo, rbo;
    glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenRenderbuffers(1, &rbo); glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, CUBEMAP_RES, CUBEMAP_RES);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    glEnable(GL_DEPTH_TEST); glCullFace(GL_BACK);
    Mat4 captureProjection = mat4_perspective(90.0f * (3.14159f / 180.0f), 1.0f, 0.1f, 1000.f);
    Mat4 captureViews[6]; GLuint bakingShader = renderer->mainShader;
    glUseProgram(bakingShader);
    glUniform1i(glGetUniformLocation(bakingShader, "diffuseMap"), 0); glUniform1i(glGetUniformLocation(bakingShader, "normalMap"), 1); glUniform1i(glGetUniformLocation(bakingShader, "specularMap"), 2);
    char uName[32];
    for (int i = 0; i < MAX_LIGHTS; ++i) { sprintf(uName, "pointShadowMaps[%d]", i); glUniform1i(glGetUniformLocation(bakingShader, uName), 4 + i); }
    for (int i = 0; i < MAX_LIGHTS; ++i) { sprintf(uName, "spotShadowMaps[%d]", i); glUniform1i(glGetUniformLocation(bakingShader, uName), 4 + MAX_LIGHTS + i); }
    unsigned char* pixels = malloc(CUBEMAP_RES * CUBEMAP_RES * 4);
    if (!pixels) { Console_Printf("[error] Failed to allocate memory for cubemap pixels."); glDeleteFramebuffers(1, &fbo); glDeleteRenderbuffers(1, &rbo); return; }
    GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
    for (int i = 0; i < scene->numBrushes; ++i) {
        Brush* probe_brush = &scene->brushes[i];
        if (!probe_brush->isReflectionProbe) continue;

        Console_Printf("Building cubemap for %s...", probe_brush->name);
        GLuint temp_cubemap_tex; glGenTextures(1, &temp_cubemap_tex); glBindTexture(GL_TEXTURE_CUBE_MAP, temp_cubemap_tex);
        for (int j = 0; j < 6; ++j) { glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_RGBA8, CUBEMAP_RES, CUBEMAP_RES, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL); }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        captureViews[0] = mat4_lookAt(probe_brush->pos, vec3_add(probe_brush->pos, (Vec3) { 1.0f, 0.0f, 0.0f }), (Vec3) { 0.0f, -1.0f, 0.0f });
        captureViews[1] = mat4_lookAt(probe_brush->pos, vec3_add(probe_brush->pos, (Vec3) { -1.0f, 0.0f, 0.0f }), (Vec3) { 0.0f, -1.0f, 0.0f });
        captureViews[2] = mat4_lookAt(probe_brush->pos, vec3_add(probe_brush->pos, (Vec3) { 0.0f, 1.0f, 0.0f }), (Vec3) { 0.0f, 0.0f, 1.0f });
        captureViews[3] = mat4_lookAt(probe_brush->pos, vec3_add(probe_brush->pos, (Vec3) { 0.0f, -1.0f, 0.0f }), (Vec3) { 0.0f, 0.0f, -1.0f });
        captureViews[4] = mat4_lookAt(probe_brush->pos, vec3_add(probe_brush->pos, (Vec3) { 0.0f, 0.0f, 1.0f }), (Vec3) { 0.0f, -1.0f, 0.0f });
        captureViews[5] = mat4_lookAt(probe_brush->pos, vec3_add(probe_brush->pos, (Vec3) { 0.0f, 0.0f, -1.0f }), (Vec3) { 0.0f, -1.0f, 0.0f });
        glViewport(0, 0, CUBEMAP_RES, CUBEMAP_RES); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        for (int j = 0; j < 6; j++) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, temp_cubemap_tex, 0);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderSceneForBaking(bakingShader, scene, renderer, captureViews[j], captureProjection);
        }
        const char* face_suffixes[] = { "px", "nx", "py", "ny", "pz", "nz" };
        for (int j = 0; j < 6; ++j) {
            char filename[256];
            sprintf(filename, "cubemaps/%s_%s.png", probe_brush->name, face_suffixes[j]);
            glBindTexture(GL_TEXTURE_CUBE_MAP, temp_cubemap_tex); glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, CUBEMAP_RES, CUBEMAP_RES, 32, CUBEMAP_RES * 4, SDL_PIXELFORMAT_RGBA32);
            if (surface) { IMG_SavePNG(surface, filename); SDL_FreeSurface(surface); }
            else { Console_Printf("[error] Failed to create SDL_Surface for saving %s", filename); }
        }
        glDeleteTextures(1, &temp_cubemap_tex);
        const char* faces_suffixes[] = { "px", "nx", "py", "ny", "pz", "nz" };
        char face_paths[6][256];
        for (int j = 0; j < 6; ++j) {
            sprintf(face_paths[j], "cubemaps/%s_%s.png", probe_brush->name, faces_suffixes[j]);
        }
        const char* face_pointers[6];
        for (int j = 0; j < 6; ++j) {
            face_pointers[j] = face_paths[j];
        }

        probe_brush->cubemapTexture = TextureManager_ReloadCubemap(face_pointers, probe_brush->cubemapTexture);

        Console_Printf("...reloaded '%s' for instant preview.", probe_brush->name);
    }
    free(pixels); glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo); glDeleteRenderbuffers(1, &rbo);
    glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
    glUseProgram(renderer->mainShader); glUniform1i(glGetUniformLocation(bakingShader, "is_unlit"), 0);
    Console_Printf("Cubemap build finished.");
}