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

void box_blur(std::vector<float>& out, const std::vector<float>& in, int width, int height, int radius)
{
    std::vector<float> temp(width * height);
    float scale = 1.0f / (2 * radius + 1);

    for (int y = 0; y < height; ++y) {
        float sum = 0;
        for (int x = -radius; x <= radius; ++x) {
            sum += in[y * width + std::clamp(x, 0, width - 1)];
        }
        for (int x = 0; x < width; ++x) {
            temp[y * width + x] = sum * scale;
            int prev_idx = std::clamp(x - radius, 0, width - 1);
            int next_idx = std::clamp(x + radius + 1, 0, width - 1);
            sum += in[y * width + next_idx] - in[y * width + prev_idx];
        }
    }

    for (int x = 0; x < width; ++x) {
        float sum = 0;
        for (int y = -radius; y <= radius; ++y) {
            sum += temp[std::clamp(y, 0, height - 1) * width + x];
        }
        for (int y = 0; y < height; ++y) {
            out[y * width + x] = sum * scale;
            int prev_idx = std::clamp(y - radius, 0, height - 1);
            int next_idx = std::clamp(y + radius + 1, 0, height - 1);
            sum += temp[next_idx * width + x] - temp[prev_idx * width + x];
        }
    }
}

uint32_t generate_seed_from_pos(const Vec3& pos)
{
    std::hash<float> hasher;
    uint32_t seed = hasher(pos.x);
    seed ^= hasher(pos.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hasher(pos.z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

std::string sanitize_filename(std::string input)
{
    std::transform(input.begin(), input.end(), input.begin(),
        [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-' ? c : '_'; });
    return input;
}

void apply_gaussian_blur(std::vector<float>& data, int width, int height, int channels)
{
    std::vector<float> temp_data(data.size());
    constexpr int KERNEL_SIZE = BLUR_RADIUS * 2 + 1;

    std::vector<float> kernel(KERNEL_SIZE);
    float sigma = static_cast<float>(BLUR_RADIUS) / 2.0f;
    float sum = 0.0f;
    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        int x = i - BLUR_RADIUS;
        kernel[i] = expf(-static_cast<float>(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }
    for (float& val : kernel)
    {
        val /= sum;
    }

    std::vector<float> totals(channels);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            std::fill(totals.begin(), totals.end(), 0.0f);
            for (int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
            {
                int sample_x = std::clamp(x + k, 0, width - 1);
                int src_idx = (y * width + sample_x) * channels;
                float weight = kernel[k + BLUR_RADIUS];
                for (int c = 0; c < channels; ++c)
                {
                    totals[c] += data[src_idx + c] * weight;
                }
            }
            int dst_idx = (y * width + x) * channels;
            for (int c = 0; c < channels; ++c)
            {
                temp_data[dst_idx + c] = totals[c];
            }
        }
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            std::fill(totals.begin(), totals.end(), 0.0f);
            for (int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
            {
                int sample_y = std::clamp(y + k, 0, height - 1);
                int src_idx = (sample_y * width + x) * channels;
                float weight = kernel[k + BLUR_RADIUS];
                for (int c = 0; c < channels; ++c)
                {
                    totals[c] += temp_data[src_idx + c] * weight;
                }
            }
            int dst_idx = (y * width + x) * channels;
            for (int c = 0; c < channels; ++c)
            {
                data[dst_idx + c] = totals[c];
            }
        }
    }
}

void apply_gaussian_blur(std::vector<unsigned char>& data, int width, int height, int channels)
{
    std::vector<unsigned char> temp_data(data.size());
    constexpr int KERNEL_SIZE = BLUR_RADIUS * 2 + 1;

    std::vector<float> kernel(KERNEL_SIZE);
    float sigma = static_cast<float>(BLUR_RADIUS) / 2.0f;
    float sum = 0.0f;
    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        int x = i - BLUR_RADIUS;
        kernel[i] = expf(-static_cast<float>(x * x) / (2.0f * sigma * sigma));
        sum += kernel[i];
    }
    for (float& val : kernel)
    {
        val /= sum;
    }

    std::vector<float> totals(channels);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            std::fill(totals.begin(), totals.end(), 0.0f);
            for (int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
            {
                int sample_x = std::clamp(x + k, 0, width - 1);
                int src_idx = (y * width + sample_x) * channels;
                float weight = kernel[k + BLUR_RADIUS];
                for (int c = 0; c < channels; ++c)
                {
                    totals[c] += data[src_idx + c] * weight;
                }
            }
            int dst_idx = (y * width + x) * channels;
            for (int c = 0; c < channels; ++c)
            {
                temp_data[dst_idx + c] = static_cast<unsigned char>(std::min(255.0f, totals[c]));
            }
        }
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            std::fill(totals.begin(), totals.end(), 0.0f);
            for (int k = -BLUR_RADIUS; k <= BLUR_RADIUS; ++k)
            {
                int sample_y = std::clamp(y + k, 0, height - 1);
                int src_idx = (sample_y * width + x) * channels;
                float weight = kernel[k + BLUR_RADIUS];
                for (int c = 0; c < channels; ++c)
                {
                    totals[c] += temp_data[src_idx + c] * weight;
                }
            }
            int dst_idx = (y * width + x) * channels;
            for (int c = 0; c < channels; ++c)
            {
                data[dst_idx + c] = static_cast<unsigned char>(std::min(255.0f, totals[c]));
            }
        }
    }
}

void apply_guided_filter(std::vector<float>& out_p, const std::vector<float>& in_p, const std::vector<float>& guide, int width, int height, int radius, float epsilon)
{
    int n_pixels = width * height;
    std::vector<float> guide_gray(n_pixels);
    for (int i = 0; i < n_pixels; ++i) {
        guide_gray[i] = (guide[i * 3 + 0] * 0.299f + guide[i * 3 + 1] * 0.587f + guide[i * 3 + 2] * 0.114f);
    }

    std::vector<float> mean_I(n_pixels);
    box_blur(mean_I, guide_gray, width, height, radius);

    std::vector<float> var_I(n_pixels);
    std::vector<float> I_sq(n_pixels);
    for (int i = 0; i < n_pixels; ++i) {
        I_sq[i] = guide_gray[i] * guide_gray[i];
    }
    box_blur(var_I, I_sq, width, height, radius);
    for (int i = 0; i < n_pixels; ++i) {
        var_I[i] -= mean_I[i] * mean_I[i];
    }

    out_p.resize(n_pixels * 3);
    std::vector<float> p_channel(n_pixels);
    std::vector<float> mean_p(n_pixels);
    std::vector<float> cov_Ip(n_pixels);
    std::vector<float> Ip(n_pixels);
    std::vector<float> a(n_pixels), b(n_pixels);
    std::vector<float> mean_a(n_pixels), mean_b(n_pixels);

    for (int c = 0; c < 3; ++c) {
        for (int i = 0; i < n_pixels; ++i) {
            p_channel[i] = in_p[i * 3 + c];
        }

        box_blur(mean_p, p_channel, width, height, radius);

        for (int i = 0; i < n_pixels; ++i) {
            Ip[i] = guide_gray[i] * p_channel[i];
        }
        box_blur(cov_Ip, Ip, width, height, radius);
        for (int i = 0; i < n_pixels; ++i) {
            cov_Ip[i] -= mean_I[i] * mean_p[i];
        }

        for (int i = 0; i < n_pixels; ++i) {
            a[i] = cov_Ip[i] / (var_I[i] + epsilon);
            b[i] = mean_p[i] - a[i] * mean_I[i];
        }

        box_blur(mean_a, a, width, height, radius);
        box_blur(mean_b, b, width, height, radius);

        for (int i = 0; i < n_pixels; ++i) {
            out_p[i * 3 + c] = mean_a[i] * guide_gray[i] + mean_b[i];
        }
    }
}

#endif // ARCH_64BIT