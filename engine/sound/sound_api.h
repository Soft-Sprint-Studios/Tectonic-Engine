#ifndef SOUND_API_H
#define SOUND_API_H

#include "compat.h"

#ifdef PLATFORM_WINDOWS
    #ifdef SOUND_DLL_EXPORTS
        #define SOUND_API __declspec(dllexport)
    #else
        #define SOUND_API __declspec(dllimport)
    #endif
#else
    #define SOUND_API __attribute__((visibility("default")))
#endif

#endif // SOUND_API_H