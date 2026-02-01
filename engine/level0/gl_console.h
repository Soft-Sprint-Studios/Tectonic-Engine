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
#ifndef GL_CONSOLE_H
#define GL_CONSOLE_H

#include <SDL.h>
#include <stdbool.h>
#include "math_lib.h"
#include "imgui_wrapper.h"
#include "level0_api.h"

    typedef enum {
        CONSOLE_COLOR_WHITE,
        CONSOLE_COLOR_RED,
        CONSOLE_COLOR_YELLOW
    } ConsoleTextColor;

    typedef struct {
        char* text;
        ConsoleTextColor color;
    } ConsoleItem;

    LEVEL0_API void Console_Toggle();
    LEVEL0_API void Console_Draw();
    LEVEL0_API void Console_Printf(const char* fmt, ...);
    LEVEL0_API void Console_Printf_Error(const char* fmt, ...);
    LEVEL0_API void Console_Printf_Warning(const char* fmt, ...);
    LEVEL0_API bool Console_IsVisible();
    LEVEL0_API void Console_ClearLog();
    LEVEL0_API const ConsoleItem* Console_GetLogItems(int* count);

    LEVEL0_API void UI_RenderGameText(int num_messages, const char* texts[4], const float positions_x[4], const float positions_y[4], const Vec4 colors[4], const float alphas[4], const int states[4], const float scales[4]);
    LEVEL0_API void UI_RenderGameHUD(int modelsDrawn, int totalModels, int brushesDrawn, int totalBrushes, float fps, float px, float py, float pz, float health, bool canUse, float radiation, float rads_per_second, const float* fps_history, int history_size);
    LEVEL0_API void UI_RenderCredits(bool active, const char* text, float timer, float duration);
    LEVEL0_API void UI_RenderDeveloperOverlay(void);

    typedef void (*command_callback_t)(int argc, char** argv);
    LEVEL0_API void Console_SetCommandHandler(command_callback_t handler);

    LEVEL0_API void Log_Init(const char* filename);
    LEVEL0_API void Log_Shutdown(void);

#endif // GL_CONSOLE_H