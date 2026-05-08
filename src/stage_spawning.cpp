#include "stage_spawning.hpp"

#include "buying.hpp"
#include "entity/archetype.hpp"
#include "entity/archetype_restore.hpp"
#include "entities/common/common.hpp"
#include "entities/shop.hpp"
#include "entities/store_light.hpp"
#include "frame_data_id.hpp"
#include "player_queries.hpp"
#include "tools/tool_archetype.hpp"

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

EntityType GetConfiguredPlayerSpawnType(const State& state) {
    if (!state.settings.debug_ui.default_spawn_enabled) {
        return EntityType::Player;
    }

    const std::uint32_t type_index = state.settings.debug_ui.default_spawn_type;
    if (type_index == 0 || type_index >= kEntityTypeCount) {
        return EntityType::Player;
    }
    return static_cast<EntityType>(type_index);
}

void GrantPlayerStarterTools(State& state, const VID& player_vid) {
    if (const std::optional<ToolKind> bomb_tool_kind = FindPreferredToolKindForSlotIndex(0)) {
        FillToolSlot(
            state.entity_tools.EnsureToolSlot(player_vid, 0),
            *bomb_tool_kind,
            kPlayerInitialBombs,
            true
        );
    }

    if (const std::optional<ToolKind> rope_tool_kind = FindPreferredToolKindForSlotIndex(1)) {
        FillToolSlot(
            state.entity_tools.EnsureToolSlot(player_vid, 1),
            *rope_tool_kind,
            kPlayerInitialRopes,
            true
        );
    }
}

void RestoreEntitySlot(EntityManager& entity_manager, const Entity& entity) {
    if (entity.vid.id >= entity_manager.entities.size()) {
        return;
    }

    entity_manager.entities[entity.vid.id] = entity;
    entity_manager.entities[entity.vid.id].active = true;

    const auto it = std::find(
        entity_manager.available_ids.begin(),
        entity_manager.available_ids.end(),
        entity.vid.id
    );
    if (it != entity_manager.available_ids.end()) {
        entity_manager.available_ids.erase(it);
    }
}

void PrepareEntityForStageEntry(Entity& entity) {
    entity.marked_for_destruction = false;
    entity.holding = false;
    entity.holding_vid.reset();
    entity.back_vid.reset();
    entity.held_by_vid.reset();
    entity.attachment_mode = AttachmentMode::None;
    entity.vel = Vec2::New(0.0F, 0.0F);
    entity.acc = Vec2::New(0.0F, 0.0F);
    entity.grounded = false;
    entity.coyote_time = 0;
    entity.fall_timer = 0;
    entity.stun_timer = 0;
    entity.holding_timer = kDefaultHoldingTimer;
    entity.dist_traveled_this_frame = 0.0F;
    entity.jumped_this_frame = false;
    entity.hang_side.reset();
    entity.hang_count = 0;
    entity.climb_detach_cooldown = 0;
    entity.jump_hold_gravity_frames_remaining = 0;
    entity.movement_flags = 0;
    entity.use_state = UseState{};
    entity.collided = false;
    entity.collided_last_frame = false;
    entity.contact_sound_cooldown = 0;
    entity.thrown_by.reset();
    entity.thrown_immunity_timer = 0;
    const EntityArchetype& archetype = GetEntityArchetype(entity.type_);
    entity.projectile_contact_damage_type = archetype.projectile_contact_damage_type;
    entity.projectile_contact_damage_amount = archetype.projectile_contact_damage_amount;
    entity.projectile_contact_timer = 0;
}

void PreparePlayerForStageEntry(Entity& player) {
    PrepareEntityForStageEntry(player);
    RestoreEntityConditionFromArchetype(player);
    RestoreEntitySizeFromArchetype(player);
    RestoreEntityHasPhysicsFromArchetype(player);
    RestoreEntityCanCollideFromArchetype(player);
    RestoreEntityDrawLayerFromArchetype(player);
    RestoreEntityRenderEnabledFromArchetype(player);
    RestoreEntityFrameDataAnimatorFromArchetype(player);
}

} // namespace

void InitCommonStageState(State& state) {
    state.stage_frame = 0;
    state.entity_manager.ClearAllEntities();
    state.entity_tools.tool_states.clear();
    state.contact = ContactBookkeeping{};
    state.particles.Clear();
    state.players.ClearEntityRefs();
    state.controlled_entity_vid.reset();
    state.gameplay_camera_anchor_world_pos.reset();
    state.mouse_trailer_vid.reset();
}

StageCarryover CaptureStageCarryover(const State& state) {
    StageCarryover carryover;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected ||
            !slot.entity_vid.has_value()) {
            continue;
        }

        const Entity* const player = state.entity_manager.GetEntity(*slot.entity_vid);
        if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
            continue;
        }

        PlayerStageCarryover player_carryover;
        player_carryover.player_id = slot.player_id;
        player_carryover.player = *player;
        player_carryover.player->holding = false;
        player_carryover.player->holding_vid.reset();
        player_carryover.player->back_vid.reset();

        if (player->holding_vid.has_value()) {
            if (const Entity* const held_item = state.entity_manager.GetEntity(*player->holding_vid)) {
                if (held_item->active &&
                    !state.players.FindPlayerIdForEntity(held_item->vid).has_value()) {
                    player_carryover.held_item = *held_item;
                    player_carryover.player->holding_vid = held_item->vid;
                    player_carryover.player->holding = true;
                }
            }
        }

        if (player->back_vid.has_value()) {
            if (const Entity* const back_item = state.entity_manager.GetEntity(*player->back_vid)) {
                if (back_item->active &&
                    !state.players.FindPlayerIdForEntity(back_item->vid).has_value()) {
                    player_carryover.back_item = *back_item;
                    player_carryover.player->back_vid = back_item->vid;
                }
            }
        }

        if (const EntityToolState* const tools = state.entity_tools.FindEntityToolState(player->vid)) {
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

        Entity player = *player_carryover.player;
        PreparePlayerForStageEntry(player);
        if (player_carryover.held_item.has_value()) {
            player.holding = true;
            player.holding_vid = player_carryover.held_item->vid;
        }
        if (player_carryover.back_item.has_value()) {
            player.back_vid = player_carryover.back_item->vid;
        }
        RestoreEntitySlot(state.entity_manager, player);
        state.players.AssignEntity(player_carryover.player_id, player.vid);
        if (const PlayerSlot* const slot = state.players.Find(player_carryover.player_id);
            slot != nullptr && slot->primary_local) {
            state.controlled_entity_vid = player.vid;
        }
        if (!state.controlled_entity_vid.has_value()) {
            state.controlled_entity_vid = player.vid;
        }

        if (player_carryover.player_tools.has_value()) {
            state.entity_tools.tool_states.push_back(*player_carryover.player_tools);
        }

        if (player_carryover.held_item.has_value()) {
            Entity held_item = *player_carryover.held_item;
            PrepareEntityForStageEntry(held_item);
            held_item.held_by_vid = player.vid;
            held_item.attachment_mode = AttachmentMode::Held;
            held_item.has_physics = false;
            held_item.can_collide = false;
            RestoreEntitySlot(state.entity_manager, held_item);
        }

        if (player_carryover.back_item.has_value()) {
            Entity back_item = *player_carryover.back_item;
            PrepareEntityForStageEntry(back_item);
            back_item.held_by_vid = player.vid;
            back_item.attachment_mode = AttachmentMode::Back;
            back_item.has_physics = false;
            back_item.can_collide = false;
            RestoreEntitySlot(state.entity_manager, back_item);
        }
    }
}

void PlacePlayerAtPosition(State& state, const Vec2& pos) {
    Entity* const player = GetPrimaryLocalPlayerMut(state);
    if (player == nullptr) {
        return;
    }
    player->pos = pos;
    player->vel = Vec2::New(0.0F, 0.0F);
    player->acc = Vec2::New(0.0F, 0.0F);
}

void SnapAttachedItemsToPlayer(State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.entity_vid.has_value()) {
            continue;
        }

        Entity* const player = state.entity_manager.GetEntityMut(*slot.entity_vid);
        if (player == nullptr) {
            continue;
        }

        const Vec2 player_center = player->GetCenter();

        if (player->holding_vid.has_value()) {
            if (Entity* const held_item = state.entity_manager.GetEntityMut(*player->holding_vid)) {
                const Vec2 hold_offset = Vec2::New(4.0F, 1.0F);
                held_item->facing = player->facing;
                held_item->draw_layer = DrawLayer::Foreground;
                held_item->SetCenter(
                    player->facing == LeftOrRight::Left
                        ? player_center + Vec2::New(-hold_offset.x, hold_offset.y)
                        : player_center + hold_offset
                );
            }
        }

        if (player->back_vid.has_value()) {
            if (Entity* const back_item = state.entity_manager.GetEntityMut(*player->back_vid)) {
                const Vec2 back_offset = Vec2::New(-3.0F, 0.0F);
                back_item->facing = player->facing;
                back_item->draw_layer = DrawLayer::Background;
                TrySetAnimation(*back_item, EntityDisplayState::Neutral);
                back_item->SetCenter(
                    player->facing == LeftOrRight::Left
                        ? player_center + Vec2::New(-back_offset.x, back_offset.y)
                        : player_center + back_offset
                );
            }
        }
    }
}

void SpawnPlayer(State& state, const Vec2& pos) {
    (void)state.players.EnsurePrimaryLocalPlayer();
    const std::optional<VID> player_vid = SpawnPlayerForPlayerId(state, kPrimaryLocalPlayerId, pos);
    if (!player_vid.has_value()) {
        return;
    }
    state.controlled_entity_vid = player_vid;
}

std::optional<VID> SpawnPlayerForPlayerId(State& state, PlayerId player_id, const Vec2& pos) {
    if (const std::optional<VID> player_vid = state.entity_manager.NewEntity()) {
        if (Entity* const player = state.entity_manager.GetEntityMut(*player_vid)) {
            const EntityType spawn_type = GetConfiguredPlayerSpawnType(state);
            SetEntityAs(*player, spawn_type);
            player->pos = pos;
            player->vel = Vec2::New(0.0F, 0.0F);
            player->acc = Vec2::New(0.0F, 0.0F);
            player->money = kPlayerInitialTestingMoney;
            state.players.AssignEntity(player_id, *player_vid);

            if (spawn_type == EntityType::Player) {
                GrantPlayerStarterTools(state, *player_vid);
            }
        }
        return player_vid;
    }
    return std::nullopt;
}

std::optional<VID> SpawnStageEntityAtTopLeft(State& state, EntityType type_, const Vec2& pos) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return std::nullopt;
    }

    SetEntityAs(*entity, type_);
    entity->pos = pos;
    entity->vel = Vec2::New(0.0F, 0.0F);
    if (type_ == EntityType::StoreLight) {
        entities::store_light::AttachStoreLight(*entity, state);
    }
    return vid;
}

std::optional<VID> SpawnStageEntityAtCenter(State& state, EntityType type_, const Vec2& center) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return std::nullopt;
    }

    SetEntityAs(*entity, type_);
    entity->SetCenter(center);
    entity->vel = Vec2::New(0.0F, 0.0F);
    return vid;
}

void SpawnAuthoredStageEntities(State& state) {
    std::vector<std::optional<VID>> spawned_vids(state.stage.entity_spawns.size(), std::nullopt);

    for (std::size_t i = 0; i < state.stage.entity_spawns.size(); ++i) {
        const StageEntitySpawn& spawn = state.stage.entity_spawns[i];
        if (spawn.type_ == EntityType::None) {
            continue;
        }
        const std::optional<VID> vid = state.entity_manager.NewEntity();
        if (!vid) {
            continue;
        }
        Entity* const entity = state.entity_manager.GetEntityMut(*vid);
        if (entity == nullptr) {
            continue;
        }

        SetEntityAs(*entity, spawn.type_);
        entity->stage_spawn_index = i;
        entity->pos = spawn.pos;
        if (spawn.size_override.has_value()) {
            entity->size = *spawn.size_override;
        }
        entity->facing = spawn.facing;
        entity->vel = Vec2::New(0.0F, 0.0F);
        if (spawn.ai_state_override.has_value()) {
            entity->ai_state = *spawn.ai_state_override;
        }
        if (spawn.type_ == EntityType::BasicExit) {
            const std::string_view exit_id =
                spawn.exit_id.empty() ? std::string_view("default") : std::string_view(spawn.exit_id);
            entity->stage_exit_id = state.stage.FindExitId(exit_id);
            if (!state.stage.exits.empty() && entity->stage_exit_id == kInvalidStageExitId) {
                throw std::runtime_error("BasicExit spawn references unknown stage exit: " +
                                         std::string(exit_id));
            }
        }
        spawned_vids[i] = *vid;
        if (spawn.type_ == EntityType::StoreLight) {
            entities::store_light::AttachStoreLight(*entity, state);
        }
        if (spawn.animation_id != kInvalidFrameDataId) {
            SetAnimation(*entity, spawn.animation_id);
        }
    }

    const auto resolve_spawn_link = [&](
        std::size_t entity_spawn_index,
        std::optional<std::size_t> linked_spawn_index,
        int slot
    ) {
        if (!linked_spawn_index.has_value()) {
            return;
        }
        if (entity_spawn_index >= spawned_vids.size() ||
            !spawned_vids[entity_spawn_index].has_value()) {
            return;
        }
        if (*linked_spawn_index >= spawned_vids.size() ||
            !spawned_vids[*linked_spawn_index].has_value()) {
            return;
        }

        Entity* const entity = state.entity_manager.GetEntityMut(*spawned_vids[entity_spawn_index]);
        const Entity* const linked_entity =
            state.entity_manager.GetEntity(*spawned_vids[*linked_spawn_index]);
        if (entity == nullptr || linked_entity == nullptr) {
            return;
        }

        switch (slot) {
        case 0:
            entity->entity_a = *spawned_vids[*linked_spawn_index];
            entity->point_a = ToIVec2(linked_entity->pos);
            entity->point_label_a = PointLabel::Target;
            break;
        case 1:
            entity->entity_b = *spawned_vids[*linked_spawn_index];
            entity->point_b = ToIVec2(linked_entity->pos);
            entity->point_label_b = PointLabel::Target;
            break;
        case 2:
            entity->entity_c = *spawned_vids[*linked_spawn_index];
            entity->point_c = ToIVec2(linked_entity->pos);
            entity->point_label_c = PointLabel::Target;
            break;
        case 3:
            entity->entity_d = *spawned_vids[*linked_spawn_index];
            entity->point_d = ToIVec2(linked_entity->pos);
            entity->point_label_d = PointLabel::Target;
            break;
        default:
            break;
        }
    };

    for (std::size_t i = 0; i < state.stage.entity_spawns.size(); ++i) {
        const StageEntitySpawn& spawn = state.stage.entity_spawns[i];
        resolve_spawn_link(i, spawn.entity_a_spawn_index, 0);
        resolve_spawn_link(i, spawn.entity_b_spawn_index, 1);
        resolve_spawn_link(i, spawn.entity_c_spawn_index, 2);
        resolve_spawn_link(i, spawn.entity_d_spawn_index, 3);
    }

    for (StageTileTrigger& trigger : state.stage.tile_triggers) {
        if (!trigger.target_spawn_index.has_value()) {
            continue;
        }
        trigger.target_vid = std::nullopt;
        if (*trigger.target_spawn_index >= spawned_vids.size() ||
            !spawned_vids[*trigger.target_spawn_index].has_value()) {
            continue;
        }
        trigger.target_vid = *spawned_vids[*trigger.target_spawn_index];
    }

    for (std::size_t i = 0; i < state.stage.entity_spawns.size(); ++i) {
        const StageEntitySpawn& spawn = state.stage.entity_spawns[i];
        if (!spawned_vids[i].has_value()) {
            continue;
        }
        Entity* const entity = state.entity_manager.GetEntityMut(*spawned_vids[i]);
        if (entity == nullptr) {
            continue;
        }

        if (spawn.buyable) {
            ConfigureEntityAsBuyable(*entity, spawn.buy_price);
        }

        if (!spawn.shop_owner_spawn_index.has_value() ||
            *spawn.shop_owner_spawn_index >= spawned_vids.size() ||
            !spawned_vids[*spawn.shop_owner_spawn_index].has_value()) {
            continue;
        }

        Entity* const shop =
            state.entity_manager.GetEntityMut(*spawned_vids[*spawn.shop_owner_spawn_index]);
        if (shop == nullptr || !shop->active || shop->type_ != EntityType::Shop) {
            continue;
        }

        entity->buyable.shop_owner_vid = shop->vid;
        entities::shop::AddShopChild(*shop, entity->vid);
    }

    for (std::size_t i = 0; i < state.stage.entity_spawns.size(); ++i) {
        if (!spawned_vids[i].has_value()) {
            continue;
        }
        Entity* const entity = state.entity_manager.GetEntityMut(*spawned_vids[i]);
        if (entity == nullptr || !entity->active || entity->type_ != EntityType::Shopkeeper ||
            !entity->entity_a.has_value()) {
            continue;
        }
        Entity* const shop = state.entity_manager.GetEntityMut(*entity->entity_a);
        if (shop != nullptr && shop->active && shop->type_ == EntityType::Shop) {
            shop->entity_a = entity->vid;
        }
    }
}

} // namespace splonks
