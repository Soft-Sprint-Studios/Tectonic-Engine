/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "sound_system.h"
#include "gl_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <AL/al.h>
#include <AL/alc.h>
#include "minimp3/minimp3.h"
#include "minivorbis.h"

#define MAX_WET_CACHE_ENTRIES 256
#define MAX_PLAYING_SOUNDS 512
#define MAX_BUFFERS 1024

typedef struct {
    ALuint bufferID;
    void* pcmData;
    unsigned int dataSize;
    ALenum format;
    ALsizei freq;
} BufferData;

static BufferData g_buffers[MAX_BUFFERS];
static int g_buffer_count = 0;

static ALCdevice* g_sound_device = NULL;
static ALCcontext* g_sound_context = NULL;
static ReverbPreset g_current_reverb_preset = REVERB_PRESET_NONE;

typedef struct {
    ALuint dryBufferID;
    ReverbPreset preset;
    ALuint wetBufferID;
} WetBufferCacheEntry;

static WetBufferCacheEntry g_wet_buffer_cache[MAX_WET_CACHE_ENTRIES];
static int g_wet_cache_count = 0;

typedef struct {
    ALuint drySourceID;
    ALuint wetSourceID;
} PlayingSourceLink;

static PlayingSourceLink g_playing_source_links[MAX_PLAYING_SOUNDS];
static int g_playing_link_count = 0;

bool SoundSystem_Init() {
    g_sound_device = alcOpenDevice(NULL);
    if (!g_sound_device) return false;

    g_sound_context = alcCreateContext(g_sound_device, NULL);
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
    for (int i = 0; i < g_buffer_count; i++) {
        free(g_buffers[i].pcmData);
        alDeleteBuffers(1, &g_buffers[i].bufferID);
    }
    g_buffer_count = 0;
    g_wet_cache_count = 0;
    g_playing_link_count = 0;

    if (g_sound_context) {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(g_sound_context);
        g_sound_context = NULL;
    }
    if (g_sound_device) {
        alcCloseDevice(g_sound_device);
        g_sound_device = NULL;
    }
}

void SoundSystem_UpdateListener(Vec3 position, Vec3 forward, Vec3 up) {
    alListener3f(AL_POSITION, position.x, position.y, position.z);
    float orientation[] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
    alListenerfv(AL_ORIENTATION, orientation);
}

void SoundSystem_SetCurrentReverb(ReverbPreset preset) {
    if (g_current_reverb_preset != preset) {
        g_current_reverb_preset = preset;
    }
}

static BufferData* find_buffer_data(ALuint bufferID) {
    for (int i = 0; i < g_buffer_count; i++) {
        if (g_buffers[i].bufferID == bufferID) return &g_buffers[i];
    }
    return NULL;
}

static unsigned int get_or_create_wet_buffer(unsigned int dryBufferID, ReverbPreset preset) {
    if (preset == REVERB_PRESET_NONE) {
        return 0;
    }

    for (int i = 0; i < g_wet_cache_count; ++i) {
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

    int num_samples = dryBuffer->dataSize / sizeof(short);
    ReverbSettings settings = DSP_Reverb_GetSettingsForPreset(preset);

    ProcessedAudio wet_audio = DSP_Reverb_Process((short*)dryBuffer->pcmData, num_samples, dryBuffer->freq, &settings, true);
    if (!wet_audio.data) {
        return 0;
    }

    ALuint wetBufferID;
    alGenBuffers(1, &wetBufferID);
    alBufferData(wetBufferID, AL_FORMAT_MONO16, wet_audio.data, wet_audio.num_samples * sizeof(short), dryBuffer->freq);
    free(wet_audio.data);

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

static unsigned int internal_LoadMP3(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        Console_Printf_Error("ERROR: Could not open MP3 file %s\n", path);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* file_buffer = (unsigned char*)malloc(file_size);
    if (!file_buffer) {
        fclose(file);
        return 0;
    }
    fread(file_buffer, 1, file_size, file);
    fclose(file);

    mp3dec_t mp3d;
    mp3dec_init(&mp3d);

    mp3dec_frame_info_t info;
    short* pcm_buffer = NULL;
    size_t pcm_size = 0;
    size_t pcm_capacity = 65536;
    pcm_buffer = (short*)malloc(pcm_capacity * sizeof(short));

    if (!pcm_buffer) {
        free(file_buffer);
        return 0;
    }

    int samples;
    unsigned char* buf_ptr = file_buffer;
    int bytes_left = file_size;

    while (bytes_left > 0 && (samples = mp3dec_decode_frame(&mp3d, buf_ptr, bytes_left, NULL, &info)) > 0) {
        if (pcm_size + (size_t)samples * info.channels > pcm_capacity) {
            pcm_capacity = pcm_capacity * 2 + (size_t)samples * info.channels;
            short* new_pcm_buffer = (short*)realloc(pcm_buffer, pcm_capacity * sizeof(short));
            if (!new_pcm_buffer) {
                free(pcm_buffer);
                free(file_buffer);
                return 0;
            }
            pcm_buffer = new_pcm_buffer;
        }

        mp3dec_decode_frame(&mp3d, buf_ptr, bytes_left, pcm_buffer + pcm_size, &info);

        pcm_size += samples * info.channels;
        buf_ptr += info.frame_bytes;
        bytes_left -= info.frame_bytes;
    }

    free(file_buffer);

    if (pcm_size == 0) {
        free(pcm_buffer);
        return 0;
    }

    short* final_pcm_buffer = pcm_buffer;
    size_t final_pcm_size_bytes = pcm_size * sizeof(short);
    ALenum format = (info.channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    if (info.channels == 2) {
        size_t mono_samples = pcm_size / 2;
        short* mono_buffer = (short*)malloc(mono_samples * sizeof(short));
        if (!mono_buffer) {
            free(pcm_buffer);
            return 0;
        }
        for (size_t i = 0; i < mono_samples; i++) {
            mono_buffer[i] = (short)(((int)pcm_buffer[i * 2] + (int)pcm_buffer[i * 2 + 1]) / 2);
        }
        free(pcm_buffer);
        final_pcm_buffer = mono_buffer;
        final_pcm_size_bytes = mono_samples * sizeof(short);
        format = AL_FORMAT_MONO16;
    }

    ALuint bufferID;
    alGenBuffers(1, &bufferID);
    alBufferData(bufferID, format, final_pcm_buffer, final_pcm_size_bytes, info.hz);

    if (alGetError() != AL_NO_ERROR) {
        free(final_pcm_buffer);
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
        free(final_pcm_buffer);
        alDeleteBuffers(1, &bufferID);
        return 0;
    }

    return bufferID;
}

static unsigned int internal_LoadOGG(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        Console_Printf_Error("ERROR: Could not open OGG file %s", path);
        return 0;
    }

    OggVorbis_File vorbis;
    if (ov_open_callbacks(file, &vorbis, NULL, 0, OV_CALLBACKS_DEFAULT) != 0) {
        Console_Printf_Error("ERROR: Invalid Ogg Vorbis file: %s", path);
        fclose(file);
        return 0;
    }

    vorbis_info* info = ov_info(&vorbis, -1);

    size_t data_capacity = 4096 * 16;
    unsigned char* pcm_data = malloc(data_capacity);
    if (!pcm_data) {
        ov_clear(&vorbis);
        return 0;
    }

    long bytes_read = 0;
    size_t total_bytes = 0;
    int bitstream;

    while ((bytes_read = ov_read(&vorbis, (char*)pcm_data + total_bytes, data_capacity - total_bytes, 0, 2, 1, &bitstream)) > 0) {
        total_bytes += bytes_read;
        if (data_capacity - total_bytes < 4096) {
            data_capacity *= 2;
            unsigned char* new_pcm_data = realloc(pcm_data, data_capacity);
            if (!new_pcm_data) {
                free(pcm_data);
                ov_clear(&vorbis);
                return 0;
            }
            pcm_data = new_pcm_data;
        }
    }

    ALenum format = (info->channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    if (info->channels == 2) {
        size_t mono_samples = total_bytes / 4;
        short* mono_buffer = malloc(mono_samples * sizeof(short));
        if (mono_buffer) {
            short* stereo_buffer = (short*)pcm_data;
            for (size_t i = 0; i < mono_samples; i++) {
                mono_buffer[i] = (short)(((int)stereo_buffer[i * 2] + (int)stereo_buffer[i * 2 + 1]) / 2);
            }
            free(pcm_data);
            pcm_data = (unsigned char*)mono_buffer;
            total_bytes = mono_samples * sizeof(short);
            format = AL_FORMAT_MONO16;
        }
    }

    ALuint bufferID;
    alGenBuffers(1, &bufferID);
    alBufferData(bufferID, format, pcm_data, total_bytes, info->rate);
    ov_clear(&vorbis);

    if (alGetError() != AL_NO_ERROR) {
        free(pcm_data);
        alDeleteBuffers(1, &bufferID);
        return 0;
    }

    if (g_buffer_count < MAX_BUFFERS) {
        g_buffers[g_buffer_count].bufferID = bufferID;
        g_buffers[g_buffer_count].pcmData = pcm_data;
        g_buffers[g_buffer_count].dataSize = total_bytes;
        g_buffers[g_buffer_count].format = format;
        g_buffers[g_buffer_count].freq = info->rate;
        g_buffer_count++;
    }
    else {
        free(pcm_data);
        alDeleteBuffers(1, &bufferID);
        return 0;
    }

    return bufferID;
}

static unsigned int internal_LoadWAV(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return 0;

    char chunkId[4];
    unsigned int chunkSize;

    fread(chunkId, 1, 4, file);
    fread(&chunkSize, 4, 1, file);
    fread(chunkId, 1, 4, file);
    if (strncmp(chunkId, "WAVE", 4) != 0) {
        fclose(file);
        return 0;
    }

    bool foundFmt = false;
    bool foundData = false;
    unsigned short audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    unsigned int sampleRate = 0, dataSize = 0;
    void* data = NULL;

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
            data = malloc(dataSize);
            if (!data) {
                fclose(file);
                return 0;
            }
            fread(data, 1, dataSize, file);
        }
        else {
            fseek(file, chunkSize, SEEK_CUR);
        }

        if (foundFmt && foundData) break;
    }

    fclose(file);

    if (!foundFmt || !foundData || data == NULL) {
        free(data);
        return 0;
    }

    ALenum format;
    if (numChannels == 1) format = (bitsPerSample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
    else format = (bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;

    ALuint bufferID;
    alGenBuffers(1, &bufferID);
    alBufferData(bufferID, format, data, dataSize, sampleRate);

    if (alGetError() != AL_NO_ERROR) {
        free(data);
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
        free(data);
        alDeleteBuffers(1, &bufferID);
        return 0;
    }

    return bufferID;
}

unsigned int SoundSystem_LoadSound(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) {
        Console_Printf_Error("ERROR: Could not determine file type for %s\n", path);
        return 0;
    }

    if (_stricmp(ext, ".wav") == 0) {
        return internal_LoadWAV(path);
    }
    else if (_stricmp(ext, ".mp3") == 0) {
        return internal_LoadMP3(path);
    }
    else if (_stricmp(ext, ".ogg") == 0) {
        return internal_LoadOGG(path);
    }

    Console_Printf_Error("ERROR: Unsupported sound format for %s\n", path);
    return 0;
}

static ALuint find_wet_source(ALuint drySourceID) {
    for (int i = 0; i < g_playing_link_count; ++i) {
        if (g_playing_source_links[i].drySourceID == drySourceID) {
            return g_playing_source_links[i].wetSourceID;
        }
    }
    return 0;
}

static void remove_link(ALuint drySourceID) {
    int found_index = -1;
    for (int i = 0; i < g_playing_link_count; ++i) {
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

unsigned int SoundSystem_PlaySound(unsigned int bufferID, Vec3 position, float volume, float pitch, float maxDistance, bool looping) {
    if (bufferID == 0) return 0;

    ReverbSettings settings = DSP_Reverb_GetSettingsForPreset(g_current_reverb_preset);
    unsigned int wetBufferID = get_or_create_wet_buffer(bufferID, g_current_reverb_preset);

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

void SoundSystem_SetSourceLooping(unsigned int sourceID, bool loop) {
    if (sourceID == 0) return;
    alSourcei(sourceID, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    ALuint wetSourceID = find_wet_source(sourceID);
    if (wetSourceID != 0) {
        alSourcei(wetSourceID, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    }
}

void SoundSystem_SetMasterVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 4.0f) volume = 4.0f;
    alListenerf(AL_GAIN, volume);
}

void SoundSystem_SetSourceProperties(unsigned int sourceID, float volume, float pitch, float maxDistance) {
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

void SoundSystem_SetSourcePosition(unsigned int sourceID, Vec3 position) {
    if (sourceID == 0) return;
    alSource3f(sourceID, AL_POSITION, position.x, position.y, position.z);
    ALuint wetSourceID = find_wet_source(sourceID);
    if (wetSourceID != 0) {
        alSource3f(wetSourceID, AL_POSITION, position.x, position.y, position.z);
    }
}

void SoundSystem_DeleteSource(unsigned int sourceID) {
    if (sourceID == 0) return;
    ALuint wetSourceID = find_wet_source(sourceID);
    if (wetSourceID != 0) {
        alDeleteSources(1, &wetSourceID);
        remove_link(sourceID);
    }
    alDeleteSources(1, &sourceID);
}

void SoundSystem_DeleteBuffer(unsigned int bufferID) {
    if (bufferID == 0) return;

    for (int i = g_wet_cache_count - 1; i >= 0; i--) {
        if (g_wet_buffer_cache[i].dryBufferID == bufferID) {
            alDeleteBuffers(1, &g_wet_buffer_cache[i].wetBufferID);
            g_wet_buffer_cache[i] = g_wet_buffer_cache[g_wet_cache_count - 1];
            g_wet_cache_count--;
        }
    }

    BufferData* buf = find_buffer_data(bufferID);
    if (buf) {
        free(buf->pcmData);
        alDeleteBuffers(1, &bufferID);

        int index = buf - g_buffers;
        if (index != g_buffer_count - 1) {
            g_buffers[index] = g_buffers[g_buffer_count - 1];
        }
        g_buffer_count--;
    }
}
