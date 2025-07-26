/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <stdio.h>
#include <stdlib.h>

typedef int (*EngineMainFunc)(int argc, char* argv[]);

#ifdef PLATFORM_WINDOWS
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 0x00000001;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HMODULE engineLib = LoadLibraryA("engine.dll");
    if (!engineLib) {
        MessageBoxA(NULL, "Failed to load engine.dll", "Engine Error", MB_ICONERROR);
        return -1;
    }

    EngineMainFunc Engine_Main = (EngineMainFunc)GetProcAddress(engineLib, "Engine_Main");
    if (!Engine_Main) {
        MessageBoxA(NULL, "Failed to find Engine_Main in engine.dll", "Engine Error", MB_ICONERROR);
        FreeLibrary(engineLib);
        return -1;
    }

    int result = Engine_Main(__argc, __argv);
    FreeLibrary(engineLib);
    return result;
}
#else
int main(int argc, char* argv[]) {
    void* engineLib = dlopen("./libengine.so", RTLD_NOW);
    if (!engineLib) {
        fprintf(stderr, "Failed to load libengine.so: %s\n", dlerror());
        return -1;
    }

    dlerror();
    EngineMainFunc Engine_Main = (EngineMainFunc)dlsym(engineLib, "Engine_Main");
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        fprintf(stderr, "Failed to find Engine_Main: %s\n", dlsym_error);
        dlclose(engineLib);
        return -1;
    }

    int result = Engine_Main(argc, argv);
    dlclose(engineLib);
    return result;
}
#endif
