#include "entity.hpp"

namespace splonks {

Entity Entity::New() {
    Entity entity;
    entity.active = false;
    entity.marked_for_destruction = false;
    entity.type_ = EntityType::None;
    entity.vid = VID{0, 0};
    entity.was_horizontally_controlled_this_frame = false;
    entity.has_physics = true;
    entity.can_collide = true;
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
    entity.display_state = EntityDisplayState::Neutral;
    entity.sprite_animator = SpriteAnimator{};
    entity.jump_delay_frame_count = kJumpDelayFrames;
    entity.jumping = false;
    entity.jumped_this_frame = false;
    entity.left_hanging = false;
    entity.right_hanging = false;
    entity.no_hang = false;
    entity.running = false;
    entity.holding = false;
    entity.climbing = false;
    entity.trying_to_jump = false;
    entity.trying_to_use = false;
    entity.trying_to_go_left = false;
    entity.trying_to_go_right = false;
    entity.trying_to_equip = false;
    entity.trying_pick_up_drop = false;
    entity.trying_to_go_up = false;
    entity.trying_to_go_down = false;
    entity.trying_to_bomb = false;
    entity.trying_to_rope = false;
    entity.trying_to_attack = false;
    entity.money = 0;
    entity.bombs = 4;
    entity.ropes = 4;
    entity.travel_sound_countdown = kTravelSoundDistInterval;
    entity.travel_sound = TravelSound::One;
    entity.super_state = EntitySuperState::Idle;
    entity.last_super_state = EntitySuperState::Idle;
    entity.state = EntityState::Idle;
    entity.health = 0;
    entity.last_health = 0;
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
    entity.collided_trigger_cooldown = 0;
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
    return left_hanging != right_hanging;
}

bool Entity::IsHorizontallyControlled() const {
    return trying_to_go_left || trying_to_go_right;
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

bool CanGoOnBack(EntityType type_) {
    switch (type_) {
    case EntityType::JetPack:
        return true;
    default:
        return false;
    }
}

} // namespace splonks
