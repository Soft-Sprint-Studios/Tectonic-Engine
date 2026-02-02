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
#include "editor_actions.h"
#include "editor_selection.h"
#include "editor_undo.h"
#include "sound_system.h"
#include "gl_video_player.h"
#include "gl_render_misc.h"

void Editor_GroupSelection() {
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
                snprintf(g_CurrentScene->brushes[sel->index].faces[sel->face_index].groupName, sizeof(g_CurrentScene->brushes[sel->index].faces[sel->face_index].groupName), "%s", group_name);
            }
            else {
                g_CurrentScene->brushes[sel->index].isGrouped = true;
                snprintf(g_CurrentScene->brushes[sel->index].groupName, sizeof(g_CurrentScene->brushes[sel->index].groupName), "%s", group_name);
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

void Editor_UngroupSelection() {
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

void Editor_FlipSelection(Scene* scene, Engine* engine, int axis) {
    if (g_EditorState.num_selections == 0) {
        return;
    }

    Undo_BeginMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections);

    Vec3 centroid = g_EditorState.gizmo_selection_centroid;

    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        EditorSelection* sel = &g_EditorState.selections[i];

        Vec3* pos = nullptr;
        Vec3* rot = nullptr;

        switch (sel->type) {
        case ENTITY_MODEL: pos = &scene->objects[sel->index].pos; rot = &scene->objects[sel->index].rot; break;
        case ENTITY_BRUSH: pos = &scene->brushes[sel->index].pos; rot = &scene->brushes[sel->index].rot; break;
        case ENTITY_LIGHT: pos = &scene->lights[sel->index].pos; rot = &scene->lights[sel->index].rot; break;
        case ENTITY_DECAL: pos = &scene->decals[sel->index].pos; rot = &scene->decals[sel->index].rot; break;
        case ENTITY_SOUND: pos = &scene->soundEntities[sel->index].pos; break;
        case ENTITY_PARTICLE_EMITTER: pos = &scene->particleEmitters[sel->index].pos; break;
        case ENTITY_SPRITE: pos = &scene->sprites[sel->index].pos; break;
        case ENTITY_VIDEO_PLAYER: pos = &scene->videoPlayers[sel->index].pos; rot = &scene->videoPlayers[sel->index].rot; break;
        case ENTITY_PARALLAX_ROOM: pos = &scene->parallaxRooms[sel->index].pos; rot = &scene->parallaxRooms[sel->index].rot; break;
        case ENTITY_LOGIC: pos = &scene->logicEntities[sel->index].pos; rot = &scene->logicEntities[sel->index].rot; break;
        case ENTITY_PLAYERSTART: pos = &scene->playerStart.pos; break;
        default: continue;
        }

        if (pos) {
            Vec3 relative_pos = vec3_sub(*pos, centroid);
            if (axis == 1) {
                relative_pos.x *= -1.0f;
                relative_pos.z *= -1.0f;
            }
            else {
                relative_pos.y *= -1.0f;
                relative_pos.z *= -1.0f;
            }
            *pos = vec3_add(centroid, relative_pos);
        }

        if (rot) {
            if (axis == 1) {
                rot->y = fmodf(rot->y + 180.0f, 360.0f);
                rot->x *= -1.0f;
                rot->z *= -1.0f;
            }
            else {
                rot->x = fmodf(rot->x + 180.0f, 360.0f);
                rot->y *= -1.0f;
                rot->z *= -1.0f;
            }
        }

        switch (sel->type) {
        case ENTITY_MODEL: { SceneObject* obj = &scene->objects[sel->index]; SceneObject_UpdateMatrix(obj); if (obj->physicsBody) Physics_SetWorldTransform(obj->physicsBody, obj->modelMatrix); break; }
        case ENTITY_BRUSH: { Brush* b = &scene->brushes[sel->index]; Brush_UpdateMatrix(b); if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix); break; }
        case ENTITY_DECAL: Decal_UpdateMatrix(&scene->decals[sel->index]); break;
        case ENTITY_PARALLAX_ROOM: ParallaxRoom_UpdateMatrix(&scene->parallaxRooms[sel->index]); break;
        case ENTITY_SOUND: SoundSystem_SetSourcePosition(scene->soundEntities[sel->index].sourceID, scene->soundEntities[sel->index].pos); break;
        default: break;
        }
    }

    Undo_EndMultiEntityModification(scene, g_EditorState.selections, g_EditorState.num_selections, "Flip Selection");
}

void Editor_MergeSelection(Scene* scene, Engine* engine) {
    if (g_EditorState.num_selections < 2) return;

    EditorSelection* brush_selections = new EditorSelection[g_EditorState.num_selections];
    int brush_count = 0;
    for (int i = 0; i < g_EditorState.num_selections; ++i) {
        if (g_EditorState.selections[i].type == ENTITY_BRUSH) {
            bool already_added = false;
            for (int j = 0; j < brush_count; ++j) {
                if (brush_selections[j].index == g_EditorState.selections[i].index) {
                    already_added = true;
                    break;
                }
            }
            if (!already_added) {
                brush_selections[brush_count++] = g_EditorState.selections[i];
            }
        }
    }

    if (brush_count < 2) {
        Console_Printf_Warning("Merge requires at least two unique brushes to be selected.");
        delete[] brush_selections;
        return;
    }

    EntityState* before_states = new EntityState[brush_count]();
    for (int i = 0; i < brush_count; i++) {
        capture_state(&before_states[i], scene, ENTITY_BRUSH, brush_selections[i].index);
    }

    int base_brush_index = brush_selections[0].index;
    Brush* base_brush = &scene->brushes[base_brush_index];

    Mat4 base_inv_matrix;
    mat4_inverse(&base_brush->modelMatrix, &base_inv_matrix);

    for (int i = 1; i < brush_count; i++) {
        int source_brush_index = brush_selections[i].index;
        Brush* source_brush = &scene->brushes[source_brush_index];

        int vertex_offset = base_brush->numVertices;

        Mat4 source_to_base_transform;
        mat4_multiply(&source_to_base_transform, &base_inv_matrix, &source_brush->modelMatrix);

        BrushVertex* new_vertices = new BrushVertex[base_brush->numVertices + source_brush->numVertices];
        if (base_brush->vertices) {
            memcpy(new_vertices, base_brush->vertices, base_brush->numVertices * sizeof(BrushVertex));
            delete[] base_brush->vertices;
        }
        base_brush->vertices = new_vertices;
        for (int v = 0; v < source_brush->numVertices; v++) {
            Vec3 transformed_pos = mat4_mul_vec3(&source_to_base_transform, source_brush->vertices[v].pos);
            base_brush->vertices[vertex_offset + v] = source_brush->vertices[v];
            base_brush->vertices[vertex_offset + v].pos = transformed_pos;
        }
        base_brush->numVertices += source_brush->numVertices;

        BrushFace* new_faces = new BrushFace[base_brush->numFaces + source_brush->numFaces];
        if (base_brush->faces) {
            memcpy(new_faces, base_brush->faces, base_brush->numFaces * sizeof(BrushFace));
            delete[] base_brush->faces;
        }
        base_brush->faces = new_faces;
        for (int j = 0; j < source_brush->numFaces; j++) {
            BrushFace* new_face = &base_brush->faces[base_brush->numFaces + j];
            BrushFace* source_face = &source_brush->faces[j];

            *new_face = *source_face;
            new_face->vertexIndices = new int[source_face->numVertexIndices];
            memcpy(new_face->vertexIndices, source_face->vertexIndices, source_face->numVertexIndices * sizeof(int));

            for (int k = 0; k < new_face->numVertexIndices; k++) {
                new_face->vertexIndices[k] += vertex_offset;
            }
        }
        base_brush->numFaces += source_brush->numFaces;
    }

    for (int i = brush_count - 1; i >= 1; --i) {
        _raw_delete_brush(scene, engine, brush_selections[i].index);
    }

    Brush_CreateRenderData(base_brush);
    if (base_brush->physicsBody) {
        Physics_RemoveRigidBody(engine->physicsWorld, base_brush->physicsBody);
        base_brush->physicsBody = nullptr;
    }
    if (Brush_IsSolid(base_brush) && base_brush->numVertices > 0) {
        Vec3* world_verts = new Vec3[base_brush->numVertices];
        for (int i = 0; i < base_brush->numVertices; i++) {
            world_verts[i] = mat4_mul_vec3(&base_brush->modelMatrix, base_brush->vertices[i].pos);
        }
        base_brush->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, (const float*)world_verts, base_brush->numVertices);
        delete[] world_verts;
    }

    EntityState* after_state = new EntityState();
    capture_state(after_state, scene, ENTITY_BRUSH, base_brush_index);

    Undo_PushMergeAction(scene, before_states, brush_count, after_state, 1, "Merge Brushes");

    delete[] brush_selections;

    Editor_ClearSelection();
    Editor_AddToSelection(ENTITY_BRUSH, base_brush_index, 0, 0);
    Console_Printf("Merged %d brushes.", brush_count);
}

void Editor_DuplicateModel(Scene* scene, Engine* engine, int index) {
    if (index < 0 || index >= scene->numObjects) return;
    if (scene->numObjects >= MAX_MODELS) return;

    SceneObject* src_obj = &scene->objects[index];

    SceneObject* new_objects = new SceneObject[scene->numObjects + 1];
    for (int i = 0; i < scene->numObjects; ++i)
        new_objects[i] = scene->objects[i];
    delete[] scene->objects;
    scene->objects = new_objects;

    scene->numObjects++;
    SceneObject* new_obj = &scene->objects[scene->numObjects - 1];
    memcpy(new_obj, src_obj, sizeof(SceneObject));

    sprintf(new_obj->targetname, "Model_%d", scene->numObjects - 1);
    new_obj->bone_matrices = nullptr;
    mat4_identity(&new_obj->animated_local_transform);
    new_obj->physicsBody = nullptr;
    new_obj->pos.x += 1.0f;

    SceneObject_UpdateMatrix(new_obj);
    new_obj->model = Model_Load(new_obj->modelPath);

    if (new_obj->model && new_obj->model->combinedVertexData && new_obj->model->totalIndexCount > 0) {
        Mat4 physics_transform = create_trs_matrix(new_obj->pos, new_obj->rot, Vec3{ 1, 1, 1 });
        new_obj->physicsBody = Physics_CreateStaticTriangleMesh(engine->physicsWorld, new_obj->model->combinedVertexData, new_obj->model->totalVertexCount, new_obj->model->combinedIndexData, new_obj->model->totalIndexCount, physics_transform, new_obj->scale);
    }

    Editor_AddToSelection(ENTITY_MODEL, scene->numObjects - 1, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_MODEL, scene->numObjects - 1, "Duplicate Model");
}

void Editor_DuplicateBrush(Scene* scene, Engine* engine, int index) {
    if (index < 0 || index >= scene->numBrushes || scene->numBrushes >= MAX_BRUSHES) return;

    Brush* src_brush = &scene->brushes[index];

    int new_brush_index = scene->numBrushes;
    Brush* new_brush = &scene->brushes[new_brush_index];
    Brush_DeepCopy(new_brush, src_brush);

    sprintf(new_brush->targetname, "Brush_%d", new_brush_index);
    new_brush->pos.x += 1.0f;

    Brush_UpdateMatrix(new_brush);
    Brush_CreateRenderData(new_brush);

    if (Brush_IsSolid(new_brush) && new_brush->numVertices > 0) {
        if (new_brush->mass > 0.0f) {
            new_brush->physicsBody = Physics_CreateDynamicBrush(engine->physicsWorld, (const float*)&new_brush->vertices->pos, new_brush->numVertices, sizeof(BrushVertex), new_brush->mass, new_brush->modelMatrix);
            if (!new_brush->isPhysicsEnabled) {
                Physics_ToggleCollision(engine->physicsWorld, new_brush->physicsBody, false);
            }
        }
        else {
            Vec3* world_verts = new Vec3[new_brush->numVertices];
            for (int i = 0; i < new_brush->numVertices; i++)
                world_verts[i] = mat4_mul_vec3(&new_brush->modelMatrix, new_brush->vertices[i].pos);

            new_brush->physicsBody = Physics_CreateStaticConvexHull(engine->physicsWorld, reinterpret_cast<const float*>(world_verts), new_brush->numVertices);
            delete[] world_verts;
        }
    }

    scene->numBrushes++;
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
    new_light->pos.x += 1.0f;
    Light_InitShadowMap(new_light);
    int new_light_index = scene->numActiveLights;
    scene->numActiveLights++;
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
    new_vp->plm = nullptr;
    new_vp->audioSource = 0;
    new_vp->pos.x += 1.0f;
    VideoPlayer_Load(new_vp);
    if (new_vp->playOnStart) {
        VideoPlayer_Play(new_vp);
    }
    int new_vp_index = scene->numVideoPlayers;
    scene->numVideoPlayers++;
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
    Editor_AddToSelection(ENTITY_SPRITE, new_sprite_index, -1, -1);
    Undo_PushCreateEntity(scene, ENTITY_SPRITE, new_sprite_index, "Duplicate Sprite");
}