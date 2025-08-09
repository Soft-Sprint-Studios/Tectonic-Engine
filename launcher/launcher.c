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