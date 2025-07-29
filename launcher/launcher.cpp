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
#include <shellapi.h>
#else
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#endif

using EngineMainFunc = int (*)(int argc, char* argv[]);
EngineMainFunc Engine_Main = nullptr;

#ifdef PLATFORM_WINDOWS
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 0x00000001;

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS*) {
    MessageBoxA(nullptr, "Engine crashed with an unhandled exception.", "Fatal Error", MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

bool LoadEngineLibrary(HMODULE& engineLib) {
    engineLib = LoadLibraryA("engine.dll");
    if (!engineLib) {
        MessageBoxA(nullptr, "Failed to load engine.dll", "Engine Error", MB_ICONERROR);
        return false;
    }
    Engine_Main = reinterpret_cast<EngineMainFunc>(GetProcAddress(engineLib, "Engine_Main"));
    if (!Engine_Main) {
        MessageBoxA(nullptr, "Failed to find Engine_Main in engine.dll", "Engine Error", MB_ICONERROR);
        FreeLibrary(engineLib);
        return false;
    }
    return true;
}

void UnloadEngineLibrary(HMODULE engineLib) {
    if (engineLib) FreeLibrary(engineLib);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SetUnhandledExceptionFilter(ExceptionHandler);
    HMODULE engineLib = nullptr;
    if (!LoadEngineLibrary(engineLib)) return -1;
    int result = 0;
    __try {
        int argc = 0;
        LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
        char** argv = new char* [argc];

        for (int i = 0; i < argc; ++i) {
            int len = WideCharToMultiByte(CP_ACP, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
            argv[i] = new char[len];
            WideCharToMultiByte(CP_ACP, 0, wargv[i], -1, argv[i], len, nullptr, nullptr);
        }

        int result = Engine_Main(argc, argv);

        for (int i = 0; i < argc; ++i)
            delete[] argv[i];
        delete[] argv;
        LocalFree(wargv);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        MessageBoxA(nullptr, "Engine crashed unexpectedly.", "Runtime Crash", MB_ICONERROR);
        result = -1;
    }
    UnloadEngineLibrary(engineLib);
    return result;
}

#else

#include <iostream>

void SignalHandler(int signal) {
    std::cerr << "Engine crashed with signal: " << strsignal(signal) << std::endl;
    std::exit(EXIT_FAILURE);
}

bool LoadEngineLibrary(void*& engineLib) {
    engineLib = dlopen("./libengine.so", RTLD_NOW);
    if (!engineLib) {
        std::cerr << "Failed to load libengine.so: " << dlerror() << std::endl;
        return false;
    }
    dlerror();
    Engine_Main = reinterpret_cast<EngineMainFunc>(dlsym(engineLib, "Engine_Main"));
    const char* error = dlerror();
    if (error) {
        std::cerr << "Failed to find Engine_Main: " << error << std::endl;
        dlclose(engineLib);
        return false;
    }
    return true;
}

void UnloadEngineLibrary(void* engineLib) {
    if (engineLib) dlclose(engineLib);
}

int main(int argc, char* argv[]) {
    signal(SIGSEGV, SignalHandler);
    signal(SIGABRT, SignalHandler);
    void* engineLib = nullptr;
    if (!LoadEngineLibrary(engineLib)) return -1;
    int result = 0;
    try {
        result = Engine_Main(argc, argv);
    } catch (...) {
        std::cerr << "Engine threw an exception." << std::endl;
        result = -1;
    }
    UnloadEngineLibrary(engineLib);
    return result;
}

#endif
