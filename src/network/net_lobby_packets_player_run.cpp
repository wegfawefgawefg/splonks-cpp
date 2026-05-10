#include "network/net_lobby_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <variant>

namespace splonks::network {

bool IsReplicatedPlayerStateMessage(const NetMessage& message) {
    return message.type == NetMessageType::PlayerStatePatched &&
           std::holds_alternative<PlayerStatePatchedMessage>(message.payload);
}

bool IsReplicatedRunStateMessage(const NetMessage& message) {
    return message.type == NetMessageType::RunStatePatched &&
           std::holds_alternative<RunStatePatchedMessage>(message.payload);
}


PlayerStateMessageEntry MakePlayerStateMessageEntry(const NetMessage& message) {
    const PlayerStatePatchedMessage* const payload = std::get_if<PlayerStatePatchedMessage>(&message.payload);
    PlayerStateMessageEntry entry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
        .player_entity_id = payload != nullptr ? payload->player_entity_id : kInvalidNetEntityId,
        .player_id = payload != nullptr ? payload->player_id : kInvalidPlayerId,
        .health = payload != nullptr ? payload->health : 0U,
        .money = payload != nullptr ? payload->money : 0U,
        .wanted = payload != nullptr ? payload->wanted : static_cast<std::uint8_t>(0),
        .connected = payload != nullptr ? payload->connected : static_cast<std::uint8_t>(1),
        .effect_count = payload != nullptr ? payload->effect_count : static_cast<std::uint8_t>(0),
    };
    if (payload == nullptr) {
        return entry;
    }
    for (std::size_t i = 0; i < entry.tool_slots.size() && i < payload->tool_slots.size(); ++i) {
        const PlayerStatePatchedToolSlot& slot = payload->tool_slots[i];
        entry.tool_slots[i] = PlayerStateToolSlotEntry{
            .kind = static_cast<std::uint16_t>(slot.kind),
            .count = slot.count,
            .cooldown = slot.cooldown,
            .active = slot.active,
        };
    }
    entry.effect_count = static_cast<std::uint8_t>(
        std::min<std::size_t>(entry.effect_count, entry.effects.size())
    );
    for (std::size_t i = 0; i < entry.effect_count; ++i) {
        const PlayerStatePatchedEffect& effect = payload->effects[i];
        entry.effects[i] = PlayerStateEffectEntry{
            .id = static_cast<std::uint16_t>(effect.id),
            .count = effect.count,
            .value = effect.value,
            .frames_remaining = effect.frames_remaining,
        };
    }
    return entry;
}

RunStateMessageEntry MakeRunStateMessageEntry(const NetMessage& message) {
    const RunStatePatchedMessage* const payload = std::get_if<RunStatePatchedMessage>(&message.payload);
    return RunStateMessageEntry{
        .message_id = message.header.message_id,
        .source_player_id = message.header.source_player_id,
        .stage_instance_id = message.header.stage_instance_id,
        .source_local_frame = message.header.source_local_frame,
        .coordinator_order = message.header.coordinator_order,
        .quest_id = payload != nullptr
            ? static_cast<std::uint16_t>(payload->quest_id)
            : static_cast<std::uint16_t>(QuestId::None),
        .stage_type = payload != nullptr
            ? static_cast<std::uint16_t>(payload->stage_type)
            : static_cast<std::uint16_t>(0),
        .frame = payload != nullptr ? payload->frame : 0U,
        .stage_frame = payload != nullptr ? payload->stage_frame : 0U,
        .depth = payload != nullptr ? payload->depth : 0U,
        .points = payload != nullptr ? payload->points : 0U,
        .deaths = payload != nullptr ? payload->deaths : 0U,
        .quest_level_number = payload != nullptr ? payload->quest_level_number : 0,
        .generation_seed = payload != nullptr ? payload->generation_seed : 0U,
        .tile_change_generation = payload != nullptr ? payload->tile_change_generation : 0U,
        .stage_gravity = payload != nullptr ? payload->stage_gravity : 0.0F,
        .border_left_tile = payload != nullptr
            ? static_cast<std::uint16_t>(payload->border_left_tile)
            : static_cast<std::uint16_t>(0),
        .border_right_tile = payload != nullptr
            ? static_cast<std::uint16_t>(payload->border_right_tile)
            : static_cast<std::uint16_t>(0),
        .border_top_tile = payload != nullptr
            ? static_cast<std::uint16_t>(payload->border_top_tile)
            : static_cast<std::uint16_t>(0),
        .border_bottom_tile = payload != nullptr
            ? static_cast<std::uint16_t>(payload->border_bottom_tile)
            : static_cast<std::uint16_t>(0),
        .void_death_y = payload != nullptr ? payload->void_death_y : 0,
        .wrap_core_origin_x = payload != nullptr ? payload->wrap_core_origin_x : 0U,
        .wrap_core_origin_y = payload != nullptr ? payload->wrap_core_origin_y : 0U,
        .wrap_core_size_x = payload != nullptr ? payload->wrap_core_size_x : 0U,
        .wrap_core_size_y = payload != nullptr ? payload->wrap_core_size_y : 0U,
        .classic_made_black_market =
            payload != nullptr ? payload->classic_made_black_market : static_cast<std::uint8_t>(0),
        .classic_made_udjat_eye =
            payload != nullptr ? payload->classic_made_udjat_eye : static_cast<std::uint8_t>(0),
        .classic_has_udjat_eye =
            payload != nullptr ? payload->classic_has_udjat_eye : static_cast<std::uint8_t>(0),
        .classic_made_moai =
            payload != nullptr ? payload->classic_made_moai : static_cast<std::uint8_t>(0),
        .classic_has_hedjet =
            payload != nullptr ? payload->classic_has_hedjet : static_cast<std::uint8_t>(0),
        .classic_has_sceptre =
            payload != nullptr ? payload->classic_has_sceptre : static_cast<std::uint8_t>(0),
        .classic_has_book_of_dead =
            payload != nullptr ? payload->classic_has_book_of_dead : static_cast<std::uint8_t>(0),
        .has_generation_seed =
            payload != nullptr ? payload->has_generation_seed : static_cast<std::uint8_t>(0),
        .border_wrap_x =
            payload != nullptr ? payload->border_wrap_x : static_cast<std::uint8_t>(0),
        .border_wrap_y =
            payload != nullptr ? payload->border_wrap_y : static_cast<std::uint8_t>(0),
        .has_void_death_y =
            payload != nullptr ? payload->has_void_death_y : static_cast<std::uint8_t>(0),
        .camera_clamp_enabled =
            payload != nullptr ? payload->camera_clamp_enabled : static_cast<std::uint8_t>(1),
        .wrap_transform_active =
            payload != nullptr ? payload->wrap_transform_active : static_cast<std::uint8_t>(0),
        .game_over = payload != nullptr ? payload->game_over : static_cast<std::uint8_t>(0),
        .win = payload != nullptr ? payload->win : static_cast<std::uint8_t>(0),
        .has_snapshot_fingerprint =
            payload != nullptr ? payload->has_snapshot_fingerprint : static_cast<std::uint8_t>(0),
        .wrap_padding_tiles =
            payload != nullptr ? payload->wrap_padding_tiles : static_cast<std::uint16_t>(0),
        .sac_altar_favor = payload != nullptr ? payload->sac_altar_favor : 0,
        .sac_altar_reward_tier = payload != nullptr ? payload->sac_altar_reward_tier : 0U,
        .snapshot_fingerprint = payload != nullptr ? payload->snapshot_fingerprint : 0U,
    };
}


NetMessage MakePlayerStateMessage(const PlayerStateMessageEntry& entry) {
    NetMessage message;
    message.header = NetMessageHeader{
        .message_id = entry.message_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    PlayerStatePatchedMessage payload{
        .player_entity_id = entry.player_entity_id,
        .player_id = entry.player_id,
        .health = entry.health,
        .money = entry.money,
        .wanted = entry.wanted,
        .connected = entry.connected,
        .effect_count = entry.effect_count,
    };
    for (std::size_t i = 0; i < payload.tool_slots.size() && i < entry.tool_slots.size(); ++i) {
        const PlayerStateToolSlotEntry& slot = entry.tool_slots[i];
        payload.tool_slots[i] = PlayerStatePatchedToolSlot{
            .kind = static_cast<ToolKind>(slot.kind),
            .count = slot.count,
            .cooldown = slot.cooldown,
            .active = slot.active,
        };
    }
    payload.effect_count = static_cast<std::uint8_t>(
        std::min<std::size_t>(payload.effect_count, payload.effects.size())
    );
    for (std::size_t i = 0; i < payload.effect_count; ++i) {
        const PlayerStateEffectEntry& effect = entry.effects[i];
        payload.effects[i] = PlayerStatePatchedEffect{
            .id = static_cast<EffectId>(effect.id),
            .count = effect.count,
            .value = effect.value,
            .frames_remaining = effect.frames_remaining,
        };
    }
    message.type = NetMessageType::PlayerStatePatched;
    message.payload = payload;
    return message;
}

NetMessage MakeRunStateMessage(const RunStateMessageEntry& entry) {
    NetMessage message;
    message.header = NetMessageHeader{
        .message_id = entry.message_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    message.type = NetMessageType::RunStatePatched;
    message.payload = RunStatePatchedMessage{
        .quest_id = static_cast<QuestId>(entry.quest_id),
        .frame = entry.frame,
        .stage_frame = entry.stage_frame,
        .depth = entry.depth,
        .points = entry.points,
        .deaths = entry.deaths,
        .stage_type = entry.stage_type,
        .quest_level_number = entry.quest_level_number,
        .generation_seed = entry.generation_seed,
        .tile_change_generation = entry.tile_change_generation,
        .stage_gravity = entry.stage_gravity,
        .border_left_tile = static_cast<Tile>(entry.border_left_tile),
        .border_right_tile = static_cast<Tile>(entry.border_right_tile),
        .border_top_tile = static_cast<Tile>(entry.border_top_tile),
        .border_bottom_tile = static_cast<Tile>(entry.border_bottom_tile),
        .void_death_y = entry.void_death_y,
        .has_generation_seed = entry.has_generation_seed,
        .border_wrap_x = entry.border_wrap_x,
        .border_wrap_y = entry.border_wrap_y,
        .has_void_death_y = entry.has_void_death_y,
        .camera_clamp_enabled = entry.camera_clamp_enabled,
        .wrap_transform_active = entry.wrap_transform_active,
        .game_over = entry.game_over,
        .win = entry.win,
        .wrap_padding_tiles = entry.wrap_padding_tiles,
        .wrap_core_origin_x = entry.wrap_core_origin_x,
        .wrap_core_origin_y = entry.wrap_core_origin_y,
        .wrap_core_size_x = entry.wrap_core_size_x,
        .wrap_core_size_y = entry.wrap_core_size_y,
        .classic_made_black_market = entry.classic_made_black_market,
        .classic_made_udjat_eye = entry.classic_made_udjat_eye,
        .classic_has_udjat_eye = entry.classic_has_udjat_eye,
        .classic_made_moai = entry.classic_made_moai,
        .classic_has_hedjet = entry.classic_has_hedjet,
        .classic_has_sceptre = entry.classic_has_sceptre,
        .classic_has_book_of_dead = entry.classic_has_book_of_dead,
        .sac_altar_favor = entry.sac_altar_favor,
        .sac_altar_reward_tier = entry.sac_altar_reward_tier,
        .has_snapshot_fingerprint = entry.has_snapshot_fingerprint,
        .snapshot_fingerprint = entry.snapshot_fingerprint,
    };
    return message;
}


} // namespace splonks::network
