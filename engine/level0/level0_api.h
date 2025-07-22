#ifndef LEVEL0_API_H
#define LEVEL0_API_H

#include "compat.h"

#ifdef PLATFORM_WINDOWS
    #ifdef LEVEL0_DLL_EXPORTS
        #define LEVEL0_API __declspec(dllexport)
    #else
        #define LEVEL0_API __declspec(dllimport)
    #endif
#else
    #define LEVEL0_API __attribute__((visibility("default")))
#endif

#endif // LEVEL0_API_H