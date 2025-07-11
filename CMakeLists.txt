cmake_minimum_required(VERSION 3.16)
project(TectonicEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)

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

set(ENGINE_SOURCES
    binds.c cvar.c discord_wrapper.cpp dsp_reverb.c editor.c editor_undo.c gameconfig.c gl_console.cpp gl_misc.c io_system.c main.c main_menu.c map.c math_lib.c model_loader.c network.c particle_system.c physics_wrapper.cpp sound_system.c texturemanager.c video_player.c weapons.c sentry_wrapper.c
    imgui-master/backends/imgui_impl_opengl3.cpp imgui-master/backends/imgui_impl_sdl2.cpp imgui-master/imgui.cpp imgui-master/imgui_draw.cpp imgui-master/imgui_tables.cpp imgui-master/imgui_widgets.cpp
    mikktspace/mikktspace.c
)
set(ENGINE_HEADERS
    compat.h binds.h cvar.h discord_wrapper.h dsp_reverb.h editor.h editor_undo.h gameconfig.h gl_console.h gl_misc.h io_system.h main_menu.h map.h math_lib.h model_loader.h network.h particle_system.h physics_wrapper.h sound_system.h texturemanager.h video_player.h weapons.h sentry_wrapper.h
)

add_executable(TectonicEngine ${ENGINE_SOURCES} ${ENGINE_HEADERS})

if(WIN32)
    target_link_options(TectonicEngine PRIVATE
        $<$<CONFIG:Debug,RelWithDebInfo>:/subsystem:console>
        $<$<CONFIG:Release>:/subsystem:windows>
    )
endif()

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
    "${CMAKE_CURRENT_SOURCE_DIR}/sentry/include"
)

target_compile_options(TectonicEngine PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/compat.h">
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-include "${CMAKE_CURRENT_SOURCE_DIR}/compat.h">
)

if(WIN32)
    message(STATUS "Configuring for Windows")
    target_compile_definitions(TectonicEngine PRIVATE _CRT_SECURE_NO_WARNINGS)

    set_target_properties(TectonicEngine PROPERTIES
        COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdb"
        COMPILE_PDB_NAME TectonicEngine
    )
	
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "Configuring for 64-bit (x64) build.")
        set(ARCH_DIR "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(STATUS "Configuring for 32-bit (x86) build.")
        set(ARCH_DIR "x86")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
    else()
        message(FATAL_ERROR "Unsupported architecture size.")
    endif()

    target_link_directories(TectonicEngine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/sdl/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/glew-2.1.0/lib/Release/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/bullet/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/DiscordRPC/lib/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.24.3/libs/${ARCH_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/sentry/lib/${ARCH_DIR}"
    )

    target_link_libraries(TectonicEngine PRIVATE
        SDL2 SDL2_image SDL2_ttf opengl32 glew32
        BulletCollision BulletDynamics LinearMath
        OpenAL32 discord-rpc ws2_32
        sentry
    )

elseif(UNIX AND NOT APPLE)
    message(STATUS "Configuring for Linux")
    find_package(SDL2 REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(OpenGL REQUIRED)

    set(SDL2_IMAGE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/include")
    set(SDL2_TTF_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/include")
    set(SDL2_IMAGE_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/linux/libSDL2_image.a")
    set(SDL2_TTF_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/lib/linux/libSDL2_ttf.a")

    set(BULLET_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bullet/include")
    set(BULLET_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bullet/lib")
    set(BULLET_LIBS
        "${BULLET_LIB_DIR}/linux/libBulletDynamics.a"
        "${BULLET_LIB_DIR}/linux/libBulletCollision.a"
        "${BULLET_LIB_DIR}/linux/libLinearMath.a"
    )

    set(OPENAL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.24.3/include")
    set(OPENAL_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/openal-soft-1.24.3/libs/Linux")
    set(OPENAL_LIB "${OPENAL_LIB_DIR}/libopenal.a")

    set(SENTRY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sentry/include")
    set(SENTRY_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/sentry/lib/linux/libsentry.so")

    target_include_directories(TectonicEngine PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_IMAGE_INCLUDE_DIR}
        ${SDL2_TTF_INCLUDE_DIR}
        ${GLEW_INCLUDE_DIRS}
        ${BULLET_INCLUDE_DIR}
        ${OPENAL_INCLUDE_DIR}
        ${SENTRY_INCLUDE_DIR}
    )

    target_link_directories(TectonicEngine PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/DiscordRPC/lib/linux"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_image-2.8.8/lib/linux"
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL2_ttf-2.24.0/lib/linux"
        ${BULLET_LIB_DIR}
        ${OPENAL_LIB_DIR}
    )

    target_link_libraries(TectonicEngine PRIVATE
        ${SDL2_LIBRARIES}
        ${SDL2_IMAGE_LIBRARY}
        ${SDL2_TTF_LIBRARY}
        ${GLEW_LIBRARIES}
        ${BULLET_LIBS}
        ${OPENAL_LIB}
        ${OPENGL_LIBRARIES}
        ${SENTRY_LIBRARY}
        discord-rpc
        pthread
        dl
        freetype
    )
else()
    message(FATAL_ERROR "Unsupported platform! This project supports only Windows and Linux.")
endif()

add_executable(Batchmodelimporter Batchmodelimporter/main.cpp)
target_compile_definitions(Batchmodelimporter PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(Batchmodelimporter PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
