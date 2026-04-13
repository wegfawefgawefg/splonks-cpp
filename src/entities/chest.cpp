#include "entities/chest.hpp"

#include "audio.hpp"
#include "controls.hpp"
#include "entities/common/common.hpp"
#include "entity/archetype.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "math_types.hpp"
#include "particles/ultra_dynamic_particle.hpp"
#include "state.hpp"
#include "utils.hpp"

#include <memory>
#include <optional>

namespace splonks::entities::chest {

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

bool IsOpenWithAnimation(const Entity& entity, FrameDataId animation_id) {
    return entity.frame_data_animator.animation_id == animation_id;
}

void LaunchChestLoot(Entity& entity) {
    entity.vel = Vec2::New(
        static_cast<float>(rng::RandomIntInclusive(0, 3) - rng::RandomIntInclusive(0, 3)),
        kChestLootLaunchY
    );
}

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

EntityType RandomChestGemType() {
    switch (rng::RandomIntInclusive(1, 3)) {
    case 1:
        return EntityType::EmeraldBig;
    case 2:
        return EntityType::SapphireBig;
    case 3:
        return EntityType::RubyBig;
    }

    return EntityType::EmeraldBig;
}

void SpawnChestSparkles(const Vec2& emit_pos, State& state) {
    const int count = rng::RandomIntInclusive(5, 7);
    for (int i = 0; i < count; ++i) {
        auto sparkle = std::make_unique<UltraDynamicParticle>();
        sparkle->frame_data_animator = FrameDataAnimator::New(frame_data_ids::Sparkle);
        sparkle->draw_layer = DrawLayer::Foreground;
        sparkle->counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(16, 28));
        sparkle->pos = emit_pos + Vec2::New(
            rng::RandomFloat(-2.0F, 2.0F),
            rng::RandomFloat(-1.0F, 1.0F)
        );
        sparkle->size = Vec2::New(rng::RandomFloat(5.0F, 8.0F), rng::RandomFloat(5.0F, 8.0F));
        sparkle->rot = rng::RandomFloat(0.0F, 360.0F);
        sparkle->alpha = rng::RandomFloat(0.75F, 1.0F);
        sparkle->vel = Vec2::New(
            rng::RandomFloat(-0.6F, 0.6F),
            rng::RandomFloat(-1.2F, -0.45F)
        );
        sparkle->svel = Vec2::New(-0.03F, -0.03F);
        sparkle->rotvel = rng::RandomFloat(-5.0F, 5.0F);
        sparkle->alpha_vel = kChestSparkleAlphaVel;
        sparkle->acc = Vec2::New(0.0F, kChestSparkleGravity);
        sparkle->sacc = Vec2::New(-0.003F, -0.003F);
        sparkle->alpha_acc = kChestSparkleAlphaAcc;
        state.particles.Add(std::move(sparkle));
    }
}

void SpawnChestTrapBomb(const Vec2& spawn_center, State& state) {
    Entity* const bomb = SpawnEntityAtCenter(EntityType::Bomb, spawn_center, state);
    if (bomb == nullptr) {
        return;
    }

    bomb->counter_a = kChestTrapFuseFrames;
    bomb->vel = Vec2::New(
        static_cast<float>(rng::RandomIntInclusive(0, 3) - rng::RandomIntInclusive(0, 3)),
        kChestLootLaunchY
    );
    SetAnimation(*bomb, frame_data_ids::LiveGrenade);
}

void SpawnChestTreasure(const Vec2& spawn_center, State& state) {
    const int gem_count = rng::RandomIntInclusive(kChestTreasureDropMin, kChestTreasureDropMax);
    for (int i = 0; i < gem_count; ++i) {
        Entity* const gem = SpawnEntityAtCenter(RandomChestGemType(), spawn_center, state);
        if (gem == nullptr) {
            return;
        }
        LaunchChestLoot(*gem);
    }

    if (rng::RandomIntInclusive(1, kChestBonusGemOdds) != 1) {
        return;
    }

    Entity* const gem = SpawnEntityAtCenter(RandomChestGemType(), spawn_center, state);
    if (gem == nullptr) {
        return;
    }
    LaunchChestLoot(*gem);
}

bool CanPlayerUseChestFromContact(
    std::size_t chest_idx,
    const State& state,
    const Graphics& graphics
) {
    if (!state.player_vid.has_value()) {
        return false;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition != EntityCondition::Normal) {
        return false;
    }

    const controls::ControlIntent control = controls::GetControlIntentForEntity(*player, state);
    if (!control.use_pressed) {
        return false;
    }

    const Entity& chest = state.entity_manager.entities[chest_idx];
    return AabbsIntersect(
        common::GetContactAabbForEntity(*player, graphics),
        common::GetContactAabbForEntity(chest, graphics)
    );
}

void ConsumeHeldChestKey(Entity* holder, Entity& key, State& state, const Graphics& graphics) {
    if (holder != nullptr && holder->holding_vid == key.vid) {
        holder->holding_vid.reset();
        holder->holding = false;
        holder->holding_timer = kDefaultHoldingTimer;
    }
    key.held_by_vid.reset();
    key.attachment_mode = AttachmentMode::None;
    StopUsingEntity(key);
    state.entity_manager.SetInactive(key.vid.id);
    state.UpdateSidForEntity(key.vid.id, graphics);
}

bool CanUnlockKeyChestFromHeldKey(
    std::size_t chest_idx,
    State& state,
    const Graphics& graphics,
    Entity** holder_out,
    Entity** key_out
) {
    if (holder_out != nullptr) {
        *holder_out = nullptr;
    }
    if (key_out != nullptr) {
        *key_out = nullptr;
    }
    if (chest_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& chest = state.entity_manager.entities[chest_idx];
    const AABB chest_aabb = common::GetContactAabbForEntity(chest, graphics);
    const std::vector<VID> touched = common::GatherTouchedEntityContactsForAabb(
        chest_idx,
        chest_aabb,
        graphics,
        state
    );
    for (const VID vid : touched) {
        Entity* const key = state.entity_manager.GetEntityMut(vid);
        if (key == nullptr || !key->active || key->type_ != EntityType::ChestKey ||
            !key->held_by_vid.has_value()) {
            continue;
        }

        if (holder_out != nullptr) {
            *holder_out = state.entity_manager.GetEntityMut(*key->held_by_vid);
        }
        if (key_out != nullptr) {
            *key_out = key;
        }
        return true;
    }
    return false;
}

bool TryOpenTreasureChestAt(
    std::size_t entity_idx,
    const Vec2& emit_pos,
    State& state,
    Audio& audio
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& chest = state.entity_manager.entities[entity_idx];
    if (!chest.active || chest.condition == EntityCondition::Dead ||
        IsOpenWithAnimation(chest, frame_data_ids::ChestOpen)) {
        return false;
    }

    SetAnimation(chest, frame_data_ids::ChestOpen);
    SpawnChestSparkles(emit_pos, state);

    if (rng::RandomIntInclusive(1, kChestTrapOdds) == 1) {
        SpawnChestTrapBomb(emit_pos, state);
        audio.PlaySoundEffect(SoundEffect::Throw);
        return true;
    }

    SpawnChestTreasure(emit_pos, state);
    audio.PlaySoundEffect(SoundEffect::GoldStack);
    return true;
}

bool TryOpenTreasureChest(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& chest = state.entity_manager.entities[entity_idx];
    const Vec2 emit_pos = common::GetEmitPointForEntity(chest, graphics, chest.GetCenter());
    return TryOpenTreasureChestAt(entity_idx, emit_pos, state, audio);
}

EntityDamageEffectResult OnDamageEffectAsChest(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
) {
    (void)amount;
    if (damage_applied) {
        return EntityDamageEffectResult::None;
    }
    if (damage_type != DamageType::Attack && damage_type != DamageType::HeavyAttack) {
        return EntityDamageEffectResult::None;
    }

    const Entity& chest = state.entity_manager.entities[entity_idx];
    if (!TryOpenTreasureChestAt(entity_idx, chest.GetCenter(), state, audio)) {
        return EntityDamageEffectResult::None;
    }
    return EntityDamageEffectResult::Consumed;
}

bool TryOpenKeyChest(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity* holder = nullptr;
    Entity* key = nullptr;
    Entity& chest = state.entity_manager.entities[entity_idx];
    if (!chest.active || chest.condition == EntityCondition::Dead ||
        IsOpenWithAnimation(chest, frame_data_ids::KeyChestOpen) ||
        !CanUnlockKeyChestFromHeldKey(entity_idx, state, graphics, &holder, &key)) {
        return false;
    }

    SetAnimation(chest, frame_data_ids::KeyChestOpen);
    const Vec2 emit_pos = common::GetEmitPointForEntity(chest, graphics, chest.GetCenter());
    SpawnChestSparkles(emit_pos, state);
    Entity* const udjat_eye = SpawnEntityAtCenter(EntityType::UdjatEye, emit_pos, state);
    if (udjat_eye != nullptr) {
        LaunchChestLoot(*udjat_eye);
    }
    if (holder != nullptr && key != nullptr) {
        ConsumeHeldChestKey(holder, *key, state, graphics);
    }
    audio.PlaySoundEffect(SoundEffect::GoldStack);
    return true;
}

} // namespace


void OnUseAsChest(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    const Entity& chest = state.entity_manager.entities[entity_idx];
    if (!chest.use_state.pressed || chest.use_state.source == AttachmentMode::Held) {
        return;
    }

    TryOpenTreasureChest(entity_idx, state, graphics, audio);
}

void StepEntityLogicAsChest(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (!CanPlayerUseChestFromContact(entity_idx, state, graphics)) {
        return;
    }

    TryOpenTreasureChest(entity_idx, state, graphics, audio);
}

void StepEntityLogicAsKeyChest(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    TryOpenKeyChest(entity_idx, state, graphics, audio);
}

extern const EntityArchetype kChestArchetype{
    .type_ = EntityType::Chest,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .on_damage_effect = OnDamageEffectAsChest,
    .on_use = OnUseAsChest,
    .step_logic = StepEntityLogicAsChest,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Chest),
};

extern const EntityArchetype kKeyChestArchetype{
    .type_ = EntityType::KeyChest,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .step_logic = StepEntityLogicAsKeyChest,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::KeyChest),
};

extern const EntityArchetype kChestKeyArchetype{
    .type_ = EntityType::ChestKey,
    .size = Vec2::New(8.0F, 4.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::ChestKey),
};

extern const EntityArchetype kUdjatEyeArchetype{
    .type_ = EntityType::UdjatEye,
    .size = Vec2::New(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .passive_item = EntityPassiveItem::UdjatEye,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::UdjatEye),
};

} // namespace splonks::entities::chest
