/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef NETWORK_H
#define NETWORK_H

//----------------------------------------//
// Brief: Networking, for now pining and download files only
//----------------------------------------//

#include <stdbool.h>
#include "level1_api.h"

#ifdef __cplusplus
extern "C" {
#endif

	LEVEL1_API void Network_Init(void);
	LEVEL1_API void Network_Shutdown(void);
	LEVEL1_API bool Network_DownloadFile(const char* url, const char* output_filepath);
	LEVEL1_API bool Network_Ping(const char* hostname);

#ifdef __cplusplus
}
#endif

#endif