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
#ifndef ENGINE_H
#define ENGINE_H

#include "map.h"
#include "engine_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { 
    MODE_GAME, 
    MODE_EDITOR, 
    MODE_MAINMENU, 
    MODE_INGAMEMENU 
} EngineMode;

typedef enum {
    TRANSITION_NONE,
    TRANSITION_TO_EDITOR,
    TRANSITION_TO_GAME
} EngineModeTransition;

extern Engine* g_engine;
extern Renderer g_renderer;
extern Scene g_scene;
extern EngineMode g_current_mode;
extern int g_last_water_cvar_state;
extern EngineModeTransition g_pending_mode_transition;
extern bool g_player_input_disabled;
extern bool g_screenshot_requested;
extern char g_screenshot_path[256];
extern bool g_is_editor_mode;

#ifdef __cplusplus
}
#endif

#endif // ENGINE_H