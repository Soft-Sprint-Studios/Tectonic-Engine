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
#include "sound_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <AL/al.h>
#include <AL/alc.h>
#include "minimp3.h"

constexpr int MAX_WET_CACHE_ENTRIES = 256;
constexpr int MAX_PLAYING_SOUNDS = 512;
constexpr int MAX_BUFFERS = 1024;

typedef struct {
    ALuint bufferID;
    void* pcmData;
    Uint dataSize;
    ALenum format;
    ALsizei freq;
} BufferData;

static BufferData g_buffers[MAX_BUFFERS];
static Int g_buffer_count = 0;

static ALCdevice* g_sound_device = nullptr;
static ALCcontext* g_sound_context = nullptr;
static ReverbPreset g_current_reverb_preset = REVERB_PRESET_NONE;

typedef struct {
    ALuint dryBufferID;
    ReverbPreset preset;
    ALuint wetBufferID;
} WetBufferCacheEntry;

static WetBufferCacheEntry g_wet_buffer_cache[MAX_WET_CACHE_ENTRIES];
static Int g_wet_cache_count = 0;

typedef struct {
    ALuint drySourceID;
    ALuint wetSourceID;
} PlayingSourceLink;

static PlayingSourceLink g_playing_source_links[MAX_PLAYING_SOUNDS];
static Int g_playing_link_count = 0;

namespace Sound {
    Bool SoundSystem_Init() {
        g_sound_device = alcOpenDevice(nullptr);
        if (!g_sound_device) return false;

        g_sound_context = alcCreateContext(g_sound_device, nullptr);
        if (!g_sound_context) {
            alcCloseDevice(g_sound_device);
            return false;
        }

        if (!alcMakeContextCurrent(g_sound_context)) {
            alcDestroyContext(g_sound_context);
            alcCloseDevice(g_sound_device);
            return false;
        }

        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

        return true;
    }

    void SoundSystem_Shutdown() {
        for (Int i = 0; i < g_buffer_count; i++) {
            delete[](Uchar*)g_buffers[i].pcmData;
            alDeleteBuffers(1, &g_buffers[i].bufferID);
        }
        g_buffer_count = 0;
        g_wet_cache_count = 0;
        g_playing_link_count = 0;

        if (g_sound_context) {
            alcMakeContextCurrent(nullptr);
            alcDestroyContext(g_sound_context);
            g_sound_context = nullptr;
        }
        if (g_sound_device) {
            alcCloseDevice(g_sound_device);
            g_sound_device = nullptr;
        }
    }

    void SoundSystem_UpdateListener(Vec3 position, Vec3 forward, Vec3 up) {
        alListener3f(AL_POSITION, position.x, position.y, position.z);
        Float orientation[] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
        alListenerfv(AL_ORIENTATION, orientation);
    }

    void SoundSystem_SetCurrentReverb(ReverbPreset preset) {
        if (g_current_reverb_preset != preset) {
            g_current_reverb_preset = preset;
        }
    }

    static BufferData* find_buffer_data(ALuint bufferID) {
        for (Int i = 0; i < g_buffer_count; i++) {
            if (g_buffers[i].bufferID == bufferID) return &g_buffers[i];
        }
        return nullptr;
    }

    static Uint get_or_create_wet_buffer(Uint dryBufferID, ReverbPreset preset) {
        if (preset == REVERB_PRESET_NONE) {
            return 0;
        }

        for (Int i = 0; i < g_wet_cache_count; ++i) {
            if (g_wet_buffer_cache[i].dryBufferID == dryBufferID && g_wet_buffer_cache[i].preset == preset) {
                return g_wet_buffer_cache[i].wetBufferID;
            }
        }

        BufferData* dryBuffer = find_buffer_data(dryBufferID);
        if (!dryBuffer || !dryBuffer->pcmData) {
            return 0;
        }

        if (dryBuffer->format != AL_FORMAT_MONO16) {
            return 0;
        }

        Int num_samples = dryBuffer->dataSize / sizeof(Short);
        ReverbSettings settings = DSP_Reverb_GetSettingsForPreset(preset);

        ProcessedAudio wet_audio = DSP_Reverb_Process((Short*)dryBuffer->pcmData, num_samples, dryBuffer->freq, &settings, true);
        if (!wet_audio.data) {
            return 0;
        }

        ALuint wetBufferID;
        alGenBuffers(1, &wetBufferID);
        alBufferData(wetBufferID, AL_FORMAT_MONO16, wet_audio.data, wet_audio.num_samples * sizeof(Short), dryBuffer->freq);
        delete[] wet_audio.data;

        if (alGetError() != AL_NO_ERROR) {
            alDeleteBuffers(1, &wetBufferID);
            return 0;
        }

        if (g_wet_cache_count < MAX_WET_CACHE_ENTRIES) {
            g_wet_buffer_cache[g_wet_cache_count].dryBufferID = dryBufferID;
            g_wet_buffer_cache[g_wet_cache_count].preset = preset;
            g_wet_buffer_cache[g_wet_cache_count].wetBufferID = wetBufferID;
            g_wet_cache_count++;
        }

        return wetBufferID;
    }

    static Uint internal_LoadMP3(const Char* path) {
        FILE* file = fopen(path, "rb");
        if (!file) {
            Console::Printf_Error("Could not open MP3 file %s\n", path);
            return 0;
        }

        fseek(file, 0, SEEK_END);
        Long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        Uchar* file_buffer = new Uchar[file_size];
        fread(file_buffer, 1, file_size, file);
        fclose(file);

        mp3dec_t mp3d;
        mp3dec_init(&mp3d);

        mp3dec_frame_info_t info;
        Short* pcm_buffer = nullptr;
        Usize pcm_size = 0;
        Usize pcm_capacity = 65536;
        pcm_buffer = new Short[pcm_capacity];

        Int samples;
        Uchar* buf_ptr = file_buffer;
        Usize bytes_left = file_size;

        while (bytes_left > 0 && (samples = mp3dec_decode_frame(&mp3d, buf_ptr, bytes_left, nullptr, &info)) > 0) {
            if (pcm_size + (Usize)samples * info.channels > pcm_capacity) {
                pcm_capacity = pcm_capacity * 2 + (Usize)samples * info.channels;
                Short* new_pcm_buffer = new Short[pcm_capacity];
                if (pcm_buffer) {
                    memcpy(new_pcm_buffer, pcm_buffer, pcm_size * sizeof(Short));
                    delete[] pcm_buffer;
                }
                pcm_buffer = new_pcm_buffer;
            }

            mp3dec_decode_frame(&mp3d, buf_ptr, bytes_left, pcm_buffer + pcm_size, &info);

            pcm_size += samples * info.channels;
            buf_ptr += info.frame_bytes;
            bytes_left -= info.frame_bytes;
        }

        delete[] file_buffer;

        if (pcm_size == 0) {
            delete[] pcm_buffer;
            return 0;
        }

        Short* final_pcm_buffer = pcm_buffer;
        Usize final_pcm_size_bytes = pcm_size * sizeof(Short);
        ALenum format = (info.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

        if (info.channels == 2) {
            Usize mono_samples = pcm_size / 2;
            Short* mono_buffer = new Short[mono_samples];
            for (Usize i = 0; i < mono_samples; i++) {
                mono_buffer[i] = (Short)(((Int)pcm_buffer[i * 2] + (Int)pcm_buffer[i * 2 + 1]) / 2);
            }
            delete[] pcm_buffer;
            final_pcm_buffer = mono_buffer;
            final_pcm_size_bytes = mono_samples * sizeof(Short);
            format = AL_FORMAT_MONO16;
        }

        ALuint bufferID;
        alGenBuffers(1, &bufferID);
        alBufferData(bufferID, format, final_pcm_buffer, final_pcm_size_bytes, info.hz);

        if (alGetError() != AL_NO_ERROR) {
            delete[] final_pcm_buffer;
            alDeleteBuffers(1, &bufferID);
            return 0;
        }

        if (g_buffer_count < MAX_BUFFERS) {
            g_buffers[g_buffer_count].bufferID = bufferID;
            g_buffers[g_buffer_count].pcmData = final_pcm_buffer;
            g_buffers[g_buffer_count].dataSize = final_pcm_size_bytes;
            g_buffers[g_buffer_count].format = format;
            g_buffers[g_buffer_count].freq = info.hz;
            g_buffer_count++;
        }
        else {
            delete[] final_pcm_buffer;
            alDeleteBuffers(1, &bufferID);
            return 0;
        }

        return bufferID;
    }

    static Uint internal_LoadWAV(const Char* path) {
        FILE* file = fopen(path, "rb");
        if (!file) return 0;

        Char chunkId[4];
        Uint chunkSize;

        fread(chunkId, 1, 4, file);
        fread(&chunkSize, 4, 1, file);
        fread(chunkId, 1, 4, file);
        if (strncmp(chunkId, "WAVE", 4) != 0) {
            fclose(file);
            return 0;
        }

        Bool foundFmt = false;
        Bool foundData = false;
        Ushort audioFormat = 0, numChannels = 0, bitsPerSample = 0;
        Uint sampleRate = 0, dataSize = 0;
        Uchar* data = nullptr;

        while (!feof(file)) {
            if (fread(chunkId, 1, 4, file) != 4) break;
            if (fread(&chunkSize, 4, 1, file) != 1) break;

            if (strncmp(chunkId, "fmt ", 4) == 0) {
                foundFmt = true;
                fread(&audioFormat, 2, 1, file);
                fread(&numChannels, 2, 1, file);
                fread(&sampleRate, 4, 1, file);
                fseek(file, 6, SEEK_CUR);
                fread(&bitsPerSample, 2, 1, file);
                if (chunkSize > 16) fseek(file, chunkSize - 16, SEEK_CUR);
            }
            else if (strncmp(chunkId, "data", 4) == 0) {
                foundData = true;
                dataSize = chunkSize;
                data = new Uchar[dataSize];
                fread(data, 1, dataSize, file);
            }
            else {
                fseek(file, chunkSize, SEEK_CUR);
            }

            if (foundFmt && foundData) break;
        }

        fclose(file);

        if (!foundFmt || !foundData || data == nullptr) {
            delete[] data;
            return 0;
        }

        ALenum format;
        if (numChannels == 1) format = (bitsPerSample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
        else format = (bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;

        ALuint bufferID;
        alGenBuffers(1, &bufferID);
        alBufferData(bufferID, format, data, dataSize, sampleRate);

        if (alGetError() != AL_NO_ERROR) {
            delete[] data;
            alDeleteBuffers(1, &bufferID);
            return 0;
        }

        if (g_buffer_count < MAX_BUFFERS) {
            g_buffers[g_buffer_count].bufferID = bufferID;
            g_buffers[g_buffer_count].pcmData = data;
            g_buffers[g_buffer_count].dataSize = dataSize;
            g_buffers[g_buffer_count].format = format;
            g_buffers[g_buffer_count].freq = sampleRate;
            g_buffer_count++;
        }
        else {
            delete[] data;
            alDeleteBuffers(1, &bufferID);
            return 0;
        }

        return bufferID;
    }

    Uint SoundSystem_LoadSound(const Char* path) {
        const Char* ext = strrchr(path, '.');
        if (!ext) {
            Console::Printf_Error("Could not determine file type for %s\n", path);
            return 0;
        }

        if (_stricmp(ext, ".wav") == 0) {
            return internal_LoadWAV(path);
        }
        else if (_stricmp(ext, ".mp3") == 0) {
            return internal_LoadMP3(path);
        }

        Console::Printf_Error("Unsupported sound format for %s\n", path);
        return 0;
    }

    static ALuint find_wet_source(ALuint drySourceID) {
        for (Int i = 0; i < g_playing_link_count; ++i) {
            if (g_playing_source_links[i].drySourceID == drySourceID) {
                return g_playing_source_links[i].wetSourceID;
            }
        }
        return 0;
    }

    static void remove_link(ALuint drySourceID) {
        Int found_index = -1;
        for (Int i = 0; i < g_playing_link_count; ++i) {
            if (g_playing_source_links[i].drySourceID == drySourceID) {
                found_index = i;
                break;
            }
        }
        if (found_index != -1) {
            if (g_playing_link_count > 1) {
                g_playing_source_links[found_index] = g_playing_source_links[g_playing_link_count - 1];
            }
            g_playing_link_count--;
        }
    }

    Uint SoundSystem_PlaySound(Uint bufferID, Vec3 position, Float volume, Float pitch, Float maxDistance, Bool looping) {
        if (bufferID == 0) return 0;

        ReverbSettings settings = DSP_Reverb_GetSettingsForPreset(g_current_reverb_preset);
        Uint wetBufferID = get_or_create_wet_buffer(bufferID, g_current_reverb_preset);

        PlayingSound p_sound = { 0, 0 };
        alGenSources(1, &p_sound.drySourceID);

        alSourcei(p_sound.drySourceID, AL_BUFFER, bufferID);
        alSource3f(p_sound.drySourceID, AL_POSITION, position.x, position.y, position.z);
        alSourcef(p_sound.drySourceID, AL_GAIN, volume * settings.dryLevel);
        alSourcef(p_sound.drySourceID, AL_PITCH, pitch);
        alSourcef(p_sound.drySourceID, AL_MAX_DISTANCE, maxDistance);
        alSourcei(p_sound.drySourceID, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
        alSourcePlay(p_sound.drySourceID);

        if (wetBufferID != 0) {
            alGenSources(1, &p_sound.wetSourceID);
            alSourcei(p_sound.wetSourceID, AL_BUFFER, wetBufferID);
            alSource3f(p_sound.wetSourceID, AL_POSITION, position.x, position.y, position.z);
            alSourcef(p_sound.wetSourceID, AL_GAIN, volume * settings.wetLevel);
            alSourcef(p_sound.wetSourceID, AL_PITCH, pitch);
            alSourcef(p_sound.wetSourceID, AL_MAX_DISTANCE, maxDistance);
            alSourcei(p_sound.wetSourceID, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
            alSourcePlay(p_sound.wetSourceID);

            if (g_playing_link_count < MAX_PLAYING_SOUNDS) {
                g_playing_source_links[g_playing_link_count].drySourceID = p_sound.drySourceID;
                g_playing_source_links[g_playing_link_count].wetSourceID = p_sound.wetSourceID;
                g_playing_link_count++;
            }
        }

        if (alGetError() != AL_NO_ERROR) {
            alDeleteSources(1, &p_sound.drySourceID);
            if (p_sound.wetSourceID != 0) alDeleteSources(1, &p_sound.wetSourceID);
            return 0;
        }

        return p_sound.drySourceID;
    }

    void SoundSystem_SetSourceLooping(Uint sourceID, Bool loop) {
        if (sourceID == 0) return;
        alSourcei(sourceID, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
        ALuint wetSourceID = find_wet_source(sourceID);
        if (wetSourceID != 0) {
            alSourcei(wetSourceID, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
        }
    }

    void SoundSystem_SetSourceIsGlobal(Uint sourceID, Bool is_global) {
        if (sourceID == 0) return;

        alSourcei(sourceID, AL_SOURCE_RELATIVE, is_global ? AL_TRUE : AL_FALSE);

        if (is_global) {
            alSource3f(sourceID, AL_POSITION, 0.0f, 0.0f, 0.0f);
        }

        ALuint wetSourceID = find_wet_source(sourceID);
        if (wetSourceID != 0) {
            alSourcei(wetSourceID, AL_SOURCE_RELATIVE, is_global ? AL_TRUE : AL_FALSE);
            if (is_global) {
                alSource3f(wetSourceID, AL_POSITION, 0.0f, 0.0f, 0.0f);
            }
        }
    }

    void SoundSystem_SetMasterVolume(Float volume) {
        if (volume < 0.0f) volume = 0.0f;
        if (volume > 4.0f) volume = 4.0f;
        alListenerf(AL_GAIN, volume);
    }

    void SoundSystem_SetSourceProperties(Uint sourceID, Float volume, Float pitch, Float maxDistance) {
        if (sourceID == 0) return;
        ReverbSettings settings = DSP_Reverb_GetSettingsForPreset(g_current_reverb_preset);
        alSourcef(sourceID, AL_GAIN, volume * settings.dryLevel);
        alSourcef(sourceID, AL_PITCH, pitch);
        alSourcef(sourceID, AL_MAX_DISTANCE, maxDistance);
        ALuint wetSourceID = find_wet_source(sourceID);
        if (wetSourceID != 0) {
            alSourcef(wetSourceID, AL_GAIN, volume * settings.wetLevel);
            alSourcef(wetSourceID, AL_PITCH, pitch);
            alSourcef(wetSourceID, AL_MAX_DISTANCE, maxDistance);
        }
    }

    void SoundSystem_SetSourcePosition(Uint sourceID, Vec3 position) {
        if (sourceID == 0) return;
        alSource3f(sourceID, AL_POSITION, position.x, position.y, position.z);
        ALuint wetSourceID = find_wet_source(sourceID);
        if (wetSourceID != 0) {
            alSource3f(wetSourceID, AL_POSITION, position.x, position.y, position.z);
        }
    }

    void SoundSystem_DeleteSource(Uint sourceID) {
        if (sourceID == 0) return;
        ALuint wetSourceID = find_wet_source(sourceID);
        if (wetSourceID != 0) {
            alDeleteSources(1, &wetSourceID);
            remove_link(sourceID);
        }
        alDeleteSources(1, &sourceID);
    }

    void SoundSystem_DeleteBuffer(Uint bufferID) {
        if (bufferID == 0) return;

        for (Int i = g_wet_cache_count - 1; i >= 0; i--) {
            if (g_wet_buffer_cache[i].dryBufferID == bufferID) {
                alDeleteBuffers(1, &g_wet_buffer_cache[i].wetBufferID);
                g_wet_buffer_cache[i] = g_wet_buffer_cache[g_wet_cache_count - 1];
                g_wet_cache_count--;
            }
        }

        BufferData* buf = find_buffer_data(bufferID);
        if (buf) {
            delete[](Uchar*)buf->pcmData;
            alDeleteBuffers(1, &bufferID);

            Int index = buf - g_buffers;
            if (index != g_buffer_count - 1) {
                g_buffers[index] = g_buffers[g_buffer_count - 1];
            }
            g_buffer_count--;
        }
    }

    void SoundSystem_Update(void) {
        if (g_playing_link_count == 0) {
            return;
        }

        Int write_idx = 0;
        for (Int read_idx = 0; read_idx < g_playing_link_count; ++read_idx) {
            ALint state;
            alGetSourcei(g_playing_source_links[read_idx].drySourceID, AL_SOURCE_STATE, &state);

            if (state == AL_STOPPED) {
                alDeleteSources(1, &g_playing_source_links[read_idx].drySourceID);
                if (g_playing_source_links[read_idx].wetSourceID != 0) {
                    alDeleteSources(1, &g_playing_source_links[read_idx].wetSourceID);
                }
            }
            else {
                if (write_idx != read_idx) {
                    g_playing_source_links[write_idx] = g_playing_source_links[read_idx];
                }
                write_idx++;
            }
        }
        g_playing_link_count = write_idx;
    }
}
