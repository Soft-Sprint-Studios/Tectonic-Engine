/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
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