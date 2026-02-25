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
#pragma once
#ifndef COMPAT_H
#define COMPAT_H

//----------------------------------------//
// Brief: Compatibility header to properly support linux & some general functions
//----------------------------------------//

#include <stdio.h>
#ifdef __cplusplus
    #include "includes.h"
#endif

//#define GAME_RELEASE 1

//#define BRANCH_PUBLIC
#define BRANCH_NOCTURNE

#if defined(BRANCH_PUBLIC)
    #define BRANCH_NAME "PUBLIC"
#elif defined(BRANCH_NOCTURNE)
    #define BRANCH_NAME "Nocturne Descent"
#else
    #define BRANCH_NAME "???"
#endif

#if defined(_WIN32)
    #define PLATFORM_WINDOWS
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h> 
    #include <direct.h> 
#elif defined(__linux__)
    #define PLATFORM_LINUX
    #include <strings.h> 
    #include <string.h>  
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#else
    #error "Unsupported platform"
#endif

#if defined(_WIN64) || defined(__x86_64__)
    #define ARCH_64BIT
#elif defined(_WIN32) || defined(__i386__)
    #define ARCH_32BIT
#else
    #error "Unknown architecture"
#endif

#if defined(_MSC_VER)
    #define COMPILER_MSVC
#elif defined(__GNUC__)
    #define COMPILER_GNU
#else
    #error "Unsupported compiler"
#endif

#ifdef PLATFORM_WINDOWS
    #define OS_STRING "Windows"
#else
    #define OS_STRING "Linux"
#endif

#ifdef ARCH_64BIT
    #define ARCH_STRING "x64 " OS_STRING
#else
    #define ARCH_STRING "x86 " OS_STRING
#endif

#ifdef PLATFORM_LINUX
    #define _stricmp strcasecmp
    #define _strnicmp strncasecmp
    #define _mkdir(path) mkdir(path, 0755)
#endif

#ifdef COMPILER_MSVC
    #define UNREACHABLE() __assume(0)
#else
    #define UNREACHABLE() __builtin_unreachable()
#endif

#ifdef PLATFORM_WINDOWS
    #pragma comment(lib, "ws2_32.lib")
#endif


#endif // COMPAT_H