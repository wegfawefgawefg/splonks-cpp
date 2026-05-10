#include "network/net_message_apply_internal.hpp"

#include "state.hpp"

#include <cstdint>
#include <optional>

namespace splonks::network {

void ApplyRunStatePatchedMessage(
    State& state,
    const RunStatePatchedMessage& payload,
    std::optional<std::uint64_t>& pending_snapshot_fingerprint
) {
    state.quest_state.quest_id = payload.quest_id;
    state.frame = payload.frame;
    state.stage_frame = payload.stage_frame;
    state.depth = payload.depth;
    state.points = payload.points;
    state.deaths = payload.deaths;
    state.stage.stage_type = static_cast<StageType>(payload.stage_type);
    state.stage.quest_level_number = payload.quest_level_number;
    state.stage.generation_seed = payload.has_generation_seed != 0
        ? std::optional<std::uint32_t>(payload.generation_seed)
        : std::nullopt;
    state.stage.tile_change_generation = payload.tile_change_generation;
    state.stage.gravity = payload.stage_gravity;
    state.stage.border.left.tile = payload.border_left_tile;
    state.stage.border.right.tile = payload.border_right_tile;
    state.stage.border.top.tile = payload.border_top_tile;
    state.stage.border.bottom.tile = payload.border_bottom_tile;
    state.stage.border.wrap_x = payload.border_wrap_x != 0;
    state.stage.border.wrap_y = payload.border_wrap_y != 0;
    state.stage.border.void_death_y = payload.has_void_death_y != 0
        ? std::optional<int>(payload.void_death_y)
        : std::nullopt;
    state.stage.camera_clamp_enabled = payload.camera_clamp_enabled != 0;
    state.stage.wrap_transform_active = payload.wrap_transform_active != 0;
    state.game_over = payload.game_over != 0;
    state.win = payload.win != 0;
    state.stage.wrap_padding_tiles = payload.wrap_padding_tiles;
    state.stage.wrap_core_origin_tiles = UVec2::New(
        payload.wrap_core_origin_x,
        payload.wrap_core_origin_y
    );
    state.stage.wrap_core_size_tiles = UVec2::New(
        payload.wrap_core_size_x,
        payload.wrap_core_size_y
    );
    state.quest_state.classic.made_black_market = payload.classic_made_black_market != 0;
    state.quest_state.classic.made_udjat_eye = payload.classic_made_udjat_eye != 0;
    state.quest_state.classic.has_udjat_eye = payload.classic_has_udjat_eye != 0;
    state.quest_state.classic.made_moai = payload.classic_made_moai != 0;
    state.quest_state.classic.has_hedjet = payload.classic_has_hedjet != 0;
    state.quest_state.classic.has_sceptre = payload.classic_has_sceptre != 0;
    state.quest_state.classic.has_book_of_dead = payload.classic_has_book_of_dead != 0;
    state.sac_altar_favor = payload.sac_altar_favor;
    state.sac_altar_reward_tier = payload.sac_altar_reward_tier;
    if (payload.has_snapshot_fingerprint != 0) {
        pending_snapshot_fingerprint = payload.snapshot_fingerprint;
    }
}

} // namespace splonks::network
