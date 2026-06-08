#include "ents/chest.hpp"

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "controls.hpp"
#include "ents/common/common.hpp"
#include "ent/spec.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "math_types.hpp"
#include "particles/sprite_particle.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <memory>
#include <optional>

namespace splonks::ents::chest {

namespace {

constexpr int kChestTrapOdds = 12;
constexpr int kChestTreasureDropMin = 3;
constexpr int kChestTreasureDropMax = 4;
constexpr int kChestBonusGemOdds = 4;
constexpr float kChestLootLaunchY = -2.0F;
constexpr float kChestSparkleGravity = 0.03F;
constexpr float kChestSparkleAlphaVel = -0.05F;
constexpr float kChestSparkleAlphaAcc = -0.002F;
constexpr float kChestTrapFuseFrames = 40.0F;

common::ContactResult OnEntContactAsUdjatEye(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (graphics == nullptr || audio == nullptr ||
        !common::CanCollectPickupFromContact(ent_idx, other_ent_idx, state)) {
        return common::ContactResult{};
    }
    Ent& collector = state.ents.ents[other_ent_idx];
    const Ent& pickup = state.ents.ents[ent_idx];
    if (!TryCollectInventoryPickup(state, collector, pickup)) {
        return common::ContactResult{};
    }

    if (state.quest_state.quest_id == QuestId::Classic) {
        state.quest_state.classic.made_udjat_eye = true;
        state.quest_state.classic.has_udjat_eye = true;
    }

    (void)PlayEntCenterSoundEmitter(state, pickup, audio_asset_ids::Equip);
    common::DeactivateCollectedPickup(ent_idx, state, *graphics);
    return common::ContactResult{};
}

bool IsOpenWithAnim(const Ent& ent, AFrameId anim_id) {
    return ent.aframe_animator.anim_id == anim_id;
}

void LaunchChestLoot(State& state, Ent& ent, std::optional<VID> opener_vid = std::nullopt) {
    ent.vel = sim::ToSimVec2(Vec2::New(
        static_cast<float>(
            state.drng.RandomIntInclusive(0, 3) -
            state.drng.RandomIntInclusive(0, 3)),
        kChestLootLaunchY
    ));
    ent.thrown_by = opener_vid;
    ent.thrown_immunity_timer =
        opener_vid.has_value() ? common::kThrownByImmunityDuration : 0;
}

EntType RandomChestGemType(State& state) {
    switch (state.drng.RandomIntInclusive(1, 3)) {
    case 1:
        return EntType::EmeraldBig;
    case 2:
        return EntType::SapphireBig;
    case 3:
        return EntType::RubyBig;
    }

    return EntType::EmeraldBig;
}

void SpawnChestSparkles(const Vec2& emit_pos, State& state) {
    const int count = rng::RandomIntInclusive(5, 7);
    for (int i = 0; i < count; ++i) {
        SpriteParticle sparkle{};
        sparkle.aframe_animator = AFrameAnimator::New(aframe_ids::Sparkle);
        sparkle.draw_layer = DrawLayer::Foreground;
        sparkle.lighting_mode = ParticleLightingMode::Emissive;
        sparkle.counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(16, 28));
        sparkle.pos = emit_pos + Vec2::New(
            rng::RandomFloat(-2.0F, 2.0F),
            rng::RandomFloat(-1.0F, 1.0F)
        );
        sparkle.size = Vec2::New(rng::RandomFloat(5.0F, 8.0F), rng::RandomFloat(5.0F, 8.0F));
        sparkle.rot = rng::RandomFloat(0.0F, 360.0F);
        sparkle.alpha = rng::RandomFloat(0.75F, 1.0F);
        sparkle.vel = Vec2::New(
            rng::RandomFloat(-0.6F, 0.6F),
            rng::RandomFloat(-1.2F, -0.45F)
        );
        sparkle.svel = Vec2::New(-0.03F, -0.03F);
        sparkle.rotvel = rng::RandomFloat(-5.0F, 5.0F);
        sparkle.alpha_vel = kChestSparkleAlphaVel;
        sparkle.acc = Vec2::New(0.0F, kChestSparkleGravity);
        sparkle.sacc = Vec2::New(-0.003F, -0.003F);
        sparkle.alpha_acc = kChestSparkleAlphaAcc;
        state.particles.Add(std::move(sparkle));
    }
}

void SpawnChestTrapBomb(const Vec2& spawn_center, State& state) {
    (void)world_ops::SpawnEnt(state, EntType::Bomb, [&](Ent& bomb) {
        bomb.SetRenderCenter(spawn_center);
        bomb.vel = sim::ToSimVec2(Vec2::New(
            static_cast<float>(
                state.drng.RandomIntInclusive(0, 3) -
                state.drng.RandomIntInclusive(0, 3)),
            kChestLootLaunchY
        ));
        bomb.acc = sim::Vec2::zero();
        bomb.counter_a = kChestTrapFuseFrames;
        SetAnim(bomb, aframe_ids::LiveGrenade);
    });
}

void SpawnChestTreasure(
    const Vec2& spawn_center,
    State& state,
    std::optional<VID> opener_vid = std::nullopt
) {
    const int gem_count =
        state.drng.RandomIntInclusive(kChestTreasureDropMin, kChestTreasureDropMax);
    for (int i = 0; i < gem_count; ++i) {
        if (world_ops::SpawnEnt(state, RandomChestGemType(state), [&](Ent& gem) {
                gem.SetRenderCenter(spawn_center);
                gem.vel = sim::Vec2::zero();
                gem.acc = sim::Vec2::zero();
                LaunchChestLoot(state, gem, opener_vid);
            }) == nullptr) {
            return;
        }
    }

    if (state.drng.RandomIntInclusive(1, kChestBonusGemOdds) != 1) {
        return;
    }

    (void)world_ops::SpawnEnt(state, RandomChestGemType(state), [&](Ent& gem) {
        gem.SetRenderCenter(spawn_center);
        gem.vel = sim::Vec2::zero();
        gem.acc = sim::Vec2::zero();
        LaunchChestLoot(state, gem, opener_vid);
    });
}

bool IsEntOverlappingChest(
    std::size_t chest_idx,
    const Ent& interactor,
    const State& state,
    const Graphics& graphics
) {
    if (!interactor.active || chest_idx >= state.ents.ents.size()) {
        return false;
    }
    const Ent& chest = state.ents.ents[chest_idx];
    const sim::AABB interactor_aabb = common::GetContactAabbForEnt(interactor, graphics);
    const sim::Vec2 interactor_center = interactor_aabb.center();
    const sim::AABB chest_aabb = GetNearestWorldAabb(
        state.stage,
        interactor_center,
        common::GetContactAabbForEnt(chest, graphics)
    );
    return gfxp::aabbs_intersect(
        interactor_aabb,
        chest_aabb
    );
}

void ConsumeHeldChestKey(Ent* holder, Ent& key, State& state, const Graphics& graphics) {
    if (holder != nullptr && holder->holding_vid == key.vid) {
        holder->holding_vid.reset();
        holder->holding = false;
        holder->holding_timer = kDefaultHoldingTimer;
    }
    key.held_by_vid.reset();
    key.attach_mode = AttachMode::None;
    StopUsingEnt(key);
    (void)world_ops::DeactivateEnt(state, key.vid);
    state.UpdateSidForEnt(key.vid.id, graphics);
}

bool CanUnlockKeyChestFromHeldKey(
    std::size_t chest_idx,
    State& state,
    const Graphics& graphics,
    std::optional<VID> required_key_vid,
    Ent** holder_out,
    Ent** key_out
) {
    if (holder_out != nullptr) {
        *holder_out = nullptr;
    }
    if (key_out != nullptr) {
        *key_out = nullptr;
    }
    if (chest_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& chest = state.ents.ents[chest_idx];
    const sim::AABB chest_aabb = common::GetContactAabbForEnt(chest, graphics);
    const std::vector<VID> touched = common::GatherTouchedEntContactsForAabb(
        chest_idx,
        chest_aabb,
        graphics,
        state
    );
    for (const VID vid : touched) {
        Ent* const key = state.ents.GetEntMut(vid);
        if (key == nullptr || !key->active || key->type_ != EntType::ChestKey ||
            !key->held_by_vid.has_value() ||
            (required_key_vid.has_value() && key->vid != *required_key_vid)) {
            continue;
        }

        if (holder_out != nullptr) {
            *holder_out = state.ents.GetEntMut(*key->held_by_vid);
        }
        if (key_out != nullptr) {
            *key_out = key;
        }
        return true;
    }
    return false;
}

bool CanUnlockKeyChestFromHeldKey(
    std::size_t chest_idx,
    State& state,
    const Graphics& graphics,
    Ent** holder_out,
    Ent** key_out
) {
    return CanUnlockKeyChestFromHeldKey(
        chest_idx,
        state,
        graphics,
        std::nullopt,
        holder_out,
        key_out
    );
}

bool TryOpenTreasureChestAt(
    std::size_t ent_idx,
    const Vec2& emit_pos,
    State& state,
    Audio& audio,
    std::optional<VID> opener_vid = std::nullopt
) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& chest = state.ents.ents[ent_idx];
    if (!chest.active || chest.condition == EntCondition::Dead ||
        IsOpenWithAnim(chest, aframe_ids::ChestOpen)) {
        return false;
    }

    SetAnim(chest, aframe_ids::ChestOpen);
    SpawnChestSparkles(emit_pos, state);
    (void)PlayWorldSoundEmitter(state, emit_pos, audio_asset_ids::ChestOpen);

    if (state.drng.RandomIntInclusive(1, kChestTrapOdds) == 1) {
        SpawnChestTrapBomb(emit_pos, state);
        (void)PlayWorldSoundEmitter(state, emit_pos, audio_asset_ids::Throw);
        return true;
    }

    SpawnChestTreasure(emit_pos, state, opener_vid);
    return true;
}

bool TryOpenTreasureChest(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::optional<VID> opener_vid = std::nullopt
) {
    if (ent_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& chest = state.ents.ents[ent_idx];
    const Vec2 emit_pos = common::GetEmitPointForEnt(chest, graphics, chest.GetRenderCenter());
    return TryOpenTreasureChestAt(ent_idx, emit_pos, state, audio, opener_vid);
}

EntDamageEffectResult OnDamageEffectAsChest(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
) {
    (void)amount;
    if (damage_applied) {
        return EntDamageEffectResult::None;
    }
    if (damage_type != DamageType::Attack &&
        damage_type != DamageType::IgnitingAttack &&
        damage_type != DamageType::HeavyAttack) {
        return EntDamageEffectResult::None;
    }

    const Ent& chest = state.ents.ents[ent_idx];
    if (!TryOpenTreasureChestAt(
            ent_idx,
            chest.GetRenderCenter(),
            state,
            audio,
            FindNearestPlayerVid(state, chest.GetSimCenter(), false))) {
        return EntDamageEffectResult::None;
    }
    return EntDamageEffectResult::Consumed;
}

bool TryOpenKeyChestWithKey(
    std::size_t ent_idx,
    VID key_vid,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent* holder = nullptr;
    Ent* key = nullptr;
    Ent& chest = state.ents.ents[ent_idx];
    if (!chest.active || chest.condition == EntCondition::Dead ||
        IsOpenWithAnim(chest, aframe_ids::KeyChestOpen) ||
        !CanUnlockKeyChestFromHeldKey(ent_idx, state, graphics, key_vid, &holder, &key)) {
        return false;
    }

    SetAnim(chest, aframe_ids::KeyChestOpen);
    const Vec2 emit_pos = common::GetEmitPointForEnt(chest, graphics, chest.GetRenderCenter());
    SpawnChestSparkles(emit_pos, state);
    (void)PlayWorldSoundEmitter(state, emit_pos, audio_asset_ids::Unlock);
    (void)PlayWorldSoundEmitter(state, emit_pos, audio_asset_ids::ChestOpen);
    (void)world_ops::SpawnEnt(state, EntType::UdjatEye, [&](Ent& udjat_eye) {
        udjat_eye.SetRenderCenter(emit_pos);
        udjat_eye.vel = sim::Vec2::zero();
        udjat_eye.acc = sim::Vec2::zero();
        LaunchChestLoot(
            state,
            udjat_eye,
            holder != nullptr ? std::optional<VID>(holder->vid)
                              : FindNearestPlayerVid(state, chest.GetSimCenter(), false));
    });
    if (holder != nullptr && key != nullptr) {
        ConsumeHeldChestKey(holder, *key, state, graphics);
    }
    return true;
}

} // namespace


void OnUseAsChest(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio) {
    const Ent& chest = state.ents.ents[ent_idx];
    if (!chest.use_state.pressed || chest.use_state.source == AttachMode::Held) {
        return;
    }

    TryOpenTreasureChest(ent_idx, state, graphics, audio, chest.use_state.user_vid);
}

bool OnInteractAsChest(
    std::size_t ent_idx,
    std::size_t interactor_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    if (interactor_idx >= state.ents.ents.size()) {
        return false;
    }
    const Ent& interactor = state.ents.ents[interactor_idx];
    if (interactor.condition == EntCondition::Dead ||
        !IsEntOverlappingChest(ent_idx, interactor, state, graphics)) {
        return false;
    }
    return TryOpenTreasureChest(ent_idx, state, graphics, audio, interactor.vid);
}

bool OnInteractAsKeyChest(
    std::size_t ent_idx,
    std::size_t interactor_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    if (interactor_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& key = state.ents.ents[interactor_idx];
    if (!key.active || key.type_ != EntType::ChestKey || !key.held_by_vid.has_value()) {
        return false;
    }
    const Ent* const holder = state.ents.GetEnt(*key.held_by_vid);
    if (holder == nullptr || !holder->active || holder->condition == EntCondition::Dead) {
        return false;
    }

    return TryOpenKeyChestWithKey(ent_idx, key.vid, state, graphics, audio);
}

void StepEntLogicAsChest(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!ShouldSimulatePlayerSlotGameplay(state, slot)) {
            continue;
        }

        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || player->condition != EntCondition::Normal ||
            !IsEntOverlappingChest(ent_idx, *player, state, graphics)) {
            continue;
        }

        const controls::ControlIntent control = controls::GetControlIntentForEnt(*player, state);
        if (!control.use_pressed) {
            continue;
        }

        (void)world_ops::TryApplyInteractEnt(
            player->vid,
            state.ents.ents[ent_idx].vid,
            state,
            graphics,
            audio
        );
        return;
    }
}

void StepEntLogicAsKeyChest(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }
    Ent& chest = state.ents.ents[ent_idx];
    if (!chest.active || chest.condition == EntCondition::Dead ||
        IsOpenWithAnim(chest, aframe_ids::KeyChestOpen)) {
        return;
    }

    for (const PlayerSlot& slot : state.players.slots) {
        if (!ShouldSimulatePlayerSlotGameplay(state, slot)) {
            continue;
        }

        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || player->condition == EntCondition::Dead ||
            !player->holding_vid.has_value()) {
            continue;
        }

        const Ent* const key = state.ents.GetEnt(*player->holding_vid);
        if (key == nullptr || key->type_ != EntType::ChestKey ||
            key->held_by_vid != player->vid) {
            continue;
        }

        Ent* holder = nullptr;
        Ent* held_key = nullptr;
        if (!CanUnlockKeyChestFromHeldKey(
                ent_idx,
                state,
                graphics,
                key->vid,
                &holder,
                &held_key) ||
            holder == nullptr || held_key == nullptr || holder->vid != player->vid) {
            continue;
        }

        (void)world_ops::TryApplyInteractEnt(
            key->vid,
            chest.vid,
            state,
            graphics,
            audio
        );
        return;
    }
}

extern const EntSpec kChestSpec{
    .type_ = EntType::Chest,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Background,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .proj_contact_damage_amount = 1,
    .can_apply_proj_contact = true,
    .on_damage = OnDamageEffectAsChest,
    .on_use = OnUseAsChest,
    .on_interact = OnInteractAsChest,
    .step_logic = StepEntLogicAsChest,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Chest),
};

extern const EntSpec kKeyChestSpec{
    .type_ = EntType::KeyChest,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Background,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .proj_contact_damage_amount = 1,
    .can_apply_proj_contact = true,
    .on_interact = OnInteractAsKeyChest,
    .step_logic = StepEntLogicAsKeyChest,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::KeyChest),
};

extern const EntSpec kChestKeySpec{
    .type_ = EntType::ChestKey,
    .size = EntSpecSize(8.0F, 4.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = 1,
    .can_apply_proj_contact = true,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::ChestKey),
};

extern const EntSpec kUdjatEyeSpec{
    .type_ = EntType::UdjatEye,
    .size = EntSpecSize(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .pickup_effect = EffectId::UdjatEye,
    .on_ent_contact = OnEntContactAsUdjatEye,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::UdjatEye),
};

} // namespace splonks::ents::chest
