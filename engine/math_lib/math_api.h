/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef MATH_API_H
#define MATH_API_H

#ifdef PLATFORM_WINDOWS
    #ifdef MATH_DLL_EXPORTS
        #define MATH_API __declspec(dllexport)
    #else
        #define MATH_API __declspec(dllimport)
    #endif
#else
    #define MATH_API __attribute__((visibility("default")))
#endif

#endif // MATH_API_H