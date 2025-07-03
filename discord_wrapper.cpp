/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "DiscordRPC/include/discord_rpc.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "discord_wrapper.h"

static const char* APPLICATION_ID = "1386692288914260071";
static int64_t StartTime;

static void handleDiscordReady(const DiscordUser* request) {
    printf("Discord: connected to user %s#%s - %s\n",
        request->username,
        request->discriminator,
        request->userId);
}

static void handleDiscordDisconnected(int errcode, const char* message) {
    printf("Discord: disconnected (%d: %s)\n", errcode, message);
}

static void handleDiscordError(int errcode, const char* message) {
    printf("Discord: error (%d: %s)\n", errcode, message);
}

void Discord_Init() {
    DiscordEventHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    handlers.ready = handleDiscordReady;
    handlers.disconnected = handleDiscordDisconnected;
    handlers.errored = handleDiscordError;
    Discord_Initialize(APPLICATION_ID, &handlers, 1, NULL);
    printf("Discord RPC Initialized.\n");
    StartTime = time(0);
}

void Discord__Shutdown() {
    Discord_Shutdown();
    printf("Discord RPC Shutdown.\n");
}

void Discord_Update(const char* state, const char* details) {
    DiscordRichPresence discordPresence;
    memset(&discordPresence, 0, sizeof(discordPresence));
    discordPresence.state = state;
    discordPresence.details = details;
    discordPresence.startTimestamp = StartTime;
    discordPresence.largeImageText = "Tectonic Engine";
    Discord_UpdatePresence(&discordPresence);
}