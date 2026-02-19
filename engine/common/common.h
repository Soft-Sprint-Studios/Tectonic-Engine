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

#include "common_inline.h"

namespace Common {
    constexpr Double M_PI = 3.14159265358979323846;
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
    constexpr Int MAP_VERSION = 22;

    Int GetBuildNumber();
    Char* trim(Char* str);
    const Char* _stristr(const Char* haystack, const Char* needle);
}

#endif // COMMON_H