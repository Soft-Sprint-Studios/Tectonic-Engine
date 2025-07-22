/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef LEVEL0_API_H
#define LEVEL0_API_H

#ifdef PLATFORM_WINDOWS
    #ifdef LEVEL0_DLL_EXPORTS
        #define LEVEL0_API __declspec(dllexport)
    #else
        #define LEVEL0_API __declspec(dllimport)
    #endif
#else
    #define LEVEL0_API __attribute__((visibility("default")))
#endif

#endif // LEVEL0_API_H