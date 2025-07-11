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
#ifdef PLATFORM_LINUX
#include <dlfcn.h>
#endif

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
const EmbeddedChecksum g_EmbeddedChecksum = { 0xBADF00D5, 0 };
#else
__declspec(allocate(".checksum_section"))
const EmbeddedChecksum g_EmbeddedChecksum = { 0xBADF00D5, 0 };
#endif

bool Checksum_Verify(const char* exePath) {
    FILE* file = fopen(exePath, "rb");
    if (!file) return false;

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* buffer = (unsigned char*)malloc(fileSize);
    if (!buffer) {
        fclose(file);
        return false;
    }

    if (fread(buffer, 1, fileSize, file) != fileSize) {
        free(buffer);
        fclose(file);
        return false;
    }
    fclose(file);

#ifdef PLATFORM_WINDOWS
    HMODULE hModule = GetModuleHandle(NULL);
    const unsigned char* struct_in_memory = (const unsigned char*)&g_EmbeddedChecksum;
    long memory_offset_of_struct = struct_in_memory - (const unsigned char*)hModule;
#else
    Dl_info info;
    if (!dladdr((void*)&g_EmbeddedChecksum, &info)) {
        free(buffer);
        return false;
    }
    const unsigned char* struct_in_memory = (const unsigned char*)&g_EmbeddedChecksum;
    long memory_offset_of_struct = struct_in_memory - (const unsigned char*)info.dli_fbase;
#endif

    long checksumStructFileOffset = -1;
    for (long i = 0; i <= fileSize - (long)sizeof(EmbeddedChecksum); ++i) {
        if (memcmp(buffer + i, &g_EmbeddedChecksum.signature, sizeof(uint32_t)) == 0) {
            checksumStructFileOffset = i;
            break;
        }
    }

    if (checksumStructFileOffset == -1) {
        free(buffer);
        return false;
    }

    uint32_t storedChecksum;
    memcpy(&storedChecksum, buffer + checksumStructFileOffset + offsetof(EmbeddedChecksum, checksum), sizeof(uint32_t));

    uint32_t zero = 0;
    memcpy(buffer + checksumStructFileOffset + offsetof(EmbeddedChecksum, checksum), &zero, sizeof(uint32_t));

    crc32_init_table();
    uint32_t calculatedChecksum = crc32_calculate(buffer, fileSize);

    free(buffer);

    if (storedChecksum != calculatedChecksum) {
        return false;
    }

    Console_Printf("Checksum OK (0x%08X)\n", storedChecksum);
    return true;
}
