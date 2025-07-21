/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <stddef.h>
#include <cstring>

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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "[Patcher] FATAL: No executable path provided." << std::endl;
        std::cerr << "Usage: " << argv[0] << " <path_to_executable>" << std::endl;
        return 1;
    }
    std::string filePath = argv[1];
    std::cout << "[Patcher] Attempting to patch: " << filePath << std::endl;

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[Patcher] FATAL: Could not open file for reading: " << filePath << std::endl;
        return 1;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "[Patcher] FATAL: Failed to read the entire file into buffer." << std::endl;
        file.close();
        return 1;
    }
    file.close();
    std::cout << "[Patcher] File size: " << size << " bytes." << std::endl;

    long checksumStructOffset = -1;
    for (long i = 0; i <= size - (long)sizeof(EmbeddedChecksum); ++i) {
        EmbeddedChecksum current_val;
        memcpy(&current_val, &buffer[i], sizeof(EmbeddedChecksum));
        if (current_val.signature == 0xBADF00D5) {
            checksumStructOffset = i;
            break;
        }
    }

    if (checksumStructOffset == -1) {
        std::cerr << "[Patcher] FATAL: Signature 0xBADF00D5 not found in binary." << std::endl;
        return 1;
    }
    std::cout << "[Patcher] Found signature at file offset: " << checksumStructOffset << std::endl;

    long checksumValueOffset = checksumStructOffset + offsetof(EmbeddedChecksum, checksum);

    *(reinterpret_cast<uint32_t*>(&buffer[checksumValueOffset])) = 0;

    crc32_init_table();
    uint32_t checksum = crc32_calculate(buffer.data(), buffer.size());

    std::cout << "[Patcher] Calculated checksum of zeroed file: 0x" << std::hex << checksum << std::dec << std::endl;

    std::fstream outFile(filePath, std::ios::binary | std::ios::in | std::ios::out);
    if (!outFile.is_open()) {
        std::cerr << "[Patcher] FATAL: Could not open file for writing: " << filePath << std::endl;
        return 1;
    }

    outFile.seekp(checksumValueOffset);
    outFile.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
    if (outFile.fail()) {
        std::cerr << "[Patcher] FATAL: Failed to write new checksum to file." << std::endl;
        outFile.close();
        return 1;
    }
    outFile.close();

    std::cout << "[Patcher] Successfully patched executable with new checksum." << std::endl;

    return 0;
}