#include "entities/common/common.hpp"

#include "world_query.hpp"

#include <vector>

namespace splonks::entities::common {

namespace {

constexpr std::uint32_t kHarmContactCooldownFrames = 8;

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
    const EntityCondition condition = entity.condition;
    const bool hurt_on_contact = entity.hurt_on_contact;
    const std::optional<VID> thrown_by = entity.thrown_by;
    const Alignment alignment = entity.alignment;
    if (condition == EntityCondition::Normal && hurt_on_contact) {
        const std::vector<VID> search_results =
            QueryEntitiesInAabb(state, entity_aabb, entity_vid);
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
                const AABB other_aabb = GetNearestWorldAabb(
                    state.stage,
                    entity.GetCenter(),
                    GetContactAabbForEntity(*other_entity, graphics)
                );
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
                        if (state.contact.HasInteractionCooldown(
                                entity.vid,
                                other_entity->vid,
                                InteractionCooldownKind::Harm
                            )) {
                            continue;
                        }
                        const DamageResult damage_result =
                            TryDamageEntity(other_entity->vid.id, state, audio, DamageType::Attack, 1);
                        switch (damage_result) {
                        case DamageResult::Died:
                        case DamageResult::Hurt:
                            state.contact.AddInteractionCooldown(
                                entity.vid,
                                other_entity->vid,
                                InteractionCooldownKind::Harm,
                                state.stage_frame,
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
    const std::optional<VID> thrown_by = entity.thrown_by;
    const Vec2 entity_vel = entity.vel;

    if (Length(entity_vel) < 1.0F) {
        return false;
    }
    const std::vector<VID> search_results =
        QueryEntitiesInAabb(state, entity_aabb, entity_vid);
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
            const AABB other_aabb = GetNearestWorldAabb(
                state.stage,
                entity.GetCenter(),
                GetContactAabbForEntity(*other_entity, graphics)
            );
            if (!AabbsIntersect(entity_aabb, other_aabb)) {
                continue;
            }
            if (other_entity->can_collide) {
                if (state.contact.HasInteractionCooldown(
                        entity.vid,
                        other_entity->vid,
                        InteractionCooldownKind::Harm
                    )) {
                    continue;
                }
                hit = true;
                const DamageResult damage_result =
                    TryDamageEntity(other_entity->vid.id, state, audio, DamageType::Attack, 1);
                switch (damage_result) {
                case DamageResult::Hurt:
                case DamageResult::Died: {
                    state.contact.AddInteractionCooldown(
                        entity.vid,
                        other_entity->vid,
                        InteractionCooldownKind::Harm,
                        state.stage_frame,
                        kHarmContactCooldownFrames
                    );
                    if (entity.collide_sound.has_value()) {
                        audio.PlaySoundEffect(*entity.collide_sound);
                    }
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
    if (entity.condition != EntityCondition::Normal) {
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
            for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(state.stage, iaabb.tl, iaabb.br)) {
                if (tile_query.tile != nullptr && *tile_query.tile == Tile::Spikes) {
                    fell_into_spikes = true;
                }
            }
        }
    }
    if (fell_into_spikes) {
        const DamageResult damage_result =
            TryDamageEntity(entity.vid.id, state, audio, DamageType::Spikes, 1);
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

} // namespace

void CommonStep(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
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
    (void)dt;
}

} // namespace splonks::entities::common
