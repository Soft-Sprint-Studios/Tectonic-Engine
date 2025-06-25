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

#include "math_lib.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        unsigned int bufferID;
    } Sound;

    typedef struct {
        unsigned int sourceID;
    } PlayingSound;


    bool SoundSystem_Init();
    void SoundSystem_Shutdown();
    void SoundSystem_UpdateListener(Vec3 position, Vec3 forward, Vec3 up);
    unsigned int SoundSystem_LoadWAV(const char* path);
    unsigned int SoundSystem_PlaySound(unsigned int bufferID, Vec3 position, float volume, float pitch, float maxDistance, bool looping);
    void SoundSystem_SetSourcePosition(unsigned int sourceID, Vec3 position);
    void SoundSystem_SetSourceProperties(unsigned int sourceID, float volume, float pitch, float maxDistance);
    void SoundSystem_DeleteSource(unsigned int sourceID);
    void SoundSystem_DeleteBuffer(unsigned int bufferID);

#ifdef __cplusplus
}
#endif

#endif // SOUND_SYSTEM_H