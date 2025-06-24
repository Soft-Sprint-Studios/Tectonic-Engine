/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef DISCORD_WRAPPER_H
#define DISCORD_WRAPPER_H

void Discord_Init();
void Discord__Shutdown();
void Discord_Update(const char* state, const char* details);

#endif // DISCORD_WRAPPER_H