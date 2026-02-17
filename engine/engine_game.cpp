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
#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <time.h>
#include "cvar.h"
#include "commands.h"
#include "gl_renderer.h"
#include "map.h"
#include "map_misc.h"
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
#include "gl_video_player.h"
#include "gl_blackholes.h"
#include "gl_geometry.h"
#include "gl_bloom.h"
#include "gl_keypad.h"
#include "gl_note.h"
#include "gl_misc.h"
#include "gl_render_misc.h"
#include "gl_overlay.h"
#include "gl_skybox.h"
#include "gl_planar.h"
#include "gl_postprocess.h"
#include "gl_ssao.h"
#include "gl_volumetrics.h"
#include "gl_monitor.h"
#include "gl_loading_screen.h"
#include "weapons.h"
#include "sentry_wrapper.h"
#include "water_manager.h"
#include "lightmapper.h"
#include "ipc_system.h"
#include "game_data.h"
#include "gl_shadows.h"
#include "engine_commands.h"
#include "engine_api.h"
#include "animations.h"
#ifdef PLATFORM_LINUX
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dlfcn.h>
#endif

const Char* g_light_styles[] = {
    "m",
    "mmnmmommommnonmmonqnmmo",
    "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba",
    "mmmmmaaaaammmmmaaaaaabcdefgabcdefg",
    "mamamamamama",
    "jklmnopqrstuvwxyzyxwvutsrqponmlkj",
    "nmonqnmomnmomomno",
    "mmmaaaabcdefgmmmmaaaammmaamm",
    "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa",
    "aaaaaaaazzzzzzzz",
    "mmamammmmammamamaaamammma",
    "abcdefghijklmnopqrrqponmlkjihgfedcba",
    "mmnnmmnnnmmnn"
};
const Int NUM_LIGHT_STYLES = sizeof(g_light_styles) / sizeof(g_light_styles[0]);

void process_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            Cvar_EngineSet("engine_running", "0");
            return;
        }
        UI_ProcessEvent(&event);
        SDL_SetCursor(g_engine->cursor);
        if (event.type == SDL_MOUSEWHEEL && g_current_mode == MODE_GAME && !Console_IsVisible()) {
            if (event.wheel.y > 0) {
                Weapons_SwitchPrev();
            }
            else if (event.wheel.y < 0) {
                Weapons_SwitchNext();
            }
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && g_current_mode == MODE_GAME && !Console_IsVisible()) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                if (g_engine->heldObject) {
                    Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
                    vec3_normalize(&forward);

                    Physics_SetCcdEnabled(g_engine->heldObject, true, 1e-7f);
                    Physics_SetGravityEnabled(g_engine->heldObject, true);
                    Physics_ApplyCentralImpulse(g_engine->heldObject, vec3_muls(forward, 10.0f));
                    g_engine->heldObject = nullptr;
                    return;
                }
                Weapons_TryFire(g_engine, &g_scene);
            }
        }
        if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
            MainMenuAction action = MainMenu_HandleEvent(&event);
            if (action == MAINMENU_ACTION_START_GAME) {
                const GameConfig* config = GameConfig_Get();
                if (strlen(config->startmap) > 0 && strcmp(config->startmap, "none") != 0) {
                    Char map_name_no_ext[128];
                    strncpy(map_name_no_ext, config->startmap, sizeof(map_name_no_ext) - 1);
                    map_name_no_ext[sizeof(map_name_no_ext) - 1] = '\0';

                    Char* dot = strrchr(map_name_no_ext, '.');
                    if (dot) {
                        *dot = '\0';
                    }

                    Char* argv[] = { (Char*)"map", map_name_no_ext };
                    Commands_Execute(2, argv);
                }
                else {
                    Console_Printf_Error("No startmap defined in gameconf.txt! Cannot start game.");
                }
            }
            else if (action == MAINMENU_ACTION_CONTINUE_GAME) {
                g_current_mode = MODE_GAME;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            else if (action == MAINMENU_ACTION_QUIT) {
                g_quit_requested = true;
            }
        }
        else if (g_current_mode == MODE_EDITOR) {
            Editor_ProcessEvent(&event, &g_scene, g_engine);
        }

        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (event.key.keysym.sym == SDLK_e && g_current_mode == MODE_GAME && !Console_IsVisible()) {
                if (g_engine->note_active) {
                    g_engine->note_active = false;
                    g_player_input_disabled = false;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    if (g_engine->active_note_entity_index != -1) {
                        IO_FireOutput(ENTITY_LOGIC, g_engine->active_note_entity_index, "OnRead", g_engine->lastFrame, nullptr);
                    }
                    return;
                }
                if (g_engine->heldObject) {
                    Physics_SetGravityEnabled(g_engine->heldObject, true);
                    g_engine->heldObject = nullptr;
                    return;
                }
                Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
                vec3_normalize(&forward);

                Vec3 ray_end = vec3_add(g_engine->camera.position, vec3_muls(forward, 3.0f));

                for (Int i = 0; i < g_scene.numBrushes; ++i) {
                    Brush* brush = &g_scene.brushes[i];
                    if (strcmp(brush->classname, "func_button") == 0) {
                        Vec3 brush_local_min, brush_local_max;
                        Brush_GetLocalAABB(brush, &brush_local_min, &brush_local_max);

                        Float t;
                        if (RayIntersectsOBB(g_engine->camera.position, forward,
                            &brush->modelMatrix,
                            brush_local_min,
                            brush_local_max,
                            &t) && t < 3.0f) {
                            Bool is_locked = (atoi(Brush_GetProperty(brush, "locked", "0")) == 1);
                            if (is_locked) {
                                IO_FireOutput(ENTITY_BRUSH, i, "OnUseLocked", g_engine->lastFrame, nullptr);
                            }
                            else {
                                const Char* delay_str = Brush_GetProperty(brush, "delay", "0");
                                Float fire_time = g_engine->lastFrame + atof(delay_str);
                                IO_FireOutput(ENTITY_BRUSH, i, "OnPressed", fire_time, nullptr);
                            }
                        }
                    }
                    if (strcmp(brush->classname, "func_door") == 0) {
                        if (atoi(Brush_GetProperty(brush, "OpenOnUse", "1")) == 1) {
                            Vec3 brush_local_min, brush_local_max;
                            Brush_GetLocalAABB(brush, &brush_local_min, &brush_local_max);

                            Float t;
                            if (RayIntersectsOBB(g_engine->camera.position, forward, &brush->modelMatrix, brush_local_min, brush_local_max, &t) && t < 3.0f) {
                                if (brush->door_state == DOOR_STATE_CLOSED || brush->door_state == DOOR_STATE_CLOSING) {
                                    brush->door_state = DOOR_STATE_OPENING;
                                }
                                else if (brush->door_state == DOOR_STATE_OPEN || brush->door_state == DOOR_STATE_OPENING) {
                                    brush->door_state = DOOR_STATE_CLOSING;
                                }
                                IO_FireOutput(ENTITY_BRUSH, i, "OnUsed", g_engine->lastFrame, nullptr);
                            }
                        }
                    }
                    if (strcmp(brush->classname, "func_healthcharger") == 0) {
                        Vec3 brush_local_min, brush_local_max;
                        Brush_GetLocalAABB(brush, &brush_local_min, &brush_local_max);

                        Float t;
                        if (RayIntersectsOBB(g_engine->camera.position, forward, &brush->modelMatrix, brush_local_min, brush_local_max, &t) && t < 3.0f) {
                            if (g_engine->camera.health < 100.0f) {
                                const Char* heal_str = Brush_GetProperty(brush, "heal_amount", "25");
                                Float heal_amount = atof(heal_str);

                                g_engine->camera.health += heal_amount;
                                if (g_engine->camera.health > 100.0f) {
                                    g_engine->camera.health = 100.0f;
                                }

                                IO_FireOutput(ENTITY_BRUSH, i, "OnUse", g_engine->lastFrame, nullptr);
                            }
                        }
                    }
                }

                for (Int i = 0; i < g_scene.numLogicEntities; ++i) {
                    LogicEntity* ent = &g_scene.logicEntities[i];
                    if (strcmp(ent->classname, "item_note") == 0) {
                        Float radius = atof(LogicEntity_GetProperty(ent, "radius", "1.0"));
                        Float dist_sq = vec3_length_sq(vec3_sub(g_engine->camera.position, ent->pos));

                        Vec3 to_ent = vec3_sub(ent->pos, g_engine->camera.position);
                        vec3_normalize(&to_ent);
                        Float dot = vec3_dot(forward, to_ent);

                        if (dist_sq < (radius * radius) + 4.0f && dot > 0.9f) {
                            ExecuteInput(ent->targetname, "Use", "", &g_scene, g_engine);
                            return;
                        }
                    }
                }

                RaycastHitInfo hitInfo;
                if (Physics_Raycast(g_engine->physicsWorld, g_engine->camera.position, ray_end, &hitInfo)) {
                    if (hitInfo.hitBody && Physics_GetMass(hitInfo.hitBody) < 2.0f && Physics_GetMass(hitInfo.hitBody) > 0.0f) {
                        g_engine->heldObject = hitInfo.hitBody;
                        g_engine->holdDistance = vec3_length(vec3_sub(hitInfo.point, g_engine->camera.position));
                        Physics_SetGravityEnabled(g_engine->heldObject, false);
                        Physics_SetCcdEnabled(g_engine->heldObject, false, 0.0f);
                    }
                }
            }
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                if (g_engine->keypad_active) {
                    g_engine->keypad_active = false;
                    g_player_input_disabled = false;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    return;
                }
                if (g_engine->note_active) {
                    g_engine->note_active = false;
                    g_player_input_disabled = false;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    if (g_engine->active_note_entity_index != -1) {
                        IO_FireOutput(ENTITY_LOGIC, g_engine->active_note_entity_index, "OnRead", g_engine->lastFrame, nullptr);
                    }
                    return;
                }
                if (g_current_mode == MODE_GAME) {
                    g_current_mode = MODE_INGAMEMENU;
                    Bool map_is_currently_loaded = (g_scene.numObjects > 0 || g_scene.numBrushes > 0);
                    MainMenu_SetInGameMenuMode(true, map_is_currently_loaded);
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
                else if (g_current_mode == MODE_INGAMEMENU) {
                    g_current_mode = MODE_GAME;
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
            }
            else if (event.key.keysym.sym == SDLK_BACKQUOTE) {
                if (Cvar_GetInt("con_enable")) {
                    Console_Toggle();
                    if (g_current_mode == MODE_GAME || g_current_mode == MODE_INGAMEMENU) {
                        SDL_SetRelativeMouseMode(Console_IsVisible() ? SDL_FALSE : SDL_TRUE);
                    }
                }
            }
#ifndef GAME_RELEASE
            else if (event.key.keysym.sym == SDLK_F5) {
                if (g_current_mode != MODE_MAINMENU) {
                    Char* args[] = { (Char*)"edit" };
                    Commands_Execute(1, args);
                }
            }
#endif
            else if (event.key.keysym.sym == SDLK_f && g_current_mode == MODE_GAME && !Console_IsVisible()) {
                g_engine->flashlight_on = !g_engine->flashlight_on;
                SoundSystem_PlaySound(g_flashlight_sound_buffer, g_engine->camera.position, 1.0f, 1.0f, 50.0f, false);
            }
            else {
                if (g_current_mode == MODE_GAME && !Console_IsVisible()) {
                    if (event.key.keysym.sym == SDLK_1) {
                        Weapons_Switch(WEAPON_NONE);
                        continue;
                    }
                    if (event.key.keysym.sym == SDLK_2) {
                        Weapons_Switch(WEAPON_PISTOL);
                        continue;
                    }
                    const Char* command = Binds_GetCommand(event.key.keysym.sym);
                    if (command) {
                        Char cmd_copy[MAX_COMMAND_LENGTH];
                        strcpy(cmd_copy, command);
                        Char* argv[16];
                        Int argc = 0;
                        Char* p = strtok(cmd_copy, " ");
                        while (p != nullptr && argc < 16) {
                            argv[argc++] = p;
                            p = strtok(nullptr, " ");
                        }
                        if (argc > 0) {
                            Commands_Execute(argc, argv);
                        }
                    }
                }
            }
        }

        if (g_current_mode == MODE_GAME || g_current_mode == MODE_EDITOR) {
            if (event.type == SDL_MOUSEMOTION) {
                Bool can_look_in_editor = (g_current_mode == MODE_EDITOR) || (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT));
                Bool can_look_in_game = (g_current_mode == MODE_GAME && !Console_IsVisible() && !g_player_input_disabled);

                if (can_look_in_game || can_look_in_editor) {
                    Float sensitivity = Cvar_GetFloat("sensitivity");
                    g_engine->camera.yaw += event.motion.xrel * 0.005f * sensitivity;
                    g_engine->camera.pitch -= event.motion.yrel * 0.005f * sensitivity;
                    if (g_engine->camera.pitch > 1.55f) g_engine->camera.pitch = 1.55f;
                    if (g_engine->camera.pitch < -1.55f) g_engine->camera.pitch = -1.55f;
                }
            }
        }
    }

    if (g_current_mode == MODE_GAME && !Console_IsVisible()) {
        const Uint8* state = SDL_GetKeyboardState(nullptr);

        Bool noclip = Cvar_GetInt("noclip");
        Float speed = (noclip ? Cvar_GetFloat("g_noclip_speed") : 5.0f) * (g_engine->camera.isCrouching ? 0.5f : 1.0f);

        if (g_player_input_disabled) {
            if (!noclip) Physics_SetLinearVelocity(g_engine->camera.physicsBody, Vec3{ 0, 0, 0 });
            return;
        }

        if (noclip) {
            Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
            vec3_normalize(&forward);
            Vec3 right = vec3_cross(forward, Vec3{ 0, 1, 0 });
            vec3_normalize(&right);

            if (state[SDL_SCANCODE_W]) g_engine->camera.position = vec3_add(g_engine->camera.position, vec3_muls(forward, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_S]) g_engine->camera.position = vec3_sub(g_engine->camera.position, vec3_muls(forward, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_D]) g_engine->camera.position = vec3_add(g_engine->camera.position, vec3_muls(right, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_A]) g_engine->camera.position = vec3_sub(g_engine->camera.position, vec3_muls(right, speed * g_engine->deltaTime));
            if (state[SDL_SCANCODE_SPACE]) g_engine->camera.position.y += speed * g_engine->deltaTime;
            if (state[SDL_SCANCODE_LCTRL]) g_engine->camera.position.y -= speed * g_engine->deltaTime;
        }
        else if (g_player_on_ladder) {
            Float ladder_speed = Cvar_GetFloat("g_speed");
            Vec3 wish_vel = { 0, 0, 0 };

            if (state[SDL_SCANCODE_W]) {
                wish_vel.y = ladder_speed;
            }
            if (state[SDL_SCANCODE_S]) {
                wish_vel.y = -ladder_speed;
            }

            Vec3 f_flat = { sinf(g_engine->camera.yaw), 0, -cosf(g_engine->camera.yaw) };
            Vec3 r_flat = { f_flat.z, 0, -f_flat.x };
            Vec3 strafe_move = { 0, 0, 0 };
            if (state[SDL_SCANCODE_A]) strafe_move = vec3_add(strafe_move, r_flat);
            if (state[SDL_SCANCODE_D]) strafe_move = vec3_sub(strafe_move, r_flat);

            vec3_normalize(&strafe_move);
            wish_vel.x = strafe_move.x * ladder_speed;
            wish_vel.z = strafe_move.z * ladder_speed;

            Vec3 horizontal_vel = { wish_vel.x, 0, wish_vel.z };
            horizontal_vel = vec3_sub(horizontal_vel, vec3_muls(g_ladder_normal, vec3_dot(horizontal_vel, g_ladder_normal)));

            Vec3 final_vel = { horizontal_vel.x, wish_vel.y, horizontal_vel.z };

            final_vel = vec3_add(final_vel, vec3_muls(g_ladder_normal, -1.0f));

            Physics_SetLinearVelocity(g_engine->camera.physicsBody, final_vel);
            Physics_Activate(g_engine->camera.physicsBody);
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
            Float max_wish_speed = Cvar_GetFloat("g_speed");
            if (state[SDL_SCANCODE_LSHIFT] && !g_engine->camera.isCrouching) {
                max_wish_speed = Cvar_GetFloat("g_sprint_speed");
            }
            if (g_engine->camera.isCrouching) {
                max_wish_speed *= 0.5f;
            }

            Float accel = Cvar_GetFloat("g_accel");
            Float friction = Cvar_GetFloat("g_friction") * g_current_friction_modifier;

            Vec3 current_vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
            Vec3 current_vel_flat = { current_vel.x, 0, current_vel.z };

            Vec3 wish_vel = vec3_muls(move, max_wish_speed);

            Vec3 vel_delta = vec3_sub(wish_vel, current_vel_flat);

            if (vec3_length_sq(vel_delta) > 0.0001f) {
                Float delta_speed = vec3_length(vel_delta);
                Float add_speed = delta_speed * accel * friction * g_engine->deltaTime;

                if (add_speed > delta_speed) {
                    add_speed = delta_speed;
                }

                current_vel_flat = vec3_add(current_vel_flat, vec3_muls(vel_delta, add_speed / delta_speed));
            }

            if (vec3_length_sq(move) < 0.01f) {
                Float speed = vec3_length(current_vel_flat);
                if (speed > 0.001f) {
                    Float drop = speed * friction * g_engine->deltaTime;
                    Float new_speed = speed - drop;
                    if (new_speed < 0) new_speed = 0;
                    current_vel_flat = vec3_muls(current_vel_flat, new_speed / speed);
                }
                else {
                    current_vel_flat = Vec3{ 0,0,0 };
                }
            }

            Physics_SetLinearVelocity(g_engine->camera.physicsBody, Vec3{ current_vel_flat.x, current_vel.y, current_vel_flat.z });
            Physics_Activate(g_engine->camera.physicsBody);

            if (state[SDL_SCANCODE_SPACE]) {
                Vec3 current_vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
                if (current_vel.y <= 0.1f && Physics_CheckGroundContact(g_engine->physicsWorld, g_engine->camera.physicsBody, 0.1f)) {
                    Physics_ApplyCentralImpulse(g_engine->camera.physicsBody, Vec3{ 0, Cvar_GetFloat("g_jump_force"), 0 });
                    SoundSystem_PlaySound(g_jump_sound_buffer, g_engine->camera.position, 1.0f, 1.0f, 50.0f, false);
                }
            }
        }
        g_engine->camera.isCrouching = state[SDL_SCANCODE_LCTRL];
    }
}

void update_state() {
    if (IPC_IsTConsoleConnected() && !g_sent_initial_ipc_data) {
        for (Int i = 0; i < Cvar_GetCount(); ++i) {
            const Cvar* c = Cvar_GetCvar(i);
            if (c && !(c->flags & CVAR_HIDDEN)) {
                Char buffer[512];
                snprintf(buffer, sizeof(buffer), "register_cvar \"%s\" \"%s\" \"%s\"", c->name, c->stringValue, c->helpText);
                IPC_SendMessage(buffer);
            }
        }
        for (Int i = 0; i < Commands_GetCount(); ++i) {
            const Command* cmd = Commands_GetCommand(i);
            if (cmd) {
                Char buffer[512];
                snprintf(buffer, sizeof(buffer), "register_cmd \"%s\" \"%s\"", cmd->name, cmd->description);
                IPC_SendMessage(buffer);
            }
        }
        g_sent_initial_ipc_data = true;
    }
    if (!IPC_IsTConsoleConnected()) {
        g_sent_initial_ipc_data = false;
    }
    Int current_rawinput_cvar = Cvar_GetInt("in_rawinput");
    if (current_rawinput_cvar != g_last_rawinput_cvar_state) {
        if (!SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, current_rawinput_cvar ? "0" : "1")) {
            Console_Printf_Warning("Failed to set raw mouse input hint.");
        }
        g_last_rawinput_cvar_state = current_rawinput_cvar;
    }
    Int current_monitor_cvar = Cvar_GetInt("r_monitor");
    if (current_monitor_cvar != g_last_monitor_cvar_state) {
        Int num_displays = SDL_GetNumVideoDisplays();
        if (current_monitor_cvar >= 0 && current_monitor_cvar < num_displays) {
            Uint32 window_flags = SDL_GetWindowFlags(g_engine->window);
            Bool is_fullscreen = (window_flags & SDL_WINDOW_FULLSCREEN_DESKTOP) || (window_flags & SDL_WINDOW_FULLSCREEN);

            SDL_Rect display_bounds;
            SDL_GetDisplayBounds(current_monitor_cvar, &display_bounds);

            if (is_fullscreen) {
                SDL_SetWindowFullscreen(g_engine->window, 0);
                SDL_SetWindowPosition(g_engine->window, display_bounds.x, display_bounds.y);
                SDL_SetWindowFullscreen(g_engine->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            else {
                Int w, h;
                SDL_GetWindowSize(g_engine->window, &w, &h);
                SDL_SetWindowPosition(g_engine->window,
                    display_bounds.x + (display_bounds.w - w) / 2,
                    display_bounds.y + (display_bounds.h - h) / 2);
            }
            g_last_monitor_cvar_state = current_monitor_cvar;
        }
        else {
            Cvar_Set("r_monitor", "0");
        }
    }

    g_is_unlit_mode = Cvar_GetInt("r_fullbright");
    Int deactivation_cvar = Cvar_GetInt("p_disable_deactivation");
    if (deactivation_cvar != g_last_deactivation_cvar_state) {
        if (g_engine->physicsWorld) {
            Physics_SetDeactivationEnabled(g_engine->physicsWorld, deactivation_cvar == 0);
        }
        g_last_deactivation_cvar_state = deactivation_cvar;
    }
    if (g_engine->camera.health < g_engine->prev_health) {
        g_engine->red_flash_intensity = 1.0f;
        g_engine->shake_amplitude = 4.0f;
        g_engine->shake_frequency = 50.0f;
        g_engine->shake_duration_timer = 0.2f;
    }
    g_engine->prev_health = g_engine->camera.health;

    if (g_engine->red_flash_intensity > 0.0f) {
        g_engine->red_flash_intensity -= g_engine->deltaTime * 2.0f;
        if (g_engine->red_flash_intensity < 0.0f) {
            g_engine->red_flash_intensity = 0.0f;
        }
    }
    if (g_engine->shake_duration_timer > 0) {
        g_engine->shake_duration_timer -= g_engine->deltaTime;
        if (g_engine->shake_duration_timer <= 0) {
            g_engine->shake_amplitude = 0.0f;
            g_engine->shake_frequency = 0.0f;
        }
    }
    g_engine->running = Cvar_GetInt("engine_running");
    if (Cvar_GetInt("s_mute")) {
        SoundSystem_SetMasterVolume(0.0f);
    }
    else {
        SoundSystem_SetMasterVolume(Cvar_GetFloat("s_volume"));
    }
    g_engine->camera.rads_per_second = 0.0f;
    if (g_current_mode == MODE_GAME) {
        for (Int i = 0; i < g_scene.numLogicEntities; ++i) {
            LogicEntity* ent = &g_scene.logicEntities[i];
            if (strcmp(ent->classname, "point_radiation_source") == 0 && ent->runtime_active) {
                Float radius = atof(LogicEntity_GetProperty(ent, "radius", "500"));
                Float rad_s = atof(LogicEntity_GetProperty(ent, "rad_s", "10.0"));

                Float dist_sq = vec3_length_sq(vec3_sub(g_engine->camera.position, ent->pos));
                if (dist_sq < radius * radius) {
                    g_engine->camera.rads_per_second += rad_s / fmaxf(1.0f, dist_sq);
                }
            }
        }

        if (g_engine->camera.rads_per_second > 0.01f) {
            g_engine->camera.radiation_level += g_engine->camera.rads_per_second * g_engine->deltaTime;
        }
        else {
            constexpr Float decay_rate = 1.5f;
            g_engine->camera.radiation_level -= decay_rate * g_engine->deltaTime;
            if (g_engine->camera.radiation_level < 0.0f) {
                g_engine->camera.radiation_level = 0.0f;
            }
        }

        if (g_engine->camera.radiation_level > 1000.0f) {
            g_engine->camera.radiation_level = 1000.0f;
        }
        if (g_engine->camera.radiation_level >= 1000.0f && Cvar_GetInt("god") == 0) {
            g_engine->camera.health = 0.0f;
        }

        if (g_engine->camera.rads_per_second > 0.1f) {
            g_geiger_timer -= g_engine->deltaTime;
            if (g_geiger_timer <= 0.0f) {
                Float interval = 1.0f / (1.0f + g_engine->camera.rads_per_second * 1.5f);
                Float pitch = 1.0f + (g_engine->camera.rads_per_second / 50.0f);
                pitch = fminf(pitch, 3.0f);

                SoundSystem_PlaySound(g_geiger_tick_sound_buffer, g_engine->camera.position, 0.5f, pitch, 10.0f, false);

                g_geiger_timer = interval + rand_float_range(0.0f, interval * 0.5f);
            }
        }
    }
    for (Int i = 0; i < MAX_GAME_TEXT_MESSAGES; ++i) {
        GameTextMessage* msg = &g_engine->active_messages[i];
        if (msg->state == TEXT_STATE_IDLE) continue;

        msg->timer += g_engine->deltaTime;

        if (msg->state == TEXT_STATE_FADING_IN) {
            if (msg->fadeInTime > 0.0f) {
                msg->currentAlpha = fminf(1.0f, msg->timer / msg->fadeInTime);
            }
            else {
                msg->currentAlpha = 1.0f;
            }
            if (msg->timer >= msg->fadeInTime) {
                msg->state = TEXT_STATE_HOLDING;
                msg->timer = 0.0f;
            }
        }
        else if (msg->state == TEXT_STATE_HOLDING) {
            msg->currentAlpha = 1.0f;
            if (msg->timer >= msg->holdTime) {
                msg->state = TEXT_STATE_FADING_OUT;
                msg->timer = 0.0f;
            }
        }
        else if (msg->state == TEXT_STATE_FADING_OUT) {
            if (msg->fadeOutTime > 0.0f) {
                msg->currentAlpha = 1.0f - fminf(1.0f, msg->timer / msg->fadeOutTime);
            }
            else {
                msg->currentAlpha = 0.0f;
            }
            if (msg->timer >= msg->fadeOutTime) {
                msg->state = TEXT_STATE_IDLE;
            }
        }
    }
    g_engine->canUse = false;
    if (g_current_mode == MODE_GAME && !g_player_input_disabled && !Console_IsVisible()) {
        if (g_engine->heldObject == nullptr) {
            Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
            vec3_normalize(&forward);
            Vec3 ray_end = vec3_add(g_engine->camera.position, vec3_muls(forward, 3.0f));

            for (Int i = 0; i < g_scene.numBrushes; ++i) {
                Brush* brush = &g_scene.brushes[i];
                Bool is_usable = false;
                if (strcmp(brush->classname, "func_button") == 0) {
                    is_usable = true;
                }
                else if (strcmp(brush->classname, "func_door") == 0) {
                    if (atoi(Brush_GetProperty(brush, "OpenOnUse", "1")) == 1) {
                        is_usable = true;
                    }
                }
                else if (strcmp(brush->classname, "func_healthcharger") == 0) {
                    is_usable = true;
                }

                if (is_usable) {
                    Vec3 brush_local_min, brush_local_max;
                    Brush_GetLocalAABB(brush, &brush_local_min, &brush_local_max);

                    Float t;
                    if (RayIntersectsOBB(g_engine->camera.position, forward, &brush->modelMatrix, brush_local_min, brush_local_max, &t) && t < 3.0f) {
                        g_engine->canUse = true;
                        break;
                    }
                }
            }
            for (Int i = 0; i < g_scene.numLogicEntities; ++i) {
                LogicEntity* ent = &g_scene.logicEntities[i];
                if (strcmp(ent->classname, "item_note") == 0) {
                    Float radius = atof(LogicEntity_GetProperty(ent, "radius", "1.0"));
                    Float dist_sq = vec3_length_sq(vec3_sub(g_engine->camera.position, ent->pos));
                    Vec3 to_ent = vec3_sub(ent->pos, g_engine->camera.position);
                    vec3_normalize(&to_ent);
                    Float dot = vec3_dot(forward, to_ent);

                    if (dist_sq < (radius * radius) + 4.0f && dot > 0.9f) {
                        g_engine->canUse = true;
                    }
                }
            }
            RaycastHitInfo hitInfo;
            if (Physics_Raycast(g_engine->physicsWorld, g_engine->camera.position, ray_end, &hitInfo)) {
                if (hitInfo.hitBody && Physics_GetMass(hitInfo.hitBody) < 2.0f && Physics_GetMass(hitInfo.hitBody) > 0.0f) {
                    g_engine->canUse = true;
                }
            }
        }
    }
    IO_ProcessPendingEvents(g_engine->lastFrame, &g_scene, g_engine);
    LogicSystem_Update(&g_scene, g_engine->deltaTime);
    if (g_engine->heldObject) {
        Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
        vec3_normalize(&forward);
        Vec3 targetPos = vec3_add(g_engine->camera.position, vec3_muls(forward, g_engine->holdDistance));

        Physics_Teleport(g_engine->heldObject, targetPos);
    }
    Scene_UpdateAnimations(&g_scene, g_engine->deltaTime);
    if (g_engine->active_camera_brush_index != -1) {
        Brush* cam_brush = &g_scene.brushes[g_engine->active_camera_brush_index];
        const Char* target_name = Brush_GetProperty(cam_brush, "target", "");
        Vec3 target_pos;
        Vec3 target_angles;

        if (IO_FindNamedEntity(&g_scene, target_name, &target_pos, &target_angles)) {
            Float moveto_time = atof(Brush_GetProperty(cam_brush, "moveto", "2.0"));
            g_engine->camera_transition_timer += g_engine->unscaledDeltaTime;

            Float t = 1.0f;
            if (moveto_time > 0.0f) {
                t = fminf(g_engine->camera_transition_timer / moveto_time, 1.0f);
            }

            g_engine->camera.position = vec3_lerp(g_engine->camera_original_pos, target_pos, t);
            g_engine->camera.yaw = g_engine->camera_original_yaw + (target_angles.y * (M_PI / 180.0f) - g_engine->camera_original_yaw) * t;
            g_engine->camera.pitch = g_engine->camera_original_pitch + (target_angles.x * (M_PI / 180.0f) - g_engine->camera_original_pitch) * t;

            if (t >= 1.0f) {
                Float hold_time = atof(Brush_GetProperty(cam_brush, "holdtime", "5.0"));
                if (g_engine->camera_transition_timer >= moveto_time + hold_time) {
                    ExecuteInput(cam_brush->targetname, "Disable", "", &g_scene, g_engine);
                    IO_FireOutput(ENTITY_BRUSH, g_engine->active_camera_brush_index, "OnEnd", g_engine->lastFrame, nullptr);
                }
            }
        }
        else {
            ExecuteInput(cam_brush->targetname, "Disable", "", &g_scene, g_engine);
        }
    }
    if (g_engine->credits_active) {
        g_engine->credits_timer += g_engine->unscaledDeltaTime;
        if (g_engine->credits_timer >= g_engine->credits_duration) {
            IO_FireOutput(ENTITY_LOGIC, g_engine->credits_entity_index, "OnCreditsDone", g_engine->lastFrame, nullptr);
            g_engine->credits_active = false;
            if (g_engine->credits_text) {
                delete[] g_engine->credits_text;
                g_engine->credits_text = nullptr;
            }
            g_engine->credits_entity_index = -1;
        }
    }
    Weapons_Update(g_engine->deltaTime);
    for (Int i = 0; i < g_scene.numActiveLights; ++i) {
        Light* light = &g_scene.lights[i];

        if (!light->is_on) {
            light->intensity = 0.0f;
        }
        else {
            const Char* style = nullptr;
            if (light->preset > 0 && light->preset <= 12) {
                style = g_light_styles[light->preset];
            }
            else if (light->preset == 13) {
                style = light->custom_style_string;
            }

            if (style && strlen(style) > 0) {
                Int style_len = strlen(style);
                light->preset_time += g_engine->deltaTime;
                while (light->preset_time >= 0.1f) {
                    light->preset_time -= 0.1f;
                    light->preset_index = (light->preset_index + 1) % style_len;
                }

                Char c = style[light->preset_index];
                Float brightness = (Float)(c - 'a') / (Float)('m' - 'a');
                light->intensity = light->base_intensity * brightness;
            }
            else {
                light->intensity = light->base_intensity;
            }
        }

        if (light->type == LIGHT_SPOT) {
            Mat4 rot_mat = create_trs_matrix(Vec3{ 0, 0, 0 }, light->rot, Vec3{ 1, 1, 1 });
            Vec3 forward = { 0, 0, -1 };
            light->direction = mat4_mul_vec3_dir(&rot_mat, forward);
            vec3_normalize(&light->direction);
        }
    }
    if (g_current_mode == MODE_MAINMENU || g_current_mode == MODE_INGAMEMENU) {
        MainMenu_Update(g_engine->deltaTime);
        return;
    }
    if (g_current_mode == MODE_EDITOR) { Editor_Update(g_engine, &g_scene); return; }
    if (Cvar_GetInt("r_particles")) {
        Float particle_cull_dist = Cvar_GetFloat("r_particles_cull_dist");
        Float particle_cull_dist_sq = particle_cull_dist * particle_cull_dist;
        for (Int i = 0; i < g_scene.numParticleEmitters; ++i) {
            if (vec3_length_sq(vec3_sub(g_scene.particleEmitters[i].pos, g_engine->camera.position)) < particle_cull_dist_sq) {
                ParticleEmitter_Update(&g_scene.particleEmitters[i], g_engine->deltaTime);
            }
        }
    }
    VideoPlayer_UpdateAll(&g_scene, g_engine->deltaTime);
    Vec3 playerPos;
    Physics_GetPosition(g_engine->camera.physicsBody, &playerPos);

    Int new_reverb_zone_index = -1;
    for (Int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "trigger_dspzone") != 0) continue;

        Vec3 min_aabb, max_aabb;
        Brush_GetWorldAABB(b, &min_aabb, &max_aabb);

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
            Brush* b = &g_scene.brushes[new_reverb_zone_index];
            const Char* preset_str = Brush_GetProperty(b, "reverb_preset", "0");
            SoundSystem_SetCurrentReverb((ReverbPreset)atoi(preset_str));
        }
        else {
            SoundSystem_SetCurrentReverb(REVERB_PRESET_NONE);
        }
    }

    for (Int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strlen(b->classname) == 0 || !b->runtime_active) continue;

        Vec3 min_aabb, max_aabb;
        Brush_GetWorldAABB(b, &min_aabb, &max_aabb);

        Bool is_inside = (playerPos.x >= min_aabb.x && playerPos.x <= max_aabb.x &&
            playerPos.y >= min_aabb.y && playerPos.y <= max_aabb.y &&
            playerPos.z >= min_aabb.z && playerPos.z <= max_aabb.z);

        if (is_inside && !b->runtime_playerIsTouching) {
            b->runtime_playerIsTouching = true;
            if (strcmp(b->classname, "trigger_once") == 0) {
                if (!b->runtime_hasFired) {
                    const Char* delay_str = Brush_GetProperty(b, "delay", "0");
                    Float fire_time = g_engine->lastFrame + atof(delay_str);
                    IO_FireOutput(ENTITY_BRUSH, i, "OnStartTouch", fire_time, nullptr);
                    b->runtime_hasFired = true;
                }
            }
            else if (strcmp(b->classname, "trigger_multiple") == 0) {
                const Char* delay_str = Brush_GetProperty(b, "delay", "0");
                Float fire_time = g_engine->lastFrame + atof(delay_str);
                IO_FireOutput(ENTITY_BRUSH, i, "OnStartTouch", fire_time, nullptr);
            }
            else if (strcmp(b->classname, "trigger_teleport") == 0) {
                const Char* target_name = Brush_GetProperty(b, "target", "");
                Vec3 target_pos;
                Vec3 target_angles;
                if (strlen(target_name) > 0 && IO_FindNamedEntity(&g_scene, target_name, &target_pos, &target_angles)) {
                    if (g_engine->camera.physicsBody) {
                        Physics_Teleport(g_engine->camera.physicsBody, target_pos);
                    }
                    g_engine->camera.position = target_pos;
                    g_engine->camera.yaw = target_angles.y * (M_PI / 180.0f);
                    g_engine->camera.pitch = target_angles.x * (M_PI / 180.0f);
                }
            }
            else if (strcmp(b->classname, "trigger_camera") == 0) {
                if (g_engine->active_camera_brush_index != i) {
                    ExecuteInput(b->targetname, "Enable", "", &g_scene, g_engine);
                }
            }
            else if (strcmp(b->classname, "trigger_paralyzeplayer") == 0) {
                g_player_input_disabled = true;
            }
        }
        else if (strcmp(b->classname, "trigger_autosave") == 0) {
            if (!b->runtime_hasFired) {
                Char save_name[128];
                time_t now = time(nullptr);
                strftime(save_name, sizeof(save_name), "autosave_%Y%m%d_%H%M%S", localtime(&now));

                Char* argv[] = { (Char*)"save", save_name };
                Cmd_SaveGame(2, argv);

                b->runtime_hasFired = true;
            }
        }
        else if (!is_inside && b->runtime_playerIsTouching) {
            b->runtime_playerIsTouching = false;
            if (strcmp(b->classname, "trigger_multiple") == 0 || strcmp(b->classname, "trigger_once") == 0) {
                IO_FireOutput(ENTITY_BRUSH, i, "OnEndTouch", g_engine->lastFrame, nullptr);
            }
        }
        if (is_inside && strcmp(b->classname, "trigger_hurt") == 0) {
            const Char* damage_str = Brush_GetProperty(b, "damage", "10");
            Float damage_per_second = atof(damage_str);
            if (Cvar_GetInt("god") == 0) {
                g_engine->camera.health -= damage_per_second * g_engine->deltaTime;
            }
        }
        else if (is_inside && strcmp(b->classname, "trigger_killplayer") == 0) {
            if (Cvar_GetInt("god") == 0) {
                g_engine->camera.health = 0.0f;
            }
        }
    }
    Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
    vec3_normalize(&forward); SoundSystem_UpdateListener(g_engine->camera.position, forward, Vec3{ 0, 1, 0 });
    SoundSystem_Update();
    Bool noclip = Cvar_GetInt("noclip");
    if (!noclip) {
        Vec3 vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
        Bool on_ground = fabs(vel.y) < 0.1f;

        if (on_ground) {
            Float dx = g_engine->camera.position.x - g_last_player_pos.x;
            Float dz = g_engine->camera.position.z - g_last_player_pos.z;
            g_distance_walked += sqrtf(dx * dx + dz * dz);

            if (g_distance_walked >= FOOTSTEP_DISTANCE) {
                SoundSystem_PlaySound(g_footstep_sound_buffer, g_engine->camera.position, 0.5f, 1.0f, 50.0f, false);
                g_distance_walked = 0.0f;
            }
        }
        else {
            g_distance_walked = 0.0f;
        }

        g_last_player_pos = g_engine->camera.position;
    }
    if (g_engine->physicsWorld) {
        for (Int i = 0; i < g_scene.numBrushes; ++i) {
            Brush* b = &g_scene.brushes[i];
            // This causes some problems where the player floats in the air temporarily disable
            //if (strcmp(b->classname, "func_water") == 0 && b->numVertices > 0) {
            //    Physics_ApplyBuoyancyInVolume(g_engine->physicsWorld, (const Float*)b->vertices, b->numVertices, &b->modelMatrix);
            //}
        }
    }
    g_current_friction_modifier = 1.0f;
    for (Int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_friction") == 0 && b->runtime_playerIsTouching) {
            const Char* modifier_str = Brush_GetProperty(b, "modifier", "100");
            Float modifier = atof(modifier_str);
            g_current_friction_modifier = modifier / 100.0f;
            break;
        }
    }
    for (Int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_conveyor") != 0) continue;

        const Char* speed_str = Brush_GetProperty(b, "speed", "0");
        Float speed = atof(speed_str);
        if (speed == 0.0f) continue;

        const Char* dir_str = Brush_GetProperty(b, "direction", "0 0 0");
        Float pitch, yaw, roll;
        sscanf(dir_str, "%f %f %f", &pitch, &yaw, &roll);

        Float pitch_rad = pitch * (M_PI / 180.0f);
        Float yaw_rad = yaw * (M_PI / 180.0f);
        Float roll_rad = roll * (M_PI / 180.0f);

        Mat4 rot_x_mat = mat4_rotate_x(pitch_rad);
        Mat4 rot_y_mat = mat4_rotate_y(yaw_rad);
        Mat4 rot_z_mat = mat4_rotate_z(roll_rad);
        Mat4 rot_mat;
        mat4_multiply(&rot_mat, &rot_y_mat, &rot_x_mat);
        mat4_multiply(&rot_mat, &rot_z_mat, &rot_mat);

        Vec3 base_dir = { 1, 0, 0 };
        Vec3 move_dir = mat4_mul_vec3_dir(&rot_mat, base_dir);
        Vec3 conveyor_vel = vec3_muls(move_dir, speed);

        if (b->runtime_playerIsTouching) {
            Vec3 player_vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
            Physics_SetLinearVelocity(g_engine->camera.physicsBody, Vec3{ conveyor_vel.x, player_vel.y, conveyor_vel.z });
            Physics_Activate(g_engine->camera.physicsBody);
        }

        Vec3 conveyor_min, conveyor_max;
        if (b->numVertices > 0) {
            Brush_GetWorldAABB(b, &conveyor_min, &conveyor_max);

            for (Int obj_idx = 0; obj_idx < g_scene.numObjects; ++obj_idx) {
                SceneObject* obj = &g_scene.objects[obj_idx];
                if (obj->mass > 0.0f) {
                    Vec3 obj_pos = obj->pos;
                    if (obj_pos.x > conveyor_min.x && obj_pos.x < conveyor_max.x &&
                        obj_pos.y > conveyor_min.y && obj_pos.y < conveyor_max.y + 1.0f &&
                        obj_pos.z > conveyor_min.z && obj_pos.z < conveyor_max.z)
                    {
                        Vec3 obj_vel = Physics_GetLinearVelocity(obj->physicsBody);
                        Physics_SetLinearVelocity(obj->physicsBody, Vec3{ conveyor_vel.x, obj_vel.y, conveyor_vel.z });
                        Physics_Activate(obj->physicsBody);
                    }
                }
            }
        }
    }
    g_player_on_ladder = false;
    for (Int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_ladder") == 0 && b->runtime_playerIsTouching) {
            Vec3 forward = { cosf(g_engine->camera.pitch) * sinf(g_engine->camera.yaw), sinf(g_engine->camera.pitch), -cosf(g_engine->camera.pitch) * cosf(g_engine->camera.yaw) };
            vec3_normalize(&forward);
            Vec3 ray_end = vec3_add(g_engine->camera.position, vec3_muls(forward, 2.0f));
            RaycastHitInfo hit_info;
            if (Physics_Raycast(g_engine->physicsWorld, g_engine->camera.position, ray_end, &hit_info)) {
                g_player_on_ladder = true;
                g_ladder_normal = hit_info.normal;
                break;
            }
        }
    }
    Bool in_gravity_zone = false;
    for (Int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "trigger_gravity") == 0 && b->runtime_playerIsTouching) {
            Float gravity_val = atof(Brush_GetProperty(b, "gravity", "9.81"));
            Physics_SetGravity(g_engine->physicsWorld, Vec3{ 0, -gravity_val, 0 });
            in_gravity_zone = true;
            break;
        }
    }

    if (!in_gravity_zone) {
        Physics_SetGravity(g_engine->physicsWorld, Vec3{ 0, -Cvar_GetFloat("gravity"), 0 });
    }
    if (g_engine->camera.health <= 0.0f) {
        g_engine->camera.health = 100.0f;
        g_engine->prev_health = g_engine->camera.health;
        Physics_Teleport(g_engine->camera.physicsBody, g_scene.playerStart.pos);
    }
    Physics_SetGravityEnabled(g_engine->camera.physicsBody, !noclip);
    if (noclip) {
        Vec3 zeroVelocity = { 0, 0, 0 };
        Physics_SetLinearVelocity(g_engine->camera.physicsBody, zeroVelocity);
    }

    if (g_engine->physicsWorld) {
        Physics_StepSimulation(g_engine->physicsWorld, g_engine->deltaTime);
    }

    if (!noclip && !g_player_input_disabled) {
        Vec3 p;
        Physics_GetPosition(g_engine->camera.physicsBody, &p);

        g_engine->camera.position.x = p.x;
        g_engine->camera.position.z = p.z;

        Float target_height = g_engine->camera.isCrouching ? Cvar_GetFloat("g_crouch_height") : PLAYER_HEIGHT_NORMAL;
        Float crouch_speed = 10.0f;

        g_engine->camera.currentHeight += (target_height - g_engine->camera.currentHeight) * g_engine->deltaTime * crouch_speed;

        Float eyeHeightOffsetFromCenter = (g_engine->camera.currentHeight / 2.0f) * 0.85f;
        g_engine->camera.position.y = p.y + eyeHeightOffsetFromCenter;
    }

    if (!noclip) {
        Vec3 current_vel = Physics_GetLinearVelocity(g_engine->camera.physicsBody);
        Bool on_ground = Physics_CheckGroundContact(g_engine->physicsWorld, g_engine->camera.physicsBody, 0.1f);

        if (on_ground && g_engine->prev_player_y_velocity < -1.0f) {
            Float fall_speed = -g_engine->prev_player_y_velocity;

            constexpr Float min_fall_speed_for_damage = 10.0f;
            constexpr Float max_fall_speed_for_damage = 30.0f;
            constexpr Float max_damage = 100.0f;

            if (fall_speed > min_fall_speed_for_damage) {
                Float damage_lerp_factor = (fall_speed - min_fall_speed_for_damage) / (max_fall_speed_for_damage - min_fall_speed_for_damage);
                damage_lerp_factor = fmaxf(0.0f, fminf(1.0f, damage_lerp_factor));

                Float damage = damage_lerp_factor * max_damage;

                if (Cvar_GetInt("god") == 0) {
                    g_engine->camera.health -= damage;
                }
            }
        }
        g_engine->prev_player_y_velocity = current_vel.y;
    }
    for (Int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_door") == 0) {
            if (vec3_length_sq(b->door_move_dir) < 0.001f) {
                b->door_start_pos = b->pos;
                Vec3 move_angles;
                sscanf(Brush_GetProperty(b, "direction", "0 90 0"), "%f %f %f", &move_angles.x, &move_angles.y, &move_angles.z);

                if (move_angles.x == -90) {
                    b->door_move_dir = Vec3{ 0, 1, 0 };
                }
                else if (move_angles.x == 90) {
                    b->door_move_dir = Vec3{ 0, -1, 0 };
                }
                else {
                    Float yaw_rad = move_angles.y * (M_PI / 180.0f);
                    b->door_move_dir.x = cosf(yaw_rad);
                    b->door_move_dir.y = 0.0f;
                    b->door_move_dir.z = -sinf(yaw_rad);
                }

                Float move_dist = atof(Brush_GetProperty(b, "distance", "0"));

                if (move_dist <= 0) {
                    Vec3 min_aabb_local, max_aabb_local;
                    Brush_GetLocalAABB(b, &min_aabb_local, &max_aabb_local);
                    Vec3 size = vec3_sub(max_aabb_local, min_aabb_local);
                    move_dist = fabsf(size.x * b->door_move_dir.x) + fabsf(size.y * b->door_move_dir.y) + fabsf(size.z * b->door_move_dir.z);
                }

                b->door_end_pos = vec3_add(b->door_start_pos, vec3_muls(b->door_move_dir, move_dist));

                if (atoi(Brush_GetProperty(b, "StartOpen", "0")) == 1) {
                    b->pos = b->door_end_pos;
                    b->door_state = DOOR_STATE_OPEN;
                }
                else {
                    b->door_state = DOOR_STATE_CLOSED;
                }
                Brush_UpdateMatrix(b);
                if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
            }

            Float speed = atof(Brush_GetProperty(b, "speed", "100"));
            if (b->door_state == DOOR_STATE_OPENING) {
                Vec3 to_end = vec3_sub(b->door_end_pos, b->pos);
                Float dist_to_end = vec3_length(to_end);
                Float move_dist = speed * g_engine->deltaTime;

                if (move_dist >= dist_to_end) {
                    b->pos = b->door_end_pos;
                    b->door_state = DOOR_STATE_OPEN;
                    IO_FireOutput(ENTITY_BRUSH, i, "OnOpened", g_engine->lastFrame, nullptr);
                }
                else {
                    b->pos = vec3_add(b->pos, vec3_muls(b->door_move_dir, move_dist));
                }
                Brush_UpdateMatrix(b);
                if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
            }
            else if (b->door_state == DOOR_STATE_CLOSING) {
                Vec3 to_start = vec3_sub(b->door_start_pos, b->pos);
                Float dist_to_start = vec3_length(to_start);
                Float move_dist = speed * g_engine->deltaTime;

                if (move_dist >= dist_to_start) {
                    b->pos = b->door_start_pos;
                    b->door_state = DOOR_STATE_CLOSED;
                    IO_FireOutput(ENTITY_BRUSH, i, "OnClosed", g_engine->lastFrame, nullptr);
                }
                else {
                    b->pos = vec3_add(b->pos, vec3_muls(vec3_muls(b->door_move_dir, -1.0f), move_dist));
                }
                Brush_UpdateMatrix(b);
                if (b->physicsBody) Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
            }
        }
        if (strcmp(b->classname, "func_wall_toggle") == 0) {
            if (!b->runtime_hasFired) {
                b->runtime_is_visible = (atoi(Brush_GetProperty(b, "StartON", "1")) != 0);
                if (b->physicsBody) {
                    Physics_ToggleCollision(g_engine->physicsWorld, b->physicsBody, b->runtime_is_visible);
                }
                b->runtime_hasFired = true;
            }
        }
        if (strcmp(b->classname, "func_rotating") == 0 && b->runtime_active) {
            Bool use_accel = atoi(Brush_GetProperty(b, "AccDcc", "0")) != 0;

            if (use_accel) {
                Float friction = atof(Brush_GetProperty(b, "fanfriction", "0"));
                Float accel_factor = 1.0f - (friction / 100.0f);
                Float lerp_speed = 2.0f + (accel_factor * 8.0f);
                b->current_angular_velocity = vec3_lerp(Vec3{ b->current_angular_velocity, 0, 0 }, Vec3{ b->target_angular_velocity, 0, 0 }, g_engine->deltaTime * lerp_speed).x;
            }
            else {
                b->current_angular_velocity = b->target_angular_velocity;
            }

            if (fabsf(b->current_angular_velocity) > 0.001f) {
                Vec3 rotation_axis = { 0, 0, 1 };
                if (atoi(Brush_GetProperty(b, "XAxis", "0")) != 0) rotation_axis = Vec3{ 1, 0, 0 };
                if (atoi(Brush_GetProperty(b, "YAxis", "0")) != 0) rotation_axis = Vec3{ 0, 1, 0 };

                Float deg_per_sec = b->current_angular_velocity;
                Vec3 delta_rot = vec3_muls(rotation_axis, deg_per_sec * g_engine->deltaTime);
                b->rot = vec3_add(b->rot, delta_rot);

                b->rot.x = fmodf(b->rot.x, 360.0f);
                b->rot.y = fmodf(b->rot.y, 360.0f);
                b->rot.z = fmodf(b->rot.z, 360.0f);

                Brush_UpdateMatrix(b);
                if (b->physicsBody) {
                    Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
                }
            }
        }
    }
    for (Int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_plat") == 0 && b->runtime_active) {
            Float speed = atof(Brush_GetProperty(b, "speed", "150"));
            Float wait = atof(Brush_GetProperty(b, "wait", "3"));
            Bool is_trigger = atoi(Brush_GetProperty(b, "is_trigger", "0")) != 0;

            if (!is_trigger && b->runtime_playerIsTouching && b->plat_state == PLAT_STATE_BOTTOM) {
                b->plat_state = PLAT_STATE_UP;
            }

            if (b->plat_state == PLAT_STATE_UP) {
                Vec3 to_end = vec3_sub(b->end_pos, b->pos);
                Float dist_to_end = vec3_length(to_end);
                Float move_dist = speed * g_engine->deltaTime;

                if (move_dist >= dist_to_end) {
                    b->pos = b->end_pos;
                    b->plat_state = PLAT_STATE_TOP;
                    b->wait_timer = wait;
                }
                else {
                    b->pos = vec3_add(b->pos, vec3_muls(b->move_dir, move_dist));
                }
            }
            else if (b->plat_state == PLAT_STATE_DOWN) {
                Vec3 to_start = vec3_sub(b->start_pos, b->pos);
                Float dist_to_start = vec3_length(to_start);
                Float move_dist = speed * g_engine->deltaTime;

                if (move_dist >= dist_to_start) {
                    b->pos = b->start_pos;
                    b->plat_state = PLAT_STATE_BOTTOM;
                }
                else {
                    b->pos = vec3_add(b->pos, vec3_muls(vec3_muls(b->move_dir, -1.0f), move_dist));
                }
            }
            else if (b->plat_state == PLAT_STATE_TOP) {
                if (wait > 0) {
                    b->wait_timer -= g_engine->deltaTime;
                    if (b->wait_timer <= 0) {
                        b->plat_state = PLAT_STATE_DOWN;
                    }
                }
            }

            Brush_UpdateMatrix(b);
            if (b->physicsBody) {
                Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
            }
        }
        if (strcmp(b->classname, "func_pendulum") == 0) {
            if (vec3_length_sq(b->pendulum_swing_dir) < 0.001f) {
                b->pendulum_start_pos = b->pos;
                Vec3 swing_angles;
                sscanf(Brush_GetProperty(b, "direction", "0 90 0"), "%f %f %f", &swing_angles.x, &swing_angles.y, &swing_angles.z);

                Mat4 rot_mat = create_trs_matrix(Vec3{ 0, 0, 0 }, swing_angles, Vec3{ 1, 1, 1 });
                b->pendulum_swing_dir = mat4_mul_vec3_dir(&rot_mat, Vec3{ 1, 0, 0 });
                vec3_normalize(&b->pendulum_swing_dir);

                if (atoi(Brush_GetProperty(b, "StartON", "1")) == 1) {
                    b->runtime_active = true;
                }
            }

            if (b->runtime_active) {
                Float speed = atof(Brush_GetProperty(b, "speed", "1.0"));
                Float distance = atof(Brush_GetProperty(b, "distance", "10.0"));

                Float sine_wave_pos = sinf(g_engine->scaledTime * speed * 2.0f * M_PI);
                Vec3 offset = vec3_muls(b->pendulum_swing_dir, sine_wave_pos * distance);

                b->pos = vec3_add(b->pendulum_start_pos, offset);

                Brush_UpdateMatrix(b);
                if (b->physicsBody) {
                    Physics_SetWorldTransform(b->physicsBody, b->modelMatrix);
                }
            }
        }
        if (strcmp(b->classname, "func_weight_button") == 0) {
            if (b->physicsBody) {
                Float required_weight = atof(Brush_GetProperty(b, "weight", "50"));
                Float current_weight = Physics_GetTotalMassOnObject(g_engine->physicsWorld, b->physicsBody);

                Bool is_pressed = current_weight >= required_weight;

                if (is_pressed && !b->runtime_was_pressed) {
                    IO_FireOutput(ENTITY_BRUSH, i, "OnPressed", g_engine->lastFrame, nullptr);
                }
                else if (!is_pressed && b->runtime_was_pressed) {
                    IO_FireOutput(ENTITY_BRUSH, i, "OnReleased", g_engine->lastFrame, nullptr);
                }

                b->runtime_was_pressed = is_pressed;
            }
        }
    }
    g_scene.post.isUnderwater = false;
    for (Int i = 0; i < g_scene.numBrushes; ++i) {
        Brush* b = &g_scene.brushes[i];
        if (strcmp(b->classname, "func_water") != 0) continue;

        Vec3 min_aabb, max_aabb;
        Brush_GetWorldAABB(b, &min_aabb, &max_aabb);

        if (g_engine->camera.position.x >= min_aabb.x && g_engine->camera.position.x <= max_aabb.x &&
            g_engine->camera.position.y >= min_aabb.y && g_engine->camera.position.y <= max_aabb.y &&
            g_engine->camera.position.z >= min_aabb.z && g_engine->camera.position.z <= max_aabb.z)
        {
            g_scene.post.isUnderwater = true;
            g_scene.post.underwaterColor = Vec3{ 0.1f, 0.3f, 0.4f };
            break;
        }
    }
    if (g_current_mode == MODE_GAME) {
        for (Int i = 0; i < g_scene.numObjects; ++i) {
            SceneObject* obj = &g_scene.objects[i];
            if (obj->physicsBody && obj->mass > 0.0f) {
                Float phys_matrix_data[16];
                Physics_GetRigidBodyTransform(obj->physicsBody, phys_matrix_data);
                Mat4 physics_transform;
                memcpy(&physics_transform, phys_matrix_data, sizeof(Mat4));
                Mat4 scale_transform = mat4_scale(obj->scale);
                mat4_multiply(&obj->modelMatrix, &physics_transform, &scale_transform);
                mat4_decompose(&obj->modelMatrix, &obj->pos, &obj->rot, &obj->scale);
            }
        }
        for (Int i = 0; i < g_scene.numBrushes; ++i) {
            Brush* b = &g_scene.brushes[i];
            if (b->physicsBody && b->mass > 0.0f) {
                Float phys_matrix_data[16];
                Physics_GetRigidBodyTransform(b->physicsBody, phys_matrix_data);
                memcpy(&b->modelMatrix, phys_matrix_data, sizeof(Mat4));
            }
        }
    }
}