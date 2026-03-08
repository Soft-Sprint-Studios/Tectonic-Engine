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


constexpr int MAX_MATERIALS = 16384;

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
        Bool alpha;
    } Material;

    extern Material g_MissingMaterial;
    extern Material g_NodrawMaterial;
#ifdef BUILD_EDITOR
    extern Bool g_is_editor_mode;
#endif
    extern Bool g_is_thumbnail_mode;
    extern Bool g_is_unlit_mode;

    extern GLuint missingTextureID;
    extern GLuint defaultNormalMapID;
    extern GLuint defaultRmaMapID;

    void TextureManager_Init();
    void TextureManager_Shutdown();

    Bool TextureManager_ParseMaterialsFromFile(const Char* filepath);

    Material* TextureManager_FindMaterial(const Char* name);
    Material* TextureManager_GetMaterial(Int index);
    Int TextureManager_GetMaterialCount();
    Int TextureManager_FindMaterialIndex(const Char* name);

    GLuint loadCubemap(const Char* faces[6]);

    void TextureManager_LoadMaterialTextures(Material* material);
    GLuint TextureManager_ReloadCubemap(const Char* faces[6], GLuint oldTextureID);
    GLuint TextureManager_LoadLUT(const Char* filename_only);
    GLuint TextureManager_LoadFromMemory(const void* data, Int data_size, Bool isSrgb, TextureLoadContext context);
    GLuint loadTexture(const Char* path, Bool isSrgb, TextureLoadContext context);


#endif // TEXTURE_MANAGER_H