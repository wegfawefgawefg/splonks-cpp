#include "entities/common.hpp"
#include "entities/mod.hpp"

#include "on_damage_effects.hpp"
#include "terrain_lighting.hpp"
#include "tile.hpp"

#include <vector>

namespace splonks::entities::common {

namespace {

constexpr std::uint32_t kHarmContactCooldownFrames = 8;

bool AabbsIntersect(const AABB& left, const AABB& right) {
    if (left.br.x < right.tl.x) {
        return false;
    }
    if (left.tl.x > right.br.x) {
        return false;
    }
    if (left.br.y < right.tl.y) {
        return false;
    }
    if (left.tl.y > right.br.y) {
        return false;
    }
    return true;
}

void SpawnEntityAtCenter(EntityType type_, const Vec2& center, State& state) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    SetEntityAs(*entity, type_);
    entity->SetCenter(center);
    entity->vel = Vec2::New(0.0F, 0.0F);
}

void SpawnGoldVeinPayout(Tile tile, const IVec2& tile_pos, State& state) {
    const Vec2 tile_tl = Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize)),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize))
    );
    const Vec2 center = tile_tl + Vec2::New(8.0F, 8.0F);

    if (tile == Tile::Gold) {
        SpawnEntityAtCenter(EntityType::Gold, center + Vec2::New(-3.0F, -1.0F), state);
        SpawnEntityAtCenter(EntityType::Gold, center + Vec2::New(0.0F, 0.0F), state);
        SpawnEntityAtCenter(EntityType::Gold, center + Vec2::New(3.0F, 1.0F), state);
        return;
    }

    if (tile == Tile::GoldBig) {
        SpawnEntityAtCenter(EntityType::Gold, center + Vec2::New(-4.0F, -1.0F), state);
        SpawnEntityAtCenter(EntityType::Gold, center + Vec2::New(0.0F, 1.0F), state);
        SpawnEntityAtCenter(EntityType::Gold, center + Vec2::New(4.0F, -1.0F), state);
        SpawnEntityAtCenter(EntityType::GoldStack, center, state);
    }
}

void BreakStageTilesInRectWc(const AABB& area, State& state) {
    const int max_x = static_cast<int>(state.stage.GetTileWidth()) - 1;
    const int max_y = static_cast<int>(state.stage.GetTileHeight()) - 1;
    const IVec2 tl = ToIVec2(area.tl / static_cast<float>(kTileSize));
    const IVec2 br = ToIVec2(area.br / static_cast<float>(kTileSize));

    for (int y = std::max(0, tl.y); y <= std::min(br.y, max_y); ++y) {
        for (int x = std::max(0, tl.x); x <= std::min(br.x, max_x); ++x) {
            const IVec2 tile_pos = IVec2::New(x, y);
            const Tile tile = state.stage.GetTile(static_cast<unsigned int>(x), static_cast<unsigned int>(y));
            if (tile == Tile::Exit) {
                continue;
            }

            if (tile == Tile::Gold || tile == Tile::GoldBig) {
                SpawnGoldVeinPayout(tile, tile_pos, state);
            }

            const EntityType embedded_treasure = state.stage.TakeEmbeddedTreasure(tile_pos);
            if (embedded_treasure != EntityType::None) {
                const Vec2 center = Vec2::New(
                    static_cast<float>(x * static_cast<int>(kTileSize) + 8),
                    static_cast<float>(y * static_cast<int>(kTileSize) + 8)
                );
                SpawnEntityAtCenter(embedded_treasure, center, state);
            }

            state.stage.SetTile(tile_pos, Tile::Air);
        }
    }
}

void CrushIfCanBeCrushed(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.super_state == EntitySuperState::Crushed) {
        entity.health = 0;
        entity.marked_for_destruction = true;
        std::optional<SoundEffect> sound_effect;
        switch (entity.type_) {
        case EntityType::Player:
            sound_effect = SoundEffect::AnimalCrush1;
            break;
        case EntityType::Bat:
            sound_effect = SoundEffect::AnimalCrush2;
            break;
        case EntityType::Gold:
        case EntityType::GoldStack:
            sound_effect = SoundEffect::MoneySmashed;
            break;
        case EntityType::Rock:
            sound_effect = SoundEffect::Thud;
            break;
        default:
            break;
        }
        if (sound_effect.has_value()) {
            audio.PlaySoundEffect(*sound_effect);
        }
    }
}

void DieIfDead(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.health == 0) {
        entity.super_state = EntitySuperState::Dead;
        entity.state = EntityState::Dead;
        if (!entity.marked_for_destruction) {
            TrySetAnimation(entity, EntityDisplayState::Dead);
        }
    }
    if (entity.super_state == EntitySuperState::Dead) {
        std::optional<SoundEffect> sound_effect;
        if (entity.stone) {
            sound_effect = SoundEffect::PotShatter;
        } else {
            switch (entity.type_) {
            case EntityType::Bomb:
            case EntityType::JetPack:
                sound_effect = SoundEffect::BombExplosion;
                break;
            case EntityType::Pot:
                sound_effect = SoundEffect::PotShatter;
                break;
            case EntityType::Box:
                sound_effect = SoundEffect::BoxBreak;
                break;
            default:
                break;
            }
        }
        if (sound_effect.has_value()) {
            audio.PlaySoundEffect(*sound_effect);
            entity.super_state = EntitySuperState::Dead;
        }
    }
}

void MaybeHurtAndStunOnContact(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const VID entity_vid = entity.vid;
    const AABB entity_aabb = GetContactAabbForEntity(entity, graphics);
    const Vec2 entity_pos = entity.pos;
    const EntitySuperState super_state = entity.super_state;
    const bool hurt_on_contact = entity.hurt_on_contact;
    const std::optional<VID> thrown_by = entity.thrown_by;
    const Alignment alignment = entity.alignment;
    if (super_state != EntitySuperState::Dead && super_state != EntitySuperState::Stunned &&
        hurt_on_contact) {
        const std::vector<VID> search_results =
            state.sid.QueryExclude(entity_aabb.tl, entity_aabb.br, entity_vid);
        std::vector<VID> results;
        for (const VID& vid : search_results) {
            const Entity& e = state.entity_manager.entities[vid.id];
            if (!e.impassable) {
                results.push_back(vid);
            }
        }
        for (const VID& vid : results) {
            if (thrown_by.has_value() && vid == *thrown_by) {
                continue;
            }
            if (Entity* const other_entity = state.entity_manager.GetEntityMut(vid)) {
                const AABB other_aabb = GetContactAabbForEntity(*other_entity, graphics);
                if (!AabbsIntersect(entity_aabb, other_aabb)) {
                    continue;
                }
                if (other_entity->type_ == EntityType::Player) {
                    const AABB player_aabb = other_aabb;
                    const AABB player_foot = {
                        .tl = Vec2::New(player_aabb.tl.x, player_aabb.br.y - 4.0F),
                        .br = player_aabb.br,
                    };
                    if (entity_pos.x >= player_foot.tl.x && entity_pos.x <= player_foot.br.x &&
                        entity_pos.y >= player_foot.tl.y && entity_pos.y <= player_foot.br.y) {
                        continue;
                    }
                }
                if (other_entity->can_collide) {
                    const bool has_correct_alignment =
                        (alignment == Alignment::Ally && other_entity->alignment == Alignment::Enemy) ||
                        (alignment == Alignment::Enemy &&
                         other_entity->alignment == Alignment::Ally) ||
                        (alignment == Alignment::Neutral);
                    if (has_correct_alignment) {
                        if (state.HasInteractionCooldown(
                                entity.vid,
                                other_entity->vid,
                                InteractionCooldownKind::Harm
                            )) {
                            continue;
                        }
                        const DamageResult damage_result =
                            TryToDamageEntity(other_entity->vid.id, state, audio, DamageType::Attack, 1);
                        switch (damage_result) {
                        case DamageResult::Died:
                        case DamageResult::Hurt:
                            state.AddInteractionCooldown(
                                entity.vid,
                                other_entity->vid,
                                InteractionCooldownKind::Harm,
                                kHarmContactCooldownFrames
                            );
                            break;
                        case DamageResult::None:
                            break;
                        }
                    }
                }
            }
        }
    }
}

SoundEffect GetProjectileCollisionSound(EntityType type_) {
    switch (type_) {
    case EntityType::Pot:
        return SoundEffect::PotShatter;
    default:
        return SoundEffect::Thud;
    }
}

bool MaybeHurtAndStunOnContactAsProjectile(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (state.stage_frame < kStageSettleFrames) {
        return false;
    }
    bool hit = false;
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const VID entity_vid = entity.vid;
    const AABB entity_aabb = GetContactAabbForEntity(entity, graphics);
    const EntityType entity_type = entity.type_;
    const std::optional<VID> thrown_by = entity.thrown_by;
    const Vec2 entity_vel = entity.vel;

    if (Length(entity_vel) < 1.0F) {
        return false;
    }
    const std::vector<VID> search_results =
        state.sid.QueryExclude(entity_aabb.tl, entity_aabb.br, entity_vid);
    std::vector<VID> results;
    for (const VID& vid : search_results) {
        const Entity& e = state.entity_manager.entities[vid.id];
        if (!e.impassable) {
            results.push_back(vid);
        }
    }
    for (const VID& vid : results) {
        if (thrown_by.has_value() && vid == *thrown_by) {
            continue;
        }
        if (Entity* const other_entity = state.entity_manager.GetEntityMut(vid)) {
            const AABB other_aabb = GetContactAabbForEntity(*other_entity, graphics);
            if (!AabbsIntersect(entity_aabb, other_aabb)) {
                continue;
            }
            if (other_entity->can_collide) {
                if (state.HasInteractionCooldown(
                        entity.vid,
                        other_entity->vid,
                        InteractionCooldownKind::Harm
                    )) {
                    continue;
                }
                hit = true;
                const DamageResult damage_result =
                    TryToDamageEntity(other_entity->vid.id, state, audio, DamageType::Attack, 1);
                switch (damage_result) {
                case DamageResult::Hurt:
                case DamageResult::Died: {
                    state.AddInteractionCooldown(
                        entity.vid,
                        other_entity->vid,
                        InteractionCooldownKind::Harm,
                        kHarmContactCooldownFrames
                    );
                    const SoundEffect sound_effect = GetProjectileCollisionSound(entity_type);
                    audio.PlaySoundEffect(sound_effect);
                    break;
                }
                case DamageResult::None:
                    break;
                }
            }
        }
    }
    return hit;
}

void MaybeHurtAndStunAsOnContactHurtfulEntityBodyOrProjectile(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    MaybeHurtAndStunOnContact(entity_idx, state, graphics, audio);
    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.super_state == EntitySuperState::Dead ||
        entity.super_state == EntitySuperState::Stunned) {
        MaybeHurtAndStunOnContactAsProjectile(entity_idx, state, graphics, audio);
    }
}

void ApplyHurtOnContact(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.hurt_on_contact) {
        MaybeHurtAndStunAsOnContactHurtfulEntityBodyOrProjectile(entity_idx, state, graphics, audio);
    } else if (entity.alignment == Alignment::Ally) {
    } else {
        MaybeHurtAndStunOnContactAsProjectile(entity_idx, state, graphics, audio);
    }
}

void DieIfFootInSpikes(std::size_t entity_idx, State& state, Audio& audio) {
    Entity& entity = state.entity_manager.entities[entity_idx];
    bool fell_into_spikes = false;
    if (entity.vel.y > 0.0F) {
        const IAABB iaabb = entity.GetAABB().AsIAABB();
        const bool override_tile_portion_check = entity.vel.y > 4.0F;
        const bool in_top_portion_of_tile = (iaabb.br.y % static_cast<int>(kTileSize)) < 4;
        if (in_top_portion_of_tile || override_tile_portion_check) {
            const std::vector<const Tile*> tiles_in_body = state.stage.GetTilesInRectWc(iaabb.tl, iaabb.br);
            for (const Tile* tile : tiles_in_body) {
                if (*tile == Tile::Spikes) {
                    fell_into_spikes = true;
                }
            }
        }
    }
    if (fell_into_spikes) {
        const DamageResult damage_result =
            TryToDamageEntity(entity.vid.id, state, audio, DamageType::Spikes, 1);
        switch (damage_result) {
        case DamageResult::Hurt:
        case DamageResult::Died:
            entity.vel.x = 0.0F;
            audio.PlaySoundEffect(SoundEffect::AnimalCrush2);
            break;
        case DamageResult::None:
            break;
        }
    }
}

void DoDamagedEffect(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    Entity& entity = state.entity_manager.entities[entity_idx];
    if (entity.last_health > entity.health) {
        switch (entity.type_) {
        case EntityType::Box:
        case EntityType::Pot:
            OnDamageEffectAsBreakawayContainer(entity_idx, state);
            break;
        case EntityType::Player:
        case EntityType::Bat:
            OnDamageEffectAsBleedingEntity(entity_idx, state);
            break;
        case EntityType::JetPack:
        case EntityType::Bomb:
            OnDeathEffectAsExplosive(entity_idx, state);
            break;
        default:
            break;
        }
    }
    entity.last_health = entity.health;
}

void DoSuperStateChangedEffect(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    Entity& entity = state.entity_manager.entities[entity_idx];

    if (entity.last_super_state != EntitySuperState::Dead &&
        entity.super_state == EntitySuperState::Dead) {
        switch (entity.type_) {
        case EntityType::JetPack:
        case EntityType::Bomb:
            OnDeathEffectAsExplosive(entity_idx, state);
            break;
        default:
            break;
        }
    }

    entity.last_super_state = entity.super_state;
}

} // namespace

void CommonStep(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    CrushIfCanBeCrushed(entity_idx, state, audio);
    DieIfDead(entity_idx, state, audio);
    StepStunTimer(entity_idx, state);
    {
        Entity& entity = state.entity_manager.entities[entity_idx];
        if (entity.hang_count > 0) {
            entity.hang_count -= 1;
        }
    }
    DoThrownByStep(entity_idx, state);
    ApplyHurtOnContact(entity_idx, state, graphics, audio);
    DieIfFootInSpikes(entity_idx, state, audio);
    DoDamagedEffect(entity_idx, state, audio);
    DoSuperStateChangedEffect(entity_idx, state, audio);
    (void)dt;
}

DamageResult TryToDamageEntity(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount
) {
    (void)audio;
    Entity& entity = state.entity_manager.entities[entity_idx];
    const DamageVulnerability v = entity.damage_vulnerability;
    if (v == DamageVulnerability::Immune) {
        return DamageResult::None;
    }
    bool can_damage = false;
    switch (entity.damage_vulnerability) {
    case DamageVulnerability::AttackingOnly:
        can_damage = damage_type == DamageType::Attack;
        break;
    case DamageVulnerability::BurningOnly:
        can_damage = damage_type == DamageType::Burn;
        break;
    case DamageVulnerability::CrushingOnly:
        can_damage = damage_type == DamageType::Crush;
        break;
    case DamageVulnerability::ExplosionOnly:
        can_damage = damage_type == DamageType::Explosion;
        break;
    case DamageVulnerability::CrushingAndSpikes:
        can_damage = damage_type == DamageType::Crush || damage_type == DamageType::Spikes;
        break;
    case DamageVulnerability::CrushingSpikesAndExplosion:
        can_damage = damage_type == DamageType::Crush || damage_type == DamageType::Spikes ||
                     damage_type == DamageType::Explosion;
        break;
    case DamageVulnerability::HeavyAttackOnly:
        can_damage = damage_type == DamageType::HeavyAttack;
        break;
    case DamageVulnerability::Vulnerable:
        can_damage = true;
        break;
    case DamageVulnerability::Immune:
        can_damage = false;
        break;
    case DamageVulnerability::AnthingExceptJumpOn:
        can_damage = damage_type != DamageType::JumpOn;
        break;
    }
    if (can_damage) {
        if (entity.stone && damage_type == DamageType::Explosion) {
            entity.health = 0;
            return DamageResult::Died;
        }
        bool do_damage_calculation = false;
        if (damage_type == DamageType::Crush) {
            entity.health = 0;
            entity.super_state = EntitySuperState::Crushed;
            return DamageResult::Died;
        }
        if (entity.super_state == EntitySuperState::Dead) {
            return DamageResult::None;
        } else {
            if (damage_type == DamageType::Spikes) {
                entity.health = 0;
                entity.super_state = EntitySuperState::Dead;
                return DamageResult::Died;
            } else if (damage_type == DamageType::Explosion) {
                do_damage_calculation = true;
                if (entity.can_be_stunned && entity.super_state != EntitySuperState::Stunned) {
                    entity.super_state = EntitySuperState::Stunned;
                    TrySetAnimation(entity, EntityDisplayState::Stunned);
                    entity.stun_timer = kDefaultStunTimer;
                }
            } else if (entity.can_be_stunned) {
                if (entity.super_state != EntitySuperState::Stunned) {
                    entity.super_state = EntitySuperState::Stunned;
                    TrySetAnimation(entity, EntityDisplayState::Stunned);
                    entity.stun_timer = kDefaultStunTimer;
                    do_damage_calculation = true;
                } else {
                    return DamageResult::None;
                }
            } else {
                do_damage_calculation = true;
            }
        }
        if (do_damage_calculation) {
            if (entity.health > amount) {
                entity.health -= amount;
                return DamageResult::Hurt;
            }
            entity.health = 0;
            return DamageResult::Died;
        }
    }
    return DamageResult::None;
}

void DoExplosion(
    std::size_t entity_idx,
    Vec2 center,
    float size,
    State& state,
    Audio& audio
) {
    audio.PlaySoundEffect(SoundEffect::BombExplosion);
    const float explosion_size = size * static_cast<float>(kTileSize);
    const AABB area = {
        .tl = center - (Vec2::New(1.0F, 1.0F) * explosion_size),
        .br = center + (Vec2::New(1.0F, 1.0F) * explosion_size),
    };
    BreakStageTilesInRectWc(area, state);
    InvalidateTerrainLightingCache(state);

    const VID this_vid = state.entity_manager.GetVid(entity_idx);
    const std::vector<VID> results = state.sid.QueryExclude(area.tl, area.br, this_vid);
    for (const VID& vid : results) {
        bool impassable = false;
        if (Entity* const entity = state.entity_manager.GetEntityMut(vid)) {
            impassable = entity->impassable;
            TryToDamageEntity(vid.id, state, audio, DamageType::Explosion, 10);
        }
        if (impassable) {
            state.entity_manager.SetInactive(vid.id);
        }
    }
}

} // namespace splonks::entities::common
