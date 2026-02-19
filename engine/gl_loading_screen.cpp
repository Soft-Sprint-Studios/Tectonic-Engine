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
#include "gl_loading_screen.h"
#include "gl_misc.h"
#include <SDL_ttf.h>
#include <SDL_image.h>
#include "gl_console.h"

static Bool g_is_active = false;
static GLuint g_background_texture = 0;
static GLuint g_text_texture = 0;
static Int g_text_width = 0, g_text_height = 0;
static GLuint g_quad_vao = 0, g_quad_vbo = 0;
static GLuint g_shader = 0;
static Int g_screen_width = 0, g_screen_height = 0;
static TTF_Font* g_font = nullptr;

static GLuint create_loading_text_texture(TTF_Font* font, const Char* text, SDL_Color color, Int* width, Int* height) {
    if (!font || !text) return 0;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return 0;
    SDL_Surface* formatted_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (!formatted_surface) return 0;

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, formatted_surface->w, formatted_surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, formatted_surface->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (width) *width = formatted_surface->w;
    if (height) *height = formatted_surface->h;

    SDL_FreeSurface(formatted_surface);
    return texture_id;
}

void LoadingScreen_Init(Int screen_width, Int screen_height) {
    g_screen_width = screen_width;
    g_screen_height = screen_height;

    g_font = TTF_OpenFont("fonts/Roboto-Regular.ttf", 32);
    if (!g_font) {
        Console_Printf_Error("Failed to load font for loading screen.");
        return;
    }

    constexpr SDL_Color white = { 255, 255, 255, 255 };
    g_text_texture = create_loading_text_texture(g_font, "Loading...", white, &g_text_width, &g_text_height);
    
    g_shader = createShaderProgram("shaders/menu.vert", "shaders/menu.frag");

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
}

void LoadingScreen_Shutdown() {
    if (g_text_texture) glDeleteTextures(1, &g_text_texture);
    if (g_background_texture) glDeleteTextures(1, &g_background_texture);
    if (g_shader) glDeleteProgram(g_shader);
    if (g_quad_vao) glDeleteVertexArrays(1, &g_quad_vao);
    if (g_quad_vbo) glDeleteBuffers(1, &g_quad_vbo);
    if (g_font) TTF_CloseFont(g_font);
}

void LoadingScreen_Show(const Char* map_name) {
    g_is_active = true;
    if (g_background_texture) {
        glDeleteTextures(1, &g_background_texture);
        g_background_texture = 0;
    }

    if (map_name == nullptr) {
        return;
    }

    Char image_path[256];
    snprintf(image_path, sizeof(image_path), "media/%s.png", map_name);
    
    SDL_Surface* surf = IMG_Load(image_path);
    if (surf) {
        SDL_Surface* fSurf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surf);

        if (fSurf) {
            glGenTextures(1, &g_background_texture);
            glBindTexture(GL_TEXTURE_2D, g_background_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fSurf->w, fSurf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, fSurf->pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            SDL_FreeSurface(fSurf);
        }
    } else {
        Console_Printf_Warning("No loading screen background found for '%s'. Expected at '%s'", map_name, image_path);
    }
}

void LoadingScreen_Hide() {
    g_is_active = false;
    if (g_background_texture) {
        glDeleteTextures(1, &g_background_texture);
        g_background_texture = 0;
    }
}

static void render_quad(GLuint texture, Float x, Float y, Float w, Float h) {
    if (!texture) return;
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    Shader_Set(g_shader, "u_texture", 0);
    Shader_Set(g_shader, "u_color_tint", Vec4{ 1.0f, 1.0f, 1.0f, 1.0f });
    
    Float vertices[] = {
        x,     y + h, 0.0f, 1.0f,
        x,     y,     0.0f, 0.0f,
        x + w, y,     1.0f, 0.0f,

        x,     y + h, 0.0f, 1.0f,
        x + w, y,     1.0f, 0.0f,
        x + w, y + h, 1.0f, 1.0f
    };
    
    glBindVertexArray(g_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_quad_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void LoadingScreen_Render() {
    if (!g_is_active) return;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(g_shader);
    Mat4 projection_matrix = mat4_ortho(0.0f, (Float)g_screen_width, (Float)g_screen_height, 0.0f, -1.0f, 1.0f);
    Shader_Set(g_shader, "projection", &projection_matrix);

    if (g_background_texture) {
        render_quad(g_background_texture, 0, 0, (Float)g_screen_width, (Float)g_screen_height);
    }
    
    if (g_text_texture) {
        Float text_x = g_screen_width - g_text_width - 20.0f;
        Float text_y = g_screen_height - g_text_height - 20.0f;
        render_quad(g_text_texture, text_x, text_y, (Float)g_text_width, (Float)g_text_height);
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}