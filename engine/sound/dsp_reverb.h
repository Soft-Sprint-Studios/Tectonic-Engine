/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
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