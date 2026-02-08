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
#include "gl_note.h"
#include "gl_console.h"
#include "io_system.h"

extern Bool g_player_input_disabled;

void Note_RenderUI(Scene* scene, Engine* engine) {
    if (!engine->note_active) {
        return;
    }

    Float screen_w, screen_h;
    UI_GetDisplaySize(&screen_w, &screen_h);

    constexpr Float window_w = 400.0f;
    constexpr Float window_h = 500.0f;
    UI_SetNextWindowPos((screen_w - window_w) * 0.5f, (screen_h - window_h) * 0.5f);
    UI_SetNextWindowSize(window_w, window_h);

    if (UI_Begin_WithFlags("##NoteUI", &engine->note_active, 1 << 0 | 1 << 3 | 1 << 5)) {
        UI_SetNextItemWidth(window_w - 20);
        UI_TextColored(Vec4{ 1.0f, 0.8f, 0.2f, 1.0f }, "%s", engine->note_title);
        UI_Separator();
        UI_Spacing();

        UI_TextWrapped("%s", engine->note_content);
    }
    UI_End();

    if (!engine->note_active) {
        g_player_input_disabled = false;
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
}