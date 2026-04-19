#include "entities/common/common.hpp"

namespace splonks::entities::common {

namespace {

unsigned int GetPickupMoneyValue(EntityType type_) {
    switch (type_) {
    case EntityType::Gold:
        return 1;
    case EntityType::GoldStack:
        return 2;
    case EntityType::GoldChunk:
        return 100;
    case EntityType::GoldNugget:
        return 500;
    case EntityType::GoldBar:
        return 500;
    case EntityType::GoldBars:
        return 1000;
    case EntityType::EmeraldBig:
        return 4;
    case EntityType::SapphireBig:
        return 6;
    case EntityType::RubyBig:
        return 8;
    default:
        return 0;
    }
}

std::optional<AudioAssetId> GetPickupSound(EntityType type_) {
    switch (type_) {
    case EntityType::Gold:
        return audio_asset_ids::Gold;
    case EntityType::GoldStack:
    case EntityType::GoldNugget:
    case EntityType::GoldBar:
    case EntityType::GoldBars:
        return audio_asset_ids::GoldStack;
    case EntityType::GoldChunk:
        return audio_asset_ids::Gold;
    case EntityType::EmeraldBig:
    case EntityType::SapphireBig:
    case EntityType::RubyBig:
        return audio_asset_ids::GoldStack;
    default:
        return std::nullopt;
    }
}

} // namespace

bool TryCollectEntityFromContact(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& collector = state.entity_manager.entities[entity_idx];
    if (!collector.active || !collector.can_collect_pickups) {
        return false;
    }

    const Entity* const pickup = state.entity_manager.GetEntity(state.entity_manager.entities[other_entity_idx].vid);
    if (pickup == nullptr) {
        return false;
    }
    if (pickup->buyable.active) {
        return false;
    }

    const unsigned int money_gained = GetPickupMoneyValue(pickup->type_);
    if (money_gained != 0) {
        collector.money += money_gained;
        if (const std::optional<AudioAssetId> sound_effect = GetPickupSound(pickup->type_)) {
            (void)PlayEntityCenterSoundEmitter(state, *pickup, *sound_effect);
        }
        state.entity_manager.SetInactive(other_entity_idx);
        state.UpdateSidForEntity(other_entity_idx, graphics);
        return true;
    }

    if (!TryCollectPassiveItem(collector, *pickup)) {
        return false;
    }

    state.entity_manager.SetInactive(other_entity_idx);
    state.UpdateSidForEntity(other_entity_idx, graphics);
    return true;
}

} // namespace splonks::entities::common
