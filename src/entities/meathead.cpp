#include "entities/meathead.hpp"

#include "audio_emitters.hpp"
#include "entities/common/common.hpp"
#include "entity/archetype.hpp"
#include "frame_data_id.hpp"
#include "particles/particle_archetypes.hpp"
#include "tile_archetype.hpp"
#include "utils.hpp"
#include "world_query.hpp"

#include <vector>

namespace splonks::entities::meathead {

namespace {

constexpr float kMeatheadPickupRange = 16.0F;
constexpr std::uint32_t kMeatheadPointsPerHeal = 10;
constexpr std::uint32_t kMeatheadPreviewIntervalFrames = 300;
constexpr float kMeatheadPopupSize = 9.0F;
constexpr int kMeatheadPopupSearchTiles = 2;

common::ContactResolution OnEntityContactAsMeathead(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (graphics == nullptr || audio == nullptr ||
        !common::CanCollectPickupFromContact(entity_idx, other_entity_idx, state)) {
        return common::ContactResolution{};
    }

    Entity& collector = state.entity_manager.entities[other_entity_idx];
    const Entity& pickup = state.entity_manager.entities[entity_idx];
    if (!TryCollectInventoryPickup(state, collector, pickup)) {
        return common::ContactResolution{};
    }

    (void)PlayEntityCenterSoundEmitter(state, pickup, audio_asset_ids::Present);
    common::DeactivateCollectedPickup(entity_idx, state, *graphics);
    return common::ContactResolution{};
}

bool IsSolidTileAt(const Stage& stage, const IVec2& tile_pos) {
    const std::optional<WorldTileQueryResult> query = QueryTileAtTilePos(stage, tile_pos);
    return query.has_value() && query->tile != nullptr && GetTileArchetype(*query->tile).solid;
}

std::optional<Vec2> FindMeatheadPopupCenter(const Entity& player, const State& state) {
    if (!player.grounded) {
        return std::nullopt;
    }

    const int air_tile_y = state.stage.GetTileCoordAtWc(ToIVec2(player.GetAABB().br - Vec2::New(0.0F, 1.0F))).y;
    const int center_tile_x = state.stage.GetTileCoordAtWc(ToIVec2(player.GetCenter())).x;
    std::vector<IVec2> candidates;
    for (int dx = -kMeatheadPopupSearchTiles; dx <= kMeatheadPopupSearchTiles; ++dx) {
        const IVec2 air_tile = IVec2::New(center_tile_x + dx, air_tile_y);
        const IVec2 support_tile = IVec2::New(center_tile_x + dx, air_tile_y + 1);
        if (IsSolidTileAt(state.stage, air_tile) || !IsSolidTileAt(state.stage, support_tile)) {
            continue;
        }
        const std::optional<WorldTileQueryResult> air_query = QueryTileAtTilePos(state.stage, air_tile);
        if (!air_query.has_value()) {
            continue;
        }
        candidates.push_back(air_query->tile_pos);
    }

    if (candidates.empty()) {
        return std::nullopt;
    }

    const IVec2 choice = candidates[static_cast<std::size_t>(rng::RandomIntInclusive(0, static_cast<int>(candidates.size()) - 1))];
    const float center_x = static_cast<float>(choice.x * static_cast<int>(kTileSize) + static_cast<int>(kTileSize / 2));
    const float support_top_y = static_cast<float>((choice.y + 1) * static_cast<int>(kTileSize));
    return Vec2::New(center_x, support_top_y - (kMeatheadPopupSize * 0.5F));
}

bool IsExcludedDeathType(EntityType type_) {
    switch (type_) {
    case EntityType::None:
    case EntityType::Player:
    case EntityType::FlappyBee:
    case EntityType::FleshGuy:
    case EntityType::BasicExit:
    case EntityType::Shop:
    case EntityType::SignGeneral:
    case EntityType::SignBomb:
    case EntityType::SignWeapon:
    case EntityType::SignRare:
    case EntityType::SignClothing:
    case EntityType::SignCraps:
    case EntityType::SignKissing:
    case EntityType::StoreLight:
    case EntityType::MouseTrailer:
    case EntityType::GhostBall:
    case EntityType::SacAltar:
    case EntityType::SacAltarTopper:
    case EntityType::Altar:
    case EntityType::KaliHead:
    case EntityType::BallAndChainBall:
    case EntityType::CobraSpit:
    case EntityType::BaseballBat:
    case EntityType::Rope:
    case EntityType::Meathead:
        return true;
    default:
        return false;
    }
}

std::optional<Vec2> SpawnMeatheadPopup(State& state, const Entity& player) {
    const std::optional<Vec2> popup_center = FindMeatheadPopupCenter(player, state);
    if (!popup_center.has_value()) {
        return std::nullopt;
    }
    state.particles.AddScripted(scripted_particle_archetype_ids::MeatheadPopup, *popup_center, rng::RandomIntInclusive(0, 1) == 1);
    return popup_center;
}

void PlayMeatheadHealFeedback(State& state, const Entity& player) {
    const std::optional<Vec2> popup_center = SpawnMeatheadPopup(state, player);
    const Vec2 sound_pos = popup_center.value_or(player.GetCenter());
    (void)PlayWorldSoundEmitter(state, sound_pos, audio_asset_ids::Present);
    (void)PlayWorldSoundEmitter(state, sound_pos, audio_asset_ids::Smooch);
}

AABB ExpandAabb(const AABB& aabb, float amount) {
    return AABB::New(
        aabb.tl - Vec2::New(amount, amount),
        aabb.br + Vec2::New(amount, amount)
    );
}

void AddMeatheadDebugAnnotations(const Entity& player, State& state) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }

    const AABB sensor = ExpandAabb(player.GetAABB(), kMeatheadPickupRange);
    state.AddDebugRectAnnotation(DebugRectAnnotation{
        .area = sensor,
        .color = DebugAnnotationColor{255, 64, 192, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = (sensor.tl + sensor.br) * 0.5F,
        .text = "meathead sensor",
        .color = DebugAnnotationColor{255, 64, 192, 255},
    });
}

void BecomeCollectible(Entity& meathead) {
    meathead.can_collide = true;
    meathead.can_be_hit = true;
    meathead.render_enabled = true;
    meathead.frame_data_animator.SetAnimation(frame_data_ids::Meathead);
    meathead.frame_data_animator.loop = true;
    meathead.frame_data_animator.animate = true;
    meathead.frame_data_animator.finished = false;
}

void StepEntityLogicAsMeathead(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& meathead = state.entity_manager.entities[entity_idx];
    if (!meathead.active) {
        return;
    }

    meathead.vel = Vec2::New(0.0F, 0.0F);
    meathead.acc = Vec2::New(0.0F, 0.0F);

    if (meathead.frame_data_animator.animation_id == frame_data_ids::MeatheadRise &&
        meathead.frame_data_animator.IsFinished()) {
        BecomeCollectible(meathead);
    }
}

} // namespace

void MaybePreviewMeatheadPassive(const Entity& player, State& state) {
    if (!HasPassiveItem(player, EntityPassiveItem::Meathead)) {
        return;
    }

    AddMeatheadDebugAnnotations(player, state);

    if (state.stage_frame == 0 || state.stage_frame % kMeatheadPreviewIntervalFrames != 0) {
        return;
    }
    SpawnMeatheadPopup(state, player);
}

void OnEntityDeathForMeathead(std::size_t entity_idx, State& state, Audio& audio) {
    if (!state.player_vid.has_value()) {
        return;
    }
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& victim = state.entity_manager.entities[entity_idx];
    if (!victim.active || victim.condition != EntityCondition::Dead || IsExcludedDeathType(victim.type_)) {
        return;
    }

    Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return;
    }
    if (!HasPassiveItem(*player, EntityPassiveItem::Meathead)) {
        return;
    }

    const AABB player_collect_area = ExpandAabb(player->GetAABB(), kMeatheadPickupRange);
    if (!WorldAabbsIntersect(state.stage, player_collect_area, victim.GetAABB())) {
        return;
    }

    (void)audio;
    player->meathead_points += 1;
    bool granted_health = false;
    while (player->meathead_points >= kMeatheadPointsPerHeal) {
        player->meathead_points -= kMeatheadPointsPerHeal;
        player->health += 1;
        granted_health = true;
    }
    if (granted_health) {
        PlayMeatheadHealFeedback(state, *player);
    }
}

extern const EntityArchetype kMeatheadArchetype{
    .type_ = EntityType::Meathead,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .passive_item = EntityPassiveItem::Meathead,
    .step_logic = StepEntityLogicAsMeathead,
    .on_entity_contact = OnEntityContactAsMeathead,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::MeatheadRise),
};

} // namespace splonks::entities::meathead
