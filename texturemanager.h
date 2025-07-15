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
// Brief: Sounds, this calls dsp_reverb
//----------------------------------------//

#include <GL/glew.h>
#include <stdbool.h>

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

extern Material g_MissingMaterial;
extern bool g_is_editor_mode;
extern bool g_is_unlit_mode;

extern GLuint missingTextureID;
extern GLuint defaultNormalMapID;
extern GLuint defaultRmaMapID;

void TextureManager_Init();
void TextureManager_Shutdown();

bool TextureManager_ParseMaterialsFromFile(const char* filepath);

Material* TextureManager_FindMaterial(const char* name);
Material* TextureManager_GetMaterial(int index);
int TextureManager_GetMaterialCount();
int TextureManager_FindMaterialIndex(const char* name);

GLuint loadCubemap(const char* faces[6]);

void TextureManager_LoadMaterialTextures(Material* material);
GLuint TextureManager_ReloadCubemap(const char* faces[6], GLuint oldTextureID);
GLuint TextureManager_LoadLUT(const char* filename_only);
GLuint loadTexture(const char* path, bool isSrgb);

#ifdef __cplusplus
}
#endif

#endif // TEXTURE_MANAGER_H