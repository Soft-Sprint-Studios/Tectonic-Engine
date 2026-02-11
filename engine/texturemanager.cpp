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
static Int num_materials = 0;

GLuint missingTextureID;
GLuint defaultNormalMapID;
GLuint defaultRmaMapID;
Material g_MissingMaterial;
Material g_NodrawMaterial;

extern Bool g_is_editor_mode;
Bool g_is_thumbnail_mode = false;
Bool g_is_unlit_mode = false;

static Char* prependTexturePath(const Char* filename) {
    if (filename == nullptr || filename[0] == '\0') return nullptr;

    if (strncmp(filename, "textures/", 9) == 0 || strncmp(filename, "lightmaps/", 10) == 0) {
        Usize len = strlen(filename) + 1;
        Char* copy = new Char[len];
        strcpy(copy, filename);
        return copy;
    }

    const Char* baseFolder = "textures/";
    Usize len = strlen(baseFolder) + strlen(filename) + 1;

    Char* fullPath = new Char[len];
    strcpy(fullPath, baseFolder);
    strcat(fullPath, filename);

    return fullPath;
}

static GLuint createMissingTexture() {
    Uchar data[64 * 64 * 4];
    for (Int y = 0; y < 64; ++y) {
        for (Int x = 0; x < 64; ++x) {
            Int i = (y * 64 + x) * 4;
            Bool is_purple = ((x / 8) % 2) ^ ((y / 8) % 2);
            if (is_purple) {
                data[i + 0] = 255; 
                data[i + 1] = 0;   
                data[i + 2] = 255;
            }
            else {
                data[i + 0] = 0;
                data[i + 1] = 0;   
                data[i + 2] = 0;
            }
            data[i + 3] = 255;
        }
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return texID;
}

static GLuint createDefaultRmaTexture() {
    GLuint texID;
    Uchar data[] = { 255, 128, 0, 255 };
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return texID;
}

static GLuint createPlaceholderTexture(Uchar r, Uchar g, Uchar b) {
    GLuint texID;
    Uchar data[] = { r, g, b, 255 };
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return texID;
}

GLuint TextureManager_LoadFromMemory(const void* data, Int data_size, Bool isSrgb, TextureLoadContext context) {
    if (!data || data_size <= 0) {
        return missingTextureID;
    }

    SDL_RWops* rw = SDL_RWFromConstMem(data, data_size);
    if (!rw) {
        Console_Printf_Error("Failed to create RWops from memory.\n");
        return missingTextureID;
    }

    SDL_Surface* surf = IMG_Load_RW(rw, 1);
    if (!surf) {
        Console_Printf_Error("Failed to load texture from memory: %s\n", IMG_GetError());
        return missingTextureID;
    }

    if (context == TEXTURE_LOAD_CONTEXT_UI_THUMBNAIL) {
        Int max_editor_dim = 128;
        if (surf->w > max_editor_dim || surf->h > max_editor_dim) {
            Float scale_factor = (Float)max_editor_dim / (Float)fmax(surf->w, surf->h);
            Int scaled_w = (Int)(surf->w * scale_factor);
            Int scaled_h = (Int)(surf->h * scale_factor);
            SDL_Surface* scaled_surf = SDL_CreateRGBSurfaceWithFormat(0, scaled_w, scaled_h, 32, SDL_PIXELFORMAT_RGBA32);
            if (scaled_surf) {
                SDL_BlitScaled(surf, nullptr, scaled_surf, nullptr);
                SDL_FreeSurface(surf);
                surf = scaled_surf;
            }
        }
    }
    else {
        Int quality = Cvar_GetInt("r_texture_quality");
        Float scale_factor = 1.0f;
        switch (quality) {
        case 1: scale_factor = 0.25f; break;
        case 2: scale_factor = 0.33f; break;
        case 3: scale_factor = 0.5f; break;
        case 4: scale_factor = 0.75f; break;
        case 5: scale_factor = 1.0f; break;
        default: scale_factor = 1.0f; break;
        }
        if (scale_factor < 1.0f && (surf->w > 16 || surf->h > 16)) {
            Int scaled_w = (Int)(surf->w * scale_factor);
            Int scaled_h = (Int)(surf->h * scale_factor);
            if (scaled_w < 1) scaled_w = 1;
            if (scaled_h < 1) scaled_h = 1;
            SDL_Surface* scaled_surf = SDL_CreateRGBSurfaceWithFormat(0, scaled_w, scaled_h, 32, SDL_PIXELFORMAT_RGBA32);
            if (scaled_surf) {
                SDL_BlitScaled(surf, nullptr, scaled_surf, nullptr);
                SDL_FreeSurface(surf);
                surf = scaled_surf;
            }
        }
    }

    SDL_Surface* fSurf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);

    if (!fSurf) {
        Console_Printf_Error("Failed to convert surface from memory data.\n");
        return missingTextureID;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, isSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, fSurf->w, fSurf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, fSurf->pixels);

    if (context == TEXTURE_LOAD_CONTEXT_WORLD && Cvar_GetInt("r_mipmapping") == 1) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        if (GLEW_EXT_texture_filter_anisotropic) {
            GLfloat max_anisotropy;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
            Float desired_anisotropy = Cvar_GetFloat("r_anisotropy");
            Float final_anisotropy = (desired_anisotropy > max_anisotropy) ? max_anisotropy : desired_anisotropy;
            if (final_anisotropy > 1.0f) {
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, final_anisotropy);
            }
        }
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    SDL_FreeSurface(fSurf);
    return texID;
}

GLuint loadTexture(const Char* path, Bool isSrgb, TextureLoadContext context) {
    Char* fullPath = prependTexturePath(path);
    if (!fullPath) {
        Console_Printf_Error("Failed to load texture '%s'\n", path);
        return missingTextureID;
    }

    SDL_Surface* surf = IMG_Load(fullPath);
    if (!surf) {
        Console_Printf_Error("Failed to load texture '%s'\n", fullPath);
        delete[] fullPath;
        return missingTextureID;
    }

    if (context == TEXTURE_LOAD_CONTEXT_UI_THUMBNAIL) {
        Int max_editor_dim = 128;
        if (surf->w > max_editor_dim || surf->h > max_editor_dim) {
            Float scale_factor = (Float)max_editor_dim / (Float)fmax(surf->w, surf->h);
            Int scaled_w = (Int)(surf->w * scale_factor);
            Int scaled_h = (Int)(surf->h * scale_factor);
            SDL_Surface* scaled_surf = SDL_CreateRGBSurfaceWithFormat(0, scaled_w, scaled_h, 32, SDL_PIXELFORMAT_RGBA32);
            if (scaled_surf) {
                SDL_BlitScaled(surf, nullptr, scaled_surf, nullptr);
                SDL_FreeSurface(surf);
                surf = scaled_surf;
            }
        }
    }
    else {
        Int quality = Cvar_GetInt("r_texture_quality");
        Float scale_factor = 1.0f;
        switch (quality) {
        case 1: scale_factor = 0.25f; break;
        case 2: scale_factor = 0.33f; break;
        case 3: scale_factor = 0.5f; break;
        case 4: scale_factor = 0.75f; break;
        case 5: scale_factor = 1.0f; break;
        default: scale_factor = 1.0f; break;
        }
        if (scale_factor < 1.0f && (surf->w > 16 || surf->h > 16)) {
            Int scaled_w = (Int)(surf->w * scale_factor);
            Int scaled_h = (Int)(surf->h * scale_factor);
            if (scaled_w < 1) scaled_w = 1;
            if (scaled_h < 1) scaled_h = 1;
            SDL_Surface* scaled_surf = SDL_CreateRGBSurfaceWithFormat(0, scaled_w, scaled_h, 32, SDL_PIXELFORMAT_RGBA32);
            if (scaled_surf) {
                SDL_BlitScaled(surf, nullptr, scaled_surf, nullptr);
                SDL_FreeSurface(surf);
                surf = scaled_surf;
            }
        }
    }

    SDL_Surface* fSurf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);

    if (!fSurf) {
        Console_Printf_Error("Failed to convert surface for '%s'\n", fullPath);
        delete[] fullPath;
        return missingTextureID;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, isSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, fSurf->w, fSurf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, fSurf->pixels);

    if (context == TEXTURE_LOAD_CONTEXT_WORLD) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        if (GLEW_EXT_texture_filter_anisotropic) {
            GLfloat max_anisotropy;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
            Float desired_anisotropy = Cvar_GetFloat("r_anisotropy");
            Float final_anisotropy = (desired_anisotropy > max_anisotropy) ? max_anisotropy : desired_anisotropy;
            if (final_anisotropy > 1.0f) {
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, final_anisotropy);
            }
        }
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    SDL_FreeSurface(fSurf);
    delete[] fullPath;
    return texID;
}

void TextureManager_LoadMaterialTextures(Material* material) {
    if (!material || material->isLoaded) {
        return;
    }

    TextureLoadContext context = g_is_thumbnail_mode ? TEXTURE_LOAD_CONTEXT_UI_THUMBNAIL : TEXTURE_LOAD_CONTEXT_WORLD;
    if (strlen(material->diffusePath) > 0) material->diffuseMap = loadTexture(material->diffusePath, true, context); else material->diffuseMap = missingTextureID;
    if (strlen(material->normalPath) > 0) material->normalMap = loadTexture(material->normalPath, false, context); else material->normalMap = defaultNormalMapID;
    if (strlen(material->rmaPath) > 0) material->rmaMap = loadTexture(material->rmaPath, false, context); else material->rmaMap = defaultRmaMapID;
    if (strlen(material->heightPath) > 0) material->heightMap = loadTexture(material->heightPath, false, context); else material->heightMap = 0;
    if (strlen(material->detailDiffusePath) > 0) material->detailDiffuseMap = loadTexture(material->detailDiffusePath, true, context); else material->detailDiffuseMap = 0;

    material->isLoaded = true;
}

GLuint loadCubemap(const Char* faces[6]) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    for (Uint i = 0; i < 6; i++) {
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
    if (Cvar_GetInt("r_mipmapping") == 1) {
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    else {
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return textureID;
}

GLuint TextureManager_LoadLUT(const Char* filename_only) {
    Char* fullPath = prependTexturePath(filename_only);
    if (!fullPath) {
        return missingTextureID;
    }

    SDL_Surface* surf = IMG_Load(fullPath);
    if (!surf) {
        Console_Printf_Error(" Failed to load LUT texture '%s'. Using missingTextureID.\n", fullPath);
        delete[] fullPath;
        return missingTextureID;
    }

    SDL_Surface* fSurf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);

    if (!fSurf) {
        Console_Printf_Error("Failed to convert LUT surface for '%s'. Using missingTextureID.\n", fullPath);
        delete[] fullPath;
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
    delete[] fullPath;

    return texID;
}

GLuint TextureManager_ReloadCubemap(const Char* faces[6], GLuint oldTextureID) {
    if (glIsTexture(oldTextureID)) {
        glDeleteTextures(1, &oldTextureID);
    }
    return loadCubemap(faces);
}

static Char* strip_numeric_suffix(const Char* name) {
    const Char* dot = strrchr(name, '.');
    if (dot && dot != name) {
        const Char* p = dot + 1;
        if (*p == '\0') return nullptr;

        Bool all_digits = true;
        while (*p) {
            if (!isdigit(static_cast<Uchar>(*p))) {
                all_digits = false;
                break;
            }
            p++;
        }

        if (all_digits) {
            Usize base_len = dot - name;
            Char* base_name = new Char[base_len + 1];
            strncpy(base_name, name, base_len);
            base_name[base_len] = '\0';
            return base_name;
        }
    }
    return nullptr;
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
    for (Int i = 0; i < num_materials; ++i) {
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

Material* TextureManager_FindMaterial(const Char* name) {
    if (strcmp(name, "nodraw") == 0) {
        return &g_NodrawMaterial;
    }
    for (Int i = 0; i < num_materials; ++i) {
        if (strcmp(materials[i].name, name) == 0)
        {
            Material* mat = &materials[i];
            if (!mat->isLoaded) {
                TextureManager_LoadMaterialTextures(mat);
            }
            return mat;
        }
    }

    Char* base_name = strip_numeric_suffix(name);
    if (base_name) {
        for (Int i = 0; i < num_materials; ++i) {
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

Int TextureManager_FindMaterialIndex(const Char* name) {
    for (Int i = 0; i < num_materials; ++i) {
        if (strcmp(materials[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

Material* TextureManager_GetMaterial(Int index) {
    if (index < 0 || index >= num_materials) return &g_MissingMaterial;
    return &materials[index];
}

Int TextureManager_GetMaterialCount() {
    return num_materials;
}

Bool TextureManager_ParseMaterialsFromFile(const Char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        Console_Printf_Error("Could not open material file '%s'\n", filepath);
        return false;
    }

    Char line[256];
    Material* current_material = nullptr;

    while (fgets(line, sizeof(line), file)) {
        Char* trimmed_line = trim(line);

        if (strlen(trimmed_line) == 0 || trimmed_line[0] == '/' || trimmed_line[0] == '#') {
            continue;
        }

        if (trimmed_line[0] == '"') {
            if (num_materials >= MAX_MATERIALS) {
                Console_Printf_Error("Max materials reached. Cannot parse more.\n");
                break;
            }
            current_material = &materials[num_materials];
            memset(current_material, 0, sizeof(Material));
            current_material->roughness = -1.0f;
            current_material->metalness = -1.0f;
            current_material->useTesselation = false;
            current_material->alpha = false;
            sscanf(trimmed_line, "\"%[^\"]\"", current_material->name);
        }
        else if (trimmed_line[0] == '{') {
        }
        else if (trimmed_line[0] == '}') {
            if (current_material) {
                num_materials++;
                current_material = nullptr;
            }
        }
        else if (current_material) {
            Char key[64], value[128];
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
                Float float_val;
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
                    else if (strcmp(key, "usetesselation") == 0) {
                        current_material->useTesselation = (float_val != 0.0f);
                    }
                    else if (strcmp(key, "alpha") == 0) {
                        current_material->alpha = (float_val != 0.0f);
                    }
                    else {
                        Console_Printf_Error("Unknown Float key '%s' in material '%s'\n", key, current_material->name);
                    }
                }
                else {
                    Console_Printf_Error("Failed to parse line in material '%s': %s\n", current_material->name, trimmed_line);
                }
            }
        }
    }

    fclose(file);
    return true;
}