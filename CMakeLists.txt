cmake_minimum_required(VERSION 3.16)
project(TectonicEngine LANGUAGES C CXX)

# Set C++ standard and IPO
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)

# Compiler-specific optimization flags
if(MSVC)
    message(STATUS "Applying MSVC optimization settings")

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL")
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /O2 /GL")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")

    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /O2 /GL /Zi")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} /O2 /GL /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /LTCG /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO    "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /LTCG /DEBUG")

    foreach(config DEBUG MINSIZEREL)
        set(CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}} /Zi")
        set(CMAKE_C_FLAGS_${config} "${CMAKE_C_FLAGS_${config}} /Zi")
        set(CMAKE_SHARED_LINKER_FLAGS_${config} "${CMAKE_SHARED_LINKER_FLAGS_${config}} /DEBUG")
        set(CMAKE_EXE_LINKER_FLAGS_${config} "${CMAKE_EXE_LINKER_FLAGS_${config}} /DEBUG")
    endforeach()

elseif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Applying GCC/Clang optimization settings")

    add_compile_options(
        $<$<CONFIG:RELEASE>:-O3>
        $<$<CONFIG:RELEASE>:-march=native>
        $<$<CONFIG:RELEASE>:-flto>

        $<$<CONFIG:RELWITHDEBINFO>:-O2>
        $<$<CONFIG:RELWITHDEBINFO>:-g>
        $<$<CONFIG:RELWITHDEBINFO>:-march=native>
        $<$<CONFIG:RELWITHDEBINFO>:-flto>
    )

    add_link_options(
        $<$<CONFIG:RELEASE>:-flto>
        $<$<CONFIG:RELWITHDEBINFO>:-flto>
    )
endif()

# Detect architecture directory for Windows
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "Configuring for 64-bit (x64) build.")
        set(ARCH_DIR "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(STATUS "Configuring for 32-bit (x86) build.")
        set(ARCH_DIR "x86")
        if(MSVC)
            string(REPLACE "/SAFESEH" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
            string(REPLACE "/SAFESEH" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO /LARGEADDRESSAWARE")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO /LARGEADDRESSAWARE")
        endif()
    else()
        message(FATAL_ERROR "Unsupported architecture size.")
    endif()
endif()

# Source files
set(ENGINE_SOURCES
    engine/discord_wrapper.c
    engine/editor.c engine/editor_undo.c
    engine/game_data.c
    engine/gl_misc.c engine/io_system.c engine/engine.c engine/main_menu.c
    engine/map.c engine/particle_system.c
    engine/video_player.c engine/weapons.c
    engine/sentry_wrapper.c engine/checksum.c
    engine/beams.c
    engine/decals.c
    engine/lightmapper.cpp
    mikktspace/mikktspace.c
)

set(ENGINE_HEADERS
    engine/compat.h engine/engine_api.h engine/discord_wrapper.h
    engine/editor.h engine/editor_undo.h
    engine/game_data.h
    engine/gl_misc.h engine/io_system.h engine/main_menu.h engine/map.h
    engine/particle_system.h
    engine/video_player.h engine/weapons.h engine/sentry_wrapper.h engine/checksum.h
    engine/beams.h
    engine/decals.h
    engine/lightmapper.h
)

# math_lib library
add_library(math_lib SHARED
    engine/math_lib/math_lib.c
    engine/math_lib/math_lib.h
    engine/math_lib/math_api.h
)

target_compile_definitions(math_lib PRIVATE
    $<$<BOOL:${WIN32}>:MATH_DLL_EXPORTS>
)

target_include_directories(math_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/engine")

target_compile_options(math_lib PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)

# IPC library
add_library(ipc_lib SHARED
    engine/ipc_lib/ipc_system.c
    engine/ipc_lib/ipc_system.h
    engine/ipc_lib/ipc_api.h
)
target_compile_definitions(ipc_lib PRIVATE
    $<$<BOOL:${WIN32}>:IPC_DLL_EXPORTS>
)
target_include_directories(ipc_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
)
target_compile_options(ipc_lib PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)

# level0 library (core utilities)
add_library(level0 SHARED
    engine/level0/gl_console.cpp
    engine/level0/cvar.c
    engine/level0/commands.c
    engine/level0/gl_console.h
    engine/level0/cvar.h
    engine/level0/commands.h
    engine/level0/level0_api.h
    imgui-master/backends/imgui_impl_opengl3.cpp
    imgui-master/backends/imgui_impl_sdl2.cpp
    imgui-master/imgui.cpp
    imgui-master/imgui_draw.cpp
    imgui-master/imgui_tables.cpp
    imgui-master/imgui_widgets.cpp
)
target_compile_definitions(level0 PRIVATE
    $<$<BOOL:${WIN32}>:LEVEL0_DLL_EXPORTS>
    IMGUI_DEFINE_MATH_OPERATORS
    $<$<BOOL:${WIN32}>:_CRT_SECURE_NO_WARNINGS>
)
target_include_directories(level0 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/math_lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/ipc_lib"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdl/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui-master"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui-master/backends"
)
target_compile_options(level0 PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)

# level1 library (mid-core utilities)
add_library(level1 SHARED
    engine/level1/gameconfig.c
    engine/level1/network.c
    engine/level1/binds.c
    engine/level1/gameconfig.h
    engine/level1/network.h
    engine/level1/binds.h
    engine/level1/level1_api.h
)

target_compile_definitions(level1 PRIVATE
    $<$<BOOL:${WIN32}>:LEVEL1_DLL_EXPORTS>
)

target_include_directories(level1 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/level0"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/level1"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
)

if(WIN32)
    target_link_directories(level1 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl/lib/${ARCH_DIR}"
    )
    
    target_link_libraries(level1 PRIVATE level0 SDL2)
else()
    if(UNIX AND NOT APPLE)
        find_package(SDL2 REQUIRED)
        target_link_libraries(level1 PRIVATE level0 SDL2::SDL2)
    else()
        message(FATAL_ERROR "Unsupported platform for level1!")
    endif()
endif()

target_compile_options(level1 PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)

# materials library
add_library(materials SHARED
    engine/materials/texturemanager.c
    engine/materials/water_manager.c
    engine/materials/texturemanager.h
    engine/materials/water_manager.h
    engine/materials/materials_api.h
)

target_compile_definitions(materials PRIVATE
    $<$<BOOL:${WIN32}>:MATERIALS_DLL_EXPORTS>
)

target_include_directories(materials PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/level0"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/math_lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdl/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/include"
)

target_compile_options(materials PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)

# models library
add_library(models SHARED
    engine/models/model_loader.c
    engine/models/model_loader.h
    engine/models/models_api.h
)

target_compile_definitions(models PRIVATE
    $<$<BOOL:${WIN32}>:MODELS_DLL_EXPORTS>
    CGLTF_IMPLEMENTATION
)

target_include_directories(models PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/materials"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/math_lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/level0"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdl/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/include"
)

target_compile_options(models PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)

if(WIN32)
    target_link_directories(models PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/lib/Release/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/${ARCH_DIR}"
    )
    target_link_libraries(models PRIVATE 
        materials 
        math_lib 
        level0 
        opengl32 
        glew32 
        SDL2 
        SDL2_image
    )
else()
    target_link_libraries(models PRIVATE 
        materials 
        math_lib 
        level0 
        GLEW::glew 
        OpenGL::GL 
        SDL2::SDL2 
        ${SDL2_IMAGE_LIBRARY}
    )
endif()

# physics library
add_library(physics SHARED
    engine/physics/physics_wrapper.cpp
    engine/physics/physics_wrapper.h
    engine/physics/physics_api.h
)

target_compile_definitions(physics PRIVATE
    $<$<BOOL:${WIN32}>:PHYSICS_DLL_EXPORTS>
)

target_include_directories(physics
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/engine"
        "${CMAKE_CURRENT_SOURCE_DIR}/engine/math_lib"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/bullet/include"
)

target_compile_options(physics PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)

if(UNIX AND NOT APPLE)
    set(BULLET_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bullet/lib/linux")
    set(BULLET_LIBS
        "${BULLET_LIB_DIR}/libBulletDynamics.a"
        "${BULLET_LIB_DIR}/libBulletCollision.a"
        "${BULLET_LIB_DIR}/libLinearMath.a"
    )
    target_link_libraries(physics PRIVATE math_lib ${BULLET_LIBS})
else()
    set(BULLET_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bullet/lib/${ARCH_DIR}")
    target_link_directories(physics PRIVATE "${BULLET_LIB_DIR}")
    target_link_libraries(physics PRIVATE
        math_lib
        BulletCollision
        BulletDynamics
        LinearMath
    )
endif()

# sound library
add_library(sound SHARED
    engine/sound/sound_system.c
    engine/sound/dsp_reverb.c
    engine/sound/sound_system.h
    engine/sound/dsp_reverb.h
    engine/sound/sound_api.h
)

target_compile_definitions(sound PRIVATE
    $<$<BOOL:${WIN32}>:SOUND_DLL_EXPORTS>
    MINIMP3_IMPLEMENTATION
    OGG_IMPL
    VORBIS_IMPL
)

target_include_directories(sound PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/math_lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/level0"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdl/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.24.3/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/minimp3"
    "${CMAKE_CURRENT_SOURCE_DIR}/minivorbis"
)

target_compile_options(sound PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)

# engine library
add_library(engine SHARED ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_compile_definitions(engine PRIVATE
    ENGINE_DLL_EXPORTS
    SDL_MAIN_HANDLED
    PL_MPEG_IMPLEMENTATION
    STB_IMAGE_WRITE_IMPLEMENTATION
    STB_IMAGE_IMPLEMENTATION
    $<$<BOOL:${WIN32}>:_CRT_SECURE_NO_WARNINGS>
)

target_include_directories(engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/physics"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/math_lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/sound"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/level0"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/level1"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/materials"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/models"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdl/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/bullet/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/DiscordRPC/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.24.3/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui-master"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui-master/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/minivorbis"
    "${CMAKE_CURRENT_SOURCE_DIR}/sentry/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/embree-4.4.0/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/oidn-2.3.3/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/stb"
)

target_compile_options(engine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)

if(WIN32)
    message(STATUS "Configuring for Windows")

    target_link_directories(level0 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/lib/Release/${ARCH_DIR}"
    )

    target_link_libraries(level0 PRIVATE math_lib SDL2 opengl32 glew32 ipc_lib) 

    set_target_properties(engine PROPERTIES COMPILE_PDB_NAME engine)
    set_target_properties(ipc_lib PROPERTIES COMPILE_PDB_NAME ipc_lib)
    set_target_properties(sound PROPERTIES COMPILE_PDB_NAME sound)
    set_target_properties(materials PROPERTIES COMPILE_PDB_NAME materials)
    set_target_properties(models PROPERTIES COMPILE_PDB_NAME models)
    set_target_properties(physics PROPERTIES COMPILE_PDB_NAME physics)
    set_target_properties(level0 PROPERTIES COMPILE_PDB_NAME level0)
    set_target_properties(level1 PROPERTIES COMPILE_PDB_NAME level1)
    set_target_properties(math_lib PROPERTIES COMPILE_PDB_NAME math_lib)

    target_link_directories(sound PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.24.3/libs/${ARCH_DIR}"
    )

    target_link_libraries(sound PRIVATE level0 SDL2 OpenAL32)
    target_link_libraries(materials PRIVATE level0 math_lib SDL2 SDL2_image opengl32 glew32)


    target_link_directories(materials PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/lib/Release/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/${ARCH_DIR}"
    )
    target_link_directories(engine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/lib/Release/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/DiscordRPC/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.24.3/libs/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/sentry/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/embree-4.4.0/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/oidn-2.3.3/lib/${ARCH_DIR}"
    )

    target_link_libraries(engine PRIVATE
        level0
        level1
        ipc_lib
        physics
        math_lib
        sound
        materials
        models
        SDL2 SDL2_image SDL2_ttf opengl32 glew32
        OpenAL32 discord-rpc ws2_32
        sentry
    )
    if(ARCH_DIR STREQUAL "x64")
        target_link_libraries(engine PRIVATE embree4 tbb12 OpenImageDenoise)
    else()
        message(STATUS "Skipping Embree and OIDN linking for 32-bit build")
    endif()
else()
    if(UNIX AND NOT APPLE)
        message(STATUS "Configuring for Linux")
        target_compile_definitions(engine PRIVATE _GNU_SOURCE)
        find_package(SDL2 REQUIRED)
        find_package(GLEW REQUIRED)
        find_package(OpenGL REQUIRED)

        set(SDL2_IMAGE_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/linux/libSDL2_image.a")
        set(SDL2_TTF_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/lib/linux/libSDL2_ttf.a")
        set(OPENAL_LIB "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.24.3/libs/Linux/libopenal.a")
        set(SENTRY_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/sentry/lib/linux/libsentry.so")
        set(EMBREE_LIB "${CMAKE_CURRENT_SOURCE_DIR}/embree-4.4.0/lib/linux/libembree4.so")
        set(TBB_LIB "${CMAKE_CURRENT_SOURCE_DIR}/embree-4.4.0/lib/linux/libtbb.so")
        set(TBB_MALLOC_LIB "${CMAKE_CURRENT_SOURCE_DIR}/embree-4.4.0/lib/linux/libtbbmalloc.so")

        set(OIDN_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/oidn-2.3.3/lib/linux")
        set(OIDN_MAIN_LIB "${OIDN_LIB_DIR}/libOpenImageDenoise.so.2.3.3")
        set(OIDN_CORE_LIB "${OIDN_LIB_DIR}/libOpenImageDenoise_core.so.2.3.3")

        target_link_libraries(sound PRIVATE ${SDL2_LIBRARIES} ${OPENAL_LIB})
        target_link_libraries(materials PRIVATE level0 math_lib SDL2::SDL2 ${SDL2_IMAGE_LIBRARY} GLEW::glew OpenGL::GL)

        target_link_directories(engine PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/DiscordRPC/lib/linux"
            "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/linux"
            "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/lib/linux"
            "${CMAKE_CURRENT_SOURCE_DIR}/embree-4.4.0/lib/linux"
            "${OIDN_LIB_DIR}"
        )

        target_link_options(engine PRIVATE "-Wl,-rpath-link,${OIDN_LIB_DIR}")

        target_link_libraries(engine PRIVATE
            level0
            level1
            physics
            math_lib
            sound
            materials
            models
            ${SDL2_LIBRARIES}
            ${SDL2_IMAGE_LIBRARY}
            ${SDL2_TTF_LIBRARY}
            ${GLEW_LIBRARIES}
            ${OPENAL_LIB}
            ${OPENGL_LIBRARIES}
            ${SENTRY_LIBRARY}
            discord-rpc
            pthread
            dl
            freetype
            ${EMBREE_LIB}
            ${TBB_LIB}
            ${TBB_MALLOC_LIB}
            ${OIDN_MAIN_LIB}
            ${OIDN_CORE_LIB}
        )
    else()
        message(FATAL_ERROR "Unsupported platform! This project supports only Windows and Linux.")
    endif()
endif()

# launcher executable
add_executable(launcher launcher/launcher.c)

if(UNIX AND NOT APPLE)
    target_link_options(launcher PRIVATE "-Wl,-rpath,'$ORIGIN'")
endif()

if(WIN32)
    set(LAUNCHER_ICON_RC "${CMAKE_CURRENT_SOURCE_DIR}/launcher/resources/launcher.rc")
    set(LAUNCHER_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/launcher/resources/launcher.manifest")

    target_sources(launcher PRIVATE ${LAUNCHER_ICON_RC} ${LAUNCHER_MANIFEST})

    target_link_options(launcher PRIVATE "/MANIFEST:EMBED" "/MANIFESTINPUT:${LAUNCHER_MANIFEST}" /subsystem:windows)
endif()


target_compile_options(launcher PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)
if(WIN32)
    target_link_options(launcher PRIVATE /subsystem:windows)
endif()

# Tectonic Console
add_executable(TConsole Tools/TConsole/main.cpp)
set_target_properties(TConsole PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(TConsole PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/include"
)
if(WIN32)
    set_target_properties(TConsole PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_directories(TConsole PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/lib/${ARCH_DIR}")
    target_link_libraries(TConsole PRIVATE fltk ws2_32 comctl32 gdiplus)
else()
    target_link_directories(TConsole PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/lib/linux")
    target_link_libraries(TConsole PRIVATE fltk fltk_gl fltk_images X11 Xext Xinerama Xft fontconfig Xrender Xfixes Xcursor dl pthread m)
endif()

# Tectonic Model Importer
add_executable(TectonicModelImporter Tools/ModelImporter/main.cpp)

set_target_properties(TectonicModelImporter PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(TectonicModelImporter PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/cgltf"
)

if(WIN32)
    set_target_properties(TectonicModelImporter PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_directories(TectonicModelImporter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/lib/${ARCH_DIR}")
    target_link_libraries(TectonicModelImporter PRIVATE fltk fltk_gl fltk_images gdiplus ws2_32 comctl32 gdi32 ole32 uuid)
else()
    target_link_directories(TectonicModelImporter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/lib/linux")
    target_link_libraries(TectonicModelImporter PRIVATE fltk fltk_gl fltk_images X11 Xext Xinerama Xft fontconfig Xrender Xfixes Xcursor dl pthread m)
endif()

# Tectonic Particle Editor
add_executable(TectonicParticleEditor Tools/ParticleEditor/main.cpp)

set_target_properties(TectonicParticleEditor PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(TectonicParticleEditor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/include"
)

if(WIN32)
    set_target_properties(TectonicParticleEditor PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_directories(TectonicParticleEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/lib/${ARCH_DIR}")
    target_link_libraries(TectonicParticleEditor PRIVATE fltk fltk_gl fltk_images gdiplus ws2_32 comctl32 gdi32 ole32 uuid)
else()
    target_link_directories(TectonicParticleEditor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/lib/linux")
    target_link_libraries(TectonicParticleEditor PRIVATE fltk fltk_gl fltk_images X11 Xext Xinerama Xft fontconfig Xrender Xfixes Xcursor dl pthread m)
endif()

# Tectonic SDK Launcher
add_executable(SDKLauncher Tools/SDKLauncher/main.cpp)

set_target_properties(SDKLauncher PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(SDKLauncher PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/include"
)

if(WIN32)
    set_target_properties(SDKLauncher PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_directories(SDKLauncher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/lib/${ARCH_DIR}")
    target_link_libraries(SDKLauncher PRIVATE fltk fltk_gl fltk_images gdiplus ws2_32 comctl32 gdi32 ole32 uuid)
else()
    target_link_directories(SDKLauncher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fltk-1.4.4/lib/linux")
    target_link_libraries(SDKLauncher PRIVATE fltk fltk_gl fltk_images X11 Xext Xinerama Xft fontconfig Xrender Xfixes Xcursor dl pthread m)
endif()

# Patcher executable and post-build command on Windows
add_executable(Patcher engine/checksumpatcher.cpp)
set_target_properties(Patcher PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    add_custom_target(patch_engine ALL
        COMMAND $<TARGET_FILE:Patcher> $<TARGET_FILE:engine>
        DEPENDS Patcher engine
        COMMENT "Patching DLL with checksum..."
        VERBATIM
    )
endif()
