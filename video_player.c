/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "video_player.h"
#include <stdio.h>
#include <al.h>
#include "gl_misc.h"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg/pl_mpeg.h"

#define NUM_AUDIO_BUFFERS 4

static GLuint video_shader = 0;
static GLuint video_vao = 0;
static GLuint video_vbo = 0;

void VideoPlayer_InitSystem(void) {
    video_shader = createShaderProgram("shaders/video.vert", "shaders/video.frag");

    float vertices[] = {
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,

        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &video_vao);
    glGenBuffers(1, &video_vbo);
    glBindVertexArray(video_vao);
    glBindBuffer(GL_ARRAY_BUFFER, video_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void VideoPlayer_ShutdownSystem(void) {
    glDeleteProgram(video_shader);
    glDeleteVertexArrays(1, &video_vao);
    glDeleteBuffers(1, &video_vbo);
}

void VideoPlayer_Load(VideoPlayer* vp) {
    if (vp->plm) {
        VideoPlayer_Free(vp);
    }
    vp->plm = plm_create_with_filename(vp->videoPath);
    if (!vp->plm) {
        printf("Error loading video: %s\n", vp->videoPath);
        return;
    }

    plm_set_audio_enabled(vp->plm, false);
    plm_set_loop(vp->plm, vp->loop);

    vp->time = 0;
    vp->nextFrameTime = 0;

    glGenTextures(1, &vp->textureID);
    glBindTexture(GL_TEXTURE_2D, vp->textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, plm_get_width(vp->plm), plm_get_height(vp->plm), 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    alGenSources(1, &vp->audioSource);
    alGenBuffers(NUM_AUDIO_BUFFERS, vp->audioBuffers);
    alSource3f(vp->audioSource, AL_POSITION, vp->pos.x, vp->pos.y, vp->pos.z);
    alSourcef(vp->audioSource, AL_GAIN, 1.0f);
    alSourcef(vp->audioSource, AL_PITCH, 1.0f);
    alSourcei(vp->audioSource, AL_LOOPING, vp->loop ? AL_TRUE : AL_FALSE);
}

void VideoPlayer_Free(VideoPlayer* vp) {
    if (vp->plm) {
        plm_destroy(vp->plm);
        vp->plm = NULL;
    }
    if (vp->textureID) {
        glDeleteTextures(1, &vp->textureID);
        vp->textureID = 0;
    }
    if (vp->audioSource) {
        alSourceStop(vp->audioSource);
        alDeleteSources(1, &vp->audioSource);
        alDeleteBuffers(NUM_AUDIO_BUFFERS, vp->audioBuffers);
        vp->audioSource = 0;
    }
}

void VideoPlayer_Play(VideoPlayer* vp) {
    if (!vp->plm || vp->state == VP_PLAYING) return;

    vp->state = VP_PLAYING;
    vp->time = 0.0;
    vp->nextFrameTime = 0.0;

    plm_seek(vp->plm, 0.0, true);
}

void VideoPlayer_Stop(VideoPlayer* vp) {
    if (!vp->plm || vp->state == VP_STOPPED) return;
    vp->state = VP_STOPPED;
    alSourceStop(vp->audioSource);
    plm_seek(vp->plm, 0, true);
}

void VideoPlayer_Restart(VideoPlayer* vp) {
    if (!vp->plm) return;
    VideoPlayer_Stop(vp);
    VideoPlayer_Play(vp);
}

uint8_t* ConvertFrameToRGB(plm_frame_t* frame) {
    int width = frame->width;
    int height = frame->height;
    uint8_t* rgb = (uint8_t*)malloc(width * height * 3);
    if (!rgb) return NULL;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int y_index = y * width + x;
            int cbcr_x = x / 2;
            int cbcr_y = y / 2;
            int cbcr_index = cbcr_y * (width / 2) + cbcr_x;

            uint8_t Y = frame->y.data[y_index];
            uint8_t Cb = frame->cb.data[cbcr_index];
            uint8_t Cr = frame->cr.data[cbcr_index];

            float fY = (float)Y;
            float fCb = (float)Cb - 128.0f;
            float fCr = (float)Cr - 128.0f;

            float R = fY + 1.402f * fCr;
            float G = fY - 0.344136f * fCb - 0.714136f * fCr;
            float B = fY + 1.772f * fCb;

            uint8_t r = (uint8_t)fmaxf(0.0f, fminf(255.0f, R));
            uint8_t g = (uint8_t)fmaxf(0.0f, fminf(255.0f, G));
            uint8_t b = (uint8_t)fmaxf(0.0f, fminf(255.0f, B));

            int out_index = (y * width + x) * 3;
            rgb[out_index + 0] = r;
            rgb[out_index + 1] = g;
            rgb[out_index + 2] = b;
        }
    }

    return rgb;
}

void VideoPlayer_Update(VideoPlayer* vp, float deltaTime) {
    if (!vp->plm || vp->state != VP_PLAYING) return;

    vp->time += deltaTime;

    if (vp->time >= vp->nextFrameTime) {
        plm_frame_t* vframe = plm_decode_video(vp->plm);
        if (vframe) {
            uint8_t* rgb = ConvertFrameToRGB(vframe);
            if (rgb) {
                glBindTexture(GL_TEXTURE_2D, vp->textureID);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vframe->width, vframe->height,
                    GL_RGB, GL_UNSIGNED_BYTE, rgb);
                glBindTexture(GL_TEXTURE_2D, 0);
                free(rgb);
            }

            double frameDuration = 1.0 / plm_get_framerate(vp->plm);
            vp->nextFrameTime += frameDuration;

        }
        else {
            if (vp->loop) {
                plm_seek(vp->plm, 0.0, true);
                vp->time = 0.0;
                vp->nextFrameTime = 0.0;
            }
            else {
                VideoPlayer_Stop(vp);
            }
        }
    }
}

void VideoPlayer_UpdateAll(Scene* scene, float deltaTime) {
    for (int i = 0; i < scene->numVideoPlayers; ++i) {
        VideoPlayer_Update(&scene->videoPlayers[i], deltaTime);
    }
}

void VideoPlayer_Render(VideoPlayer* vp, Mat4* view, Mat4* projection) {
    if (!vp || vp->state == VP_STOPPED || vp->textureID == 0) return;

    glUseProgram(video_shader);

    vp->modelMatrix = create_trs_matrix(vp->pos, vp->rot, (Vec3) { vp->size.x, vp->size.y, 1.0f });

    glUniformMatrix4fv(glGetUniformLocation(video_shader, "model"), 1, GL_FALSE, vp->modelMatrix.m);
    glUniformMatrix4fv(glGetUniformLocation(video_shader, "view"), 1, GL_FALSE, view->m);
    glUniformMatrix4fv(glGetUniformLocation(video_shader, "projection"), 1, GL_FALSE, projection->m);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vp->textureID);
    glUniform1i(glGetUniformLocation(video_shader, "videoTexture"), 0);

    glBindVertexArray(video_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}