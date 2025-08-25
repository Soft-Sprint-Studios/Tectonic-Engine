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
#include "discord_wrapper.h"
#include <discord_rpc.h>
#include "gl_console.h"
#include <ctime>
#include <cstring>

class DiscordManager {
public:
    DiscordManager() {
        DiscordEventHandlers handlers{};
        handlers.ready = handleDiscordReady;
        handlers.disconnected = handleDiscordDisconnected;
        handlers.errored = handleDiscordError;

        Discord_Initialize(APPLICATION_ID, &handlers, 1, nullptr);
        StartTime = std::time(nullptr);
        Console_Printf("Discord RPC Initialized.\n");
    }

    ~DiscordManager() {
        Discord_Shutdown();
        Console_Printf("Discord RPC Shutdown.\n");
    }

    void Update(const char* state, const char* details) {
        DiscordRichPresence presence{};
        presence.state = state;
        presence.details = details;
        presence.startTimestamp = StartTime;
        presence.largeImageText = "Tectonic Engine";
        Discord_UpdatePresence(&presence);
    }

private:
    static void handleDiscordReady(const DiscordUser* user) {
        Console_Printf("Discord: connected to user %s#%s - %s\n",
            user->username, user->discriminator, user->userId);
    }

    static void handleDiscordDisconnected(int err, const char* msg) {
        Console_Printf("Discord: disconnected (%d: %s)\n", err, msg);
    }

    static void handleDiscordError(int err, const char* msg) {
        Console_Printf("Discord: error (%d: %s)\n", err, msg);
    }

    static inline const char* APPLICATION_ID = "1386692288914260071";
    static inline int64_t StartTime;
};

static DiscordManager* g_manager = nullptr;

// TODO: convert engine.c to cpp so we dont need this extern C anymore

extern "C" {

    void Discord_Init() {
        if (!g_manager)
            g_manager = new DiscordManager();
    }

    void Discord__Shutdown() {
        delete g_manager;
        g_manager = nullptr;
    }

    void Discord_Update(const char* state, const char* details) {
        if (g_manager)
            g_manager->Update(state, details);
    }

}
