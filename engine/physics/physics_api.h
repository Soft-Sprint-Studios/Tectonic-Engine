#ifndef PHYSICS_API_H
#define PHYSICS_API_H

#ifdef PLATFORM_WINDOWS
    #ifdef PHYSICS_DLL_EXPORTS
        #define PHYSICS_API __declspec(dllexport)
    #else
        #define PHYSICS_API __declspec(dllimport)
    #endif
#else
    #define PHYSICS_API __attribute__((visibility("default")))
#endif

#endif // PHYSICS_API_H
