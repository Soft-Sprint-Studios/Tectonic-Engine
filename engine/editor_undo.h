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
#ifndef EDITOR_UNDO_H
#define EDITOR_UNDO_H

//----------------------------------------//
// Brief: Handles the level editor undos
//----------------------------------------//

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        EntityType type;
        int index;
        int face_index;
        int vertex_index;
    } EditorSelection;

    typedef struct {
        EntityType type;
        int index;
        union {
            SceneObject object;
            Brush brush;
            Light light;
            Decal decal;
            SoundEntity soundEntity;
            ParticleEmitter particleEmitter;
            Sprite sprite;
            VideoPlayer videoPlayer;
            ParallaxRoom parallaxRoom;
            LogicEntity logicEntity;
            PlayerStart playerStart;
            Fog fog;
            PostProcessSettings post;
        } data;
        char modelPath[128];
        char parFile[128];
        char soundPath[128];
    } EntityState;


    void Undo_Init();
    void Undo_Shutdown();
    void Undo_PerformUndo(Scene* scene, Engine* engine);
    void Undo_PerformRedo(Scene* scene, Engine* engine);
    void capture_state(EntityState* state, Scene* scene, EntityType type, int index);

    void Undo_BeginMultiEntityModification(Scene* scene, EditorSelection* selections, int num_selections);
    void Undo_EndMultiEntityModification(Scene* scene, EditorSelection* selections, int num_selections, const char* description);
    void Undo_PushCreateMultipleEntities(Scene* scene, EditorSelection* selections, int num_selections, const char* description);
    void Undo_PushDeleteMultipleEntities(Scene* scene, EntityState* deleted_states, int num_states, const char* description);

    void Undo_BeginEntityModification(Scene* scene, EntityType type, int index);
    void Undo_EndEntityModification(Scene* scene, EntityType type, int index, const char* description);
    void Undo_PushCreateEntity(Scene* scene, EntityType type, int index, const char* description);
    void Undo_PushDeleteEntity(Scene* scene, EntityType type, int index, const char* description);

    void _raw_delete_brush(Scene* scene, Engine* engine, int index);
    void _raw_delete_model(Scene* scene, int index, Engine* engine);
    void _raw_delete_light(Scene* scene, int index);
    void _raw_delete_decal(Scene* scene, int index);
    void _raw_delete_sound_entity(Scene* scene, int index);
    void _raw_delete_particle_emitter(Scene* scene, int index);
    void _raw_delete_sprite(Scene* scene, int index);
    void _raw_delete_video_player(Scene* scene, int index);
    void _raw_delete_parallax_room(Scene* scene, int index);
    void _raw_delete_logic_entity(Scene* scene, int index);

#ifdef __cplusplus
}
#endif

#endif // EDITOR_UNDO_H