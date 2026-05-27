cmake_minimum_required(VERSION 3.20)

project(splonks_cpp VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SPLONKS_MODE "developer" CACHE STRING "Splonks build mode: developer or release")
set_property(CACHE SPLONKS_MODE PROPERTY STRINGS developer release)
if (NOT SPLONKS_MODE MATCHES "^(developer|release)$")
    message(FATAL_ERROR "SPLONKS_MODE must be one of: developer, release")
endif()

option(SPLONKS_STRICT "Enable stricter warning sets." ON)
option(SPLONKS_WARN_AS_ERROR "Treat warnings as errors." ON)
option(SPLONKS_FETCH_DEPS "Fetch missing SDL/imgui dependencies with CMake FetchContent" ON)
set(SPLONKS_IOS_DEVELOPMENT_TEAM "" CACHE STRING "Apple Developer Team ID for iOS device builds")
set(SPLONKS_IOS_CODE_SIGN_IDENTITY "Apple Development" CACHE STRING "Code signing identity for iOS device builds")

find_package(SDL3 3.4.0 CONFIG QUIET)

if (NOT SDL3_FOUND)
    if (SPLONKS_FETCH_DEPS)
        set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            # release-3.4.0
            GIT_TAG a962f40bbba175e9716557a25d5d7965f134a3d3
        )
        FetchContent_MakeAvailable(SDL3)
    else()
        message(FATAL_ERROR "SDL3 not found. Install/provide SDL3 or enable SPLONKS_FETCH_DEPS.")
    endif()
endif()

find_package(SDL3_ttf CONFIG QUIET)

if (NOT SDL3_ttf_FOUND AND SPLONKS_FETCH_DEPS)
    if (DEFINED sdl3_BINARY_DIR AND EXISTS "${sdl3_BINARY_DIR}/SDL3Config.cmake")
        set(SDL3_DIR "${sdl3_BINARY_DIR}" CACHE PATH "" FORCE)
    endif()
    FetchContent_Declare(
        SDL3_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        # release-3.2.2
        GIT_TAG a1ce3670aec736ecbf0936c43f2f0cc53aa61e5b
    )
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3_ttf)
elseif (NOT SDL3_ttf_FOUND)
    message(FATAL_ERROR "SDL3_ttf not found. Install/provide SDL3_ttf or enable SPLONKS_FETCH_DEPS.")
endif()

find_package(SDL3_image CONFIG QUIET)

if (NOT SDL3_image_FOUND AND SPLONKS_FETCH_DEPS)
    if (DEFINED sdl3_BINARY_DIR AND EXISTS "${sdl3_BINARY_DIR}/SDL3Config.cmake")
        set(SDL3_DIR "${sdl3_BINARY_DIR}" CACHE PATH "" FORCE)
    endif()
    FetchContent_Declare(
        SDL3_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        # release-3.2.0
        GIT_TAG 4a762bdfb7b43dae7a8a818567847881e49bdab4
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
elseif (NOT SDL3_image_FOUND)
    message(FATAL_ERROR "SDL3_image not found. Install/provide SDL3_image or enable SPLONKS_FETCH_DEPS.")
endif()

find_package(SDL3_mixer CONFIG QUIET)

if (NOT SDL3_mixer_FOUND AND SPLONKS_FETCH_DEPS)
    if (DEFINED sdl3_BINARY_DIR AND EXISTS "${sdl3_BINARY_DIR}/SDL3Config.cmake")
        set(SDL3_DIR "${sdl3_BINARY_DIR}" CACHE PATH "" FORCE)
    endif()
    FetchContent_Declare(
        SDL3_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
        # release-3.2.0
        GIT_TAG cedfeef30e93db35eee6b25759117da63f8e5a4f
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
elseif (NOT SDL3_mixer_FOUND)
    message(FATAL_ERROR "SDL3_mixer not found. Install/provide SDL3_mixer or enable SPLONKS_FETCH_DEPS.")
endif()

if (SPLONKS_FETCH_DEPS)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        # v1.92.7
        GIT_TAG dac07199cfd761113d966eb8ad739254e10df2fe
    )
    FetchContent_MakeAvailable(imgui)
else()
    message(FATAL_ERROR "imgui dependency is currently only available through SPLONKS_FETCH_DEPS.")
endif()

set(SPLONKS_SOURCES
    src/audio.cpp
    src/audio_asset.cpp
    src/audio_acoustics.cpp
    src/audio_emitters.cpp
    src/audio_filter.cpp
    src/audio_sound_effects.cpp
    src/cli.cpp
    src/cli_state_smoke.cpp
    src/content_names.cpp
    src/buying.cpp
    src/contact_bookkeeping.cpp
    src/debug/playback_core.cpp
    src/debug/playback_recording_io.cpp
    src/debug/playback_text_export.cpp
    src/debug/playback_snapshot_state.cpp
    src/debug/input_bot.cpp
    src/debug/control_server.cpp
    src/debug/playback_ui_common.cpp
    src/debug/playback_ui_ents.cpp
    src/debug/playback_ui_menu.cpp
    src/debug/playback_ui_network.cpp
    src/debug/playback_ui_settings.cpp
    src/debug/debug_stage_basic.cpp
    src/debug/debug_stage_boulder_sacrifice.cpp
    src/debug/debug_stage_lighting.cpp
    src/debug/debug_stage_platforms.cpp
    src/debug/debug_stage_scenarios.cpp
    src/debug/debug_stage_traps.cpp
    src/debug/debug_stage_init.cpp
    src/debug/shop_test_stage.cpp
    src/direction.cpp
    src/effects.cpp
    src/effects/specs.cpp
    src/effects/render.cpp
    src/effects/treasure_pickup.cpp
    src/ent.cpp
    src/ent_tool_inventory.cpp
    src/ent/spec.cpp
    src/ent/spec_restore.cpp
    src/ent/spec_registry.cpp
    src/ent/display_states.cpp
    src/ent/manager.cpp
    src/ents/altar.cpp
    src/ents/arrow_trap.cpp
    src/ents/ball_and_chain.cpp
    src/ents/baseball_bat.cpp
    src/ents/basic_exit.cpp
    src/ents/bat.cpp
    src/ents/block.cpp
    src/ents/boulder.cpp
    src/ents/bomb.cpp
    src/ents/bow.cpp
    src/ents/box.cpp
    src/ents/caveman.cpp
    src/ents/chest.cpp
    src/ents/cobra.cpp
    src/ents/craps_table.cpp
    src/ents/debug_moving_light.cpp
    src/ents/jaws.cpp
    src/ents/crystal_skull.cpp
    src/ents/bones.cpp
    src/ents/trap_block.cpp
    src/ents/ceiling_trap.cpp
    src/ents/tomb_lord.cpp
    src/ents/door.cpp
    src/ents/entrance.cpp
    src/ents/ankh.cpp
    src/ents/crown.cpp
    src/ents/xoc_block.cpp
    src/ents/moai.cpp
    src/ents/yeti.cpp
    src/ents/alien_ship.cpp
    src/ents/alien_boss.cpp
    src/ents/barrier_emitter.cpp
    src/ents/thwomp_trap.cpp
    src/ents/lamp.cpp
    src/ents/mantrap.cpp
    src/ents/frog.cpp
    src/ents/monkey.cpp
    src/ents/piranha.cpp
    src/ents/zombie.cpp
    src/ents/vampire.cpp
    src/ents/ufo.cpp
    src/ents/hawkman.cpp
    src/ents/skeleton.cpp
    src/ents/common/contact_damage.cpp
    src/ents/common/damage.cpp
    src/ents/common/explosion.cpp
    src/ents/common/frame.cpp
    src/ents/common/ground_walker.cpp
    src/ents/common/hang.cpp
    src/ents/common/knockback.cpp
    src/ents/common/physics.cpp
    src/ents/common/blocking_contact.cpp
    src/ents/common/ent_contact.cpp
    src/ents/common/tile_contact.cpp
    src/ents/common/step.cpp
    src/ents/common/collect.cpp
    src/ents/common/stomp.cpp
    src/ents/common/push.cpp
    src/ents/common/carry.cpp
    src/ents/common/throw.cpp
    src/ents/common/tool_use.cpp
    src/ents/damsel.cpp
    src/ents/dvdlogo.cpp
    src/ents/dice.cpp
    src/ents/emerald_big.cpp
    src/ents/flappy_bee.cpp
    src/ents/flesh_guy.cpp
    src/ents/gear_items.cpp
    src/ents/ghost_ball.cpp
    src/ents/pot.cpp
    src/ents/giant_tiki_head.cpp
    src/ents/gold_idol.cpp
    src/ents/jetpack.cpp
    src/ents/kali_head.cpp
    src/ents/lantern.cpp
    src/ents/machete.cpp
    src/ents/mattock.cpp
    src/ents/meathead.cpp
    src/ents/none_spec.cpp
    src/ents/money.cpp
    src/ents/moving_platform.cpp
    src/ents/mouse_trailer.cpp
    src/ents/player.cpp
    src/ents/rock.cpp
    src/ents/rope.cpp
    src/ents/ruby_big.cpp
    src/ents/sac_altar.cpp
    src/ents/sac_altar_topper.cpp
    src/ents/scarab.cpp
    src/ents/shotgun.cpp
    src/ents/shop.cpp
    src/ents/shop_tile_triggers.cpp
    src/ents/shopkeeper.cpp
    src/ents/sign.cpp
    src/ents/sapphire_big.cpp
    src/ents/snake.cpp
    src/ents/spider.cpp
    src/ents/spider_hang.cpp
    src/ents/stomp_pad.cpp
    src/ents/store_light.cpp
    src/ents/teleporter.cpp
    src/ents/web_cannon.cpp
    src/ents/pistol.cpp
    src/aframe_animator.cpp
    src/aframe.cpp
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
    src/network/input_lockstep.cpp
    src/network/net_fuzzer.cpp
    src/network/net_ent_links.cpp
    src/network/net_lobby.cpp
    src/network/net_lobby_endpoint.cpp
    src/network/net_lobby_input_lockstep.cpp
    src/network/net_lobby_join.cpp
    src/network/net_lobby_packet_pump.cpp
    src/network/net_lobby_packets.cpp
    src/network/net_lobby_player_lifecycle.cpp
    src/network/net_lobby_retained_player.cpp
    src/network/net_lobby_session.cpp
    src/network/net_lobby_stage.cpp
    src/network/net_progression.cpp
    src/network/net_protocol.cpp
    src/network/net_session.cpp
    src/network/net_transport.cpp
    src/on_damage_effects.cpp
    src/player_registry.cpp
    src/pres_commands.cpp
    src/room.cpp
    src/render/render.cpp
    src/render/camera.cpp
    src/render/menu/common.cpp
    src/render/menu/top_level.cpp
    src/render/menu/settings.cpp
    src/render/menu/lighting.cpp
    src/render/gameplay.cpp
    src/hud/entries.cpp
    src/render/particles.cpp
    src/render/postfx.cpp
    src/render/tile_lighting.cpp
    src/render/debug.cpp
    src/render/debug_lighting.cpp
    src/render/ents.cpp
    src/render/stone_overlay.cpp
    src/render/tiles_and_ents.cpp
    src/render/ui.cpp
    src/render/world_texture.cpp
    src/raw_audio_asset.cpp
    src/raw_aframe.cpp
    src/quest.cpp
    src/quest_glyphs.cpp
    src/quest_parse_utils.cpp
    src/quest_pools.cpp
    src/quest_stage_config.cpp
    src/quest_stage_loader.cpp
    src/settings.cpp
    src/sid.cpp
    src/stage.cpp
    src/stage_bounds.cpp
    src/stage_factory.cpp
    src/stage_queries.cpp
    src/stage_fluids.cpp
    src/stage_init.cpp
    src/stage_spawning.cpp
    src/stage_rotation.cpp
    src/stage_shake.cpp
    src/stage_wrap.cpp
    src/stage_tile_triggers.cpp
    src/state_fingerprint.cpp
    src/water.cpp
    src/world_ops/ent.cpp
    src/world_ops/tile.cpp
    src/stage_gen/cave.cpp
    src/stage_gen/classic/ambient_passes.cpp
    src/stage_gen/classic/glyph_actions.cpp
    src/stage_gen/classic/item_pools.cpp
    src/stage_gen/classic/room_layout.cpp
    src/stage_gen/classic/room_templates.cpp
    src/stage_gen/classic/stage_pass_helpers.cpp
    src/stage_gen/classic/stage_passes.cpp
    src/stage_gen/classic/tile_palette.cpp
    src/stage_gen/classic/treasure_passes.cpp
    src/stage_gen/room_template_loader.cpp
    src/stage_gen/classic/stagegen.cpp
    src/stage_gen/test.cpp
    src/stage_break.cpp
    src/stage_acoustics.cpp
    src/stage_progression.cpp
    src/state.cpp
    src/step.cpp
    src/step_ents.cpp
    src/text.cpp
    src/world_query.cpp
    src/tools/tool_spec.cpp
    src/tools/throw_bomb.cpp
    src/tools/throw_pot.cpp
    src/tools/throw_rope.cpp
    src/stage_lighting.cpp
    src/particles/particle_specs.cpp
    src/particles/ribbon_particle.cpp
    src/particles/scripted_particle.cpp
    src/particles/segmented_sprite_particle.cpp
    src/particles/sprite_particle.cpp
    src/particles/system.cpp
    src/tile.cpp
    src/tile_spec.cpp
    src/tile_contact_data.cpp
    src/tile_source_data.cpp
    src/utils.cpp
    src/controls.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

if (ANDROID)
    add_library(splonks-cpp SHARED ${SPLONKS_SOURCES})
else()
    add_executable(splonks-cpp ${SPLONKS_SOURCES})
endif()

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
if (WIN32)
    target_link_libraries(splonks-cpp PRIVATE ws2_32)
endif()

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

if (ANDROID)
    set_target_properties(splonks-cpp PROPERTIES
        OUTPUT_NAME main
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(SPLONKS_IOS_CODE_SIGNING_ALLOWED "NO")
    if (CMAKE_OSX_SYSROOT STREQUAL "iphoneos")
        set(SPLONKS_IOS_CODE_SIGNING_ALLOWED "YES")
    endif()

    set_target_properties(splonks-cpp PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.splonks.game"
        MACOSX_BUNDLE_BUNDLE_NAME "Splonks"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "${SPLONKS_IOS_CODE_SIGNING_ALLOWED}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "dev.splonks.game"
    )
    if (SPLONKS_IOS_DEVELOPMENT_TEAM)
        set_target_properties(splonks-cpp PROPERTIES
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${SPLONKS_IOS_DEVELOPMENT_TEAM}"
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${SPLONKS_IOS_CODE_SIGN_IDENTITY}"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        )
    endif()
    add_custom_command(TARGET splonks-cpp POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_BUNDLE_DIR:splonks-cpp>/assets"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data"
            "$<TARGET_BUNDLE_DIR:splonks-cpp>/data"
        COMMENT "Copying Splonks content into iOS app bundle"
    )
endif()
