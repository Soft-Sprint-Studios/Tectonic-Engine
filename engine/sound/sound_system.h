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
