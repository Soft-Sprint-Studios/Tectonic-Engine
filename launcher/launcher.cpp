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

#include <iostream>
#include <stdexcept>
#include <string>

using EngineMainFunc = int(*)(int argc, char* argv[]);

#ifdef PLATFORM_WINDOWS

extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 0x00000001;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HMODULE engineLib = LoadLibraryA("engine.dll");
    if (!engineLib) {
        MessageBoxA(nullptr, "Failed to load engine.dll", "Engine Error", MB_ICONERROR | MB_OK);
        return -1;
    }

    auto Engine_Main = reinterpret_cast<EngineMainFunc>(GetProcAddress(engineLib, "Engine_Main"));
    if (!Engine_Main) {
        MessageBoxA(nullptr, "Failed to find Engine_Main in engine.dll", "Engine Error", MB_ICONERROR | MB_OK);
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
        cerr << "Failed to load libengine.so: " << dlerror() << endl;
        return -1;
    }

    dlerror();
    auto Engine_Main = reinterpret_cast<EngineMainFunc>(dlsym(engineLib, "Engine_Main"));
    if (const char* error = dlerror()) {
        cerr << "Failed to find Engine_Main: " << error << endl;
        dlclose(engineLib);
        return -1;
    }

    int result = Engine_Main(argc, argv);
    dlclose(engineLib);
    return result;
}
#endif
