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


typedef struct {
    Uint drySourceID;
    Uint wetSourceID;
} PlayingSound;

namespace Sound {
    SOUND_API Bool SoundSystem_Init();
    SOUND_API void SoundSystem_Shutdown();
    SOUND_API void SoundSystem_UpdateListener(Vec3 position, Vec3 forward, Vec3 up);
    SOUND_API void SoundSystem_SetCurrentReverb(ReverbPreset preset);
    SOUND_API Uint SoundSystem_LoadSound(const Char* path);
    SOUND_API Uint SoundSystem_PlaySound(Uint bufferID, Vec3 position, Float volume, Float pitch, Float maxDistance, Bool looping);
    SOUND_API void SoundSystem_SetSourcePosition(Uint sourceID, Vec3 position);
    SOUND_API void SoundSystem_SetSourceProperties(Uint sourceID, Float volume, Float pitch, Float maxDistance);
    SOUND_API void SoundSystem_SetSourceIsGlobal(Uint sourceID, Bool is_global);
    SOUND_API void SoundSystem_SetSourceLooping(Uint sourceID, Bool loop);
    SOUND_API void SoundSystem_SetMasterVolume(Float volume);
    SOUND_API void SoundSystem_DeleteSource(Uint sourceID);
    SOUND_API void SoundSystem_DeleteBuffer(Uint bufferID);
    SOUND_API void SoundSystem_Update(void);
}


#endif // SOUND_SYSTEM_H
