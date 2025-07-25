/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef MODELS_API_H
#define MODELS_API_H

#ifdef PLATFORM_WINDOWS
    #ifdef MODELS_DLL_EXPORTS
        #define MODELS_API __declspec(dllexport)
    #else
        #define MODELS_API __declspec(dllimport)
    #endif
#else
    #define MODELS_API __attribute__((visibility("default")))
#endif

#endif // MODELS_API_H