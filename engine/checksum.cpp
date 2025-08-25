/*
 * MIT License
 *
 * Copyright (c) 2025 Soft Sprint Studios
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
#include "checksum.h"
#include "gl_console.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class Checksum {
private:
#ifdef PLATFORM_WINDOWS
    static constexpr const char* module_names[8] = { "engine.dll", "level0.dll", "level1.dll", "math_lib.dll",
                                                     "physics.dll", "sound.dll", "materials.dll", "models.dll" };
#else
    static constexpr const char* module_names[8] = { "libengine.so", "liblevel0.so", "liblevel1.so", "libmath_lib.so",
                                                     "libphysics.so", "libsound.so", "libmaterials.so", "libmodels.so" };
#endif
    static constexpr int num_modules = 8;

    static uint32_t crc_table[256];
    static bool table_initialized;

    static void crc32_init_table() {
        if (table_initialized) return;
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int j = 0; j < 8; j++) {
                c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
            }
            crc_table[i] = c;
        }
        table_initialized = true;
    }

    static uint32_t crc32_calculate(const void* data, size_t size) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        uint32_t crc = ~0U;
        while (size--) {
            crc = crc_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ ~0U;
    }

    static char* get_module_directory() {
        char path[1024];
#ifdef PLATFORM_WINDOWS
        HMODULE hModule = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(get_module_directory), &hModule);
        GetModuleFileNameA(hModule, path, sizeof(path));
#else
        Dl_info info;
        dladdr(reinterpret_cast<void*>(get_module_directory), &info);
        strncpy(path, info.dli_fname, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
#endif
        const char* last_slash = strrchr(path, '/');
        const char* last_bslash = strrchr(path, '\\');
        const char* last_separator = (last_slash > last_bslash) ? last_slash : last_bslash;

        if (last_separator) {
            size_t len = last_separator - path + 1;
            char* dir = static_cast<char*>(malloc(len + 1));
            strncpy(dir, path, len);
            dir[len] = '\0';
            return dir;
        }
        return strdup("./");
    }

public:
    static bool Verify(const char* exePath) {
        unsigned char* full_buffer = nullptr;
        long totalSize = 0;
        long engineModuleSize = 0;

        char* moduleDir = get_module_directory();
        if (!moduleDir) return false;

        for (int i = 0; i < num_modules; ++i) {
            char modulePath[512];
            snprintf(modulePath, sizeof(modulePath), "%s%s", moduleDir, module_names[i]);

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

            unsigned char* temp_buffer = static_cast<unsigned char*>(realloc(full_buffer, totalSize + moduleSize));
            if (!temp_buffer) {
                Console_Printf_Error("[Checksum] Failed to reallocate memory for module: %s", modulePath);
                fclose(moduleFile);
                free(moduleDir);
                free(full_buffer);
                return false;
            }
            full_buffer = temp_buffer;

            if (fread(full_buffer + totalSize, 1, moduleSize, moduleFile) != static_cast<size_t>(moduleSize)) {
                Console_Printf_Error("[Checksum] Failed to read module: %s", modulePath);
                fclose(moduleFile);
                free(moduleDir);
                free(full_buffer);
                return false;
            }

            fclose(moduleFile);
            if (i == 0) engineModuleSize = moduleSize;
            totalSize += moduleSize;
        }

        free(moduleDir);
        if (totalSize == 0) return false;

        long checksumStructFileOffset = -1;
        for (long i = 0; i <= engineModuleSize - static_cast<long>(sizeof(EmbeddedChecksum)); ++i) {
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
        return storedChecksum == calculatedChecksum;
    }
};

uint32_t Checksum::crc_table[256];
bool Checksum::table_initialized = false;

// TODO: convert engine.c to cpp so we dont need this extern C anymore

extern "C" bool Checksum_Verify(const char* exePath) {
    return Checksum::Verify(exePath);
}

#ifdef PLATFORM_LINUX
__attribute__((used))
__attribute__((section(".checksum_section")))
EmbeddedChecksum g_EmbeddedChecksum = { 0xBADF00D5, 0 };
#else
__declspec(allocate(".checksum_section"))
EmbeddedChecksum g_EmbeddedChecksum = { 0xBADF00D5, 0 };
#endif
