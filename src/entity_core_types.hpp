#pragma once

#include "math_types.hpp"

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
    Bat,
    Rock,
    MouseTrailer,
    JetPack,
    Bomb,
    Gold,
    GoldStack,
    Rope,
    Pot,
    Box,
    StompPad,
    BaseballBat,
    AltarLeft,
    AltarRight,
    SacAltarLeft,
    SacAltarRight,
    GoldIdol,
    Chest,
    Mattock,
    Cape,
    Shotgun,
    Teleporter,
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
    RubyBig,
    EmeraldBig,
    SapphireBig,
    Shopkeeper,
    Damsel,
    SignGeneral,
    SignBomb,
    SignWeapon,
    SignRare,
    SignClothing,
    SignCraps,
    SignKissing,
    Lantern,
    LanternRed,
    GiantTikiHead,
    KaliHead,
    ArrowTrap,
    Snake,
    Caveman,
    SpiderHang,
    GiantSpiderHang,
    Scarab,
};

constexpr std::size_t EntityTypeIndex(EntityType type_) {
    return static_cast<std::size_t>(type_);
}

constexpr std::size_t kEntityTypeCount = EntityTypeIndex(EntityType::Scarab) + 1;

constexpr float kTravelSoundDistInterval = 24.0F;
constexpr float kWalkerClimberTravelSoundDistInterval = 24.0F;

enum class TravelSound {
    One,
    Two,
};

enum class EntitySuperState {
    Idle,
    Dead,
    Stunned,
    Pursuing,
    Attacking,
    Defending,
    Fleeing,
    Searching,
    Patrolling,
    Roaming,
    Returning,
};

enum class EntityState {
    Idle,
    Walking,
    Running,
    Flying,
    Jumping,
    Climbing,
    WindingUp,
    Attacking,
    Guarding,
    Taunting,
    PickingUp,
    Using,
    Crouching,
    Dead,
    Stunned,
    Dodging,
    Flinching,
    Falling,
    InUse,
    Pushing,
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

enum class DrawLayer {
    Background,
    Middle,
    Foreground,
};

enum class LeftOrRight {
    Left,
    Right,
};

enum class DamageType {
    Attack,
    HeavyAttack,
    JumpOn,
    Burn,
    Explosion,
    Crush,
    Spikes,
};

enum class DamageVulnerability {
    Immune,
    BurningOnly,
    CrushingOnly,
    AttackingOnly,
    HeavyAttackOnly,
    ExplosionOnly,
    CrushingAndSpikes,
    CrushingSpikesAndExplosion,
    Vulnerable,
    AnthingExceptJumpOn,
};

struct VID {
    std::size_t id = 0;
    std::uint32_t version = 0;
};

inline bool operator==(const VID& left, const VID& right) {
    return left.id == right.id && left.version == right.version;
}

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
};

enum class AttachmentMode {
    None,
    Held,
    Back,
};

constexpr std::uint32_t kDefaultHoldingTimer = 8;
constexpr std::uint32_t kCollidedTriggerCooldown = 3;

enum class Origin {
    TopLeft,
    Center,
    Foot,
};

} // namespace splonks
