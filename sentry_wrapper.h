/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef SENTRY_WRAPPER_H
#define SENTRY_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

void Sentry_Init(void);
void Sentry_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // SENTRY_WRAPPER_H