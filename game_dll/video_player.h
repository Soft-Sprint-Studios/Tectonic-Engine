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
#pragma once
#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

//----------------------------------------//
// Brief: Video player, no sound support yet
//----------------------------------------//

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

	void VideoPlayer_UpdateAll(Scene* scene, float deltaTime);
	void VideoPlayer_InitSystem(void);
	void VideoPlayer_ShutdownSystem(void);

	void VideoPlayer_Load(VideoPlayer* vp);
	void VideoPlayer_Free(VideoPlayer* vp);
	void VideoPlayer_Play(VideoPlayer* vp);
	void VideoPlayer_Stop(VideoPlayer* vp);
	void VideoPlayer_Restart(VideoPlayer* vp);
	void VideoPlayer_Update(VideoPlayer* vp, float deltaTime);

#ifdef __cplusplus
}
#endif

#endif // VIDEO_PLAYER_H