/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef MATERIALS_API_H
#define MATERIALS_API_H

#ifdef PLATFORM_WINDOWS
    #ifdef MATERIALS_DLL_EXPORTS
        #define MATERIALS_API __declspec(dllexport)
    #else
        #define MATERIALS_API __declspec(dllimport)
    #endif
#else
    #define MATERIALS_API __attribute__((visibility("default")))
#endif

#endif // MATERIALS_API_H