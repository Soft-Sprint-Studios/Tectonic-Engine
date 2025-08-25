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
#include "gl_keypad.h"
#include "gl_console.h"
#include "io_system.h"

extern bool g_player_input_disabled;

void Keypad_RenderUI(Scene* scene, Engine* engine) {
    if (!engine->keypad_active) {
        return;
    }

    float screen_w, screen_h;
    UI_GetDisplaySize(&screen_w, &screen_h);
    UI_SetNextWindowPos(screen_w * 0.5f - 90.0f, screen_h * 0.5f - 140.0f);
    UI_SetNextWindowSize(80, 125);

    if (UI_Begin_WithFlags("Keypad", &engine->keypad_active, 1 << 0 | 1 << 3 | 1 << 5)) {
        UI_InputText_Flags("##code", engine->keypad_input_buffer, sizeof(engine->keypad_input_buffer), 1 << 10);

        const char* buttons[] = { "7", "8", "9", "4", "5", "6", "1", "2", "3", "C", "0", "E" };
        for (int i = 0; i < 12; i++) {
            if (i % 3 != 0) {
                UI_SameLine();
            }

            if (UI_Button(buttons[i])) {
                size_t len = strlen(engine->keypad_input_buffer);
                if (strcmp(buttons[i], "C") == 0) {
                    memset(engine->keypad_input_buffer, 0, sizeof(engine->keypad_input_buffer));
                } else if (strcmp(buttons[i], "E") == 0) {
                    LogicEntity* keypad_ent = &scene->logicEntities[engine->active_keypad_entity_index];
                    const char* correct_password = LogicEntity_GetProperty(keypad_ent, "password", "");

                    if (strcmp(engine->keypad_input_buffer, correct_password) == 0) {
                        IO_FireOutput(ENTITY_LOGIC, engine->active_keypad_entity_index, "OnPasswordCorrect", engine->lastFrame, NULL);
                    } else {
                        IO_FireOutput(ENTITY_LOGIC, engine->active_keypad_entity_index, "OnPasswordIncorrect", engine->lastFrame, NULL);
                    }
                    
                    engine->keypad_active = false;
                } else {
                    if (len < sizeof(engine->keypad_input_buffer) - 1) {
                        engine->keypad_input_buffer[len] = buttons[i][0];
                        engine->keypad_input_buffer[len + 1] = '\0';
                    }
                }
            }
        }
    }
    UI_End();

    if (!engine->keypad_active) {
        g_player_input_disabled = false;
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
}