/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "texturemanager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>

static Material materials[MAX_MATERIALS];
static int num_materials = 0;

static GLuint missingTextureID;
static GLuint defaultNormalMapID;
static GLuint defaultRmaMapID;
Material g_MissingMaterial;

#define FOURCC_DXT1 0x31545844
#define FOURCC_DXT5 0x35545844

#pragma pack(push,1)
typedef struct {
    unsigned int dwSize;
    unsigned int dwFlags;
    unsigned int dwHeight;
    unsigned int dwWidth;
    unsigned int dwPitchOrLinearSize;
    unsigned int dwDepth;
    unsigned int dwMipMapCount;
    unsigned int dwReserved1[11];
    struct {
        unsigned int dwSize;
        unsigned int dwFlags;
        unsigned int dwFourCC;
        unsigned int dwRGBBitCount;
        unsigned int dwRBitMask;
        unsigned int dwGBitMask;
        unsigned int dwBBitMask;
        unsigned int dwABitMask;
    } ddspf;
    unsigned int dwCaps;
    unsigned int dwCaps2;
    unsigned int dwCaps3;
    unsigned int dwCaps4;
    unsigned int dwReserved2;
} DDS_HEADER;
#pragma pack(pop)

static char* prependTexturePath(const char* filename) {
    if (filename == NULL || filename[0] == '\0') return NULL;
    const char* baseFolder = "textures/";
    size_t len = strlen(baseFolder) + strlen(filename) + 1;
    char* fullPath = (char*)malloc(len);
    if (!fullPath) {
        printf("TextureManager ERROR: Memory allocation failed for texture path.\n");
        return NULL;
    }
    strcpy(fullPath, baseFolder);
    strcat(fullPath, filename);
    return fullPath;
}

GLuint loadDDSTexture(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf("Failed to open '%s'\n", filename);
        return 0;
    }

    char filecode[4];
    fread(filecode, 1, 4, fp);
    if (memcmp(filecode, "DDS ", 4) != 0) {
        printf("Not a DDS file: %s\n", filename);
        fclose(fp);
        return 0;
    }

    DDS_HEADER header;
    fread(&header, sizeof(DDS_HEADER), 1, fp);

    unsigned int width = header.dwWidth;
    unsigned int height = header.dwHeight;
    unsigned int mipMapCount = header.dwMipMapCount ? header.dwMipMapCount : 1;
    unsigned int fourCC = header.ddspf.dwFourCC;

    GLenum format;
    unsigned int blockSize;
    if (fourCC == FOURCC_DXT1) {
        format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        blockSize = 8;
    }
    else if (fourCC == FOURCC_DXT5) {
        format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        blockSize = 16;
    }
    else {
        printf("Unsupported DDS format\n");
        fclose(fp);
        return 0;
    }

    size_t bufsize = 0;
    unsigned int w = width;
    unsigned int h = height;
    for (unsigned int i = 0; i < mipMapCount; i++) {
        size_t size = ((w + 3) / 4) * ((h + 3) / 4) * blockSize;
        bufsize += size;
        w = w > 1 ? w / 2 : 1;
        h = h > 1 ? h / 2 : 1;
    }

    unsigned char* buffer = (unsigned char*)malloc(bufsize);
    if (!buffer) {
        printf("Out of memory\n");
        fclose(fp);
        return 0;
    }

    fread(buffer, 1, bufsize, fp);
    fclose(fp);

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    w = width;
    h = height;
    size_t offset = 0;

    for (unsigned int level = 0; level < mipMapCount; level++) {
        size_t size = ((w + 3) / 4) * ((h + 3) / 4) * blockSize;
        glCompressedTexImage2D(GL_TEXTURE_2D, level, format, w, h, 0, (GLsizei)size, buffer + offset);
        offset += size;
        w = w > 1 ? w / 2 : 1;
        h = h > 1 ? h / 2 : 1;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipMapCount > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    free(buffer);

    return textureID;
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
    unsigned char data[] = { 128, 0, 255, 255 };
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

static GLuint loadTexture(const char* path) {
    char* fullPath = prependTexturePath(path);
    if (!fullPath) {
        printf("TextureManager WARNING: Failed to load texture '%s'. Using placeholder.\n", path);
        return missingTextureID;
    }

    const char* ext = strrchr(fullPath, '.');
    if (ext && (_stricmp(ext, ".dds") == 0)) {
        GLuint texID = loadDDSTexture(fullPath);
        free(fullPath);
        return texID ? texID : missingTextureID;
    }

    SDL_Surface* surf = IMG_Load(fullPath);
    if (!surf) {
        printf("TextureManager WARNING: Failed to load texture '%s'. Using placeholder.\n", fullPath);
        free(fullPath);
        return missingTextureID;
    }

    SDL_Surface* fSurf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);
    if (!fSurf) {
        printf("TextureManager ERROR: Failed to convert surface for '%s'\n", fullPath);
        free(fullPath);
        return missingTextureID;
    }

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fSurf->w, fSurf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, fSurf->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLfloat max_anisotropy;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropy);

    SDL_FreeSurface(fSurf);
    free(fullPath);
    return texID;
}

void TextureManager_LoadMaterialTextures(Material* material) {
    if (!material || material->isLoaded) {
        return;
    }

    if (strlen(material->diffusePath) > 0) material->diffuseMap = loadTexture(material->diffusePath); else material->diffuseMap = missingTextureID;
    if (strlen(material->normalPath) > 0) material->normalMap = loadTexture(material->normalPath); else material->normalMap = defaultNormalMapID;
    if (strlen(material->rmaPath) > 0) material->rmaMap = loadTexture(material->rmaPath); else material->rmaMap = defaultRmaMapID;
    if (strlen(material->heightPath) > 0) material->heightMap = loadTexture(material->heightPath); else material->heightMap = 0;
    if (strlen(material->detailDiffusePath) > 0) material->detailDiffuseMap = loadTexture(material->detailDiffusePath); else material->detailDiffuseMap = 0;

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
            printf("Cubemap texture failed to load at path: %s\n", faces[i]);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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
        printf("TextureManager WARNING: Failed to load LUT texture '%s'. Using missingTextureID.\n", fullPath);
        free(fullPath);
        return missingTextureID;
    }

    SDL_Surface* fSurf = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);

    if (!fSurf) {
        printf("TextureManager ERROR: Failed to convert LUT surface for '%s'. Using missingTextureID.\n", fullPath);
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

static char* trim_whitespace(char* str) {
    char* end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
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

    printf("Texture Manager Initialized.\n");
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

    printf("Texture Manager Shutdown.\n");
}

Material* TextureManager_FindMaterial(const char* name) {
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
                if (!mat->isLoaded) {
                    if (strlen(mat->diffusePath) > 0) mat->diffuseMap = loadTexture(mat->diffusePath); else mat->diffuseMap = missingTextureID;
                    if (strlen(mat->normalPath) > 0) mat->normalMap = loadTexture(mat->normalPath); else mat->normalMap = defaultNormalMapID;
                    if (strlen(mat->rmaPath) > 0) mat->rmaMap = loadTexture(mat->rmaPath); else mat->rmaMap = defaultRmaMapID;
                    if (strlen(mat->heightPath) > 0) mat->heightMap = loadTexture(mat->heightPath); else mat->heightMap = 0;
                    mat->isLoaded = true;
                }
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
        printf("TextureManager ERROR: Could not open material file '%s'\n", filepath);
        return false;
    }

    char line[256];
    Material* current_material = NULL;

    while (fgets(line, sizeof(line), file)) {
        char* trimmed_line = trim_whitespace(line);

        if (strlen(trimmed_line) == 0 || trimmed_line[0] == '/' || trimmed_line[0] == '#') {
            continue;
        }

        if (trimmed_line[0] == '"') {
            if (num_materials >= MAX_MATERIALS) {
                printf("TextureManager ERROR: Max materials reached. Cannot parse more.\n");
                break;
            }
            current_material = &materials[num_materials];
            memset(current_material, 0, sizeof(Material));
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
                else if (strcmp(key, "rma") == 0) {
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
                    if (strcmp(key, "cubemapStrength") == 0) {
                        current_material->cubemapStrength = float_val;
                    }
                    else if (strcmp(key, "heightScale") == 0) {
                        current_material->heightScale = float_val;
                    }
                    else if (strcmp(key, "detailscale") == 0) {
                        current_material->detailScale = float_val;
                    }
                }
            }
        }
    }

    fclose(file);
    return true;
}