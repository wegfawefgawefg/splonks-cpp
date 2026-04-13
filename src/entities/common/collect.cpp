#include "entities/common/common.hpp"

namespace splonks::entities::common {

namespace {

bool CanCollectPickups(const Entity& entity) {
    switch (entity.type_) {
    case EntityType::Player:
        return true;
    case EntityType::None:
    case EntityType::Block:
    case EntityType::GhostBall:
    case EntityType::Bat:
    case EntityType::Rock:
    case EntityType::MouseTrailer:
    case EntityType::JetPack:
    case EntityType::Bomb:
    case EntityType::Gold:
    case EntityType::GoldStack:
    case EntityType::GoldChunk:
    case EntityType::GoldNugget:
    case EntityType::GoldBar:
    case EntityType::GoldBars:
    case EntityType::StompPad:
    case EntityType::Rope:
    case EntityType::Pot:
    case EntityType::Box:
    case EntityType::BaseballBat:
    case EntityType::Altar:
        case EntityType::SacAltar:
        case EntityType::GoldIdol:
    case EntityType::Chest:
    case EntityType::KeyChest:
    case EntityType::ChestKey:
    case EntityType::UdjatEye:
    case EntityType::Mattock:
    case EntityType::Cape:
    case EntityType::Shotgun:
    case EntityType::Teleporter:
    case EntityType::Gloves:
    case EntityType::Spectacles:
    case EntityType::WebCannon:
    case EntityType::Pistol:
    case EntityType::Mitt:
    case EntityType::Paste:
    case EntityType::SpringShoes:
    case EntityType::SpikeShoes:
    case EntityType::Machete:
    case EntityType::BombBox:
    case EntityType::BombBag:
    case EntityType::Bow:
    case EntityType::Compass:
    case EntityType::Parachute:
    case EntityType::RopePile:
    case EntityType::Dice:
    case EntityType::RubyBig:
    case EntityType::EmeraldBig:
    case EntityType::SapphireBig:
    case EntityType::Shopkeeper:
    case EntityType::Damsel:
    case EntityType::SignGeneral:
    case EntityType::SignBomb:
    case EntityType::SignWeapon:
    case EntityType::SignRare:
    case EntityType::SignClothing:
    case EntityType::SignCraps:
    case EntityType::SignKissing:
    case EntityType::Lantern:
    case EntityType::LanternRed:
    case EntityType::GiantTikiHead:
    case EntityType::KaliHead:
    case EntityType::ArrowTrap:
    case EntityType::Snake:
    case EntityType::Caveman:
    case EntityType::Spider:
    case EntityType::SpiderHang:
    case EntityType::GiantSpiderHang:
    case EntityType::Scarab:
        return false;
    }

    return false;
}

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

std::optional<SoundEffect> GetPickupSound(EntityType type_) {
    switch (type_) {
    case EntityType::Gold:
        return SoundEffect::Gold;
    case EntityType::GoldStack:
    case EntityType::GoldNugget:
    case EntityType::GoldBar:
    case EntityType::GoldBars:
        return SoundEffect::GoldStack;
    case EntityType::GoldChunk:
        return SoundEffect::Gold;
    case EntityType::EmeraldBig:
    case EntityType::SapphireBig:
    case EntityType::RubyBig:
        return SoundEffect::GoldStack;
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
    if (entity_idx >= state.entity_manager.entities.size() ||
        other_entity_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& collector = state.entity_manager.entities[entity_idx];
    if (!collector.active || !CanCollectPickups(collector)) {
        return false;
    }

    const Entity* const pickup = state.entity_manager.GetEntity(state.entity_manager.entities[other_entity_idx].vid);
    if (pickup == nullptr) {
        return false;
    }

    const unsigned int money_gained = GetPickupMoneyValue(pickup->type_);
    if (money_gained != 0) {
        collector.money += money_gained;
        if (const std::optional<SoundEffect> sound_effect = GetPickupSound(pickup->type_)) {
            audio.PlaySoundEffect(*sound_effect);
        }
        state.entity_manager.SetInactive(other_entity_idx);
        state.UpdateSidForEntity(other_entity_idx, graphics);
        return true;
    }

    if (!TryCollectPassiveItem(collector, pickup->type_)) {
        return false;
    }

    state.entity_manager.SetInactive(other_entity_idx);
    state.UpdateSidForEntity(other_entity_idx, graphics);
    return true;
}

} // namespace splonks::entities::common
