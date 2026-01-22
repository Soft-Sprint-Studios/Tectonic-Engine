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
#include <float.h>
#include "editor_ui.h"
#include "editor_windows.h"
#include "editor_math.h"
#include "editor_misc.h"
#include "editor_selection.h"
#include "editor_actions.h"
#include "sound_system.h"
#include "gl_video_player.h"
#include "gl_console.h"
#include "game_data.h"
#include "commands.h"
#include "cvar.h"

void Editor_RenderUI(Engine* engine, Scene* scene, Renderer* renderer) {
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
        if (UI_Button("Add Model")) { g_EditorState.show_add_model_popup = true; }
    }
    if (model_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_MODEL, model_to_delete, "Delete Model"); _raw_delete_model(scene, model_to_delete, engine); Editor_RemoveFromSelection(ENTITY_MODEL, model_to_delete); }
    if (UI_CollapsingHeader("Brushes", 1)) {
        for (int i = 0; i < scene->numBrushes; ++i) {
            char label[128];
            const char* entity_tag = "";
            if (strlen(scene->brushes[i].classname) > 0) {
                entity_tag = "[E]";
            }
            if (strlen(scene->brushes[i].targetname) > 0) {
                sprintf(label, "%s %s##%d", scene->brushes[i].targetname, entity_tag, i);
            }
            else {
                sprintf(label, "Brush %d %s##%d", i, entity_tag, i);
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
        if (UI_Button("Add Light")) { if (scene->numActiveLights < MAX_LIGHTS) { Light* new_light = &scene->lights[scene->numActiveLights]; scene->numActiveLights++; memset(new_light, 0, sizeof(Light));  new_light->custom_style_string[0] = '\0'; sprintf(new_light->targetname, "Light_%d", scene->numActiveLights - 1); new_light->type = LIGHT_POINT; new_light->position = g_EditorState.editor_camera.position; new_light->color = (Vec3){ 1,1,1 }; new_light->intensity = 1.0f; new_light->direction = (Vec3){ 0, -1, 0 }; new_light->shadowFarPlane = 25.0f; new_light->shadowBias = 0.05f; new_light->intensity = 1.0f; new_light->radius = 10.0f; new_light->base_intensity = 1.0f; new_light->is_on = true; Light_InitShadowMap(new_light); Undo_PushCreateEntity(scene, ENTITY_LIGHT, scene->numActiveLights - 1, "Create Light"); } }
    }
    if (light_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_LIGHT, light_to_delete, "Delete Light"); _raw_delete_light(scene, light_to_delete); Editor_RemoveFromSelection(ENTITY_LIGHT, light_to_delete); }
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
        if (UI_Button("Add Decal")) {
            if (scene->numDecals < MAX_DECALS) {
                Decal* d = &scene->decals[scene->numDecals]; memset(d, 0, sizeof(Decal)); sprintf(d->targetname, "Decal_%d", scene->numDecals); d->pos = g_EditorState.editor_camera.position; d->size = (Vec3){ 1, 1, 1 }; d->material = TextureManager_FindMaterial(TextureManager_GetMaterial(0)->name);
                d->uv_scale = (Vec2){ 1.0f, 1.0f }; d->uv_offset = (Vec2){ 0.0f, 0.0f }; d->uv_rotation = 0.0f;
                d->lightmap_scale = 1.0f;
                Decal_UpdateMatrix(d); scene->numDecals++; Undo_PushCreateEntity(scene, ENTITY_DECAL, scene->numDecals - 1, "Create Decal");
            }
        }
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
            }UI_SameLine(0, 20.0f); char del_label[32]; sprintf(del_label, "[X]##particle%d", i);  if (UI_Button(del_label)) { particle_to_delete = i; }
        }
        if (UI_Button("Add Emitter")) {
            g_EditorState.show_particle_browser_popup = true;
            ScanParticleFiles();
        }
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

                int num_logic_classes = 0;
                const char** logic_classes = GameData_GetLogicEntityClassnames(&num_logic_classes);
                if (num_logic_classes > 0) {
                    strcpy(ent->classname, logic_classes[0]);
                    const TGD_EntityDef* def = GameData_FindEntityDef(ent->classname);
                    if (def) {
                        ent->numProperties = def->num_properties;
                        for (int i = 0; i < def->num_properties; ++i) {
                            strcpy(ent->properties[i].key, def->properties[i].key);
                            strcpy(ent->properties[i].value, def->properties[i].default_value);
                        }
                    }
                }
                sprintf(ent->targetname, "%s_%d", ent->classname, scene->numLogicEntities);
                ent->pos = g_EditorState.editor_camera.position;
                scene->numLogicEntities++;
                Undo_PushCreateEntity(scene, ENTITY_LOGIC, scene->numLogicEntities - 1, "Create Logic Entity");
            }
        }
    }
    if (logic_entity_to_delete != -1) { Undo_PushDeleteEntity(scene, ENTITY_LOGIC, logic_entity_to_delete, "Delete Logic Entity"); _raw_delete_logic_entity(scene, logic_entity_to_delete); Editor_RemoveFromSelection(ENTITY_LOGIC, logic_entity_to_delete); }
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
        UI_Text("Lighting");
        if (UI_Checkbox("Casts Shadows", &obj->casts_shadows)) {
            Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index);
            Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Toggle Model Shadows");
        }
        if (UI_Checkbox("Use Lightmap", &obj->useLightmap)) {
            Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index);
            Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Toggle Model Lightmap");
        }
        if (obj->useLightmap) {
            if (UI_DragFloat("Lightmap Scale", &obj->lightmapScale, 0.1f, 0.1f, 4.0f)) {}
            if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_MODEL, primary->index);
            if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_MODEL, primary->index, "Edit Lightmap Scale");
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

        if (obj->model && obj->model->num_animations > 0) {
            UI_Separator();
            if (UI_CollapsingHeader("Animation", 1)) {
                const char** anim_names = malloc(obj->model->num_animations * sizeof(const char*));
                if (anim_names) {
                    for (int i = 0; i < obj->model->num_animations; ++i) {
                        anim_names[i] = obj->model->animations[i].name;
                    }
                    if (UI_Combo("Clip", &g_EditorState.preview_animation_index, anim_names, obj->model->num_animations, -1)) {
                        g_EditorState.preview_animation_time = 0.0f;
                        g_EditorState.preview_animation_playing = false;
                    }
                    free(anim_names);
                }

                if (g_EditorState.preview_animation_index != -1) {
                    AnimationClip* clip = &obj->model->animations[g_EditorState.preview_animation_index];
                    if (g_EditorState.preview_animation_playing) {
                        if (UI_Button("Pause")) { g_EditorState.preview_animation_playing = false; }
                    }
                    else {
                        if (UI_Button("Play")) { g_EditorState.preview_animation_playing = true; }
                    }
                    UI_SameLine();
                    if (UI_Button("Stop")) {
                        g_EditorState.preview_animation_playing = false;
                        g_EditorState.preview_animation_time = 0.0f;
                    }
                    UI_DragFloat("Time", &g_EditorState.preview_animation_time, 0.01f, 0.0f, clip->duration);
                }
            }
        }
    }
    else if (primary && primary->type == ENTITY_BRUSH) {
        Brush* b = &scene->brushes[primary->index];
        UI_Separator();
        UI_InputText("Name", b->targetname, sizeof(b->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Edit Brush Name"); }
        UI_Separator(); bool transform_changed = false;
        UI_DragFloat3("Position", &b->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->pos.x = SnapValue(b->pos.x, g_EditorState.grid_size); b->pos.y = SnapValue(b->pos.y, g_EditorState.grid_size); b->pos.z = SnapValue(b->pos.z, g_EditorState.grid_size); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Move Brush"); }
        UI_DragFloat3("Rotation", &b->rot.x, 1.0f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->rot.x = SnapValue(b->rot.x, 15.0f); b->rot.y = SnapValue(b->rot.y, 15.0f); b->rot.z = SnapValue(b->rot.z, 15.0f); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Rotate Brush"); }
        UI_DragFloat3("Scale", &b->scale.x, 0.01f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { if (g_EditorState.snap_to_grid) { b->scale.x = SnapValue(b->scale.x, 0.25f); b->scale.y = SnapValue(b->scale.y, 0.25f); b->scale.z = SnapValue(b->scale.z, 0.25f); } transform_changed = true; Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Scale Brush"); }
        if (UI_Checkbox("Use Vertex Lighting", &b->useVertexLighting)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            if (b->useVertexLighting) {
                if (b->lightmapAtlas) { glDeleteTextures(1, &b->lightmapAtlas); b->lightmapAtlas = 0; }
                if (b->directionalLightmapAtlas) { glDeleteTextures(1, &b->directionalLightmapAtlas); b->directionalLightmapAtlas = 0; }
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Toggle Brush Vertex Lighting");
        }
        if (UI_Checkbox("Casts Shadows", &b->casts_shadows)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Toggle Brush Shadows");
        }
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
            if (Brush_IsSolid(b) && b->numVertices > 0) {
                if (b->mass > 0.0f) {
                    b->physicsBody = Physics_CreateDynamicBrush(engine->physicsWorld, (const float*)&b->vertices->pos, b->numVertices, sizeof(BrushVertex), b->mass, b->modelMatrix);
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
        UI_Text("Brush Entity Class");
        int num_brush_classes = 0;
        const char** brush_classes = GameData_GetBrushEntityClassnames(&num_brush_classes);
        int current_class_idx = 0;
        for (int i = 1; i < num_brush_classes; ++i) {
            if (strcmp(b->classname, brush_classes[i]) == 0) {
                current_class_idx = i;
                break;
            }
        }

        if (UI_Combo("Classname", &current_class_idx, brush_classes, num_brush_classes, -1)) {
            Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index);
            if (current_class_idx == 0) {
                b->classname[0] = '\0';
                b->numProperties = 0;
            }
            else {
                strcpy(b->classname, brush_classes[current_class_idx]);
                if (strcmp(b->classname, "env_reflectionprobe") == 0) {
                    int x = (int)roundf(b->pos.x);
                    int y = (int)roundf(b->pos.y);
                    int z = (int)roundf(b->pos.z);

                    char name_buf[128];
                    snprintf(name_buf, sizeof(name_buf), "Probe_%s%d_%s%d_%s%d",
                        (x < 0 ? "n" : ""), abs(x),
                        (y < 0 ? "n" : ""), abs(y),
                        (z < 0 ? "n" : ""), abs(z));

                    strncpy(b->name, name_buf, sizeof(b->name) - 1);
                    strncpy(b->targetname, b->name, sizeof(b->targetname) - 1);
                }
                const TGD_EntityDef* def = GameData_FindEntityDef(b->classname);
                if (def) {
                    b->numProperties = def->num_properties;
                    for (int k = 0; k < def->num_properties; ++k) {
                        strcpy(b->properties[k].key, def->properties[k].key);
                        strcpy(b->properties[k].value, def->properties[k].default_value);
                    }
                }
            }
            Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Change Brush Class");
        }

        const TGD_EntityDef* brush_def = GameData_FindEntityDef(b->classname);
        if (brush_def) {
            UI_Separator();
            UI_Text("Properties");

            const char** target_names = NULL;
            int num_targets = 0;
            for (int k = 0; k < scene->numObjects; ++k) if (strlen(scene->objects[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->objects[k].targetname; }
            for (int k = 0; k < scene->numBrushes; ++k) if (strlen(scene->brushes[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->brushes[k].targetname; }
            for (int k = 0; k < scene->numActiveLights; ++k) if (strlen(scene->lights[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->lights[k].targetname; }
            for (int k = 0; k < scene->numSoundEntities; ++k) if (strlen(scene->soundEntities[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->soundEntities[k].targetname; }
            for (int k = 0; k < scene->numParticleEmitters; ++k) if (strlen(scene->particleEmitters[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->particleEmitters[k].targetname; }
            for (int k = 0; k < scene->numVideoPlayers; ++k) if (strlen(scene->videoPlayers[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->videoPlayers[k].targetname; }
            for (int k = 0; k < scene->numSprites; ++k) if (strlen(scene->sprites[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->sprites[k].targetname; }
            for (int k = 0; k < scene->numLogicEntities; ++k) if (strlen(scene->logicEntities[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->logicEntities[k].targetname; }

            for (int i = 0; i < brush_def->num_properties; ++i) {
                const TGD_Property* prop = &brush_def->properties[i];
                if (i >= b->numProperties) continue;

                UI_PushID(i);
                switch (prop->type) {
                case TGD_PROP_CHECKBOX: {
                    bool is_checked = (atoi(b->properties[i].value) != 0);
                    if (UI_Checkbox(prop->display_name, &is_checked)) {
                        strcpy(b->properties[i].value, is_checked ? "1" : "0");
                    }
                    break;
                }
                case TGD_PROP_CHOICES: {
                    const char** display_items = (const char**)malloc(prop->num_choices * sizeof(const char*));
                    int current_item = -1;
                    for (int j = 0; j < prop->num_choices; ++j) {
                        display_items[j] = prop->choices[j].display_name;
                        if (strcmp(b->properties[i].value, prop->choices[j].value) == 0) {
                            current_item = j;
                        }
                    }
                    if (UI_Combo(prop->display_name, &current_item, display_items, prop->num_choices, -1)) {
                        if (current_item >= 0) {
                            strcpy(b->properties[i].value, prop->choices[current_item].value);
                        }
                    }
                    free(display_items);
                    break;
                }
                case TGD_PROP_TEXTURE: {
                    char button_label[256];
                    snprintf(button_label, sizeof(button_label), "%s: %s", prop->display_name, b->properties[i].value);
                    if (UI_Button(button_label)) {
                        g_EditorState.texture_browser_target = 100 + i;
                        g_EditorState.show_texture_browser = true;
                    }
                    break;
                }
                case TGD_PROP_ENTITIES: {
                    int current_item = -1;
                    for (int k = 0; k < num_targets; ++k) {
                        if (strcmp(b->properties[i].value, target_names[k]) == 0) {
                            current_item = k;
                            break;
                        }
                    }
                    if (UI_Combo(prop->display_name, &current_item, target_names, num_targets, -1)) {
                        if (current_item >= 0) {
                            strncpy(b->properties[i].value, target_names[current_item], sizeof(b->properties[i].value) - 1);
                        }
                    }
                    break;
                }
                default:
                    UI_InputText(prop->display_name, b->properties[i].value, sizeof(b->properties[i].value));
                    break;
                }
                if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_BRUSH, primary->index); }
                if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_BRUSH, primary->index, "Edit Brush Property"); }
                UI_PopID();
            }
            if (target_names) free(target_names);
            RenderIOEditor(ENTITY_BRUSH, primary->index);
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
                        if (Brush_IsSolid(b) && b->numVertices > 0) {
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
        Light* light = &scene->lights[primary->index];

        UI_InputText("Name", light->targetname, sizeof(light->targetname));
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Name");
        }

        if (UI_RadioButton("Point", light->type == LIGHT_POINT)) {
            if (light->type != LIGHT_POINT) {
                Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                Light_DestroyShadowMap(light);
                light->type = LIGHT_POINT;
                Light_InitShadowMap(light);
                Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Type");
            }
        }
        UI_SameLine();
        if (UI_RadioButton("Spot", light->type == LIGHT_SPOT)) {
            if (light->type != LIGHT_SPOT) {
                Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                Light_DestroyShadowMap(light);
                light->type = LIGHT_SPOT;
                if (light->cutOff <= 0.0f) {
                    light->cutOff = cosf(12.5f * M_PI / 180.0f);
                    light->outerCutOff = cosf(17.5f * M_PI / 180.0f);
                }
                Light_InitShadowMap(light);
                Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Type");
            }
        }
        UI_SameLine();
        if (UI_RadioButton("Area (Baked Only)", light->type == LIGHT_AREA)) {
            if (light->type != LIGHT_AREA) {
                Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
                Light_DestroyShadowMap(light);
                light->type = LIGHT_AREA;
                light->is_static = true;
                Light_InitShadowMap(light);
                Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Type");
            }
        }

        UI_Separator();

        UI_DragFloat3("Position", &light->position.x, 0.1f, 0, 0);
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            if (g_EditorState.snap_to_grid) {
                light->position.x = SnapValue(light->position.x, g_EditorState.grid_size);
                light->position.y = SnapValue(light->position.y, g_EditorState.grid_size);
                light->position.z = SnapValue(light->position.z, g_EditorState.grid_size);
            }
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Move Light");
        }

        if (light->type == LIGHT_SPOT || light->type == LIGHT_AREA) {
            UI_DragFloat3("Rotation", &light->rot.x, 1.0f, -360.0f, 360.0f);
            if (UI_IsItemActivated()) {
                Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
            }
            if (UI_IsItemDeactivatedAfterEdit()) {
                if (g_EditorState.snap_to_grid) {
                    light->rot.x = SnapValue(light->rot.x, 15.0f);
                    light->rot.y = SnapValue(light->rot.y, 15.0f);
                    light->rot.z = SnapValue(light->rot.z, 15.0f);
                }
                Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Rotate Light");
            }
        }

        UI_ColorEdit3("Color", &light->color.x);
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Color");
        }

        UI_DragFloat("Intensity", &light->base_intensity, 0.05f, 0.0f, 1000.0f);
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Intensity");
        }

        if (light->type == LIGHT_AREA) {
            UI_DragFloat("Width", &light->width, 0.1f, 0.1f, 1000.0f);
            if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); }
            if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Width"); }
            UI_DragFloat("Height", &light->height, 0.1f, 0.1f, 1000.0f);
            if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); }
            if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Height"); }
        }

        UI_DragFloat("Radius", &light->radius, 0.1f, 0.1f, 1000.0f);
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Light Radius");
        }

        UI_DragFloat("Volumetric Intensity", &light->volumetricIntensity, 0.05f, 0.0f, 10.0f);
        if (UI_IsItemActivated()) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
        }
        if (UI_IsItemDeactivatedAfterEdit()) {
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Volumetric Intensity");
        }

        UI_Separator();

        const char* light_modes[] = { "Dynamic", "Static (Fully Baked)", "Mixed (Baked Indirect)" };
        if (UI_Combo("Bake Mode", &light->is_static, light_modes, 3, -1)) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Bake Mode");
        }

        const char* preset_names[] = {
            "0: Normal", "1: Flicker 1", "2: Slow Strong Pulse", "3: Candle 1",
            "4: Fast Strobe", "5: Gentle Pulse", "6: Flicker 2", "7: Candle 2",
            "8: Candle 3", "9: Slow Strobe", "10: Fluorescent", "11: Slow Pulse 2",
            "12: Underwater", "13: Custom"
        };
        if (UI_Combo("Preset", &light->preset, preset_names, 14, 14)) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Change Light Preset");
        }

        if (light->preset == 13) {
            UI_InputText("Custom Style", light->custom_style_string, sizeof(light->custom_style_string));
            if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); }
            if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Edit Custom Light Style"); }
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
                    if (light->cookieMapHandle != 0) { glMakeTextureHandleNonResidentARB(light->cookieMapHandle); }
                    light->cookiePath[0] = '\0';
                    light->cookieMap = 0;
                    light->cookieMapHandle = 0;
                    Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Clear Light Cookie");
                }
            }
        }
        if (UI_Checkbox("On by default", &light->is_on)) { Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index); Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Toggle Light On"); }

        if (UI_Checkbox("Static Shadow Map", &light->is_static_shadow)) {
            Undo_BeginEntityModification(scene, ENTITY_LIGHT, primary->index);
            light->has_rendered_static_shadow = false;
            Undo_EndEntityModification(scene, ENTITY_LIGHT, primary->index, "Toggle Static Shadow");
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
            g_EditorState.texture_browser_target = 5;
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

        UI_Separator();
        if (UI_DragFloat("Lightmap Scale", &d->lightmap_scale, 0.125f, 0.125f, 16.0f)) {}
        if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index); }
        if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal Lightmap Scale"); }

        UI_Separator();
        UI_Text("Texture Mapping");

        UI_Text("Scale"); UI_SameLine(); UI_SetNextItemWidth(80);
        if (UI_InputFloat("X##DScale", &d->uv_scale.x, 0.01f, 0.1f, "%.2f")) {}
        if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
        if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal UV Scale");
        UI_SameLine(); UI_SetNextItemWidth(80);
        if (UI_InputFloat("Y##DScale", &d->uv_scale.y, 0.01f, 0.1f, "%.2f")) {}
        if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
        if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal UV Scale");

        UI_Text("Shift"); UI_SameLine(); UI_SetNextItemWidth(80);
        if (UI_InputFloat("X##DShift", &d->uv_offset.x, 0.1f, 1.0f, "%.2f")) {}
        if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
        if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal UV Shift");
        UI_SameLine(); UI_SetNextItemWidth(80);
        if (UI_InputFloat("Y##DShift", &d->uv_offset.y, 0.1f, 1.0f, "%.2f")) {}
        if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
        if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal UV Shift");

        UI_Text("Rotation"); UI_SameLine(); UI_SetNextItemWidth(172);
        if (UI_DragFloat("##DRotation", &d->uv_rotation, 1.0f, -360.0f, 360.0f)) {}
        if (UI_IsItemActivated()) Undo_BeginEntityModification(scene, ENTITY_DECAL, primary->index);
        if (UI_IsItemDeactivatedAfterEdit()) Undo_EndEntityModification(scene, ENTITY_DECAL, primary->index, "Edit Decal UV Rotation");

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
        if (UI_Checkbox("Global Sound", &s->isGlobal)) {
            Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index);
            SoundSystem_SetSourceIsGlobal(s->sourceID, s->isGlobal);
            if (!s->isGlobal) {
                SoundSystem_SetSourcePosition(s->sourceID, s->pos);
            }
            Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Toggle Sound Global");
        }
        if (UI_Checkbox("Play on Start", &s->play_on_start)) {
            Undo_BeginEntityModification(scene, ENTITY_SOUND, primary->index);
            Undo_EndEntityModification(scene, ENTITY_SOUND, primary->index, "Toggle Play on Start");
        }
    }
    else if (primary && primary->type == ENTITY_PARTICLE_EMITTER) {
        ParticleEmitter* emitter = &scene->particleEmitters[primary->index]; UI_Text("Particle Emitter: %s", emitter->parFile); UI_Separator(); UI_DragFloat3("Position", &emitter->pos.x, 0.1f, 0, 0); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index, "Move Emitter"); }
        UI_InputText("Name", emitter->targetname, sizeof(emitter->targetname)); if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index); } if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index, "Edit Emitter Name"); } if (UI_Checkbox("On by default", &emitter->on_by_default)) { Undo_BeginEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index); emitter->is_on = emitter->on_by_default; Undo_EndEntityModification(scene, ENTITY_PARTICLE_EMITTER, primary->index, "Toggle Emitter On"); } if (UI_Button("Reload .par File")) { ParticleSystem_Free(emitter->system); ParticleSystem* ps = ParticleSystem_Load(emitter->parFile); if (ps) { ParticleEmitter_Init(emitter, ps, emitter->pos); } else { Console_Printf_Error("Failed to reload particle system: %s", emitter->parFile); emitter->system = NULL; } }
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
        int num_logic_classes = 0;
        const char** logic_classes = GameData_GetLogicEntityClassnames(&num_logic_classes);
        int current_class_index = -1;
        for (int i = 0; i < num_logic_classes; ++i) {
            if (strcmp(ent->classname, logic_classes[i]) == 0) {
                current_class_index = i;
                break;
            }
        }
        if (UI_Combo("Classname", &current_class_index, logic_classes, num_logic_classes, -1)) {
            if (current_class_index >= 0) {
                Undo_BeginEntityModification(scene, ENTITY_LOGIC, primary->index);
                strcpy(ent->classname, logic_classes[current_class_index]);
                const TGD_EntityDef* def = GameData_FindEntityDef(ent->classname);
                if (def) {
                    ent->numProperties = def->num_properties;
                    for (int k = 0; k < def->num_properties; ++k) {
                        strcpy(ent->properties[k].key, def->properties[k].key);
                        strcpy(ent->properties[k].value, def->properties[k].default_value);
                    }
                }
                else {
                    ent->numProperties = 0;
                }
                Undo_EndEntityModification(scene, ENTITY_LOGIC, primary->index, "Change Logic Class");
            }
        }
        UI_InputText("Targetname", ent->targetname, sizeof(ent->targetname));
        if (UI_DragFloat3("Position", &ent->pos.x, 0.1f, 0, 0)) {}
        if (UI_DragFloat3("Rotation", &ent->rot.x, 1.0f, 0, 0)) {}

        UI_Separator();
        const TGD_EntityDef* def = GameData_FindEntityDef(ent->classname);
        if (def) {
            UI_Text("Properties");

            const char** target_names = NULL;
            int num_targets = 0;
            for (int k = 0; k < scene->numObjects; ++k) if (strlen(scene->objects[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->objects[k].targetname; }
            for (int k = 0; k < scene->numBrushes; ++k) if (strlen(scene->brushes[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->brushes[k].targetname; }
            for (int k = 0; k < scene->numActiveLights; ++k) if (strlen(scene->lights[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->lights[k].targetname; }
            for (int k = 0; k < scene->numSoundEntities; ++k) if (strlen(scene->soundEntities[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->soundEntities[k].targetname; }
            for (int k = 0; k < scene->numParticleEmitters; ++k) if (strlen(scene->particleEmitters[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->particleEmitters[k].targetname; }
            for (int k = 0; k < scene->numVideoPlayers; ++k) if (strlen(scene->videoPlayers[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->videoPlayers[k].targetname; }
            for (int k = 0; k < scene->numSprites; ++k) if (strlen(scene->sprites[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->sprites[k].targetname; }
            for (int k = 0; k < scene->numLogicEntities; ++k) if (strlen(scene->logicEntities[k].targetname) > 0) { target_names = realloc(target_names, ++num_targets * sizeof(char*)); target_names[num_targets - 1] = scene->logicEntities[k].targetname; }

            for (int i = 0; i < ent->numProperties; ++i) {
                const TGD_Property* prop = &def->properties[i];
                UI_PushID(i);
                switch (prop->type) {
                case TGD_PROP_CHECKBOX: {
                    bool is_checked = (atoi(ent->properties[i].value) != 0);
                    if (UI_Checkbox(prop->display_name, &is_checked)) {
                        strcpy(ent->properties[i].value, is_checked ? "1" : "0");
                    }
                    break;
                }
                case TGD_PROP_COLOR: {
                    Vec3 color;
                    sscanf(ent->properties[i].value, "%f %f %f", &color.x, &color.y, &color.z);
                    if (UI_ColorEdit3(prop->display_name, &color.x)) {
                        sprintf(ent->properties[i].value, "%.3f %.3f %.3f", color.x, color.y, color.z);
                    }
                    break;
                }
                case TGD_PROP_CHOICES: {
                    const char** display_items = (const char**)malloc(prop->num_choices * sizeof(const char*));
                    int current_item = -1;
                    for (int j = 0; j < prop->num_choices; ++j) {
                        display_items[j] = prop->choices[j].display_name;
                        if (strcmp(ent->properties[i].value, prop->choices[j].value) == 0) {
                            current_item = j;
                        }
                    }
                    if (UI_Combo(prop->display_name, &current_item, display_items, prop->num_choices, -1)) {
                        if (current_item >= 0) {
                            strcpy(ent->properties[i].value, prop->choices[current_item].value);
                        }
                    }
                    free(display_items);
                    break;
                }
                case TGD_PROP_TEXTURE: {
                    char button_label[256];
                    snprintf(button_label, sizeof(button_label), "%s: %s", prop->display_name, ent->properties[i].value);
                    if (UI_Button(button_label)) {
                        g_EditorState.texture_browser_target = 200 + i;
                        g_EditorState.show_texture_browser = true;
                    }
                    break;
                }
                case TGD_PROP_ENTITIES: {
                    int current_item = -1;
                    for (int k = 0; k < num_targets; ++k) {
                        if (strcmp(ent->properties[i].value, target_names[k]) == 0) {
                            current_item = k;
                            break;
                        }
                    }
                    if (UI_Combo(prop->display_name, &current_item, target_names, num_targets, -1)) {
                        if (current_item >= 0) {
                            strncpy(ent->properties[i].value, target_names[current_item], sizeof(ent->properties[i].value) - 1);
                        }
                    }
                    break;
                }
                default:
                    UI_InputText(prop->display_name, ent->properties[i].value, sizeof(ent->properties[i].value));
                    break;
                }
                if (UI_IsItemActivated()) { Undo_BeginEntityModification(scene, ENTITY_LOGIC, primary->index); }
                if (UI_IsItemDeactivatedAfterEdit()) { Undo_EndEntityModification(scene, ENTITY_LOGIC, primary->index, "Edit Logic Property"); }
                UI_PopID();
            }
            if (target_names) free(target_names);
            RenderIOEditor(ENTITY_LOGIC, primary->index);
        }
    }
    UI_Separator(); UI_Text("Scene Settings"); UI_Separator();
    if (UI_CollapsingHeader("Sun", 1)) {
        UI_Checkbox("Enabled##Sun", &scene->sun.enabled);
        UI_ColorEdit3("Color##Sun", &scene->sun.color.x);
        UI_DragFloat("Intensity##Sun", &scene->sun.intensity, 0.05f, 0.0f, 100.0f);
        UI_DragFloat("Volumetric Intensity##Sun", &scene->sun.volumetricIntensity, 0.05f, 0.0f, 10.0f);
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
        if (UI_Checkbox("Invert", &scene->post.invertEnabled)) {}
        if (scene->post.invertEnabled) {
            UI_DragFloat("Invert Strength", &scene->post.invertStrength, 0.01f, 0.0f, 1.0f);
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
            scene->colorCorrection.lutTexture = loadTexture(scene->colorCorrection.lutPath, false, TEXTURE_LOAD_CONTEXT_WORLD);
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
    if (UI_RadioButton("Tube", g_EditorState.current_brush_shape == BRUSH_SHAPE_TUBE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_TUBE; }
    UI_SameLine();
    if (UI_RadioButton("Wedge", g_EditorState.current_brush_shape == BRUSH_SHAPE_WEDGE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_WEDGE; }
    UI_SameLine();
    if (UI_RadioButton("Spike", g_EditorState.current_brush_shape == BRUSH_SHAPE_SPIKE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_SPIKE; }
    if (UI_RadioButton("Sphere", g_EditorState.current_brush_shape == BRUSH_SHAPE_SPHERE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_SPHERE; }
    UI_SameLine();
    if (UI_RadioButton("Semi-Sphere", g_EditorState.current_brush_shape == BRUSH_SHAPE_SEMI_SPHERE)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_SEMI_SPHERE; }
    UI_SameLine();
    if (UI_RadioButton("Arch", g_EditorState.current_brush_shape == BRUSH_SHAPE_ARCH)) { g_EditorState.current_brush_shape = BRUSH_SHAPE_ARCH; }
    if (g_EditorState.current_brush_shape == BRUSH_SHAPE_CYLINDER || g_EditorState.current_brush_shape == BRUSH_SHAPE_TUBE || g_EditorState.current_brush_shape == BRUSH_SHAPE_SPIKE || g_EditorState.current_brush_shape == BRUSH_SHAPE_SPHERE || g_EditorState.current_brush_shape == BRUSH_SHAPE_SEMI_SPHERE) {
        UI_DragInt("Sides", &g_EditorState.cylinder_creation_steps, 1, 4, 64);
    }
    if (g_EditorState.current_brush_shape == BRUSH_SHAPE_TUBE) {
        UI_DragFloat("Wall Thickness", &g_EditorState.tube_wall_thickness, 0.05f, 0.1f, 16.0f);
    }
    UI_Separator(); UI_Text("Editor Settings"); UI_Separator(); if (UI_Button(g_EditorState.snap_to_grid ? "Snapping: ON" : "Snapping: OFF")) { g_EditorState.snap_to_grid = !g_EditorState.snap_to_grid; } UI_SameLine(); UI_DragFloat("Grid Size", &g_EditorState.grid_size, 0.015625f, 0.015625f, 64.0f);
    bool is_unlit = Cvar_GetInt("r_fullbright");
    if (UI_Checkbox("Unlit Mode", &is_unlit)) {
        Cvar_Set("r_fullbright", is_unlit ? "1" : "0");
    }
    for (int i = 0; i < 5; i++) {
        UI_Spacing();
    }
    UI_End();

    if (UI_BeginMainMenuBar()) {
        if (UI_BeginMenu("File", true)) {
            if (UI_MenuItem("New Map", NULL, false, true)) {
                if (g_is_map_dirty) {
                    g_pending_action = PENDING_ACTION_NEW_MAP;
                }
                else {
                    Scene_Clear(scene, engine);
                    strcpy(g_EditorState.currentMapPath, "untitled.map");
                    Undo_Init();
                }
            }
            if (UI_MenuItem("Load Map...", NULL, false, true)) {
                if (g_is_map_dirty) {
                    g_pending_action = PENDING_ACTION_LOAD_MAP;
                }
                else {
                    g_EditorState.show_load_map_popup = true;
                    ScanMapFiles();
                }
            }
            if (UI_MenuItem("Save", "Ctrl+S", false, true)) {
                if (strcmp(g_EditorState.currentMapPath, "untitled.map") == 0) {
                    g_EditorState.show_save_map_popup = true;
                }
                else {
                    Scene_SaveMap(scene, NULL, g_EditorState.currentMapPath);
                    Editor_SetMapDirty(false);
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
            if (UI_MenuItem("Exit Editor", "F5", false, true)) {
                if (g_is_map_dirty) {
                    g_pending_action = PENDING_ACTION_EXIT_EDITOR;
                }
                else {
                    char* args[] = { "edit" };
                    Commands_Execute(1, args);
                }
            }
            UI_EndMenu();
        }
        if (UI_BeginMenu("Edit", true)) { if (UI_MenuItem("Undo", "Ctrl+Z", false, true)) { Undo_PerformUndo(scene, engine); } if (UI_MenuItem("Redo", "Ctrl+Y", false, true)) { Undo_PerformRedo(scene, engine); } UI_EndMenu(); }
        if (UI_BeginMenu("Tools", true)) {
            if (UI_MenuItem("Group", "Ctrl+G", false, g_EditorState.num_selections > 1)) { Editor_GroupSelection(); }
            if (UI_MenuItem("Ungroup", "Ctrl+U", false, g_EditorState.num_selections > 0)) { Editor_UngroupSelection(); }
            if (UI_MenuItem("Transform", "Ctrl+M", false, g_EditorState.num_selections > 0)) {
                g_EditorState.show_transform_window = true;
                if (g_EditorState.transform_window_mode == TRANSFORM_MODE_SCALE) {
                    g_EditorState.transform_window_values = (Vec3){ 1, 1, 1 };
                }
                else {
                    g_EditorState.transform_window_values = (Vec3){ 0, 0, 0 };
                }
            }
            bool can_merge = false;
            if (g_EditorState.num_selections > 1) {
                can_merge = true;
                for (int i = 0; i < g_EditorState.num_selections; ++i) {
                    if (g_EditorState.selections[i].type != ENTITY_BRUSH) {
                        can_merge = false;
                        break;
                    }
                }
            }
            if (UI_MenuItem("Merge", NULL, false, can_merge)) {
                Editor_MergeSelection(scene, engine);
            }
            UI_Separator();
            if (UI_MenuItem("Flip Horizontal", "Ctrl+L", false, g_EditorState.num_selections > 0)) {
                Editor_FlipSelection(scene, engine, 1);
            }
            if (UI_MenuItem("Flip Vertical", "Ctrl+I", false, g_EditorState.num_selections > 0)) {
                Editor_FlipSelection(scene, engine, 0);
            }
            if (UI_MenuItem("Go to Coordinates...", NULL, false, true)) {
                g_EditorState.show_goto_coord_window = true;
                g_EditorState.goto_coord_input[0] = '\0';
            }
            UI_Separator();
            if (UI_MenuItem("Map Information", NULL, false, true)) {
                g_EditorState.show_map_info_window = true;
            }
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
            if (UI_MenuItem("Build Environment probes...", NULL, false, true)) {
                g_EditorState.show_build_cubemaps_popup = true;
                g_EditorState.cubemap_resolution_index = 2;
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
            Editor_SetMapDirty(false);
            if (g_pending_action != PENDING_ACTION_NONE) {
                Editor_ExecutePendingAction(engine, scene, renderer);
            }
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
                Editor_SetMapDirty(false);
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
    Editor_RenderParticleBrowser(scene);
    Editor_RenderBakeLightingWindow(scene, engine);
    Editor_RenderBuildCubemapsWindow(renderer, scene, engine);
    Editor_RenderArchPropertiesWindow(scene, engine);
    Editor_RenderMapInfoWindow(scene);
    Editor_RenderTransformWindow(scene, engine);
    Editor_RenderGoToCoordinatesWindow();

    if (g_pending_action != PENDING_ACTION_NONE) {
        UI_OpenPopup("Unsaved Changes");
    }

    if (UI_BeginPopupModal("Unsaved Changes", NULL, 1 << 3)) {
        UI_Text("You have unsaved changes. Do you want to save them?");
        UI_Spacing();

        if (UI_Button("Save")) {
            if (strcmp(g_EditorState.currentMapPath, "untitled.map") == 0) {
                g_EditorState.show_save_map_popup = true;
            }
            else {
                Scene_SaveMap(scene, NULL, g_EditorState.currentMapPath);
                Editor_SetMapDirty(false);
                Editor_ExecutePendingAction(engine, scene, renderer);
            }
            UI_CloseCurrentPopup();
        }
        UI_SameLine();
        if (UI_Button("Don't Save")) {
            Editor_ExecutePendingAction(engine, scene, renderer);
            UI_CloseCurrentPopup();
        }
        UI_SameLine();
        if (UI_Button("Cancel")) {
            g_pending_action = PENDING_ACTION_NONE;
            UI_CloseCurrentPopup();
        }
        UI_EndPopup();
    }

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

        bool show_dims = false;
        Vec3 b_min, b_max;

        bool is_actively_creating = g_EditorState.is_dragging_for_creation || g_EditorState.is_in_brush_creation_mode;
        bool is_single_brush_selected = (g_EditorState.num_selections == 1 && Editor_GetPrimarySelection()->type == ENTITY_BRUSH);

        if (is_actively_creating) {
            show_dims = true;
            b_min = g_EditorState.preview_brush_world_min;
            b_max = g_EditorState.preview_brush_world_max;
        }
        else if (is_single_brush_selected) {
            show_dims = true;
            EditorSelection* sel = Editor_GetPrimarySelection();
            Brush* b = &scene->brushes[sel->index];
            if (b->numVertices > 0) {
                b_min = (Vec3){ FLT_MAX, FLT_MAX, FLT_MAX };
                b_max = (Vec3){ -FLT_MAX, -FLT_MAX, -FLT_MAX };
                for (int v_idx = 0; v_idx < b->numVertices; ++v_idx) {
                    Vec3 world_v = mat4_mul_vec3(&b->modelMatrix, b->vertices[v_idx].pos);
                    b_min.x = fminf(b_min.x, world_v.x);
                    b_min.y = fminf(b_min.y, world_v.y);
                    b_min.z = fminf(b_min.z, world_v.z);
                    b_max.x = fmaxf(b_max.x, world_v.x);
                    b_max.y = fmaxf(b_max.y, world_v.y);
                    b_max.z = fmaxf(b_max.z, world_v.z);
                }
            }
            else {
                b_min = b->pos;
                b_max = b->pos;
            }
        }

        if (show_dims && type >= VIEW_TOP_XZ) {
            void* draw_list = UI_GetWindowDrawList();
            unsigned int text_color = UI_GetColorU32(255, 255, 255, 255);
            Vec3 size = vec3_sub(b_max, b_min);

            Vec3 top_mid_world, left_mid_world;
            char horizontal_text[32], vertical_text[32];

            if (type == VIEW_TOP_XZ) {
                top_mid_world = (Vec3){ (b_min.x + b_max.x) / 2.0f, b_min.y, b_max.z };
                left_mid_world = (Vec3){ b_min.x, b_min.y, (b_min.z + b_max.z) / 2.0f };
                sprintf(horizontal_text, "%.0f", fabsf(size.x));
                sprintf(vertical_text, "%.0f", fabsf(size.z));
            }
            else if (type == VIEW_FRONT_XY) {
                top_mid_world = (Vec3){ (b_min.x + b_max.x) / 2.0f, b_max.y, b_min.z };
                left_mid_world = (Vec3){ b_min.x, (b_min.y + b_max.y) / 2.0f, b_min.z };
                sprintf(horizontal_text, "%.0f", fabsf(size.x));
                sprintf(vertical_text, "%.0f", fabsf(size.y));
            }
            else {
                top_mid_world = (Vec3){ b_min.x, b_max.y, (b_min.z + b_max.z) / 2.0f };
                left_mid_world = (Vec3){ b_min.x, (b_min.y + b_max.y) / 2.0f, b_min.z };
                sprintf(horizontal_text, "%.0f", fabsf(size.z));
                sprintf(vertical_text, "%.0f", fabsf(size.y));
            }

            Vec2 top_mid_screen = WorldToScreen(top_mid_world, type);
            Vec2 left_mid_screen = WorldToScreen(left_mid_world, type);

            float h_text_offset = strlen(horizontal_text) * 4.0f;
            UI_DrawList_AddText(draw_list, win_x + top_mid_screen.x - h_text_offset, win_y + top_mid_screen.y - 20.0f, text_color, horizontal_text);

            float v_text_offset = strlen(vertical_text) * 8.0f;
            UI_DrawList_AddText(draw_list, win_x + left_mid_screen.x - v_text_offset - 10.0f, win_y + left_mid_screen.y - 8.0f, text_color, vertical_text);
        }
        UI_End();
        UI_PopStyleVar(1);
    }
    Editor_RenderFaceEditSheet(scene, engine);
    Editor_RenderStatusBar();
}