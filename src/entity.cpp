#include "entity.hpp"

#include "entity/display_states.hpp"
#include "entity/display_support.hpp"
#include "entity/archetype.hpp"
#include "frame_data_id.hpp"
#include "tile.hpp"

#include <vector>

namespace splonks {

namespace {

struct StoneBaseFields {
    bool crusher_pusher = false;
    bool impassable = false;
    DamageVulnerability damage_vulnerability = DamageVulnerability::Vulnerable;
};

StoneBaseFields BuildStoneBaseFieldsForEntityType(EntityType type_) {
    const EntityArchetype& archetype = GetEntityArchetype(type_);
    return StoneBaseFields{
        .crusher_pusher = archetype.crusher_pusher,
        .impassable = archetype.impassable,
        .damage_vulnerability = archetype.damage_vulnerability,
    };
}

constexpr std::uint64_t PassiveItemBit(EntityPassiveItem passive_item) {
    return 1ULL << static_cast<unsigned int>(passive_item);
}

constexpr std::uint32_t MovementFlagBit(EntityMovementFlag movement_flag) {
    return 1U << static_cast<unsigned int>(movement_flag);
}

} // namespace

Entity Entity::New() {
    Entity entity;
    entity.active = false;
    entity.marked_for_destruction = false;
    entity.type_ = EntityType::None;
    entity.vid = VID{0, 0};
    entity.was_horizontally_controlled_this_frame = false;
    entity.has_physics = true;
    entity.can_collide = true;
    entity.stone = false;
    entity.crusher_pusher = false;
    entity.grounded = false;
    entity.coyote_time = 0;
    entity.stun_timer = 0;
    entity.can_be_picked_up = true;
    entity.impassable = false;
    entity.fall_distance = 0.0F;
    entity.pos = Vec2::New(0.0F, 0.0F);
    entity.vel = Vec2::New(0.0F, 0.0F);
    entity.acc = Vec2::New(0.0F, 0.0F);
    entity.size = Vec2::New(8.0F, 8.0F);
    entity.dist_traveled_this_frame = 0.0F;
    entity.origin = Origin::TopLeft;
    entity.facing = LeftOrRight::Left;
    entity.vertical_flip = false;
    entity.draw_layer = DrawLayer::Middle;
    TrySetAnimation(entity, EntityDisplayState::Neutral);
    entity.frame_data_animator = FrameDataAnimator{};
    entity.jump_delay_frame_count = kJumpDelayFrames;
    entity.jumped_this_frame = false;
    entity.hang_side.reset();
    entity.can_hang_ledge = false;
    entity.can_hang_wall = false;
    entity.hang_count = 0;
    entity.holding = false;
    entity.passive_item_flags = 0;
    entity.money = 0;
    entity.bombs = 0;
    entity.ropes = 0;
    entity.attachment_mode = AttachmentMode::None;
    entity.use_state = UseState{};
    entity.travel_sound_countdown = kTravelSoundDistInterval;
    entity.travel_sound = TravelSound::One;
    entity.condition = EntityCondition::Normal;
    entity.last_condition = EntityCondition::Normal;
    entity.ai_state = EntityAiState::Idle;
    entity.last_ai_state = EntityAiState::Idle;
    entity.movement_flags = 0;
    entity.health = 0;
    entity.hurt_on_contact = false;
    entity.damage_vulnerability = DamageVulnerability::Vulnerable;
    entity.attack_weight = 0.0F;
    entity.weight = 0.0F;
    entity.bomb_throw_delay_countdown = 0;
    entity.rope_throw_delay_countdown = 0;
    entity.attack_delay_countdown = 0;
    entity.equip_delay_countdown = 0;
    entity.thrown_immunity_timer = 0;
    entity.collided = false;
    entity.collided_last_frame = false;
    entity.contact_sound_cooldown = 0;
    entity.can_be_stunned = false;
    entity.point_a = IVec2::New(0, 0);
    entity.point_b = IVec2::New(0, 0);
    entity.point_c = IVec2::New(0, 0);
    entity.point_d = IVec2::New(0, 0);
    entity.point_label_a = PointLabel::None;
    entity.point_label_b = PointLabel::None;
    entity.point_label_c = PointLabel::None;
    entity.point_label_d = PointLabel::None;
    entity.holding_timer = kDefaultHoldingTimer;
    entity.entity_label_a = EntityLabel::None;
    entity.entity_label_b = EntityLabel::None;
    entity.entity_label_c = EntityLabel::None;
    entity.entity_label_d = EntityLabel::None;
    entity.alignment = Alignment::Neutral;
    entity.counter_a = 0.0F;
    entity.counter_b = 0.0F;
    entity.counter_c = 0.0F;
    entity.counter_d = 0.0F;
    entity.threshold_a = 0.0F;
    entity.threshold_b = 0.0F;
    entity.threshold_c = 0.0F;
    entity.threshold_d = 0.0F;
    return entity;
}

void Entity::Reset() {
    const VID existing_vid = vid;
    *this = Entity::New();
    vid = existing_vid;
    active = true;
}

void UseEntity(Entity& entity, std::optional<VID> user_vid, AttachmentMode source) {
    const bool was_down = entity.use_state.down;
    entity.use_state.down = true;
    entity.use_state.pressed = !was_down;
    entity.use_state.released = false;
    entity.use_state.frames = was_down ? entity.use_state.frames + 1 : 1;
    entity.use_state.user_vid = user_vid;
    entity.use_state.source = source;
}

void StopUsingEntity(Entity& entity) {
    const bool was_down = entity.use_state.down;
    entity.use_state.down = false;
    entity.use_state.pressed = false;
    entity.use_state.released = was_down;
    entity.use_state.frames = 0;
    entity.use_state.user_vid.reset();
    entity.use_state.source = AttachmentMode::None;
}

bool HasMovementFlag(const Entity& entity, EntityMovementFlag movement_flag) {
    return (entity.movement_flags & MovementFlagBit(movement_flag)) != 0;
}

void SetMovementFlag(Entity& entity, EntityMovementFlag movement_flag, bool enabled) {
    if (enabled) {
        entity.movement_flags |= MovementFlagBit(movement_flag);
        return;
    }

    entity.movement_flags &= ~MovementFlagBit(movement_flag);
}

void ClearTransientMovementFlags(Entity& entity) {
    SetMovementFlag(entity, EntityMovementFlag::Walking, false);
    SetMovementFlag(entity, EntityMovementFlag::Running, false);
    SetMovementFlag(entity, EntityMovementFlag::Pushing, false);
}

std::tuple<Vec2, Vec2> Entity::GetBounds() const {
    switch (origin) {
    case Origin::TopLeft:
        return {pos, pos + size - Vec2::New(1.0F, 1.0F)};
    case Origin::Center: {
        const Vec2 tl = pos - size / 2.0F;
        const Vec2 br = tl + size - Vec2::New(1.0F, 1.0F);
        return {tl, br};
    }
    case Origin::Foot: {
        const Vec2 tl = pos - Vec2::New(size.x / 2.0F, size.y);
        const Vec2 br = tl + size - Vec2::New(1.0F, 1.0F);
        return {tl, br};
    }
    }

    return {pos, pos + size - Vec2::New(1.0F, 1.0F)};
}

AABB Entity::GetAABB() const {
    switch (origin) {
    case Origin::TopLeft:
        return AABB::New(pos, pos + size - Vec2::New(1.0F, 1.0F));
    case Origin::Center: {
        const Vec2 tl = pos - size / 2.0F;
        const Vec2 br = tl + size - Vec2::New(1.0F, 1.0F);
        return AABB::New(tl, br);
    }
    case Origin::Foot: {
        const Vec2 tl = pos - Vec2::New(size.x / 2.0F, size.y);
        const Vec2 br = tl + size - Vec2::New(1.0F, 1.0F);
        return AABB::New(tl, br);
    }
    }

    return AABB::New(pos, pos + size - Vec2::New(1.0F, 1.0F));
}

Vec2 Entity::GetCenter() const {
    switch (origin) {
    case Origin::TopLeft:
        return pos + size / 2.0F;
    case Origin::Center:
        return pos;
    case Origin::Foot:
        return pos + Vec2::New(size.x / 2.0F, 0.0F);
    }

    return pos;
}

void Entity::SetCenter(const Vec2& center) {
    switch (origin) {
    case Origin::TopLeft:
        pos = center - size / 2.0F;
        return;
    case Origin::Center:
        pos = center;
        return;
    case Origin::Foot:
        pos = center - Vec2::New(size.x / 2.0F, 0.0F);
        return;
    }
}

void Entity::IncTravelSound() {
    switch (travel_sound) {
    case TravelSound::One:
        travel_sound = TravelSound::Two;
        return;
    case TravelSound::Two:
        travel_sound = TravelSound::One;
        return;
    }
}

bool Entity::IsHanging() const {
    return hang_side.has_value();
}

bool Entity::IsClimbing() const {
    return HasMovementFlag(*this, EntityMovementFlag::Climbing);
}

bool Entity::IsHorizontallyControlled() const {
    return was_horizontally_controlled_this_frame;
}

AABB Entity::GetFeet() const {
    const auto [tl, br] = GetBounds();
    return AABB::New(Vec2::New(tl.x, br.y), br + Vec2::New(0.0F, 1.0F));
}

void Entity::SetGrounded(const Stage& stage) {
    const AABB feet = GetFeet();
    if (feet.br.y >= static_cast<float>(stage.GetHeight())) {
        grounded |= true;
        return;
    }

    const IVec2 feet_tl_tile_pos = ToIVec2(feet.tl) / static_cast<int>(kTileSize);
    const IVec2 feet_br_tile_pos = ToIVec2(feet.br) / static_cast<int>(kTileSize);
    const std::vector<const Tile*> tiles_at_feet =
        stage.GetTilesInRect(feet_tl_tile_pos, feet_br_tile_pos);
    grounded |= CollidableTileInList(tiles_at_feet);
}

std::tuple<Vec2, Vec2> Entity::GetTlAndTrCorners() const {
    switch (origin) {
    case Origin::TopLeft:
        return {Vec2::New(pos.x, pos.y), Vec2::New(pos.x + size.x, pos.y)};
    case Origin::Center:
        return {Vec2::New(pos.x - size.x / 2.0F, pos.y - size.y / 2.0F),
                Vec2::New(pos.x + size.x / 2.0F, pos.y - size.y / 2.0F)};
    case Origin::Foot:
        return {Vec2::New(pos.x - size.x / 2.0F, pos.y - size.y),
                Vec2::New(pos.x + size.x / 2.0F, pos.y - size.y)};
    }

    return {pos, pos};
}

HangHands Entity::GetHangHands() const {
    const auto [tl, tr] = GetTlAndTrCorners();
    HangHands hang_hands;
    hang_hands.left = tl;
    hang_hands.right = tr;
    return hang_hands;
}

HangHandBounds Entity::GetHangHandsBounds() const {
    const auto [tl, _br] = GetBounds();
    const Vec2 right_edge = tl + Vec2::New(size.x, 0.0F);
    HangHandBounds hang_hands;
    hang_hands.left_tl = tl - kHangHandSize;
    hang_hands.left_br = tl;
    hang_hands.right_tl = right_edge - Vec2::New(0.0F, kHangHandSize.y);
    hang_hands.right_br = right_edge + Vec2::New(kHangHandSize.x, 0.0F);
    return hang_hands;
}

bool TrySetAnimation(Entity& entity, EntityDisplayState display_state) {
    const auto selection = GetFrameDataSelectionForDisplayState(EntityDisplayInput{
        .type_ = entity.type_,
        .display_state = display_state,
    });
    if (!selection.has_value()) {
        return false;
    }

    SetAnimation(entity, selection->animation_id);
    entity.frame_data_animator.animate = selection->animate;
    if (selection->has_forced_frame) {
        entity.frame_data_animator.SetForcedFrame(selection->forced_frame);
    }
    return true;
}

void SetAnimation(Entity& entity, FrameDataId animation_id) {
    entity.frame_data_animator.SetAnimation(animation_id);
}

const char* PassiveItemToString(EntityPassiveItem passive_item) {
    switch (passive_item) {
    case EntityPassiveItem::Gloves:
        return "Gloves";
    case EntityPassiveItem::Spectacles:
        return "Spectacles";
    case EntityPassiveItem::Compass:
        return "Compass";
    case EntityPassiveItem::Mitt:
        return "Mitt";
    case EntityPassiveItem::Paste:
        return "Paste";
    case EntityPassiveItem::SpringShoes:
        return "SpringShoes";
    case EntityPassiveItem::SpikeShoes:
        return "SpikeShoes";
    case EntityPassiveItem::Count:
        return "Count";
    }

    return "Unknown";
}

bool HasPassiveItem(const Entity& entity, EntityPassiveItem passive_item) {
    return (entity.passive_item_flags & PassiveItemBit(passive_item)) != 0;
}

void SetPassiveItem(Entity& entity, EntityPassiveItem passive_item, bool enabled) {
    if (enabled) {
        entity.passive_item_flags |= PassiveItemBit(passive_item);
        return;
    }

    entity.passive_item_flags &= ~PassiveItemBit(passive_item);
}

bool TryCollectPassiveItem(Entity& entity, EntityType pickup_type) {
    const std::optional<EntityPassiveItem> passive_item =
        GetEntityArchetype(pickup_type).passive_item;
    if (!passive_item.has_value()) {
        return false;
    }

    SetPassiveItem(entity, *passive_item, true);
    return true;
}

bool CanRevealEmbeddedTreasure(const Entity& entity) {
    return HasPassiveItem(entity, EntityPassiveItem::Spectacles);
}

void EnableStone(Entity& entity) {
    entity.stone = true;
    entity.crusher_pusher = true;
    entity.impassable = true;
    entity.damage_vulnerability = DamageVulnerability::ExplosionOnly;
}

void DisableStone(Entity& entity) {
    const StoneBaseFields base_fields = BuildStoneBaseFieldsForEntityType(entity.type_);
    entity.stone = false;
    entity.crusher_pusher = base_fields.crusher_pusher;
    entity.impassable = base_fields.impassable;
    entity.damage_vulnerability = base_fields.damage_vulnerability;
}

} // namespace splonks
