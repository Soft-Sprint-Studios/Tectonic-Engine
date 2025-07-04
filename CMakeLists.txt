cmake_minimum_required(VERSION 3.16)
project(TectonicEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINE_SOURCES
    binds.c cvar.c discord_wrapper.cpp dsp_reverb.c editor.c editor_undo.c gameconfig.c gl_console.cpp gl_misc.c io_system.c main.c main_menu.c map.c math_lib.c model_loader.c network.c particle_system.c physics_wrapper.cpp sound_system.c texturemanager.c video_player.c
    imgui-master/backends/imgui_impl_opengl3.cpp imgui-master/backends/imgui_impl_sdl2.cpp imgui-master/imgui.cpp imgui-master/imgui_draw.cpp imgui-master/imgui_tables.cpp imgui-master/imgui_widgets.cpp
    mikktspace/mikktspace.c
)
set(ENGINE_HEADERS
    compat.h binds.h cvar.h discord_wrapper.h dsp_reverb.h editor.h editor_undo.h gameconfig.h gl_console.h gl_misc.h io_system.h main_menu.h map.h math_lib.h model_loader.h network.h particle_system.h physics_wrapper.h sound_system.h texturemanager.h video_player.h
)

add_executable(TectonicEngine ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_compile_definitions(TectonicEngine PRIVATE
    IMGUI_DEFINE_MATH_OPERATORS
    SDL_MAIN_HANDLED
    PL_MPEG_IMPLEMENTATION
    CGLTF_IMPLEMENTATION
    MINIMP3_IMPLEMENTATION
)

target_include_directories(TectonicEngine PRIVATE
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
)

target_compile_options(TectonicEngine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/compat.h">
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/compat.h">
)

if(WIN32)
    message(STATUS "Configuring for Windows")

    target_compile_definitions(TectonicEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /subsystem:console")

    target_link_directories(TectonicEngine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl/lib/x64"
        "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/lib/Release/x64"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/x64"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/lib/x64"
        "${CMAKE_CURRENT_SOURCE_DIR}/bullet/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/DiscordRPC/lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.24.3/libs/Win64"
    )

    target_link_libraries(TectonicEngine PRIVATE
        SDL2 SDL2_image SDL2_ttf opengl32 glew32
        BulletCollision BulletDynamics LinearMath
        OpenAL32 discord-rpc ws2_32
    )

elseif(UNIX AND NOT APPLE)
    message(STATUS "Configuring for Linux")

    find_package(SDL2 REQUIRED)
    find_package(SDL2_image REQUIRED)
    find_package(SDL2_ttf REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(Bullet REQUIRED COMPONENTS BulletCollision BulletDynamics LinearMath)
    find_package(OpenAL REQUIRED)

    target_link_directories(TectonicEngine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/DiscordRPC/lib/linux")

    target_link_libraries(TectonicEngine PRIVATE
        SDL2::SDL2
        SDL2::SDL2_image
        SDL2::SDL2_ttf
        GLEW::GLEW
        Bullet::BulletCollision
        Bullet::BulletDynamics
        Bullet::LinearMath
        OpenAL::OpenAL
        discord-rpc
        GL
        pthread
        dl
    )
else()
    message(FATAL_ERROR "Unsupported platform! This project supports only Windows and Linux.")
endif()

add_executable(Batchmodelimporter Batchmodelimporter/main.cpp)
target_compile_definitions(Batchmodelimporter PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(Batchmodelimporter PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)