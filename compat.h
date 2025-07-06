/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef COMPAT_H
#define COMPAT_H

#ifdef __cplusplus
    #include <cctype>
#else
    #include <ctype.h>
#endif

#if defined(_WIN32)
    #define PLATFORM_WINDOWS
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
#else
    #error "Unsupported platform detected. This engine is designed for Windows and Linux."
#endif

#ifdef PLATFORM_LINUX
    #define _stricmp strcasecmp
    #define _strdup strdup
    #define _mkdir(path) mkdir(path, 0755)
#endif

#ifdef PLATFORM_WINDOWS
    #pragma comment(lib, "ws2_32.lib")
#endif

static const char* _stristr(const char* haystack, const char* needle) {
    if (!needle || !*needle) {
        return haystack;
    }
    for (; *haystack; ++haystack) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            const char* h = haystack;
            const char* n = needle;
            while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
                h++;
                n++;
            }
            if (!*n) {
                return haystack;
            }
        }
    }
    return NULL;
}

#endif // COMPAT_H