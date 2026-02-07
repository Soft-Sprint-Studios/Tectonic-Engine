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
#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

//----------------------------------------//
// Brief: Texture manager, this handles missing textures and the "materials.def"
//----------------------------------------//

#include <GL/glew.h>
#include <stdbool.h>
#include "materials_api.h"


#define MAX_MATERIALS 16384

    typedef enum {
        TEXTURE_LOAD_CONTEXT_WORLD,
        TEXTURE_LOAD_CONTEXT_UI_THUMBNAIL
    } TextureLoadContext;

    typedef struct {
        Char name[64];
        GLuint diffuseMap;
        GLuint normalMap;
        GLuint rmaMap;
        GLuint heightMap;
        GLuint detailDiffuseMap;

        Char diffusePath[128];
        Char normalPath[128];
        Char rmaPath[128];
        Char heightPath[128];
        Char detailDiffusePath[128];
        Bool isLoaded;

        Float heightScale;
        Float detailScale;
        Float roughness;
        Float metalness;
        Bool useTesselation;
        Bool alpha;
    } Material;

    extern MATERIALS_API Material g_MissingMaterial;
    extern MATERIALS_API Material g_NodrawMaterial;
    extern MATERIALS_API Bool g_is_editor_mode;
    extern MATERIALS_API Bool g_is_thumbnail_mode;
    extern MATERIALS_API Bool g_is_unlit_mode;

    extern MATERIALS_API GLuint missingTextureID;
    extern MATERIALS_API GLuint defaultNormalMapID;
    extern MATERIALS_API GLuint defaultRmaMapID;

    MATERIALS_API void TextureManager_Init();
    MATERIALS_API void TextureManager_Shutdown();

    MATERIALS_API Bool TextureManager_ParseMaterialsFromFile(const Char* filepath);

    MATERIALS_API Material* TextureManager_FindMaterial(const Char* name);
    MATERIALS_API Material* TextureManager_GetMaterial(Int index);
    MATERIALS_API Int TextureManager_GetMaterialCount();
    MATERIALS_API Int TextureManager_FindMaterialIndex(const Char* name);

    MATERIALS_API GLuint loadCubemap(const Char* faces[6]);

    MATERIALS_API void TextureManager_LoadMaterialTextures(Material* material);
    MATERIALS_API GLuint TextureManager_ReloadCubemap(const Char* faces[6], GLuint oldTextureID);
    MATERIALS_API GLuint TextureManager_LoadLUT(const Char* filename_only);
    MATERIALS_API GLuint TextureManager_LoadFromMemory(const void* data, Int data_size, Bool isSrgb, TextureLoadContext context);
    MATERIALS_API GLuint loadTexture(const Char* path, Bool isSrgb, TextureLoadContext context);


#endif // TEXTURE_MANAGER_H