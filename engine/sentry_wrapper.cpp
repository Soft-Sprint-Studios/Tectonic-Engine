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
#include "sentry_wrapper.h"
#include <sentry.h>
#include "gl_console.h"
#include <cstdio>

class SentryManager {
public:
    SentryManager() {
        sentry_options_t* options = sentry_options_new();

        sentry_options_set_dsn(options,
            "https://cf008152a413b73d0676c836c674868f@o4505736231124992.ingest.us.sentry.io/4509651269648384");

        char release_string[128];
        snprintf(release_string, sizeof(release_string),
            "TectonicEngine@D.E.V-build%d-%s", Compat_GetBuildNumber(), ARCH_STRING);
        sentry_options_set_release(options, release_string);

        sentry_options_set_debug(options, 1);

#ifdef PLATFORM_WINDOWS
        sentry_options_set_handler_path(options, "./crashpad_handler.exe");
#else
        sentry_options_set_handler_path(options, "./crashpad_handler");
#endif

        sentry_init(options);
        Console_Printf("Sentry Crash Reporting Initialized.\n");
    }

    ~SentryManager() {
        sentry_shutdown();
        Console_Printf("Sentry Crash Reporting Shutdown.\n");
    }
};

static SentryManager* g_manager = nullptr;

// TODO: convert engine.c to cpp so we dont need this extern C anymore

extern "C" {

    void Sentry_Init() {
        if (!g_manager)
            g_manager = new SentryManager();
    }

    void Sentry_Shutdown() {
        delete g_manager;
        g_manager = nullptr;
    }

}
