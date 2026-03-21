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
#ifndef LIGHTMAPPER_INTERNAL_H
#define LIGHTMAPPER_INTERNAL_H

constexpr Float SHADOW_BIAS = 0.005f;
constexpr Int BLUR_RADIUS = 2;
constexpr Int NUM_AREA_LIGHT_SAMPLES = 16;
constexpr Int INDIRECT_SAMPLES_PER_POINT_BRUSHES = 64; // for lightmapped brushes and models
constexpr Int INDIRECT_SAMPLES_PER_POINT_MODELS = 512; // for vertex lit models
constexpr Int INDIRECT_SAMPLES_PER_POINT_AMBIENT_PROBES = 64;
constexpr Int INDIRECT_SAMPLES_PER_POINT_DECALS = 64;
constexpr Float LUXELS_PER_UNIT = 16.0f;
constexpr Float BLACK_THRESHOLD = 0.0001f;

#endif // LIGHTMAPPER_INTERNAL_H