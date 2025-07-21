/*
 * Copyright © 2025 Soft Sprint Studios
 * All rights reserved.
 *
 * This file is proprietary and confidential. Unauthorized reproduction,
 * modification, or distribution is strictly prohibited unless explicit
 * written permission is granted by Soft Sprint Studios.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "../engine/compat.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <shellapi.h>

typedef int (*EngineMainFunc)(int, char**);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = 0;
    wchar_t** argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvW) {
        MessageBoxA(NULL, "Fatal: Could not parse command line", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    char** argv = (char**)malloc(sizeof(char*) * argc);
    if (!argv) {
        LocalFree(argvW);
        MessageBoxA(NULL, "Fatal: Out of memory", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    for (int i = 0; i < argc; i++) {
        int size_needed = WideCharToMultiByte(CP_ACP, 0, argvW[i], -1, NULL, 0, NULL, NULL);
        argv[i] = (char*)malloc(size_needed);
        WideCharToMultiByte(CP_ACP, 0, argvW[i], -1, argv[i], size_needed, NULL, NULL);
    }
    LocalFree(argvW);
    const char* engine_lib_path = "TectonicEngine.dll";
    HINSTANCE engine_lib = LoadLibrary(engine_lib_path);
    if (!engine_lib) {
        MessageBoxA(NULL, "Fatal: Could not load TectonicEngine.dll", "Error", MB_OK | MB_ICONERROR);
        for (int i = 0; i < argc; i++) free(argv[i]);
        free(argv);
        return 1;
    }
    EngineMainFunc engine_main = (EngineMainFunc)GetProcAddress(engine_lib, "Engine_Main");
    if (!engine_main) {
        MessageBoxA(NULL, "Fatal: Could not find Engine_Main in TectonicEngine.dll", "Error", MB_OK | MB_ICONERROR);
        FreeLibrary(engine_lib);
        for (int i = 0; i < argc; i++) free(argv[i]);
        free(argv);
        return 1;
    }
    int result = engine_main(argc, argv);
    FreeLibrary(engine_lib);
    for (int i = 0; i < argc; i++) free(argv[i]);
    free(argv);
    return result;
}
#else
#include <dlfcn.h>

typedef int (*EngineMainFunc)(int, char**);

int main(int argc, char* argv[]) {
    const char* engine_lib_path = "./libTectonicEngine.so";
    void* engine_lib = dlopen(engine_lib_path, RTLD_LAZY);
    if (!engine_lib) {
        fprintf(stderr, "Fatal: Could not load %s: %s\n", engine_lib_path, dlerror());
        return 1;
    }
    EngineMainFunc engine_main = (EngineMainFunc)dlsym(engine_lib, "Engine_Main");
    if (!engine_main) {
        fprintf(stderr, "Fatal: Could not find Engine_Main in %s: %s\n", engine_lib_path, dlerror());
        dlclose(engine_lib);
        return 1;
    }
    int result = engine_main(argc, argv);
    dlclose(engine_lib);
    return result;
}
#endif
