/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

//----------------------------------------//
// Brief: Texture manager, this handles missing textures and the "materials.def"
//----------------------------------------//

#include <GL/glew.h>
#include <stdbool.h>
#include "materials_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MATERIALS 16384

typedef struct {
    char name[64];
    GLuint diffuseMap;
    GLuint normalMap;
    GLuint rmaMap;
    GLuint heightMap;
    GLuint detailDiffuseMap;

    char diffusePath[128];
    char normalPath[128];
    char rmaPath[128];
    char heightPath[128];
    char detailDiffusePath[128];
    bool isLoaded;

    float heightScale;
    float detailScale;
    float roughness;
    float metalness;
} Material;

extern MATERIALS_API Material g_MissingMaterial;
extern MATERIALS_API Material g_NodrawMaterial;
extern MATERIALS_API bool g_is_editor_mode;
extern MATERIALS_API bool g_is_unlit_mode;

extern MATERIALS_API GLuint missingTextureID;
extern MATERIALS_API GLuint defaultNormalMapID;
extern MATERIALS_API GLuint defaultRmaMapID;

MATERIALS_API void TextureManager_Init();
MATERIALS_API void TextureManager_Shutdown();

MATERIALS_API bool TextureManager_ParseMaterialsFromFile(const char* filepath);

MATERIALS_API Material* TextureManager_FindMaterial(const char* name);
MATERIALS_API Material* TextureManager_GetMaterial(int index);
MATERIALS_API int TextureManager_GetMaterialCount();
MATERIALS_API int TextureManager_FindMaterialIndex(const char* name);

MATERIALS_API GLuint loadCubemap(const char* faces[6]);

MATERIALS_API void TextureManager_LoadMaterialTextures(Material* material);
MATERIALS_API GLuint TextureManager_ReloadCubemap(const char* faces[6], GLuint oldTextureID);
MATERIALS_API GLuint TextureManager_LoadLUT(const char* filename_only);
MATERIALS_API GLuint loadTexture(const char* path, bool isSrgb);

#ifdef __cplusplus
}
#endif

#endif // TEXTURE_MANAGER_H