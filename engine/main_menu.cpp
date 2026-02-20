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
#include "main_menu.h"
#include "engine_commands.h"
#include "gl_video_player.h"
#include <SDL_ttf.h>
#include <GL/glew.h>
#include <stdio.h>
#include <string.h>
#include "gameconfig.h"
#include "gl_console.h"
#include "math_lib.h"
#include "gl_misc.h"
#include "io_system.h"
#include "cvar.h"

static VideoPlayer g_background_video;
static Bool g_has_background_video = false;
Bool g_show_options_menu = false;
Bool g_show_load_game_menu = false;
Bool g_show_save_game_menu = false;

static Char** g_save_game_files = nullptr;
static Int g_num_save_games = 0;
static Int g_selected_save_index = -1;

static TTF_Font* g_menu_font = nullptr;
static GLuint g_text_texture_start = 0;
static GLuint g_text_texture_load = 0;
static GLuint g_text_texture_save = 0;
static GLuint g_text_texture_options = 0;
static GLuint g_text_texture_quit = 0;
static GLuint g_text_texture_game_title = 0;

static Int g_text_width_start, g_text_height_start;
static Int g_text_width_load, g_text_height_load;
static Int g_text_width_save, g_text_height_save;
static Int g_text_width_options, g_text_height_options;
static Int g_text_width_quit, g_text_height_quit;
static Int g_text_width_game_title, g_text_height_game_title;

static Int g_selected_button_index = 0;
static Int g_num_buttons = 4;

static Int g_screen_width = 0;
static Int g_screen_height = 0;

static GLuint g_quad_vao = 0;
static GLuint g_quad_vbo = 0;
static GLuint g_menu_shader = 0;

static Float g_animation_timer = 0.0f;
static Float g_title_y_offset_base = 0.0f;
static Float g_title_current_y_offset = 0.0f;
static Float g_button_hover_offset = 0.0f;

static Bool g_is_in_game_menu = false;
static Bool g_is_map_loaded = false;

static Int find_int_in_array(Int value, const Int* arr, Int arr_size) {
    for (Int i = 0; i < arr_size; ++i) {
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
                Float fov = Cvar_GetFloat("fov_vertical");
                if (UI_DragFloat("Field of View", &fov, 1.0f, 55.0f, 110.0f)) {
                    Char value_str[16];
                    snprintf(value_str, sizeof(value_str), "%.0f", fov);
                    Cvar_Set("fov_vertical", value_str);
                }
                Bool crosshair = Cvar_GetInt("crosshair");
                if (UI_Checkbox("Show Crosshair", &crosshair)) {
                    Cvar_Set("crosshair", crosshair ? "1" : "0");
                }
                UI_EndTabItem();
            }
            if (UI_BeginTabItem("Graphics")) {
                UI_Text("Display");
                UI_Separator();
                const Char* quality_levels[] = { "Very Low", "Low", "Medium", "High", "Very High" };
                Int current_quality = Cvar_GetInt("r_texture_quality") - 1;
                if (UI_Combo("Texture Quality", &current_quality, quality_levels, 5, -1)) {
                    Char value_str[4];
                    snprintf(value_str, sizeof(value_str), "%d", current_quality + 1);
                    Cvar_Set("r_texture_quality", value_str);
                }
                const Char* fps_options[] = { "30", "60", "120", "144", "240", "Unlimited" };
                const Int fps_values[] = { 30, 60, 120, 144, 240, 0 };
                Int current_fps_limit = Cvar_GetInt("fps_max");
                Int selection = find_int_in_array(current_fps_limit, fps_values, 6);
                if (selection == -1) selection = 5;

                if (UI_Combo("Max FPS", &selection, fps_options, 6, -1)) {
                    Char value_str[8];
                    snprintf(value_str, sizeof(value_str), "%d", fps_values[selection]);
                    Cvar_Set("fps_max", value_str);
                }

                UI_Spacing();
                Int num_displays = SDL_GetNumVideoDisplays();
                const Char** display_names = new const Char * [num_displays];
                for (Int i = 0; i < num_displays; ++i) {
                    display_names[i] = SDL_GetDisplayName(i);
                }

                Int current_monitor = Cvar_GetInt("r_monitor");
                if (UI_Combo("Monitor", &current_monitor, display_names, num_displays, -1)) {
                    if (current_monitor >= 0 && current_monitor < num_displays) {
                        Char value_str[4];
                        snprintf(value_str, sizeof(value_str), "%d", current_monitor);
                        Cvar_Set("r_monitor", value_str);
                    }
                }
                delete[] display_names;

                Bool vsync = Cvar_GetInt("r_vsync");
                if (UI_Checkbox("V-Sync", &vsync)) {
                    Cvar_Set("r_vsync", vsync ? "1" : "0");
                }

                UI_Spacing();
                UI_Text("Effects");
                UI_Separator();
                Bool fxaa = Cvar_GetInt("r_fxaa");
                if (UI_Checkbox("FXAA", &fxaa)) { Cvar_Set("r_fxaa", fxaa ? "1" : "0"); }
                Bool bloom = Cvar_GetInt("r_bloom");
                if (UI_Checkbox("Bloom", &bloom)) { Cvar_Set("r_bloom", bloom ? "1" : "0"); }
                Bool ssao = Cvar_GetInt("r_ssao");
                if (UI_Checkbox("SSAO", &ssao)) { Cvar_Set("r_ssao", ssao ? "1" : "0"); }
                Bool volumetrics = Cvar_GetInt("r_volumetrics");
                if (UI_Checkbox("Volumetric Lighting", &volumetrics)) { Cvar_Set("r_volumetrics", volumetrics ? "1" : "0"); }
                Bool relief = Cvar_GetInt("r_relief_mapping");
                if (UI_Checkbox("Relief Mapping", &relief)) { Cvar_Set("r_relief_mapping", relief ? "1" : "0"); }
                UI_EndTabItem();
            }
            if (UI_BeginTabItem("Audio")) {
                Float volume = Cvar_GetFloat("volume");
                if (UI_DragFloat("Master Volume", &volume, 0.01f, 0.0f, 4.0f)) {
                    Char value_str[16];
                    snprintf(value_str, sizeof(value_str), "%.2f", volume);
                    Cvar_Set("volume", value_str);
                }
                UI_EndTabItem();
            }
            if (UI_BeginTabItem("Controls")) {
                Float sensitivity = Cvar_GetFloat("sensitivity");
                if (UI_DragFloat("Mouse Sensitivity", &sensitivity, 0.01f, 0.1f, 10.0f)) {
                    Char value_str[16];
                    snprintf(value_str, sizeof(value_str), "%.2f", sensitivity);
                    Cvar_Set("sensitivity", value_str);
                }
                Bool raw_input = Cvar_GetInt("in_rawinput");
                if (UI_Checkbox("Raw Mouse Input", &raw_input)) {
                    Cvar_Set("in_rawinput", raw_input ? "1" : "0");
                }
                UI_EndTabItem();
            }
            UI_EndTabBar();
        }
        UI_Separator();
        constexpr Float button_width = 80.0f;
        Float window_width = UI_GetWindowWidth();
        UI_SetCursorPosX(window_width - button_width - 15.0f);
        if (UI_Button("Close")) {
            g_show_options_menu = false;
        }
    }
    UI_End();
}

static void ScanSaveGames() {
    if (g_save_game_files) {
        for (Int i = 0; i < g_num_save_games; ++i) {
            delete[] g_save_game_files[i];
        }
        delete[] g_save_game_files;
        g_save_game_files = nullptr;
    }
    g_num_save_games = 0;
    g_selected_save_index = -1;

    const Char* exts[] = { ".sav" };
    g_save_game_files = IO_ScanDirectory("saves/", exts, 1, &g_num_save_games);

    for (Int i = 0; i < g_num_save_games; ++i) {
        Char* dot = strrchr(g_save_game_files[i], '.');
        if (dot) *dot = '\0';
    }
}

static void MainMenu_RenderLoadGameWindow() {
    if (!g_show_load_game_menu) return;
    UI_SetNextWindowSize(400, 300);
    if (UI_Begin("Load Game", &g_show_load_game_menu)) {
        if (UI_Button("Refresh")) ScanSaveGames();
        UI_Separator();
        if (UI_BeginChild("save_list", 0, -40, false, 0)) {
            for (Int i = 0; i < g_num_save_games; ++i) {
                if (UI_Selectable(g_save_game_files[i], g_selected_save_index == i)) {
                    g_selected_save_index = i;
                }
            }
        }
        UI_EndChild();
        UI_Separator();
        UI_BeginDisabled(g_selected_save_index == -1);
        if (UI_Button("Load")) {
            Char* argv[] = { (Char*)"load", g_save_game_files[g_selected_save_index] };
            Cmd_LoadGame(2, argv);
            g_show_load_game_menu = false;
        }
        UI_EndDisabled();
    }
    UI_End();
}

static void MainMenu_RenderSaveGameWindow() {
    if (!g_show_save_game_menu) return;
    UI_SetNextWindowSize(300, 100);
    if (UI_Begin("Save Game", &g_show_save_game_menu)) {
        static Char save_name[64] = "MySave";
        UI_InputText("Save Name", save_name, sizeof(save_name));
        if (UI_Button("Save")) {
            Char* argv[] = { (Char*)"save", save_name };
            Cmd_SaveGame(2, argv);
            g_show_save_game_menu = false;
        }
    }
    UI_End();
}

static GLuint create_text_texture(TTF_Font* font, const Char* text, SDL_Color color, Int* width_out, Int* height_out) {
    if (!font || !text) return 0;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return 0;
    SDL_Surface* formatted_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (!formatted_surface) return 0;
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

static void render_textured_quad(GLuint texture, Float x, Float y, Float w, Float h, SDL_Color color, Float current_offset_x) {
    glUseProgram(g_menu_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    Shader_Set(g_menu_shader, "u_texture", 0);
    Shader_Set(g_menu_shader, "u_color_tint", Vec4{ (Float)color.r / 255.0f, (Float)color.g / 255.0f, (Float)color.b / 255.0f, (Float)color.a / 255.0f });

    Float vertices[] = {
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

Bool MainMenu_Init(Int screen_width, Int screen_height) {
    g_screen_width = screen_width;
    g_screen_height = screen_height;
    if (TTF_Init() == -1) return false;
    g_menu_font = TTF_OpenFont("fonts/Roboto-Regular.ttf", 64);
    if (!g_menu_font) return false;
    VideoPlayer_InitSystem();
    memset(&g_background_video, 0, sizeof(VideoPlayer));
    strcpy(g_background_video.videoPath, "media/menu.mpg");
    VideoPlayer_Load(&g_background_video);
    if (g_background_video.plm) {
        g_has_background_video = true;
        g_background_video.loop = true;
        VideoPlayer_Play(&g_background_video);
    }
    constexpr SDL_Color white = { 255, 255, 255, 255 };
    constexpr SDL_Color title_color = { 255, 255, 0, 255 };
    const GameConfig* config = GameConfig_Get();
    const Char* game_name = config->gamename[0] != '\0' ? config->gamename : "Tectonic Engine";
    g_text_texture_game_title = create_text_texture(g_menu_font, game_name, title_color, &g_text_width_game_title, &g_text_height_game_title);
    g_text_texture_start = create_text_texture(g_menu_font, "START GAME", white, &g_text_width_start, &g_text_height_start);
    g_text_texture_load = create_text_texture(g_menu_font, "LOAD GAME", white, &g_text_width_load, &g_text_height_load);
    g_text_texture_save = create_text_texture(g_menu_font, "SAVE GAME", white, &g_text_width_save, &g_text_height_save);
    g_text_texture_options = create_text_texture(g_menu_font, "OPTIONS", white, &g_text_width_options, &g_text_height_options);
    g_text_texture_quit = create_text_texture(g_menu_font, "QUIT", white, &g_text_width_quit, &g_text_height_quit);

    g_menu_shader = createShaderProgram("shaders/menu.vert", "shaders/menu.frag");
    if (g_menu_shader == 0) return false;

    glGenVertexArrays(1, &g_quad_vao);
    glGenBuffers(1, &g_quad_vbo);
    glBindVertexArray(g_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(Float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(Float), (void*)(2 * sizeof(Float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return true;
}

void MainMenu_Shutdown() {
    if (g_has_background_video) VideoPlayer_Free(&g_background_video);
    VideoPlayer_ShutdownSystem();
    if (g_text_texture_start) glDeleteTextures(1, &g_text_texture_start);
    if (g_text_texture_load) glDeleteTextures(1, &g_text_texture_load);
    if (g_text_texture_save) glDeleteTextures(1, &g_text_texture_save);
    if (g_text_texture_options) glDeleteTextures(1, &g_text_texture_options);
    if (g_text_texture_quit) glDeleteTextures(1, &g_text_texture_quit);
    if (g_text_texture_game_title) glDeleteTextures(1, &g_text_texture_game_title);
    if (g_quad_vao) glDeleteVertexArrays(1, &g_quad_vao);
    if (g_quad_vbo) glDeleteBuffers(1, &g_quad_vbo);
    if (g_menu_shader) glDeleteProgram(g_menu_shader);
    if (g_menu_font) TTF_CloseFont(g_menu_font);
    TTF_Quit();
}

void MainMenu_SetInGameMenuMode(Bool is_in_game, Bool is_map_loaded_state) {
    g_is_in_game_menu = is_in_game;
    g_is_map_loaded = is_map_loaded_state;
    constexpr SDL_Color white = { 255, 255, 255, 255 };

    if (g_text_texture_start) glDeleteTextures(1, &g_text_texture_start);

    if (g_is_in_game_menu && g_is_map_loaded) {
        g_text_texture_start = create_text_texture(g_menu_font, "CONTINUE", white, &g_text_width_start, &g_text_height_start);
        g_num_buttons = 5;
    }
    else {
        g_text_texture_start = create_text_texture(g_menu_font, "START GAME", white, &g_text_width_start, &g_text_height_start);
        g_num_buttons = 4;
    }
    g_selected_button_index = 0;
}

MainMenuAction MainMenu_HandleEvent(SDL_Event* event) {
    if (g_show_options_menu || g_show_load_game_menu || g_show_save_game_menu) {
        if (UI_WantCaptureMouse() || UI_WantCaptureKeyboard()) {
            return MAINMENU_ACTION_NONE;
        }
    }
    if (event->type == SDL_MOUSEMOTION) {
        Int mouseX = event->motion.x;
        Int mouseY = event->motion.y;
        constexpr Float button_spacing = 60.0f;
        Float button_y_start = g_screen_height / 2.0f - (g_is_in_game_menu ? 120.0f : 20.0f);

        Float start_x = (g_screen_width - g_text_width_start) / 2.0f;
        Float start_y = button_y_start;
        if (mouseX >= start_x && mouseX <= start_x + g_text_width_start && mouseY >= start_y && mouseY <= start_y + g_text_height_start) g_selected_button_index = 0;

        Float load_x = (g_screen_width - g_text_width_load) / 2.0f;
        Float load_y = button_y_start + g_text_height_start + button_spacing;
        if (mouseX >= load_x && mouseX <= load_x + g_text_width_load && mouseY >= load_y && mouseY <= load_y + g_text_height_load) g_selected_button_index = 1;

        Float options_x = (g_screen_width - g_text_width_options) / 2.0f;
        Float options_y = button_y_start + (g_text_height_start + button_spacing) * 2;
        if (mouseX >= options_x && mouseX <= options_x + g_text_width_options && mouseY >= options_y && mouseY <= options_y + g_text_height_options) g_selected_button_index = 2;

        if (g_is_in_game_menu) {
            Float save_x = (g_screen_width - g_text_width_save) / 2.0f;
            Float save_y = button_y_start + (g_text_height_start + button_spacing) * 3;
            if (mouseX >= save_x && mouseX <= save_x + g_text_width_save && mouseY >= save_y && mouseY <= save_y + g_text_height_save) g_selected_button_index = 3;

            Float quit_x = (g_screen_width - g_text_width_quit) / 2.0f;
            Float quit_y = button_y_start + (g_text_height_start + button_spacing) * 4;
            if (mouseX >= quit_x && mouseX <= quit_x + g_text_width_quit && mouseY >= quit_y && mouseY <= quit_y + g_text_height_quit) g_selected_button_index = 4;
        }
        else {
            Float quit_x = (g_screen_width - g_text_width_quit) / 2.0f;
            Float quit_y = button_y_start + (g_text_height_start + button_spacing) * 3;
            if (mouseX >= quit_x && mouseX <= quit_x + g_text_width_quit && mouseY >= quit_y && mouseY <= quit_y + g_text_height_quit) g_selected_button_index = 3;
        }
    }
    else if (event->type == SDL_MOUSEBUTTONDOWN || (event->type == SDL_KEYDOWN && (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_KP_ENTER))) {
        if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button != SDL_BUTTON_LEFT) return MAINMENU_ACTION_NONE;

        switch (g_selected_button_index) {
        case 0: return (g_is_in_game_menu && g_is_map_loaded) ? MAINMENU_ACTION_CONTINUE_GAME : MAINMENU_ACTION_START_GAME;
        case 1: g_show_load_game_menu = true; ScanSaveGames(); return MAINMENU_ACTION_NONE;
        case 2: g_show_options_menu = true; return MAINMENU_ACTION_NONE;
        case 3:
            if (g_is_in_game_menu) { g_show_save_game_menu = true; return MAINMENU_ACTION_NONE; }
            else { return MAINMENU_ACTION_QUIT; }
        case 4: if (g_is_in_game_menu) return MAINMENU_ACTION_QUIT;
        }
    }
    else if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_UP) {
            g_selected_button_index = (g_selected_button_index - 1 + g_num_buttons) % g_num_buttons;
        }
        else if (event->key.keysym.sym == SDLK_DOWN) {
            g_selected_button_index = (g_selected_button_index + 1) % g_num_buttons;
        }
    }
    return MAINMENU_ACTION_NONE;
}

void MainMenu_Update(Float deltaTime) {
    if (g_has_background_video) VideoPlayer_Update(&g_background_video, deltaTime);
    g_animation_timer += deltaTime;
    g_button_hover_offset = sinf(g_animation_timer * 4.0f) * 10.0f;
    g_title_current_y_offset = g_title_y_offset_base + sinf(g_animation_timer * 2.0f) * 5.0f;
}

void MainMenu_Render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Mat4 projection_matrix = Math::mat4_ortho(0.0f, (Float)g_screen_width, (Float)g_screen_height, 0.0f, -1.0f, 1.0f);
    glUseProgram(g_menu_shader);
    Shader_Set(g_menu_shader, "projection", &projection_matrix);

    if (g_has_background_video) {
        VideoPlayer_Render2D(&g_background_video, 0, 0, (Float)g_screen_width, (Float)g_screen_height, g_screen_width, g_screen_height);
    }

    if (!g_is_in_game_menu) {
        Float title_x = (g_screen_width - g_text_width_game_title) / 2.0f;
        g_title_y_offset_base = g_screen_height / 2.0f - g_text_height_game_title * 2.5f;
        render_textured_quad(g_text_texture_game_title, title_x, g_title_current_y_offset,
            g_text_width_game_title, g_text_height_game_title,
            SDL_Color{
            255, 255, 0, 255
        }, 0.0f);
    }

    constexpr Float button_spacing = 60.0f;
    Float button_y_start = g_screen_height / 2.0f - (g_is_in_game_menu ? 120.0f : 20.0f);

    constexpr SDL_Color normal_color = { 255, 255, 255, 255 };
    constexpr SDL_Color hover_color = { 255, 255, 0, 255 };

    Float current_button_offset_x = (g_selected_button_index == 0) ? g_button_hover_offset : 0.0f;
    SDL_Color current_button_color = (g_selected_button_index == 0) ? hover_color : normal_color;
    render_textured_quad(g_text_texture_start, (g_screen_width - g_text_width_start) / 2.0f,
        button_y_start, g_text_width_start, g_text_height_start,
        current_button_color, current_button_offset_x);

    current_button_offset_x = (g_selected_button_index == 1) ? g_button_hover_offset : 0.0f;
    current_button_color = (g_selected_button_index == 1) ? hover_color : normal_color;
    render_textured_quad(g_text_texture_load, (g_screen_width - g_text_width_load) / 2.0f,
        button_y_start + g_text_height_start + button_spacing,
        g_text_width_load, g_text_height_load,
        current_button_color, current_button_offset_x);

    current_button_offset_x = (g_selected_button_index == 2) ? g_button_hover_offset : 0.0f;
    current_button_color = (g_selected_button_index == 2) ? hover_color : normal_color;
    render_textured_quad(g_text_texture_options, (g_screen_width - g_text_width_options) / 2.0f,
        button_y_start + (g_text_height_start + button_spacing) * 2,
        g_text_width_options, g_text_height_options,
        current_button_color, current_button_offset_x);

    Int quit_button_index = 3;
    Float current_y = button_y_start + (g_text_height_start + button_spacing) * 3;

    if (g_is_in_game_menu) {
        current_button_offset_x = (g_selected_button_index == 3) ? g_button_hover_offset : 0.0f;
        current_button_color = (g_selected_button_index == 3) ? hover_color : normal_color;
        render_textured_quad(g_text_texture_save, (g_screen_width - g_text_width_save) / 2.0f,
            current_y, g_text_width_save, g_text_height_save,
            current_button_color, current_button_offset_x);
        quit_button_index = 4;
        current_y += g_text_height_start + button_spacing;
    }

    current_button_offset_x = (g_selected_button_index == quit_button_index) ? g_button_hover_offset : 0.0f;
    current_button_color = (g_selected_button_index == quit_button_index) ? hover_color : normal_color;
    render_textured_quad(g_text_texture_quit, (g_screen_width - g_text_width_quit) / 2.0f,
        current_y, g_text_width_quit, g_text_height_quit,
        current_button_color, current_button_offset_x);

    if (g_show_options_menu) MainMenu_RenderOptionsMenu();
    if (g_show_load_game_menu) MainMenu_RenderLoadGameWindow();
    if (g_show_save_game_menu) MainMenu_RenderSaveGameWindow();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}