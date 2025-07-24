/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifndef LIGHTMAPPER_H
#define LIGHTMAPPER_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

	void Lightmapper_Generate(Scene* scene, Engine* engine, int resolution);

#ifdef __cplusplus
}
#endif

#endif // LIGHTMAPPER_H