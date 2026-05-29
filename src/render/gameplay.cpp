#include "render/gameplay.hpp"

#include "ent/manager.hpp"
#include "ents/common/common.hpp"
#include "graphics.hpp"
#include "network/net_lobby.hpp"
#include "player_queries.hpp"
#include "render/camera.hpp"
#include "render/tiles_and_ents.hpp"
#include "state.hpp"
#include "stage_progression.hpp"
#include "text.hpp"
#include "world_query.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace splonks {

namespace {

struct CameraFocus {
    Vec2 target = Vec2::New(0.0F, 0.0F);
    float zoom = 1.0F;
    bool has_target = false;
};

struct CameraViewBounds {
    Vec2 tl = Vec2::New(0.0F, 0.0F);
    Vec2 br = Vec2::New(0.0F, 0.0F);
};

void LerpCamera(Graphics& graphics, const Vec2& target, float zoom) {
    const float t = std::clamp(graphics.camera_lerp_factor, 0.0F, 1.0F);
    graphics.camera.target += (target - graphics.camera.target) * t;
    graphics.camera.zoom += (zoom - graphics.camera.zoom) * t;
}

Vec2 RotateWorldPointForActiveWorldRotation(const Graphics& graphics, const Vec2& world_pos) {
    if (!graphics.world_rotation_active) {
        return world_pos;
    }

    constexpr float kDegreesToRadians = 3.14159265358979323846F / 180.0F;
    const float radians = graphics.world_rotation_degrees * kDegreesToRadians;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    const Vec2 delta = world_pos - graphics.world_rotation_pivot;
    return graphics.world_rotation_pivot + Vec2::New(
        (delta.x * c) - (delta.y * s),
        (delta.x * s) + (delta.y * c)
    );
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
    }
    return "This shouldnt be possible...???";
}

const char* GetStageTypeTransitionMessage(StageType stage_type) {
    switch (stage_type) {
    case StageType::Blank:
        return "!!!!expect a crash on a press!!!!";
    case StageType::Test1:
        return "You feel like figuring out bugs...";
    }
    return "Press [jump] to go deeper...";
}

const char* GetStageTransitionTitle(const State& state) {
    if (!state.pending_stage_transition.has_value()) {
        return "No Transition";
    }

    const StageLoadTarget& target = state.pending_stage_transition->destination;
    if (target.kind == StageLoadTargetKind::DebugLevel) {
        return GetDebugLevelKindName(target.debug_level);
    }
    if (target.kind == StageLoadTargetKind::QuestStage) {
        return target.quest_stage_id.data();
    }
    return GetStageTypeTransitionTitle(target.stage_type);
}

const char* GetStageTransitionMessage(const State& state) {
    if (!state.pending_stage_transition.has_value()) {
        return "Press [jump] to continue...";
    }

    if (state.net_session.role != network::NetRole::Offline) {
        if (state.scene_frame < 60) {
            return "Preparing synced stage...";
        }
        if (network::IsInputLockstepCatchupBlocking(state)) {
            return "Synchronizing with host...";
        }
        return "Entering game...";
    }

    if (state.scene_frame < 60) {
        return "";
    }

    if (state.pending_stage_transition->destination.kind != StageLoadTargetKind::StageType) {
        return "Press [jump] to continue...";
    }

    const StageLoadTarget& target = state.pending_stage_transition->destination;
    return GetStageTypeTransitionMessage(target.stage_type);
}

CameraViewBounds GetCameraViewBounds(const Graphics& graphics, const Vec2& target, float zoom) {
    return CameraViewBounds{
        .tl = target - (graphics.camera.offset / zoom),
        .br = target + ((ToVec2(graphics.dims) - graphics.camera.offset) / zoom),
    };
}

Vec2 GetCameraTargetForCenteredViewBounds(
    const Graphics& graphics,
    const CameraViewBounds& bounds,
    float zoom
) {
    const Vec2 center = (bounds.tl + bounds.br) / 2.0F;
    const Vec2 centered_offset = graphics.camera.offset - (ToVec2(graphics.dims) / 2.0F);
    return center + (centered_offset / zoom);
}

CameraFocus ComputeLocalPlayerCameraFocus(const State& state, const Graphics& graphics) {
    const Ent* anchor_ent = GetPrimaryLocalPlayer(state);
    if (anchor_ent == nullptr || !anchor_ent->active ||
        anchor_ent->condition == EntCondition::Dead) {
        anchor_ent = nullptr;
        for (const PlayerSlot& slot : state.players.slots) {
            if (!slot.connected || slot.connection_kind != PlayerConnectionKind::Local ||
                !slot.ent_vid.has_value()) {
                continue;
            }
            const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
            if (player != nullptr && player->active && player->condition != EntCondition::Dead) {
                anchor_ent = player;
                break;
            }
        }
    }
    if (anchor_ent == nullptr) {
        return {};
    }

    const Vec2 anchor_center =
        ents::common::GetVisualCenterForEnt(*anchor_ent, graphics, anchor_ent->GetCenter());
    const float default_zoom = GetDefaultFollowCameraZoom(graphics);
    Vec2 union_tl = Vec2::New(
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    );
    Vec2 union_br = Vec2::New(
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    );
    std::size_t count = 0;

    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.connection_kind != PlayerConnectionKind::Local ||
            !slot.ent_vid.has_value()) {
            continue;
        }
        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || !player->active || player->condition == EntCondition::Dead) {
            continue;
        }

        const Vec2 visual_center =
            ents::common::GetVisualCenterForEnt(*player, graphics, player->GetCenter());
        const Vec2 local_center =
            anchor_center + GetNearestWorldDelta(state.stage, anchor_center, visual_center);
        const Vec2 single_camera_target = ClampCameraTargetToStage(state.stage, local_center);
        const CameraViewBounds player_view =
            GetCameraViewBounds(graphics, single_camera_target, default_zoom);
        union_tl.x = std::min(union_tl.x, player_view.tl.x);
        union_tl.y = std::min(union_tl.y, player_view.tl.y);
        union_br.x = std::max(union_br.x, player_view.br.x);
        union_br.y = std::max(union_br.y, player_view.br.y);
        ++count;
    }

    if (count == 0) {
        return {};
    }

    CameraFocus focus;
    const CameraViewBounds union_view = CameraViewBounds{
        .tl = union_tl,
        .br = union_br,
    };
    focus.target = GetCameraTargetForCenteredViewBounds(graphics, union_view, default_zoom);
    focus.zoom = default_zoom;
    focus.has_target = true;

    if (count > 1) {
        const float required_width = std::max(1.0F, union_br.x - union_tl.x);
        const float required_height = std::max(1.0F, union_br.y - union_tl.y);
        const float fit_zoom_x = static_cast<float>(graphics.dims.x) / required_width;
        const float fit_zoom_y = static_cast<float>(graphics.dims.y) / required_height;
        const float fit_zoom = std::min(fit_zoom_x, fit_zoom_y);
        focus.zoom = std::min(fit_zoom, focus.zoom);
        focus.target = GetCameraTargetForCenteredViewBounds(graphics, union_view, focus.zoom);
    }

    return focus;
}

} // namespace

void RenderPlaying(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    Vec2 target = GetStageCameraCenter(state.stage);
    float zoom = GetDefaultFollowCameraZoom(graphics);

    const Ent* camera_target_ent = nullptr;
    if (state.controlled_ent_vid.has_value()) {
        camera_target_ent = state.ents.GetEnt(*state.controlled_ent_vid);
    }
    if ((camera_target_ent == nullptr || !camera_target_ent->active ||
         camera_target_ent->condition == EntCondition::Dead) &&
        state.mode == Mode::GameOver) {
        const std::optional<VID> spectator_vid = FindFirstConnectedLivingPlayerVid(state);
        camera_target_ent = spectator_vid.has_value()
            ? state.ents.GetEnt(*spectator_vid)
            : GetPrimaryLocalPlayer(state);
    }

    if (graphics.camera_mode == CameraMode::StageFit) {
        zoom = GetStageFitCameraZoom(state.stage, graphics);
    } else if (camera_target_ent != nullptr && camera_target_ent->active) {
        if (!graphics.debug_lock_play_camera) {
            const CameraFocus local_focus = ComputeLocalPlayerCameraFocus(state, graphics);
            const Vec2 raw_follow_target =
                local_focus.has_target
                    ? local_focus.target
                    : ents::common::GetVisualCenterForEnt(
                          *camera_target_ent,
                          graphics,
                          camera_target_ent->GetCenter()
                      );
            const Vec2 camera_follow_target =
                RotateWorldPointForActiveWorldRotation(graphics, raw_follow_target);
            if (local_focus.has_target) {
                zoom = local_focus.zoom;
            }
            if (graphics.world_rotation_active) {
                graphics.play_cam.pos = camera_follow_target;
            } else {
                const Vec2 delta = GetNearestWorldDelta(
                    state.stage,
                    graphics.play_cam.pos,
                    camera_follow_target
                );
                graphics.play_cam.pos += delta * 0.075F;
                graphics.play_cam.pos = ClampCameraTargetToStage(state.stage, graphics.play_cam.pos);
            }
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
    if (graphics.world_rotation_active) {
        graphics.camera.target = target;
        graphics.camera.zoom = zoom;
    } else {
        LerpCamera(graphics, target, zoom);
    }

    if (graphics.world_rotation_active) {
        SDL_SetRenderDrawColor(renderer, 6, 6, 6, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 38, 43, 68, 255);
    }
    SDL_RenderClear(renderer);
    RenderStageTileWrapper(renderer, state, graphics);
    RenderStageTiles(renderer, state, graphics);
    RenderBackgroundStamps(renderer, state, graphics);
    RenderStagePreEntForegroundTiles(renderer, state, graphics);
    RenderEnts(renderer, state, graphics);
    RenderStageFluids(renderer, state, graphics);
    RenderStageForegroundTiles(renderer, state, graphics);
    RenderStageForegroundTileWrapper(renderer, state, graphics);
    RenderEmbeddedTreasureOverlays(renderer, state, graphics);
    RenderStageTileCaps(renderer, state, graphics);
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

    const float center_x = static_cast<float>(graphics.dims.x) / 2.0F;
    const float center_y = static_cast<float>(graphics.dims.y) / 2.0F;
    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuTitle,
        "Classic Quest Clear",
        center_x,
        center_y - 24.0F,
        SDL_Color{255, 255, 255, 255}
    );
    DrawText(
        renderer,
        graphics,
        30,
        graphics.ui_font,
        "Press [jump] to restart Mines 1",
        center_x - (static_cast<float>(graphics.dims.x) * 0.16F),
        center_y + 48.0F,
        SDL_Color{255, 255, 255, 255}
    );
}

} // namespace splonks
