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

#include <stdbool.h>
#include <stdio.h>

//#define GAME_RELEASE 1
#define ENABLE_CHECKSUM 1
//#define DISABLE_DEBUGGER 1

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
    #include <fcntl.h>
    #include <errno.h>
#else
    #error "Unsupported platform"
#endif

#if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
    #define ARCH_64BIT
#elif defined(_WIN32) || defined(__i386__) || defined(__arm__) || defined(__arm)
    #define ARCH_32BIT
#else
    #error "Unknown architecture"
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

static char* trim(char* str) {
    char* end;

    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    end = str + strlen(str) - 1;

    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    end[1] = '\0';

    return str;
}

#ifdef DISABLE_DEBUGGER
static bool CheckForDebugger(void) {
#ifdef PLATFORM_WINDOWS
    return IsDebuggerPresent();
#else
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) {
        return false;
    }
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "TracerPid:", 10) == 0) {
            int tracer_pid = 0;
            sscanf(line + 10, "%d", &tracer_pid);
            fclose(f);
            return tracer_pid != 0;
        }
    }
    fclose(f);
    return false;
#endif
}
#endif

static int g_build_number = -1;

static int get_month_from_name(const char* month_name) {
    if (strcmp(month_name, "Jan") == 0) return 1;
    if (strcmp(month_name, "Feb") == 0) return 2;
    if (strcmp(month_name, "Mar") == 0) return 3;
    if (strcmp(month_name, "Apr") == 0) return 4;
    if (strcmp(month_name, "May") == 0) return 5;
    if (strcmp(month_name, "Jun") == 0) return 6;
    if (strcmp(month_name, "Jul") == 0) return 7;
    if (strcmp(month_name, "Aug") == 0) return 8;
    if (strcmp(month_name, "Sep") == 0) return 9;
    if (strcmp(month_name, "Oct") == 0) return 10;
    if (strcmp(month_name, "Nov") == 0) return 11;
    if (strcmp(month_name, "Dec") == 0) return 12;
    return 0;
}

static int days_from_origin(int year, int month, int day) {
    if (month < 3) {
        year--;
        month += 12;
    }
    return 365 * year + year / 4 - year / 100 + year / 400 + (153 * month - 457) / 5 + day - 306;
}

static int Compat_GetBuildNumber() {
    if (g_build_number == -1) {
        char month_str[4];
        int day, year;
        sscanf(__DATE__, "%s %d %d", month_str, &day, &year);
        int month = get_month_from_name(month_str);

        int days_current = days_from_origin(year, month, day);
        int days_ref = days_from_origin(2025, 6, 1);

        g_build_number = days_current - days_ref;
        if (g_build_number < 0) {
            g_build_number = 0;
        }
    }
    return g_build_number;
}

#endif // COMPAT_H