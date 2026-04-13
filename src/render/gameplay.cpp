#include "render/gameplay.hpp"

#include "entity/manager.hpp"
#include "graphics.hpp"
#include "render/tiles_and_entities.hpp"
#include "state.hpp"
#include "text.hpp"
#include "world_query.hpp"

#include <SDL3/SDL.h>

namespace splonks {

namespace {

void DrawCenteredText(
    SDL_Renderer* renderer,
    Graphics& graphics,
    TextType text_type,
    const char* text,
    float center_x,
    float y,
    SDL_Color color
) {
    int text_width = 0;
    int text_height = 0;
    if (!MeasureText(graphics, text_type, text, &text_width, &text_height)) {
        DrawText(renderer, graphics, text_type, text, center_x, y, color);
        return;
    }
    DrawText(
        renderer,
        graphics,
        text_type,
        text,
        center_x - (static_cast<float>(text_width) / 2.0F),
        y - (static_cast<float>(text_height) / 2.0F),
        color
    );
}

const char* GetStageTransitionTitle(const State& state) {
    switch (state.next_stage.value_or(StageType::Blank)) {
    case StageType::Test1:
        return "Test1";
    case StageType::Blank:
        return "Blank?? Expect crash";
    case StageType::Cave1:
        return "Cave";
    case StageType::Cave2:
        return "Cave 2";
    case StageType::Cave3:
        return "Cave 3";
    case StageType::Ice1:
        return "Ice";
    case StageType::Ice2:
        return "Ice 2";
    case StageType::Ice3:
        return "Ice 3";
    case StageType::Desert1:
        return "Desert";
    case StageType::Desert2:
        return "Desert 2";
    case StageType::Desert3:
        return "Desert 3";
    case StageType::Temple1:
        return "Temple";
    case StageType::Temple2:
        return "Temple 2";
    case StageType::Temple3:
        return "Temple 3";
    case StageType::Boss:
        return "Boss";
    }
    return "This shouldnt be possible...???";
}

const char* GetStageTransitionMessage(const State& state) {
    switch (state.next_stage.value_or(StageType::Blank)) {
    case StageType::Blank:
        return "!!!!expect a crash on a press!!!!";
    case StageType::Test1:
        return "You feel like figuring out bugs...";
    case StageType::Cave1:
        return "You enter the cave...";
    case StageType::Ice1:
        return "Its getting cold...";
    case StageType::Desert1:
        return "This place looks old...";
    case StageType::Temple1:
        return "Cant turn back now...";
    case StageType::Boss:
        return "The end is near...";
    default:
        return "Press [jump] to go deeper...";
    }
}

} // namespace

void RenderPlaying(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    const float base = 5.0F;
    if (graphics.dims.x < 1280U) {
        const float ratio = 1280.0F / static_cast<float>(graphics.dims.x);
        graphics.camera.zoom = base / ratio;
    } else {
        graphics.camera.zoom = base;
    }

    if (state.controlled_entity_vid.has_value()) {
        if (const Entity* const controlled = state.entity_manager.GetEntity(*state.controlled_entity_vid)) {
            if (!graphics.debug_lock_play_camera) {
                const Vec2 delta =
                    GetNearestWorldDelta(state.stage, graphics.play_cam.pos, controlled->pos);
                graphics.play_cam.pos += delta * 0.075F;

                if (state.stage.camera_clamp_enabled) {
                    const Vec2 stage_dims = ToVec2(state.stage.GetStageDims());
                    const Vec2 margin = state.stage.camera_clamp_margin;
                    const Vec2 map_tl_bound = margin;
                    const Vec2 map_br_bound = stage_dims - margin;

                    if (stage_dims.x <= margin.x * 2.0F) {
                        graphics.play_cam.pos.x = stage_dims.x / 2.0F;
                    } else {
                        graphics.play_cam.pos.x = graphics.play_cam.pos.x < map_tl_bound.x
                                                      ? map_tl_bound.x
                                                      : (graphics.play_cam.pos.x > map_br_bound.x
                                                             ? map_br_bound.x
                                                             : graphics.play_cam.pos.x);
                    }

                    if (stage_dims.y <= margin.y * 2.0F) {
                        graphics.play_cam.pos.y = stage_dims.y / 2.0F;
                    } else {
                        graphics.play_cam.pos.y = graphics.play_cam.pos.y < map_tl_bound.y
                                                      ? map_tl_bound.y
                                                      : (graphics.play_cam.pos.y > map_br_bound.y
                                                             ? map_br_bound.y
                                                             : graphics.play_cam.pos.y);
                    }
                }
            }

            graphics.camera.target = graphics.play_cam.pos;
        } else {
            graphics.camera.target = ToVec2(state.stage.GetStageDims()) / 2.0F;
        }
    } else {
        graphics.camera.target = ToVec2(state.stage.GetStageDims()) / 2.0F;
    }

    SDL_SetRenderDrawColor(renderer, 38, 43, 68, 255);
    SDL_RenderClear(renderer);
    RenderStageTileWrapper(renderer, state, graphics);
    RenderStageTiles(renderer, state, graphics);
    RenderEmbeddedTreasureOverlays(renderer, state, graphics);
    RenderBackgroundStamps(renderer, state, graphics);
    RenderEntities(renderer, state, graphics);
    RenderParticles(renderer, state, graphics);
}

void RenderStageTransition(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    const float center_x = static_cast<float>(graphics.dims.x) / 2.0F;
    const float center_y = static_cast<float>(graphics.dims.y) / 2.0F;
    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuTitle,
        GetStageTransitionTitle(state),
        center_x,
        center_y,
        SDL_Color{255, 255, 255, 255}
    );
    DrawText(
        renderer,
        graphics,
        30,
        graphics.ui_font,
        GetStageTransitionMessage(state),
        center_x - (static_cast<float>(graphics.dims.x) * 0.16F),
        center_y + 75.0F,
        SDL_Color{255, 255, 255, 255}
    );
}

void RenderGameOver(SDL_Renderer* renderer, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 50, 0, 0, 255);
    SDL_RenderClear(renderer);

    const float center_x = static_cast<float>(graphics.dims.x) / 2.0F;
    const float center_y = static_cast<float>(graphics.dims.y) / 2.0F;
    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuTitle,
        "Game Over",
        center_x,
        center_y,
        SDL_Color{255, 255, 255, 255}
    );
    DrawText(
        renderer,
        graphics,
        30,
        graphics.ui_font,
        "Press [jump] to try again...",
        center_x - (static_cast<float>(graphics.dims.x) * 0.15F),
        center_y + 75.0F,
        SDL_Color{255, 255, 255, 255}
    );
}

void RenderWin(SDL_Renderer* renderer, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 0, 50, 20, 255);
    SDL_RenderClear(renderer);

    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuTitle,
        "Win! Nice!",
        static_cast<float>(graphics.dims.x) / 2.0F,
        static_cast<float>(graphics.dims.y) / 2.0F,
        SDL_Color{255, 255, 255, 255}
    );
}

} // namespace splonks
