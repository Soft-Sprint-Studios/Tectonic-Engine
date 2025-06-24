/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "sound_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <al.h>
#include <alc.h>

static ALCdevice* g_sound_device = NULL;
static ALCcontext* g_sound_context = NULL;

typedef struct {
    char chunkId[4];
    unsigned int chunkSize;
    char format[4];
    char subchunk1Id[4];
    unsigned int subchunk1Size;
    unsigned short audioFormat;
    unsigned short numChannels;
    unsigned int sampleRate;
    unsigned int byteRate;
    unsigned short blockAlign;
    unsigned short bitsPerSample;
    char subchunk2Id[4];
    unsigned int subchunk2Size;
} WavHeader;

bool SoundSystem_Init() {
    g_sound_device = alcOpenDevice(NULL);
    if (!g_sound_device) {
        printf("ERROR: Failed to open OpenAL device.\n");
        return false;
    }

    g_sound_context = alcCreateContext(g_sound_device, NULL);
    if (!g_sound_context) {
        printf("ERROR: Failed to create OpenAL context.\n");
        alcCloseDevice(g_sound_device);
        return false;
    }

    if (!alcMakeContextCurrent(g_sound_context)) {
        printf("ERROR: Failed to make OpenAL context current.\n");
        alcDestroyContext(g_sound_context);
        alcCloseDevice(g_sound_device);
        return false;
    }

    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    printf("Sound System Initialized.\n");
    return true;
}

void SoundSystem_Shutdown() {
    if (g_sound_context) {
        alcMakeContextCurrent(NULL);
        alcDestroyContext(g_sound_context);
        g_sound_context = NULL;
    }
    if (g_sound_device) {
        alcCloseDevice(g_sound_device);
        g_sound_device = NULL;
    }
    printf("Sound System Shutdown.\n");
}

void SoundSystem_UpdateListener(Vec3 position, Vec3 forward, Vec3 up) {
    alListener3f(AL_POSITION, position.x, position.y, position.z);
    float orientation[] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
    alListenerfv(AL_ORIENTATION, orientation);
}

unsigned int SoundSystem_LoadWAV(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("ERROR: Could not open WAV file: %s\n", path);
        return 0;
    }

    char chunkId[4];
    unsigned int chunkSize;

    fread(chunkId, 1, 4, file);
    fread(&chunkSize, 1, 4, file);
    fread(chunkId, 1, 4, file);
    if (strncmp(chunkId, "WAVE", 4) != 0) {
        printf("ERROR: Invalid WAV file format for %s\n", path);
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
        if (fread(&chunkSize, 1, 4, file) != 4) break;

        if (strncmp(chunkId, "fmt ", 4) == 0) {
            foundFmt = true;
            fread(&audioFormat, 1, 2, file);
            fread(&numChannels, 1, 2, file);
            fread(&sampleRate, 1, 4, file);
            fseek(file, 6, SEEK_CUR);
            fread(&bitsPerSample, 1, 2, file);
            if (chunkSize > 16) {
                fseek(file, chunkSize - 16, SEEK_CUR);
            }
        }
        else if (strncmp(chunkId, "data", 4) == 0) {
            foundData = true;
            dataSize = chunkSize;
            data = malloc(dataSize);
            if (!data) {
                printf("ERROR: Could not allocate memory for WAV data.\n");
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
        printf("ERROR: Could not find required 'fmt ' or 'data' chunks in %s\n", path);
        free(data);
        return 0;
    }

    ALenum format;
    if (numChannels == 1) {
        format = (bitsPerSample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
    }
    else {
        format = (bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
    }

    ALuint bufferID;
    alGenBuffers(1, &bufferID);
    alBufferData(bufferID, format, data, dataSize, sampleRate);
    free(data);

    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        printf("OpenAL Error during buffer creation: %d\n", error);
        return 0;
    }

    return bufferID;
}

unsigned int SoundSystem_PlaySound(unsigned int bufferID, Vec3 position, float volume, float pitch, float maxDistance) {
    if (bufferID == 0) return 0;

    ALuint sourceID;
    alGenSources(1, &sourceID);

    ALint channels;
    alGetBufferi(bufferID, AL_CHANNELS, &channels);

    if (channels > 1) {
        alSourcei(sourceID, AL_SOURCE_RELATIVE, AL_TRUE);
        alSource3f(sourceID, AL_POSITION, 0.0f, 0.0f, 0.0f);
    }
    else {
        alSourcei(sourceID, AL_SOURCE_RELATIVE, AL_FALSE);
        alSource3f(sourceID, AL_POSITION, position.x, position.y, position.z);
        alSourcef(sourceID, AL_ROLLOFF_FACTOR, 1.0f);
        alSourcef(sourceID, AL_REFERENCE_DISTANCE, 1.0f);
        alSourcef(sourceID, AL_MAX_DISTANCE, maxDistance);
    }

    alSourcei(sourceID, AL_BUFFER, bufferID);
    alSourcef(sourceID, AL_PITCH, pitch);
    alSourcef(sourceID, AL_GAIN, volume);
    alSourcei(sourceID, AL_LOOPING, AL_FALSE);

    alSourcePlay(sourceID);

    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        printf("OpenAL Error during sound play: %d\n", error);
        alDeleteSources(1, &sourceID);
        return 0;
    }

    return sourceID;
}

void SoundSystem_SetSourceProperties(unsigned int sourceID, float volume, float pitch, float maxDistance) {
    if (sourceID == 0) return;
    alSourcef(sourceID, AL_GAIN, volume);
    alSourcef(sourceID, AL_PITCH, pitch);
    alSourcef(sourceID, AL_MAX_DISTANCE, maxDistance);
}

void SoundSystem_SetSourcePosition(unsigned int sourceID, Vec3 position) {
    if (sourceID == 0) return;
    alSource3f(sourceID, AL_POSITION, position.x, position.y, position.z);
}

void SoundSystem_DeleteSource(unsigned int sourceID) {
    if (sourceID == 0) return;
    alSourceStop(sourceID);
    alDeleteSources(1, &sourceID);
}

void SoundSystem_DeleteBuffer(unsigned int bufferID) {
    if (bufferID == 0) return;
    alDeleteBuffers(1, &bufferID);
}