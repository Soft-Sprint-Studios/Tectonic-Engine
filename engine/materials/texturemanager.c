/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "texturemanager.h"
#include "gl_console.h"
#include "cvar.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>

static Material materials[MAX_MATERIALS];
static int num_materials = 0;

MATERIALS_API GLuint missingTextureID;
MATERIALS_API GLuint defaultNormalMapID;
MATERIALS_API GLuint defaultRmaMapID;
MATERIALS_API Material g_MissingMaterial;
MATERIALS_API Material g_NodrawMaterial;

MATERIALS_API bool g_is_editor_mode = false;
MATERIALS_API bool g_is_unlit_mode = false;

static char* prependTexturePath(const char* filename) {
    if (filename == NULL || filename[0] == '\0') return NULL;
    const char* baseFolder = "textures/";
    size_t len = strlen(baseFolder) + strlen(filename) + 1;
    char* fullPath = (char*)malloc(len);
    if (!fullPath) {
        Console_Printf_Error("TextureManager ERROR: Memory allocation failed for texture path.\n");
        return NULL;
    }
    strcpy(fullPath, baseFolder);
    strcat(fullPath, filename);
    return fullPath;
}

static GLuint createMissingTexture() {
#define width 64
#define height 64
    unsigned char data[width * height * 4];
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int i = (y * width + x) * 4;
            bool is_purple = ((x / 8) % 2) ^ ((y / 8) % 2);
            if (is_purple) {
                data[i + 0] = 255; data[i + 1] = 0;   data[i + 2] = 255;
            }
            else {
                data[i + 0] = 0;   data[i + 1] = 0;   data[i + 2] = 0;
            }
            data[i + 3] = 255;
        }
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texID;
}

static GLuint createDefaultRmaTexture() {
    GLuint texID;
    unsigned char data[] = { 255, 128, 0, 255 };
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return texID;
}

static GLuint createPlaceholderTexture(unsigned char r, unsigned char g, unsigned char b) {
    GLuint texID;
    unsigned char data[] = { r, g, b, 255 };
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return texID;
}

GLuint loadTexture(const char* path, bool isSrgb) {
    char* fullPath = prependTexturePath(path);
    if (!fullPath) {
        Console_Printf_Warning("TextureManager WARNING: Failed to load texture '%s'. Using placeholder.\n", path);
        return missingTextureID;
    }

    SDL_Surface* surf = IMG_Load(fullPath);
    if (!surf) {
        Console_Printf_Warning("TextureManager WARNING: Failed to load texture '%s'. Using placeholder.\n", fullPath);
        free(fullPath);
        return missingTextureID;
    }

    if (!g_is_editor_mode) {
        int quality = Cvar_GetInt("r_texture_quality");
        float scale_factor = 1.0f;
        switch (quality) {
        case 1: scale_factor = 0.25f; break;
        case 2: scale_factor = 0.33f; break;
        case 3: scale_factor = 0.5f; break;
        case 4: scale_factor = 0.75f; break;
        case 5: scale_factor = 1.0f; break;
        default: scale_factor = 1.0f; break;
        }

        if (scale_factor < 1.0f) {
            int scaled_w = (int)(surf->w * scale_factor);
            int scaled_h = (int)(surf->h * scale_factor);

            if (scaled_w == 0) scaled_w = 1;
            if (scaled_h == 0) scaled_h = 1;

            SDL_Surface* scaled_surf = SDL_CreateRGBSurfaceWithFormat(0, scaled_w, scaled_h, 32, SDL_PIXELFORMAT_RGBA32);
            if (scaled_surf) {
                SDL_BlitScaled(surf, NULL, scaled_surf, NULL);
                SDL_FreeSurface(surf);
                surf = scaled_surf;
            }
            else {
                Console_Printf_Warning("TextureManager WARNING: Failed to create scaled surface for '%s' (quality). Using original resolution.\n", fullPath);
            }
        }
    }

    if (g_is_editor_mode) {
        int max_editor_dim = 128;

        if (surf->w > max_editor_dim || surf->h > max_editor_dim) {
            float scale_factor = (float)max_editor_dim / (float)fmax(surf->w, surf->h);
            int scaled_w = (int)(surf->w * scale_factor);
            int scaled_h = (int)(surf->h * scale_factor);

            SDL_Surface* scaled_surf = SDL_CreateRGBSurfaceWithFormat(0, scaled_w, scaled_h, 32, SDL_PIXELFORMAT_RGBA32);
            if (scaled_surf) {
                SDL_BlitScaled(surf, NULL, scaled_surf, NULL);
                SDL_FreeSurface(surf);
                surf = scaled_surf;
            }
            else {
                Console_Printf_Error("TextureManager ERROR: Failed to create scaled surface for '%s' (editor). Using full-res.\n", fullPath);
            }
        }
    }

    SDL_Surface* fSurf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);

    if (!fSurf) {
        Console_Printf_Error("TextureManager ERROR: Failed to convert surface for '%s'\n", fullPath);
        free(fullPath);
        return missingTextureID;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, isSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, fSurf->w, fSurf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, fSurf->pixels);

    if (!g_is_editor_mode) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        GLfloat max_anisotropy;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropy);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    SDL_FreeSurface(fSurf);
    free(fullPath);
    return texID;
}

void TextureManager_LoadMaterialTextures(Material* material) {
    if (!material || material->isLoaded) {
        return;
    }

    if (strlen(material->diffusePath) > 0) material->diffuseMap = loadTexture(material->diffusePath, true); else material->diffuseMap = missingTextureID;
    if (strlen(material->normalPath) > 0) material->normalMap = loadTexture(material->normalPath, false); else material->normalMap = defaultNormalMapID;
    if (strlen(material->rmaPath) > 0) material->rmaMap = loadTexture(material->rmaPath, false); else material->rmaMap = defaultRmaMapID;
    if (strlen(material->heightPath) > 0) material->heightMap = loadTexture(material->heightPath, false); else material->heightMap = 0;
    if (strlen(material->detailDiffusePath) > 0) material->detailDiffuseMap = loadTexture(material->detailDiffusePath, true); else material->detailDiffuseMap = 0;

    material->isLoaded = true;
}

GLuint loadCubemap(const char* faces[6]) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    for (unsigned int i = 0; i < 6; i++) {
        SDL_Surface* surf = IMG_Load(faces[i]);
        if (surf) {
            SDL_Surface* fSurf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGB24, 0);
            SDL_FreeSurface(surf);
            if (fSurf) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, fSurf->w, fSurf->h, 0, GL_RGB, GL_UNSIGNED_BYTE, fSurf->pixels);
                SDL_FreeSurface(fSurf);
            }
        }
        else {
            Console_Printf_Warning("Cubemap texture failed to load at path: %s\n", faces[i]);
        }
    }
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return textureID;
}

GLuint TextureManager_LoadLUT(const char* filename_only) {
    char* fullPath = prependTexturePath(filename_only);
    if (!fullPath) {
        return missingTextureID;
    }

    SDL_Surface* surf = IMG_Load(fullPath);
    if (!surf) {
        Console_Printf_Warning("TextureManager WARNING: Failed to load LUT texture '%s'. Using missingTextureID.\n", fullPath);
        free(fullPath);
        return missingTextureID;
    }

    SDL_Surface* fSurf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);

    if (!fSurf) {
        Console_Printf_Warning("TextureManager ERROR: Failed to convert LUT surface for '%s'. Using missingTextureID.\n", fullPath);
        free(fullPath);
        return missingTextureID;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fSurf->w, fSurf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, fSurf->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    SDL_FreeSurface(fSurf);
    free(fullPath);

    return texID;
}

GLuint TextureManager_ReloadCubemap(const char* faces[6], GLuint oldTextureID) {
    if (glIsTexture(oldTextureID)) {
        glDeleteTextures(1, &oldTextureID);
    }
    return loadCubemap(faces);
}

static char* strip_numeric_suffix(const char* name) {
    const char* dot = strrchr(name, '.');
    if (dot && dot != name) {
        bool all_digits = true;
        const char* p = dot + 1;
        if (*p == '\0') {
            all_digits = false;
        }
        while (*p) {
            if (!isdigit((unsigned char)*p)) {
                all_digits = false;
                break;
            }
            p++;
        }

        if (all_digits) {
            size_t base_len = dot - name;
            char* base_name = malloc(base_len + 1);
            if (base_name) {
                strncpy(base_name, name, base_len);
                base_name[base_len] = '\0';
                return base_name;
            }
        }
    }
    return NULL;
}

void TextureManager_Init() {
    memset(materials, 0, sizeof(materials));
    num_materials = 0;

    missingTextureID = createMissingTexture();
    defaultNormalMapID = createPlaceholderTexture(128, 128, 255);
    defaultRmaMapID = createDefaultRmaTexture();

    strncpy(g_MissingMaterial.name, "___MISSING___", 63);
    g_MissingMaterial.diffuseMap = missingTextureID;
    g_MissingMaterial.isLoaded = true;
    g_MissingMaterial.normalMap = defaultNormalMapID;
    g_MissingMaterial.rmaMap = defaultRmaMapID;

    strncpy(g_NodrawMaterial.name, "nodraw", 63);
    g_NodrawMaterial.isLoaded = true;
    g_NodrawMaterial.diffuseMap = missingTextureID;
    g_NodrawMaterial.normalMap = defaultNormalMapID;
    g_NodrawMaterial.rmaMap = defaultRmaMapID;

    Console_Printf("Texture Manager Initialized.\n");
}

void TextureManager_Shutdown() {
    for (int i = 0; i < num_materials; ++i) {
        if (materials[i].diffuseMap != missingTextureID) glDeleteTextures(1, &materials[i].diffuseMap);
        if (materials[i].normalMap != defaultNormalMapID) glDeleteTextures(1, &materials[i].normalMap);
        if (materials[i].rmaMap != defaultRmaMapID) glDeleteTextures(1, &materials[i].rmaMap);
        if (materials[i].heightMap != 0) glDeleteTextures(1, &materials[i].heightMap);
        if (materials[i].detailDiffuseMap != 0) glDeleteTextures(1, &materials[i].detailDiffuseMap);
    }

    glDeleteTextures(1, &missingTextureID);
    glDeleteTextures(1, &defaultNormalMapID);
    glDeleteTextures(1, &defaultRmaMapID);

    Console_Printf("Texture Manager Shutdown.\n");
}

Material* TextureManager_FindMaterial(const char* name) {
    if (strcmp(name, "nodraw") == 0) {
        return &g_NodrawMaterial;
    }
    for (int i = 0; i < num_materials; ++i) {
        if (strcmp(materials[i].name, name) == 0)
        {
            Material* mat = &materials[i];
            if (!mat->isLoaded) {
                TextureManager_LoadMaterialTextures(mat);
            }
            return mat;
        }
    }

    char* base_name = strip_numeric_suffix(name);
    if (base_name) {
        for (int i = 0; i < num_materials; ++i) {
            if (strcmp(materials[i].name, base_name) == 0) {
                free(base_name);
                Material* mat = &materials[i];
                if (!mat->isLoaded) TextureManager_LoadMaterialTextures(mat);
                return mat;
            }
        }
        free(base_name);
    }

    return &g_MissingMaterial;
}

int TextureManager_FindMaterialIndex(const char* name) {
    for (int i = 0; i < num_materials; ++i) {
        if (strcmp(materials[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

Material* TextureManager_GetMaterial(int index) {
    if (index < 0 || index >= num_materials) return &g_MissingMaterial;
    return &materials[index];
}

int TextureManager_GetMaterialCount() {
    return num_materials;
}

bool TextureManager_ParseMaterialsFromFile(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        Console_Printf_Error("TextureManager ERROR: Could not open material file '%s'\n", filepath);
        return false;
    }

    char line[256];
    Material* current_material = NULL;

    while (fgets(line, sizeof(line), file)) {
        char* trimmed_line = trim(line);

        if (strlen(trimmed_line) == 0 || trimmed_line[0] == '/' || trimmed_line[0] == '#') {
            continue;
        }

        if (trimmed_line[0] == '"') {
            if (num_materials >= MAX_MATERIALS) {
                Console_Printf_Error("TextureManager ERROR: Max materials reached. Cannot parse more.\n");
                break;
            }
            current_material = &materials[num_materials];
            memset(current_material, 0, sizeof(Material));
            current_material->roughness = -1.0f;
            current_material->metalness = -1.0f;
            sscanf(trimmed_line, "\"%[^\"]\"", current_material->name);
        }
        else if (trimmed_line[0] == '{') {
        }
        else if (trimmed_line[0] == '}') {
            if (current_material) {
                num_materials++;
                current_material = NULL;
            }
        }
        else if (current_material) {
            char key[64], value[128];
            if (sscanf(trimmed_line, "%s = \"%127[^\"]\"", key, value) == 2) {
                if (strcmp(key, "diffuse") == 0) {
                    strcpy(current_material->diffusePath, value);
                }
                else if (strcmp(key, "normal") == 0) {
                    strcpy(current_material->normalPath, value);
                }
                else if (strcmp(key, "arm") == 0) {
                    strcpy(current_material->rmaPath, value);
                }
                else if (strcmp(key, "height") == 0) {
                    strcpy(current_material->heightPath, value);
                }
                else if (strcmp(key, "detail") == 0) {
                    strcpy(current_material->detailDiffusePath, value);
                }
            }
            else {
                float float_val;
                if (sscanf(trimmed_line, "%s = %f", key, &float_val) == 2) {
                    if (strcmp(key, "heightScale") == 0) {
                        current_material->heightScale = float_val;
                    }
                    else if (strcmp(key, "detailscale") == 0) {
                        current_material->detailScale = float_val;
                    }
                    else if (strcmp(key, "roughness") == 0) {
                        current_material->roughness = float_val;
                    }
                    else if (strcmp(key, "metalness") == 0) {
                        current_material->metalness = float_val;
                    }
                }
            }
        }
    }

    fclose(file);
    return true;
}