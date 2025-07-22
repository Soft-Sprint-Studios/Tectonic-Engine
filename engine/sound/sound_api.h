/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef SOUND_API_H
#define SOUND_API_H

#ifdef PLATFORM_WINDOWS
    #ifdef SOUND_DLL_EXPORTS
        #define SOUND_API __declspec(dllexport)
    #else
        #define SOUND_API __declspec(dllimport)
    #endif
#else
    #define SOUND_API __attribute__((visibility("default")))
#endif

#endif // SOUND_API_H