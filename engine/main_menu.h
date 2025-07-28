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
#ifndef MAIN_MENU_H
#define MAIN_MENU_H

//----------------------------------------//
// Brief: The input/output handling
//----------------------------------------//

#include <SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool g_show_options_menu;

typedef enum {
    MAINMENU_ACTION_NONE,
    MAINMENU_ACTION_START_GAME,
    MAINMENU_ACTION_OPTIONS,
    MAINMENU_ACTION_QUIT,
    MAINMENU_ACTION_CONTINUE_GAME
} MainMenuAction;

bool MainMenu_Init(int screen_width, int screen_height);
void MainMenu_Shutdown();
void MainMenu_SetInGameMenuMode(bool is_in_game, bool is_map_loaded);
MainMenuAction MainMenu_HandleEvent(SDL_Event* event);
void MainMenu_Update(float deltaTime);
void MainMenu_Render();

#ifdef __cplusplus
}
#endif

#endif // MAIN_MENU_H