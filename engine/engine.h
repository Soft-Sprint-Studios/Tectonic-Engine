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
#ifndef ENGINE_H
#define ENGINE_H

#include "map.h"


    typedef enum {
        MODE_GAME,
#ifdef BUILD_EDITOR
        MODE_EDITOR,
#endif
        MODE_MAINMENU,
        MODE_INGAMEMENU
    } EngineMode;

    typedef enum {
        TRANSITION_NONE,
#ifdef BUILD_EDITOR
        TRANSITION_TO_EDITOR,
        TRANSITION_TO_GAME
#endif
    } EngineModeTransition;

    extern Engine* g_engine;
    extern Renderer g_renderer;
    extern Scene g_scene;
    extern EngineMode g_current_mode;
    extern Int g_last_water_cvar_state;
    extern EngineModeTransition g_pending_mode_transition;
    extern Bool g_player_input_disabled;
    extern Bool g_screenshot_requested;
    extern Bool g_quit_requested;
    extern Bool g_restart_requested;
    extern Char g_screenshot_path[256];
#ifdef BUILD_EDITOR
    extern Bool g_is_editor_mode;
#endif
    extern Uint g_flashlight_sound_buffer;
    extern Uint g_footstep_sound_buffer;
    extern Uint g_jump_sound_buffer;
    extern Uint g_geiger_tick_sound_buffer;
    extern Float g_geiger_timer;
    extern Bool g_player_on_ladder;
    extern Vec3 g_ladder_normal;
    extern Float g_current_friction_modifier;
    extern Bool g_sent_initial_ipc_data;
    extern Int g_last_rawinput_cvar_state;
    extern Int g_last_deactivation_cvar_state;
    extern Int g_last_monitor_cvar_state;
    extern Int g_current_reverb_zone_index;
    extern Vec3 g_last_player_pos;
    extern Float g_distance_walked;
    extern Float FOOTSTEP_DISTANCE;


#endif // ENGINE_H