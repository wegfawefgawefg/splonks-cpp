#pragma once

#include "../draw_layer.hpp"
#include "damage_types.hpp"
#include "left_or_right.hpp"
#include "math_types.hpp"
#include "vid.hpp"

#include <cstddef>
#include <cstdint>

namespace splonks {

struct HangHands {
    Vec2 left;
    Vec2 right;
};

struct HangHandBounds {
    Vec2 left_tl;
    Vec2 left_br;
    Vec2 right_tl;
    Vec2 right_br;
};

enum class EntityType {
    None,
    Player,
    Block,
    GhostBall,
    BasicExit,
    Entrance,
    DvdLogo,
    Bat,
    Rock,
    MouseTrailer,
    JetPack,
    Bomb,
    Gold,
    GoldStack,
    GoldChunk,
    GoldNugget,
    GoldBar,
    GoldBars,
    Rope,
    Pot,
    Box,
    StompPad,
    BaseballBat,
    Altar,
    SacAltar,
    GoldIdol,
    Chest,
    KeyChest,
    ChestKey,
    UdjatEye,
    Mattock,
    Cape,
    Shotgun,
    Teleporter,
    TeleporterBackpack,
    Gloves,
    Spectacles,
    WebCannon,
    Pistol,
    Mitt,
    Paste,
    SpringShoes,
    SpikeShoes,
    Machete,
    BombBox,
    BombBag,
    Bow,
    Compass,
    Parachute,
    RopePile,
    Dice,
    CrapsTable,
    RubyBig,
    EmeraldBig,
    SapphireBig,
    Shop,
    Shopkeeper,
    Damsel,
    SignGeneral,
    SignBomb,
    SignWeapon,
    SignRare,
    SignClothing,
    SignCraps,
    SignKissing,
    StoreLight,
    Lantern,
    LanternRed,
    GiantTikiHead,
    Boulder,
    MovingPlatform,
    SacAltarTopper,
    KaliHead,
    BallAndChainBall,
    ArrowTrap,
    Arrow,
    Skull,
    Skeleton,
    Snake,
    Cobra,
    CobraSpit,
    Caveman,
    Spider,
    SpiderHang,
    RageSpider,
    RageSpiderHang,
    GiantSpider,
    GiantSpiderHang,
    Scarab,
    Mantrap,
    Frog,
    FireFrog,
    Monkey,
    Piranha,
    Zombie,
    Vampire,
    Ufo,
    Hawkman,
    Meathead,
    WebBall,
    Cobweb,
    Jaws,
    CrystalSkull,
    Bones,
    TrapBlock,
    CeilingTrap,
    TombLord,
    Door,
    Ankh,
    Crown,
    XocBlock,
    Moai,
    Moai2,
    Moai3,
    MoaiInside,
    Yeti,
    AlienShip,
    AlienBoss,
    BarrierEmitter,
    Beam,
    ThwompTrap,
    Lamp,
    LampRed,
    Sign,
    FlappyBee,
    FleshGuy,
    DebugMovingLight,
};

constexpr std::size_t EntityTypeIndex(EntityType type_) {
    return static_cast<std::size_t>(type_);
}

constexpr std::size_t kEntityTypeCount = EntityTypeIndex(EntityType::DebugMovingLight) + 1;

constexpr bool IsPlayerLikeEntityType(EntityType type_) {
    return type_ == EntityType::Player || type_ == EntityType::FlappyBee ||
           type_ == EntityType::FleshGuy;
}

constexpr float kTravelSoundDistInterval = 24.0F;
constexpr float kWalkerClimberTravelSoundDistInterval = 12.0F;
constexpr float kClimberTravelSoundDistInterval = 9.0F;

enum class TravelSound {
    One,
    Two,
};

enum class EntityCondition {
    Normal,
    Dead,
    Stunned,
};

enum class EntityAiState {
    Idle,
    Disturbed,
    Patrolling,
    Pursuing,
    Returning,
};

enum class EntityMovementFlag : std::uint8_t {
    Walking,
    Running,
    Pushing,
    Climbing,
    Hanging,
};

enum class PointLabel {
    None,
    Target,
    GoingHere,
    Boundary,
    Avoid,
};

enum class Alignment {
    Ally,
    Neutral,
    Enemy,
};

enum class EntityLabel {
    None,
    AttackThis,
    GetThis,
    AvoidThis,
    BeNearThis,
    GoToThis,
    AttachedToThis,
};

enum class EntityDisplayState {
    Neutral,
    NeutralHolding,
    Walk,
    WalkHolding,
    Fly,
    Dead,
    Stunned,
    Climbing,
    Hanging,
    Falling,
    EmoteDab,
    EmoteBald,
};

enum class AttachmentMode {
    None,
    Held,
    Back,
};

constexpr std::uint32_t kDefaultHoldingTimer = 8;
constexpr std::uint32_t kCollidedTriggerCooldown = 3;

} // namespace splonks
