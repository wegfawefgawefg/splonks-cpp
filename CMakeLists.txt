cmake_minimum_required(VERSION 3.20)

project(splonks_cpp VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SPLONKS_STRICT "Enable stricter warning sets." ON)
option(SPLONKS_WARN_AS_ERROR "Treat warnings as errors." ON)

find_package(SDL3 CONFIG QUIET)

if (NOT SDL3_FOUND)
    set(SPLONKS_LOCAL_SDL3_CONFIG
        "/home/vega/Coding/Graphics/hardware-renderer-cpp/build/debug/_deps/sdl3-build"
    )
    if (EXISTS "${SPLONKS_LOCAL_SDL3_CONFIG}/SDL3Config.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${SPLONKS_LOCAL_SDL3_CONFIG}")
        find_package(SDL3 CONFIG QUIET)
    endif()
endif()

if (NOT SDL3_FOUND)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.10
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

find_package(SDL3_ttf CONFIG QUIET)

if (NOT SDL3_ttf_FOUND)
    if (DEFINED sdl3_BINARY_DIR AND EXISTS "${sdl3_BINARY_DIR}/SDL3Config.cmake")
        set(SDL3_DIR "${sdl3_BINARY_DIR}" CACHE PATH "" FORCE)
    endif()
    FetchContent_Declare(
        SDL3_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-3.2.2
        GIT_SHALLOW TRUE
    )
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3_ttf)
endif()

add_executable(splonks-cpp
    src/audio.cpp
    src/direction.cpp
    src/entity.cpp
    src/entity_display_states.cpp
    src/entity_manager.cpp
    src/entities/baseball_bat.cpp
    src/entities/bat.cpp
    src/entities/block.cpp
    src/entities/bomb.cpp
    src/entities/breakaway_container.cpp
    src/entities/common.cpp
    src/entities/ghost_ball.cpp
    src/entities/jetpack.cpp
    src/entities/mod.cpp
    src/entities/money.cpp
    src/entities/mouse_trailer.cpp
    src/entities/player.cpp
    src/entities/rock.cpp
    src/entities/rope.cpp
    src/graphics.cpp
    src/inputs.cpp
    src/main.cpp
    src/menu_settings.cpp
    src/menu_title.cpp
    src/menu_video.cpp
    src/on_damage_effects.cpp
    src/room.cpp
    src/render.cpp
    src/render_debug.cpp
    src/render_tiles_and_entities.cpp
    src/render_ui.cpp
    src/settings.cpp
    src/sid.cpp
    src/sprite.cpp
    src/stage.cpp
    src/stage_init.cpp
    src/stage_gen/cave.cpp
    src/stage_gen/mod.cpp
    src/stage_gen/test.cpp
    src/state.cpp
    src/step.cpp
    src/step_entities.cpp
    src/text.cpp
    src/special_effects/dynamic_effect.cpp
    src/special_effects/mod.cpp
    src/special_effects/special_effect.cpp
    src/special_effects/spline_effect.cpp
    src/special_effects/static_effect.cpp
    src/tile.cpp
    src/utils.cpp
    src/systems/controls.cpp
    src/systems/mod.cpp
    src/special_effects/ultra_dynamic_effect.cpp
)

target_include_directories(
    splonks-cpp
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../gubsy/third_party"
)
target_compile_features(splonks-cpp PRIVATE cxx_std_20)
target_link_libraries(splonks-cpp PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)

if (MSVC)
    target_compile_options(splonks-cpp PRIVATE /W4 /permissive-)
    if (SPLONKS_WARN_AS_ERROR)
        target_compile_options(splonks-cpp PRIVATE /WX)
    endif()
else()
    target_compile_options(
        splonks-cpp
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-function
            $<$<BOOL:${SPLONKS_STRICT}>:-Wconversion>
            $<$<BOOL:${SPLONKS_STRICT}>:-Wsign-conversion>
            $<$<BOOL:${SPLONKS_STRICT}>:-Wshadow>
            $<$<BOOL:${SPLONKS_STRICT}>:-Wnon-virtual-dtor>
            $<$<BOOL:${SPLONKS_WARN_AS_ERROR}>:-Werror>
    )
endif()

set_target_properties(splonks-cpp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
