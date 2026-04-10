#include "entities/mod.hpp"

#include <array>

namespace splonks::entities {

namespace {

void SetEntityNone(Entity& entity) {
    entity.Reset();
    entity.type_ = EntityType::None;
}

void SetEntityGold(Entity& entity) {
    money::SetEntityMoney(entity, EntityType::Gold);
}

void SetEntityGoldStack(Entity& entity) {
    money::SetEntityMoney(entity, EntityType::GoldStack);
}

void SetEntityPot(Entity& entity) {
    breakaway_container::SetEntityBreakawayContainer(entity, EntityType::Pot);
}

void SetEntityBox(Entity& entity) {
    breakaway_container::SetEntityBreakawayContainer(entity, EntityType::Box);
}

constexpr std::size_t EntityTypeIndex(EntityType type_) {
    return static_cast<std::size_t>(type_);
}

constexpr std::size_t kEntityTypeCount =
    static_cast<std::size_t>(EntityType::Scarab) + 1;

constexpr std::array<EntitySetupFn, kEntityTypeCount> kEntitySetupFns{{
    &SetEntityNone,                 // None
    &player::SetEntityPlayer,       // Player
    &block::SetEntityBlock,         // Block
    &ghost_ball::SetEntityGhostBall,// GhostBall
    &bat::SetEntityBat,             // Bat
    &rock::SetEntityRock,           // Rock
    &mouse_trailer::SetEntityMouseTrailer, // MouseTrailer
    &jetpack::SetEntityJetpack,     // JetPack
    &bomb::SetEntityBomb,           // Bomb
    &SetEntityGold,                 // Gold
    &SetEntityGoldStack,            // GoldStack
    &rope::SetEntityRope,           // Rope
    &SetEntityPot,                  // Pot
    &SetEntityBox,                  // Box
    &stomp_pad::SetEntityStompPad,  // StompPad
    &baseball_bat::SetEntityBaseballBat, // BaseballBat
    &altar::SetEntityAltarLeft,     // AltarLeft
    &altar::SetEntityAltarRight,    // AltarRight
    &sac_altar::SetEntitySacAltarLeft, // SacAltarLeft
    &sac_altar::SetEntitySacAltarRight, // SacAltarRight
    &gold_idol::SetEntityGoldIdol,  // GoldIdol
    &chest::SetEntityChest,         // Chest
    &mattock::SetEntityMattock,     // Mattock
    &dice::SetEntityDice,           // Dice
    &ruby_big::SetEntityRubyBig,    // RubyBig
    &emerald_big::SetEntityEmeraldBig, // EmeraldBig
    &sapphire_big::SetEntitySapphireBig, // SapphireBig
    &shopkeeper::SetEntityShopkeeper, // Shopkeeper
    &damsel::SetEntityDamsel,       // Damsel
    &sign::SetEntitySignGeneral,    // SignGeneral
    &sign::SetEntitySignBomb,       // SignBomb
    &sign::SetEntitySignWeapon,     // SignWeapon
    &sign::SetEntitySignRare,       // SignRare
    &sign::SetEntitySignClothing,   // SignClothing
    &sign::SetEntitySignCraps,      // SignCraps
    &sign::SetEntitySignKissing,    // SignKissing
    &lantern::SetEntityLantern,     // Lantern
    &lantern::SetEntityLanternRed,  // LanternRed
    &giant_tiki_head::SetEntityGiantTikiHead, // GiantTikiHead
    &arrow_trap::SetEntityArrowTrap, // ArrowTrap
    &snake::SetEntitySnake,         // Snake
    &caveman::SetEntityCaveman,     // Caveman
    &spider_hang::SetEntitySpiderHang, // SpiderHang
    &spider_hang::SetEntityGiantSpiderHang, // GiantSpiderHang
    &scarab::SetEntityScarab,       // Scarab
}};

} // namespace

EntitySetupFn GetEntitySetupFn(EntityType type_) {
    const std::size_t index = EntityTypeIndex(type_);
    if (index >= kEntitySetupFns.size()) {
        return &SetEntityNone;
    }
    return kEntitySetupFns[index];
}

void SetEntityByType(Entity& entity, EntityType type_) {
    GetEntitySetupFn(type_)(entity);
}

} // namespace splonks::entities
