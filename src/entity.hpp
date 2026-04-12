#pragma once

#include "entity_core_types.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "stage.hpp"
#include "utils.hpp"

#include <optional>
#include <cstdint>
#include <tuple>

namespace splonks {

constexpr unsigned int kJumpDelayFrames = 1;

enum class EntityPassiveItem : std::uint8_t {
    Gloves,
    Spectacles,
    Compass,
    Mitt,
    Paste,
    SpringShoes,
    SpikeShoes,
    Count,
};

struct UseState {
    bool down = false;
    bool pressed = false;
    bool released = false;
    std::uint32_t frames = 0;
    std::optional<VID> user_vid;
    AttachmentMode source = AttachmentMode::None;
};

struct Entity {
    bool active = false;
    bool marked_for_destruction = false;
    EntityType type_ = EntityType::None;
    VID vid;
    bool was_horizontally_controlled_this_frame = false;
    bool has_physics = true;
    bool can_collide = true;
    bool stone = false;
    bool wanted = false;
    bool crusher_pusher = false;
    bool can_go_on_back = false;
    bool grounded = false;
    std::uint32_t coyote_time = 0;
    std::uint32_t stun_timer = 0;
    bool can_be_picked_up = true;
    bool impassable = false;
    float fall_distance = 0.0F;
    Vec2 pos;
    Vec2 vel;
    Vec2 acc;
    Vec2 size;
    float dist_traveled_this_frame = 0.0F;
    Origin origin = Origin::TopLeft;
    LeftOrRight facing = LeftOrRight::Left;
    bool vertical_flip = false;
    DrawLayer draw_layer = DrawLayer::Middle;
    EntityDisplayState display_state = EntityDisplayState::Neutral;
    FrameDataAnimator frame_data_animator;
    std::uint32_t jump_delay_frame_count = kJumpDelayFrames;
    bool jumped_this_frame = false;
    bool left_hanging = false;
    bool right_hanging = false;
    bool can_hang_ledge = false;
    bool can_hang_wall = false;
    std::uint32_t hang_count = 0;
    bool holding = false;
    bool climbing = false;
    std::uint64_t passive_item_flags = 0;
    std::uint32_t money = 0;
    std::uint32_t bombs = 4;
    std::uint32_t ropes = 4;
    std::optional<VID> back_vid;
    AttachmentMode attachment_mode = AttachmentMode::None;
    UseState use_state;
    float travel_sound_countdown = kTravelSoundDistInterval;
    TravelSound travel_sound = TravelSound::One;
    EntityCondition condition = EntityCondition::Normal;
    EntityCondition last_condition = EntityCondition::Normal;
    EntityAiState ai_state = EntityAiState::Idle;
    EntityAiState last_ai_state = EntityAiState::Idle;
    EntityState state = EntityState::Idle;
    std::uint32_t health = 0;
    std::uint32_t last_health = 0;
    bool hurt_on_contact = false;
    float attack_weight = 0.0F;
    float weight = 0.0F;
    std::uint32_t bomb_throw_delay_countdown = 0;
    std::uint32_t rope_throw_delay_countdown = 0;
    std::uint32_t attack_delay_countdown = 0;
    std::uint32_t equip_delay_countdown = 0;
    std::optional<VID> thrown_by;
    std::uint32_t thrown_immunity_timer = 0;
    bool collided = false;
    bool collided_last_frame = false;
    std::uint32_t collided_trigger_cooldown = 0;
    DamageVulnerability damage_vulnerability = DamageVulnerability::Vulnerable;
    bool can_be_stunned = false;
    IVec2 point_a;
    IVec2 point_b;
    IVec2 point_c;
    IVec2 point_d;
    PointLabel point_label_a = PointLabel::None;
    PointLabel point_label_b = PointLabel::None;
    PointLabel point_label_c = PointLabel::None;
    PointLabel point_label_d = PointLabel::None;
    std::optional<VID> holding_vid;
    std::optional<VID> held_by_vid;
    std::uint32_t holding_timer = kDefaultHoldingTimer;
    std::optional<VID> entity_a;
    std::optional<VID> entity_b;
    std::optional<VID> entity_c;
    std::optional<VID> entity_d;
    EntityLabel entity_label_a = EntityLabel::None;
    EntityLabel entity_label_b = EntityLabel::None;
    EntityLabel entity_label_c = EntityLabel::None;
    EntityLabel entity_label_d = EntityLabel::None;
    Alignment alignment = Alignment::Neutral;
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    float counter_c = 0.0F;
    float counter_d = 0.0F;
    float threshold_a = 0.0F;
    float threshold_b = 0.0F;
    float threshold_c = 0.0F;
    float threshold_d = 0.0F;

    static constexpr Vec2 kHangHandSize = {1.0F, 4.0F};

    static Entity New();
    void Reset();
    std::tuple<Vec2, Vec2> GetBounds() const;
    AABB GetAABB() const;
    Vec2 GetCenter() const;
    void SetCenter(const Vec2& center);
    void IncTravelSound();
    bool IsHanging() const;
    bool IsHorizontallyControlled() const;
    AABB GetFeet() const;
    void SetGrounded(const Stage& stage);
    std::tuple<Vec2, Vec2> GetTlAndTrCorners() const;
    HangHands GetHangHands() const;
    HangHandBounds GetHangHandsBounds() const;
};

// Raw animation path.
// Use this when entity-owned logic knows the exact authored animation id it wants.
// This does not change semantic display state.
void SetAnimation(Entity& entity, FrameDataId animation_id);
// Semantic animation path.
// Use this from shared/external gameplay code that only knows a generic display state
// like Neutral, Walk, Hanging, or Stunned rather than an exact authored animation id.
bool TrySetAnimation(Entity& entity, EntityDisplayState display_state);
void UseEntity(Entity& entity, std::optional<VID> user_vid, AttachmentMode source);
void StopUsingEntity(Entity& entity);
const char* PassiveItemToString(EntityPassiveItem passive_item);
bool HasPassiveItem(const Entity& entity, EntityPassiveItem passive_item);
void SetPassiveItem(Entity& entity, EntityPassiveItem passive_item, bool enabled);
bool TryCollectPassiveItem(Entity& entity, EntityType pickup_type);
bool CanRevealEmbeddedTreasure(const Entity& entity);
void EnableStone(Entity& entity);
void DisableStone(Entity& entity);

} // namespace splonks
