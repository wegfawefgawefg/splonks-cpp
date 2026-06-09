#include "stage_spawning.hpp"

#include "aframe_id.hpp"
#include "buying.hpp"
#include "ent/spec.hpp"
#include "ent/spec_restore.hpp"
#include "ents/common/common.hpp"
#include "ents/shop.hpp"
#include "ents/store_light.hpp"
#include "player_queries.hpp"
#include "fxp.hpp"
#include "tools/tool_spec.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace splonks {

namespace {

constexpr std::uint16_t kPlayerInitialBombs = 400;
constexpr std::uint16_t kPlayerInitialRopes = 400;
constexpr std::uint32_t kPlayerInitialTestingMoney = 100000;

EntType GetConfiguredPlayerSpawnType(const State& state, PlayerId player_id) {
    if (const PlayerSlot* const slot = state.players.Find(player_id)) {
        if (IsPlayerLikeEntType(slot->preferred_spawn_type)) {
            return slot->preferred_spawn_type;
        }
    }
    if (!state.settings.debug_ui.default_spawn_enabled) {
        return EntType::Player;
    }

    const std::uint32_t type_index = state.settings.debug_ui.default_spawn_type;
    if (type_index == 0 || type_index >= kEntTypeCount) {
        return EntType::Player;
    }
    return static_cast<EntType>(type_index);
}

void GrantPlayerStarterTools(State& state, const VID& player_vid) {
    if (const std::optional<ToolKind> bomb_tool_kind = FindPreferredToolKindForSlotIndex(0)) {
        FillToolSlot(state.ent_tools.EnsureToolSlot(player_vid, 0), *bomb_tool_kind,
                     kPlayerInitialBombs, true);
    }

    if (const std::optional<ToolKind> rope_tool_kind = FindPreferredToolKindForSlotIndex(1)) {
        FillToolSlot(state.ent_tools.EnsureToolSlot(player_vid, 1), *rope_tool_kind,
                     kPlayerInitialRopes, true);
    }
}

void RestoreEntSlot(EntPool& ents, const Ent& ent) {
    if (ent.vid.id >= ents.ents.size()) {
        return;
    }

    ents.ents[ent.vid.id] = ent;
    ents.ents[ent.vid.id].active = true;

    const auto it = std::find(ents.available_ids.begin(), ents.available_ids.end(), ent.vid.id);
    if (it != ents.available_ids.end()) {
        ents.available_ids.erase(it);
    }
}

void PrepareEntForStageEntry(Ent& ent) {
    ent.marked_for_destruction = false;
    ent.buyable = Buyable{};
    ent.stage_spawn_index.reset();
    ent.holding = false;
    ent.holding_vid.reset();
    ent.back_vid.reset();
    ent.held_by_vid.reset();
    ent.attach_mode = AttachMode::None;
    ent.vel = FxVec2::zero();
    ent.acc = FxVec2::zero();
    ent.grounded = false;
    ent.coyote_time = 0;
    ent.fall_timer = 0;
    ent.stun_timer = 0;
    ent.holding_timer = kDefaultHoldingTimer;
    ent.dist_traveled_this_frame = FxScalar::zero();
    ent.jumped_this_frame = false;
    ent.hang_side.reset();
    ent.hang_count = 0;
    ent.climb_detach_cooldown = 0;
    ent.jump_hold_gravity_frames_remaining = 0;
    ent.movement_flags = 0;
    ent.use_state = UseState{};
    ent.collided = false;
    ent.collided_last_frame = false;
    ent.contact_sound_cooldown = 0;
    ent.thrown_by.reset();
    ent.thrown_immunity_timer = 0;
    const EntSpec& spec = GetEntSpec(ent.type_);
    ent.proj_contact_damage_type = spec.proj_contact_damage_type;
    ent.proj_contact_damage_amount = spec.proj_contact_damage_amount;
    ent.proj_contact_timer = 0;
}

void PreparePlayerForStageEntry(Ent& player) {
    PrepareEntForStageEntry(player);
    RestoreEntConditionFromSpec(player);
    RestoreEntSizeFromSpec(player);
    RestoreEntHasPhysicsFromSpec(player);
    RestoreEntCanCollideFromSpec(player);
    RestoreEntDrawLayerFromSpec(player);
    RestoreEntRenderEnabledFromSpec(player);
    RestoreEntAFrameAnimatorFromSpec(player);
}

} // namespace

void InitCommonStageState(State& state) {
    state.stage_frame = 0;
    state.ents.ClearAllEnts();
    state.ent_tools.tool_states.clear();
    state.contact = ContactBookkeeping{};
    state.particles.Clear();
    state.players.ClearEntRefs();
    state.controlled_ent_vid.reset();
    state.gameplay_camera_anchor_world_pos.reset();
    state.mouse_trailer_vid.reset();
}

StageCarryover CaptureStageCarryover(const State& state) {
    StageCarryover carryover;
    constexpr bool preserve_attached_items = true;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }

        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || !player->active || player->condition == EntCondition::Dead) {
            continue;
        }

        PlayerStageCarryover player_carryover;
        player_carryover.player_id = slot.player_id;
        player_carryover.player = *player;
        player_carryover.player->holding = false;
        player_carryover.player->holding_vid.reset();
        player_carryover.player->back_vid.reset();

        if (preserve_attached_items && player->holding_vid.has_value()) {
            if (const Ent* const held_item = state.ents.GetEnt(*player->holding_vid)) {
                if (held_item->active &&
                    !state.players.FindPlayerIdForEnt(held_item->vid).has_value()) {
                    player_carryover.held_item = *held_item;
                    player_carryover.player->holding_vid = held_item->vid;
                    player_carryover.player->holding = true;
                }
            }
        }

        if (preserve_attached_items && player->back_vid.has_value()) {
            if (const Ent* const back_item = state.ents.GetEnt(*player->back_vid)) {
                if (back_item->active &&
                    !state.players.FindPlayerIdForEnt(back_item->vid).has_value()) {
                    player_carryover.back_item = *back_item;
                    player_carryover.player->back_vid = back_item->vid;
                }
            }
        }

        if (const EntToolState* const tools = state.ent_tools.FindEntToolState(player->vid)) {
            player_carryover.player_tools = *tools;
        }

        carryover.players.push_back(player_carryover);
    }

    return carryover;
}

void RestoreStageCarryover(State& state, const StageCarryover& carryover) {
    for (const PlayerStageCarryover& player_carryover : carryover.players) {
        if (!player_carryover.player.has_value()) {
            continue;
        }

        Ent player = *player_carryover.player;
        PreparePlayerForStageEntry(player);
        if (player_carryover.held_item.has_value()) {
            player.holding = true;
            player.holding_vid = player_carryover.held_item->vid;
        }
        if (player_carryover.back_item.has_value()) {
            player.back_vid = player_carryover.back_item->vid;
        }
        RestoreEntSlot(state.ents, player);
        state.players.AssignEnt(player_carryover.player_id, player.vid);
        if (const PlayerSlot* const slot = state.players.Find(player_carryover.player_id);
            slot != nullptr && slot->primary_local) {
            state.controlled_ent_vid = player.vid;
        }
        if (!state.controlled_ent_vid.has_value()) {
            state.controlled_ent_vid = player.vid;
        }

        if (player_carryover.player_tools.has_value()) {
            state.ent_tools.tool_states.push_back(*player_carryover.player_tools);
        }

        if (player_carryover.held_item.has_value()) {
            Ent held_item = *player_carryover.held_item;
            PrepareEntForStageEntry(held_item);
            held_item.held_by_vid = player.vid;
            held_item.attach_mode = AttachMode::Held;
            held_item.has_physics = false;
            held_item.can_collide = false;
            RestoreEntSlot(state.ents, held_item);
        }

        if (player_carryover.back_item.has_value()) {
            Ent back_item = *player_carryover.back_item;
            PrepareEntForStageEntry(back_item);
            back_item.held_by_vid = player.vid;
            back_item.attach_mode = AttachMode::Back;
            back_item.has_physics = false;
            back_item.can_collide = false;
            RestoreEntSlot(state.ents, back_item);
        }
    }
}

void PlacePlayerAtPosition(State& state, FxVec2 pos) {
    Ent* const player = GetPrimaryLocalPlayerMut(state);
    if (player == nullptr) {
        return;
    }
    player->pos = pos;
    player->vel = FxVec2::zero();
    player->acc = FxVec2::zero();
}

void PlacePlayerAtAuthoredPosition(State& state, const FVec2& pos) {
    PlacePlayerAtPosition(state, ToFxVec2(pos));
}

void SnapAttachedItemsToPlayer(State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }

        Ent* const player = state.ents.GetEntMut(*slot.ent_vid);
        if (player == nullptr) {
            continue;
        }

        const FxVec2 player_center = player->GetCenter();

        if (player->holding_vid.has_value()) {
            if (Ent* const held_item = state.ents.GetEntMut(*player->holding_vid)) {
                const FxVec2 hold_offset = PixelVec2(4, 1);
                held_item->facing = player->facing;
                held_item->draw_layer = DrawLayer::Foreground;
                held_item->SetCenter(player->facing == Side::Left
                                            ? player_center +
                                                  FxVec2{-hold_offset.x, hold_offset.y}
                                            : player_center + hold_offset);
            }
        }

        if (player->back_vid.has_value()) {
            if (Ent* const back_item = state.ents.GetEntMut(*player->back_vid)) {
                const FxVec2 back_offset = PixelVec2(-3, 0);
                back_item->facing = player->facing;
                back_item->draw_layer = DrawLayer::Background;
                TrySetAnim(*back_item, EntDisplayState::Neutral);
                back_item->SetCenter(player->facing == Side::Left
                                            ? player_center +
                                                  FxVec2{-back_offset.x, back_offset.y}
                                            : player_center + back_offset);
            }
        }
    }
}

void SpawnPlayer(State& state, FxVec2 pos) {
    (void)state.players.EnsurePrimaryLocalPlayer();
    const std::optional<VID> player_vid = SpawnPlayerForPlayerId(state, kPrimaryLocalPlayerId, pos);
    if (!player_vid.has_value()) {
        return;
    }
    state.controlled_ent_vid = player_vid;
}

void SpawnPlayerAtAuthoredPosition(State& state, const FVec2& pos) {
    SpawnPlayer(state, ToFxVec2(pos));
}

std::optional<VID> SpawnPlayerForPlayerId(State& state, PlayerId player_id, FxVec2 pos) {
    if (const std::optional<VID> player_vid = state.ents.NewEnt()) {
        if (Ent* const player = state.ents.GetEntMut(*player_vid)) {
            const EntType spawn_type = GetConfiguredPlayerSpawnType(state, player_id);
            SetEntAs(*player, spawn_type);
            player->pos = pos;
            player->vel = FxVec2::zero();
            player->acc = FxVec2::zero();
            player->money = kPlayerInitialTestingMoney;
            state.players.AssignEnt(player_id, *player_vid);

            if (spawn_type == EntType::Player) {
                GrantPlayerStarterTools(state, *player_vid);
            }
        }
        return player_vid;
    }
    return std::nullopt;
}

std::optional<VID> SpawnPlayerForPlayerIdAtAuthoredPosition(
    State& state,
    PlayerId player_id,
    const FVec2& pos
) {
    return SpawnPlayerForPlayerId(state, player_id, ToFxVec2(pos));
}

std::optional<VID> SpawnStageEntAtTopLeft(State& state, EntType type_, FxVec2 pos) {
    const std::optional<VID> vid = state.ents.NewEnt();
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Ent* const ent = state.ents.GetEntMut(*vid);
    if (ent == nullptr) {
        return std::nullopt;
    }

    SetEntAs(*ent, type_);
    ent->pos = pos;
    ent->vel = FxVec2::zero();
    if (type_ == EntType::StoreLight) {
        ents::store_light::AttachStoreLight(*ent, state);
    }
    return vid;
}

std::optional<VID> SpawnStageEntAtAuthoredTopLeft(State& state, EntType type_, const FVec2& pos) {
    return SpawnStageEntAtTopLeft(state, type_, ToFxVec2(pos));
}

std::optional<VID> SpawnStageEntAtCenter(State& state, EntType type_, FxVec2 center) {
    const std::optional<VID> vid = state.ents.NewEnt();
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Ent* const ent = state.ents.GetEntMut(*vid);
    if (ent == nullptr) {
        return std::nullopt;
    }

    SetEntAs(*ent, type_);
    ent->SetCenter(center);
    ent->vel = FxVec2::zero();
    return vid;
}

std::optional<VID> SpawnStageEntAtAuthoredCenter(State& state, EntType type_, const FVec2& center) {
    return SpawnStageEntAtCenter(state, type_, ToFxVec2(center));
}

void SpawnAuthoredStageEnts(State& state) {
    std::vector<std::optional<VID>> spawned_vids(state.stage.ent_spawns.size(), std::nullopt);

    for (std::size_t i = 0; i < state.stage.ent_spawns.size(); ++i) {
        const EntSpawn& spawn = state.stage.ent_spawns[i];
        if (spawn.type_ == EntType::None) {
            continue;
        }
        const std::optional<VID> vid = state.ents.NewEnt();
        if (!vid) {
            continue;
        }
        Ent* const ent = state.ents.GetEntMut(*vid);
        if (ent == nullptr) {
            continue;
        }

        SetEntAs(*ent, spawn.type_);
        ent->stage_spawn_index = static_cast<std::uint32_t>(i);
        ent->pos = ToFxVec2(spawn.pos);
        if (spawn.size_override.has_value()) {
            ent->size = ToFxVec2(*spawn.size_override);
        }
        ent->facing = spawn.facing;
        ent->vel = FxVec2::zero();
        if (spawn.ai_state_override.has_value()) {
            ent->ai_state = *spawn.ai_state_override;
        }
        if (spawn.type_ == EntType::BasicExit) {
            const std::string_view exit_id = spawn.exit_id.empty()
                                                 ? std::string_view("default")
                                                 : std::string_view(spawn.exit_id);
            ent->stage_exit_id = state.stage.FindExitId(exit_id);
            if (!state.stage.exits.empty() && ent->stage_exit_id == kInvalidStageExitId) {
                throw std::runtime_error("BasicExit spawn references unknown stage exit: " +
                                         std::string(exit_id));
            }
        }
        spawned_vids[i] = *vid;
        if (spawn.type_ == EntType::StoreLight) {
            ents::store_light::AttachStoreLight(*ent, state);
        }
        if (spawn.anim_id != kInvalidAFrameId) {
            SetAnim(*ent, spawn.anim_id);
        }
    }

    const auto resolve_spawn_link = [&](std::size_t ent_spawn_index,
                                        std::optional<std::uint32_t> linked_spawn_index, int slot) {
        if (!linked_spawn_index.has_value()) {
            return;
        }
        if (ent_spawn_index >= spawned_vids.size() || !spawned_vids[ent_spawn_index].has_value()) {
            return;
        }
        const std::size_t linked_index = static_cast<std::size_t>(*linked_spawn_index);
        if (linked_index >= spawned_vids.size() || !spawned_vids[linked_index].has_value()) {
            return;
        }

        Ent* const ent = state.ents.GetEntMut(*spawned_vids[ent_spawn_index]);
        const Ent* const linked_ent = state.ents.GetEnt(*spawned_vids[linked_index]);
        if (ent == nullptr || linked_ent == nullptr) {
            return;
        }

        switch (slot) {
        case 0:
            ent->ent_a = *spawned_vids[*linked_spawn_index];
            ent->point_a = ToPixelIVec2Round(linked_ent->pos);
            ent->point_label_a = PointLabel::Target;
            break;
        case 1:
            ent->ent_b = *spawned_vids[*linked_spawn_index];
            ent->point_b = ToPixelIVec2Round(linked_ent->pos);
            ent->point_label_b = PointLabel::Target;
            break;
        case 2:
            ent->ent_c = *spawned_vids[*linked_spawn_index];
            ent->point_c = ToPixelIVec2Round(linked_ent->pos);
            ent->point_label_c = PointLabel::Target;
            break;
        case 3:
            ent->ent_d = *spawned_vids[*linked_spawn_index];
            ent->point_d = ToPixelIVec2Round(linked_ent->pos);
            ent->point_label_d = PointLabel::Target;
            break;
        default:
            break;
        }
    };

    for (std::size_t i = 0; i < state.stage.ent_spawns.size(); ++i) {
        const EntSpawn& spawn = state.stage.ent_spawns[i];
        resolve_spawn_link(i, spawn.ent_a_spawn_index, 0);
        resolve_spawn_link(i, spawn.ent_b_spawn_index, 1);
        resolve_spawn_link(i, spawn.ent_c_spawn_index, 2);
        resolve_spawn_link(i, spawn.ent_d_spawn_index, 3);
    }

    for (StageTileTrigger& trigger : state.stage.tile_triggers) {
        if (!trigger.target_spawn_index.has_value()) {
            continue;
        }
        trigger.target_vid = std::nullopt;
        const std::size_t target_spawn_index =
            static_cast<std::size_t>(*trigger.target_spawn_index);
        if (target_spawn_index >= spawned_vids.size() ||
            !spawned_vids[target_spawn_index].has_value()) {
            continue;
        }
        trigger.target_vid = *spawned_vids[target_spawn_index];
    }

    for (std::size_t i = 0; i < state.stage.ent_spawns.size(); ++i) {
        const EntSpawn& spawn = state.stage.ent_spawns[i];
        if (!spawned_vids[i].has_value()) {
            continue;
        }
        Ent* const ent = state.ents.GetEntMut(*spawned_vids[i]);
        if (ent == nullptr) {
            continue;
        }

        if (spawn.buyable) {
            ConfigureEntAsBuyable(*ent, spawn.buy_price);
        }

        if (!spawn.shop_owner_spawn_index.has_value()) {
            continue;
        }
        const std::size_t shop_owner_spawn_index =
            static_cast<std::size_t>(*spawn.shop_owner_spawn_index);
        if (shop_owner_spawn_index >= spawned_vids.size() ||
            !spawned_vids[shop_owner_spawn_index].has_value()) {
            continue;
        }

        Ent* const shop = state.ents.GetEntMut(*spawned_vids[shop_owner_spawn_index]);
        if (shop == nullptr || !shop->active || shop->type_ != EntType::Shop) {
            continue;
        }

        ent->buyable.shop_owner_vid = shop->vid;
        ents::shop::AddShopChild(*shop, ent->vid);
    }

    for (std::size_t i = 0; i < state.stage.ent_spawns.size(); ++i) {
        if (!spawned_vids[i].has_value()) {
            continue;
        }
        Ent* const ent = state.ents.GetEntMut(*spawned_vids[i]);
        if (ent == nullptr || !ent->active || ent->type_ != EntType::Shopkeeper ||
            !ent->ent_a.has_value()) {
            continue;
        }
        Ent* const shop = state.ents.GetEntMut(*ent->ent_a);
        if (shop != nullptr && shop->active && shop->type_ == EntType::Shop) {
            shop->ent_a = ent->vid;
        }
    }
}

} // namespace splonks
