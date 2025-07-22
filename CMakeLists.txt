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
    foreach(config RELEASE RELWITHDEBINFO)
        set(CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}} /O2 /GL /Zi")
        set(CMAKE_C_FLAGS_${config} "${CMAKE_C_FLAGS_${config}} /O2 /GL /Zi")
        set(CMAKE_SHARED_LINKER_FLAGS_${config} "${CMAKE_SHARED_LINKER_FLAGS_${config}} /LTCG /DEBUG")
        set(CMAKE_EXE_LINKER_FLAGS_${config} "${CMAKE_EXE_LINKER_FLAGS_${config}} /LTCG /DEBUG")
    endforeach()

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

            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
        endif()
    else()
        message(FATAL_ERROR "Unsupported architecture size.")
    endif()
endif()

# Source files
set(ENGINE_SOURCES
    engine/binds.c engine/cvar.c engine/discord_wrapper.c engine/dsp_reverb.c
    engine/editor.c engine/editor_undo.c engine/gameconfig.c engine/gl_console.cpp
    engine/gl_misc.c engine/io_system.c engine/engine.c engine/main_menu.c
    engine/map.c engine/model_loader.c engine/network.c engine/particle_system.c
    engine/sound_system.c engine/texturemanager.c engine/video_player.c engine/weapons.c
    engine/sentry_wrapper.c engine/checksum.c engine/commands.c engine/water_manager.c
    imgui-master/backends/imgui_impl_opengl3.cpp imgui-master/backends/imgui_impl_sdl2.cpp
    imgui-master/imgui.cpp imgui-master/imgui_draw.cpp imgui-master/imgui_tables.cpp imgui-master/imgui_widgets.cpp
    mikktspace/mikktspace.c
)

set(ENGINE_HEADERS
    engine/compat.h engine/engine_api.h engine/binds.h engine/cvar.h engine/discord_wrapper.h
    engine/dsp_reverb.h engine/editor.h engine/editor_undo.h engine/gameconfig.h engine/gl_console.h
    engine/gl_misc.h engine/io_system.h engine/main_menu.h engine/map.h engine/model_loader.h
    engine/network.h engine/particle_system.h engine/sound_system.h engine/texturemanager.h
    engine/video_player.h engine/weapons.h engine/sentry_wrapper.h engine/checksum.h engine/commands.h
    engine/water_manager.h
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

# engine library
add_library(engine SHARED ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_compile_definitions(engine PRIVATE
    ENGINE_DLL_EXPORTS
    IMGUI_DEFINE_MATH_OPERATORS
    SDL_MAIN_HANDLED
    PL_MPEG_IMPLEMENTATION
    CGLTF_IMPLEMENTATION
    MINIMP3_IMPLEMENTATION
    OGG_IMPL
    VORBIS_IMPL
    $<$<BOOL:${WIN32}>:_CRT_SECURE_NO_WARNINGS>
)

target_include_directories(engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/physics"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/math_lib"
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
)

target_compile_options(engine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)

if(WIN32)
    message(STATUS "Configuring for Windows")

    set_target_properties(engine PROPERTIES
        COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdb"
        COMPILE_PDB_NAME engine
    )

    target_link_directories(engine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/lib/Release/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/DiscordRPC/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.24.3/libs/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/sentry/lib/${ARCH_DIR}"
    )

    target_link_libraries(engine PRIVATE
        SDL2 SDL2_image SDL2_ttf opengl32 glew32
        physics
        OpenAL32 discord-rpc ws2_32
        sentry math_lib
    )
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

        target_link_directories(engine PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/DiscordRPC/lib/linux"
            "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/linux"
            "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/lib/linux"
        )

        target_link_libraries(engine PRIVATE
            ${SDL2_LIBRARIES}
            ${SDL2_IMAGE_LIBRARY}
            ${SDL2_TTF_LIBRARY}
            ${GLEW_LIBRARIES}
            physics
            ${OPENAL_LIB}
            ${OPENGL_LIBRARIES}
            ${SENTRY_LIBRARY}
            discord-rpc
            pthread
            dl
            freetype
            math_lib
        )
    else()
        message(FATAL_ERROR "Unsupported platform! This project supports only Windows and Linux.")
    endif()
endif()

# launcher executable
add_executable(launcher launcher/launcher.c)
target_link_libraries(launcher PRIVATE engine)
target_compile_options(launcher PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
    $<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/engine/compat.h">
)
if(WIN32)
    target_link_options(launcher PRIVATE /subsystem:windows)
endif()

# Batchmodelimporter executable
add_executable(Batchmodelimporter Batchmodelimporter/main.cpp)
target_compile_definitions(Batchmodelimporter PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(Batchmodelimporter PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
if(WIN32)
    target_link_options(Batchmodelimporter PRIVATE /subsystem:console)
endif()

# Patcher executable and post-build command on Windows
add_executable(Patcher engine/checksumpatcher.cpp)
set_target_properties(Patcher PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

if(WIN32)
    add_custom_command(
        TARGET launcher
        POST_BUILD
        COMMAND $<TARGET_FILE:Patcher> $<TARGET_FILE:engine>
        COMMENT "Patching engine DLL with checksum..."
        VERBATIM
    )
endif()
