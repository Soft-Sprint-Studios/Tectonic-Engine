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
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <stddef.h>
#include <cstring>

#ifdef PLATFORM_WINDOWS
const char* g_module_names[] = {
    "engine.dll", "level0.dll", "level1.dll",
    "math_lib.dll", "physics.dll", "sound.dll",
    "materials.dll", "models.dll"
};
#else
const char* g_module_names[] = {
    "libengine.so", "liblevel0.so", "liblevel1.so",
    "libmath_lib.so", "libphysics.so", "libsound.so",
    "libmaterials.so", "libmodels.so"
};
#endif
const int g_num_modules = sizeof(g_module_names) / sizeof(g_module_names[0]);

static uint32_t crc_table[256];
static bool table_initialized = false;

void crc32_init_table() {
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

uint32_t crc32_calculate(const unsigned char* data, size_t size) {
    uint32_t crc = 0;
    crc = crc ^ ~0U;
    while (size--) {
        crc = crc_table[(crc ^ *data++) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ ~0U;
}

typedef struct {
    uint32_t signature;
    uint32_t checksum;
} EmbeddedChecksum;

string get_directory(const string& path) {
    size_t found = path.find_last_of("/\\");
    if (string::npos != found) {
        return path.substr(0, found + 1);
    }
    return "./";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "[Patcher] FATAL: No executable path provided." << endl;
        cerr << "Usage: " << argv[0] << " <path_to_engine_dll>" << endl;
        return 1;
    }
    string fileToPatchPath = argv[1];
    cout << "[Patcher] Target for patching: " << fileToPatchPath << endl;

    string buildDir = get_directory(fileToPatchPath);
    cout << "[Patcher] Using build directory: " << buildDir << endl;

    vector<unsigned char> fullBuffer;
    long engineDllSize = 0;

    for (int i = 0; i < g_num_modules; ++i) {
        string modulePath = buildDir + g_module_names[i];
        cout << "[Patcher] Reading module: " << modulePath << endl;

        ifstream moduleFile(modulePath, ios::binary | ios::ate);
        if (!moduleFile.is_open()) {
            cerr << "[Patcher] FATAL: Could not open module for reading: " << modulePath << endl;
            return 1;
        }

        streamsize moduleSize = moduleFile.tellg();
        moduleFile.seekg(0, ios::beg);

        size_t originalSize = fullBuffer.size();
        fullBuffer.resize(originalSize + moduleSize);

        if (!moduleFile.read(reinterpret_cast<char*>(fullBuffer.data() + originalSize), moduleSize)) {
            cerr << "[Patcher] FATAL: Failed to read module into buffer: " << modulePath << endl;
            moduleFile.close();
            return 1;
        }
        moduleFile.close();

        if (i == 0) {
            engineDllSize = moduleSize;
        }
        cout << "[Patcher] Appended " << moduleSize << " bytes. Total buffer size now: " << fullBuffer.size() << " bytes." << endl;
    }

    long checksumStructOffset = -1;
    for (long i = 0; i <= engineDllSize - (long)sizeof(EmbeddedChecksum); ++i) {
        EmbeddedChecksum current_val;
        memcpy(&current_val, &fullBuffer[i], sizeof(EmbeddedChecksum));
        if (current_val.signature == 0xBADF00D5) {
            checksumStructOffset = i;
            break;
        }
    }

    if (checksumStructOffset == -1) {
        cerr << "[Patcher] FATAL: Signature 0xBADF00D5 not found in binary." << endl;
        return 1;
    }
    cout << "[Patcher] Found signature at offset: " << checksumStructOffset << endl;

    long checksumValueOffset = checksumStructOffset + offsetof(EmbeddedChecksum, checksum);

    *(reinterpret_cast<uint32_t*>(&fullBuffer[checksumValueOffset])) = 0;

    crc32_init_table();
    uint32_t checksum = crc32_calculate(fullBuffer.data(), fullBuffer.size());

    cout << "[Patcher] Calculated checksum of combined files: 0x" << hex << checksum << dec << endl;

    fstream outFile(fileToPatchPath, ios::binary | ios::in | ios::out);
    if (!outFile.is_open()) {
        cerr << "[Patcher] FATAL: Could not open file for writing: " << fileToPatchPath << endl;
        return 1;
    }

    outFile.seekp(checksumValueOffset);
    outFile.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
    if (outFile.fail()) {
        cerr << "[Patcher] FATAL: Failed to write new checksum to file." << endl;
        outFile.close();
        return 1;
    }
    outFile.close();

    cout << "[Patcher] Successfully patched executable with new checksum." << endl;

    return 0;
}