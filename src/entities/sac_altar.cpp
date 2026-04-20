#include "entities/sac_altar.hpp"

#include "audio_emitters.hpp"
#include "entities/common/common.hpp"
#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "math_types.hpp"
#include "on_damage_effects.hpp"
#include "particles/dynamic_particle.hpp"
#include "particles/ultra_dynamic_particle.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_query.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace splonks::entities::sac_altar {

namespace {

constexpr std::int32_t kAccessoryRewardFavor = 8;
constexpr std::int32_t kSecondRewardFavor = 16;
constexpr std::int32_t kHealthRewardFavor = 32;
constexpr std::int32_t kAltarBreakFavorPenalty = 8;
constexpr std::int32_t kBallAndChainPunishmentFavorThreshold = -16;
constexpr std::int32_t kGoldIdolSacrificeFavor = 8;
constexpr std::uint32_t kHealthRewardAmount = 8;
constexpr float kSacrificeAreaTopOffset = 18.0F;
constexpr float kSacrificeAreaBottomOffset = 15.0F;
constexpr float kSacrificeSmokeScaleBias = 1.0F;
constexpr float kTopperSacAnimationHoldFrames = 60.0F;
constexpr float kBallAndChainSpawnOffsetY = 18.0F;

Entity* SpawnEntityAtCenter(EntityType type_, const Vec2& center, State& state) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return nullptr;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return nullptr;
    }

    SetEntityAs(*entity, type_);
    entity->SetCenter(center);
    entity->vel = Vec2::New(0.0F, 0.0F);
    entity->acc = Vec2::New(0.0F, 0.0F);
    return entity;
}

bool IsOwnerAltarHalf(const Entity& altar) {
    return altar.frame_data_animator.animation_id == frame_data_ids::SacAltarLeft;
}

Entity* GetOwnerAltarMut(Entity& altar_piece, State& state) {
    if (altar_piece.type_ == EntityType::SacAltar && IsOwnerAltarHalf(altar_piece)) {
        return &altar_piece;
    }
    if (!altar_piece.entity_a.has_value()) {
        return nullptr;
    }

    Entity* const owner = state.entity_manager.GetEntityMut(*altar_piece.entity_a);
    if (owner == nullptr || !owner->active || owner->type_ != EntityType::SacAltar ||
        !IsOwnerAltarHalf(*owner)) {
        return nullptr;
    }
    return owner;
}

bool BelongsToOwnerAltar(const Entity& entity, const Entity& owner) {
    if (entity.vid == owner.vid) {
        return true;
    }
    if (entity.entity_a.has_value() && *entity.entity_a == owner.vid) {
        return true;
    }
    if (owner.entity_a.has_value() && entity.vid == *owner.entity_a) {
        return true;
    }
    return false;
}

AABB GetSacrificeArea(const Entity& altar) {
    return AABB::New(
        altar.pos + Vec2::New(-1.0F, -kSacrificeAreaTopOffset),
        altar.pos + Vec2::New(31.0F, kSacrificeAreaBottomOffset)
    );
}

Entity* GetPlayerMut(State& state) {
    if (!state.player_vid.has_value()) {
        return nullptr;
    }
    Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return nullptr;
    }
    return player;
}

bool PlayerHasBallAndChainPunishment(const State& state) {
    if (!state.player_vid.has_value()) {
        return false;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || !player->entity_d.has_value()) {
        return false;
    }

    const Entity* const ball = state.entity_manager.GetEntity(*player->entity_d);
    return ball != nullptr && ball->active && ball->type_ == EntityType::BallAndChainBall;
}

void SpawnBallAndChainPunishment(State& state) {
    Entity* const player = GetPlayerMut(state);
    if (player == nullptr || PlayerHasBallAndChainPunishment(state)) {
        return;
    }

    Entity* const ball = SpawnEntityAtCenter(
        EntityType::BallAndChainBall,
        player->GetCenter() + Vec2::New(0.0F, kBallAndChainSpawnOffsetY),
        state
    );
    if (ball == nullptr) {
        return;
    }

    ball->entity_a = player->vid;
    ball->vel = player->vel;
    ball->acc = Vec2::New(0.0F, 0.0F);
    player->entity_d = ball->vid;
}

void ApplySacAltarFavorDelta(State& state, std::int32_t favor_delta) {
    state.sac_altar_favor += favor_delta;
    if (state.sac_altar_favor <= kBallAndChainPunishmentFavorThreshold) {
        SpawnBallAndChainPunishment(state);
    }
}

std::optional<std::int32_t> GetSacrificeFavorValueImpl(const Entity& victim, bool alive) {
    if (victim.type_ == EntityType::GoldIdol) {
        return kGoldIdolSacrificeFavor;
    }
    if (victim.type_ == EntityType::Bomb) {
        return std::nullopt;
    }

    switch (victim.type_) {
    case EntityType::Player:
    case EntityType::Damsel:
        return alive ? 8 : 4;
    case EntityType::Shopkeeper:
        return alive ? 6 : 3;
    default:
        break;
    }

    if (!victim.can_be_stunned) {
        return std::nullopt;
    }
    return alive ? 2 : 1;
}

bool IsBackItemType(EntityType type_) {
    switch (type_) {
    case EntityType::Cape:
    case EntityType::JetPack:
    case EntityType::Parachute:
        return true;
    default:
        return false;
    }
}

bool PlayerHasBackItemType(const Entity& player, State& state, EntityType type_) {
    if (!player.back_vid.has_value()) {
        return false;
    }

    const Entity* const back_item = state.entity_manager.GetEntity(*player.back_vid);
    return back_item != nullptr && back_item->active && back_item->type_ == type_;
}

bool PlayerOwnsRewardType(const Entity& player, State& state, EntityType type_) {
    const EntityArchetype& archetype = GetEntityArchetype(type_);
    if (archetype.passive_item.has_value() && HasPassiveItem(player, *archetype.passive_item)) {
        return true;
    }
    if (IsBackItemType(type_) && PlayerHasBackItemType(player, state, type_)) {
        return true;
    }
    return false;
}

EntityType PickAccessoryReward(std::optional<VID> reward_target_vid, State& state) {
    constexpr std::array<EntityType, 8> kAccessoryRewards{
        EntityType::Gloves,
        EntityType::Mitt,
        EntityType::SpringShoes,
        EntityType::SpikeShoes,
        EntityType::Cape,
        EntityType::Spectacles,
        EntityType::Paste,
        EntityType::Compass,
    };

    std::vector<EntityType> available_rewards;
    const Entity* reward_target = nullptr;
    if (reward_target_vid.has_value()) {
        reward_target = state.entity_manager.GetEntity(*reward_target_vid);
    }

    for (const EntityType type_ : kAccessoryRewards) {
        if (reward_target != nullptr && PlayerOwnsRewardType(*reward_target, state, type_)) {
            continue;
        }
        if (type_ == EntityType::Cape && reward_target != nullptr && reward_target->back_vid.has_value()) {
            continue;
        }
        available_rewards.push_back(type_);
    }

    if (!available_rewards.empty()) {
        const int reward_index = rng::RandomIntExclusive(0, static_cast<int>(available_rewards.size()));
        return available_rewards[static_cast<std::size_t>(reward_index)];
    }

    if (reward_target == nullptr || !reward_target->back_vid.has_value()) {
        return EntityType::JetPack;
    }
    return EntityType::BombBox;
}

std::optional<VID> GetRewardTargetVid(const State& state) {
    if (!state.player_vid.has_value()) {
        return std::nullopt;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return std::nullopt;
    }
    return state.player_vid;
}

Vec2 GetAltarEffectPos(const Entity& altar, const State& state, const Graphics& graphics) {
    if (altar.entity_a.has_value()) {
        if (const Entity* const topper = state.entity_manager.GetEntity(*altar.entity_a)) {
            return common::GetEmitPointForEntity(*topper, graphics, topper->GetCenter());
        }
    }

    return common::GetEmitPointForEntity(
        altar,
        graphics,
        altar.pos + Vec2::New(16.0F, -8.0F)
    );
}

Vec2 GetAltarSoundPos(const Entity& altar, const State& state, const Graphics& graphics) {
    if (altar.entity_a.has_value()) {
        if (const Entity* const topper = state.entity_manager.GetEntity(*altar.entity_a)) {
            return common::GetVisualCenterForEntity(*topper, graphics, topper->GetCenter());
        }
    }

    return common::GetVisualCenterForEntity(
        altar,
        graphics,
        altar.pos + Vec2::New(16.0F, -8.0F)
    );
}

void TriggerTopperSacAnimation(Entity& altar, State& state) {
    if (!altar.entity_a.has_value()) {
        return;
    }

    Entity* const topper = state.entity_manager.GetEntityMut(*altar.entity_a);
    if (topper == nullptr || !topper->active) {
        return;
    }

    SetAnimation(*topper, frame_data_ids::SacAltarSac);
    topper->frame_data_animator.SetForcedFrame(0);
    topper->frame_data_animator.loop = false;
    topper->frame_data_animator.animate = true;
    topper->frame_data_animator.finished = false;
    topper->counter_b = kTopperSacAnimationHoldFrames;
}

void SpawnSacrificeSmoke(State& state, const Vec2& pos) {
    for (int i = 0; i < 8; ++i) {
        auto smoke = std::make_unique<UltraDynamicParticle>();
        smoke->frame_data_animator = FrameDataAnimator::New(frame_data_ids::BigSmoke);
        smoke->draw_layer = DrawLayer::Foreground;
        smoke->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(20, 32));
        smoke->pos = pos + Vec2::New(
            rng::RandomFloat(-5.0F, 5.0F),
            rng::RandomFloat(-3.0F, 3.0F)
        );
        const float size = rng::RandomFloat(5.0F + kSacrificeSmokeScaleBias, 9.0F + kSacrificeSmokeScaleBias);
        smoke->size = Vec2::New(size, size);
        smoke->rot = rng::RandomFloat(0.0F, 360.0F);
        smoke->alpha = rng::RandomFloat(0.65F, 0.95F);
        smoke->vel = Vec2::New(
            rng::RandomFloat(-0.25F, 0.25F),
            rng::RandomFloat(-0.85F, -0.3F)
        );
        smoke->svel = Vec2::New(rng::RandomFloat(0.02F, 0.05F), rng::RandomFloat(0.02F, 0.05F));
        smoke->rotvel = rng::RandomFloat(-0.3F, 0.3F);
        smoke->alpha_vel = -0.025F;
        smoke->acc = Vec2::New(0.0F, -0.01F);
        smoke->alpha_acc = -0.002F;
        state.particles.Add(std::move(smoke));
    }
}

void SpawnSacrificeBodySmoke(State& state, const Vec2& pos) {
    for (int i = 0; i < 6; ++i) {
        auto smoke = std::make_unique<UltraDynamicParticle>();
        smoke->frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
        smoke->draw_layer = DrawLayer::Foreground;
        smoke->counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(12, 20));
        smoke->pos = pos + Vec2::New(
            rng::RandomFloat(-4.0F, 4.0F),
            rng::RandomFloat(-2.0F, 2.0F)
        );
        const float size = rng::RandomFloat(3.0F, 5.0F);
        smoke->size = Vec2::New(size, size);
        smoke->rot = rng::RandomFloat(0.0F, 360.0F);
        smoke->alpha = rng::RandomFloat(0.7F, 0.95F);
        smoke->vel = Vec2::New(
            rng::RandomFloat(-0.22F, 0.22F),
            rng::RandomFloat(-1.1F, -0.45F)
        );
        smoke->svel = Vec2::New(rng::RandomFloat(0.04F, 0.10F), rng::RandomFloat(0.04F, 0.10F));
        smoke->rotvel = rng::RandomFloat(-2.0F, 2.0F);
        smoke->alpha_vel = -0.055F;
        smoke->acc = Vec2::New(0.0F, -0.015F);
        smoke->sacc = Vec2::New(0.008F, 0.008F);
        smoke->alpha_acc = -0.002F;
        state.particles.Add(std::move(smoke));
    }

    for (int i = 0; i < 5; ++i) {
        auto smoke = std::make_unique<UltraDynamicParticle>();
        smoke->frame_data_animator = FrameDataAnimator::New(frame_data_ids::BigSmoke);
        smoke->draw_layer = DrawLayer::Foreground;
        smoke->counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(18, 28));
        smoke->pos = pos + Vec2::New(
            rng::RandomFloat(-3.0F, 3.0F),
            rng::RandomFloat(-2.0F, 2.0F)
        );
        const float size = rng::RandomFloat(5.0F, 8.0F);
        smoke->size = Vec2::New(size, size);
        smoke->rot = rng::RandomFloat(0.0F, 360.0F);
        smoke->alpha = rng::RandomFloat(0.65F, 0.9F);
        smoke->vel = Vec2::New(
            rng::RandomFloat(-0.65F, 0.65F),
            rng::RandomFloat(-0.75F, -0.2F)
        );
        smoke->svel = Vec2::New(rng::RandomFloat(0.03F, 0.08F), rng::RandomFloat(0.03F, 0.08F));
        smoke->rotvel = rng::RandomFloat(-1.5F, 1.5F);
        smoke->alpha_vel = -0.035F;
        smoke->acc = Vec2::New(0.0F, -0.01F);
        smoke->sacc = Vec2::New(0.006F, 0.006F);
        smoke->alpha_acc = -0.0015F;
        state.particles.Add(std::move(smoke));
    }
}

void SpawnSacrificeSparks(State& state, const Vec2& pos) {
    for (int i = 0; i < 6; ++i) {
        auto spark = std::make_unique<DynamicParticle>();
        spark->frame_data_animator = FrameDataAnimator::New(frame_data_ids::Spark);
        spark->draw_layer = DrawLayer::Foreground;
        spark->counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(5, 9));
        spark->pos = pos + Vec2::New(
            rng::RandomFloat(-2.0F, 2.0F),
            rng::RandomFloat(-1.0F, 1.0F)
        );
        spark->size = Vec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(4.0F, 7.0F));
        spark->rot = rng::RandomFloat(0.0F, 360.0F);
        spark->alpha = 1.0F;
        spark->vel = Vec2::New(
            rng::RandomFloat(-0.55F, 0.55F),
            rng::RandomFloat(-2.6F, -1.2F)
        );
        spark->svel = Vec2::New(-0.12F, -0.12F);
        spark->rotvel = rng::RandomFloat(-8.0F, 8.0F);
        spark->alpha_vel = -0.14F;
        state.particles.Add(std::move(spark));
    }
}

void SpawnSacrificeBlood(State& state, const Vec2& pos) {
    for (int i = 0; i < 14; ++i) {
        auto blood = std::make_unique<UltraDynamicParticle>();
        blood->frame_data_animator = FrameDataAnimator::New(frame_data_ids::BloodBall);
        blood->draw_layer = DrawLayer::Foreground;
        blood->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(18, 30));
        blood->pos = pos + Vec2::New(
            rng::RandomFloat(-3.0F, 3.0F),
            rng::RandomFloat(-2.0F, 2.0F)
        );
        const float size = rng::RandomFloat(3.0F, 6.0F);
        blood->size = Vec2::New(size, size);
        blood->rot = rng::RandomFloat(0.0F, 360.0F);
        blood->alpha = rng::RandomFloat(0.75F, 1.0F);
        blood->vel = Vec2::New(
            rng::RandomFloat(-1.2F, 1.2F),
            rng::RandomFloat(-2.4F, -0.6F)
        );
        blood->svel = Vec2::New(-0.05F, -0.05F);
        blood->rotvel = rng::RandomFloat(-4.0F, 4.0F);
        blood->alpha_vel = -0.03F;
        blood->acc = Vec2::New(0.0F, 0.12F);
        blood->sacc = Vec2::New(-0.002F, -0.002F);
        blood->alpha_acc = -0.0015F;
        state.particles.Add(std::move(blood));
    }
}

void DeactivateAltarEntity(Entity& entity, State& state) {
    if (!entity.active) {
        return;
    }

    state.sid.Remove(entity.vid);
    entity.health = 0;
    entity.condition = EntityCondition::Dead;
    entity.render_enabled = false;
    entity.marked_for_destruction = false;
    state.entity_manager.SetInactive(entity.vid.id);
}

void SpawnAltarBreakEffects(const Entity& entity, State& state) {
    const Vec2 center = entity.GetCenter();
    SpawnSacrificeSmoke(state, center);
    SpawnSacrificeBodySmoke(state, center);
    SpawnSacrificeSparks(state, center);
}

void DeactivateLinkedAltarPieces(Entity& owner, State& state) {
    std::array<VID, 3> piece_vids{owner.vid, owner.vid, owner.vid};
    std::size_t num_piece_vids = 1;

    if (owner.entity_a.has_value()) {
        piece_vids[num_piece_vids] = *owner.entity_a;
        num_piece_vids += 1;
    }

    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active || !BelongsToOwnerAltar(entity, owner) || entity.vid == owner.vid) {
            continue;
        }
        bool already_added = false;
        for (std::size_t i = 0; i < num_piece_vids; ++i) {
            if (piece_vids[i] == entity.vid) {
                already_added = true;
                break;
            }
        }
        if (!already_added && num_piece_vids < piece_vids.size()) {
            piece_vids[num_piece_vids] = entity.vid;
            num_piece_vids += 1;
        }
    }

    for (std::size_t i = 0; i < num_piece_vids; ++i) {
        Entity& piece = state.entity_manager.entities[piece_vids[i].id];
        DeactivateAltarEntity(piece, state);
    }
}

bool GrantSacAltarReward(Entity& altar, State& state, const Graphics& graphics) {
    const Vec2 emit_pos = GetAltarEffectPos(altar, state, graphics);

    if (state.sac_altar_reward_tier == 0 && state.sac_altar_favor >= kAccessoryRewardFavor) {
        const EntityType reward_type = PickAccessoryReward(GetRewardTargetVid(state), state);
        Entity* const reward = SpawnEntityAtCenter(reward_type, emit_pos + Vec2::New(0.0F, -3.0F), state);
        if (reward == nullptr) {
            return false;
        }

        reward->vel = Vec2::New(rng::RandomFloat(-0.55F, 0.55F), -1.7F);
        state.sac_altar_reward_tier = 1;
        (void)PlayWorldSoundEmitter(state, emit_pos, audio_asset_ids::Present);
        return true;
    }

    if (state.sac_altar_reward_tier == 1 && state.sac_altar_favor >= kSecondRewardFavor) {
        Entity* const reward = SpawnEntityAtCenter(EntityType::Meathead, emit_pos + Vec2::New(0.0F, -2.0F), state);
        if (reward == nullptr) {
            return false;
        }

        reward->entity_a = altar.vid;
        reward->draw_layer = DrawLayer::Middle;
        reward->vel = Vec2::New(0.0F, 0.0F);
        reward->acc = Vec2::New(0.0F, 0.0F);
        state.sac_altar_reward_tier = 2;
        (void)PlayWorldSoundEmitter(state, emit_pos, audio_asset_ids::Present);
        return true;
    }

    if (state.sac_altar_reward_tier == 2 && state.sac_altar_favor >= kHealthRewardFavor) {
        if (const std::optional<VID> reward_target_vid = GetRewardTargetVid(state); reward_target_vid.has_value()) {
            if (Entity* const reward_target = state.entity_manager.GetEntityMut(*reward_target_vid)) {
                reward_target->health += kHealthRewardAmount;
            }
        }
        SpawnSacrificeSmoke(state, emit_pos);
        state.sac_altar_reward_tier = 3;
        (void)PlayWorldSoundEmitter(state, emit_pos, audio_asset_ids::Present);
        return true;
    }

    return false;
}

void SacrificeVictim(
    Entity& altar,
    Entity& victim,
    State& state,
    const Graphics& graphics
) {
    const std::optional<std::int32_t> favor = GetSacrificeFavorValue(victim);
    if (!favor.has_value()) {
        return;
    }

    const AABB victim_aabb = common::GetContactAabbForEntity(victim, graphics);
    const Vec2 victim_effect_pos = Vec2::New(
        (victim_aabb.tl.x + victim_aabb.br.x) * 0.5F,
        victim_aabb.br.y - 2.0F
    );
    const Vec2 altar_emit = GetAltarEffectPos(altar, state, graphics);
    const Vec2 altar_sound = GetAltarSoundPos(altar, state, graphics);

    common::DropHeldItemFromEntity(victim, state);
    common::ReleaseEntityFromHolder(victim, state);
    victim.marked_for_destruction = true;
    state.entity_manager.SetInactive(victim.vid.id);
    state.UpdateSidForEntity(victim.vid.id, graphics);

    ApplySacAltarFavorDelta(state, *favor);
    TriggerTopperSacAnimation(altar, state);
    SpawnSacrificeSmoke(state, altar_emit);
    SpawnSacrificeBodySmoke(state, victim_effect_pos);
    SpawnSacrificeSparks(state, victim_effect_pos);
    SpawnSacrificeBlood(state, victim_effect_pos);
    SpawnDamageEffectAnimationBurst(frame_data_ids::BloodBall, victim_effect_pos, state);
    (void)PlayWorldSoundEmitter(state, altar_sound, audio_asset_ids::Sacrifice);

    while (GrantSacAltarReward(altar, state, graphics)) {
    }
}

} // namespace

std::optional<std::int32_t> GetSacrificeFavorValue(const Entity& victim) {
    if (victim.condition != EntityCondition::Stunned && victim.condition != EntityCondition::Dead) {
        return std::nullopt;
    }
    return GetSacrificeFavorValueImpl(victim, victim.condition == EntityCondition::Stunned);
}

std::optional<std::int32_t> GetLivingSacrificeFavorValue(const Entity& victim) {
    return GetSacrificeFavorValueImpl(victim, true);
}

void SpawnSacrificeGainEffects(State& state, Audio& audio, const Vec2& pos) {
    (void)audio;
    SpawnSacrificeBodySmoke(state, pos);
    SpawnSacrificeSparks(state, pos);
    SpawnSacrificeBlood(state, pos);
    SpawnDamageEffectAnimationBurst(frame_data_ids::BloodBall, pos, state);
    (void)PlayWorldSoundEmitter(state, pos, audio_asset_ids::Sacrifice);
}

bool TryDepositStoredFavor(
    Entity& altar_piece,
    std::int32_t favor,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    (void)audio;
    if (favor <= 0) {
        return false;
    }

    Entity* const owner = GetOwnerAltarMut(altar_piece, state);
    if (owner == nullptr || !owner->active) {
        return false;
    }

    const Vec2 altar_emit = GetAltarEffectPos(*owner, state, graphics);
    const Vec2 altar_sound = GetAltarSoundPos(*owner, state, graphics);
    ApplySacAltarFavorDelta(state, favor);
    TriggerTopperSacAnimation(*owner, state);
    SpawnSacrificeSmoke(state, altar_emit);
    (void)PlayWorldSoundEmitter(state, altar_sound, audio_asset_ids::Sacrifice);
    while (GrantSacAltarReward(*owner, state, graphics)) {
    }
    return true;
}

void OnDeathAsSacAltarPiece(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;

    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& altar_piece = state.entity_manager.entities[entity_idx];
    if (!altar_piece.active) {
        return;
    }

    Entity* const owner = GetOwnerAltarMut(altar_piece, state);
    if (owner == nullptr) {
        return;
    }
    if (!owner->active) {
        return;
    }

    ApplySacAltarFavorDelta(state, -kAltarBreakFavorPenalty);
    const Vec2 owner_center = owner->GetCenter();
    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active || !BelongsToOwnerAltar(entity, *owner)) {
            continue;
        }
        SpawnAltarBreakEffects(entity, state);
    }
    (void)PlayWorldSoundEmitter(state, owner_center, audio_asset_ids::PotShatter);
    AddShake(state, owner_center, 1.4F, 1.8F, ShakeMask::All, owner->vid);
    DeactivateLinkedAltarPieces(*owner, state);
}

namespace {

void StepEntityLogicAsSacAltar(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& altar = state.entity_manager.entities[entity_idx];
    if (!altar.active || !IsOwnerAltarHalf(altar)) {
        return;
    }

    const AABB sacrifice_area = GetSacrificeArea(altar);
    const std::vector<VID> candidates = QueryEntitiesInAabb(state, sacrifice_area, altar.vid);
    for (const VID& candidate_vid : candidates) {
        Entity* const victim = state.entity_manager.GetEntityMut(candidate_vid);
        if (victim == nullptr || !victim->active) {
            continue;
        }
        if (victim->type_ == EntityType::SacAltar || victim->type_ == EntityType::SacAltarTopper ||
            victim->type_ == EntityType::Altar) {
            continue;
        }
        if (victim->held_by_vid.has_value()) {
            continue;
        }
        if (!GetSacrificeFavorValue(*victim).has_value()) {
            continue;
        }
        if (!victim->grounded) {
            continue;
        }

        const AABB victim_aabb = common::GetContactAabbForEntity(*victim, graphics);
        if (!WorldAabbsIntersect(state.stage, sacrifice_area, victim_aabb)) {
            continue;
        }

        SacrificeVictim(altar, *victim, state, graphics);
    }
}

} // namespace

extern const EntityArchetype kSacAltarArchetype{
    .type_ = EntityType::SacAltar,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .vanish_on_death = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingSpikesAndExplosion,
    .on_death = OnDeathAsSacAltarPiece,
    .step_logic = StepEntityLogicAsSacAltar,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::SacAltarLeft),
};

} // namespace splonks::entities::sac_altar
