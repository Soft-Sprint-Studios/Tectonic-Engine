/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "checksum.h"
#include "gl_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#ifdef PLATFORM_WINDOWS
const char* g_module_names[] = { "engine.dll", "level0.dll", "level1.dll", "math_lib.dll", "physics.dll", "sound.dll", "materials.dll" };
#else
const char* g_module_names[] = { "libengine.so", "liblevel0.so", "liblevel1.so", "libmath_lib.so", "libphysics.so", "libsound.so", "libmaterials.so" };
#endif
const int g_num_modules = sizeof(g_module_names) / sizeof(g_module_names[0]);

static uint32_t crc_table[256];
static int table_initialized = 0;

static void crc32_init_table(void) {
    if (table_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
    table_initialized = 1;
}

static uint32_t crc32_calculate(const void* data, size_t size) {
    const uint8_t* p = data;
    uint32_t crc = 0;
    crc = crc ^ ~0U;
    while (size--) {
        crc = crc_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ ~0U;
}

#ifdef PLATFORM_LINUX
__attribute__((used))
__attribute__((section(".checksum_section")))
EmbeddedChecksum g_EmbeddedChecksum = { 0xBADF00D5, 0 };
#else
__declspec(allocate(".checksum_section"))
EmbeddedChecksum g_EmbeddedChecksum = { 0xBADF00D5, 0 };
#endif

static char* get_module_directory() {
    char path[1024];
#ifdef PLATFORM_WINDOWS
    HMODULE hModule = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)get_module_directory, &hModule);
    GetModuleFileNameA(hModule, path, sizeof(path));
#else
    Dl_info info;
    dladdr((void*)get_module_directory, &info);
    strncpy(path, info.dli_fname, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
#endif

    const char* last_slash = strrchr(path, '/');
    const char* last_bslash = strrchr(path, '\\');
    const char* last_separator = (last_slash > last_bslash) ? last_slash : last_bslash;

    if (last_separator) {
        size_t len = last_separator - path + 1;
        char* dir = (char*)malloc(len + 1);
        strncpy(dir, path, len);
        dir[len] = '\0';
        return dir;
    }
    return _strdup("./");
}


bool Checksum_Verify(const char* exePath) {
    unsigned char* full_buffer = NULL;
    long totalSize = 0;
    long engineModuleSize = 0;

    char* moduleDir = get_module_directory();
    if (!moduleDir) {
        return false;
    }

    for (int i = 0; i < g_num_modules; ++i) {
        char modulePath[512];
        snprintf(modulePath, sizeof(modulePath), "%s%s", moduleDir, g_module_names[i]);

        FILE* moduleFile = fopen(modulePath, "rb");
        if (!moduleFile) {
            Console_Printf_Error("[Checksum] Failed to open module: %s", modulePath);
            free(moduleDir);
            free(full_buffer);
            return false;
        }

        fseek(moduleFile, 0, SEEK_END);
        long moduleSize = ftell(moduleFile);
        fseek(moduleFile, 0, SEEK_SET);

        unsigned char* temp_buffer = (unsigned char*)realloc(full_buffer, totalSize + moduleSize);
        if (!temp_buffer) {
            Console_Printf_Error("[Checksum] Failed to reallocate memory for module: %s", modulePath);
            fclose(moduleFile);
            free(moduleDir);
            free(full_buffer);
            return false;
        }
        full_buffer = temp_buffer;

        if (fread(full_buffer + totalSize, 1, moduleSize, moduleFile) != moduleSize) {
            Console_Printf_Error("[Checksum] Failed to read module: %s", modulePath);
            fclose(moduleFile);
            free(moduleDir);
            free(full_buffer);
            return false;
        }

        fclose(moduleFile);
        if (i == 0) {
            engineModuleSize = moduleSize;
        }
        totalSize += moduleSize;
    }
    free(moduleDir);

    if (totalSize == 0) {
        return false;
    }

    long checksumStructFileOffset = -1;
    for (long i = 0; i <= engineModuleSize - (long)sizeof(EmbeddedChecksum); ++i) {
        if (memcmp(full_buffer + i, &g_EmbeddedChecksum.signature, sizeof(uint32_t)) == 0) {
            checksumStructFileOffset = i;
            break;
        }
    }

    if (checksumStructFileOffset == -1) {
        free(full_buffer);
        return false;
    }

    uint32_t storedChecksum;
    memcpy(&storedChecksum, full_buffer + checksumStructFileOffset + offsetof(EmbeddedChecksum, checksum), sizeof(uint32_t));

    uint32_t zero = 0;
    memcpy(full_buffer + checksumStructFileOffset + offsetof(EmbeddedChecksum, checksum), &zero, sizeof(uint32_t));

    crc32_init_table();
    uint32_t calculatedChecksum = crc32_calculate(full_buffer, totalSize);

    free(full_buffer);

    if (storedChecksum != calculatedChecksum) {
        return false;
    }

    return true;
}