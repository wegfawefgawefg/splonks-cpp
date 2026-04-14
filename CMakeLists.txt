cmake_minimum_required(VERSION 3.20)

project(splonks_cpp VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SPLONKS_STRICT "Enable stricter warning sets." ON)
option(SPLONKS_WARN_AS_ERROR "Treat warnings as errors." ON)

find_package(SDL3 3.4.0 CONFIG QUIET)

if (NOT SDL3_FOUND)
    set(SPLONKS_LOCAL_SDL3_CONFIG
        "/home/vega/Coding/Graphics/hardware-renderer-cpp/build/debug/_deps/sdl3-build"
    )
    if (EXISTS "${SPLONKS_LOCAL_SDL3_CONFIG}/SDL3Config.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${SPLONKS_LOCAL_SDL3_CONFIG}")
        find_package(SDL3 3.4.0 CONFIG QUIET)
    endif()
endif()

if (NOT SDL3_FOUND)
    set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.0
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

find_package(SDL3_image CONFIG QUIET)

if (NOT SDL3_image_FOUND)
    if (DEFINED sdl3_BINARY_DIR AND EXISTS "${sdl3_BINARY_DIR}/SDL3Config.cmake")
        set(SDL3_DIR "${sdl3_BINARY_DIR}" CACHE PATH "" FORCE)
    endif()
    FetchContent_Declare(
        SDL3_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG release-3.2.0
        GIT_SHALLOW TRUE
    )
    set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JPG OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_QOI OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_BMP OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_GIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_LBM OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_PCX OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_PNM OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_SVG OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TGA OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_XCF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_XPM OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_XV OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3_image)
endif()

find_package(SDL3_mixer CONFIG QUIET)

if (NOT SDL3_mixer_FOUND)
    if (DEFINED sdl3_BINARY_DIR AND EXISTS "${sdl3_BINARY_DIR}/SDL3Config.cmake")
        set(SDL3_DIR "${sdl3_BINARY_DIR}" CACHE PATH "" FORCE)
    endif()
    FetchContent_Declare(
        SDL3_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
        GIT_TAG release-3.2.0
        GIT_SHALLOW TRUE
    )
    set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLMIXER_WERROR OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_FLAC OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_MP3 OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_VORBIS_STB ON CACHE BOOL "" FORCE)
    set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_VORBIS_TREMOR OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3_mixer)
endif()

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.7
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_executable(splonks-cpp
    src/audio.cpp
    src/cli.cpp
    src/contact_bookkeeping.cpp
    src/debug/playback_core.cpp
    src/debug/playback_recording_io.cpp
    src/debug/playback_text_export.cpp
    src/debug/playback_snapshot_state.cpp
    src/debug/playback_ui_common.cpp
    src/debug/playback_ui_entities.cpp
    src/debug/playback_ui_menu.cpp
    src/debug/playback_ui_settings.cpp
    src/direction.cpp
    src/entity.cpp
    src/entity_tool_inventory.cpp
    src/entity/archetype.cpp
    src/entity/archetype_restore.cpp
    src/entity/archetype_registry.cpp
    src/entity/display_states.cpp
    src/entity/manager.cpp
    src/entities/altar.cpp
    src/entities/arrow_trap.cpp
    src/entities/baseball_bat.cpp
    src/entities/bat.cpp
    src/entities/block.cpp
    src/entities/bomb.cpp
    src/entities/bow.cpp
    src/entities/box.cpp
    src/entities/caveman.cpp
    src/entities/chest.cpp
    src/entities/common/contact_damage.cpp
    src/entities/common/damage.cpp
    src/entities/common/explosion.cpp
    src/entities/common/frame.cpp
    src/entities/common/ground_walker.cpp
    src/entities/common/hang.cpp
    src/entities/common/physics.cpp
    src/entities/common/blocking_contact.cpp
    src/entities/common/entity_contact.cpp
    src/entities/common/tile_contact.cpp
    src/entities/common/step.cpp
    src/entities/common/collect.cpp
    src/entities/common/stomp.cpp
    src/entities/common/push.cpp
    src/entities/common/carry.cpp
    src/entities/common/throw.cpp
    src/entities/common/tool_use.cpp
    src/entities/damsel.cpp
    src/entities/dvdlogo.cpp
    src/entities/dice.cpp
    src/entities/emerald_big.cpp
    src/entities/gear_items.cpp
    src/entities/ghost_ball.cpp
    src/entities/pot.cpp
    src/entities/giant_tiki_head.cpp
    src/entities/gold_idol.cpp
    src/entities/jetpack.cpp
    src/entities/kali_head.cpp
    src/entities/lantern.cpp
    src/entities/machete.cpp
    src/entities/mattock.cpp
    src/entities/none_archetype.cpp
    src/entities/money.cpp
    src/entities/mouse_trailer.cpp
    src/entities/player.cpp
    src/entities/rock.cpp
    src/entities/rope.cpp
    src/entities/ruby_big.cpp
    src/entities/sac_altar.cpp
    src/entities/scarab.cpp
    src/entities/shotgun.cpp
    src/entities/shopkeeper.cpp
    src/entities/sign.cpp
    src/entities/sapphire_big.cpp
    src/entities/snake.cpp
    src/entities/spider.cpp
    src/entities/spider_hang.cpp
    src/entities/stomp_pad.cpp
    src/entities/teleporter.cpp
    src/entities/web_cannon.cpp
    src/entities/pistol.cpp
    src/frame_data_animator.cpp
    src/frame_data.cpp
    src/graphics.cpp
    src/hitscan.cpp
    src/imgui_layer.cpp
    src/inputs.cpp
    src/main.cpp
    src/menu/lighting.cpp
    src/menu/settings.cpp
    src/menu/postfx.cpp
    src/menu/title.cpp
    src/menu/ui.cpp
    src/menu/video.cpp
    src/on_damage_effects.cpp
    src/room.cpp
    src/render/render.cpp
    src/render/menu/common.cpp
    src/render/menu/top_level.cpp
    src/render/menu/settings.cpp
    src/render/menu/lighting.cpp
    src/render/gameplay.cpp
    src/render/postfx.cpp
    src/render/tile_lighting.cpp
    src/render/debug.cpp
    src/render/stone_overlay.cpp
    src/render/tiles_and_entities.cpp
    src/render/ui.cpp
    src/raw_frame_data.cpp
    src/settings.cpp
    src/sid.cpp
    src/stage.cpp
    src/stage_init.cpp
    src/stage_wrap.cpp
    src/stage_gen/cave.cpp
    src/stage_gen/splk_mines.cpp
    src/stage_gen/test.cpp
    src/stage_break.cpp
    src/stage_progression.cpp
    src/state.cpp
    src/step.cpp
    src/step_entities.cpp
    src/text.cpp
    src/world_query.cpp
    src/tools/tool_archetype.cpp
    src/tools/throw_bomb.cpp
    src/tools/throw_pot.cpp
    src/tools/throw_rope.cpp
    src/stage_lighting.cpp
    src/particles/dynamic_particle.cpp
    src/particles/system.cpp
    src/particles/particle.cpp
    src/particles/spline_particle.cpp
    src/particles/static_particle.cpp
    src/tile.cpp
    src/tile_archetype.cpp
    src/tile_source_data.cpp
    src/utils.cpp
    src/controls.cpp
    src/particles/ultra_dynamic_particle.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

set(SPLONKS_IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(
    splonks-cpp
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../gubsy/third_party"
        "${imgui_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}/backends"
)
target_compile_features(splonks-cpp PRIVATE cxx_std_20)
target_link_libraries(
    splonks-cpp
    PRIVATE
        SDL3::SDL3
        SDL3_ttf::SDL3_ttf
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer
)

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

if (NOT MSVC)
    set_source_files_properties(
        ${SPLONKS_IMGUI_SOURCES}
        PROPERTIES
            COMPILE_OPTIONS "-Wno-error;-Wno-conversion;-Wno-sign-conversion;-Wno-shadow"
    )
endif()

set_target_properties(splonks-cpp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
