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
#include "map.h"
#include "map_lighting.h"
#include "map_misc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "math_lib.h"
#include "physics_wrapper.h"
#include "cvar.h"
#include "sound_system.h"
#include "io_system.h"
#include "gl_video_player.h"
#include "gl_console.h"
#include "gl_render_misc.h"
#include "water_manager.h"
#include "mikktspace.h"
#include <float.h>
#include <time.h>
#include <sys/stat.h>

void Scene_Clear(Scene* scene, Engine* engine) {
    IO_Clear();

    if (scene->objects) {
        for (Int i = 0; i < scene->numObjects; ++i) {
            if (scene->objects[i].model) {
                Model_Free(scene->objects[i].model);
            }
            if (scene->objects[i].bakedVertexColors) {
                delete[] scene->objects[i].bakedVertexColors;
                scene->objects[i].bakedVertexColors = nullptr;
            }
            if (scene->objects[i].bakedVertexDirections) {
                delete[] scene->objects[i].bakedVertexDirections;
                scene->objects[i].bakedVertexDirections = nullptr;
            }
            if (scene->objects[i].lightmapHandle) {
                glMakeTextureHandleNonResidentARB(scene->objects[i].lightmapHandle);
                glDeleteTextures(1, &scene->objects[i].lightmapTexture);
            }
            if (scene->objects[i].dirLightmapHandle) {
                glMakeTextureHandleNonResidentARB(scene->objects[i].dirLightmapHandle);
                glDeleteTextures(1, &scene->objects[i].dirLightmapTexture);
            }
        }
        delete[] scene->objects;
        scene->objects = nullptr;
    }

    for (Int i = 0; i < scene->numBrushes; ++i) {
        for (Int j = 0; j < scene->brushes[i].numFaces; ++j) {
            if (scene->brushes[i].lightmapAtlas != 0) {
                glDeleteTextures(1, &scene->brushes[i].lightmapAtlas);
            }
            if (scene->brushes[i].directionalLightmapAtlas != 0) {
                glDeleteTextures(1, &scene->brushes[i].directionalLightmapAtlas);
            }
        }
        Brush_FreeData(&scene->brushes[i]);
        scene->brushes[i].physicsBody = nullptr;
    }

    for (Int i = 0; i < scene->numActiveLights; ++i) {
        Light_DestroyShadowMap(&scene->lights[i]);
    }

    for (Int i = 0; i < scene->numSoundEntities; ++i) {
        Sound::SoundSystem_DeleteSource(scene->soundEntities[i].sourceID);
        Sound::SoundSystem_DeleteBuffer(scene->soundEntities[i].bufferID);
    }

    for (Int i = 0; i < scene->numParticleEmitters; ++i) {
        ParticleEmitter_Free(&scene->particleEmitters[i]);
        ParticleSystem_Free(scene->particleEmitters[i].system);
    }

    for (Int i = 0; i < scene->numVideoPlayers; ++i) {
        VideoPlayer_Free(&scene->videoPlayers[i]);
    }

    for (Int i = 0; i < scene->numDecals; ++i) {
        if (scene->decals[i].lightmapAtlas) {
            glDeleteTextures(1, &scene->decals[i].lightmapAtlas);
        }
        if (scene->decals[i].directionalLightmapAtlas) {
            glDeleteTextures(1, &scene->decals[i].directionalLightmapAtlas);
        }
    }

    for (Int i = 0; i < scene->numParallaxRooms; ++i) {
        if (scene->parallaxRooms[i].cubemapTexture) {
            glDeleteTextures(1, &scene->parallaxRooms[i].cubemapTexture);
        }
    }

    for (Int i = 0; i < scene->numLogicEntities; ++i) {
        if (scene->logicEntities[i].monitor_fbo != 0) {
            glDeleteFramebuffers(1, &scene->logicEntities[i].monitor_fbo);
            glDeleteTextures(1, &scene->logicEntities[i].monitor_texture);
            glDeleteRenderbuffers(1, &scene->logicEntities[i].monitor_depth);
            scene->logicEntities[i].monitor_fbo = 0;
        }
    }

    scene->numLogicEntities = 0;

    if (engine->camera.physicsBody) {
        engine->camera.physicsBody = nullptr;
    }

    if (engine->physicsWorld) {
        Physics::DestroyWorld(engine->physicsWorld);
        engine->physicsWorld = nullptr;
    }

    scene->numSprites = 0;
    memset(scene, 0, sizeof(Scene));
    scene->originalMapPath[0] = '\0';
    scene->static_shadows_generated = false;
    scene->playerStart.pos = Vec3{ 0, 5, 0 };
    if (engine) {
        engine->camera.health = 100.0f;
        engine->camera.radiation_level = 0.0f;
        engine->flashlight_on = false;
    }
    scene->post.enabled = true;
    scene->post.crtCurvature = 0.1f;
    scene->post.vignetteStrength = 0.8f;
    scene->post.vignetteRadius = 0.75f;
    scene->post.lensFlareEnabled = true;
    scene->post.lensFlareStrength = 1.0f;
    scene->post.scanlineStrength = 0.0f;
    scene->post.grainIntensity = 0.07f;
    scene->post.chromaticAberrationEnabled = true;
    scene->post.chromaticAberrationStrength = 0.005f;
    scene->post.sharpenEnabled = false;
    scene->post.sharpenAmount = 0.15f;
    scene->post.fade_active = false;
    scene->post.fade_alpha = 0.0f;
    scene->post.fade_color = Vec3{ 0, 0, 0 };
    scene->post.bwEnabled = false;
    scene->post.bwStrength = 1.0f;
    scene->colorCorrection.enabled = false;
    memset(scene->colorCorrection.lutPath, 0, sizeof(scene->colorCorrection.lutPath));
    scene->colorCorrection.lutTexture = 0;
    if (scene->ambient_probes) {
        delete[] scene->ambient_probes;
        scene->ambient_probes = nullptr;
    }
    scene->num_ambient_probes = 0;
    scene->sun.enabled = true;
    scene->sun.direction = Vec3{ -0.5f, -1.0f, -0.5f };
    Math::vec3_normalize(&scene->sun.direction);
    scene->sun.color = Vec3{ 1.0f, 0.95f, 0.85f };
    scene->sun.intensity = 1.0f;
    scene->lightmapResolution = 128;
}

Bool Scene_LoadMap(Scene* scene, Renderer* renderer, const Char* mapPath, Engine* engine) {
    FILE* file = fopen(mapPath, "r");
    if (!file) {
        Console::Printf_Error("Could not find map file: %s", mapPath);
        return false;
    }

    Char version_line[256];
    Int map_file_version = 0;

    if (fgets(version_line, sizeof(version_line), file) &&
        sscanf(version_line, "MAP_VERSION %d", &map_file_version) == 1) {

        if (map_file_version < Common::MIN_MAP_VERSION || map_file_version > Common::MAP_VERSION) {
            Console::Printf_Error("Map version unsupported! Map is v%d, supported range is v%d–v%d.",
                map_file_version, Common::MIN_MAP_VERSION, Common::MAP_VERSION);
            fclose(file);
            return false;
        }
    }
    else {
        Console::Printf_Error("Invalid or missing map version.");
        fclose(file);
        return false;
    }

    Scene_Clear(scene, engine);

    strncpy(scene->mapPath, mapPath, sizeof(scene->mapPath) - 1);
    scene->mapPath[sizeof(scene->mapPath) - 1] = '\0';

#ifdef BRANCH_NOCTURNE
    // hack to avoid static at geometry on level4
    if (strstr(mapPath, "level4.map") != nullptr) {
        Cvar_Set("r_skybox", "0");
    }
    else {
        Cvar_Set("r_skybox", "1");
    }
#endif

    engine->physicsWorld = Physics::CreateWorld(Cvar_GetFloat("gravity") * -1.0f);

    Char line[2048];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        Char keyword[64];
        sscanf(line, "%s", keyword);
        if (strcmp(keyword, "player_start") == 0) {
            if (sscanf(line, "%*s %f %f %f %f %f", &scene->playerStart.pos.x, &scene->playerStart.pos.y, &scene->playerStart.pos.z, &scene->playerStart.yaw, &scene->playerStart.pitch) != 5) {
                sscanf(line, "%*s %f %f %f", &scene->playerStart.pos.x, &scene->playerStart.pos.y, &scene->playerStart.pos.z);
                scene->playerStart.yaw = 0.0f;
                scene->playerStart.pitch = 0.0f;
            }
        }
        else if (strcmp(keyword, "player_status") == 0) {
            Int flashlight_on_int = 0;
            if (sscanf(line, "%*s %f %f %d", &engine->camera.health, &engine->camera.radiation_level, &flashlight_on_int) == 3) {
                engine->flashlight_on = (Bool)flashlight_on_int;
            }
        }
        else if (strcmp(keyword, "original_map_path") == 0) {
            sscanf(line, "%*s \"%255[^\"]\"", scene->originalMapPath);
        }
        else if (strcmp(keyword, "lightmap_resolution") == 0) {
            sscanf(line, "%*s %d", &scene->lightmapResolution);
        }
        else if (strcmp(keyword, "post_settings") == 0) {
            Int enabled_int, flare_int, ca_enabled_int, sharpen_enabled_int, bw_enabled_int, invert_enabled_int;

            if (map_file_version <= 21) {
                Int dummy_dof_enabled_int;
                Float dummy_dof_focus, dummy_dof_aperture;
                sscanf(line, "%*s %d %f %f %f %d %f %f %f %d %f %f %d %f %d %f %d %f %d %f",
                    &enabled_int, &scene->post.crtCurvature, &scene->post.vignetteStrength,
                    &scene->post.vignetteRadius, &flare_int, &scene->post.lensFlareStrength, &scene->post.scanlineStrength, &scene->post.grainIntensity,
                    &dummy_dof_enabled_int, &dummy_dof_focus, &dummy_dof_aperture,
                    &ca_enabled_int, &scene->post.chromaticAberrationStrength, &sharpen_enabled_int, &scene->post.sharpenAmount,
                    &bw_enabled_int, &scene->post.bwStrength, &invert_enabled_int, &scene->post.invertStrength);
            }
            else {
                sscanf(line, "%*s %d %f %f %f %d %f %f %f %d %f %d %f %d %f %d %f",
                    &enabled_int, &scene->post.crtCurvature, &scene->post.vignetteStrength,
                    &scene->post.vignetteRadius, &flare_int, &scene->post.lensFlareStrength, &scene->post.scanlineStrength, &scene->post.grainIntensity,
                    &ca_enabled_int, &scene->post.chromaticAberrationStrength, &sharpen_enabled_int, &scene->post.sharpenAmount,
                    &bw_enabled_int, &scene->post.bwStrength, &invert_enabled_int, &scene->post.invertStrength);
            }
            scene->post.enabled = (Bool)enabled_int;
            scene->post.lensFlareEnabled = (Bool)flare_int;
            scene->post.chromaticAberrationEnabled = (Bool)ca_enabled_int;
            scene->post.sharpenEnabled = (Bool)sharpen_enabled_int;
            scene->post.bwEnabled = (Bool)bw_enabled_int;
            scene->post.invertEnabled = (Bool)invert_enabled_int;
        }
        else if (strcmp(keyword, "skybox") == 0) {
            Int use_cubemap_int = 0;
            sscanf(line, "%*s %d \"%127[^\"]\"", &use_cubemap_int, scene->skybox_path);
            scene->use_cubemap_skybox = (Bool)use_cubemap_int;
        }
        else if (strcmp(keyword, "sun") == 0) {
            Int enabled_int;
            Int args_count = sscanf(line, "%*s %d %f %f %f %f %f %f %f %f %f %f %f %f",
                &enabled_int,
                &scene->sun.direction.x, &scene->sun.direction.y, &scene->sun.direction.z,
                &scene->sun.color.x, &scene->sun.color.y, &scene->sun.color.z,
                &scene->sun.intensity,
                &scene->sun.windDirection.x, &scene->sun.windDirection.y, &scene->sun.windDirection.z, &scene->sun.windStrength,
                &scene->sun.volumetricIntensity);

            scene->sun.enabled = (Bool)enabled_int;
            Math::vec3_normalize(&scene->sun.direction);

            if (args_count < 13) {
                scene->sun.volumetricIntensity = 0.0f;
            }
        }
        else if (strcmp(keyword, "color_correction") == 0) {
            Int enabled_int = 0;
            sscanf(line, "%*s %d \"%127[^\"]\"", &enabled_int, scene->colorCorrection.lutPath);
            scene->colorCorrection.enabled = (Bool)enabled_int;
            if (scene->colorCorrection.enabled && strlen(scene->colorCorrection.lutPath) > 0) {
                scene->colorCorrection.lutTexture = loadTexture(scene->colorCorrection.lutPath, false, TEXTURE_LOAD_CONTEXT_WORLD);
            }
        }
        else if (strcmp(keyword, "brush_begin") == 0) {
            if (scene->numBrushes >= Common::MAX_BRUSHES) continue;
            Brush* b = &scene->brushes[scene->numBrushes];
            memset(b, 0, sizeof(Brush));
            b->mass = 0.0f;
            b->isPhysicsEnabled = true;
            b->runtime_active = true;
            b->runtime_playerIsTouching = false;
            b->runtime_hasFired = false;
            b->casts_shadows = true;
            Char water_def_name[64] = "";
            sscanf(line, "%*s %f %f %f %f %f %f %f %f %f", &b->pos.x, &b->pos.y, &b->pos.z, &b->rot.x, &b->rot.y, &b->rot.z, &b->scale.x, &b->scale.y, &b->scale.z);
            while (fgets(line, sizeof(line), file) && strncmp(line, "brush_end", 9) != 0) {
                Int dummy_int;
                Int active_int, has_fired_int, touching_int, door_state_int, plat_state_int;
                if (sscanf(line, " runtime_states %d %d %d %f %d %d", &active_int, &has_fired_int, &touching_int, &b->wait_timer, &door_state_int, &plat_state_int) == 6) {
                    b->runtime_active = (Bool)active_int;
                    b->runtime_hasFired = (Bool)has_fired_int;
                    b->runtime_playerIsTouching = (Bool)touching_int;
                    b->door_state = (DoorState)door_state_int;
                    b->plat_state = (PlatState)plat_state_int;
                    continue;
                }
                Char face_keyword[64];
                sscanf(line, "%s", face_keyword);
                if (sscanf(line, " num_verts %d", &b->numVertices) == 1) {
                    b->vertices = new BrushVertex[b->numVertices];
                    for (Int i = 0; i < b->numVertices; ++i) {
                        fgets(line, sizeof(line), file);
                        if (sscanf(line, " v %*d %f %f %f %f %f %f %f", &b->vertices[i].pos.x, &b->vertices[i].pos.y, &b->vertices[i].pos.z, &b->vertices[i].color.x, &b->vertices[i].color.y, &b->vertices[i].color.z, &b->vertices[i].color.w) != 7) {
                            b->vertices[i].color = Vec4{ 0,0,0,1 };
                        }
                    }
                }
                else if (sscanf(line, " num_faces %d", &b->numFaces) == 1) {
                    b->faces = new BrushFace[b->numFaces]{};
                    for (Int i = 0; i < b->numFaces; ++i) {
                        fgets(line, sizeof(line), file);
                        Char mat_name[64], mat2_name[64], mat3_name[64], mat4_name[64];

                        Char* lightmap_scale_ptr = strstr(line, "lightmap_scale");
                        if (lightmap_scale_ptr) {
                            sscanf(lightmap_scale_ptr, "lightmap_scale %f", &b->faces[i].lightmap_scale);
                            *lightmap_scale_ptr = '\0';
                        }

                        Char* grouped_ptr = strstr(line, "is_grouped");
                        if (grouped_ptr) {
                            Int grouped_int;
                            if (sscanf(grouped_ptr, "is_grouped %d \"%63[^\"]\"", &grouped_int, b->faces[i].groupName) == 2) {
                                b->faces[i].isGrouped = (Bool)grouped_int;
                            }
                            *grouped_ptr = '\0';
                        }
                        else {
                            b->faces[i].isGrouped = false;
                            b->faces[i].groupName[0] = '\0';
                        }

                        sscanf(line, " f %*d %63s %63s %63s %63s %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d",
                            mat_name, mat2_name, mat3_name, mat4_name, &b->faces[i].uv_offset.x, &b->faces[i].uv_offset.y, &b->faces[i].uv_rotation, &b->faces[i].uv_scale.x, &b->faces[i].uv_scale.y,
                            &b->faces[i].uv_offset2.x, &b->faces[i].uv_offset2.y, &b->faces[i].uv_rotation2, &b->faces[i].uv_scale2.x, &b->faces[i].uv_scale2.y,
                            &b->faces[i].uv_offset3.x, &b->faces[i].uv_offset3.y, &b->faces[i].uv_rotation3, &b->faces[i].uv_scale3.x, &b->faces[i].uv_scale3.y,
                            &b->faces[i].uv_offset4.x, &b->faces[i].uv_offset4.y, &b->faces[i].uv_rotation4, &b->faces[i].uv_scale4.x, &b->faces[i].uv_scale4.y,
                            &b->faces[i].numVertexIndices);

                        b->faces[i].material = TextureManager_FindMaterial(mat_name);
                        b->faces[i].material2 = strcmp(mat2_name, "null") == 0 ? nullptr : TextureManager_FindMaterial(mat2_name);
                        b->faces[i].material3 = strcmp(mat3_name, "null") == 0 ? nullptr : TextureManager_FindMaterial(mat3_name);
                        b->faces[i].material4 = strcmp(mat4_name, "null") == 0 ? nullptr : TextureManager_FindMaterial(mat4_name);

                        b->faces[i].vertexIndices = new Int[b->faces[i].numVertexIndices];

                        Char* p = strchr(line, ':');
                        if (p) {
                            p++;
                            Int total_offset = 0;
                            for (Int j = 0; j < b->faces[i].numVertexIndices; ++j) {
                                Int chars_read = 0;
                                sscanf(p + total_offset, " %d %n", &b->faces[i].vertexIndices[j], &chars_read);
                                total_offset += chars_read;
                            }
                        }
                    }
                }
                else if (sscanf(line, " name \"%63[^\"]\"", b->name) == 1) {}
                else if (sscanf(line, " targetname \"%63[^\"]\"", b->targetname) == 1) {}
                else if (sscanf(line, " mass %f", &b->mass) == 1) {}
                else if (sscanf(line, " isPhysicsEnabled %d", &dummy_int) == 1) { b->isPhysicsEnabled = (Bool)dummy_int; }
                else if (sscanf(line, " casts_shadows %d", &dummy_int) == 1) { b->casts_shadows = (Bool)dummy_int; }
                else if (sscanf(line, " classname \"%63[^\"]\"", b->classname) == 1) {}
                else if (strstr(line, "properties")) {
                    b->numProperties = 0;
                    while (fgets(line, sizeof(line), file) && !strstr(line, "}")) {
                        if (b->numProperties < Common::MAX_ENTITY_PROPERTIES) {
                            if (sscanf(line, " \"%63[^\"]\" \"%1023[^\"]\"", b->properties[b->numProperties].key, b->properties[b->numProperties].value) == 2) {
                                b->numProperties++;
                            }
                        }
                    }
                }
                else if (sscanf(line, " is_grouped %d \"%63[^\"]\"", &dummy_int, b->groupName) == 2) {
                    b->isGrouped = (Bool)dummy_int;
                }
                else {
                    b->groupName[0] = '\0';
                }
            }
            Char map_name_sanitized[128];
            const Char* m_last_slash = strrchr(scene->originalMapPath, '/');
            const Char* m_last_bslash = strrchr(scene->originalMapPath, '\\');
            const Char* m_filename = (m_last_slash > m_last_bslash) ? m_last_slash + 1 : (m_last_bslash ? m_last_bslash + 1 : scene->originalMapPath);
            const Char* m_dot = strrchr(m_filename, '.');
            if (m_dot) {
                Usize len = m_dot - m_filename;
                strncpy(map_name_sanitized, m_filename, len);
                map_name_sanitized[len] = '\0';
            }
            else {
                strcpy(map_name_sanitized, m_filename);
            }
            if (strcmp(b->classname, "env_reflectionprobe") == 0) {
                const Char* faces_suffixes[] = { "px", "nx", "py", "ny", "pz", "nz" };
                Char face_paths[6][256];
                for (Int i = 0; i < 6; ++i)  sprintf(face_paths[i], "cubemaps/%s/%s_%s.png", map_name_sanitized, b->name, faces_suffixes[i]);
                const Char* face_pointers[6];
                for (Int i = 0; i < 6; ++i) face_pointers[i] = face_paths[i];
                b->cubemapTexture = loadCubemap(face_pointers);
            }
            Brush_UpdateMatrix(b);
            Brush_GenerateLightmapAtlas(b, map_name_sanitized, scene->numBrushes, scene->lightmapResolution);
            Brush_CreateRenderData(b);
            if (Brush_IsSolid(b) && b->numVertices > 0) {
                if (strcmp(b->classname, "func_plat") == 0) {
                    b->start_pos = b->pos;
                    Float height = atof(Brush_GetProperty(b, "height", "0"));
                    b->end_pos = Vec3{ b->pos.x, b->pos.y + height, b->pos.z };
                    b->move_dir = Math::vec3_sub(b->end_pos, b->start_pos);
                    Math::vec3_normalize(&b->move_dir);
                    b->physicsBody = Physics::CreateKinematicBrush(engine->physicsWorld, (const Float*)b->vertices, b->numVertices, b->modelMatrix);
                }
                else if (b->mass > 0.0f) {
                    b->physicsBody = Physics::CreateDynamicBrush(engine->physicsWorld, (const Float*)&b->vertices->pos, b->numVertices, sizeof(BrushVertex), b->mass, b->modelMatrix);
                    if (!b->isPhysicsEnabled) Physics::ToggleCollision(engine->physicsWorld, b->physicsBody, false);
                }
                else {
                    Vec3* world_verts = new Vec3[b->numVertices];
                    for (Int i = 0; i < b->numVertices; i++) world_verts[i] = Math::mat4_mul_vec3(&b->modelMatrix, b->vertices[i].pos);
                    b->physicsBody = Physics::CreateStaticConvexHull(engine->physicsWorld, (const Float*)world_verts, b->numVertices);
                    delete[] world_verts;
                }
            }
            b->current_angular_velocity = 0.0f;
            b->target_angular_velocity = 0.0f;
            b->plat_state = PLAT_STATE_BOTTOM;
            b->wait_timer = 0.0f;
            if (strcmp(b->classname, "func_rotating") == 0) {
                if (atoi(Brush_GetProperty(b, "StartON", "1")) != 0) {
                    b->target_angular_velocity = atof(Brush_GetProperty(b, "speed", "10"));
                }
            }
            scene->numBrushes++;
        }
       else if (strcmp(keyword, "gltf_model") == 0) {
            if (scene->numObjects >= Common::MAX_MODELS) continue;
            SceneObject* oldObjects = scene->objects;
            Int oldNum = scene->numObjects;
            scene->numObjects++;
            scene->objects = new SceneObject[scene->numObjects];
            if (oldObjects) {
               for (Int k = 0; k < oldNum; ++k) scene->objects[k] = oldObjects[k];
               delete[] oldObjects;
            }
            SceneObject* newObj = &scene->objects[scene->numObjects - 1];
            memset(newObj, 0, sizeof(SceneObject));
            Char* p = line + strlen(keyword);
            while (*p && isspace(*p)) p++;
            Char* path_end = p;
            while (*path_end && !isspace(*path_end)) path_end++;
            Usize path_len = path_end - p;
            if (path_len < sizeof(newObj->modelPath)) { strncpy(newObj->modelPath, p, path_len); newObj->modelPath[path_len] = '\0'; }
            p = path_end;
            while (*p && isspace(*p)) p++;
            if (*p == '"') { p++; Char* quote_end = strchr(p, '"'); if (quote_end) { Usize name_len = quote_end - p; if (name_len < sizeof(newObj->targetname)) { strncpy(newObj->targetname, p, name_len); newObj->targetname[name_len] = '\0'; } p = quote_end + 1; } }
            Int phys_enabled_int; Int sway_enabled_int = 0;
            Int casts_shadows_int = 1;
            Int use_lightmap_int = 0;
            Float lmap_scale = 1.0f;
            sscanf(p, "%f %f %f %f %f %f %f %f %f %f %d %d %f %f %d %d %f",
                &newObj->pos.x, &newObj->pos.y, &newObj->pos.z,
                &newObj->rot.x, &newObj->rot.y, &newObj->rot.z,
                &newObj->scale.x, &newObj->scale.y, &newObj->scale.z,
                &newObj->mass, &phys_enabled_int, &sway_enabled_int,
                &newObj->fadeStartDist, &newObj->fadeEndDist, &casts_shadows_int,
                &use_lightmap_int, &lmap_scale);
            newObj->casts_shadows = (Bool)casts_shadows_int;
            newObj->useLightmap = (Bool)use_lightmap_int;
            newObj->lightmapScale = (lmap_scale > 0.0f) ? lmap_scale : 1.0f;
            newObj->swayEnabled = (Bool)sway_enabled_int; newObj->isPhysicsEnabled = (Bool)phys_enabled_int;
            newObj->animation_playing = false;
            newObj->animation_looping = true;
            newObj->current_animation = -1;
            newObj->animation_time = 0.0f;
            newObj->bone_matrices = nullptr;
            Math::mat4_identity(&newObj->animated_local_transform);
            Long current_pos = ftell(file); Char next_line[256];
            if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                Int grouped_int;
                sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, newObj->groupName);
                newObj->isGrouped = (Bool)grouped_int;
            } else {
                newObj->groupName[0] = '\0';
                fseek(file, current_pos, SEEK_SET);
            }
            SceneObject_UpdateMatrix(newObj);
            newObj->model = Model_Load(newObj->modelPath);
            if (newObj->model && newObj->model->num_animations > 0) {
                newObj->current_animation = 0;
            }
            if (newObj->useLightmap) {
                SceneObject_LoadLightmaps(newObj, scene->numObjects - 1, scene->originalMapPath);
            }
            else {
                SceneObject_LoadVertexLighting(newObj, scene->numObjects - 1, scene->originalMapPath);
                SceneObject_LoadVertexDirectionalLighting(newObj, scene->numObjects - 1, scene->originalMapPath);
            }
            if (!newObj->model) { scene->numObjects--; continue; }
            if (newObj->mass > 0.0f) { newObj->physicsBody = Physics::CreateDynamicConvexHull(engine->physicsWorld, newObj->model->combinedVertexData, newObj->model->totalVertexCount, newObj->mass, newObj->modelMatrix); if (!newObj->isPhysicsEnabled) Physics::ToggleCollision(engine->physicsWorld, newObj->physicsBody, false); }
            else if (newObj->model && newObj->model->combinedVertexData && newObj->model->totalIndexCount > 0) { Mat4 physics_transform = Math::create_trs_matrix(newObj->pos, newObj->rot, Vec3{ 1.0f, 1.0f, 1.0f }); newObj->physicsBody = Physics::CreateStaticTriangleMesh(engine->physicsWorld, newObj->model->combinedVertexData, newObj->model->totalVertexCount, newObj->model->combinedIndexData, newObj->model->totalIndexCount, physics_transform, newObj->scale); }
        }
        else if (strcmp(keyword, "light") == 0) {
            if (scene->numActiveLights >= Common::MAX_LIGHTS) continue;
            Light* light = &scene->lights[scene->numActiveLights];
            memset(light, 0, sizeof(Light));

            Int type_int = 0, preset_int = 0, is_static_int = 0, is_static_shadow_int = 0, is_on_int = 1;
            Char* p = line + strlen(keyword);

            while (*p && isspace(*p)) p++;
            if (*p == '"') {
                p++;
                Char* end = strchr(p, '"');
                if (end) {
                    Usize len = end - p;
                    if (len < sizeof(light->targetname)) strncpy(light->targetname, p, len);
                    light->targetname[len] = '\0';
                    p = end + 1;
                }
            }

            sscanf(p, "%d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d %d %d %f %f",
                &type_int,
                &light->pos.x, &light->pos.y, &light->pos.z,
                &light->rot.x, &light->rot.y, &light->rot.z,
                &light->color.x, &light->color.y, &light->color.z,
                &light->base_intensity,
                &light->radius,
                &light->cutOff, &light->outerCutOff,
                &light->shadowFarPlane, &light->shadowBias,
                &light->volumetricIntensity,
                &preset_int, &is_static_int, &is_static_shadow_int,
                &light->width, &light->height
            );

            Char* cookie_start = strchr(p, '"');
            if (cookie_start) {
                p = cookie_start + 1;
                Char* end = strchr(p, '"');
                if (end) {
                    Usize len = end - p;
                    if (len < sizeof(light->cookiePath)) strncpy(light->cookiePath, p, len);
                    light->cookiePath[len] = '\0';
                    p = end + 1;
                }
            }

            Char* style_start = (p) ? strchr(p, '"') : nullptr;
            if (style_start) {
                p = style_start + 1;
                Char* end = strchr(p, '"');
                if (end) {
                    Usize len = end - p;
                    if (len < sizeof(light->custom_style_string)) strncpy(light->custom_style_string, p, len);
                    light->custom_style_string[len] = '\0';
                    p = end + 1;
                }
            }

            if (p) sscanf(p, "%d", &is_on_int);

            light->preset = preset_int;
            light->type = (LightType)type_int;
            light->is_on = (Bool)is_on_int;
            light->is_static = is_static_int;
            light->is_static_shadow = (Bool)is_static_shadow_int;
            light->intensity = light->base_intensity;

            Long current_pos = ftell(file);
            Char next_line[256];
            if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                Int grouped_int;
                sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, light->groupName);
                light->isGrouped = (Bool)grouped_int;
            }
            else {
                light->groupName[0] = '\0';
                fseek(file, current_pos, SEEK_SET);
            }

            if (strlen(light->cookiePath) > 0 && strcmp(light->cookiePath, "none") != 0) {
                Material* cookieMat = TextureManager_FindMaterial(light->cookiePath);
                if (cookieMat && cookieMat != &g_MissingMaterial) {
                    light->cookieMap = cookieMat->diffuseMap;
                    light->cookieMapHandle = glGetTextureHandleARB(light->cookieMap);
                    glMakeTextureHandleResidentARB(light->cookieMapHandle);
                }
            }
            else {
                light->cookiePath[0] = '\0';
                light->cookieMap = 0;
                light->cookieMapHandle = 0;
            }

            Light_InitShadowMap(light);
            scene->numActiveLights++;
            }
        else if (strcmp(keyword, "decal") == 0) {
            if (scene->numDecals < Common::MAX_DECALS) {
                Decal* d = &scene->decals[scene->numDecals];
                Char mat_name[64];
                memset(d, 0, sizeof(Decal));
                d->lightmap_scale = 1.0f;
                d->uv_scale = Vec2{ 1.0f, 1.0f };
                d->uv_offset = Vec2{ 0.0f, 0.0f };
                d->uv_rotation = 0.0f;
                Char* p = line + strlen(keyword);
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; Char* end = strchr(p, '"'); if (end) { strncpy(mat_name, p, end - p); mat_name[end - p] = '\0'; p = end + 1; } }
                else { Char* end = p; while (*end && !isspace(*end)) end++; strncpy(mat_name, p, end - p); mat_name[end - p] = '\0'; p = end; }
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; Char* end = strchr(p, '"'); if (end) { strncpy(d->targetname, p, end - p); d->targetname[end - p] = '\0'; p = end + 1; } }
                sscanf(p, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                    &d->pos.x, &d->pos.y, &d->pos.z,
                    &d->rot.x, &d->rot.y, &d->rot.z,
                    &d->size.x, &d->size.y, &d->size.z,
                    &d->lightmap_scale,
                    &d->uv_offset.x, &d->uv_offset.y,
                    &d->uv_scale.x, &d->uv_scale.y,
                    &d->uv_rotation
                );
                if (d->lightmap_scale <= 0.0f) d->lightmap_scale = 1.0f;
                if (d->uv_scale.x == 0.0f) d->uv_scale.x = 1.0f;
                if (d->uv_scale.y == 0.0f) d->uv_scale.y = 1.0f;
                d->material = TextureManager_FindMaterial(mat_name);
                Long current_pos = ftell(file); Char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    Int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, d->groupName); d->isGrouped = (Bool)grouped_int;
                } else {
                    d->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                Decal_UpdateMatrix(d);
                Char map_name_sanitized[128];
                const Char* last_slash = strrchr(scene->mapPath, '/');
                const Char* last_bslash = strrchr(scene->mapPath, '\\');
                const Char* map_filename = (last_slash > last_bslash) ? last_slash + 1 : (last_bslash ? last_bslash + 1 : scene->mapPath);
                const Char* dot = strrchr(map_filename, '.');
                if (dot) {
                    Usize len = dot - map_filename;
                    strncpy(map_name_sanitized, scene->originalMapPath, len);
                    map_name_sanitized[len] = '\0';
                }
                else {
                    strcpy(map_name_sanitized, scene->originalMapPath);
                }
                Decal_LoadLightmaps(d, map_name_sanitized, scene->numDecals);
                scene->numDecals++;
            }
        }
        else if (strcmp(keyword, "sound_entity") == 0) {
            if (scene->numSoundEntities < Common::MAX_SOUNDS) {
                SoundEntity* s = &scene->soundEntities[scene->numSoundEntities];
                memset(s, 0, sizeof(SoundEntity));
                Int is_looping_int = 0, play_on_start_int = 0, is_global_int = 0;
                Char* p = line + strlen(keyword); while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; Char* end = strchr(p, '"'); if (end) { Usize len = end - p; if (len < sizeof(s->targetname)) { strncpy(s->targetname, p, len); s->targetname[len] = '\0'; } p = end + 1; } }
                while (*p && isspace(*p)) p++;
                Char* path_end = p; while (*path_end && !isspace(*path_end)) path_end++; Usize path_len = path_end - p;
                if (path_len < sizeof(s->soundPath)) { strncpy(s->soundPath, p, path_len); s->soundPath[path_len] = '\0'; } p = path_end;
                if (map_file_version >= 18) {
                    sscanf(p, "%f %f %f %f %f %f %d %d %d", &s->pos.x, &s->pos.y, &s->pos.z, &s->volume, &s->pitch, &s->maxDistance, &is_looping_int, &play_on_start_int, &is_global_int);
                }
                else {
                    sscanf(p, "%f %f %f %f %f %f %d %d", &s->pos.x, &s->pos.y, &s->pos.z, &s->volume, &s->pitch, &s->maxDistance, &is_looping_int, &play_on_start_int);
                    is_global_int = 0;
                }
                s->isGlobal = (Bool)is_global_int;
                s->is_looping = (Bool)is_looping_int; s->play_on_start = (Bool)play_on_start_int;
                Long current_pos = ftell(file); Char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    Int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, s->groupName); s->isGrouped = (Bool)grouped_int;
                } else {
                    s->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                s->bufferID = Sound::SoundSystem_LoadSound(s->soundPath);
                if (s->play_on_start) {
                    s->sourceID = Sound::SoundSystem_PlaySound(s->bufferID, s->pos, s->volume, s->pitch, s->maxDistance, s->is_looping);
                    Sound::SoundSystem_SetSourceIsGlobal(s->sourceID, s->isGlobal);
                }
                scene->numSoundEntities++;
            }
        }
        else if (strcmp(keyword, "particle_emitter") == 0) {
            if (scene->numParticleEmitters < Common::MAX_PARTICLE_EMITTERS) {
                ParticleEmitter* emitter = &scene->particleEmitters[scene->numParticleEmitters];
                memset(emitter, 0, sizeof(ParticleEmitter));
                Int on_default_int = 1;
                sscanf(line, "%*s \"%127[^\"]\" \"%63[^\"]\" %d %f %f %f", emitter->parFile, emitter->targetname, &on_default_int, &emitter->pos.x, &emitter->pos.y, &emitter->pos.z);
                emitter->on_by_default = (Bool)on_default_int;
                Long current_pos = ftell(file); Char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    Int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, emitter->groupName); emitter->isGrouped = (Bool)grouped_int;
                } else {
                    emitter->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                ParticleSystem* ps = ParticleSystem_Load(emitter->parFile);
                if (ps) { ParticleEmitter_Init(emitter, ps, emitter->pos); scene->numParticleEmitters++; }
            }
        }
        else if (strcmp(keyword, "sprite") == 0) {
            if (scene->numSprites < Common::MAX_SPRITES) {
                Sprite* s = &scene->sprites[scene->numSprites];
                memset(s, 0, sizeof(Sprite));
                Char mat_name[64];
                sscanf(line, "%*s \"%63[^\"]\" %f %f %f %f \"%63[^\"]\"", s->targetname, &s->pos.x, &s->pos.y, &s->pos.z, &s->scale, mat_name);
                s->material = TextureManager_FindMaterial(mat_name);
                s->visible = true;
                Long current_pos = ftell(file); Char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    Int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, s->groupName); s->isGrouped = (Bool)grouped_int;
                } else {
                    s->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                scene->numSprites++;
            }
        }
        else if (strcmp(keyword, "video_player") == 0) {
            if (scene->numVideoPlayers < Common::MAX_VIDEO_PLAYERS) {
                VideoPlayer* vp = &scene->videoPlayers[scene->numVideoPlayers];
                memset(vp, 0, sizeof(VideoPlayer));
                Int play_on_start_int = 0, loop_int = 0;
                Char* p = line + strlen(keyword);
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; Char* end = strchr(p, '"'); if (end) { Usize len = end - p; if (len < sizeof(vp->videoPath)) strncpy(vp->videoPath, p, len), vp->videoPath[len] = '\0'; p = end + 1; } }
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; Char* end = strchr(p, '"'); if (end) { Usize len = end - p; if (len < sizeof(vp->targetname)) strncpy(vp->targetname, p, len), vp->targetname[len] = '\0'; p = end + 1; } }
                sscanf(p, "%d %d %f %f %f %f %f %f %f %f", &play_on_start_int, &loop_int, &vp->pos.x, &vp->pos.y, &vp->pos.z, &vp->rot.x, &vp->rot.y, &vp->rot.z, &vp->size.x, &vp->size.y);
                vp->playOnStart = (Bool)play_on_start_int; vp->loop = (Bool)loop_int;
                Long current_pos = ftell(file); Char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    Int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, vp->groupName); vp->isGrouped = (Bool)grouped_int;
                } else {
                    vp->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                VideoPlayer_Load(vp); if (vp->playOnStart) VideoPlayer_Play(vp);
                scene->numVideoPlayers++;
            }
        }
        else if (strcmp(keyword, "parallax_room") == 0) {
            if (scene->numParallaxRooms < Common::MAX_PARALLAX_ROOMS) {
                ParallaxRoom* p_room = &scene->parallaxRooms[scene->numParallaxRooms];
                memset(p_room, 0, sizeof(ParallaxRoom));
                Char* p = line + strlen(keyword);
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; Char* end = strchr(p, '"'); if (end) { Usize len = end - p; if (len < sizeof(p_room->cubemapPath)) strncpy(p_room->cubemapPath, p, len), p_room->cubemapPath[len] = '\0'; p = end + 1; } }
                while (*p && isspace(*p)) p++;
                if (*p == '"') { p++; Char* end = strchr(p, '"'); if (end) { Usize len = end - p; if (len < sizeof(p_room->targetname)) strncpy(p_room->targetname, p, len), p_room->targetname[len] = '\0'; p = end + 1; } }
                sscanf(p, "%f %f %f %f %f %f %f %f %f", &p_room->pos.x, &p_room->pos.y, &p_room->pos.z, &p_room->rot.x, &p_room->rot.y, &p_room->rot.z, &p_room->size.x, &p_room->size.y, &p_room->roomDepth);
                Long current_pos = ftell(file); Char next_line[256];
                if (fgets(next_line, sizeof(next_line), file) && strstr(next_line, "is_grouped")) {
                    Int grouped_int; sscanf(next_line, " is_grouped %d \"%63[^\"]\"", &grouped_int, p_room->groupName); p_room->isGrouped = (Bool)grouped_int;
                } else {
                    p_room->groupName[0] = '\0';
                    fseek(file, current_pos, SEEK_SET);
                }
                const Char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
                Char face_paths[6][256]; const Char* face_pointers[6];
                for (Int i = 0; i < 6; ++i) { sprintf(face_paths[i], "%s%s", p_room->cubemapPath, suffixes[i]); face_pointers[i] = face_paths[i]; }
                p_room->cubemapTexture = loadCubemap(face_pointers); ParallaxRoom_UpdateMatrix(p_room);
                scene->numParallaxRooms++;
            }
        }
        else if (strcmp(keyword, "logic_entity_begin") == 0) {
            if (scene->numLogicEntities >= Common::MAX_LOGIC_ENTITIES) continue;
            LogicEntity* ent = &scene->logicEntities[scene->numLogicEntities];
            memset(ent, 0, sizeof(LogicEntity));
            while (fgets(line, sizeof(line), file) && strncmp(line, "logic_entity_end", 16) != 0) {
                Int dummy_int;
                if (sscanf(line, " classname \"%63[^\"]\"", ent->classname) == 1) {}
                else if (sscanf(line, " targetname \"%63[^\"]\"", ent->targetname) == 1) {}
                else if (sscanf(line, " pos %f %f %f", &ent->pos.x, &ent->pos.y, &ent->pos.z) == 3) {}
                else if (sscanf(line, " rot %f %f %f", &ent->rot.x, &ent->rot.y, &ent->rot.z) == 3) {}
                else if (sscanf(line, " is_grouped %d \"%63[^\"]\"", &dummy_int, ent->groupName) == 2) {
                    ent->isGrouped = (Bool)dummy_int;
                } else {
                    ent->groupName[0] = '\0';
                }
                if (sscanf(line, " runtime_active %d", (Int*)&ent->runtime_active) == 1) {}
                else if (sscanf(line, " runtime_float_a %f", &ent->runtime_float_a) == 1) {}
                else if (sscanf(line, " runtime_int_a %d", &ent->runtime_int_a) == 1) {}
                else if (sscanf(line, " runtime_float_b %f", &ent->runtime_float_b) == 1) {}
                else if (strstr(line, "properties")) {
                    while (fgets(line, sizeof(line), file) && !strstr(line, "}")) {
                        if (ent->numProperties < Common::MAX_ENTITY_PROPERTIES) {
                            if (sscanf(line, " \"%63[^\"]\" \"%1023[^\"]\"", ent->properties[ent->numProperties].key, ent->properties[ent->numProperties].value) == 2) {
                                ent->numProperties++;
                            }
                        }
                    }
                }
            }
            if (strcmp(ent->classname, "env_beam") == 0) {
                const Char* starton_val = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_val) != 0);
            }
            if (strcmp(ent->classname, "env_fog") == 0 || strcmp(ent->classname, "env_blackhole") == 0) {
                const Char* starton_val = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_val) != 0);
            }
            if (strcmp(ent->classname, "logic_random") == 0) { if (strcmp(LogicEntity_GetProperty(ent, "is_default_enabled", "0"), "1") == 0) { ent->runtime_active = true; } }
            else if (strcmp(ent->classname, "env_blackhole") == 0) {
                const Char* starton_str = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_str) == 1);
            }
            else if (strcmp(ent->classname, "env_glow") == 0) {
                const Char* starton_val = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_val) != 0);
            }
            if (strcmp(ent->classname, "logic_repeat") == 0) {
                if (atoi(LogicEntity_GetProperty(ent, "StartON", "0")) != 0) {
                    ent->runtime_active = true;
                    ent->runtime_float_a = atof(LogicEntity_GetProperty(ent, "delay", "1.0"));
                    ent->runtime_int_a = atoi(LogicEntity_GetProperty(ent, "repeats", "-1"));
                }
            }
            else if (strcmp(ent->classname, "logic_branch") == 0) {
                ent->runtime_int_a = atoi(LogicEntity_GetProperty(ent, "InitialValue", "0"));
            }
            if (strcmp(ent->classname, "env_overlay") == 0) {
                const Char* starton_val = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_val) != 0);
            }
            if (strcmp(ent->classname, "point_radiation_source") == 0) {
                const Char* starton_val = LogicEntity_GetProperty(ent, "starton", "1");
                ent->runtime_active = (atoi(starton_val) != 0);
            }
            scene->numLogicEntities++;
        }
        else if (strcmp(keyword, "io_connection") == 0) {
            if (g_num_io_connections < MAX_IO_CONNECTIONS) {
                IOConnection* conn = &g_io_connections[g_num_io_connections];
                conn->active = true; conn->parameter[0] = '\0';
                Int type_int, fire_once_int, has_fired_int = 0;
                Int items_scanned = sscanf(line, "%*s %d %d \"%63[^\"]\" \"%63[^\"]\" \"%63[^\"]\" %f %d %d \"%63[^\"]\"", &type_int, &conn->sourceIndex, conn->outputName, conn->targetName, conn->inputName, &conn->delay, &fire_once_int, &has_fired_int, conn->parameter);
                conn->sourceType = (EntityType)type_int; conn->fireOnce = (Bool)fire_once_int; conn->hasFired = (Bool)has_fired_int;
                g_num_io_connections++;
            }
        }
    }
    fclose(file);
    if (scene->originalMapPath[0] == '\0') {
        strncpy(scene->originalMapPath, scene->mapPath, sizeof(scene->originalMapPath) - 1);
        scene->originalMapPath[sizeof(scene->originalMapPath) - 1] = '\0';
    }
    if (scene->use_cubemap_skybox && strlen(scene->skybox_path) > 0) {
        const Char* suffixes[] = { "_px.png", "_nx.png", "_py.png", "_ny.png", "_pz.png", "_nz.png" };
        Char face_paths[6][256]; const Char* face_pointers[6];
        for (Int i = 0; i < 6; ++i) { sprintf(face_paths[i], "skybox/%s%s", scene->skybox_path, suffixes[i]); face_pointers[i] = face_paths[i]; }
        scene->skybox_cubemap = loadCubemap(face_pointers);
    }
    else { scene->skybox_cubemap = 0; }
    engine->camera.physicsBody = Physics::CreatePlayerCapsule(engine->physicsWorld, 0.4f, Cvar_GetFloat("g_player_height"), 80.0f, scene->playerStart.pos);
    engine->camera.position = scene->playerStart.pos;
    engine->camera.yaw = scene->playerStart.yaw;
    engine->camera.pitch = scene->playerStart.pitch;

    Char map_name_sanitized[128];
    Char* dot = strrchr(scene->mapPath, '.');
    if (dot) {
        Usize len = dot - scene->mapPath;
        strncpy(map_name_sanitized, scene->originalMapPath, len);
        map_name_sanitized[len] = '\0';
    }
    else {
        strcpy(map_name_sanitized, scene->originalMapPath);
    }
    Scene_LoadAmbientProbes(scene);

    for (Int i = 0; i < scene->numLogicEntities; ++i) {
        if (strcmp(scene->logicEntities[i].classname, "logic_auto") == 0) {
            IO_FireOutput(ENTITY_LOGIC, i, "OnMapSpawn", 0.0f, nullptr);
        }
    }

    return true;
}

Bool Scene_SaveMap(Scene* scene, Engine* engine, const Char* mapPath) {
    Char backup_path[256];
    sprintf(backup_path, "%s.bak", mapPath);
    rename(mapPath, backup_path);

    FILE* file = fopen(mapPath, "w");
    if (!file) {
        Console::Printf_Error("Failed to open %s for writing.", mapPath);
        return false;
    }
    fprintf(file, "MAP_VERSION %d\n\n", Common::MAP_VERSION);
    fprintf(file, "original_map_path \"%s\"\n\n", scene->originalMapPath);
    fprintf(file, "lightmap_resolution %d\n", scene->lightmapResolution);
    if (engine) {
        fprintf(file, "player_start %.4f %.4f %.4f %.4f %.4f\n\n", engine->camera.position.x, engine->camera.position.y, engine->camera.position.z, engine->camera.yaw, engine->camera.pitch);
        fprintf(file, "player_status %.2f %.2f %d\n\n", engine->camera.health, engine->camera.radiation_level, (Int)engine->flashlight_on);
    }
    else {
        fprintf(file, "player_start %.4f %.4f %.4f %.4f %.4f\n\n", scene->playerStart.pos.x, scene->playerStart.pos.y, scene->playerStart.pos.z, scene->playerStart.yaw, scene->playerStart.pitch);
    }
    fprintf(file, "post_settings %d %.4f %.4f %.4f %d %.4f %.4f %.4f %d %.4f %d %.4f %d %.4f %d %.4f\n\n",
        (Int)scene->post.enabled, scene->post.crtCurvature, scene->post.vignetteStrength, scene->post.vignetteRadius,
        (Int)scene->post.lensFlareEnabled, scene->post.lensFlareStrength, scene->post.scanlineStrength, scene->post.grainIntensity,
        (Int)scene->post.chromaticAberrationEnabled, scene->post.chromaticAberrationStrength,
        (Int)scene->post.sharpenEnabled, scene->post.sharpenAmount,
        (Int)scene->post.bwEnabled, scene->post.bwStrength,
        (Int)scene->post.invertEnabled, scene->post.invertStrength
    );
    fprintf(file, "skybox %d \"%s\"\n\n", (Int)scene->use_cubemap_skybox, scene->skybox_path);
    fprintf(file, "sun %d %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f   %.4f %.4f %.4f %.4f %.4f\n\n",
        (Int)scene->sun.enabled,
        scene->sun.direction.x, scene->sun.direction.y, scene->sun.direction.z,
        scene->sun.color.x, scene->sun.color.y, scene->sun.color.z,
        scene->sun.intensity,
        scene->sun.windDirection.x, scene->sun.windDirection.y, scene->sun.windDirection.z,
        scene->sun.windStrength,
        scene->sun.volumetricIntensity);
    fprintf(file, "color_correction %d \"%s\"\n\n", (Int)scene->colorCorrection.enabled, scene->colorCorrection.lutPath);
    for (Int i = 0; i < scene->numBrushes; ++i) {
        Brush* b = &scene->brushes[i];
        fprintf(file, "brush_begin %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", b->pos.x, b->pos.y, b->pos.z, b->rot.x, b->rot.y, b->rot.z, b->scale.x, b->scale.y, b->scale.z);
        if (engine) {
            fprintf(file, "  runtime_states %d %d %d %f %d %d\n", (Int)b->runtime_active, (Int)b->runtime_hasFired, (Int)b->runtime_playerIsTouching, b->wait_timer, (Int)b->door_state, (Int)b->plat_state);
        }
        if (strlen(b->targetname) > 0) fprintf(file, "  targetname \"%s\"\n", b->targetname);
        if (strlen(b->classname) > 0) fprintf(file, "  classname \"%s\"\n", b->classname);
        if (b->isGrouped && b->groupName[0] != '\0') fprintf(file, "  is_grouped 1 \"%s\"\n", b->groupName);
        fprintf(file, "  mass %.4f\n", b->mass);
        fprintf(file, "  isPhysicsEnabled %d\n", (Int)b->isPhysicsEnabled);
        fprintf(file, "  casts_shadows %d\n", (Int)b->casts_shadows);
        if (strcmp(b->classname, "env_reflectionprobe") == 0) {
            fprintf(file, "  name \"%s\"\n", b->name);
        }
        fprintf(file, "  num_verts %d\n", b->numVertices);
        for (Int v = 0; v < b->numVertices; ++v) {
            fprintf(file, "  v %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", v,
                b->vertices[v].pos.x, b->vertices[v].pos.y, b->vertices[v].pos.z,
                b->vertices[v].color.x, b->vertices[v].color.y, b->vertices[v].color.z, b->vertices[v].color.w);
        }
        fprintf(file, "  num_faces %d\n", b->numFaces);
        for (Int j = 0; j < b->numFaces; ++j) {
            BrushFace* face = &b->faces[j];
            const Char* mat_name = face->material ? face->material->name : "___MISSING___";
            const Char* mat2_name = face->material2 ? face->material2->name : "null";
            const Char* mat3_name = face->material3 ? face->material3->name : "null";
            const Char* mat4_name = face->material4 ? face->material4->name : "null";

            fprintf(file, "  f %d %s %s %s %s %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %d :",
                j, mat_name, mat2_name, mat3_name, mat4_name,
                face->uv_offset.x, face->uv_offset.y, face->uv_rotation, face->uv_scale.x, face->uv_scale.y,
                face->uv_offset2.x, face->uv_offset2.y, face->uv_rotation2, face->uv_scale2.x, face->uv_scale2.y,
                face->uv_offset3.x, face->uv_offset3.y, face->uv_rotation3, face->uv_scale3.x, face->uv_scale3.y,
                face->uv_offset4.x, face->uv_offset4.y, face->uv_rotation4, face->uv_scale4.x, face->uv_scale4.y,
                face->numVertexIndices);
            for (Int k = 0; k < face->numVertexIndices; ++k) fprintf(file, " %d", face->vertexIndices[k]);
            fprintf(file, " lightmap_scale %.4f", face->lightmap_scale);
            if (face->isGrouped && face->groupName[0] != '\0') fprintf(file, " is_grouped 1 \"%s\"", face->groupName);
            fprintf(file, "\n");
        }
        if (b->numProperties > 0) {
            fprintf(file, "  properties\n");
            fprintf(file, "  {\n");
            for (Int j = 0; j < b->numProperties; ++j) {
                fprintf(file, "    \"%s\" \"%s\"\n", b->properties[j].key, b->properties[j].value);
            }
            fprintf(file, "  }\n");
        }
        fprintf(file, "brush_end\n\n");
    }

    for (Int i = 0; i < scene->numObjects; ++i) {
        SceneObject* obj = &scene->objects[i];
        fprintf(file, "gltf_model %s \"%s\" %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f %.4f %.4f %d %d %.4f %.4f %d %d %.4f\n",
            obj->modelPath, obj->targetname, obj->pos.x, obj->pos.y, obj->pos.z,
            obj->rot.x, obj->rot.y, obj->rot.z, obj->scale.x, obj->scale.y, obj->scale.z,
            obj->mass, (Int)obj->isPhysicsEnabled, (Int)obj->swayEnabled, obj->fadeStartDist, obj->fadeEndDist, (Int)obj->casts_shadows,
            (Int)obj->useLightmap, obj->lightmapScale);
        if (obj->isGrouped && obj->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", obj->groupName);
    }
    fprintf(file, "\n");
    for (Int i = 0; i < scene->numActiveLights; ++i) {
        Light* light = &scene->lights[i];
        const Char* cookiePathStr = (strlen(light->cookiePath) > 0) ? light->cookiePath : "none";
        fprintf(file, "light \"%s\" %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %d %d %d %.4f %.4f \"%s\" \"%s\" %d\n",
            light->targetname, (Int)light->type, light->pos.x, light->pos.y, light->pos.z, light->rot.x, light->rot.y, light->rot.z,
            light->color.x, light->color.y, light->color.z, light->base_intensity, light->radius,
            light->cutOff, light->outerCutOff, light->shadowFarPlane, light->shadowBias, light->volumetricIntensity,
            light->preset, (Int)light->is_static, (Int)light->is_static_shadow, light->width, light->height, cookiePathStr, light->custom_style_string, (Int)light->is_on);
        if (light->isGrouped && light->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", light->groupName);
    }
    fprintf(file, "\n");
    for (Int i = 0; i < scene->numDecals; ++i) {
        Decal* d = &scene->decals[i];
        const Char* mat_name = d->material ? d->material->name : "___MISSING___";
        fprintf(file, "decal \"%s\" \"%s\" %.4f %.4f %.4f   %.4f %.4f %.4f   %.4f %.4f %.4f %.4f   %.4f %.4f   %.4f %.4f   %.4f\n",
            mat_name, d->targetname,
            d->pos.x, d->pos.y, d->pos.z,
            d->rot.x, d->rot.y, d->rot.z,
            d->size.x, d->size.y, d->size.z,
            d->lightmap_scale,
            d->uv_offset.x, d->uv_offset.y,
            d->uv_scale.x, d->uv_scale.y,
            d->uv_rotation
        );
        if (d->isGrouped && d->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", d->groupName);
    }
    fprintf(file, "\n");
    for (Int i = 0; i < scene->numParticleEmitters; ++i) {
        ParticleEmitter* emitter = &scene->particleEmitters[i];
        fprintf(file, "particle_emitter \"%s\" \"%s\" %d %.4f %.4f %.4f\n", emitter->parFile, emitter->targetname, (Int)emitter->on_by_default, emitter->pos.x, emitter->pos.y, emitter->pos.z);
        if (emitter->isGrouped && emitter->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", emitter->groupName);
    }
    fprintf(file, "\n");
    for (Int i = 0; i < scene->numSoundEntities; ++i) {
        SoundEntity* s = &scene->soundEntities[i];
        fprintf(file, "sound_entity \"%s\" %s %.4f %.4f %.4f %.4f %.4f %.4f %d %d %d\n", s->targetname, s->soundPath, s->pos.x, s->pos.y, s->pos.z, s->volume, s->pitch, s->maxDistance, (Int)s->is_looping, (Int)s->play_on_start, (Int)s->isGlobal);
        if (s->isGrouped && s->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", s->groupName);
    }
    fprintf(file, "\n");
    for (Int i = 0; i < scene->numVideoPlayers; ++i) {
        VideoPlayer* vp = &scene->videoPlayers[i];
        fprintf(file, "video_player \"%s\" \"%s\" %d %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n",
            vp->videoPath, vp->targetname, (Int)vp->playOnStart, (Int)vp->loop,
            vp->pos.x, vp->pos.y, vp->pos.z,
            vp->rot.x, vp->rot.y, vp->rot.z,
            vp->size.x, vp->size.y);
        if (vp->isGrouped && vp->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", vp->groupName);
    }
    fprintf(file, "\n");
    for (Int i = 0; i < scene->numParallaxRooms; ++i) {
        ParallaxRoom* p = &scene->parallaxRooms[i];
        fprintf(file, "parallax_room \"%s\" \"%s\" %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n",
            p->cubemapPath, p->targetname,
            p->pos.x, p->pos.y, p->pos.z,
            p->rot.x, p->rot.y, p->rot.z,
            p->size.x, p->size.y,
            p->roomDepth);
        if (p->isGrouped && p->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", p->groupName);
    }
    for (Int i = 0; i < scene->numSprites; ++i) {
        Sprite* s = &scene->sprites[i];
        fprintf(file, "sprite \"%s\" %.4f %.4f %.4f %.4f \"%s\"\n",
            s->targetname, s->pos.x, s->pos.y, s->pos.z, s->scale, s->material ? s->material->name : "___MISSING___");
        if (s->isGrouped && s->groupName[0] != '\0') fprintf(file, "is_grouped 1 \"%s\"\n", s->groupName);
    }
    fprintf(file, "\n");
    for (Int i = 0; i < scene->numLogicEntities; ++i) {
        LogicEntity* ent = &scene->logicEntities[i];
        fprintf(file, "logic_entity_begin\n");
        fprintf(file, "  classname \"%s\"\n", ent->classname);
        fprintf(file, "  targetname \"%s\"\n", ent->targetname);
        if (ent->isGrouped && ent->groupName[0] != '\0') fprintf(file, "  is_grouped 1 \"%s\"\n", ent->groupName);
        fprintf(file, "  pos %.4f %.4f %.4f\n", ent->pos.x, ent->pos.y, ent->pos.z);
        fprintf(file, "  rot %.4f %.4f %.4f\n", ent->rot.x, ent->rot.y, ent->rot.z);
        fprintf(file, "  runtime_active %d\n", ent->runtime_active);
        fprintf(file, "  runtime_float_a %.4f\n", ent->runtime_float_a);
        fprintf(file, "  runtime_active %d\n", ent->runtime_active);
        fprintf(file, "  runtime_float_a %.4f\n", ent->runtime_float_a);
        fprintf(file, "  runtime_int_a %d\n", ent->runtime_int_a);
        fprintf(file, "  runtime_float_b %.4f\n", ent->runtime_float_b);
        fprintf(file, "  properties\n");
        fprintf(file, "  {\n");
        for (Int j = 0; j < ent->numProperties; ++j) {
            fprintf(file, "    \"%s\" \"%s\"\n", ent->properties[j].key, ent->properties[j].value);
        }
        fprintf(file, "  }\n");
        fprintf(file, "logic_entity_end\n\n");
    }
    for (Int i = 0; i < g_num_io_connections; ++i) {
        IOConnection* conn = &g_io_connections[i];
        if (conn->active) {
            fprintf(file, "io_connection %d %d \"%s\" \"%s\" \"%s\" %.4f %d %d \"%s\"\n", (Int)conn->sourceType, conn->sourceIndex, conn->outputName, conn->targetName, conn->inputName, conn->delay, (Int)conn->fireOnce, (Int)conn->hasFired, conn->parameter);
        }
    }
    fclose(file);
    if (!engine) {
        CreateMapBackup(mapPath);
    }
    return true;
}