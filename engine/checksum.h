/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef CHECKSUM_H
#define CHECKSUM_H

//----------------------------------------//
// Brief: Checksum that runs at start of game
//----------------------------------------//

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        uint32_t signature;
        uint32_t checksum;
    } EmbeddedChecksum;

#ifdef PLATFORM_LINUX
    __attribute__((used))
        __attribute__((section(".checksum_section")))
        extern EmbeddedChecksum g_EmbeddedChecksum;

#else
#pragma section(".checksum_section", read, write)
    __declspec(allocate(".checksum_section"))
        extern EmbeddedChecksum g_EmbeddedChecksum;
#endif

    bool Checksum_Verify(const char* exePath);

#ifdef __cplusplus
}
#endif

#endif // CHECKSUM_H