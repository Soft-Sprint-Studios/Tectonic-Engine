/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "main_menu.h"
#include <SDL_ttf.h>
#include <GL/glew.h>
#include <stdio.h>
#include <string.h>
#include "gameconfig.h"
#include "gl_console.h"
#include "math_lib.h"
#include "gl_misc.h"
#include "cvar.h"

bool g_show_options_menu = false;

static TTF_Font* g_menu_font = NULL;
static GLuint g_text_texture_start = 0;
static GLuint g_text_texture_options = 0;
static GLuint g_text_texture_quit = 0;
static GLuint g_text_texture_game_title = 0;

static int g_text_width_start, g_text_height_start;
static int g_text_width_options, g_text_height_options;
static int g_text_width_quit, g_text_height_quit;
static int g_text_width_game_title, g_text_height_game_title;

static int g_selected_button_index = 0;
static int g_num_buttons = 3;

static int g_screen_width = 0;
static int g_screen_height = 0;

static GLuint g_quad_vao = 0;
static GLuint g_quad_vbo = 0;
static GLuint g_menu_shader = 0;

static float g_animation_timer = 0.0f;
static float g_title_y_offset_base = 0.0f;
static float g_title_current_y_offset = 0.0f;
static float g_button_hover_offset = 0.0f;

static bool g_is_in_game_menu = false;
static bool g_is_map_loaded = false;

static int find_int_in_array(int value, const int* arr, int arr_size) {
    for (int i = 0; i < arr_size; ++i) {
        if (arr[i] == value) {
            return i;
        }
    }
    return -1;
}

static void MainMenu_RenderOptionsMenu() {
    UI_SetNextWindowSize(500, 400);
    if (UI_Begin("Options", &g_show_options_menu)) {

        if (UI_BeginTabBar("OptionsTabs", 0)) {
            if (UI_BeginTabItem("Gameplay")) {
                float fov = Cvar_GetFloat("fov_vertical");
                if (UI_DragFloat("Field of View", &fov, 1.0f, 55.0f, 110.0f)) {
                    char value_str[16];
                    sprintf(value_str, "%.0f", fov);
                    Cvar_Set("fov_vertical", value_str);
                }

                bool crosshair = Cvar_GetInt("crosshair");
                if (UI_Checkbox("Show Crosshair", &crosshair)) {
                    Cvar_Set("crosshair", crosshair ? "1" : "0");
                }
                UI_EndTabItem();
            }

            if (UI_BeginTabItem("Graphics")) {
                UI_Text("Display");
                UI_Separator();

                const char* quality_levels[] = { "Very Low", "Low", "Medium", "High", "Very High" };
                int current_quality = Cvar_GetInt("r_texture_quality") - 1;
                if (UI_Combo("Texture Quality", &current_quality, quality_levels, 5, -1)) {
                    char value_str[4];
                    sprintf(value_str, "%d", current_quality + 1);
                    Cvar_Set("r_texture_quality", value_str);
                }

                const char* fps_options[] = { "30", "60", "120", "144", "240", "Unlimited" };
                const int fps_values[] = { 30, 60, 120, 144, 240, 0 };
                int current_fps_limit = Cvar_GetInt("fps_max");
                int selection = find_int_in_array(current_fps_limit, fps_values, 6);
                if (selection == -1) selection = 5;

                if (UI_Combo("Max FPS", &selection, fps_options, 6, -1)) {
                    char value_str[8];
                    sprintf(value_str, "%d", fps_values[selection]);
                    Cvar_Set("fps_max", value_str);
                }

                bool vsync = Cvar_GetInt("r_vsync");
                if (UI_Checkbox("V-Sync", &vsync)) {
                    Cvar_Set("r_vsync", vsync ? "1" : "0");
                }

                UI_Spacing();
                UI_Text("Effects");
                UI_Separator();

                bool fxaa = Cvar_GetInt("r_fxaa");
                if (UI_Checkbox("FXAA", &fxaa)) {
                    Cvar_Set("r_fxaa", fxaa ? "1" : "0");
                }

                bool bloom = Cvar_GetInt("r_bloom");
                if (UI_Checkbox("Bloom", &bloom)) {
                    Cvar_Set("r_bloom", bloom ? "1" : "0");
                }

                bool ssao = Cvar_GetInt("r_ssao");
                if (UI_Checkbox("SSAO", &ssao)) {
                    Cvar_Set("r_ssao", ssao ? "1" : "0");
                }

                bool volumetrics = Cvar_GetInt("r_volumetrics");
                if (UI_Checkbox("Volumetric Lighting", &volumetrics)) {
                    Cvar_Set("r_volumetrics", volumetrics ? "1" : "0");
                }

                bool relief = Cvar_GetInt("r_relief_mapping");
                if (UI_Checkbox("Relief Mapping", &relief)) {
                    Cvar_Set("r_relief_mapping", relief ? "1" : "0");
                }

                bool motionblur = Cvar_GetInt("r_motionblur");
                if (UI_Checkbox("Motion Blur", &motionblur)) {
                    Cvar_Set("r_motionblur", motionblur ? "1" : "0");
                }

                bool dof = Cvar_GetInt("r_dof");
                if (UI_Checkbox("Depth of Field", &dof)) {
                    Cvar_Set("r_dof", dof ? "1" : "0");
                }
                UI_EndTabItem();
            }

            if (UI_BeginTabItem("Audio")) {
                float volume = Cvar_GetFloat("volume");
                if (UI_DragFloat("Master Volume", &volume, 0.01f, 0.0f, 4.0f)) {
                    char value_str[16];
                    sprintf(value_str, "%.2f", volume);
                    Cvar_Set("volume", value_str);
                }
                UI_EndTabItem();
            }

            if (UI_BeginTabItem("Controls")) {
                float sensitivity = Cvar_GetFloat("sensitivity");
                if (UI_DragFloat("Mouse Sensitivity", &sensitivity, 0.01f, 0.1f, 10.0f)) {
                    char value_str[16];
                    sprintf(value_str, "%.2f", sensitivity);
                    Cvar_Set("sensitivity", value_str);
                }
                UI_EndTabItem();
            }
            UI_EndTabBar();
        }

        UI_Separator();
        float button_width = 80.0f;
        float window_width = UI_GetWindowWidth();
        UI_SetCursorPosX(window_width - button_width - 15.0f);
        if (UI_Button("Close")) {
            g_show_options_menu = false;
        }
    }
    UI_End();
}

static GLuint create_text_texture(TTF_Font* font, const char* text, SDL_Color color, int* width_out, int* height_out) {
    if (!font || !text) return 0;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) {
        Console_Printf_Error("MainMenu ERROR: TTF_RenderText_Blended failed: %s", TTF_GetError());
        return 0;
    }
    SDL_Surface* formatted_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (!formatted_surface) {
        Console_Printf_Error("MainMenu ERROR: SDL_ConvertSurfaceFormat failed: %s", SDL_GetError());
        return 0;
    }
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, formatted_surface->w, formatted_surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, formatted_surface->pixels);
    if (width_out) *width_out = formatted_surface->w;
    if (height_out) *height_out = formatted_surface->h;
    SDL_FreeSurface(formatted_surface);
    return texture_id;
}

static void render_textured_quad(GLuint texture, float x, float y, float w, float h, SDL_Color color, float current_offset_x) {
    glUseProgram(g_menu_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(g_menu_shader, "u_texture"), 0);
    glUniform4f(glGetUniformLocation(g_menu_shader, "u_color_tint"), (float)color.r / 255.0f, (float)color.g / 255.0f, (float)color.b / 255.0f, (float)color.a / 255.0f);

    float vertices[] = {
        x + current_offset_x, y + h,  0.0f, 1.0f,
        x + current_offset_x, y,      0.0f, 0.0f,
        x + w + current_offset_x, y,  1.0f, 0.0f,

        x + current_offset_x, y + h,  0.0f, 1.0f,
        x + w + current_offset_x, y,  1.0f, 0.0f,
        x + w + current_offset_x, y + h,  1.0f, 1.0f
    };

    glBindVertexArray(g_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

bool MainMenu_Init(int screen_width, int screen_height) {
    g_screen_width = screen_width;
    g_screen_height = screen_height;
    if (TTF_Init() == -1) {
        Console_Printf_Error("MainMenu ERROR: TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    g_menu_font = TTF_OpenFont("fonts/Roboto-Regular.ttf", 64);
    if (!g_menu_font) {
        Console_Printf_Error("MainMenu ERROR: Failed to load font: %s", TTF_GetError());
        return false;
    }
    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Color title_color = { 255, 255, 0, 255 };
    const GameConfig* config = GameConfig_Get();
    const char* game_name = config->gamename[0] != '\0' ? config->gamename : "Tectonic Engine";
    g_text_texture_game_title = create_text_texture(g_menu_font, game_name, title_color, &g_text_width_game_title, &g_text_height_game_title);
    g_text_texture_options = create_text_texture(g_menu_font, "OPTIONS", white, &g_text_width_options, &g_text_height_options);
    g_text_texture_quit = create_text_texture(g_menu_font, "QUIT", white, &g_text_width_quit, &g_text_height_quit);
    g_text_texture_start = create_text_texture(g_menu_font, "START GAME", white, &g_text_width_start, &g_text_height_start);
    if (!g_text_texture_game_title || !g_text_texture_options || !g_text_texture_quit || !g_text_texture_start) {
        Console_Printf_Error("MainMenu ERROR: Failed to create one or more text textures.");
        MainMenu_Shutdown();
        return false;
    }

    g_menu_shader = createShaderProgram("shaders/menu.vert", "shaders/menu.frag");
    if (g_menu_shader == 0) {
        Console_Printf_Error("MainMenu ERROR: Failed to create menu shader program.");
        return false;
    }

    glGenVertexArrays(1, &g_quad_vao);
    glGenBuffers(1, &g_quad_vbo);
    glBindVertexArray(g_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    Console_Printf("Main Menu Initialized.");
    return true;
}

void MainMenu_Shutdown() {
    if (g_text_texture_start) glDeleteTextures(1, &g_text_texture_start);
    if (g_text_texture_options) glDeleteTextures(1, &g_text_texture_options);
    if (g_text_texture_quit) glDeleteTextures(1, &g_text_texture_quit);
    if (g_text_texture_game_title) glDeleteTextures(1, &g_text_texture_game_title);
    if (g_quad_vao) glDeleteVertexArrays(1, &g_quad_vao);
    if (g_quad_vbo) glDeleteBuffers(1, &g_quad_vbo);
    if (g_menu_shader) glDeleteProgram(g_menu_shader);
    if (g_menu_font) TTF_CloseFont(g_menu_font);
    TTF_Quit();
    Console_Printf("Main Menu Shutdown.");
}

void MainMenu_SetInGameMenuMode(bool is_in_game, bool is_map_loaded_state) {
    g_is_in_game_menu = is_in_game;
    g_is_map_loaded = is_map_loaded_state;
    SDL_Color white = { 255, 255, 255, 255 };

    if (g_text_texture_start) {
        glDeleteTextures(1, &g_text_texture_start);
    }

    if (g_is_in_game_menu && g_is_map_loaded) {
        g_text_texture_start = create_text_texture(g_menu_font, "CONTINUE", white, &g_text_width_start, &g_text_height_start);
    }
    else {
        g_text_texture_start = create_text_texture(g_menu_font, "START GAME", white, &g_text_width_start, &g_text_height_start);
    }
    if (!g_text_texture_start) {
        Console_Printf_Error("MainMenu ERROR: Failed to update start/continue texture.");
    }
    g_selected_button_index = 0;
}

MainMenuAction MainMenu_HandleEvent(SDL_Event* event) {
    if (g_show_options_menu && (UI_WantCaptureMouse() || UI_WantCaptureKeyboard())) {
        return MAINMENU_ACTION_NONE;
    }
    if (event->type == SDL_MOUSEMOTION) {
        int mouseX = event->motion.x;
        int mouseY = event->motion.y;
        float button_spacing = 60.0f;
        float button_y_start = g_screen_height / 2.0f - (g_is_in_game_menu ? 80.0f : 20.0f);
        float start_x = (g_screen_width - g_text_width_start) / 2.0f;
        float start_y = button_y_start;
        if (mouseX >= start_x && mouseX <= start_x + g_text_width_start &&
            mouseY >= start_y && mouseY <= start_y + g_text_height_start) {
            g_selected_button_index = 0;
        }
        float options_x = (g_screen_width - g_text_width_options) / 2.0f;
        float options_y = button_y_start + g_text_height_start + button_spacing;
        if (mouseX >= options_x && mouseX <= options_x + g_text_width_options &&
            mouseY >= options_y && mouseY <= options_y + g_text_height_options) {
            g_selected_button_index = 1;
        }
        float quit_x = (g_screen_width - g_text_width_quit) / 2.0f;
        float quit_y = button_y_start + g_text_height_start * 2 + button_spacing * 2;
        if (mouseX >= quit_x && mouseX <= quit_x + g_text_width_quit &&
            mouseY >= quit_y && mouseY <= quit_y + g_text_height_quit) {
            g_selected_button_index = 2;
        }
    }
    else if (event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            if (g_selected_button_index == 0) {
                if (g_is_in_game_menu && g_is_map_loaded) return MAINMENU_ACTION_CONTINUE_GAME;
                else return MAINMENU_ACTION_START_GAME;
            }
            if (g_selected_button_index == 1) {
                g_show_options_menu = true;
                return MAINMENU_ACTION_NONE;
            }
            if (g_selected_button_index == 2) return MAINMENU_ACTION_QUIT;
        }
    }
    else if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_UP) {
            g_selected_button_index = (g_selected_button_index - 1 + g_num_buttons) % g_num_buttons;
        }
        else if (event->key.keysym.sym == SDLK_DOWN) {
            g_selected_button_index = (g_selected_button_index + 1) % g_num_buttons;
        }
        else if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER) {
            if (g_selected_button_index == 0) {
                if (g_is_in_game_menu && g_is_map_loaded) return MAINMENU_ACTION_CONTINUE_GAME;
                else return MAINMENU_ACTION_START_GAME;
            }
            if (g_selected_button_index == 1) {
                g_show_options_menu = true;
                return MAINMENU_ACTION_NONE;
            }
            if (g_selected_button_index == 2) return MAINMENU_ACTION_QUIT;
        }
    }
    return MAINMENU_ACTION_NONE;
}

void MainMenu_Update(float deltaTime) {
    g_animation_timer += deltaTime;
    g_button_hover_offset = sinf(g_animation_timer * 4.0f) * 10.0f;
    g_title_current_y_offset = g_title_y_offset_base + sinf(g_animation_timer * 2.0f) * 5.0f;
}

void MainMenu_Render() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Mat4 projection_matrix = mat4_ortho(0.0f, (float)g_screen_width, (float)g_screen_height, 0.0f, -1.0f, 1.0f);

    glUseProgram(g_menu_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_menu_shader, "projection"), 1, GL_FALSE, projection_matrix.m);

    if (!g_is_in_game_menu) {
        float title_x = (g_screen_width - g_text_width_game_title) / 2.0f;
        g_title_y_offset_base = g_screen_height / 2.0f - g_text_height_game_title * 2.5f;
        render_textured_quad(g_text_texture_game_title, title_x, g_title_current_y_offset,
            g_text_width_game_title, g_text_height_game_title,
            (SDL_Color) {
            255, 255, 0, 255
        }, 0.0f);
    }

    float button_spacing = 60.0f;
    float button_y_start = g_screen_height / 2.0f - (g_is_in_game_menu ? 80.0f : 20.0f);

    SDL_Color normal_color = { 255, 255, 255, 255 };
    SDL_Color hover_color = { 255, 255, 0, 255 };

    float current_button_offset_x = (g_selected_button_index == 0) ? g_button_hover_offset : 0.0f;
    SDL_Color current_button_color = (g_selected_button_index == 0) ? hover_color : normal_color;
    render_textured_quad(g_text_texture_start, (g_screen_width - g_text_width_start) / 2.0f,
        button_y_start, g_text_width_start, g_text_height_start,
        current_button_color, current_button_offset_x);

    current_button_offset_x = (g_selected_button_index == 1) ? g_button_hover_offset : 0.0f;
    current_button_color = (g_selected_button_index == 1) ? hover_color : normal_color;
    render_textured_quad(g_text_texture_options, (g_screen_width - g_text_width_options) / 2.0f,
        button_y_start + g_text_height_start + button_spacing,
        g_text_width_options, g_text_height_options,
        current_button_color, current_button_offset_x);

    current_button_offset_x = (g_selected_button_index == 2) ? g_button_hover_offset : 0.0f;
    current_button_color = (g_selected_button_index == 2) ? hover_color : normal_color;
    render_textured_quad(g_text_texture_quit, (g_screen_width - g_text_width_quit) / 2.0f,
        button_y_start + g_text_height_start * 2 + button_spacing * 2,
        g_text_width_quit, g_text_height_quit,
        current_button_color, current_button_offset_x);

    if (g_show_options_menu) {
        MainMenu_RenderOptionsMenu();
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}