/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef ENGINE_API_H
#define ENGINE_API_H

#include "compat.h"

#ifdef PLATFORM_WINDOWS
    #ifdef ENGINE_DLL_EXPORTS
        #define ENGINE_API __declspec(dllexport)
    #else
        #define ENGINE_API __declspec(dllimport)
    #endif
#else
    #define ENGINE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

ENGINE_API int Engine_Main(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif

#endif // ENGINE_API_H