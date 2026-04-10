#include "entity.hpp"

#include "entity_display_states.hpp"
#include "frame_data_id.hpp"
#include "entities/mod.hpp"

namespace splonks {

namespace {

struct StoneBaseFields {
    bool crusher_pusher = false;
    bool impassable = false;
    DamageVulnerability damage_vulnerability = DamageVulnerability::Vulnerable;
};

StoneBaseFields BuildStoneBaseFieldsForEntityType(EntityType type_) {
    Entity base_entity;
    entities::SetEntityByType(base_entity, type_);
    return StoneBaseFields{
        .crusher_pusher = base_entity.crusher_pusher,
        .impassable = base_entity.impassable,
        .damage_vulnerability = base_entity.damage_vulnerability,
    };
}

constexpr std::uint64_t PassiveItemBit(EntityPassiveItem passive_item) {
    return 1ULL << static_cast<unsigned int>(passive_item);
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
    TrySetDisplayState(entity, EntityDisplayState::Neutral);
    entity.frame_data_animator = FrameDataAnimator{};
    entity.jump_delay_frame_count = kJumpDelayFrames;
    entity.jumped_this_frame = false;
    entity.left_hanging = false;
    entity.right_hanging = false;
    entity.can_hang_ledge = false;
    entity.can_hang_wall = false;
    entity.hang_count = 0;
    entity.holding = false;
    entity.climbing = false;
    entity.passive_item_flags = 0;
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

bool CanGoOnBack(EntityType type_) {
    switch (type_) {
    case EntityType::JetPack:
        return true;
    default:
        return false;
    }
}

bool TrySetDisplayState(Entity& entity, EntityDisplayState display_state) {
    const auto selection = GetFrameDataSelectionForDisplayState(EntityDisplayInput{
        .type_ = entity.type_,
        .display_state = display_state,
    });
    if (!selection.has_value()) {
        return false;
    }

    entity.display_state = display_state;
    return true;
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

std::optional<EntityPassiveItem> PassiveItemForEntityType(EntityType type_) {
    switch (type_) {
    case EntityType::Gloves:
        return EntityPassiveItem::Gloves;
    case EntityType::Spectacles:
        return EntityPassiveItem::Spectacles;
    case EntityType::Compass:
        return EntityPassiveItem::Compass;
    case EntityType::Mitt:
        return EntityPassiveItem::Mitt;
    case EntityType::Paste:
        return EntityPassiveItem::Paste;
    case EntityType::SpringShoes:
        return EntityPassiveItem::SpringShoes;
    case EntityType::SpikeShoes:
        return EntityPassiveItem::SpikeShoes;
    default:
        return std::nullopt;
    }
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
    const std::optional<EntityPassiveItem> passive_item = PassiveItemForEntityType(pickup_type);
    if (!passive_item.has_value()) {
        return false;
    }

    SetPassiveItem(entity, *passive_item, true);
    return true;
}

bool CanRevealEmbeddedTreasure(const Entity& entity) {
    return HasPassiveItem(entity, EntityPassiveItem::Spectacles);
}

FrameDataId GetDefaultAnimationIdForEntityType(EntityType type_) {
    switch (type_) {
    case EntityType::Gold:
        return frame_data_ids::GoldCoin;
    case EntityType::GoldStack:
        return frame_data_ids::GoldStack;
    case EntityType::Rock:
        return frame_data_ids::Rock;
    case EntityType::JetPack:
        return frame_data_ids::Jetpack;
    case EntityType::GoldIdol:
        return frame_data_ids::GoldIdol;
    case EntityType::Mattock:
        return frame_data_ids::Mattock;
    case EntityType::Cape:
        return frame_data_ids::CapePickup;
    case EntityType::Shotgun:
        return frame_data_ids::Shotgun;
    case EntityType::Teleporter:
        return frame_data_ids::Teleporter;
    case EntityType::Gloves:
        return frame_data_ids::Gloves;
    case EntityType::Spectacles:
        return frame_data_ids::Spectacles;
    case EntityType::WebCannon:
        return frame_data_ids::WebCannon;
    case EntityType::Pistol:
        return frame_data_ids::Pistol;
    case EntityType::Mitt:
        return frame_data_ids::Mitt;
    case EntityType::Paste:
        return frame_data_ids::Paste;
    case EntityType::SpringShoes:
        return frame_data_ids::SpringShoes;
    case EntityType::SpikeShoes:
        return frame_data_ids::SpikeShoes;
    case EntityType::Machete:
        return frame_data_ids::Machete;
    case EntityType::BombBox:
        return frame_data_ids::BombBox;
    case EntityType::BombBag:
        return frame_data_ids::BombBag;
    case EntityType::Bow:
        return frame_data_ids::Bow;
    case EntityType::Compass:
        return frame_data_ids::Compass;
    case EntityType::Parachute:
        return frame_data_ids::Parachute;
    case EntityType::RopePile:
        return frame_data_ids::RopePile;
    case EntityType::EmeraldBig:
        return frame_data_ids::EmeraldBig;
    case EntityType::SapphireBig:
        return frame_data_ids::SapphireBig;
    case EntityType::RubyBig:
        return frame_data_ids::RubyBig;
    default:
        return frame_data_ids::NoSprite;
    }
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
