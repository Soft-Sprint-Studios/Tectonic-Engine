/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

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