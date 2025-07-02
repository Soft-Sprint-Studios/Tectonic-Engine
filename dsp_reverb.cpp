/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "dsp_reverb.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define REVERB_TAIL_SECONDS 2.0f

const float comb_tunings[8] = { 25.31f, 26.94f, 28.96f, 30.75f, 32.24f, 33.81f, 35.31f, 36.69f };
const float allpass_tunings[4] = { 5.56f, 4.41f, 3.53f, 2.89f };

typedef struct {
    float feedback;
    float* buffer;
    int buffer_size;
    int buf_idx;
} AllPass;

typedef struct {
    float feedback;
    float damping;
    float filter_store;
    float pan_l, pan_r;
    float* buffer;
    int buffer_size;
    int buf_idx;
} Comb;

typedef struct {
    ReverbSettings settings;
    int sampleRate;
    Comb combs[8];
    AllPass allpasses[4];
} SimpleReverb;

static void AllPass_Init(AllPass* ap, int buffer_size) {
    ap->buffer_size = buffer_size;
    ap->buffer = (float*)calloc(buffer_size, sizeof(float));
    ap->buf_idx = 0;
    ap->feedback = 0.5f;
}

static void Comb_Init(Comb* c, int buffer_size) {
    c->buffer_size = buffer_size;
    c->buffer = (float*)calloc(buffer_size, sizeof(float));
    c->buf_idx = 0;
    c->filter_store = 0.0f;
}

static float AllPass_Process(AllPass* ap, float input) {
    if (!ap->buffer) return input;
    float buf_out = ap->buffer[ap->buf_idx];
    float new_val = input + buf_out * ap->feedback;
    ap->buffer[ap->buf_idx] = new_val;
    if (++ap->buf_idx >= ap->buffer_size) ap->buf_idx = 0;
    return -input + buf_out;
}

static float Comb_Process(Comb* c, float input) {
    if (!c->buffer) return 0.0f;
    float output = c->buffer[c->buf_idx];
    c->filter_store = (output * (1.0f - c->damping)) + (c->filter_store * c->damping);
    c->buffer[c->buf_idx] = input + (c->filter_store * c->feedback);
    if (++c->buf_idx >= c->buffer_size) c->buf_idx = 0;
    return output;
}

static void SimpleReverb_UpdateParameters(SimpleReverb* rev) {
    float roomSize = rev->settings.roomSize;
    float damping = rev->settings.damping;
    float width = rev->settings.width;

    for (int i = 0; i < 8; ++i) {
        rev->combs[i].feedback = roomSize;
        rev->combs[i].damping = damping;
        rev->combs[i].pan_l = 0.5f * (1.0f - width) + (i % 2 == 0 ? width : 0.0f);
        rev->combs[i].pan_r = 0.5f * (1.0f - width) + (i % 2 != 0 ? width : 0.0f);
    }
}

static void SimpleReverb_Init(SimpleReverb* rev, int sampleRate) {
    rev->sampleRate = sampleRate;
    for (int i = 0; i < 8; ++i) {
        Comb_Init(&rev->combs[i], (int)(comb_tunings[i] * sampleRate * 0.001f));
    }
    for (int i = 0; i < 4; ++i) {
        AllPass_Init(&rev->allpasses[i], (int)(allpass_tunings[i] * sampleRate * 0.001f));
    }
}

static void SimpleReverb_Free(SimpleReverb* rev) {
    for (int i = 0; i < 8; ++i) free(rev->combs[i].buffer);
    for (int i = 0; i < 4; ++i) free(rev->allpasses[i].buffer);
}

static void SimpleReverb_Process(SimpleReverb* rev, const float* input, float* output, int num_samples, bool wet_only) {
    float* output_float_l = (float*)calloc(num_samples, sizeof(float));
    float* output_float_r = (float*)calloc(num_samples, sizeof(float));
    if (!output_float_l || !output_float_r) {
        free(output_float_l);
        free(output_float_r);
        return;
    }

    for (int i = 0; i < num_samples; i++) {
        float in_sample = input[i] * 0.15f;
        float out_l = 0, out_r = 0;

        for (int j = 0; j < 8; ++j) {
            float comb_out = Comb_Process(&rev->combs[j], in_sample);
            out_l += comb_out * rev->combs[j].pan_l;
            out_r += comb_out * rev->combs[j].pan_r;
        }

        for (int j = 0; j < 4; ++j) {
            out_l = AllPass_Process(&rev->allpasses[j], out_l);
            out_r = AllPass_Process(&rev->allpasses[j], out_r);
        }
        output_float_l[i] = out_l;
        output_float_r[i] = out_r;
    }

    for (int i = 0; i < num_samples; i++) {
        float wet_signal = (output_float_l[i] * rev->settings.width + output_float_r[i] * (1.0f - rev->settings.width));
        float dry_signal = wet_only ? 0.0f : input[i];
        float mixed_sample = (wet_signal * rev->settings.wetLevel + dry_signal * rev->settings.dryLevel);
        output[i] = fmaxf(-1.0f, fminf(1.0f, mixed_sample));
    }
    free(output_float_l);
    free(output_float_r);
}
ReverbSettings DSP_Reverb_GetSettingsForPreset(ReverbPreset preset) {
    ReverbSettings s = { 0 };
    switch (preset) {
    case REVERB_PRESET_NONE:
        s.roomSize = 0.0f;
        s.damping = 0.0f;
        s.wetLevel = 0.0f;
        s.dryLevel = 1.0f;
        s.width = 0.5f;
        break;
    case REVERB_PRESET_SMALL_ROOM:
        s.roomSize = 0.6f;
        s.damping = 0.2f;
        s.wetLevel = 0.6f;
        s.dryLevel = 0.9f;
        s.width = 0.6f;
        break;
    case REVERB_PRESET_MEDIUM_ROOM:
        s.roomSize = 0.75f;
        s.damping = 0.3f;
        s.wetLevel = 0.7f;
        s.dryLevel = 0.8f;
        s.width = 0.7f;
        break;
    case REVERB_PRESET_LARGE_ROOM:
        s.roomSize = 0.85f;
        s.damping = 0.4f;
        s.wetLevel = 0.8f;
        s.dryLevel = 0.7f;
        s.width = 0.8f;
        break;
    case REVERB_PRESET_HALL:
        s.roomSize = 0.94f;
        s.damping = 0.5f;
        s.wetLevel = 0.8f;
        s.dryLevel = 0.6f;
        s.width = 0.9f;
        break;
    case REVERB_PRESET_CAVE:
        s.roomSize = 0.98f;
        s.damping = 0.1f;
        s.wetLevel = 0.9f;
        s.dryLevel = 0.5f;
        s.width = 1.0f;
        break;
    default:
        s.roomSize = 0.0f;
        s.damping = 0.0f;
        s.wetLevel = 0.0f;
        s.dryLevel = 1.0f;
        s.width = 0.5f;
        break;
    }
    return s;
}

ProcessedAudio DSP_Reverb_Process(const short* input, int num_samples, int sample_rate, const ReverbSettings* settings, bool wet_only) {
    ProcessedAudio result = { NULL, 0 };
    if (!input || num_samples <= 0) return result;

    int tail_samples = (int)(sample_rate * REVERB_TAIL_SECONDS); \
        int total_samples = num_samples + tail_samples;

    float* padded_input_float = (float*)malloc(total_samples * sizeof(float));
    float* output_float = (float*)malloc(total_samples * sizeof(float));
    if (!padded_input_float || !output_float) {
        free(padded_input_float);
        free(output_float);
        return result;
    }

    for (int i = 0; i < num_samples; ++i) {
        padded_input_float[i] = input[i] / 32768.0f;
    }
    memset(padded_input_float + num_samples, 0, tail_samples * sizeof(float));

    SimpleReverb reverb;
    SimpleReverb_Init(&reverb, sample_rate);
    reverb.settings = *settings;
    SimpleReverb_UpdateParameters(&reverb);
    SimpleReverb_Process(&reverb, padded_input_float, output_float, total_samples, wet_only);
    SimpleReverb_Free(&reverb);

    free(padded_input_float);

    short* output_short = (short*)malloc(total_samples * sizeof(short));
    if (!output_short) {
        free(output_float);
        return result;
    }

    for (int i = 0; i < total_samples; ++i) {
        output_short[i] = (short)(output_float[i] * 32767.0f);
    }
    free(output_float);

    result.data = output_short;
    result.num_samples = total_samples;
    return result;
}