#pragma once

#include "damage_types.hpp"
#include "entity/core_types.hpp"
#include "math_types.hpp"
#include "presentation_commands.hpp"
#include "stage.hpp"
#include "vid.hpp"

#include <optional>
#include <cstdint>
#include <variant>

namespace splonks {

enum class GameplayTileLayer : std::uint8_t {
    Foreground,
    Backwall,
};

enum class GameplayActionKind : std::uint16_t {
    None,
    UseTool,
    PickupEntity,
    DropEntity,
    ThrowEntity,
    UseHeldEntity,
    UseBackEntity,
    PutHeldEntityOnBack,
    TakeOffBackEntity,
    InteractEntity,
    CollectEntity,
    PushEntity,
    BreakTile,
    DamageEntity,
    HitEntity,
};

enum class GameplayUseEdge : std::uint8_t {
    None,
    Press,
    Release,
};

struct UseToolAction {
    VID source_vid{};
    Vec2 velocity = Vec2::New(0.0F, 0.0F);
    std::uint32_t tool_slot = 0;
};

struct PickupEntityAction {
    VID source_vid{};
    VID target_vid{};
};

struct DropEntityAction {
    VID source_vid{};
    VID target_vid{};
};

struct ThrowEntityAction {
    VID source_vid{};
    VID target_vid{};
    Vec2 velocity = Vec2::New(0.0F, 0.0F);
};

struct UseHeldEntityAction {
    VID source_vid{};
    VID target_vid{};
    IVec2 direction = IVec2::New(0, 0);
    GameplayUseEdge use_edge = GameplayUseEdge::None;
};

struct UseBackEntityAction {
    VID source_vid{};
    VID target_vid{};
    IVec2 direction = IVec2::New(0, 0);
    GameplayUseEdge use_edge = GameplayUseEdge::None;
};

struct PutHeldEntityOnBackAction {
    VID source_vid{};
    VID target_vid{};
};

struct TakeOffBackEntityAction {
    VID source_vid{};
    VID target_vid{};
};

struct InteractEntityAction {
    VID source_vid{};
    VID target_vid{};
};

struct CollectEntityAction {
    VID source_vid{};
    VID target_vid{};
};

struct PushEntityAction {
    VID source_vid{};
    VID target_vid{};
    Vec2 velocity = Vec2::New(0.0F, 0.0F);
};

struct BreakTileAction {
    std::optional<VID> source_vid = std::nullopt;
    IVec2 tile_pos = IVec2::New(0, 0);
};

struct DamageEntityAction {
    std::optional<VID> source_vid = std::nullopt;
    VID target_vid{};
    DamageType damage_type = DamageType::Attack;
    unsigned int amount = 0;
};

struct HitEntityAction {
    std::optional<VID> source_vid = std::nullopt;
    VID target_vid{};
    Vec2 velocity = Vec2::New(0.0F, 0.0F);
    DamageType damage_type = DamageType::Attack;
    DamageType projectile_contact_damage_type = DamageType::Attack;
    unsigned int amount = 0;
    unsigned int projectile_contact_damage_amount = 0;
    std::uint32_t thrown_immunity_timer = 0;
    std::uint32_t projectile_contact_duration = 0;
    bool clear_velocity = true;
    bool clear_acceleration = true;
    bool knockback_on_no_damage = false;
};

using GameplayActionPayload = std::variant<
    std::monostate,
    UseToolAction,
    PickupEntityAction,
    DropEntityAction,
    ThrowEntityAction,
    UseHeldEntityAction,
    UseBackEntityAction,
    PutHeldEntityOnBackAction,
    TakeOffBackEntityAction,
    InteractEntityAction,
    CollectEntityAction,
    PushEntityAction,
    BreakTileAction,
    DamageEntityAction,
    HitEntityAction
>;

struct GameplayActionRequested {
    GameplayActionPayload payload{};

    GameplayActionRequested() = default;

    template <typename Action>
    GameplayActionRequested(Action action) : payload(action) {}
};

template <typename... Lambdas>
struct GameplayActionVisitor : Lambdas... {
    using Lambdas::operator()...;
};

template <typename... Lambdas>
GameplayActionVisitor(Lambdas...) -> GameplayActionVisitor<Lambdas...>;

inline GameplayActionKind GetGameplayActionKind(const GameplayActionRequested& request) {
    return std::visit(
        GameplayActionVisitor{
            [](const std::monostate&) { return GameplayActionKind::None; },
            [](const UseToolAction&) { return GameplayActionKind::UseTool; },
            [](const PickupEntityAction&) { return GameplayActionKind::PickupEntity; },
            [](const DropEntityAction&) { return GameplayActionKind::DropEntity; },
            [](const ThrowEntityAction&) { return GameplayActionKind::ThrowEntity; },
            [](const UseHeldEntityAction&) { return GameplayActionKind::UseHeldEntity; },
            [](const UseBackEntityAction&) { return GameplayActionKind::UseBackEntity; },
            [](const PutHeldEntityOnBackAction&) { return GameplayActionKind::PutHeldEntityOnBack; },
            [](const TakeOffBackEntityAction&) { return GameplayActionKind::TakeOffBackEntity; },
            [](const InteractEntityAction&) { return GameplayActionKind::InteractEntity; },
            [](const CollectEntityAction&) { return GameplayActionKind::CollectEntity; },
            [](const PushEntityAction&) { return GameplayActionKind::PushEntity; },
            [](const BreakTileAction&) { return GameplayActionKind::BreakTile; },
            [](const DamageEntityAction&) { return GameplayActionKind::DamageEntity; },
            [](const HitEntityAction&) { return GameplayActionKind::HitEntity; },
        },
        request.payload
    );
}

inline std::optional<VID> GetGameplayActionSourceVid(const GameplayActionRequested& request) {
    return std::visit(
        GameplayActionVisitor{
            [](const std::monostate&) -> std::optional<VID> { return std::nullopt; },
            [](const UseToolAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const PickupEntityAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const DropEntityAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const ThrowEntityAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const UseHeldEntityAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const UseBackEntityAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const PutHeldEntityOnBackAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const TakeOffBackEntityAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const InteractEntityAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const CollectEntityAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const PushEntityAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const BreakTileAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const DamageEntityAction& action) -> std::optional<VID> { return action.source_vid; },
            [](const HitEntityAction& action) -> std::optional<VID> { return action.source_vid; },
        },
        request.payload
    );
}

inline std::optional<VID> GetGameplayActionTargetVid(const GameplayActionRequested& request) {
    return std::visit(
        GameplayActionVisitor{
            [](const std::monostate&) -> std::optional<VID> { return std::nullopt; },
            [](const UseToolAction&) -> std::optional<VID> { return std::nullopt; },
            [](const PickupEntityAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const DropEntityAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const ThrowEntityAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const UseHeldEntityAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const UseBackEntityAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const PutHeldEntityOnBackAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const TakeOffBackEntityAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const InteractEntityAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const CollectEntityAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const PushEntityAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const BreakTileAction&) -> std::optional<VID> { return std::nullopt; },
            [](const DamageEntityAction& action) -> std::optional<VID> { return action.target_vid; },
            [](const HitEntityAction& action) -> std::optional<VID> { return action.target_vid; },
        },
        request.payload
    );
}

struct GameplayEntitySpawned {
    VID entity_vid{};
    std::optional<VID> held_by_vid = std::nullopt;
    EntityType entity_type = EntityType::None;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    Vec2 size = Vec2::New(0.0F, 0.0F);
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    float light_strength = 0.0F;
    Color3 light_color = Color3::White();
    std::int32_t light_radius = 0;
    std::uint32_t movement_flags = 0;
    bool use_pressed = false;
    std::uint8_t animate = 0;
    std::uint8_t animation_loop = 1;
    std::uint8_t animation_finished = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct GameplayEntityDeactivated {
    VID entity_vid{};
};

struct GameplayEntityHeld {
    VID holder_vid{};
    VID held_vid{};
    AttachmentMode attachment_mode = AttachmentMode::Held;
};

struct GameplayEntityDropped {
    VID entity_vid{};
    std::optional<VID> dropped_by_vid = std::nullopt;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
};

struct GameplayEntityThrown {
    VID thrower_vid{};
    VID entity_vid{};
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
};

struct GameplayEntityDamaged {
    VID entity_vid{};
    std::optional<VID> source_vid = std::nullopt;
    DamageType damage_type = DamageType::Attack;
    unsigned int amount = 0;
    unsigned int remaining_health = 0;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    std::uint32_t fall_timer = 0;
    std::uint32_t stun_timer = 0;
    std::uint32_t projectile_contact_timer = 0;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t animate = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct GameplayEntityStatePatched {
    VID entity_vid{};
    VID source_vid{};
    std::optional<VID> entity_a_vid;
    std::optional<VID> entity_b_vid;
    std::optional<VID> entity_c_vid;
    std::optional<VID> entity_d_vid;
    std::optional<VID> holding_vid;
    std::optional<VID> held_by_vid;
    std::optional<VID> back_vid;
    Vec2 pos = Vec2::New(0.0F, 0.0F);
    Vec2 vel = Vec2::New(0.0F, 0.0F);
    Vec2 acc = Vec2::New(0.0F, 0.0F);
    Vec2 size = Vec2::New(0.0F, 0.0F);
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    float counter_c = 0.0F;
    float counter_d = 0.0F;
    float threshold_a = 0.0F;
    float threshold_b = 0.0F;
    IVec2 point_a = IVec2::New(0, 0);
    IVec2 point_b = IVec2::New(0, 0);
    IVec2 point_c = IVec2::New(0, 0);
    IVec2 point_d = IVec2::New(0, 0);
    unsigned int health = 0;
    std::uint32_t coyote_time = 0;
    std::uint32_t fall_timer = 0;
    std::uint32_t stun_timer = 0;
    std::uint32_t projectile_contact_timer = 0;
    float light_strength = 0.0F;
    Color3 light_color = Color3::White();
    std::int32_t light_radius = 0;
    float rotation = 0.0F;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t active = 0;
    std::uint8_t has_physics = 1;
    std::uint8_t can_collide = 1;
    std::uint8_t can_apply_projectile_contact = 1;
    std::uint8_t damage_vulnerability = 0;
    std::uint8_t facing = 0;
    std::uint8_t ai_state = 0;
    std::uint8_t wanted = 0;
    std::uint8_t holding = 0;
    std::uint8_t render_enabled = 1;
    std::uint8_t attachment_mode = 0;
    std::uint8_t draw_layer = 0;
    std::uint32_t movement_flags = 0;
    std::uint32_t money = 0;
    std::int32_t stage_exit_id = -1;
    std::uint32_t runtime_flags = 0;
    std::uint8_t buyable_active = 0;
    std::uint32_t buyable_display_quantity = 0;
    FrameDataId buyable_display_icon_animation_id = kInvalidFrameDataId;
    std::optional<VID> buyable_shop_owner_vid;
    std::uint8_t animate = 0;
    std::uint8_t animation_loop = 1;
    std::uint8_t animation_finished = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
};

struct GameplayPlayerStatePatched {
    VID player_vid{};
};

struct GameplayRunStatePatched {
};

struct GameplayTileChanged {
    IVec2 tile_pos = IVec2::New(0, 0);
    Tile tile = Tile::Air;
    TileRotation rotation = kTileRotation0;
    GameplayTileLayer layer = GameplayTileLayer::Foreground;
};

struct GameplayTileBroken {
    IVec2 tile_pos = IVec2::New(0, 0);
};

struct GameplayStageLightAdded {
    VID light_vid{};
    IVec2 tile_pos = IVec2::New(0, 0);
    int radius = 0;
};

struct GameplayStageLightRemoved {
    VID light_vid{};
};

} // namespace splonks
