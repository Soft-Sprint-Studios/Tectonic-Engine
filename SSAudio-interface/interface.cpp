/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */

 // The SSAudio Interface Wrapper
 /*
 *   SSAudio is composed of three parts:
 *   - interface wrapper (you are here)
 *   - API plugin
 *
 *   The Interface Wrapper contains the base classes
 *   for adding various audio plugins,
 *   which can be made by the community and us!
 */

#include "baseplay.h"


void InitSSAudio() {
	CBaseAudioPlayer::CBaseAudioPlayer();
}