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

#ifdef __cplusplus
extern "C" {
#endif

	void Network_Init(void);
	void Network_Shutdown(void);
	bool Network_DownloadFile(const char* url, const char* output_filepath);
	bool Network_Ping(const char* hostname);

#ifdef __cplusplus
}
#endif

#endif