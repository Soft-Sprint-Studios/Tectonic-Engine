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
#include <discord_rpc.h>
#include "gl_console.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "discord_wrapper.h"

static const char* APPLICATION_ID = "1386692288914260071";
static int64_t StartTime;

static void handleDiscordReady(const DiscordUser* request) {
    Console_Printf("Discord: connected to user %s#%s - %s\n",
        request->username,
        request->discriminator,
        request->userId);
}

static void handleDiscordDisconnected(int errcode, const char* message) {
    Console_Printf("Discord: disconnected (%d: %s)\n", errcode, message);
}

static void handleDiscordError(int errcode, const char* message) {
    Console_Printf("Discord: error (%d: %s)\n", errcode, message);
}

void Discord_Init() {
    DiscordEventHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    handlers.ready = handleDiscordReady;
    handlers.disconnected = handleDiscordDisconnected;
    handlers.errored = handleDiscordError;
    Discord_Initialize(APPLICATION_ID, &handlers, 1, NULL);
    Console_Printf("Discord RPC Initialized.\n");
    StartTime = time(0);
}

void Discord__Shutdown() {
    Discord_Shutdown();
    Console_Printf("Discord RPC Shutdown.\n");
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