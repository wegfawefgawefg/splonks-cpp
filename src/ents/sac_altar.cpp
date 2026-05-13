#include "ents/sac_altar.hpp"

#include "audio_emitters.hpp"
#include "ents/common/common.hpp"
#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "math_types.hpp"
#include "particles/sprite_particle.hpp"
#include "on_damage_effects.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace splonks::ents::sac_altar {

namespace {

constexpr std::int32_t kAccessoryRewardFavor = 8;
constexpr std::int32_t kSecondRewardFavor = 16;
constexpr std::int32_t kHealthRewardFavor = 32;
constexpr std::int32_t kAltarBreakFavorPenalty = 8;
constexpr std::int32_t kBallAndChainPunishmentFavorThreshold = -16;
constexpr std::int32_t kGoldIdolSacrificeFavor = 8;
constexpr std::uint32_t kHealthRewardAmount = 8;
constexpr float kSacrificeSurfaceTopOffset = 20.0F;
constexpr float kSacrificeSurfaceBottomOffset = 2.0F;
constexpr float kSacrificeSmokeScaleBias = 1.0F;
constexpr float kTopperSacAnimHoldFrames = 60.0F;
constexpr float kBallAndChainSpawnOffsetY = 18.0F;

bool IsOwnerAltarHalf(const Ent& altar) {
    return altar.aframe_animator.anim_id == aframe_ids::SacAltarLeft;
}

Ent* GetOwnerAltarMut(Ent& altar_piece, State& state) {
    if (altar_piece.type_ == EntType::SacAltar && IsOwnerAltarHalf(altar_piece)) {
        return &altar_piece;
    }
    if (!altar_piece.ent_a.has_value()) {
        return nullptr;
    }

    Ent* const owner = state.ents.GetEntMut(*altar_piece.ent_a);
    if (owner == nullptr || !owner->active || owner->type_ != EntType::SacAltar ||
        !IsOwnerAltarHalf(*owner)) {
        return nullptr;
    }
    return owner;
}

bool BelongsToOwnerAltar(const Ent& ent, const Ent& owner) {
    if (ent.vid == owner.vid) {
        return true;
    }
    if (ent.ent_a.has_value() && *ent.ent_a == owner.vid) {
        return true;
    }
    if (owner.ent_a.has_value() && ent.vid == *owner.ent_a) {
        return true;
    }
    return false;
}

AABB GetSacrificeArea(const Ent& altar) {
    return AABB::New(
        altar.pos + Vec2::New(-1.0F, -kSacrificeSurfaceTopOffset),
        altar.pos + Vec2::New(31.0F, kSacrificeSurfaceBottomOffset)
    );
}

bool PlayerHasBallAndChainPunishment(const State& state, const Ent& player) {
    if (!player.ent_d.has_value()) {
        return false;
    }

    const Ent* const ball = state.ents.GetEnt(*player.ent_d);
    return ball != nullptr && ball->active && ball->type_ == EntType::BallAndChainBall;
}

void SpawnBallAndChainPunishment(State& state, const Ent* altar_context) {
    const Vec2 search_pos = altar_context != nullptr ? altar_context->GetCenter() : Vec2::New(0.0F, 0.0F);
    const std::optional<VID> player_vid = altar_context != nullptr
        ? FindNearestPlayerVid(state, search_pos, true)
        : FindFirstConnectedLivingPlayerVid(state);
    Ent* const player = player_vid.has_value() ? state.ents.GetEntMut(*player_vid) : nullptr;
    if (player == nullptr || !player->active || player->condition == EntCondition::Dead ||
        PlayerHasBallAndChainPunishment(state, *player)) {
        return;
    }

    Ent* const ball = world_ops::SpawnEnt(
        state,
        EntType::BallAndChainBall,
        [&](Ent& spawned_ball) {
            spawned_ball.SetCenter(player->GetCenter() + Vec2::New(0.0F, kBallAndChainSpawnOffsetY));
            spawned_ball.ent_a = player->vid;
            spawned_ball.vel = player->vel;
            spawned_ball.acc = Vec2::New(0.0F, 0.0F);
        }
    );
    if (ball == nullptr) {
        return;
    }

    player->ent_d = ball->vid;
}

void ApplySacAltarFavorDelta(State& state, std::int32_t favor_delta, const Ent* altar_context) {
    state.sac_altar_favor += favor_delta;
    if (state.sac_altar_favor <= kBallAndChainPunishmentFavorThreshold) {
        SpawnBallAndChainPunishment(state, altar_context);
    }
}

std::optional<std::int32_t> GetSacrificeFavorValueImpl(const Ent& victim, bool alive) {
    if (victim.type_ == EntType::GoldIdol) {
        return kGoldIdolSacrificeFavor;
    }
    if (victim.type_ == EntType::Bomb) {
        return std::nullopt;
    }

    switch (victim.type_) {
    case EntType::Player:
    case EntType::FlappyBee:
    case EntType::FleshGuy:
    case EntType::Damsel:
        return alive ? 8 : 4;
    case EntType::Shopkeeper:
        return alive ? 6 : 3;
    default:
        break;
    }

    if (!victim.can_be_stunned) {
        return std::nullopt;
    }
    return alive ? 2 : 1;
}

bool IsBackItemType(EntType type_) {
    switch (type_) {
    case EntType::Cape:
    case EntType::JetPack:
    case EntType::TeleporterBackpack:
    case EntType::Parachute:
        return true;
    default:
        return false;
    }
}

bool PlayerHasBackItemType(const Ent& player, State& state, EntType type_) {
    if (!player.back_vid.has_value()) {
        return false;
    }

    const Ent* const back_item = state.ents.GetEnt(*player.back_vid);
    return back_item != nullptr && back_item->active && back_item->type_ == type_;
}

bool PlayerOwnsRewardType(const Ent& player, State& state, EntType type_) {
    const EntSpec& spec = GetEntSpec(type_);
    if (spec.pickup_effect.has_value() && HasEffect(player, *spec.pickup_effect)) {
        return true;
    }
    if (IsBackItemType(type_) && PlayerHasBackItemType(player, state, type_)) {
        return true;
    }
    return false;
}

EntType PickAccessoryReward(std::optional<VID> reward_target_vid, State& state) {
    constexpr std::array<EntType, 8> kAccessoryRewards{
        EntType::Gloves,
        EntType::Mitt,
        EntType::SpringShoes,
        EntType::SpikeShoes,
        EntType::Cape,
        EntType::Spectacles,
        EntType::Paste,
        EntType::Compass,
    };

    std::vector<EntType> available_rewards;
    const Ent* reward_target = nullptr;
    if (reward_target_vid.has_value()) {
        reward_target = state.ents.GetEnt(*reward_target_vid);
    }

    for (const EntType type_ : kAccessoryRewards) {
        if (reward_target != nullptr && PlayerOwnsRewardType(*reward_target, state, type_)) {
            continue;
        }
        if (type_ == EntType::Cape && reward_target != nullptr && reward_target->back_vid.has_value()) {
            continue;
        }
        available_rewards.push_back(type_);
    }

    if (!available_rewards.empty()) {
        const int reward_index =
            state.drng.RandomIntExclusive(0, static_cast<int>(available_rewards.size()));
        return available_rewards[static_cast<std::size_t>(reward_index)];
    }

    if (reward_target == nullptr || !reward_target->back_vid.has_value()) {
        return EntType::JetPack;
    }
    return EntType::BombBox;
}

std::optional<VID> GetRewardTargetVid(const State& state, const Ent& altar) {
    return FindNearestPlayerVid(state, altar.GetCenter(), true);
}

Vec2 GetAltarEffectPos(const Ent& altar, const State& state, const Graphics& graphics) {
    if (altar.ent_a.has_value()) {
        if (const Ent* const topper = state.ents.GetEnt(*altar.ent_a)) {
            return common::GetEmitPointForEnt(*topper, graphics, topper->GetCenter());
        }
    }

    return common::GetEmitPointForEnt(
        altar,
        graphics,
        altar.pos + Vec2::New(16.0F, -8.0F)
    );
}

Vec2 GetAltarSoundPos(const Ent& altar, const State& state, const Graphics& graphics) {
    if (altar.ent_a.has_value()) {
        if (const Ent* const topper = state.ents.GetEnt(*altar.ent_a)) {
            return common::GetVisualCenterForEnt(*topper, graphics, topper->GetCenter());
        }
    }

    return common::GetVisualCenterForEnt(
        altar,
        graphics,
        altar.pos + Vec2::New(16.0F, -8.0F)
    );
}

void TriggerTopperSacAnim(Ent& altar, State& state) {
    if (!altar.ent_a.has_value()) {
        return;
    }

    Ent* const topper = state.ents.GetEntMut(*altar.ent_a);
    if (topper == nullptr || !topper->active) {
        return;
    }

    SetAnim(*topper, aframe_ids::SacAltarSac);
    topper->aframe_animator.SetForcedFrame(0);
    topper->aframe_animator.loop = false;
    topper->aframe_animator.animate = true;
    topper->aframe_animator.finished = false;
    topper->counter_b = kTopperSacAnimHoldFrames;
}

void SpawnSacrificeSmoke(State& state, const Vec2& pos) {
    for (int i = 0; i < 8; ++i) {
        SpriteParticle smoke{};
        smoke.aframe_animator = AFrameAnimator::New(aframe_ids::BigSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(20, 32));
        smoke.pos = pos + Vec2::New(
            rng::RandomFloat(-5.0F, 5.0F),
            rng::RandomFloat(-3.0F, 3.0F)
        );
        const float size = rng::RandomFloat(5.0F + kSacrificeSmokeScaleBias, 9.0F + kSacrificeSmokeScaleBias);
        smoke.size = Vec2::New(size, size);
        smoke.rot = rng::RandomFloat(0.0F, 360.0F);
        smoke.alpha = rng::RandomFloat(0.65F, 0.95F);
        smoke.vel = Vec2::New(
            rng::RandomFloat(-0.25F, 0.25F),
            rng::RandomFloat(-0.85F, -0.3F)
        );
        smoke.svel = Vec2::New(rng::RandomFloat(0.02F, 0.05F), rng::RandomFloat(0.02F, 0.05F));
        smoke.rotvel = rng::RandomFloat(-0.3F, 0.3F);
        smoke.alpha_vel = -0.025F;
        smoke.acc = Vec2::New(0.0F, -0.01F);
        smoke.alpha_acc = -0.002F;
        state.particles.Add(std::move(smoke));
    }
}

void SpawnSacrificeBodySmoke(State& state, const Vec2& pos) {
    for (int i = 0; i < 6; ++i) {
        SpriteParticle smoke{};
        smoke.aframe_animator = AFrameAnimator::New(aframe_ids::LittleSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(12, 20));
        smoke.pos = pos + Vec2::New(
            rng::RandomFloat(-4.0F, 4.0F),
            rng::RandomFloat(-2.0F, 2.0F)
        );
        const float size = rng::RandomFloat(3.0F, 5.0F);
        smoke.size = Vec2::New(size, size);
        smoke.rot = rng::RandomFloat(0.0F, 360.0F);
        smoke.alpha = rng::RandomFloat(0.7F, 0.95F);
        smoke.vel = Vec2::New(
            rng::RandomFloat(-0.22F, 0.22F),
            rng::RandomFloat(-1.1F, -0.45F)
        );
        smoke.svel = Vec2::New(rng::RandomFloat(0.04F, 0.10F), rng::RandomFloat(0.04F, 0.10F));
        smoke.rotvel = rng::RandomFloat(-2.0F, 2.0F);
        smoke.alpha_vel = -0.055F;
        smoke.acc = Vec2::New(0.0F, -0.015F);
        smoke.sacc = Vec2::New(0.008F, 0.008F);
        smoke.alpha_acc = -0.002F;
        state.particles.Add(std::move(smoke));
    }

    for (int i = 0; i < 5; ++i) {
        SpriteParticle smoke{};
        smoke.aframe_animator = AFrameAnimator::New(aframe_ids::BigSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(18, 28));
        smoke.pos = pos + Vec2::New(
            rng::RandomFloat(-3.0F, 3.0F),
            rng::RandomFloat(-2.0F, 2.0F)
        );
        const float size = rng::RandomFloat(5.0F, 8.0F);
        smoke.size = Vec2::New(size, size);
        smoke.rot = rng::RandomFloat(0.0F, 360.0F);
        smoke.alpha = rng::RandomFloat(0.65F, 0.9F);
        smoke.vel = Vec2::New(
            rng::RandomFloat(-0.65F, 0.65F),
            rng::RandomFloat(-0.75F, -0.2F)
        );
        smoke.svel = Vec2::New(rng::RandomFloat(0.03F, 0.08F), rng::RandomFloat(0.03F, 0.08F));
        smoke.rotvel = rng::RandomFloat(-1.5F, 1.5F);
        smoke.alpha_vel = -0.035F;
        smoke.acc = Vec2::New(0.0F, -0.01F);
        smoke.sacc = Vec2::New(0.006F, 0.006F);
        smoke.alpha_acc = -0.0015F;
        state.particles.Add(std::move(smoke));
    }
}

void SpawnSacrificeSparks(State& state, const Vec2& pos) {
    for (int i = 0; i < 6; ++i) {
        SpriteParticle spark{};
        spark.aframe_animator = AFrameAnimator::New(aframe_ids::Spark);
        spark.draw_layer = DrawLayer::Foreground;
        spark.lighting_mode = ParticleLightingMode::Emissive;
        spark.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(5, 9));
        spark.pos = pos + Vec2::New(
            rng::RandomFloat(-2.0F, 2.0F),
            rng::RandomFloat(-1.0F, 1.0F)
        );
        spark.size = Vec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(4.0F, 7.0F));
        spark.rot = rng::RandomFloat(0.0F, 360.0F);
        spark.alpha = 1.0F;
        spark.vel = Vec2::New(
            rng::RandomFloat(-0.55F, 0.55F),
            rng::RandomFloat(-2.6F, -1.2F)
        );
        spark.svel = Vec2::New(-0.12F, -0.12F);
        spark.rotvel = rng::RandomFloat(-8.0F, 8.0F);
        spark.alpha_vel = -0.14F;
        state.particles.Add(std::move(spark));
    }
}

void SpawnSacrificeBlood(State& state, const Vec2& pos) {
    for (int i = 0; i < 14; ++i) {
        SpriteParticle blood{};
        blood.aframe_animator = AFrameAnimator::New(aframe_ids::BloodBall);
        blood.draw_layer = DrawLayer::Foreground;
        blood.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(18, 30));
        blood.pos = pos + Vec2::New(
            rng::RandomFloat(-3.0F, 3.0F),
            rng::RandomFloat(-2.0F, 2.0F)
        );
        const float size = rng::RandomFloat(3.0F, 6.0F);
        blood.size = Vec2::New(size, size);
        blood.rot = rng::RandomFloat(0.0F, 360.0F);
        blood.alpha = rng::RandomFloat(0.75F, 1.0F);
        blood.vel = Vec2::New(
            rng::RandomFloat(-1.2F, 1.2F),
            rng::RandomFloat(-2.4F, -0.6F)
        );
        blood.svel = Vec2::New(-0.05F, -0.05F);
        blood.rotvel = rng::RandomFloat(-4.0F, 4.0F);
        blood.alpha_vel = -0.03F;
        blood.acc = Vec2::New(0.0F, 0.12F);
        blood.sacc = Vec2::New(-0.002F, -0.002F);
        blood.alpha_acc = -0.0015F;
        state.particles.Add(std::move(blood));
    }
}

void DeactivateAltarEnt(Ent& ent, State& state) {
    if (!ent.active) {
        return;
    }

    state.sid.Remove(ent.vid);
    ent.health = 0;
    ent.condition = EntCondition::Dead;
    ent.render_enabled = false;
    ent.marked_for_destruction = false;
    (void)world_ops::DeactivateEnt(state, ent.vid);
}

void SpawnAltarBreakEffects(const Ent& ent, State& state) {
    const Vec2 center = ent.GetCenter();
    SpawnSacrificeSmoke(state, center);
    SpawnSacrificeBodySmoke(state, center);
    SpawnSacrificeSparks(state, center);
}

void DeactivateLinkedAltarPieces(Ent& owner, State& state) {
    std::array<VID, 3> piece_vids{owner.vid, owner.vid, owner.vid};
    std::size_t num_piece_vids = 1;

    if (owner.ent_a.has_value()) {
        piece_vids[num_piece_vids] = *owner.ent_a;
        num_piece_vids += 1;
    }

    for (const Ent& ent : state.ents.ents) {
        if (!ent.active || !BelongsToOwnerAltar(ent, owner) || ent.vid == owner.vid) {
            continue;
        }
        bool already_added = false;
        for (std::size_t i = 0; i < num_piece_vids; ++i) {
            if (piece_vids[i] == ent.vid) {
                already_added = true;
                break;
            }
        }
        if (!already_added && num_piece_vids < piece_vids.size()) {
            piece_vids[num_piece_vids] = ent.vid;
            num_piece_vids += 1;
        }
    }

    for (std::size_t i = 0; i < num_piece_vids; ++i) {
        Ent& piece = state.ents.ents[piece_vids[i].id];
        DeactivateAltarEnt(piece, state);
    }
}

bool GrantSacAltarReward(Ent& altar, State& state, const Graphics& graphics) {
    const Vec2 emit_pos = GetAltarEffectPos(altar, state, graphics);

    if (state.sac_altar_reward_tier == 0 && state.sac_altar_favor >= kAccessoryRewardFavor) {
        const EntType reward_type = PickAccessoryReward(GetRewardTargetVid(state, altar), state);
        Ent* const reward = world_ops::SpawnEnt(state, reward_type, [&](Ent& spawned_reward) {
            spawned_reward.SetCenter(emit_pos + Vec2::New(0.0F, -3.0F));
            spawned_reward.vel = Vec2::New(state.drng.RandomFloat(-0.55F, 0.55F), -1.7F);
            spawned_reward.acc = Vec2::New(0.0F, 0.0F);
        });
        if (reward == nullptr) {
            return false;
        }

        state.sac_altar_reward_tier = 1;
        (void)PlayWorldSoundEmitter(state, emit_pos, audio_asset_ids::Present);
        return true;
    }

    if (state.sac_altar_reward_tier == 1 && state.sac_altar_favor >= kSecondRewardFavor) {
        Ent* const reward = world_ops::SpawnEnt(state, EntType::Meathead, [&](Ent& spawned_reward) {
            spawned_reward.SetCenter(emit_pos + Vec2::New(0.0F, -2.0F));
            spawned_reward.ent_a = altar.vid;
            spawned_reward.draw_layer = DrawLayer::Middle;
            spawned_reward.vel = Vec2::New(0.0F, 0.0F);
            spawned_reward.acc = Vec2::New(0.0F, 0.0F);
        });
        if (reward == nullptr) {
            return false;
        }

        state.sac_altar_reward_tier = 2;
        (void)PlayWorldSoundEmitter(state, emit_pos, audio_asset_ids::Present);
        return true;
    }

    if (state.sac_altar_reward_tier == 2 && state.sac_altar_favor >= kHealthRewardFavor) {
        if (const std::optional<VID> reward_target_vid = GetRewardTargetVid(state, altar);
            reward_target_vid.has_value()) {
            if (Ent* const reward_target = state.ents.GetEntMut(*reward_target_vid)) {
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
    Ent& altar,
    Ent& victim,
    State& state,
    const Graphics& graphics
) {
    const std::optional<std::int32_t> favor = GetSacrificeFavorValue(victim);
    if (!favor.has_value()) {
        return;
    }

    const AABB victim_aabb = common::GetContactAabbForEnt(victim, graphics);
    const Vec2 victim_effect_pos = Vec2::New(
        (victim_aabb.tl.x + victim_aabb.br.x) * 0.5F,
        victim_aabb.br.y - 2.0F
    );
    const Vec2 altar_emit = GetAltarEffectPos(altar, state, graphics);
    const Vec2 altar_sound = GetAltarSoundPos(altar, state, graphics);

    common::DropHeldItemFromEnt(victim, state);
    common::ReleaseEntFromHolder(victim, state);
    victim.marked_for_destruction = true;
    (void)world_ops::DeactivateEnt(state, victim.vid);
    state.UpdateSidForEnt(victim.vid.id, graphics);

    ApplySacAltarFavorDelta(state, *favor, &altar);
    TriggerTopperSacAnim(altar, state);
    SpawnSacrificeSmoke(state, altar_emit);
    SpawnSacrificeBodySmoke(state, victim_effect_pos);
    SpawnSacrificeSparks(state, victim_effect_pos);
    SpawnSacrificeBlood(state, victim_effect_pos);
    SpawnDamageEffectAnimBurst(aframe_ids::BloodBall, victim_effect_pos, state);
    (void)PlayWorldSoundEmitter(state, altar_sound, audio_asset_ids::Sacrifice);

    while (GrantSacAltarReward(altar, state, graphics)) {
    }
}

} // namespace

std::optional<std::int32_t> GetSacrificeFavorValue(const Ent& victim) {
    if (victim.condition != EntCondition::Stunned && victim.condition != EntCondition::Dead) {
        return std::nullopt;
    }
    return GetSacrificeFavorValueImpl(victim, victim.condition == EntCondition::Stunned);
}

std::optional<std::int32_t> GetLivingSacrificeFavorValue(const Ent& victim) {
    return GetSacrificeFavorValueImpl(victim, true);
}

void SpawnSacrificeGainEffects(State& state, Audio& audio, const Vec2& pos) {
    (void)audio;
    SpawnSacrificeBodySmoke(state, pos);
    SpawnSacrificeSparks(state, pos);
    SpawnSacrificeBlood(state, pos);
    SpawnDamageEffectAnimBurst(aframe_ids::BloodBall, pos, state);
    (void)PlayWorldSoundEmitter(state, pos, audio_asset_ids::Sacrifice);
}

bool TryDepositStoredFavor(
    Ent& altar_piece,
    std::int32_t favor,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    (void)audio;
    if (favor <= 0) {
        return false;
    }

    Ent* const owner = GetOwnerAltarMut(altar_piece, state);
    if (owner == nullptr || !owner->active) {
        return false;
    }

    const Vec2 altar_emit = GetAltarEffectPos(*owner, state, graphics);
    const Vec2 altar_sound = GetAltarSoundPos(*owner, state, graphics);
    ApplySacAltarFavorDelta(state, favor, owner);
    TriggerTopperSacAnim(*owner, state);
    SpawnSacrificeSmoke(state, altar_emit);
    (void)PlayWorldSoundEmitter(state, altar_sound, audio_asset_ids::Sacrifice);
    while (GrantSacAltarReward(*owner, state, graphics)) {
    }
    return true;
}

void OnDeathAsSacAltarPiece(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;

    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& altar_piece = state.ents.ents[ent_idx];
    if (!altar_piece.active) {
        return;
    }

    Ent* const owner = GetOwnerAltarMut(altar_piece, state);
    if (owner == nullptr) {
        return;
    }
    if (!owner->active) {
        return;
    }

    ApplySacAltarFavorDelta(state, -kAltarBreakFavorPenalty, owner);
    const Vec2 owner_center = owner->GetCenter();
    for (const Ent& ent : state.ents.ents) {
        if (!ent.active || !BelongsToOwnerAltar(ent, *owner)) {
            continue;
        }
        SpawnAltarBreakEffects(ent, state);
    }
    (void)PlayWorldSoundEmitter(state, owner_center, audio_asset_ids::PotShatter);
    AddShake(state, owner_center, 1.4F, 1.8F, ShakeMask::All, owner->vid);
    DeactivateLinkedAltarPieces(*owner, state);
}

namespace {

void StepEntLogicAsSacAltar(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& altar = state.ents.ents[ent_idx];
    if (!altar.active || !IsOwnerAltarHalf(altar)) {
        return;
    }

    const AABB sacrifice_area = GetSacrificeArea(altar);
    const std::vector<VID> candidates = QueryEntsInAabb(state, sacrifice_area, altar.vid);
    for (const VID& candidate_vid : candidates) {
        Ent* const victim = state.ents.GetEntMut(candidate_vid);
        if (victim == nullptr || !victim->active) {
            continue;
        }
        if (victim->type_ == EntType::SacAltar || victim->type_ == EntType::SacAltarTopper ||
            victim->type_ == EntType::Altar) {
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

        const AABB victim_aabb = common::GetContactAabbForEnt(*victim, graphics);
        if (victim_aabb.br.y < sacrifice_area.tl.y || victim_aabb.br.y > sacrifice_area.br.y) {
            continue;
        }
        if (!WorldAabbsIntersect(state.stage, sacrifice_area, victim_aabb)) {
            continue;
        }

        SacrificeVictim(altar, *victim, state, graphics);
    }
}

} // namespace

extern const EntSpec kSacAltarSpec{
    .type_ = EntType::SacAltar,
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
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingSpikesAndExplosion,
    .on_death = OnDeathAsSacAltarPiece,
    .step_logic = StepEntLogicAsSacAltar,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::SacAltarLeft),
};

} // namespace splonks::ents::sac_altar
