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
#ifdef ARCH_64BIT
#include "lightmapper_misc.h"
#include "lightmapper_internal.h"
#include <algorithm>
#include <cmath>
#include <functional>

void box_blur(vector<Float>& out, const vector<Float>& in, Int width, Int height, Int radius)
{
    vector<Float> temp(width * height);
    Float scale = 1.0f / (2 * radius + 1);

    for (Int y = 0; y < height; ++y) {
        Float sum = 0;
        for (Int x = -radius; x <= radius; ++x) {
            sum += in[y * width + clamp(x, 0, width - 1)];
        }
        for (Int x = 0; x < width; ++x) {
            temp[y * width + x] = sum * scale;
            Int prev_idx = clamp(x - radius, 0, width - 1);
            Int next_idx = clamp(x + radius + 1, 0, width - 1);
            sum += in[y * width + next_idx] - in[y * width + prev_idx];
        }
    }

    for (Int x = 0; x < width; ++x) {
        Float sum = 0;
        for (Int y = -radius; y <= radius; ++y) {
            sum += temp[clamp(y, 0, height - 1) * width + x];
        }
        for (Int y = 0; y < height; ++y) {
            out[y * width + x] = sum * scale;
            Int prev_idx = clamp(y - radius, 0, height - 1);
            Int next_idx = clamp(y + radius + 1, 0, height - 1);
            sum += temp[next_idx * width + x] - temp[prev_idx * width + x];
        }
    }
}

uint32_t generate_seed_from_pos(const Vec3& pos)
{
    hash<Float> hasher;
    uint32_t seed = hasher(pos.x);
    seed ^= hasher(pos.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hasher(pos.z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

string sanitize_filename(string input)
{
    transform(input.begin(), input.end(), input.begin(),
        [](Uchar c) { return isalnum(c) || c == '_' || c == '-' ? c : '_'; });
    return input;
}

void apply_gaussian_blur(vector<Float>& data, Int width, Int height, Int channels)
{
    vector<Float> temp_data(data.size());
    constexpr Int KERNEL_SIZE = BLUR_RADIUS * 2 + 1;

    vector<Float> kernel(KERNEL_SIZE);
    Float sigma = static_cast<Float>(BLUR_RADIUS) / 2.0f;
    Float sum = 0.0f;
    for (Int i = 0; i < KERNEL_SIZE; ++i)
    {
        Int x = i - BLUR_RADIUS;
        kernel[i] = expf(-static_cast<Float>(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }
    for (Float& val : kernel)
    {
        val /= sum;
    }

    vector<Float> totals(channels);
    for (Int y = 0; y < height; ++y)
    {
        for (Int x = 0; x < width; ++x)
        {
            fill(totals.begin(), totals.end(), 0.0f);
            for (Int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
            {
                Int sample_x = clamp(x + k, 0, width - 1);
                Int src_idx = (y * width + sample_x) * channels;
                Float weight = kernel[k + BLUR_RADIUS];
                for (Int c = 0; c < channels; ++c)
                {
                    totals[c] += data[src_idx + c] * weight;
                }
            }
            Int dst_idx = (y * width + x) * channels;
            for (Int c = 0; c < channels; ++c)
            {
                temp_data[dst_idx + c] = totals[c];
            }
        }
    }

    for (Int y = 0; y < height; ++y)
    {
        for (Int x = 0; x < width; ++x)
        {
            fill(totals.begin(), totals.end(), 0.0f);
            for (Int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
            {
                Int sample_y = clamp(y + k, 0, height - 1);
                Int src_idx = (sample_y * width + x) * channels;
                Float weight = kernel[k + BLUR_RADIUS];
                for (Int c = 0; c < channels; ++c)
                {
                    totals[c] += temp_data[src_idx + c] * weight;
                }
            }
            Int dst_idx = (y * width + x) * channels;
            for (Int c = 0; c < channels; ++c)
            {
                data[dst_idx + c] = totals[c];
            }
        }
    }
}

void apply_gaussian_blur(vector<Uchar>& data, Int width, Int height, Int channels)
{
    vector<Uchar> temp_data(data.size());
    constexpr Int KERNEL_SIZE = BLUR_RADIUS * 2 + 1;

    vector<Float> kernel(KERNEL_SIZE);
    Float sigma = static_cast<Float>(BLUR_RADIUS) / 2.0f;
    Float sum = 0.0f;
    for (Int i = 0; i < KERNEL_SIZE; ++i)
    {
        Int x = i - BLUR_RADIUS;
        kernel[i] = expf(-static_cast<Float>(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }
    for (Float& val : kernel)
    {
        val /= sum;
    }

    vector<Float> totals(channels);
    for (Int y = 0; y < height; ++y)
    {
        for (Int x = 0; x < width; ++x)
        {
            fill(totals.begin(), totals.end(), 0.0f);
            for (Int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
            {
                Int sample_x = clamp(x + k, 0, width - 1);
                Int src_idx = (y * width + sample_x) * channels;
                Float weight = kernel[k + BLUR_RADIUS];
                for (Int c = 0; c < channels; ++c)
                {
                    totals[c] += data[src_idx + c] * weight;
                }
            }
            Int dst_idx = (y * width + x) * channels;
            for (Int c = 0; c < channels; ++c)
            {
                temp_data[dst_idx + c] = static_cast<Uchar>(min(255.0f, totals[c]));
            }
        }
    }

    for (Int y = 0; y < height; ++y)
    {
        for (Int x = 0; x < width; ++x)
        {
            fill(totals.begin(), totals.end(), 0.0f);
            for (Int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
            {
                Int sample_y = clamp(y + k, 0, height - 1);
                Int src_idx = (sample_y * width + x) * channels;
                Float weight = kernel[k + BLUR_RADIUS];
                for (Int c = 0; c < channels; ++c)
                {
                    totals[c] += temp_data[src_idx + c] * weight;
                }
            }
            Int dst_idx = (y * width + x) * channels;
            for (Int c = 0; c < channels; ++c)
            {
                data[dst_idx + c] = static_cast<Uchar>(min(255.0f, totals[c]));
            }
        }
    }
}

void apply_guided_filter(vector<Float>& out_p, const vector<Float>& in_p, const vector<Float>& guide, Int width, Int height, Int radius, Float epsilon)
{
    Int n_pixels = width * height;
    vector<Float> guide_gray(n_pixels);
    for (Int i = 0; i < n_pixels; ++i) {
        guide_gray[i] = (guide[i * 3 + 0] * 0.299f + guide[i * 3 + 1] * 0.587f + guide[i * 3 + 2] * 0.114f);
    }

    vector<Float> mean_I(n_pixels);
    box_blur(mean_I, guide_gray, width, height, radius);

    vector<Float> var_I(n_pixels);
    vector<Float> I_sq(n_pixels);
    for (Int i = 0; i < n_pixels; ++i) {
        I_sq[i] = guide_gray[i] * guide_gray[i];
    }
    box_blur(var_I, I_sq, width, height, radius);
    for (Int i = 0; i < n_pixels; ++i) {
        var_I[i] -= mean_I[i] * mean_I[i];
    }

    out_p.resize(n_pixels * 3);
    vector<Float> p_channel(n_pixels);
    vector<Float> mean_p(n_pixels);
    vector<Float> cov_Ip(n_pixels);
    vector<Float> Ip(n_pixels);
    vector<Float> a(n_pixels), b(n_pixels);
    vector<Float> mean_a(n_pixels), mean_b(n_pixels);

    for (Int c = 0; c < 3; ++c) {
        for (Int i = 0; i < n_pixels; ++i) {
            p_channel[i] = in_p[i * 3 + c];
        }

        box_blur(mean_p, p_channel, width, height, radius);

        for (Int i = 0; i < n_pixels; ++i) {
            Ip[i] = guide_gray[i] * p_channel[i];
        }
        box_blur(cov_Ip, Ip, width, height, radius);
        for (Int i = 0; i < n_pixels; ++i) {
            cov_Ip[i] -= mean_I[i] * mean_p[i];
        }

        for (Int i = 0; i < n_pixels; ++i) {
            a[i] = cov_Ip[i] / (var_I[i] + epsilon);
            b[i] = mean_p[i] - a[i] * mean_I[i];
        }

        box_blur(mean_a, a, width, height, radius);
        box_blur(mean_b, b, width, height, radius);

        for (Int i = 0; i < n_pixels; ++i) {
            out_p[i * 3 + c] = mean_a[i] * guide_gray[i] + mean_b[i];
        }
    }
}

#endif // ARCH_64BIT