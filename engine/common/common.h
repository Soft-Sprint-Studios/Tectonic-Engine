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
#ifndef COMMON_H
#define COMMON_H

#include <cstdio>
#include <cctype>
#include <cstring>

namespace Common {
    constexpr Double PI = 3.14159265358979323846;
    constexpr Int LIGHTMAPPADDING = 2;

    constexpr Int MAX_LIGHTS = 256;
    constexpr Int MAX_BRUSHES = 8192;
    constexpr Int MAX_MODELS = 8192;
    constexpr Int MAX_DECALS = 8192;
    constexpr Int MAX_SOUNDS = 2048;
    constexpr Int MAX_PARTICLE_EMITTERS = 2048;
    constexpr Int MAX_SPRITES = 8192;
    constexpr Int MAX_VIDEO_PLAYERS = 32;
    constexpr Int MAX_PARALLAX_ROOMS = 128;
    constexpr Int MAX_BRUSH_VERTS = 32768;
    constexpr Int MAX_BRUSH_FACES = 16384;
    constexpr Int MAX_LOGIC_ENTITIES = 8192;
    constexpr Int MAX_ENTITY_PROPERTIES = 32;

    constexpr Int MIN_MAP_VERSION = 18;
    constexpr Int MAP_VERSION = 23;

    static Int g_build_number = -1;

    static Int get_month_from_name(const Char* month_name) {
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

    static Int days_from_origin(Int year, Int month, Int day) {
        if (month < 3) {
            year--;
            month += 12;
        }
        return 365 * year + year / 4 - year / 100 + year / 400 + (153 * month - 457) / 5 + day - 306;
    }

    inline Int GetBuildNumber() {
        if (g_build_number == -1) {
            Char month_str[4];
            Int day, year;
            sscanf(__DATE__, "%s %d %d", month_str, &day, &year);
            Int month = get_month_from_name(month_str);

            Int days_current = days_from_origin(year, month, day);
            Int days_ref = days_from_origin(2025, 6, 1);

            g_build_number = days_current - days_ref;
            if (g_build_number < 0)
                g_build_number = 0;
        }
        return g_build_number;
    }

    inline Char* trim(Char* str) {
        Char* end;

        while (isspace((Uchar)*str)) str++;

        if (*str == '\0')
            return str;

        end = str + strlen(str) - 1;

        while (end > str && isspace((Uchar)*end))
            end--;

        end[1] = '\0';

        return str;
    }

    inline const Char* _stristr(const Char* haystack, const Char* needle) {
        if (!needle || !*needle)
            return haystack;
        for (; *haystack; ++haystack) {
            if (tolower((Uchar)*haystack) == tolower((Uchar)*needle)) {
                const Char* h = haystack;
                const Char* n = needle;
                while (*h && *n && tolower((Uchar)*h) == tolower((Uchar)*n)) {
                    h++;
                    n++;
                }
                if (!*n)
                    return haystack;
            }
        }
        return nullptr;
    }
}

#endif // COMMON_H