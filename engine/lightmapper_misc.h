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
#ifndef LIGHTMAPPER_MISC_H
#define LIGHTMAPPER_MISC_H

#ifndef __cplusplus
#error lightmapper_misc.h cannot be included from a c file
#endif

#ifdef ARCH_64BIT

#include "math_lib.h"
#include <vector>
#include <string>
#include <cstdint>

uint32_t generate_seed_from_pos(const Vec3& pos);

string sanitize_filename(string input);

void apply_gaussian_blur(vector<float>& data, int width, int height, int channels);
void apply_gaussian_blur(vector<unsigned char>& data, int width, int height, int channels);
void apply_guided_filter(vector<float>& out_p, const vector<float>& in_p, const vector<float>& guide, int width, int height, int radius, float epsilon);

#endif // ARCH_64BIT
#endif // LIGHTMAPPER_MISC_H