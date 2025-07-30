/*
 * MIT License
 *
 * Copyright (c) 2025 Soft Sprint Studios
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
#ifndef DSP_REVERB_H
#define DSP_REVERB_H

//----------------------------------------//
// Brief: DSP Style reverbs
//----------------------------------------//

#include <stdbool.h>
#include <SDL_thread.h>
#include <SDL_mutex.h>
#include "sound_api.h"

#ifdef __cplusplus
extern "C" {
#endif

    SOUND_API void DSP_Reverb_Thread_Init(void);
    SOUND_API void DSP_Reverb_Thread_Shutdown(void);

    typedef enum {
        REVERB_PRESET_NONE,
        REVERB_PRESET_SMALL_ROOM,
        REVERB_PRESET_MEDIUM_ROOM,
        REVERB_PRESET_LARGE_ROOM,
        REVERB_PRESET_HALL,
        REVERB_PRESET_CAVE,
        REVERB_PRESET_COUNT
    } ReverbPreset;

    typedef struct {
        float roomSize;
        float damping;
        float wetLevel;
        float dryLevel;
        float width;
    } ReverbSettings;

    typedef struct {
        short* data;
        int num_samples;
    } ProcessedAudio;

    SOUND_API ReverbSettings DSP_Reverb_GetSettingsForPreset(ReverbPreset preset);

    SOUND_API ProcessedAudio DSP_Reverb_Process(const short* input, int num_samples, int sample_rate, const ReverbSettings* settings, bool wet_only);

#ifdef __cplusplus
}
#endif

#endif