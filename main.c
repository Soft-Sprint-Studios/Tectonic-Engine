/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "cvar.h"
#include "gl_console.h"
#include "gl_misc.h"
#include "map.h"
#include "physics_wrapper.h"
#include "editor.h"
#include <stdlib.h>
#include <float.h>
#include "sound_system.h"
#include "io_system.h"
#include "binds.h"
#include "gameconfig.h"
#include "discord_wrapper.h"
#include "main_menu.h"
#include "network.h"
#include "dsp_reverb.h"
#include "video_player.h"
#ifdef PLATFORM_LINUX
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifdef PLATFORM_WINDOWS
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 0x00000001;
#endif

#define SUN_SHADOW_MAP_SIZE 4096
#define PLAYER_JUMP_FORCE 350.0f

#define BLOOM_DOWNSAMPLE 8
#define SSAO_DOWNSAMPLE 2
#define VOLUMETRIC_DOWNSAMPLE 4

static void SaveFramebufferToPNG(GLuint fbo, int width, int height, const char* filepath);
static void BuildCubemaps();

static void init_renderer(void);
static void init_scene(void);

typedef enum { MODE_GAME, MODE_EDITOR, MODE_MAINMENU, MODE_INGAMEMENU } EngineMode;

static Engine g_engine_instance;
static Engine* g_engine = &g_engine_instance;
static Renderer g_renderer;
static Scene g_scene;
static EngineMode g_current_mode = MODE_GAME;

static Uint32 g_fps_last_update = 0;
static int g_fps_frame_count = 0;
static float g_fps_display = 0.0f;

static unsigned int g_flashlight_sound_buffer = 0;
static unsigned int g_footstep_sound_buffer = 0;
static Vec3 g_last_player_pos = { 0.0f, 0.0f, 0.0f };
static float g_distance_walked = 0.0f;
const float FOOTSTEP_DISTANCE = 2.0f;
static int g_current_reverb_zone_index = -1;
static int g_last_vsync_cvar_state = -1;

float quadVertices[] = { -1.0f,1.0f,0.0f,1.0f,-1.0f,-1.0f,0.0f,0.0f,1.0f,-1.0f,1.0f,0.0f,-1.0f,1.0f,0.0f,1.0f,1.0f,-1.0f,1.0f,0.0f,1.0f,1.0f,1.0f,1.0f };
float parallaxRoomVertices[] = {
    -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f,
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   1.0f, 0.0f, 0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 0.0f,   1.0f, 0.0f, 0.0f, 0.0f,

    -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f,
     0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 0.0f,   1.0f, 0.0f, 0.0f, 0.0f,
     0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f,   1.0f, 0.0f, 0.0f, 0.0f
};
float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};
float decalQuadVertices[] = { -0.5f,-0.5f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.5f,-0.5f,0.0f,0.0f,0.0f,1.0f,1.0f,0.0f,1.0f,0.0f,0.0f,0.5f,0.5f,0.0f,0.0f,0.0f,1.0f,1.0f,1.0f,1.0f,0.0f,0.0f,0.5f,0.5f,0.0f,0.0f,0.0f,1.0f,1.0f,1.0f,1.0f,0.0f,0.0f,-0.5f,0.5f,0.0f,0.0f,0.0f,1.0f,0.0f,1.0f,1.0f,0.0f,0.0f,-0.5f,-0.5f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,1.0f,0.0f,0.0f };

static int FindReflectionProbeForPoint(Vec3 p) {
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (!b->isReflectionProbe) continue;

        if (b->numVertices == 0 || b->vertices == NULL) continue;

        Vec3 min_aabb_world = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 max_aabb_world = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        for (int j = 0; j < b->numVertices; ++j) {
            Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
            min_aabb_world.x = fminf(min_aabb_world.x, world_v.x);
            min_aabb_world.y = fminf(min_aabb_world.y, world_v.y);
            min_aabb_world.z = fminf(min_aabb_world.z, world_v.z);
            max_aabb_world.x = fmaxf(max_aabb_world.x, world_v.x);
            max_aabb_world.y = fmaxf(max_aabb_world.y, world_v.y);
            max_aabb_world.z = fmaxf(max_aabb_world.z, world_v.z);
        }

        if (p.x >= min_aabb_world.x && p.x <= max_aabb_world.x &&
            p.y >= min_aabb_world.y && p.y <= max_aabb_world.y &&
            p.z >= min_aabb_world.z && p.z <= max_aabb_world.z)
        {
            return i;
        }
    }
    return -1;
}

void render_object(GLuint shader, SceneObject* obj, bool is_baking_pass, const Frustum* frustum) {
    bool envMapEnabled = false;

    if (!is_baking_pass && shader == g_renderer.mainShader) {
        int reflection_brush_idx = FindReflectionProbeForPoint(obj->pos);
        if (reflection_brush_idx != -1) {
            Brush* reflection_brush = &g_scene.brushes[reflection_brush_idx];
            if (reflection_brush->cubemapTexture != 0) {
                glActiveTexture(GL_TEXTURE10);
                glBindTexture(GL_TEXTURE_CUBE_MAP, reflection_brush->cubemapTexture);
                glUniform1i(glGetUniformLocation(shader, "environmentMap"), 10);
                glUniform1i(glGetUniformLocation(shader, "useParallaxCorrection"), 1);

                Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
                Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                for (int i = 0; i < reflection_brush->numVertices; ++i) {
                    Vec3 world_v = mat4_mul_vec3(&reflection_brush->modelMatrix, reflection_brush->vertices[i].pos);
                    min_aabb.x = fminf(min_aabb.x, world_v.x); min_aabb.y = fminf(min_aabb.y, world_v.y); min_aabb.z = fminf(min_aabb.z, world_v.z);
                    max_aabb.x = fmaxf(max_aabb.x, world_v.x); max_aabb.y = fmaxf(max_aabb.y, world_v.y); max_aabb.z = fmaxf(max_aabb.z, world_v.z);
                }
                glUniform3fv(glGetUniformLocation(shader, "probeBoxMin"), 1, &min_aabb.x);
                glUniform3fv(glGetUniformLocation(shader, "probeBoxMax"), 1, &max_aabb.x);
                glUniform3fv(glGetUniformLocation(shader, "probePosition"), 1, &reflection_brush->pos.x);
                envMapEnabled = true;
            }
        }
    }

    glUniform1i(glGetUniformLocation(shader, "useEnvironmentMap"), envMapEnabled);

    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, obj->modelMatrix.m);
    if (obj->model) {
        for (int i = 0; i < obj->model->meshCount; ++i) {
            Mesh* mesh = &obj->model->meshes[i];
            Material* material = mesh->material;
            if (shader == g_renderer.mainShader || shader == g_renderer.vplGenerationShader) {
                float finalHeightScale = Cvar_GetInt("r_parallax_mapping") ? material->heightScale : 0.0f;
                glUniform1f(glGetUniformLocation(shader, "heightScale"), finalHeightScale);
                glUniform1f(glGetUniformLocation(shader, "heightScale"), material->heightScale);
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, material->diffuseMap);
                glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, material->normalMap);
                glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, material->rmaMap);
                glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, material->heightMap);
                glUniform1f(glGetUniformLocation(shader, "detailScale"), material->detailScale);
                glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, material->detailDiffuseMap);
            }
            glBindVertexArray(mesh->VAO);
            if (shader == g_renderer.mainShader) {
                if (mesh->useEBO) { glDrawElements(GL_PATCHES, mesh->indexCount, GL_UNSIGNED_INT, 0); }
                else { glDrawArrays(GL_PATCHES, 0, mesh->indexCount); }
            }
            else {
                if (mesh->useEBO) { glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0); }
                else { glDrawArrays(GL_TRIANGLES, 0, mesh->indexCount); }
            }
        }
    }
}

void render_brush(GLuint shader, Brush* b, bool is_baking_pass, const Frustum* frustum) {
    if (b->isReflectionProbe || b->isTrigger || b->isWater) return;
    bool envMapEnabled = false;

    if (!is_baking_pass && shader == g_renderer.mainShader) {
        int reflection_brush_idx = FindReflectionProbeForPoint(b->pos);
        if (reflection_brush_idx != -1) {
            Brush* reflection_brush = &g_scene.brushes[reflection_brush_idx];
            if (reflection_brush->cubemapTexture != 0) {
                glActiveTexture(GL_TEXTURE10);
                glBindTexture(GL_TEXTURE_CUBE_MAP, reflection_brush->cubemapTexture);
                glUniform1i(glGetUniformLocation(shader, "environmentMap"), 10);
                glUniform1i(glGetUniformLocation(shader, "useParallaxCorrection"), 1);
                Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
                Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                for (int i = 0; i < reflection_brush->numVertices; ++i) {
                    Vec3 world_v = mat4_mul_vec3(&reflection_brush->modelMatrix, reflection_brush->vertices[i].pos);
                    min_aabb.x = fminf(min_aabb.x, world_v.x); min_aabb.y = fminf(min_aabb.y, world_v.y); min_aabb.z = fminf(min_aabb.z, world_v.z);
                    max_aabb.x = fmaxf(max_aabb.x, world_v.x); max_aabb.y = fmaxf(max_aabb.y, world_v.y); max_aabb.z = fmaxf(max_aabb.z, world_v.z);
                }
                glUniform3fv(glGetUniformLocation(shader, "probeBoxMin"), 1, &min_aabb.x);
                glUniform3fv(glGetUniformLocation(shader, "probeBoxMax"), 1, &max_aabb.x);
                glUniform3fv(glGetUniformLocation(shader, "probePosition"), 1, &reflection_brush->pos.x);
                envMapEnabled = true;
            }
        }
    }
    glUniform1i(glGetUniformLocation(shader, "useEnvironmentMap"), envMapEnabled);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, b->modelMatrix.m);

    glBindVertexArray(b->vao);
    if (shader == g_renderer.mainShader || shader == g_renderer.vplGenerationShader) {
        int vbo_offset = 0;
        for (int i = 0; i < b->numFaces; ++i) {
            Material* material = TextureManager_FindMaterial(b->faces[i].material->name);
            float parallax_enabled = Cvar_GetInt("r_parallax_mapping") ? material->heightScale : 0.0f;
            glUniform1f(glGetUniformLocation(shader, "heightScale"), parallax_enabled);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, material->diffuseMap);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, material->normalMap);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, material->rmaMap);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, material->heightMap);
            glUniform1f(glGetUniformLocation(shader, "detailScale"), material->detailScale);
            glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, material->detailDiffuseMap);

            Material* material2 = b->faces[i].material2 ? TextureManager_FindMaterial(b->faces[i].material2->name) : NULL;
            if (material2) {
                glUniform1i(glGetUniformLocation(shader, "diffuseMap2"), 12);
                glUniform1i(glGetUniformLocation(shader, "normalMap2"), 13);
                glUniform1i(glGetUniformLocation(shader, "rmaMap2"), 14);
                glUniform1i(glGetUniformLocation(shader, "heightMap2"), 15);
                glUniform1f(glGetUniformLocation(shader, "heightScale2"), parallax_enabled ? material2->heightScale : 0.0f);
                glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, material2->diffuseMap);
                glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, material2->normalMap);
                glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, material2->rmaMap);
                glActiveTexture(GL_TEXTURE15); glBindTexture(GL_TEXTURE_2D, material2->heightMap);
            }
            else {
                glUniform1f(glGetUniformLocation(shader, "heightScale2"), 0.0f);
                glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, missingTextureID);
                glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, defaultNormalMapID);
                glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, defaultRmaMapID);
                glActiveTexture(GL_TEXTURE15); glBindTexture(GL_TEXTURE_2D, 0);
            }

            Material* material3 = b->faces[i].material3 ? TextureManager_FindMaterial(b->faces[i].material3->name) : NULL;
            if (material3) {
                glUniform1i(glGetUniformLocation(shader, "diffuseMap3"), 17);
                glUniform1i(glGetUniformLocation(shader, "normalMap3"), 18);
                glUniform1i(glGetUniformLocation(shader, "rmaMap3"), 19);
                glUniform1i(glGetUniformLocation(shader, "heightMap3"), 20);
                glUniform1f(glGetUniformLocation(shader, "heightScale3"), parallax_enabled ? material3->heightScale : 0.0f);
                glActiveTexture(GL_TEXTURE17); glBindTexture(GL_TEXTURE_2D, material3->diffuseMap);
                glActiveTexture(GL_TEXTURE18); glBindTexture(GL_TEXTURE_2D, material3->normalMap);
                glActiveTexture(GL_TEXTURE19); glBindTexture(GL_TEXTURE_2D, material3->rmaMap);
                glActiveTexture(GL_TEXTURE20); glBindTexture(GL_TEXTURE_2D, material3->heightMap);
            }
            else {
                glUniform1f(glGetUniformLocation(shader, "heightScale3"), 0.0f);
                glActiveTexture(GL_TEXTURE17); glBindTexture(GL_TEXTURE_2D, missingTextureID);
                glActiveTexture(GL_TEXTURE18); glBindTexture(GL_TEXTURE_2D, defaultNormalMapID);
                glActiveTexture(GL_TEXTURE19); glBindTexture(GL_TEXTURE_2D, defaultRmaMapID);
                glActiveTexture(GL_TEXTURE20); glBindTexture(GL_TEXTURE_2D, 0);
            }

            Material* material4 = b->faces[i].material4 ? TextureManager_FindMaterial(b->faces[i].material4->name) : NULL;
            if (material4) {
                glUniform1i(glGetUniformLocation(shader, "diffuseMap4"), 21);
                glUniform1i(glGetUniformLocation(shader, "normalMap4"), 22);
                glUniform1i(glGetUniformLocation(shader, "rmaMap4"), 23);
                glUniform1i(glGetUniformLocation(shader, "heightMap4"), 24);
                glUniform1f(glGetUniformLocation(shader, "heightScale4"), parallax_enabled ? material4->heightScale : 0.0f);
                glActiveTexture(GL_TEXTURE21); glBindTexture(GL_TEXTURE_2D, material4->diffuseMap);
                glActiveTexture(GL_TEXTURE22); glBindTexture(GL_TEXTURE_2D, material4->normalMap);
                glActiveTexture(GL_TEXTURE23); glBindTexture(GL_TEXTURE_2D, material4->rmaMap);
                glActiveTexture(GL_TEXTURE24); glBindTexture(GL_TEXTURE_2D, material4->heightMap);
            }
            else {
                glUniform1f(glGetUniformLocation(shader, "heightScale4"), 0.0f);
                glActiveTexture(GL_TEXTURE21); glBindTexture(GL_TEXTURE_2D, missingTextureID);
                glActiveTexture(GL_TEXTURE22); glBindTexture(GL_TEXTURE_2D, defaultNormalMapID);
                glActiveTexture(GL_TEXTURE23); glBindTexture(GL_TEXTURE_2D, defaultRmaMapID);
                glActiveTexture(GL_TEXTURE24); glBindTexture(GL_TEXTURE_2D, 0);
            }

            int num_face_verts = (b->faces[i].numVertexIndices - 2) * 3;
            if (shader == g_renderer.mainShader) {
                glDrawArrays(GL_PATCHES, vbo_offset, num_face_verts);
            }
            else {
                glDrawArrays(GL_TRIANGLES, vbo_offset, num_face_verts);
            }
            vbo_offset += num_face_verts;
        }
    }
    else {
        if (shader == g_renderer.mainShader) {
            glDrawArrays(GL_PATCHES, 0, b->totalRenderVertexCount);
        }
        else {
            glDrawArrays(GL_TRIANGLES, 0, b->totalRenderVertexCount);
        }
    }
}

void handle_command(int argc, char** argv) {
    if (argc == 0) return;
    char* cmd = argv[0];
    if (_stricmp(cmd, "edit") == 0) {
        if (g_current_mode == MODE_GAME) {
            g_current_mode = MODE_EDITOR; SDL_SetRelativeMouseMode(SDL_FALSE); Editor_Init(g_engine, &g_renderer, &g_scene);
        }
        else { g_current_mode = MODE_GAME; Editor_Shutdown(); SDL_SetRelativeMouseMode(SDL_TRUE); }
    }
    else if (_stricmp(cmd, "quit") == 0 || _stricmp(cmd, "exit") == 0) Cvar_EngineSet("engine_running", "0");
    else if (_stricmp(cmd, "setpos") == 0) {
        if (argc == 4) {
            float x = atof(argv[1]);
            float y = atof(argv[2]);
            float z = atof(argv[3]);
            Vec3 new_pos = { x, y, z };

            if (g_engine->camera.physicsBody) {
                Physics_Teleport(g_engine->camera.physicsBody, new_pos);
            }
            g_engine->camera.position = new_pos;

            Console_Printf("Teleported to %.2f, %.2f, %.2f", x, y, z);
        }
        else {
            Console_Printf("Usage: setpos <x> <y> <z>");
        }
    }
    else if (_stricmp(cmd, "noclip") == 0) {
        Cvar* c = Cvar_Find("noclip");
        if (c) {
            bool currently_noclip = c->intValue;
            Cvar_Set("noclip", currently_noclip ? "0" : "1"); Console_Printf("noclip %s", Cvar_GetString("noclip"));
            if (currently_noclip) { Physics_Teleport(g_engine->camera.physicsBody, g_engine->camera.position); }
        }
    }
    else if (_stricmp(cmd, "bind") == 0) {
        if (argc == 3) {
            Binds_Set(argv[1], argv[2]);
        }
        else {
            Console_Printf("Usage: bind \"key\" \"command\"");
        }
    }
    else if (_stricmp(cmd, "map") == 0) {
        if (argc == 2) {
            g_current_mode = MODE_MAINMENU;
            SDL_SetRelativeMouseMode(SDL_FALSE);

            char map_path[256];
            snprintf(map_path, sizeof(map_path), "%s.map", argv[1]);

            Console_Printf("Loading map: %s", map_path);
            if (Scene_LoadMap(&g_scene, &g_renderer, map_path, g_engine)) {
                g_current_mode = MODE_GAME;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            else {
                Console_Printf("[error] Failed to load map: %s", map_path);
            }
        }
        else {
            Console_Printf("Usage: map <mapname>");
        }
    }
    else if (_stricmp(cmd, "maps") == 0) {
        const char* dir_path = "./";
        Console_Printf("Available maps in root directory:");

#ifdef PLATFORM_WINDOWS
        char search_path[256];
        sprintf(search_path, "%s*.map", dir_path);
        WIN32_FIND_DATAA find_data;
        HANDLE h_find = FindFirstFileA(search_path, &find_data);
        if (h_find == INVALID_HANDLE_VALUE) {
            Console_Printf("...No maps found.");
        }
        else {
            do {
                if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    Console_Printf("  %s", find_data.cFileName);
                }
            } while (FindNextFileA(h_find, &find_data) != 0);
            FindClose(h_find);
        }
#else
        DIR* d = opendir(dir_path);
        if (!d) {
            Console_Printf("...Could not open directory.");
        }
        else {
            struct dirent* dir;
            int count = 0;
            while ((dir = readdir(d)) != NULL) {
                const char* ext = strrchr(dir->d_name, '.');
                if (ext && (_stricmp(ext, ".map") == 0)) {
                    Console_Printf("  %s", dir->d_name);
                    count++;
                }
            }
            if (count == 0) {
                Console_Printf("...No maps found.");
            }
            closedir(d);
        }
#endif
    }
    else if (_stricmp(cmd, "disconnect") == 0) {
        if (g_current_mode == MODE_GAME || g_current_mode == MODE_EDITOR) {
            Console_Printf("Disconnecting from map...");
            g_current_mode = MODE_MAINMENU;

            SDL_SetRelativeMouseMode(SDL_FALSE);

            if (g_is_editor_mode) {
                Editor_Shutdown();
            }

            Scene_Clear(&g_scene, g_engine);
            MainMenu_SetInGameMenuMode(false, false);

        }
        else {
            Console_Printf("Not currently in a map.");
        }
    }
    else if (_stricmp(cmd, "download") == 0) {
        if (argc == 2 && strncmp(argv[1], "http", 4) == 0) {
            const char* url = argv[1];
            const char* filename_start = strrchr(url, '/');
            if (filename_start) {
                filename_start++;
            }
            else {
                filename_start = url;
            }

            _mkdir("downloads");

            char output_path[256];
            snprintf(output_path, sizeof(output_path), "downloads/%s", filename_start);

            Console_Printf("Starting download for %s...", url);
            Network_DownloadFile(url, output_path);
        }
        else {
            Console_Printf("Usage: download http://... or https://...");
        }
    }
    else if (_stricmp(cmd, "ping") == 0) {
        if (argc == 2) {
            Console_Printf("Pinging %s...", argv[1]);
            Network_Ping(argv[1]);
        }
        else {
            Console_Printf("Usage: ping <hostname>");
        }
    }
    else if (_stricmp(cmd, "cvarlist") == 0) {
        Console_Printf("--- CVAR List ---");
        for (int i = 0; i < num_cvars; i++) {
            Cvar* c = &cvar_list[i];
            if (c->flags & CVAR_HIDDEN) {
                continue;
            }
            Console_Printf("%s - %s (current: \"%s\")", c->name, c->helpText, c->stringValue);
        }
        Console_Printf("-----------------");
    }
    else if (_stricmp(cmd, "build_cubemaps") == 0) {
        BuildCubemaps();
    }
    else if (argc >= 2) { if (Cvar_Find(cmd)) Cvar_Set(cmd, argv[1]); else Console_Printf("[error] Unknown cvar: %s", cmd); }
    else if (argc == 1) {
        Cvar* c = Cvar_Find(cmd);
        if (c && !(c->flags & CVAR_HIDDEN)) {
            Console_Printf("%s = %s // %s", c->name, c->stringValue, c->helpText);
        }
        else {
            Console_Printf("[error] Unknown cvar: %s", cmd);
        }
    }
}

void init_engine(SDL_Window* window, SDL_GLContext context) {
    g_engine->window = window; g_engine->context = context; g_engine->running = true; g_engine->deltaTime = 0.0f; g_engine->lastFrame = 0.0f;
    g_engine->camera = (Camera){ {0,1,5}, 0,0, false, PLAYER_HEIGHT_NORMAL, NULL };  g_engine->flashlight_on = false;
    GameConfig_Init();
    UI_Init(window, context);
    SoundSystem_Init();
    Cvar_Init();
    Cvar_Register("volume", "2.5", "Master volume for the game (0.0 to 4.0)", CVAR_NONE);
    Cvar_Register("r_vpl_count", "64", "Number of VPLs to generate per light.", CVAR_NONE);
    Cvar_Register("noclip", "0", "", CVAR_NONE);
    Cvar_Register("gravity", "9.81", "", CVAR_NONE);
    Cvar_Register("engine_running", "1", "", CVAR_HIDDEN);
    Cvar_Register("r_autoexposure", "1", "Enable auto-exposure (tonemapping).", CVAR_NONE);
    Cvar_Register("r_autoexposure_speed", "1.0", "Adaptation speed for auto-exposure.", CVAR_NONE);
    Cvar_Register("r_autoexposure_key", "0.1", "The middle-grey value the scene luminance will adapt towards.", CVAR_NONE);
    Cvar_Register("r_ssao", "1", "Enable Screen-Space Ambient Occlusion.", CVAR_NONE);
    Cvar_Register("r_bloom", "1", "Enable or disable the bloom effect.", CVAR_NONE);
    Cvar_Register("r_volumetrics", "1", "Enable or disable volumetric lighting.", CVAR_NONE);
    Cvar_Register("r_depth_aa", "1", "Enable Depth/Normal based Anti-Aliasing.", CVAR_NONE);
    Cvar_Register("r_faceculling", "1", "Enable back-face culling for main render pass. (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_wireframe", "0", "Render geometry in wireframe mode. (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_shadows", "1", "Master switch for all dynamic shadows. (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_vpl", "1", "Master switch for Virtual Point Light Global Illumination. (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_vpl_static", "0", "Generate VPLs only once on map load for static GI. (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_shadow_map_size", "1024", "Resolution for point/spot light shadow maps (e.g., 512, 1024, 2048).", CVAR_NONE);
    Cvar_Register("r_parallax_mapping", "1", "Enable parallax mapping. (0=off, 1=on)", CVAR_NONE);
    Cvar_Register("r_vsync", "0", "Enable or disable vertical sync (0=off, 1=on).", CVAR_NONE);
    Cvar_Register("fps_max", "300", "Maximum frames per second. 0 for unlimited. VSync overrides this.", CVAR_NONE);
    Cvar_Register("show_fps", "0", "Show FPS counter in the top-left corner.", CVAR_NONE);
    Cvar_Register("show_pos", "0", "Show player position in the top-left corner.", CVAR_NONE);
    Cvar_Register("r_debug_albedo", "0", "Show G-Buffer albedo.", CVAR_NONE);
    Cvar_Register("r_debug_normals", "0", "Show G-Buffer view-space normals.", CVAR_NONE);
    Cvar_Register("r_debug_position", "0", "Show G-Buffer view-space positions.", CVAR_NONE);
    Cvar_Register("r_debug_metallic", "0", "Show PBR metallic channel.", CVAR_NONE);
    Cvar_Register("r_debug_roughness", "0", "Show PBR roughness channel.", CVAR_NONE);
    Cvar_Register("r_debug_ao", "0", "Show screen-space ambient occlusion buffer.", CVAR_NONE);
    Cvar_Register("r_debug_velocity", "0", "Show motion vector velocity buffer.", CVAR_NONE);
    Cvar_Register("r_debug_volumetric", "0", "Show volumetric lighting buffer.", CVAR_NONE);
    Cvar_Register("r_debug_bloom", "0", "Show the bloom brightness mask texture.", CVAR_NONE);
    Cvar_Register("r_debug_vpl", "0", "Show G-Buffer indirect illumination.", CVAR_NONE);
    Cvar_Register("r_sun_shadow_distance", "50.0", "The orthographic size (radius) for the sun's shadow map frustum. Lower values = sharper shadows closer to the camera.", CVAR_NONE);
    Cvar_Register("r_texture_quality", "5", "Texture quality setting (1=very low, 2=low, 3=medium, 4=high, 5=very high).", CVAR_NONE);
    Cvar_Register("fov_vertical", "55", "The vertical field of view in degrees.", CVAR_NONE);
    Cvar_Register("r_motionblur", "0", "Enable camera and object motion blur.", CVAR_NONE);
    Cvar_Register("g_speed", "6.0", "Player walking speed.", CVAR_NONE);
    Cvar_Register("g_sprint_speed", "8.0", "Player sprinting speed.", CVAR_NONE);
    Cvar_Register("g_accel", "15.0", "Player acceleration.", CVAR_NONE);
    Cvar_Register("g_friction", "5.0", "Player friction.", CVAR_NONE);
    Cvar_Load("cvars.txt");
    IO_Init();
    Binds_Init();
    Network_Init();
    g_flashlight_sound_buffer = SoundSystem_LoadSound("sounds/flashlight01.wav");
    g_footstep_sound_buffer = SoundSystem_LoadSound("sounds/footstep.wav");
    Console_SetCommandHandler(handle_command);
    TextureManager_Init();
    TextureManager_ParseMaterialsFromFile("materials.def");
    VideoPlayer_InitSystem();
    init_renderer();
    init_scene();
    Discord_Init();
    g_current_mode = MODE_MAINMENU;
    if (!MainMenu_Init(WINDOW_WIDTH, WINDOW_HEIGHT)) {
        Console_Printf("[ERROR] Failed to initialize Main Menu.");
        g_engine->running = false;
    }
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

void init_renderer() {
    g_renderer.mainShader = createShaderProgramTess("shaders/main.vert", "shaders/main.tcs", "shaders/main.tes", "shaders/main.frag");
    g_renderer.debugBufferShader = createShaderProgram("shaders/debug_buffer.vert", "shaders/debug_buffer.frag");
    g_renderer.pointDepthShader = createShaderProgramGeom("shaders/depth_point.vert", "shaders/depth_point.geom", "shaders/depth_point.frag");
    g_renderer.vplGenerationShader = createShaderProgram("shaders/vpl_gen.vert", "shaders/vpl_gen.frag");
    g_renderer.vplComputeShader = createShaderProgramCompute("shaders/vpl_compute.comp");
    g_renderer.spotDepthShader = createShaderProgram("shaders/depth_spot.vert", "shaders/depth_spot.frag");
    g_renderer.skyboxShader = createShaderProgram("shaders/skybox.vert", "shaders/skybox.frag");
    g_renderer.postProcessShader = createShaderProgram("shaders/postprocess.vert", "shaders/postprocess.frag");
    g_renderer.histogramShader = createShaderProgramCompute("shaders/histogram.comp");
    g_renderer.exposureShader = createShaderProgramCompute("shaders/exposure.comp");
    g_renderer.bloomShader = createShaderProgram("shaders/bloom.vert", "shaders/bloom.frag");
    g_renderer.bloomBlurShader = createShaderProgram("shaders/bloom_blur.vert", "shaders/bloom_blur.frag");
    g_renderer.dofShader = createShaderProgram("shaders/dof.vert", "shaders/dof.frag");
    g_renderer.volumetricShader = createShaderProgram("shaders/volumetric.vert", "shaders/volumetric.frag");
    g_renderer.volumetricBlurShader = createShaderProgram("shaders/volumetric_blur.vert", "shaders/volumetric_blur.frag");
    g_renderer.depthAaShader = createShaderProgram("shaders/depth_aa.vert", "shaders/depth_aa.frag");
    g_renderer.motionBlurShader = createShaderProgram("shaders/motion_blur.vert", "shaders/motion_blur.frag");
    g_renderer.ssaoShader = createShaderProgram("shaders/ssao.vert", "shaders/ssao.frag");
    g_renderer.ssaoBlurShader = createShaderProgram("shaders/ssao_blur.vert", "shaders/ssao_blur.frag");
    g_renderer.waterShader = createShaderProgramTess("shaders/water.vert", "shaders/water.tcs", "shaders/water.tes", "shaders/water.frag");
    g_renderer.parallaxInteriorShader = createShaderProgram("shaders/parallax_interior.vert", "shaders/parallax_interior.frag");
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    const int LOW_RES_WIDTH = WINDOW_WIDTH / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
    const int LOW_RES_HEIGHT = WINDOW_HEIGHT / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
    glGenFramebuffers(1, &g_renderer.gBufferFBO); glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.gBufferFBO);
    glGenTextures(1, &g_renderer.gLitColor); glBindTexture(GL_TEXTURE_2D, g_renderer.gLitColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_renderer.gLitColor, 0);
    glGenTextures(1, &g_renderer.gPosition); glBindTexture(GL_TEXTURE_2D, g_renderer.gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g_renderer.gPosition, 0);
    glGenTextures(1, &g_renderer.gNormal); glBindTexture(GL_TEXTURE_2D, g_renderer.gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, g_renderer.gNormal, 0);
    glGenTextures(1, &g_renderer.gAlbedo); glBindTexture(GL_TEXTURE_2D, g_renderer.gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, g_renderer.gAlbedo, 0);
    glGenTextures(1, &g_renderer.gPBRParams); glBindTexture(GL_TEXTURE_2D, g_renderer.gPBRParams);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, g_renderer.gPBRParams, 0);
    glGenTextures(1, &g_renderer.gVelocity);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gVelocity);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RG, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, g_renderer.gVelocity, 0);
    glGenTextures(1, &g_renderer.gIndirectLight);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gIndirectLight);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT6, GL_TEXTURE_2D, g_renderer.gIndirectLight, 0);
    GLuint attachments[7] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6 }; glDrawBuffers(7, attachments);
    GLuint rboDepth; glGenRenderbuffers(1, &rboDepth); glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, LOW_RES_WIDTH, LOW_RES_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("G-Buffer Framebuffer not complete!\n");
    glGenFramebuffers(1, &g_renderer.vplGenerationFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.vplGenerationFBO);
    glGenTextures(1, &g_renderer.vplPosTex);
    glBindTexture(GL_TEXTURE_2D, g_renderer.vplPosTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, VPL_GEN_TEXTURE_SIZE, VPL_GEN_TEXTURE_SIZE, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_renderer.vplPosTex, 0);
    glGenTextures(1, &g_renderer.vplNormalTex);
    glBindTexture(GL_TEXTURE_2D, g_renderer.vplNormalTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, VPL_GEN_TEXTURE_SIZE, VPL_GEN_TEXTURE_SIZE, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g_renderer.vplNormalTex, 0);
    glGenTextures(1, &g_renderer.vplAlbedoTex);
    glBindTexture(GL_TEXTURE_2D, g_renderer.vplAlbedoTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, VPL_GEN_TEXTURE_SIZE, VPL_GEN_TEXTURE_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, g_renderer.vplAlbedoTex, 0);
    GLuint vpl_attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, vpl_attachments);
    GLuint vpl_rboDepth; glGenRenderbuffers(1, &vpl_rboDepth); glBindRenderbuffer(GL_RENDERBUFFER, vpl_rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, VPL_GEN_TEXTURE_SIZE, VPL_GEN_TEXTURE_SIZE);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, vpl_rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("VPL Generation Framebuffer not complete!\n");
    glGenBuffers(1, &g_renderer.vplSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_renderer.vplSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_VPLS * sizeof(VPL), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, g_renderer.vplSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    const int bloom_width = WINDOW_WIDTH / BLOOM_DOWNSAMPLE;
    const int bloom_height = WINDOW_HEIGHT / BLOOM_DOWNSAMPLE;
    glGenFramebuffers(1, &g_renderer.bloomFBO); glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.bloomFBO);
    glGenTextures(1, &g_renderer.bloomBrightnessTexture); glBindTexture(GL_TEXTURE_2D, g_renderer.bloomBrightnessTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, bloom_width, bloom_height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_renderer.bloomBrightnessTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Bloom FBO not complete!\n");
    glGenFramebuffers(2, g_renderer.pingpongFBO); glGenTextures(2, g_renderer.pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.pingpongFBO[i]); glBindTexture(GL_TEXTURE_2D, g_renderer.pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, bloom_width, bloom_height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 0.0f, 0.0f, 0.0f, 1.0f }; glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_renderer.pingpongColorbuffers[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Ping-pong FBO %d not complete!\n", i);
    }
    glGenFramebuffers(1, &g_renderer.finalRenderFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.finalRenderFBO);
    glGenTextures(1, &g_renderer.finalRenderTexture);
    glBindTexture(GL_TEXTURE_2D, g_renderer.finalRenderTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_renderer.finalRenderTexture, 0);
    glGenTextures(1, &g_renderer.finalDepthTexture);
    glBindTexture(GL_TEXTURE_2D, g_renderer.finalDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_renderer.finalDepthTexture, 0);
    GLuint final_rboDepth; glGenRenderbuffers(1, &final_rboDepth); glBindRenderbuffer(GL_RENDERBUFFER, final_rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, WINDOW_WIDTH, WINDOW_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, final_rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Final Render Framebuffer not complete!\n");
    glGenFramebuffers(1, &g_renderer.postProcessFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.postProcessFBO);
    glGenTextures(1, &g_renderer.postProcessTexture);
    glBindTexture(GL_TEXTURE_2D, g_renderer.postProcessTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_renderer.postProcessTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Post Process Framebuffer not complete!\n");
    glGenFramebuffers(1, &g_renderer.volumetricFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.volumetricFBO);
    glGenTextures(1, &g_renderer.volumetricTexture);
    glBindTexture(GL_TEXTURE_2D, g_renderer.volumetricTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, WINDOW_WIDTH / VOLUMETRIC_DOWNSAMPLE, WINDOW_HEIGHT / VOLUMETRIC_DOWNSAMPLE, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_renderer.volumetricTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Volumetric FBO not complete!\n");
    glGenFramebuffers(2, g_renderer.volPingpongFBO);
    glGenTextures(2, g_renderer.volPingpongTextures);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.volPingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, g_renderer.volPingpongTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, WINDOW_WIDTH / VOLUMETRIC_DOWNSAMPLE, WINDOW_HEIGHT / VOLUMETRIC_DOWNSAMPLE, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_renderer.volPingpongTextures[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Volumetric Ping-Pong FBO %d not complete!\n", i);
    }
    glGenFramebuffers(1, &g_renderer.sunShadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.sunShadowFBO);
    glGenTextures(1, &g_renderer.sunShadowMap);
    glBindTexture(GL_TEXTURE_2D, g_renderer.sunShadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SUN_SHADOW_MAP_SIZE, SUN_SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_renderer.sunShadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("Sun Shadow Framebuffer not complete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glGenVertexArrays(1, &g_renderer.quadVAO); glGenBuffers(1, &g_renderer.quadVBO);
    glBindVertexArray(g_renderer.quadVAO); glBindBuffer(GL_ARRAY_BUFFER, g_renderer.quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))); glEnableVertexAttribArray(1);
    glGenVertexArrays(1, &g_renderer.skyboxVAO); glGenBuffers(1, &g_renderer.skyboxVBO);
    glBindVertexArray(g_renderer.skyboxVAO); glBindBuffer(GL_ARRAY_BUFFER, g_renderer.skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glGenVertexArrays(1, &g_renderer.decalVAO); glGenBuffers(1, &g_renderer.decalVBO);
    glBindVertexArray(g_renderer.decalVAO); glBindBuffer(GL_ARRAY_BUFFER, g_renderer.decalVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(decalQuadVertices), &decalQuadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float))); glEnableVertexAttribArray(3);
    glGenVertexArrays(1, &g_renderer.parallaxRoomVAO); glGenBuffers(1, &g_renderer.parallaxRoomVBO);
    glBindVertexArray(g_renderer.parallaxRoomVAO); glBindBuffer(GL_ARRAY_BUFFER, g_renderer.parallaxRoomVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(parallaxRoomVertices), parallaxRoomVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(8 * sizeof(float))); glEnableVertexAttribArray(3);
    glBindVertexArray(0);
    g_renderer.brdfLUTTexture = TextureManager_LoadLUT("brdf_lut.png");
    if (g_renderer.brdfLUTTexture == 0) {
        Console_Printf("[ERROR] Failed to load brdf_lut.png! Ensure it's in the 'textures' folder.");
    }
    glUseProgram(g_renderer.mainShader);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "diffuseMap"), 0); glUniform1i(glGetUniformLocation(g_renderer.mainShader, "normalMap"), 1);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "rmaMap"), 2); glUniform1i(glGetUniformLocation(g_renderer.mainShader, "heightMap"), 3); glUniform1i(glGetUniformLocation(g_renderer.mainShader, "detailDiffuseMap"), 7);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "environmentMap"), 10);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "brdfLUT"), 16);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "diffuseMap2"), 12);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "normalMap2"), 13);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "rmaMap2"), 14);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "heightMap2"), 15);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "diffuseMap3"), 17);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "normalMap3"), 18);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "rmaMap3"), 19);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "heightMap3"), 20);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "diffuseMap4"), 21);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "normalMap4"), 22);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "rmaMap4"), 23);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "heightMap4"), 24);
    glUseProgram(g_renderer.volumetricShader);
    glUniform1i(glGetUniformLocation(g_renderer.volumetricShader, "gPosition"), 0);
    glUseProgram(g_renderer.volumetricBlurShader);
    glUniform1i(glGetUniformLocation(g_renderer.volumetricBlurShader, "image"), 0);
    glUseProgram(g_renderer.skyboxShader);
    glUseProgram(g_renderer.postProcessShader);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "sceneTexture"), 0);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "bloomBlur"), 1);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "gPosition"), 2);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "volumetricTexture"), 3);
    glUseProgram(g_renderer.bloomShader); glUniform1i(glGetUniformLocation(g_renderer.bloomShader, "sceneTexture"), 0);
    glUseProgram(g_renderer.bloomBlurShader); glUniform1i(glGetUniformLocation(g_renderer.bloomBlurShader, "image"), 0);
    glUseProgram(g_renderer.dofShader);
    glUniform1i(glGetUniformLocation(g_renderer.dofShader, "screenTexture"), 0);
    glUniform1i(glGetUniformLocation(g_renderer.dofShader, "depthTexture"), 1);
    mat4_identity(&g_renderer.prevViewProjection);
    glGenBuffers(1, &g_renderer.exposureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_renderer.exposureSSBO);
    struct GPUExposureData {
        float exposure;
    } initialData = { 1.0f };
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(initialData), &initialData, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, g_renderer.exposureSSBO);
    glGenBuffers(1, &g_renderer.histogramSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_renderer.histogramSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 256 * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, g_renderer.histogramSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    const int ssao_width = WINDOW_WIDTH / SSAO_DOWNSAMPLE;
    const int ssao_height = WINDOW_HEIGHT / SSAO_DOWNSAMPLE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glGenFramebuffers(1, &g_renderer.ssaoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.ssaoFBO);
    glGenTextures(1, &g_renderer.ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, g_renderer.ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ssao_width, ssao_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_renderer.ssaoColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("SSAO Framebuffer not complete!\n");
    glGenFramebuffers(1, &g_renderer.ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.ssaoBlurFBO);
    glGenTextures(1, &g_renderer.ssaoBlurColorBuffer);
    glBindTexture(GL_TEXTURE_2D, g_renderer.ssaoBlurColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ssao_width, ssao_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_renderer.ssaoBlurColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("SSAO Blur Framebuffer not complete!\n");
    glUseProgram(g_renderer.ssaoShader);
    glUniform1i(glGetUniformLocation(g_renderer.ssaoShader, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(g_renderer.ssaoShader, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(g_renderer.ssaoShader, "texNoise"), 2);
    glUseProgram(g_renderer.ssaoBlurShader);
    glUniform1i(glGetUniformLocation(g_renderer.ssaoBlurShader, "ssaoInput"), 0);
    glUseProgram(g_renderer.postProcessShader);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "ssao"), 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUniform1i(glGetUniformLocation(g_renderer.ssaoBlurShader, "ssaoInput"), 0);
    glUseProgram(g_renderer.postProcessShader);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "ssao"), 4);
    glUseProgram(g_renderer.waterShader);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "dudvMap"), 0);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "normalMap"), 1);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "reflectionMap"), 2);
    g_renderer.dudvMap = loadTexture("dudv.png", false);
    g_renderer.waterNormalMap = loadTexture("water_normal.png", false);
    g_renderer.cloudTexture = loadTexture("clouds.png", false);
    if (g_renderer.cloudTexture == 0) {
        Console_Printf("[ERROR] Failed to load clouds.png! Ensure it's in the 'textures' folder.");
    }
    glGenBuffers(1, &g_renderer.lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_renderer.lightSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_LIGHTS * sizeof(ShaderLight), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, g_renderer.lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    const GLubyte* gpu = glGetString(GL_RENDERER);
    const GLubyte* gl_version = glGetString(GL_VERSION);
    printf("------------------------------------------------------\n");
    printf("Renderer Context Initialized:\n");
    printf("  GPU: %s\n", (const char*)gpu);
    printf("  OpenGL Version: %s\n", (const char*)gl_version);
    printf("------------------------------------------------------\n");
}

void init_scene() {
    memset(&g_scene, 0, sizeof(Scene));
    const GameConfig* config = GameConfig_Get();
    Scene_LoadMap(&g_scene, &g_renderer, config->startmap, g_engine);
    strncpy(g_scene.mapPath, config->startmap, sizeof(g_scene.mapPath) - 1);
    g_scene.mapPath[sizeof(g_scene.mapPath) - 1] = '\0';
    g_last_player_pos = g_scene.playerStart.position;
}

void process_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        UI_ProcessEvent(&event);

        if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
            MainMenuAction action = MainMenu_HandleEvent(&event);
            if (action == MAINMENU_ACTION_START_GAME) {
                g_current_mode = MODE_GAME;
                SDL_SetRelativeMouseMode(SDL_TRUE);
                Console_Printf("Starting game...");
                MainMenu_SetInGameMenuMode(false, true);
            }
            else if (action == MAINMENU_ACTION_CONTINUE_GAME) {
                g_current_mode = MODE_GAME;
                SDL_SetRelativeMouseMode(SDL_TRUE);
                Console_Printf("Returning to game...");
            }
            else if (action == MAINMENU_ACTION_QUIT) {
                Cvar_EngineSet("engine_running", "0");
            }
        }
        else if (g_current_mode == MODE_EDITOR) {
            Editor_ProcessEvent(&event, &g_scene, g_engine);
        }

        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (event.key.keysym.sym == SDLK_e && g_current_mode == MODE_GAME && !Console_IsVisible()) {
                Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
                vec3_normalize(&forward);

                Vec3 ray_end = vec3_add(g_engine->camera.position, vec3_muls(forward, 3.0f));

                for (int i = 0; i < g_scene.numBrushes; ++i) {
                    Brush* brush = &g_scene.brushes[i];
                    if (brush->isTrigger) {
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
                            brush_local_min = (Vec3){ -0.1f, -0.1f, -0.1f };
                            brush_local_max = (Vec3){ 0.1f,  0.1f,  0.1f };
                        }

                        float t;
                        if (RayIntersectsOBB(g_engine->camera.position, forward,
                            &brush->modelMatrix,
                            brush_local_min,
                            brush_local_max,
                            &t) && t < 3.0f) {
                            IO_FireOutput(ENTITY_BRUSH, i, "OnUse", g_engine->lastFrame);
                        }
                    }
                }
            }
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                if (g_current_mode == MODE_GAME) {
                    g_current_mode = MODE_INGAMEMENU;
                    bool map_is_currently_loaded = (g_scene.numObjects > 0 || g_scene.numBrushes > 0);
                    MainMenu_SetInGameMenuMode(true, map_is_currently_loaded);
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    Console_Printf("In-game menu opened.");
                }
                else if (g_current_mode == MODE_INGAMEMENU) {
                    g_current_mode = MODE_GAME;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    Console_Printf("In-game menu closed.");
                }
            }
            else if (event.key.keysym.sym == SDLK_BACKQUOTE) {
                Console_Toggle();
                if (g_current_mode == MODE_GAME || g_current_mode == MODE_INGAMEMENU) {
                    SDL_SetRelativeMouseMode(Console_IsVisible() ? SDL_FALSE : SDL_TRUE);
                }
            }
            else if (event.key.keysym.sym == SDLK_F5) {
                if (g_current_mode != MODE_MAINMENU) {
                    handle_command(1, (char* []) { "edit" });
                }
            }
            else if (event.key.keysym.sym == SDLK_f && g_current_mode == MODE_GAME && !Console_IsVisible()) {
                g_engine->flashlight_on = !g_engine->flashlight_on;
                SoundSystem_PlaySound(g_flashlight_sound_buffer, g_engine->camera.position, 1.0f, 1.0f, 50.0f, false);
            }
            else {
                if (g_current_mode == MODE_GAME && !Console_IsVisible()) {
                    const char* command = Binds_GetCommand(event.key.keysym.sym);
                    if (command) {
                        char cmd_copy[MAX_COMMAND_LENGTH];
                        strcpy(cmd_copy, command);
                        char* argv[16];
                        int argc = 0;
                        char* p = strtok(cmd_copy, " ");
                        while (p != NULL && argc < 16) {
                            argv[argc++] = p;
                            p = strtok(NULL, " ");
                        }
                        if (argc > 0) {
                            handle_command(argc, argv);
                        }
                    }
                }
            }
        }

        if (g_current_mode == MODE_GAME || g_current_mode == MODE_EDITOR) {
            if (event.type == SDL_MOUSEMOTION) {
                bool can_look_in_editor = (g_current_mode == MODE_EDITOR) || (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT));
                bool can_look_in_game = (g_current_mode == MODE_GAME && !Console_IsVisible());

                if (can_look_in_game || can_look_in_editor) {
                    g_engine->camera.yaw += event.motion.xrel * 0.005f;
                    g_engine->camera.pitch -= event.motion.yrel * 0.005f;
                    if (g_engine->camera.pitch > 1.55f) g_engine->camera.pitch = 1.55f;
                    if (g_engine->camera.pitch < -1.55f) g_engine->camera.pitch = -1.55f;
                }
            }
        }
    }

    if (g_current_mode == MODE_GAME && !Console_IsVisible()) {
        const Uint8* state = SDL_GetKeyboardState(NULL);

        bool noclip = Cvar_GetInt("noclip");
        float speed = (noclip ? 10.0f : 5.0f) * (g_engine->camera.isCrouching ? 0.5f : 1.0f);

        if (noclip) {
            Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
            vec3_normalize(&forward);
            Vec3 right = vec3_cross(forward, (Vec3) { 0, 1, 0 });
            vec3_normalize(&right);

            if (state[SDL_SCANCODE_W]) g_engine->camera.position = vec3_add(g_engine->camera.position, vec3_muls(forward, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_S]) g_engine->camera.position = vec3_sub(g_engine->camera.position, vec3_muls(forward, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_D]) g_engine->camera.position = vec3_add(g_engine->camera.position, vec3_muls(right, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_A]) g_engine->camera.position = vec3_sub(g_engine->camera.position, vec3_muls(right, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_SPACE]) g_engine->camera.position.y += speed * g_engine->deltaTime;
            if (state[SDL_SCANCODE_LCTRL]) g_engine->camera.position.y -= speed * g_engine->deltaTime;

        }
        else {
            Vec3 f_flat = { sinf(g_engine->camera.yaw), 0, -cosf(g_engine->camera.yaw) };
            Vec3 r_flat = { f_flat.z, 0, -f_flat.x };
            Vec3 move = { 0,0,0 };

            if (state[SDL_SCANCODE_W]) move = vec3_add(move, f_flat);
            if (state[SDL_SCANCODE_S]) move = vec3_sub(move, f_flat);
            if (state[SDL_SCANCODE_A]) move = vec3_add(move, r_flat);
            if (state[SDL_SCANCODE_D]) move = vec3_sub(move, r_flat);

            vec3_normalize(&move);
            float max_wish_speed = Cvar_GetFloat("g_speed");
            if (state[SDL_SCANCODE_LSHIFT] && !g_engine->camera.isCrouching) {
                max_wish_speed = Cvar_GetFloat("g_sprint_speed");
            }
            if (g_engine->camera.isCrouching) {
                max_wish_speed *= 0.5f;
            }

            float accel = Cvar_GetFloat("g_accel");
            float friction = Cvar_GetFloat("g_friction");

            Vec3 current_vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
            Vec3 current_vel_flat = { current_vel.x, 0, current_vel.z };

            Vec3 wish_vel = vec3_muls(move, max_wish_speed);

            Vec3 vel_delta = vec3_sub(wish_vel, current_vel_flat);

            if (vec3_length_sq(vel_delta) > 0.0001f) {
                float delta_speed = vec3_length(vel_delta);

                float add_speed = delta_speed * accel * g_engine->deltaTime;

                if (add_speed > delta_speed) {
                    add_speed = delta_speed;
                }

                current_vel_flat = vec3_add(current_vel_flat, vec3_muls(vel_delta, add_speed / delta_speed));
            }

            if (vec3_length_sq(move) < 0.01f) {
                float speed = vec3_length(current_vel_flat);
                if (speed > 0.001f) {
                    float drop = speed * friction * g_engine->deltaTime;
                    float new_speed = speed - drop;
                    if (new_speed < 0) new_speed = 0;
                    current_vel_flat = vec3_muls(current_vel_flat, new_speed / speed);
                }
                else {
                    current_vel_flat = (Vec3){ 0,0,0 };
                }
            }

            Physics_SetLinearVelocity(g_engine->camera.physicsBody, (Vec3) { current_vel_flat.x, current_vel.y, current_vel_flat.z });
            Physics_Activate(g_engine->camera.physicsBody);

            if (state[SDL_SCANCODE_SPACE]) {
                if (fabs(Physics_GetLinearVelocity(g_engine->camera.physicsBody).y) < 0.01f) {
                    Physics_ApplyCentralImpulse(g_engine->camera.physicsBody, (Vec3) { 0, PLAYER_JUMP_FORCE, 0 });
                }
            }
        }
        g_engine->camera.isCrouching = state[SDL_SCANCODE_LCTRL];
    }
}

void update_state() {
    g_engine->running = Cvar_GetInt("engine_running");
    SoundSystem_SetMasterVolume(Cvar_GetFloat("volume"));
    IO_ProcessPendingEvents(g_engine->lastFrame, &g_scene, g_engine);
    for (int i = 0; i < g_scene.numActiveLights; ++i) {
        Light* light = &g_scene.lights[i];
        light->intensity = light->is_on ? light->base_intensity : 0.0f;
    }
    for (int i = 0; i < g_scene.numActiveLights; ++i) {
        if (g_scene.lights[i].type == LIGHT_SPOT) {
            Mat4 rot_mat = create_trs_matrix((Vec3) { 0, 0, 0 }, g_scene.lights[i].rot, (Vec3) { 1, 1, 1 });
            Vec3 forward = { 0, 0, -1 }; g_scene.lights[i].direction = mat4_mul_vec3_dir(&rot_mat, forward); vec3_normalize(&g_scene.lights[i].direction);
        }
    }
    if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
        MainMenu_Update(g_engine->deltaTime);
        return;
    }
    if (g_current_mode == MODE_EDITOR) { Editor_Update(g_engine, &g_scene); return; }
    for (int i = 0; i < g_scene.numParticleEmitters; ++i) {
        ParticleEmitter_Update(&g_scene.particleEmitters[i], g_engine->deltaTime);
    }
    VideoPlayer_UpdateAll(&g_scene, g_engine->deltaTime);
    Vec3 playerPos;
    Physics_GetPosition(g_engine->camera.physicsBody, &playerPos);

    int new_reverb_zone_index = -1;
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (!b->isDSP) continue;
        if (b->numVertices == 0) continue;

        Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        for (int v = 0; v < b->numVertices; ++v) {
            Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v].pos);
            min_aabb.x = fminf(min_aabb.x, world_v.x);
            min_aabb.y = fminf(min_aabb.y, world_v.y);
            min_aabb.z = fminf(min_aabb.z, world_v.z);
            max_aabb.x = fmaxf(max_aabb.x, world_v.x);
            max_aabb.y = fmaxf(max_aabb.y, world_v.y);
            max_aabb.z = fmaxf(max_aabb.z, world_v.z);
        }

        if (playerPos.x >= min_aabb.x && playerPos.x <= max_aabb.x &&
            playerPos.y >= min_aabb.y && playerPos.y <= max_aabb.y &&
            playerPos.z >= min_aabb.z && playerPos.z <= max_aabb.z)
        {
            new_reverb_zone_index = i;
            break;
        }
    }

    if (new_reverb_zone_index != g_current_reverb_zone_index) {
        g_current_reverb_zone_index = new_reverb_zone_index;
        if (new_reverb_zone_index != -1) {
            SoundSystem_SetCurrentReverb(g_scene.brushes[new_reverb_zone_index].reverbPreset);
        }
        else {
            SoundSystem_SetCurrentReverb(REVERB_PRESET_NONE);
        }
    }

    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (!b->isTrigger) continue;

        if (b->numVertices == 0) continue;

        Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
        Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        for (int v = 0; v < b->numVertices; ++v) {
            Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v].pos);
            min_aabb.x = fminf(min_aabb.x, world_v.x);
            min_aabb.y = fminf(min_aabb.y, world_v.y);
            min_aabb.z = fminf(min_aabb.z, world_v.z);
            max_aabb.x = fmaxf(max_aabb.x, world_v.x);
            max_aabb.y = fmaxf(max_aabb.y, world_v.y);
            max_aabb.z = fmaxf(max_aabb.z, world_v.z);
        }

        bool is_inside = (playerPos.x >= min_aabb.x && playerPos.x <= max_aabb.x &&
            playerPos.y >= min_aabb.y && playerPos.y <= max_aabb.y &&
            playerPos.z >= min_aabb.z && playerPos.z <= max_aabb.z);

        if (is_inside && !b->playerIsTouching) {
            b->playerIsTouching = true;
            IO_FireOutput(ENTITY_BRUSH, i, "OnTouch", g_engine->lastFrame);
        }
        else if (!is_inside && b->playerIsTouching) {
            b->playerIsTouching = false;
            IO_FireOutput(ENTITY_BRUSH, i, "OnEndTouch", g_engine->lastFrame);
        }
    }
    Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
    vec3_normalize(&forward); SoundSystem_UpdateListener(g_engine->camera.position, forward, (Vec3) { 0, 1, 0 });
    bool noclip = Cvar_GetInt("noclip");
    if (!noclip) {
        Vec3 vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
        bool on_ground = fabs(vel.y) < 0.1f;

        if (on_ground) {
            float dx = g_engine->camera.position.x - g_last_player_pos.x;
            float dz = g_engine->camera.position.z - g_last_player_pos.z;
            g_distance_walked += sqrtf(dx * dx + dz * dz);

            if (g_distance_walked >= FOOTSTEP_DISTANCE) {
                SoundSystem_PlaySound(g_footstep_sound_buffer, g_engine->camera.position, 0.7f, 1.0f, 50.0f, false);
                g_distance_walked = 0.0f;
            }
        }
        else {
            g_distance_walked = 0.0f;
        }

        g_last_player_pos = g_engine->camera.position;
    }
    Physics_SetGravityEnabled(g_engine->camera.physicsBody, !noclip);
    if (noclip) Physics_SetLinearVelocity(g_engine->camera.physicsBody, (Vec3) { 0, 0, 0 });
    if (g_engine->physicsWorld) Physics_StepSimulation(g_engine->physicsWorld, g_engine->deltaTime);
    if (!noclip) { Vec3 p; Physics_GetPosition(g_engine->camera.physicsBody, &p); g_engine->camera.position.x = p.x; g_engine->camera.position.z = p.z; float h = g_engine->camera.isCrouching ? PLAYER_HEIGHT_CROUCH : PLAYER_HEIGHT_NORMAL; float eyeHeightOffsetFromCenter = (g_engine->camera.currentHeight / 2.0f) * 0.85f;
    g_engine->camera.position.y = p.y + eyeHeightOffsetFromCenter;
    }
    if (g_current_mode == MODE_GAME) {
        for (int i = 0; i < g_scene.numObjects; ++i) {
            SceneObject* obj = &g_scene.objects[i];
            if (obj->physicsBody && obj->mass > 0.0f) {
                float phys_matrix_data[16];
                Physics_GetRigidBodyTransform(obj->physicsBody, phys_matrix_data);
                Mat4 physics_transform;
                memcpy(&physics_transform, phys_matrix_data, sizeof(Mat4));
                Mat4 scale_transform = mat4_scale(obj->scale);
                mat4_multiply(&obj->modelMatrix, &physics_transform, &scale_transform);
            }
        }
    }
}

static void render_vpl_pass() {
    g_scene.num_vpls = 0;
    int vpls_per_light = Cvar_GetInt("r_vpl_count");
    if (vpls_per_light <= 0) return;

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glCullFace(GL_BACK);

    for (int i = 0; i < g_scene.numActiveLights; ++i) {
        Light* light = &g_scene.lights[i];
        if (light->intensity <= 0.0f || (g_scene.num_vpls >= MAX_VPLS)) continue;

        if (light->type == LIGHT_POINT) {
            int vpls_this_light = vpls_per_light;
            if (g_scene.num_vpls + vpls_this_light > MAX_VPLS) {
                vpls_this_light = MAX_VPLS - g_scene.num_vpls;
            }
            int vpls_per_face = vpls_this_light / 6;
            if (vpls_per_face < 1) vpls_per_face = 1;

            Mat4 lightProjection = mat4_perspective(90.0f * 3.14159f / 180.0f, 1.0f, 0.1f, light->radius);
            Mat4 shadowViews[6];
            shadowViews[0] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 1.0f, 0.0f, 0.0f }), (Vec3) { 0.0f, -1.0f, 0.0f });
            shadowViews[1] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { -1.0f, 0.0f, 0.0f }), (Vec3) { 0.0f, -1.0f, 0.0f });
            shadowViews[2] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0.0f, 1.0f, 0.0f }), (Vec3) { 0.0f, 0.0f, 1.0f });
            shadowViews[3] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0.0f, -1.0f, 0.0f }), (Vec3) { 0.0f, 0.0f, -1.0f });
            shadowViews[4] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0.0f, 0.0f, 1.0f }), (Vec3) { 0.0f, -1.0f, 0.0f });
            shadowViews[5] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0.0f, 0.0f, -1.0f }), (Vec3) { 0.0f, -1.0f, 0.0f });

            for (int face = 0; face < 6; ++face) {
                if (g_scene.num_vpls + vpls_per_face > MAX_VPLS) break;

                glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.vplGenerationFBO);
                glViewport(0, 0, VPL_GEN_TEXTURE_SIZE, VPL_GEN_TEXTURE_SIZE);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glUseProgram(g_renderer.vplGenerationShader);
                glUniformMatrix4fv(glGetUniformLocation(g_renderer.vplGenerationShader, "view"), 1, GL_FALSE, shadowViews[face].m);
                glUniformMatrix4fv(glGetUniformLocation(g_renderer.vplGenerationShader, "projection"), 1, GL_FALSE, lightProjection.m);

                Frustum light_frustum;
                Mat4 light_vp;
                mat4_multiply(&light_vp, &lightProjection, &shadowViews[face]);
                extract_frustum_planes(&light_vp, &light_frustum, true);

                for (int j = 0; j < g_scene.numObjects; ++j) {
                    SceneObject* obj = &g_scene.objects[j];
                    if (obj->model) {
                        Vec3 world_min = mat4_mul_vec3(&obj->modelMatrix, obj->model->aabb_min);
                        Vec3 world_max = mat4_mul_vec3(&obj->modelMatrix, obj->model->aabb_max);
                        if (!frustum_check_aabb(&light_frustum, world_min, world_max)) continue;
                    }
                    render_object(g_renderer.vplGenerationShader, obj, false, &light_frustum);
                }
                for (int j = 0; j < g_scene.numBrushes; ++j) {
                    Brush* b = &g_scene.brushes[j];
                    if (b->numVertices > 0) {
                        Vec3 min_v = { FLT_MAX, FLT_MAX, FLT_MAX }; Vec3 max_v = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                        for (int k = 0; k < b->numVertices; ++k) {
                            Vec3 p = mat4_mul_vec3(&b->modelMatrix, b->vertices[k].pos);
                            min_v.x = fminf(min_v.x, p.x); min_v.y = fminf(min_v.y, p.y); min_v.z = fminf(min_v.z, p.z);
                            max_v.x = fmaxf(max_v.x, p.x); max_v.y = fmaxf(max_v.y, p.y); max_v.z = fmaxf(max_v.z, p.z);
                        }
                        if (!frustum_check_aabb(&light_frustum, min_v, max_v)) continue;
                    }
                    render_brush(g_renderer.vplGenerationShader, b, false, &light_frustum);
                }

                glUseProgram(g_renderer.vplComputeShader);
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_renderer.vplPosTex);
                glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, g_renderer.vplNormalTex);
                glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, g_renderer.vplAlbedoTex);
                glUniform1i(glGetUniformLocation(g_renderer.vplComputeShader, "u_posTex"), 0);
                glUniform1i(glGetUniformLocation(g_renderer.vplComputeShader, "u_normalTex"), 1);
                glUniform1i(glGetUniformLocation(g_renderer.vplComputeShader, "u_albedoTex"), 2);
                glUniform1i(glGetUniformLocation(g_renderer.vplComputeShader, "u_vpl_offset"), g_scene.num_vpls);
                glUniform3fv(glGetUniformLocation(g_renderer.vplComputeShader, "u_lightPos"), 1, &light->position.x);
                glUniform3fv(glGetUniformLocation(g_renderer.vplComputeShader, "u_lightColor"), 1, &light->color.x);
                glUniform1f(glGetUniformLocation(g_renderer.vplComputeShader, "u_lightIntensity"), light->intensity);

                int workgroup_size = 64;
                int num_workgroups = (vpls_per_face + workgroup_size - 1) / workgroup_size;
                glDispatchCompute(num_workgroups, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                g_scene.num_vpls += vpls_per_face;
            }
        }
        else {
            if (g_scene.num_vpls + vpls_per_light > MAX_VPLS) continue;
            Mat4 lightView, lightProjection;
            float angle_rad = acosf(fmaxf(-1.0f, fminf(1.0f, light->cutOff)));
            if (angle_rad < 0.01f) angle_rad = 0.01f;
            lightProjection = mat4_perspective(angle_rad * 2.0f, 1.0f, 0.1f, light->radius);
            Vec3 up_vector = (Vec3){ 0, 1, 0 };
            if (fabs(vec3_dot(light->direction, up_vector)) > 0.99f) { up_vector = (Vec3){ 1, 0, 0 }; }
            lightView = mat4_lookAt(light->position, vec3_add(light->position, light->direction), up_vector);

            glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.vplGenerationFBO);
            glViewport(0, 0, VPL_GEN_TEXTURE_SIZE, VPL_GEN_TEXTURE_SIZE);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(g_renderer.vplGenerationShader);
            glUniformMatrix4fv(glGetUniformLocation(g_renderer.vplGenerationShader, "view"), 1, GL_FALSE, lightView.m);
            glUniformMatrix4fv(glGetUniformLocation(g_renderer.vplGenerationShader, "projection"), 1, GL_FALSE, lightProjection.m);

            Frustum light_frustum;
            Mat4 light_vp;
            mat4_multiply(&light_vp, &lightProjection, &lightView);
            extract_frustum_planes(&light_vp, &light_frustum, true);

            for (int j = 0; j < g_scene.numObjects; ++j) {
                SceneObject* obj = &g_scene.objects[j];
                if (obj->model) {
                    Vec3 world_min = mat4_mul_vec3(&obj->modelMatrix, obj->model->aabb_min);
                    Vec3 world_max = mat4_mul_vec3(&obj->modelMatrix, obj->model->aabb_max);
                    if (!frustum_check_aabb(&light_frustum, world_min, world_max)) continue;
                }
                render_object(g_renderer.vplGenerationShader, obj, false, &light_frustum);
            }
            for (int j = 0; j < g_scene.numBrushes; ++j) {
                Brush* b = &g_scene.brushes[j];
                if (b->numVertices > 0) {
                    Vec3 min_v = { FLT_MAX, FLT_MAX, FLT_MAX }; Vec3 max_v = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                    for (int k = 0; k < b->numVertices; ++k) {
                        Vec3 p = mat4_mul_vec3(&b->modelMatrix, b->vertices[k].pos);
                        min_v.x = fminf(min_v.x, p.x); min_v.y = fminf(min_v.y, p.y); min_v.z = fminf(min_v.z, p.z);
                        max_v.x = fmaxf(max_v.x, p.x); max_v.y = fmaxf(max_v.y, p.y); max_v.z = fmaxf(max_v.z, p.z);
                    }
                    if (!frustum_check_aabb(&light_frustum, min_v, max_v)) continue;
                }
                render_brush(g_renderer.vplGenerationShader, b, false, &light_frustum);
            }

            glUseProgram(g_renderer.vplComputeShader);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_renderer.vplPosTex);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, g_renderer.vplNormalTex);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, g_renderer.vplAlbedoTex);
            glUniform1i(glGetUniformLocation(g_renderer.vplComputeShader, "u_posTex"), 0);
            glUniform1i(glGetUniformLocation(g_renderer.vplComputeShader, "u_normalTex"), 1);
            glUniform1i(glGetUniformLocation(g_renderer.vplComputeShader, "u_albedoTex"), 2);
            glUniform1i(glGetUniformLocation(g_renderer.vplComputeShader, "u_vpl_offset"), g_scene.num_vpls);
            glUniform3fv(glGetUniformLocation(g_renderer.vplComputeShader, "u_lightPos"), 1, &light->position.x);
            glUniform3fv(glGetUniformLocation(g_renderer.vplComputeShader, "u_lightColor"), 1, &light->color.x);
            glUniform1f(glGetUniformLocation(g_renderer.vplComputeShader, "u_lightIntensity"), light->intensity);

            int workgroup_size = 64;
            int num_workgroups = (vpls_per_light + workgroup_size - 1) / workgroup_size;
            glDispatchCompute(num_workgroups, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            g_scene.num_vpls += vpls_per_light;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
}

void render_sun_shadows(const Mat4* sunLightSpaceMatrix) {
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
    glViewport(0, 0, SUN_SHADOW_MAP_SIZE, SUN_SHADOW_MAP_SIZE);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.sunShadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_renderer.spotDepthShader);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.spotDepthShader, "lightSpaceMatrix"), 1, GL_FALSE, sunLightSpaceMatrix->m);

    for (int j = 0; j < g_scene.numObjects; ++j) {
        render_object(g_renderer.spotDepthShader, &g_scene.objects[j], false, NULL);
    }
    for (int j = 0; j < g_scene.numBrushes; ++j) {
        if (g_scene.brushes[j].isWater) continue;
        render_brush(g_renderer.spotDepthShader, &g_scene.brushes[j], false, NULL);
    }

    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void render_parallax_rooms(Mat4* view, Mat4* projection) {
    glUseProgram(g_renderer.parallaxInteriorShader);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.parallaxInteriorShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.parallaxInteriorShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform3fv(glGetUniformLocation(g_renderer.parallaxInteriorShader, "viewPos"), 1, &g_engine->camera.position.x);

    for (int i = 0; i < g_scene.numParallaxRooms; ++i) {
        ParallaxRoom* p = &g_scene.parallaxRooms[i];
        if (p->cubemapTexture == 0) continue;

        glUniformMatrix4fv(glGetUniformLocation(g_renderer.parallaxInteriorShader, "model"), 1, GL_FALSE, p->modelMatrix.m);
        glUniform1f(glGetUniformLocation(g_renderer.parallaxInteriorShader, "roomDepth"), p->roomDepth);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, p->cubemapTexture);
        glUniform1i(glGetUniformLocation(g_renderer.parallaxInteriorShader, "roomCubemap"), 0);

        glBindVertexArray(g_renderer.parallaxRoomVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    glBindVertexArray(0);
}

void render_shadows() {
    glEnable(GL_DEPTH_TEST); glCullFace(GL_FRONT);  
    int shadow_map_size = Cvar_GetInt("r_shadow_map_size");
    if (shadow_map_size <= 0) {
        shadow_map_size = 1024;
    }
    glViewport(0, 0, shadow_map_size, shadow_map_size);
    for (int i = 0; i < g_scene.numActiveLights; ++i) {
        Light* light = &g_scene.lights[i];
        if (light->intensity <= 0.0f) continue;
        glBindFramebuffer(GL_FRAMEBUFFER, light->shadowFBO); 
        glClear(GL_DEPTH_BUFFER_BIT);
        GLuint current_shader;
        if (light->type == LIGHT_POINT) {
            current_shader = g_renderer.pointDepthShader; glUseProgram(current_shader);
            Mat4 shadowProj = mat4_perspective(90.0f * 3.14159f / 180.0f, 1.0f, 1.0f, light->shadowFarPlane);
            Mat4 shadowTransforms[6];
            shadowTransforms[0] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 1, 0, 0 }), (Vec3) { 0, -1, 0 });
            shadowTransforms[1] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { -1, 0, 0 }), (Vec3) { 0, -1, 0 });
            shadowTransforms[2] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0, 1, 0 }), (Vec3) { 0, 0, 1 });
            shadowTransforms[3] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0, -1, 0 }), (Vec3) { 0, 0, -1 });
            shadowTransforms[4] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0, 0, 1 }), (Vec3) { 0, -1, 0 });
            shadowTransforms[5] = mat4_lookAt(light->position, vec3_add(light->position, (Vec3) { 0, 0, -1 }), (Vec3) { 0, -1, 0 });
            for (int j = 0; j < 6; ++j) { mat4_multiply(&shadowTransforms[j], &shadowProj, &shadowTransforms[j]); char uName[64]; sprintf(uName, "shadowMatrices[%d]", j); glUniformMatrix4fv(glGetUniformLocation(current_shader, uName), 1, GL_FALSE, shadowTransforms[j].m); }
            glUniform1f(glGetUniformLocation(current_shader, "far_plane"), light->shadowFarPlane); glUniform3fv(glGetUniformLocation(current_shader, "lightPos"), 1, &light->position.x);
        }
        else {
            current_shader = g_renderer.spotDepthShader; glUseProgram(current_shader);
            float angle_rad = acosf(fmaxf(-1.0f, fminf(1.0f, light->cutOff))); if (angle_rad < 0.01f) angle_rad = 0.01f;
            Mat4 lightProjection = mat4_perspective(angle_rad * 2.0f, 1.0f, 1.0f, light->shadowFarPlane);
            Vec3 up_vector = (Vec3){ 0, 1, 0 }; if (fabs(vec3_dot(light->direction, up_vector)) > 0.99f) { up_vector = (Vec3){ 1, 0, 0 }; }
            Mat4 lightView = mat4_lookAt(light->position, vec3_add(light->position, light->direction), up_vector);
            Mat4 lightSpaceMatrix; mat4_multiply(&lightSpaceMatrix, &lightProjection, &lightView);
            glUniformMatrix4fv(glGetUniformLocation(current_shader, "lightSpaceMatrix"), 1, GL_FALSE, lightSpaceMatrix.m);
        }
        for (int j = 0; j < g_scene.numObjects; ++j) render_object(current_shader, &g_scene.objects[j], false, NULL);
        for (int j = 0; j < g_scene.numBrushes; ++j) render_brush(current_shader, &g_scene.brushes[j], false, NULL);
    }
    glCullFace(GL_BACK); glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void render_water(Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix) {
    glUseProgram(g_renderer.waterShader);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUniformMatrix4fv(glGetUniformLocation(g_renderer.waterShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.waterShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "viewPos"), 1, &g_engine->camera.position.x);

    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "sun.enabled"), g_scene.sun.enabled);
    glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "sun.direction"), 1, &g_scene.sun.direction.x);
    glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "sun.color"), 1, &g_scene.sun.color.x);
    glUniform1f(glGetUniformLocation(g_renderer.waterShader, "sun.intensity"), g_scene.sun.intensity);

    glUniformMatrix4fv(glGetUniformLocation(g_renderer.waterShader, "sunLightSpaceMatrix"), 1, GL_FALSE, sunLightSpaceMatrix->m);

    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "numActiveLights"), g_scene.numActiveLights);

    Mat4 lightSpaceMatrices[MAX_LIGHTS];
    for (int i = 0; i < g_scene.numActiveLights; ++i) {
        Light* light = &g_scene.lights[i];
        if (light->type == LIGHT_SPOT) {
            float angle_rad = acosf(fmaxf(-1.0f, fminf(1.0f, light->cutOff))); if (angle_rad < 0.01f) angle_rad = 0.01f;
            Mat4 lightProjection = mat4_perspective(angle_rad * 2.0f, 1.0f, 1.0f, light->shadowFarPlane);
            Vec3 up_vector = (Vec3){ 0, 1, 0 }; if (fabs(vec3_dot(light->direction, up_vector)) > 0.99f) { up_vector = (Vec3){ 1, 0, 0 }; }
            Mat4 lightView = mat4_lookAt(light->position, vec3_add(light->position, light->direction), up_vector);
            mat4_multiply(&lightSpaceMatrices[i], &lightProjection, &lightView);
        }
        else {
            mat4_identity(&lightSpaceMatrices[i]);
        }
    }

    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "flashlight.enabled"), g_engine->flashlight_on);
    if (g_engine->flashlight_on) {
        Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
        vec3_normalize(&forward);
        glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "flashlight.position"), 1, &g_engine->camera.position.x);
        glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "flashlight.direction"), 1, &forward.x);
    }

    glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "cameraPosition"), 1, &g_engine->camera.position.x);
    glUniform1f(glGetUniformLocation(g_renderer.waterShader, "time"), g_engine->lastFrame);

    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, g_renderer.sunShadowMap);
    glUniform1i(glGetUniformLocation(g_renderer.waterShader, "sunShadowMap"), 11);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_renderer.dudvMap);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, g_renderer.waterNormalMap);

    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (!b->isWater) continue;

        int probe_idx = FindReflectionProbeForPoint(b->pos);
        GLuint reflectionTex;
        if (probe_idx != -1) {
            Brush* reflection_brush = &g_scene.brushes[probe_idx];
            reflectionTex = reflection_brush->cubemapTexture;

            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "useParallaxCorrection"), 1);

            Vec3 min_aabb = { FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 max_aabb = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (int k = 0; k < reflection_brush->numVertices; ++k) {
                Vec3 world_v = mat4_mul_vec3(&reflection_brush->modelMatrix, reflection_brush->vertices[k].pos);
                min_aabb.x = fminf(min_aabb.x, world_v.x); min_aabb.y = fminf(min_aabb.y, world_v.y); min_aabb.z = fminf(min_aabb.z, world_v.z);
                max_aabb.x = fmaxf(max_aabb.x, world_v.x); max_aabb.y = fmaxf(max_aabb.y, world_v.y); max_aabb.z = fmaxf(max_aabb.z, world_v.z);
            }
            glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "probeBoxMin"), 1, &min_aabb.x);
            glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "probeBoxMax"), 1, &max_aabb.x);
            glUniform3fv(glGetUniformLocation(g_renderer.waterShader, "probePosition"), 1, &reflection_brush->pos.x);
        }
        else {
            glUniform1i(glGetUniformLocation(g_renderer.waterShader, "useParallaxCorrection"), 0);
        }

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, reflectionTex);

        glUniformMatrix4fv(glGetUniformLocation(g_renderer.waterShader, "model"), 1, GL_FALSE, b->modelMatrix.m);
        glPatchParameteri(GL_PATCH_VERTICES, 3);
        glBindVertexArray(b->vao);
        glDrawArrays(GL_PATCHES, 0, b->totalRenderVertexCount);
    }
    glBindVertexArray(0);
}

void render_geometry_pass(Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix, bool unlit) {
    Frustum frustum;
    Mat4 view_proj;
    mat4_multiply(&view_proj, projection, view);
    extract_frustum_planes(&view_proj, &frustum, true);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.gBufferFBO);
    glViewport(0, 0, WINDOW_WIDTH / GEOMETRY_PASS_DOWNSAMPLE_FACTOR, WINDOW_HEIGHT / GEOMETRY_PASS_DOWNSAMPLE_FACTOR);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GLuint attachments[7] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6 };
    glDrawBuffers(7, attachments);
    if (Cvar_GetInt("r_faceculling")) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    else {
        glDisable(GL_CULL_FACE);
    }
    if (Cvar_GetInt("r_wireframe")) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    glEnable(GL_DEPTH_TEST); glUseProgram(g_renderer.mainShader);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.mainShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.mainShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform2f(glGetUniformLocation(g_renderer.mainShader, "viewportSize"), (float)(WINDOW_WIDTH / GEOMETRY_PASS_DOWNSAMPLE_FACTOR), (float)(WINDOW_HEIGHT / GEOMETRY_PASS_DOWNSAMPLE_FACTOR));
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.mainShader, "prevViewProjection"), 1, GL_FALSE, g_renderer.prevViewProjection.m);
    glUniform3fv(glGetUniformLocation(g_renderer.mainShader, "viewPos"), 1, &g_engine->camera.position.x);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "sun.enabled"), g_scene.sun.enabled);
    glUniform3fv(glGetUniformLocation(g_renderer.mainShader, "sun.direction"), 1, &g_scene.sun.direction.x);
    glUniform3fv(glGetUniformLocation(g_renderer.mainShader, "sun.color"), 1, &g_scene.sun.color.x);
    glUniform1f(glGetUniformLocation(g_renderer.mainShader, "sun.intensity"), g_scene.sun.intensity);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.mainShader, "sunLightSpaceMatrix"), 1, GL_FALSE, sunLightSpaceMatrix->m);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, g_renderer.sunShadowMap);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "sunShadowMap"), 11);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "is_unlit"), 0);
    glActiveTexture(GL_TEXTURE16);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "num_vpls"), g_scene.num_vpls);
    glBindTexture(GL_TEXTURE_2D, g_renderer.brdfLUTTexture);
    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "is_unlit"), unlit);

    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "numActiveLights"), g_scene.numActiveLights);

    if (g_scene.numActiveLights > 0) {
        ShaderLight shader_lights[MAX_LIGHTS];
        Mat4 lightSpaceMatrices[MAX_LIGHTS];
        for (int i = 0; i < g_scene.numActiveLights; ++i) {
            Light* light = &g_scene.lights[i];
            shader_lights[i].position.x = light->position.x;
            shader_lights[i].position.y = light->position.y;
            shader_lights[i].position.z = light->position.z;
            shader_lights[i].position.w = (float)light->type;

            shader_lights[i].direction.x = light->direction.x;
            shader_lights[i].direction.y = light->direction.y;
            shader_lights[i].direction.z = light->direction.z;

            shader_lights[i].color.x = light->color.x;
            shader_lights[i].color.y = light->color.y;
            shader_lights[i].color.z = light->color.z;
            shader_lights[i].color.w = light->intensity;

            shader_lights[i].params1.x = light->radius;
            shader_lights[i].params1.y = light->cutOff;
            shader_lights[i].params1.z = light->outerCutOff;

            shader_lights[i].params2.x = light->shadowFarPlane;
            shader_lights[i].params2.y = light->shadowBias;
            shader_lights[i].params2.z = light->volumetricIntensity;

            shader_lights[i].shadowMapHandle[0] = (unsigned int)(light->shadowMapHandle & 0xFFFFFFFF);
            shader_lights[i].shadowMapHandle[1] = (unsigned int)(light->shadowMapHandle >> 32);

            if (light->cookieMap != 0) {
                if (light->cookieMapHandle == 0) {
                    light->cookieMapHandle = glGetTextureHandleARB(light->cookieMap);
                    glMakeTextureHandleResidentARB(light->cookieMapHandle);
                }
                shader_lights[i].cookieMapHandle[0] = (unsigned int)(light->cookieMapHandle & 0xFFFFFFFF);
                shader_lights[i].cookieMapHandle[1] = (unsigned int)(light->cookieMapHandle >> 32);
            }
            else {
                shader_lights[i].cookieMapHandle[0] = 0;
                shader_lights[i].cookieMapHandle[1] = 0;
            }

            if (light->type == LIGHT_SPOT) {
                float angle_rad = acosf(fmaxf(-1.0f, fminf(1.0f, light->cutOff))); if (angle_rad < 0.01f) angle_rad = 0.01f;
                Mat4 lightProjection = mat4_perspective(angle_rad * 2.0f, 1.0f, 1.0f, light->shadowFarPlane);
                Vec3 up_vector = (Vec3){ 0, 1, 0 }; if (fabs(vec3_dot(light->direction, up_vector)) > 0.99f) { up_vector = (Vec3){ 1, 0, 0 }; }
                Mat4 lightView = mat4_lookAt(light->position, vec3_add(light->position, light->direction), up_vector);
                mat4_multiply(&lightSpaceMatrices[i], &lightProjection, &lightView);
            }
            else {
                mat4_identity(&lightSpaceMatrices[i]);
            }
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_renderer.lightSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, g_scene.numActiveLights * sizeof(ShaderLight), shader_lights);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    glUniform1i(glGetUniformLocation(g_renderer.mainShader, "flashlight.enabled"), g_engine->flashlight_on);
    if (g_engine->flashlight_on) {
        Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
        vec3_normalize(&forward);
        glUniform3fv(glGetUniformLocation(g_renderer.mainShader, "flashlight.position"), 1, &g_engine->camera.position.x);
        glUniform3fv(glGetUniformLocation(g_renderer.mainShader, "flashlight.direction"), 1, &forward.x);
    }
    for (int i = 0; i < g_scene.numObjects; i++) {
        SceneObject* obj = &g_scene.objects[i];
        glUniform1i(glGetUniformLocation(g_renderer.mainShader, "isBrush"), 0);
        if (obj->model) {
            Vec3 world_min = mat4_mul_vec3(&obj->modelMatrix, obj->model->aabb_min);
            Vec3 world_max = mat4_mul_vec3(&obj->modelMatrix, obj->model->aabb_max);
            Vec3 real_min = { fminf(world_min.x, world_max.x), fminf(world_min.y, world_max.y), fminf(world_min.z, world_max.z) };
            Vec3 real_max = { fmaxf(world_min.x, world_max.x), fmaxf(world_min.y, world_max.y), fmaxf(world_min.z, world_max.z) };

            if (!frustum_check_aabb(&frustum, real_min, real_max)) {
                continue;
            }
        }
        render_object(g_renderer.mainShader, &g_scene.objects[i], false, &frustum);
    }
    for (int i = 0; i < g_scene.numBrushes; i++) {
        Brush* b = &g_scene.brushes[i];
        glUniform1i(glGetUniformLocation(g_renderer.mainShader, "isBrush"), 1);
        if (b->isWater) continue;
        if (b->numVertices > 0) {
            Vec3 min_v = { FLT_MAX, FLT_MAX, FLT_MAX };
            Vec3 max_v = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (int j = 0; j < b->numVertices; ++j) {
                Vec3 p = mat4_mul_vec3(&b->modelMatrix, b->vertices[j].pos);
                min_v.x = fminf(min_v.x, p.x); min_v.y = fminf(min_v.y, p.y); min_v.z = fminf(min_v.z, p.z);
                max_v.x = fmaxf(max_v.x, p.x); max_v.y = fmaxf(max_v.y, p.y); max_v.z = fmaxf(max_v.z, p.z);
            }
            if (!frustum_check_aabb(&frustum, min_v, max_v)) {
                continue;
            }
        }
        render_brush(g_renderer.mainShader, &g_scene.brushes[i], false, &frustum);
    }
    render_parallax_rooms(view, projection);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); glUseProgram(g_renderer.mainShader);
    for (int i = 0; i < g_scene.numDecals; ++i) {
        Decal* d = &g_scene.decals[i]; glUniformMatrix4fv(glGetUniformLocation(g_renderer.mainShader, "model"), 1, GL_FALSE, d->modelMatrix.m);
        glUniform1f(glGetUniformLocation(g_renderer.mainShader, "heightScale"), 0.0f);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, d->material->diffuseMap); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, d->material->normalMap); glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, d->material->rmaMap);
        glBindVertexArray(g_renderer.decalVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    if (Cvar_GetInt("r_faceculling")) {
        glDisable(GL_CULL_FACE);
    }
    if (Cvar_GetInt("r_wireframe")) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    glBindVertexArray(0); glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_bloom_pass() {
    glUseProgram(g_renderer.bloomShader); glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.bloomFBO);
    glViewport(0, 0, WINDOW_WIDTH / BLOOM_DOWNSAMPLE, WINDOW_HEIGHT / BLOOM_DOWNSAMPLE);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_renderer.gLitColor);
    glBindVertexArray(g_renderer.quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
    bool horizontal = true, first_iteration = true; unsigned int amount = 10; glUseProgram(g_renderer.bloomBlurShader);
    for (unsigned int i = 0; i < amount; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.pingpongFBO[horizontal]); glUniform1i(glGetUniformLocation(g_renderer.bloomBlurShader, "horizontal"), horizontal);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, first_iteration ? g_renderer.bloomBrightnessTexture : g_renderer.pingpongColorbuffers[!horizontal]);
        glBindVertexArray(g_renderer.quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal; if (first_iteration) first_iteration = false;
    }
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_volumetric_pass(Mat4* view, Mat4* projection, const Mat4* sunLightSpaceMatrix) {
    bool should_render_volumetrics = false;
    if (g_scene.sun.enabled && g_scene.sun.volumetricIntensity > 0.001f) {
        should_render_volumetrics = true;
    }
    if (!should_render_volumetrics) {
        for (int i = 0; i < g_scene.numActiveLights; ++i) {
            if (g_scene.lights[i].intensity > 0.001f && g_scene.lights[i].volumetricIntensity > 0.001f) {
                should_render_volumetrics = true;
                break;
            }
        }
    }

    if (!should_render_volumetrics) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.volumetricFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.volPingpongFBO[0]);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.volumetricFBO);
    glViewport(0, 0, WINDOW_WIDTH / VOLUMETRIC_DOWNSAMPLE, WINDOW_HEIGHT / VOLUMETRIC_DOWNSAMPLE);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_renderer.volumetricShader);
    glUniform3fv(glGetUniformLocation(g_renderer.volumetricShader, "viewPos"), 1, &g_engine->camera.position.x);

    Mat4 invView, invProj;
    mat4_inverse(view, &invView);
    mat4_inverse(projection, &invProj);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.volumetricShader, "invView"), 1, GL_FALSE, invView.m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.volumetricShader, "invProjection"), 1, GL_FALSE, invProj.m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.volumetricShader, "projection"), 1, GL_FALSE, projection->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.volumetricShader, "view"), 1, GL_FALSE, view->m);

    glUniform1i(glGetUniformLocation(g_renderer.volumetricShader, "numActiveLights"), g_scene.numActiveLights);

    glUniform1i(glGetUniformLocation(g_renderer.volumetricShader, "sun.enabled"), g_scene.sun.enabled);
    if (g_scene.sun.enabled) {
        glActiveTexture(GL_TEXTURE15);
        glBindTexture(GL_TEXTURE_2D, g_renderer.sunShadowMap);
        glUniform1i(glGetUniformLocation(g_renderer.volumetricShader, "sunShadowMap"), 15);
        glUniformMatrix4fv(glGetUniformLocation(g_renderer.volumetricShader, "sunLightSpaceMatrix"), 1, GL_FALSE, sunLightSpaceMatrix->m);
        glUniform3fv(glGetUniformLocation(g_renderer.volumetricShader, "sun.direction"), 1, &g_scene.sun.direction.x);
        glUniform3fv(glGetUniformLocation(g_renderer.volumetricShader, "sun.color"), 1, &g_scene.sun.color.x);
        glUniform1f(glGetUniformLocation(g_renderer.volumetricShader, "sun.intensity"), g_scene.sun.intensity);
        glUniform1f(glGetUniformLocation(g_renderer.volumetricShader, "sun.volumetricIntensity"), g_scene.sun.volumetricIntensity);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gPosition);

    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    bool horizontal = true, first_iteration = true;
    unsigned int amount = 4;
    glUseProgram(g_renderer.volumetricBlurShader);
    for (unsigned int i = 0; i < amount; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.volPingpongFBO[horizontal]);
        glUniform1i(glGetUniformLocation(g_renderer.volumetricBlurShader, "horizontal"), horizontal);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, first_iteration ? g_renderer.volumetricTexture : g_renderer.volPingpongTextures[!horizontal]);
        glBindVertexArray(g_renderer.quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
}

void render_ssao_pass(Mat4* projection) {
    const int ssao_width = WINDOW_WIDTH / SSAO_DOWNSAMPLE;
    const int ssao_height = WINDOW_HEIGHT / SSAO_DOWNSAMPLE;
    glViewport(0, 0, ssao_width, ssao_height);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_renderer.ssaoShader);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.ssaoShader, "projection"), 1, GL_FALSE, projection->m);
    glUniform2f(glGetUniformLocation(g_renderer.ssaoShader, "screenSize"), (float)ssao_width, (float)ssao_height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gPosition);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gNormal);
    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_renderer.ssaoBlurShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.ssaoColorBuffer);
    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_lighting_composite_pass(Mat4* view, Mat4* projection) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.finalRenderFBO);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_renderer.postProcessShader);
    glUniform2f(glGetUniformLocation(g_renderer.postProcessShader, "resolution"), WINDOW_WIDTH, WINDOW_HEIGHT);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "time"), g_engine->lastFrame);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_exposure"), g_renderer.currentExposure);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_fogEnabled"), g_scene.fog.enabled);
    glUniform3fv(glGetUniformLocation(g_renderer.postProcessShader, "u_fogColor"), 1, &g_scene.fog.color.x);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_fogStart"), g_scene.fog.start);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_fogEnd"), g_scene.fog.end);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_postEnabled"), g_scene.post.enabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_crtCurvature"), g_scene.post.crtCurvature);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_vignetteStrength"), g_scene.post.vignetteStrength);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_vignetteRadius"), g_scene.post.vignetteRadius);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_lensFlareEnabled"), g_scene.post.lensFlareEnabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_lensFlareStrength"), g_scene.post.lensFlareStrength);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_scanlineStrength"), g_scene.post.scanlineStrength);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_grainIntensity"), g_scene.post.grainIntensity);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_chromaticAberrationEnabled"), g_scene.post.chromaticAberrationEnabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_chromaticAberrationStrength"), g_scene.post.chromaticAberrationStrength);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_sharpenEnabled"), g_scene.post.sharpenEnabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_sharpenAmount"), g_scene.post.sharpenAmount);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_bwEnabled"), g_scene.post.bwEnabled);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "u_bwStrength"), g_scene.post.bwStrength);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_bloomEnabled"), Cvar_GetInt("r_bloom"));
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_volumetricsEnabled"), Cvar_GetInt("r_volumetrics"));
    Vec2 light_pos_on_screen = { -2.0, -2.0 }; float flare_intensity = 0.0;
    if (g_scene.numActiveLights > 0) {
        Vec3 light_world_pos = g_scene.lights[0].position; Mat4 view_proj; mat4_multiply(&view_proj, projection, view); float clip_space_pos[4]; float w = 1.0f;
        clip_space_pos[0] = view_proj.m[0] * light_world_pos.x + view_proj.m[4] * light_world_pos.y + view_proj.m[8] * light_world_pos.z + view_proj.m[12] * w;
        clip_space_pos[1] = view_proj.m[1] * light_world_pos.x + view_proj.m[5] * light_world_pos.y + view_proj.m[9] * light_world_pos.z + view_proj.m[13] * w;
        clip_space_pos[2] = view_proj.m[2] * light_world_pos.x + view_proj.m[6] * light_world_pos.y + view_proj.m[10] * light_world_pos.z + view_proj.m[14] * w;
        clip_space_pos[3] = view_proj.m[3] * light_world_pos.x + view_proj.m[7] * light_world_pos.y + view_proj.m[11] * light_world_pos.z + view_proj.m[15] * w;
        float clip_w = clip_space_pos[3];
        if (clip_w > 0) {
            float ndc_x = clip_space_pos[0] / clip_w; float ndc_y = clip_space_pos[1] / clip_w;
            if (ndc_x > -1.0 && ndc_x < 1.0 && ndc_y > -1.0 && ndc_y < 1.0) { light_pos_on_screen.x = ndc_x * 0.5 + 0.5; light_pos_on_screen.y = ndc_y * 0.5 + 0.5; flare_intensity = 1.0; }
            glUniform3fv(glGetUniformLocation(g_renderer.postProcessShader, "u_flareLightWorldPos"), 1, &light_world_pos.x);
            glUniformMatrix4fv(glGetUniformLocation(g_renderer.postProcessShader, "u_view"), 1, GL_FALSE, view->m);
        }
    }
    glUniform2fv(glGetUniformLocation(g_renderer.postProcessShader, "lightPosOnScreen"), 1, &light_pos_on_screen.x);
    glUniform1f(glGetUniformLocation(g_renderer.postProcessShader, "flareIntensity"), flare_intensity);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, g_renderer.gLitColor);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, g_renderer.pingpongColorbuffers[0]);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, g_renderer.gPosition);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, g_renderer.volPingpongTextures[0]);
    glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, g_renderer.gIndirectLight);
    if (Cvar_GetInt("r_ssao")) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, g_renderer.ssaoBlurColorBuffer);
    }
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "sceneTexture"), 0);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "bloomBlur"), 1);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "gPosition"), 2);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "volumetricTexture"), 3);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "gIndirectLight"), 5);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "ssao"), 4);
    glUniform1i(glGetUniformLocation(g_renderer.postProcessShader, "u_ssaoEnabled"), Cvar_GetInt("r_ssao"));
    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void render_skybox(Mat4* view, Mat4* projection) {
    glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.finalRenderFBO);
    glDepthFunc(GL_LEQUAL);
    glUseProgram(g_renderer.skyboxShader);
    glCullFace(GL_FRONT);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.skyboxShader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(g_renderer.skyboxShader, "projection"), 1, GL_FALSE, projection->m);

    Vec3 sunDirNormalized = g_scene.sun.direction;
    vec3_normalize(&sunDirNormalized);

    glUniform3fv(glGetUniformLocation(g_renderer.skyboxShader, "sunDirection"), 1, &sunDirNormalized.x);
    glUniform3fv(glGetUniformLocation(g_renderer.skyboxShader, "cameraPos"), 1, &g_engine->camera.position.x);
    glUniform1i(glGetUniformLocation(g_renderer.skyboxShader, "cloudMap"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.cloudTexture);

    glUniform1f(glGetUniformLocation(g_renderer.skyboxShader, "time"), g_engine->lastFrame);

    glBindVertexArray(g_renderer.skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glCullFace(GL_BACK);
    glDepthFunc(GL_LESS);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void present_final_image(GLuint source_fbo) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void cleanup() {
    Physics_DestroyWorld(g_engine->physicsWorld);
    for (int i = 0; i < g_scene.numParticleEmitters; i++) {
        ParticleEmitter_Free(&g_scene.particleEmitters[i]);
        ParticleSystem_Free(g_scene.particleEmitters[i].system);
    }
    for (int i = 0; i < g_scene.numParallaxRooms; ++i) {
        if (g_scene.parallaxRooms[i].cubemapTexture) glDeleteTextures(1, &g_scene.parallaxRooms[i].cubemapTexture);
    }
    for (int i = 0; i < g_scene.numActiveLights; i++) Light_DestroyShadowMap(&g_scene.lights[i]);
    for (int i = 0; i < g_scene.numBrushes; ++i) {
        if (g_scene.brushes[i].isReflectionProbe) {
            glDeleteTextures(1, &g_scene.brushes[i].cubemapTexture);
        }
        Brush_FreeData(&g_scene.brushes[i]);
    }
    if (g_scene.objects) {
        for (int i = 0; i < g_scene.numObjects; ++i) {
            if (g_scene.objects[i].model) Model_Free(g_scene.objects[i].model);
        }
        free(g_scene.objects);
        g_scene.objects = NULL;
    }
    glDeleteProgram(g_renderer.mainShader);
    glDeleteProgram(g_renderer.pointDepthShader);
    glDeleteProgram(g_renderer.vplGenerationShader);
    glDeleteProgram(g_renderer.vplComputeShader);
    glDeleteProgram(g_renderer.debugBufferShader);
    glDeleteProgram(g_renderer.spotDepthShader);
    glDeleteProgram(g_renderer.skyboxShader);
    glDeleteProgram(g_renderer.postProcessShader);
    glDeleteProgram(g_renderer.bloomShader);
    glDeleteProgram(g_renderer.bloomBlurShader);
    glDeleteProgram(g_renderer.dofShader);
    glDeleteProgram(g_renderer.ssaoShader);
    glDeleteProgram(g_renderer.ssaoBlurShader);
    glDeleteProgram(g_renderer.parallaxInteriorShader);
    glDeleteProgram(g_renderer.volumetricShader);
    glDeleteProgram(g_renderer.volumetricBlurShader);
    glDeleteProgram(g_renderer.histogramShader);
    glDeleteProgram(g_renderer.exposureShader);
    glDeleteProgram(g_renderer.depthAaShader);
    glDeleteProgram(g_renderer.motionBlurShader);
    glDeleteFramebuffers(1, &g_renderer.gBufferFBO);
    glDeleteTextures(1, &g_renderer.gLitColor);
    glDeleteTextures(1, &g_renderer.gPosition);
    glDeleteTextures(1, &g_renderer.gNormal);
    glDeleteTextures(1, &g_renderer.gAlbedo);
    glDeleteTextures(1, &g_renderer.gPBRParams);
    glDeleteTextures(1, &g_renderer.gVelocity);
    glDeleteFramebuffers(1, &g_renderer.vplGenerationFBO);
    glDeleteTextures(1, &g_renderer.vplPosTex);
    glDeleteTextures(1, &g_renderer.vplNormalTex);
    glDeleteTextures(1, &g_renderer.vplAlbedoTex);
    glDeleteBuffers(1, &g_renderer.vplSSBO);
    glDeleteFramebuffers(1, &g_renderer.ssaoFBO);
    glDeleteFramebuffers(1, &g_renderer.ssaoBlurFBO);
    glDeleteTextures(1, &g_renderer.ssaoColorBuffer);
    glDeleteTextures(1, &g_renderer.ssaoBlurColorBuffer);
    glDeleteFramebuffers(1, &g_renderer.finalRenderFBO);
    glDeleteTextures(1, &g_renderer.finalRenderTexture);
    glDeleteTextures(1, &g_renderer.finalDepthTexture);
    glDeleteFramebuffers(1, &g_renderer.postProcessFBO);
    glDeleteTextures(1, &g_renderer.postProcessTexture);
    glDeleteVertexArrays(1, &g_renderer.quadVAO);
    glDeleteBuffers(1, &g_renderer.quadVBO);
    glDeleteVertexArrays(1, &g_renderer.skyboxVAO);
    glDeleteBuffers(1, &g_renderer.skyboxVBO);
    glDeleteFramebuffers(1, &g_renderer.sunShadowFBO);
    glDeleteTextures(1, &g_renderer.sunShadowMap);
    glDeleteVertexArrays(1, &g_renderer.decalVAO); glDeleteBuffers(1, &g_renderer.decalVBO);
    glDeleteVertexArrays(1, &g_renderer.parallaxRoomVAO); glDeleteBuffers(1, &g_renderer.parallaxRoomVBO);
    glDeleteFramebuffers(1, &g_renderer.bloomFBO); glDeleteTextures(1, &g_renderer.bloomBrightnessTexture);
    glDeleteFramebuffers(2, g_renderer.pingpongFBO); glDeleteTextures(2, g_renderer.pingpongColorbuffers);
    glDeleteFramebuffers(1, &g_renderer.volumetricFBO);
    glDeleteTextures(1, &g_renderer.volumetricTexture);
    glDeleteFramebuffers(2, g_renderer.volPingpongFBO);
    glDeleteTextures(2, g_renderer.volPingpongTextures);
    glDeleteBuffers(1, &g_renderer.lightSSBO);
    glDeleteTextures(1, &g_renderer.dudvMap);
    glDeleteTextures(1, &g_renderer.waterNormalMap);
    glDeleteBuffers(1, &g_renderer.histogramSSBO);
    glDeleteBuffers(1, &g_renderer.exposureSSBO);
    VideoPlayer_ShutdownSystem();
    SoundSystem_DeleteBuffer(g_flashlight_sound_buffer);
    SoundSystem_DeleteBuffer(g_footstep_sound_buffer);
    TextureManager_Shutdown();
    SoundSystem_Shutdown();
    IO_Shutdown();
    Binds_Shutdown();
    Cvar_Save("cvars.txt");
    Editor_Shutdown();
    Network_Shutdown();
    UI_Shutdown();
    Discord__Shutdown();
    SDL_GL_DeleteContext(g_engine->context);
    SDL_DestroyWindow(g_engine->window);
    IMG_Quit();
    SDL_Quit();
}

void render_autoexposure_pass() {
    bool auto_exposure_enabled = Cvar_GetInt("r_autoexposure");

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_renderer.histogramSSBO);
    GLuint zero = 0;
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    glUseProgram(g_renderer.histogramShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gLitColor);
    glUniform1i(glGetUniformLocation(g_renderer.histogramShader, "u_inputTexture"), 0);
    glDispatchCompute((GLuint)(WINDOW_WIDTH / 16), (GLuint)(WINDOW_HEIGHT / 16), 1);

    glUseProgram(g_renderer.exposureShader);
    glUniform1f(glGetUniformLocation(g_renderer.exposureShader, "u_autoexposure_key"), Cvar_GetFloat("r_autoexposure_key"));
    glUniform1f(glGetUniformLocation(g_renderer.exposureShader, "u_autoexposure_speed"), Cvar_GetFloat("r_autoexposure_speed"));
    glUniform1f(glGetUniformLocation(g_renderer.exposureShader, "u_deltaTime"), g_engine->deltaTime);
    glUniform1i(glGetUniformLocation(g_renderer.exposureShader, "u_autoexposure_enabled"), auto_exposure_enabled);

    glDispatchCompute(1, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void render_dof_pass(GLuint sourceTexture, GLuint sourceDepthTexture, GLuint destFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, destFBO);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_renderer.dofShader);
    glUniform1f(glGetUniformLocation(g_renderer.dofShader, "u_focusDistance"), g_scene.post.dofFocusDistance);
    glUniform1f(glGetUniformLocation(g_renderer.dofShader, "u_aperture"), g_scene.post.dofAperture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(g_renderer.dofShader, "screenTexture"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, sourceDepthTexture);
    glUniform1i(glGetUniformLocation(g_renderer.dofShader, "depthTexture"), 1);
    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_depth_aa_pass(GLuint sourceTexture, GLuint destFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, destFBO);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_renderer.depthAaShader);
    glUniform2f(glGetUniformLocation(g_renderer.depthAaShader, "screenSize"), (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(g_renderer.depthAaShader, "finalImage"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gPosition);
    glUniform1i(glGetUniformLocation(g_renderer.depthAaShader, "gPosition"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gNormal);
    glUniform1i(glGetUniformLocation(g_renderer.depthAaShader, "gNormal"), 2);

    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render_motion_blur_pass(GLuint sourceTexture, GLuint destFBO) {
    glBindFramebuffer(GL_FRAMEBUFFER, destFBO);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_renderer.motionBlurShader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glUniform1i(glGetUniformLocation(g_renderer.motionBlurShader, "sceneTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_renderer.gVelocity);
    glUniform1i(glGetUniformLocation(g_renderer.motionBlurShader, "velocityTexture"), 1);

    glBindVertexArray(g_renderer.quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SaveFramebufferToPNG(GLuint fbo, int width, int height, const char* filepath) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    unsigned char* pixels = (unsigned char*)malloc(width * height * 4);
    if (!pixels) {
        Console_Printf("[ERROR] Failed to allocate memory for screenshot pixels.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    unsigned char* temp_row = (unsigned char*)malloc(width * 4);
    for (int y = 0; y < height / 2; ++y) {
        memcpy(temp_row, pixels + y * width * 4, width * 4);
        memcpy(pixels + y * width * 4, pixels + (height - 1 - y) * width * 4, width * 4);
        memcpy(pixels + (height - 1 - y) * width * 4, temp_row, width * 4);
    }
    free(temp_row);

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, width * 4,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

    if (!surface) {
        Console_Printf("[ERROR] Failed to create SDL surface for screenshot.");
    }
    else {
        if (IMG_SavePNG(surface, filepath) != 0) {
            Console_Printf("[ERROR] Failed to save screenshot to %s: %s", filepath, IMG_GetError());
        }
        else {
            Console_Printf("Saved cubemap face to %s", filepath);
        }
        SDL_FreeSurface(surface);
    }

    free(pixels);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void BuildCubemaps() {
    Console_Printf("Starting cubemap build...");
    glFinish();

    Camera original_camera = g_engine->camera;

    Vec3 targets[] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    Vec3 ups[] = { {0,-1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {0,-1,0}, {0,-1,0} };
    const char* suffixes[] = { "px", "nx", "py", "ny", "pz", "nz" };

    const int CUBEMAP_RES = 256;
    GLuint cubemap_fbo, cubemap_texture, cubemap_rbo;
    glGenFramebuffers(1, &cubemap_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, cubemap_fbo);
    glGenTextures(1, &cubemap_texture);
    glBindTexture(GL_TEXTURE_2D, cubemap_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, CUBEMAP_RES, CUBEMAP_RES, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cubemap_texture, 0);
    glGenRenderbuffers(1, &cubemap_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, cubemap_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, CUBEMAP_RES, CUBEMAP_RES);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, cubemap_rbo);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Console_Printf("[ERROR] Cubemap face FBO not complete!");
        glDeleteFramebuffers(1, &cubemap_fbo);
        glDeleteTextures(1, &cubemap_texture);
        glDeleteRenderbuffers(1, &cubemap_rbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    for (int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (!b->isReflectionProbe) continue;
        if (strlen(b->name) == 0) {
            Console_Printf("[WARNING] Skipping unnamed reflection probe at index %d.", i);
            continue;
        }

        Console_Printf("Building cubemap for probe '%s'...", b->name);

        for (int face_idx = 0; face_idx < 6; ++face_idx) {
            g_engine->camera.position = b->pos;
            Vec3 target_pos = vec3_add(g_engine->camera.position, targets[face_idx]);
            Mat4 view = mat4_lookAt(g_engine->camera.position, target_pos, ups[face_idx]);
            Mat4 projection = mat4_perspective(90.0f * (3.14159f / 180.f), 1.0f, 0.1f, 1000.f);

            render_shadows();
            Mat4 sunLightSpaceMatrix;
            mat4_identity(&sunLightSpaceMatrix);
            if (g_scene.sun.enabled) {
                Calculate_Sun_Light_Space_Matrix(&sunLightSpaceMatrix, &g_scene.sun, g_engine->camera.position);
                render_sun_shadows(&sunLightSpaceMatrix);
            }

            render_geometry_pass(&view, &projection, &sunLightSpaceMatrix, false);

            glBindFramebuffer(GL_FRAMEBUFFER, cubemap_fbo);
            glViewport(0, 0, CUBEMAP_RES, CUBEMAP_RES);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glEnable(GL_FRAMEBUFFER_SRGB);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, g_renderer.gBufferFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cubemap_fbo);
            glBlitFramebuffer(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, CUBEMAP_RES, CUBEMAP_RES, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBlitFramebuffer(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, CUBEMAP_RES, CUBEMAP_RES, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

            glBindFramebuffer(GL_FRAMEBUFFER, cubemap_fbo);
            render_skybox(&view, &projection);

            glDisable(GL_FRAMEBUFFER_SRGB);

            char filepath[256];
            sprintf(filepath, "cubemaps/%s_%s.png", b->name, suffixes[face_idx]);
            SaveFramebufferToPNG(cubemap_fbo, CUBEMAP_RES, CUBEMAP_RES, filepath);
        }

        const char* face_paths[6];
        char paths_storage[6][256];
        for (int k = 0; k < 6; ++k) {
            sprintf(paths_storage[k], "cubemaps/%s_%s.png", b->name, suffixes[k]);
            face_paths[k] = paths_storage[k];
        }
        b->cubemapTexture = TextureManager_ReloadCubemap(face_paths, b->cubemapTexture);
    }

    glDeleteFramebuffers(1, &cubemap_fbo);
    glDeleteTextures(1, &cubemap_texture);
    glDeleteRenderbuffers(1, &cubemap_rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    g_engine->camera = original_camera;
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    Console_Printf("Cubemap build finished.");
}

static void render_debug_buffer(GLuint textureID, int viewMode) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_renderer.debugBufferShader);
    glUniform1i(glGetUniformLocation(g_renderer.debugBufferShader, "viewMode"), viewMode);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(g_renderer.debugBufferShader, "debugTexture"), 0);

    glBindVertexArray(g_renderer.quadVAO);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO); IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window = SDL_CreateWindow("Tectonic Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    glewExperimental = GL_TRUE; glewInit();
    init_engine(window, context);
    if (Cvar_GetInt("r_vsync")) {
        SDL_GL_SetSwapInterval(1);
    }
    else {
        SDL_GL_SetSwapInterval(0);
    }
    if (!GLEW_ARB_bindless_texture) {
        fprintf(stderr, "FATAL ERROR: GL_ARB_bindless_texture is not supported by your GPU/drivers.\n");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "GPU Feature Missing", "Your graphics card does not support bindless textures (GL_ARB_bindless_texture), which is required by this engine.", window);
        return -1;
    }
    SDL_SetRelativeMouseMode(SDL_FALSE);
    MainMenu_SetInGameMenuMode(false, false);

    g_fps_last_update = SDL_GetTicks();
    while (g_engine->running) {
        Uint32 frameStartTicks = SDL_GetTicks();
        int current_vsync_cvar = Cvar_GetInt("r_vsync");
        if (current_vsync_cvar != g_last_vsync_cvar_state) {
            if (SDL_GL_SetSwapInterval(current_vsync_cvar) == 0) {
                Console_Printf("V-Sync set to %s.", current_vsync_cvar ? "ON" : "OFF");
            }
            else {
                Console_Printf("[warning] Could not set V-Sync: %s", SDL_GetError());
            }
            g_last_vsync_cvar_state = current_vsync_cvar;
        }
        float currentFrame = (float)SDL_GetTicks() / 1000.0f; g_engine->deltaTime = currentFrame - g_engine->lastFrame; g_engine->lastFrame = currentFrame;
        g_fps_frame_count++;
        Uint32 currentTicks = SDL_GetTicks();
        if (currentTicks - g_fps_last_update >= 1000) {
            g_fps_display = (float)g_fps_frame_count / ((float)(currentTicks - g_fps_last_update) / 1000.0f);
            g_fps_last_update = currentTicks;
            g_fps_frame_count = 0;
        }
        process_input(); update_state();
        if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
            const GameConfig* config = GameConfig_Get();
            if (g_current_mode == MODE_MAINMENU) {
                Discord_Update(config->gamename, "In Main Menu");
            }
            else {
                Discord_Update(config->gamename, "Paused");
            }
            MainMenu_Render();
        }
        else if (g_current_mode == MODE_GAME) {
            char details_str[128];
            if (Cvar_GetInt("r_vpl")) {
                if (Cvar_GetInt("r_vpl_static")) {
                    if (!g_scene.static_vpls_generated) {
                        Console_Printf("Generating static VPLs for the map...");
                        render_vpl_pass();
                        g_scene.static_vpls_generated = true;
                        Console_Printf("Static VPL generation complete. %d VPLs generated.", g_scene.num_vpls);
                    }
                }
                else {
                    render_vpl_pass();
                }
            }
            else {
                g_scene.num_vpls = 0;
            }
            sprintf(details_str, "Map: %s", g_scene.mapPath);
            Discord_Update("Playing", details_str);
            Vec3 f = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw),sinf(g_engine->camera.pitch),-cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) }; vec3_normalize(&f);
            Vec3 t = vec3_add(g_engine->camera.position, f);
            Mat4 view = mat4_lookAt(g_engine->camera.position, t, (Vec3) { 0, 1, 0 });
            float fov_degrees = Cvar_GetFloat("fov_vertical");
            Mat4 projection = mat4_perspective(fov_degrees * (3.14159f / 180.f), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 1000.f);
            Mat4 sunLightSpaceMatrix;
            mat4_identity(&sunLightSpaceMatrix);

            if (Cvar_GetInt("r_shadows")) {
                render_shadows();
                if (g_scene.sun.enabled) {
                    Calculate_Sun_Light_Space_Matrix(&sunLightSpaceMatrix, &g_scene.sun, g_engine->camera.position);
                    render_sun_shadows(&sunLightSpaceMatrix);
                }
            }
            render_geometry_pass(&view, &projection, &sunLightSpaceMatrix, false);
            if (Cvar_GetInt("r_ssao")) {
                render_ssao_pass(&projection);
            }
            if (Cvar_GetInt("r_volumetrics")) {
                render_volumetric_pass(&view, &projection, &sunLightSpaceMatrix);
            }
            if (Cvar_GetInt("r_bloom")) {
                render_bloom_pass();
            }
            render_autoexposure_pass();
            render_lighting_composite_pass(&view, &projection);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, g_renderer.gBufferFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_renderer.finalRenderFBO);
            const int LOW_RES_WIDTH = WINDOW_WIDTH / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
            const int LOW_RES_HEIGHT = WINDOW_HEIGHT / GEOMETRY_PASS_DOWNSAMPLE_FACTOR;
            glBlitFramebuffer(0, 0, LOW_RES_WIDTH, LOW_RES_HEIGHT, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            render_skybox(&view, &projection);
            glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.finalRenderFBO);
            for (int i = 0; i < g_scene.numVideoPlayers; ++i) {
                VideoPlayer_Render(&g_scene.videoPlayers[i], &view, &projection);
            }
            glEnable(GL_BLEND);
            glDepthMask(GL_FALSE);
            render_water(&view, &projection, &sunLightSpaceMatrix);
            for (int i = 0; i < g_scene.numParticleEmitters; ++i) {
                ParticleEmitter_Render(&g_scene.particleEmitters[i], view, projection);
            }
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            GLuint source_fbo = g_renderer.finalRenderFBO;
            GLuint source_tex = g_renderer.finalRenderTexture;
            if (g_scene.post.dofEnabled) {
                render_dof_pass(source_tex, g_renderer.finalDepthTexture, g_renderer.postProcessFBO);
                source_fbo = g_renderer.postProcessFBO;
                source_tex = g_renderer.postProcessTexture;
            }

            if (Cvar_GetInt("r_motionblur")) {
                GLuint target_fbo = (source_fbo == g_renderer.finalRenderFBO) ? g_renderer.postProcessFBO : g_renderer.finalRenderFBO;
                render_motion_blur_pass(source_tex, target_fbo);
                source_fbo = target_fbo;
                source_tex = (source_fbo == g_renderer.finalRenderFBO) ? g_renderer.finalRenderTexture : g_renderer.postProcessTexture;
            }

            if (Cvar_GetInt("r_depth_aa")) {
                GLuint target_fbo = (source_fbo == g_renderer.finalRenderFBO) ? g_renderer.postProcessFBO : g_renderer.finalRenderFBO;
                render_depth_aa_pass(source_tex, target_fbo);
                source_fbo = target_fbo;
            }

            bool debug_view_active = false;
            if (Cvar_GetInt("r_debug_albedo")) { render_debug_buffer(g_renderer.gAlbedo, 5); debug_view_active = true; }
            else if (Cvar_GetInt("r_debug_normals")) { render_debug_buffer(g_renderer.gNormal, 5); debug_view_active = true; }
            else if (Cvar_GetInt("r_debug_position")) { render_debug_buffer(g_renderer.gPosition, 5); debug_view_active = true; }
            else if (Cvar_GetInt("r_debug_metallic")) { render_debug_buffer(g_renderer.gPBRParams, 1); debug_view_active = true; }
            else if (Cvar_GetInt("r_debug_roughness")) { render_debug_buffer(g_renderer.gPBRParams, 2); debug_view_active = true; }
            else if (Cvar_GetInt("r_debug_ao")) { render_debug_buffer(g_renderer.ssaoBlurColorBuffer, 1); debug_view_active = true; }
            else if (Cvar_GetInt("r_debug_velocity")) { render_debug_buffer(g_renderer.gVelocity, 0); debug_view_active = true; }
            else if (Cvar_GetInt("r_debug_volumetric")) { render_debug_buffer(g_renderer.volPingpongTextures[0], 0); debug_view_active = true; }
            else if (Cvar_GetInt("r_debug_bloom")) { render_debug_buffer(g_renderer.bloomBrightnessTexture, 0); debug_view_active = true; }
            else if (Cvar_GetInt("r_debug_vpl")) { render_debug_buffer(g_renderer.gIndirectLight, 6); debug_view_active = true; }

            if (!debug_view_active) {
                present_final_image(source_fbo);
            }
            Mat4 currentViewProjection;
            mat4_multiply(&currentViewProjection, &projection, &view);
            g_renderer.prevViewProjection = currentViewProjection;
        }
        else {
            char details_str[128];
            sprintf(details_str, "Map: %s", g_scene.mapPath);
            Discord_Update("In the Editor", details_str);
            Editor_RenderAllViewports(g_engine, &g_renderer, &g_scene);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        UI_BeginFrame();
        if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
        }
        else if (g_current_mode == MODE_EDITOR) { Editor_RenderUI(g_engine, &g_scene, &g_renderer); }
        else {
            UI_RenderGameHUD(g_fps_display, g_engine->camera.position.x, g_engine->camera.position.y, g_engine->camera.position.z);
        }
        Console_Draw(); 
        int vsync_enabled = Cvar_GetInt("r_vsync");
        int fps_max = Cvar_GetInt("fps_max");

        if (vsync_enabled == 0 && fps_max > 0) {
            float targetFrameTimeMs = 1000.0f / (float)fps_max;
            Uint32 frameTicks = SDL_GetTicks() - frameStartTicks;
            if (frameTicks < targetFrameTimeMs) {
                SDL_Delay((Uint32)(targetFrameTimeMs - frameTicks));
            }
        }
        UI_EndFrame(window);
    }
    cleanup(); return 0;
}