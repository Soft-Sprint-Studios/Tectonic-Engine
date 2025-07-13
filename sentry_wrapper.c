/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "sentry_wrapper.h"
#include <sentry.h>
#include "gl_console.h"

void Sentry_Init(void) {
    sentry_options_t *options = sentry_options_new();

    sentry_options_set_dsn(options, "https://cf008152a413b73d0676c836c674868f@o4505736231124992.ingest.us.sentry.io/4509651269648384");

    char release_string[128];
    snprintf(release_string, sizeof(release_string), "TectonicEngine@D.E.V-build%d-%s", g_build_number, ARCH_STRING);
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

void Sentry_Shutdown(void) {
    sentry_shutdown();
    Console_Printf("Sentry Crash Reporting Shutdown.\n");
}