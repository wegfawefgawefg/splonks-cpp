#include "render/gameplay.hpp"

#include "entity/manager.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "render/tiles_and_entities.hpp"
#include "state.hpp"
#include "stage_progression.hpp"
#include "text.hpp"
#include "world_query.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

namespace splonks {

namespace {

Vec2 ClampCameraTargetToStage(const Stage& stage, Vec2 target) {
    if (!stage.camera_clamp_enabled) {
        return target;
    }

    const Vec2 stage_dims = ToVec2(stage.GetStageDims());
    const Vec2 margin = stage.camera_clamp_margin;
    const Vec2 map_tl_bound = margin;
    const Vec2 map_br_bound = stage_dims - margin;

    if (stage_dims.x <= margin.x * 2.0F) {
        target.x = stage_dims.x / 2.0F;
    } else {
        target.x = std::clamp(target.x, map_tl_bound.x, map_br_bound.x);
    }

    if (stage_dims.y <= margin.y * 2.0F) {
        target.y = stage_dims.y / 2.0F;
    } else {
        target.y = std::clamp(target.y, map_tl_bound.y, map_br_bound.y);
    }

    return target;
}

void LerpCamera(Graphics& graphics, const Vec2& target, float zoom) {
    const float t = std::clamp(graphics.camera_lerp_factor, 0.0F, 1.0F);
    graphics.camera.target += (target - graphics.camera.target) * t;
    graphics.camera.zoom += (zoom - graphics.camera.zoom) * t;
}

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

const char* GetStageTypeTransitionTitle(StageType stage_type) {
    switch (stage_type) {
    case StageType::Test1:
        return "Test1";
    case StageType::Blank:
        return "Blank?? Expect crash";
    case StageType::SplkMines1:
        return "Mines";
    case StageType::SplkMines2:
        return "Mines 2";
    case StageType::SplkMines3:
        return "Mines 3";
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

const char* GetStageTypeTransitionMessage(StageType stage_type) {
    switch (stage_type) {
    case StageType::Blank:
        return "!!!!expect a crash on a press!!!!";
    case StageType::Test1:
        return "You feel like figuring out bugs...";
    case StageType::SplkMines1:
        return "You enter the mines...";
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

const char* GetStageTransitionTitle(const State& state) {
    if (!state.pending_stage_transition.has_value()) {
        return "No Transition";
    }

    const StageLoadTarget& target = state.pending_stage_transition->destination;
    if (target.kind == StageLoadTargetKind::DebugLevel) {
        return GetDebugLevelKindName(target.debug_level);
    }
    return GetStageTypeTransitionTitle(target.stage_type);
}

const char* GetStageTransitionMessage(const State& state) {
    if (!state.pending_stage_transition.has_value()) {
        return "Press [jump] to continue...";
    }

    const StageLoadTarget& target = state.pending_stage_transition->destination;
    if (target.kind == StageLoadTargetKind::DebugLevel) {
        return "Loading debug level...";
    }
    return GetStageTypeTransitionMessage(target.stage_type);
}

} // namespace

void RenderPlaying(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    Vec2 target = GetStageCameraCenter(state.stage);
    float zoom = GetDefaultFollowCameraZoom(graphics);

    const Entity* camera_target_entity = nullptr;
    if (state.controlled_entity_vid.has_value()) {
        camera_target_entity = state.entity_manager.GetEntity(*state.controlled_entity_vid);
    }
    if ((camera_target_entity == nullptr || !camera_target_entity->active) &&
        state.mode == Mode::GameOver && state.player_vid.has_value()) {
        camera_target_entity = state.entity_manager.GetEntity(*state.player_vid);
    }

    if (graphics.camera_mode == CameraMode::StageFit) {
        zoom = GetStageFitCameraZoom(state.stage, graphics);
    } else if (camera_target_entity != nullptr && camera_target_entity->active) {
        if (!graphics.debug_lock_play_camera) {
            const Vec2 controlled_visual_center =
                entities::common::GetVisualCenterForEntity(*camera_target_entity, graphics, camera_target_entity->GetCenter());
            const Vec2 delta = GetNearestWorldDelta(
                state.stage,
                graphics.play_cam.pos,
                controlled_visual_center
            );
            graphics.play_cam.pos += delta * 0.075F;
            graphics.play_cam.pos = ClampCameraTargetToStage(state.stage, graphics.play_cam.pos);
        }
        target = graphics.play_cam.pos;
    } else if (state.mode == Mode::GameOver && state.gameplay_camera_anchor_world_pos.has_value()) {
        if (!graphics.debug_lock_play_camera) {
            graphics.play_cam.pos = ClampCameraTargetToStage(
                state.stage,
                *state.gameplay_camera_anchor_world_pos
            );
        }
        target = graphics.play_cam.pos;
    }

    zoom *= graphics.camera_zoom_multiplier;
    LerpCamera(graphics, target, zoom);

    SDL_SetRenderDrawColor(renderer, 38, 43, 68, 255);
    SDL_RenderClear(renderer);
    RenderStageTileWrapper(renderer, state, graphics);
    RenderStageTiles(renderer, state, graphics);
    RenderEmbeddedTreasureOverlays(renderer, state, graphics);
    RenderBackgroundStamps(renderer, state, graphics);
    RenderEntities(renderer, state, graphics);
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

void RenderGameOver(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    RenderPlaying(renderer, state, graphics);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 84);
    SDL_FRect overlay{
        0.0F,
        0.0F,
        static_cast<float>(graphics.dims.x),
        static_cast<float>(graphics.dims.y),
    };
    SDL_RenderFillRect(renderer, &overlay);

    const float center_x = static_cast<float>(graphics.dims.x) / 2.0F;
    const float center_y = static_cast<float>(graphics.dims.y) / 2.0F;
    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuTitle,
        "Game Over",
        center_x,
        center_y - 24.0F,
        SDL_Color{255, 255, 255, 255}
    );

    const char* prompt = state.scene_frame >= 60
        ? "Press [jump] to try again..."
        : "...";
    DrawText(
        renderer,
        graphics,
        30,
        graphics.ui_font,
        prompt,
        center_x - (static_cast<float>(graphics.dims.x) * 0.15F),
        center_y + 48.0F,
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
