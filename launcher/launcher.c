/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include "engine_api.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <stdlib.h>
#endif

#ifdef PLATFORM_WINDOWS
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 0x00000001;
#endif

#ifdef PLATFORM_WINDOWS
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return Engine_Main(__argc, __argv);
}
#else
int main(int argc, char* argv[]) {
    return Engine_Main(argc, argv);
}
#endif