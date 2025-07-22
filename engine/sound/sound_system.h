/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef SOUND_SYSTEM_H
#define SOUND_SYSTEM_H

//----------------------------------------//
// Brief: Sounds, this calls dsp_reverb
//----------------------------------------//

#include "math_lib.h"
#include "gl_console.h"
#include <stdbool.h>
#include "dsp_reverb.h"
#include "sound_api.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        unsigned int bufferID;
    } Sound;

    typedef struct {
        unsigned int drySourceID;
        unsigned int wetSourceID;
    } PlayingSound;

    SOUND_API bool SoundSystem_Init();
    SOUND_API void SoundSystem_Shutdown();
    SOUND_API void SoundSystem_UpdateListener(Vec3 position, Vec3 forward, Vec3 up);
    SOUND_API void SoundSystem_SetCurrentReverb(ReverbPreset preset);
    SOUND_API unsigned int SoundSystem_LoadSound(const char* path);
    SOUND_API unsigned int SoundSystem_PlaySound(unsigned int bufferID, Vec3 position, float volume, float pitch, float maxDistance, bool looping);
    SOUND_API void SoundSystem_SetSourcePosition(unsigned int sourceID, Vec3 position);
    SOUND_API void SoundSystem_SetSourceProperties(unsigned int sourceID, float volume, float pitch, float maxDistance);
    SOUND_API void SoundSystem_SetSourceLooping(unsigned int sourceID, bool loop);
    SOUND_API void SoundSystem_SetMasterVolume(float volume);
    SOUND_API void SoundSystem_DeleteSource(unsigned int sourceID);
    SOUND_API void SoundSystem_DeleteBuffer(unsigned int bufferID);

#ifdef __cplusplus
}
#endif

#endif // SOUND_SYSTEM_H
