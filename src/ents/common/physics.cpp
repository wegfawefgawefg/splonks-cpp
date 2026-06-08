#include "ents/common/common.hpp"
#include "world_query.hpp"

#include "sim/fxp.hpp"
#include "tile.hpp"
#include "tile_spec.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace splonks::ents::common {

namespace {

bool VidLess(const VID& left, const VID& right) {
    if (left.id != right.id) {
        return left.id < right.id;
    }
    return left.version < right.version;
}

sim::AABB GetAabbAtPosition(const Ent& ent, sim::Vec2 pos) {
    return sim::AABB::from_pos_size(pos, ent.size - sim::Vec2::from_pixels(1, 1));
}

void StoreDistanceTraveled(std::size_t ent_idx, State& state, sim::Vec2 start_pos) {
    Ent& ent = state.ents.ents[ent_idx];
    sim::Scalar dist_traveled = gfxp::manhattan_length(ent.pos - start_pos);
    if (dist_traveled < sim::Scalar::from_int(1)) {
        dist_traveled = sim::Scalar::zero();
    }
    ent.dist_traveled_this_frame = dist_traveled;
}

void ResolveBlockingOverlap(
    std::size_t ent_idx,
    State& state,
    bool check_tiles,
    bool check_ents
) {
    Ent& ent = state.ents.ents[ent_idx];
    const sim::AABB current_aabb = ent.GetSimAABB();
    const BlockingContactSet current_contacts =
        GatherBlockingContactsForAabb(ent_idx, current_aabb, state, check_tiles, check_ents);
    if (!ResolveBlockingContactSet(ent_idx, current_contacts, state).blocks_movement) {
        return;
    }

    const int max_push = static_cast<int>(kTileSize) * 2;
    const std::array<IVec2, 4> candidates = {
        IVec2::New(0, -1),
        IVec2::New(-1, 0),
        IVec2::New(1, 0),
        IVec2::New(0, 1),
    };

    for (int distance = 1; distance <= max_push; ++distance) {
        for (const IVec2& direction : candidates) {
            const sim::Vec2 candidate_pos =
                ent.pos + sim::Vec2::from_pixels(direction.x * distance, direction.y * distance);
            const sim::AABB candidate_aabb = GetAabbAtPosition(ent, candidate_pos);
            const BlockingContactSet candidate_contacts = GatherBlockingContactsForAabb(
                ent_idx, candidate_aabb, state, check_tiles, check_ents);
            if (!ResolveBlockingContactSet(ent_idx, candidate_contacts, state).blocks_movement) {
                ent.pos = candidate_pos;
                return;
            }
        }
    }
}

bool HasBlockingTileContact(const BlockingContactSet& contacts) {
    for (const TileContact& tile_contact : contacts.tile_contacts) {
        if (tile_contact.blocks_movement) {
            return true;
        }
    }
    return false;
}

BlockingImpactSurface GetImpactSurfaceForBlockedContacts(const BlockingContactSet& contacts) {
    if (HasBlockingTileContact(contacts)) {
        return BlockingImpactSurface::Tiles;
    }
    if (contacts.touches_stage_bounds) {
        return BlockingImpactSurface::StageBounds;
    }
    return BlockingImpactSurface::ImpassableEnt;
}

sim::AABB GetTileAabbForContact(const TileContact& tile_contact, const Stage& stage, sim::Vec2 anchor) {
    const sim::Vec2 tile_tl =
        sim::Vec2::from_pixels(tile_contact.tile_pos.x * static_cast<int>(kTileSize),
                               tile_contact.tile_pos.y * static_cast<int>(kTileSize));
    return GetNearestWorldAabb(
        stage,
        anchor,
        sim::AABB::from_corners(
            tile_tl,
            tile_tl + sim::Vec2::from_pixels(static_cast<int>(kTileSize - 1),
                                             static_cast<int>(kTileSize - 1)))
    );
}

bool TrySnapToDownwardBlockingSurface(
    Ent& ent,
    const BlockingContactSet& contacts,
    const Stage& stage,
    sim::AABB current_aabb
) {
    const sim::Scalar next_bottom = current_aabb.br.y + sim::Scalar::from_int(1);

    if (contacts.touches_stage_bounds &&
        stage.IsBorderSideBlocking(StageBorderSideKind::Bottom) &&
        next_bottom > sim::Scalar::from_int(static_cast<std::int32_t>(stage.GetHeight() - 1))) {
        ent.pos.y = sim::Scalar::from_int(static_cast<std::int32_t>(stage.GetHeight())) - ent.size.y;
        return true;
    }

    std::optional<sim::Scalar> nearest_floor_top;
    const sim::Vec2 anchor = current_aabb.center();
    for (const TileContact& tile_contact : contacts.tile_contacts) {
        if (!tile_contact.blocks_movement) {
            continue;
        }

        const sim::AABB tile_aabb = GetTileAabbForContact(tile_contact, stage, anchor);
        if (tile_aabb.tl.y < current_aabb.br.y) {
            continue;
        }
        if (tile_aabb.tl.y > next_bottom) {
            continue;
        }

        if (!nearest_floor_top.has_value() || tile_aabb.tl.y < *nearest_floor_top) {
            nearest_floor_top = tile_aabb.tl.y;
        }
    }

    if (!nearest_floor_top.has_value()) {
        return false;
    }

    ent.pos.y = *nearest_floor_top - ent.size.y;
    return true;
}

bool DoesOneWayTopContactBlock(
    const Ent& ent,
    const TileContact& tile_contact,
    const Stage& stage,
    sim::AABB current_aabb,
    sim::AABB next_aabb,
    BlockingImpactAxis impact_axis,
    int direction
) {
    if (tile_contact.tile == nullptr || !IsTileOneWayTopSolid(*tile_contact.tile)) {
        return false;
    }
    if (impact_axis != BlockingImpactAxis::Vertical || direction <= 0) {
        return false;
    }
    if (ent.IsClimbing()) {
        return false;
    }

    const sim::AABB tile_aabb = GetTileAabbForContact(tile_contact, stage, next_aabb.center());
    if (current_aabb.br.y >= tile_aabb.tl.y) {
        return false;
    }
    if (next_aabb.br.y < tile_aabb.tl.y) {
        return false;
    }
    return next_aabb.br.x >= tile_aabb.tl.x && next_aabb.tl.x <= tile_aabb.br.x;
}

BlockingContactSet GatherBlockingContactsForMovement(
    std::size_t ent_idx,
    sim::AABB current_aabb,
    sim::AABB next_aabb,
    State& state,
    bool check_tiles,
    bool check_ents,
    BlockingImpactAxis impact_axis,
    int direction
) {
    BlockingContactSet contacts =
        GatherBlockingContactsForAabb(ent_idx, next_aabb, state, check_tiles, check_ents);
    if (!check_tiles) {
        return contacts;
    }

    const Ent& ent = state.ents.ents[ent_idx];
    for (TileContact& tile_contact : contacts.tile_contacts) {
        if (DoesOneWayTopContactBlock(
                ent, tile_contact, state.stage, current_aabb, next_aabb, impact_axis, direction)) {
            tile_contact.blocks_movement = true;
        }
    }
    return contacts;
}

int GetIntegerStepDistance(sim::Scalar distance, unsigned int time) {
    const sim::Scalar abs_distance = distance.abs();
    int integer_distance = abs_distance.floor_int();
    const sim::Scalar fractional_distance =
        abs_distance - sim::Scalar::from_int(integer_distance);
    if (fractional_distance != sim::Scalar::zero()) {
        const sim::Scalar reciprocal = sim::Scalar::from_int(1) / fractional_distance;
        const int fractional_period = reciprocal.round_int();
        if (fractional_period != 0 &&
            (time % static_cast<unsigned int>(fractional_period)) == 0U) {
            integer_distance += 1;
        }
    }
    if (distance < sim::Scalar::zero()) {
        integer_distance *= -1;
    }
    return integer_distance;
}

int FloorDiv(int value, int divisor) {
    const int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        return quotient - 1;
    }
    return quotient;
}

float GetGroundFrictionMultiplier(std::size_t ent_idx, State& state) {
    constexpr float kDefaultGroundFriction = 0.85F;
    const Ent& ent = state.ents.ents[ent_idx];
    const sim::AABB ent_aabb = ent.GetSimAABB();
    const int support_y = (ent_aabb.br.y + sim::Scalar::from_int(1)).to_pixels_floor();
    const int min_tile_x = FloorDiv(ent_aabb.tl.x.to_pixels_floor(), static_cast<int>(kTileSize));
    const int max_tile_x = FloorDiv(ent_aabb.br.x.to_pixels_floor(), static_cast<int>(kTileSize));
    const int support_tile_y = FloorDiv(support_y, static_cast<int>(kTileSize));

    float friction = 0.0F;
    bool found_support_surface = false;
    for (int tile_x = min_tile_x; tile_x <= max_tile_x; ++tile_x) {
        const Tile tile = state.stage.GetTileOrBorder(tile_x, support_tile_y);
        if (!IsTileGroundSupport(tile)) {
            continue;
        }
        if (IsTileOneWayTopSolid(tile) &&
            ent_aabb.br.y >= sim::Scalar::from_int(support_tile_y * static_cast<int>(kTileSize))) {
            continue;
        }
        const float tile_friction = GetTileFriction(tile);
        friction = found_support_surface ? std::min(friction, tile_friction) : tile_friction;
        found_support_surface = true;
    }

    const VID vid = ent.vid;
    const sim::AABB feet_aabb = {
        .tl = sim::Vec2{ent_aabb.tl.x, ent_aabb.br.y},
        .br = ent_aabb.br + sim::PixelVec2(0, 1),
    };
    for (const VID& other_vid : QueryEntsInAabb(state, feet_aabb, vid)) {
        const Ent* const other = state.ents.GetEnt(other_vid);
        if (other == nullptr || !other->active || !other->impassable) {
            continue;
        }

        const sim::AABB other_aabb =
            GetNearestWorldAabb(state.stage, ent_aabb.center(), other->GetSimAABB());
        if (!gfxp::aabbs_intersect(feet_aabb, other_aabb)) {
            continue;
        }

        const float other_friction = sim::ToRenderScalar(other->support_ground_friction);
        friction = found_support_surface ? std::min(friction, other_friction) : other_friction;
        found_support_surface = true;
    }

    return found_support_surface ? friction : kDefaultGroundFriction;
}

bool DispatchPostSweepEntOverlapContacts(
    std::size_t ent_idx,
    State& state,
    Graphics* graphics,
    Audio* audio,
    BlockingImpactAxis impact_axis,
    int direction
) {
    if (graphics == nullptr || audio == nullptr) {
        return false;
    }
    return TryDispatchEntEntOverlapContacts(
        ent_idx,
        state,
        *graphics,
        *audio,
        ContactContext{
            .phase = ContactPhase::SweptEntered,
            .has_impact = false,
            .impact_axis = impact_axis,
            .direction = direction,
            .mover_vid = state.ents.ents[ent_idx].vid,
        }
    );
}

sim::AABB GetTopCarryStrip(const Ent& ent) {
    const sim::AABB aabb = ent.GetSimAABB();
    return sim::AABB{
        .tl = sim::Vec2{aabb.tl.x, aabb.tl.y - sim::Scalar::from_int(1)},
        .br = sim::Vec2{aabb.br.x, aabb.tl.y - sim::Scalar::from_int(1)},
    };
}

sim::AABB GetTopCarryQueryArea(const Ent& ent, const IVec2& direction) {
    const sim::AABB carry_strip = GetTopCarryStrip(ent);
    if (direction.y > 0) {
        return sim::AABB{
            .tl = sim::Vec2{carry_strip.tl.x, carry_strip.tl.y - sim::Scalar::from_int(1)},
            .br = carry_strip.br,
        };
    }
    return carry_strip;
}

bool IsCarryTargetOnTopOfMover(
    const Ent& mover,
    const Ent& target,
    const Stage& stage
) {
    if (!target.active || !target.can_collide || target.impassable) {
        return false;
    }
    if (target.held_by_vid.has_value() || target.attach_mode != AttachMode::None) {
        return false;
    }

    const sim::AABB carry_strip = GetTopCarryStrip(mover);
    const sim::AABB target_feet =
        GetNearestWorldAabb(stage, mover.GetSimCenter(), target.GetSimFeet());
    if (!gfxp::aabbs_intersect(carry_strip, target_feet)) {
        return false;
    }

    const sim::Scalar overlap_x =
        std::min(carry_strip.br.x, target_feet.br.x) -
        std::max(carry_strip.tl.x, target_feet.tl.x);
    return overlap_x > sim::Scalar::zero();
}

sim::AABB GetHangCarryStripForMoverSide(const Ent& mover, Side mover_side) {
    const sim::AABB aabb = mover.GetSimAABB();
    if (mover_side == Side::Right) {
        const sim::Scalar x = aabb.br.x + sim::Scalar::from_int(1);
        return sim::AABB{
            .tl = sim::Vec2{x, aabb.tl.y},
            .br = sim::Vec2{x, aabb.br.y},
        };
    }
    const sim::Scalar x = aabb.tl.x - sim::Scalar::from_int(1);
    return sim::AABB{
        .tl = sim::Vec2{x, aabb.tl.y},
        .br = sim::Vec2{x, aabb.br.y},
    };
}

bool IsHangCarryTargetOnMoverSide(
    const Ent& mover,
    const Ent& target,
    const Stage& stage,
    Side mover_side
) {
    if (!target.active || !target.can_collide || target.impassable || !target.IsHanging()) {
        return false;
    }
    if (target.held_by_vid.has_value() || target.attach_mode != AttachMode::None) {
        return false;
    }

    if (mover_side == Side::Left) {
        if (target.hang_side != Side::Right) {
            return false;
        }
    } else {
        if (target.hang_side != Side::Left) {
            return false;
        }
    }

    const sim::AABB mover_aabb = mover.GetSimAABB();
    const sim::AABB target_aabb =
        GetNearestWorldAabb(stage, mover.GetSimCenter(), target.GetSimAABB());
    const sim::Scalar overlap_y =
        std::min(mover_aabb.br.y, target_aabb.br.y) -
        std::max(mover_aabb.tl.y, target_aabb.tl.y);
    if (overlap_y <= sim::Scalar::zero()) {
        return false;
    }

    if (mover_side == Side::Right) {
        return target_aabb.tl.x == mover_aabb.br.x + sim::Scalar::from_int(1);
    }
    return target_aabb.br.x == mover_aabb.tl.x - sim::Scalar::from_int(1);
}

void AppendHangCarryTargetsOnMoverSide(
    std::size_t mover_idx,
    Side mover_side,
    State& state,
    std::vector<VID>& hanger_vids
) {
    if (mover_idx >= state.ents.ents.size()) {
        return;
    }

    const Ent& mover = state.ents.ents[mover_idx];
    if (!mover.active || !mover.impassable) {
        return;
    }

    for (const VID& vid : QueryEntsInAabb(
             state,
             GetHangCarryStripForMoverSide(mover, mover_side),
             mover.vid)) {
        const Ent* const target = state.ents.GetEnt(vid);
        if (target == nullptr) {
            continue;
        }
        if (!IsHangCarryTargetOnMoverSide(mover, *target, state.stage, mover_side)) {
            continue;
        }
        if (std::find(hanger_vids.begin(), hanger_vids.end(), vid) == hanger_vids.end()) {
            hanger_vids.push_back(vid);
        }
    }
}

void TryCarryEntsOnTopByOnePixel(
    std::size_t mover_idx,
    const IVec2& direction,
    State& state,
    const Graphics& graphics,
    Audio* audio
) {
    if (direction.x == 0 && direction.y <= 0) {
        return;
    }
    if (mover_idx >= state.ents.ents.size()) {
        return;
    }

    const Ent& mover = state.ents.ents[mover_idx];
    if (!mover.active || !mover.impassable) {
        return;
    }

    std::vector<VID> rider_vids;
    for (const VID& vid : QueryEntsInAabb(
             state,
             GetTopCarryQueryArea(mover, direction),
             mover.vid)) {
        const Ent* const target = state.ents.GetEnt(vid);
        if (target == nullptr) {
            continue;
        }
        if (!IsCarryTargetOnTopOfMover(mover, *target, state.stage)) {
            continue;
        }
        rider_vids.push_back(vid);
    }

    std::sort(
        rider_vids.begin(),
        rider_vids.end(),
        [&](const VID& lhs, const VID& rhs) {
            const Ent* const left = state.ents.GetEnt(lhs);
            const Ent* const right = state.ents.GetEnt(rhs);
            if (left == nullptr || right == nullptr) {
                return false;
            }

            const sim::Scalar left_x =
                GetNearestWorldAabb(state.stage, mover.GetSimCenter(), left->GetSimAABB()).tl.x;
            const sim::Scalar right_x =
                GetNearestWorldAabb(state.stage, mover.GetSimCenter(), right->GetSimAABB()).tl.x;
            if (direction.x > 0) {
                if (left_x != right_x) {
                    return left_x > right_x;
                }
                return VidLess(lhs, rhs);
            }
            if (direction.x < 0) {
                if (left_x != right_x) {
                    return left_x < right_x;
                }
                return VidLess(lhs, rhs);
            }
            return VidLess(lhs, rhs);
        }
    );

    for (const VID& rider_vid : rider_vids) {
        if (!TryDisplaceEntByOnePixel(rider_vid.id, direction, state, graphics, audio)) {
            continue;
        }
        SyncEntAttachs(rider_vid.id, state, graphics);
    }
}

void MoveEntPixelStep(
    std::size_t ent_idx,
    State& state,
    bool check_tiles,
    bool check_ents,
    Graphics* graphics,
    Audio* audio
) {
    Ent& ent = state.ents.ents[ent_idx];
    const sim::Vec2 start_pos = ent.pos;

    ResolveBlockingOverlap(ent_idx, state, check_tiles, check_ents);

    const int move_x = GetIntegerStepDistance(ent.vel.x, state.stage_frame);
    const int move_y = GetIntegerStepDistance(ent.vel.y, state.stage_frame);

    if (move_x > 0) {
        for (int i = 0; i < move_x; ++i) {
            std::vector<VID> hanging_carry_vids;
            AppendHangCarryTargetsOnMoverSide(
                ent_idx,
                Side::Left,
                state,
                hanging_carry_vids
            );
            const sim::Vec2 next_pos = ent.pos + sim::Vec2::from_pixels(1, 0);
            const sim::AABB current_aabb = ent.GetSimAABB();
            const sim::AABB next_aabb = GetAabbAtPosition(ent, next_pos);
            const BlockingContactSet contacts = GatherBlockingContactsForMovement(
                ent_idx, current_aabb, next_aabb, state, check_tiles, check_ents,
                BlockingImpactAxis::Horizontal, 1);
            const ContactResult contact_result =
                ResolveBlockingContactSet(ent_idx, contacts, state);
            if (contact_result.stop_sweep) {
                StoreDistanceTraveled(ent_idx, state, start_pos);
                return;
            }
            if (contact_result.blocks_movement) {
                const ContactContext blocked_context = {
                    .phase = ContactPhase::AttemptedBlocked,
                    .has_impact = true,
                    .impact_axis = BlockingImpactAxis::Horizontal,
                    .impact_surface = GetImpactSurfaceForBlockedContacts(contacts),
                    .impact_velocity = sim::ToRenderScalar(ent.vel.x),
                    .direction = 1,
                    .mover_vid = ent.vid,
                };
                const ContactResult ent_resolution = TryDispatchEntEntContacts(
                    ent_idx,
                    contacts.ent_vids,
                    blocked_context,
                    state,
                    graphics,
                    audio
                );
                if (ent_resolution.stop_sweep) {
                    ent.collided = true;
                    ent.vel.x = sim::Scalar::zero();
                    StoreDistanceTraveled(ent_idx, state, start_pos);
                    return;
                }
                const ContactResult tile_resolution =
                    TryDispatchEntTileContacts(ent_idx, contacts, blocked_context, state, audio);
                if (tile_resolution.stop_sweep || ent_resolution.stop_sweep) {
                    ent.collided = true;
                    ent.vel.x = sim::Scalar::zero();
                    StoreDistanceTraveled(ent_idx, state, start_pos);
                    return;
                }
                ent.vel.x = sim::Scalar::zero();
                ent.collided = true;
                break;
            }
            ent.pos = next_pos;
            if (graphics != nullptr) {
                for (const VID& hanging_vid : hanging_carry_vids) {
                    if (!TryDisplaceEntByOnePixel(
                            hanging_vid.id,
                            IVec2::New(1, 0),
                            state,
                            *graphics,
                            audio)) {
                        continue;
                    }
                    SyncEntAttachs(hanging_vid.id, state, *graphics);
                }
            }
            if (graphics != nullptr) {
                TryCarryEntsOnTopByOnePixel(
                    ent_idx,
                    IVec2::New(1, 0),
                    state,
                    *graphics,
                    audio
                );
            }
            if (DispatchPostSweepEntOverlapContacts(
                    ent_idx,
                    state,
                    graphics,
                    audio,
                    BlockingImpactAxis::Horizontal,
                    1)) {
                StoreDistanceTraveled(ent_idx, state, start_pos);
                return;
            }
        }
    } else if (move_x < 0) {
        for (int i = 0; i < -move_x; ++i) {
            std::vector<VID> hanging_carry_vids;
            AppendHangCarryTargetsOnMoverSide(
                ent_idx,
                Side::Right,
                state,
                hanging_carry_vids
            );
            const sim::Vec2 next_pos = ent.pos + sim::Vec2::from_pixels(-1, 0);
            const sim::AABB current_aabb = ent.GetSimAABB();
            const sim::AABB next_aabb = GetAabbAtPosition(ent, next_pos);
            const BlockingContactSet contacts = GatherBlockingContactsForMovement(
                ent_idx, current_aabb, next_aabb, state, check_tiles, check_ents,
                BlockingImpactAxis::Horizontal, -1);
            const ContactResult contact_result =
                ResolveBlockingContactSet(ent_idx, contacts, state);
            if (contact_result.stop_sweep) {
                StoreDistanceTraveled(ent_idx, state, start_pos);
                return;
            }
            if (contact_result.blocks_movement) {
                const ContactContext blocked_context = {
                    .phase = ContactPhase::AttemptedBlocked,
                    .has_impact = true,
                    .impact_axis = BlockingImpactAxis::Horizontal,
                    .impact_surface = GetImpactSurfaceForBlockedContacts(contacts),
                    .impact_velocity = sim::ToRenderScalar(ent.vel.x),
                    .direction = -1,
                    .mover_vid = ent.vid,
                };
                const ContactResult ent_resolution = TryDispatchEntEntContacts(
                    ent_idx,
                    contacts.ent_vids,
                    blocked_context,
                    state,
                    graphics,
                    audio
                );
                if (ent_resolution.stop_sweep) {
                    ent.collided = true;
                    ent.vel.x = sim::Scalar::zero();
                    StoreDistanceTraveled(ent_idx, state, start_pos);
                    return;
                }
                const ContactResult tile_resolution =
                    TryDispatchEntTileContacts(ent_idx, contacts, blocked_context, state, audio);
                if (tile_resolution.stop_sweep || ent_resolution.stop_sweep) {
                    ent.collided = true;
                    ent.vel.x = sim::Scalar::zero();
                    StoreDistanceTraveled(ent_idx, state, start_pos);
                    return;
                }
                ent.vel.x = sim::Scalar::zero();
                ent.collided = true;
                break;
            }
            ent.pos = next_pos;
            if (graphics != nullptr) {
                for (const VID& hanging_vid : hanging_carry_vids) {
                    if (!TryDisplaceEntByOnePixel(
                            hanging_vid.id,
                            IVec2::New(-1, 0),
                            state,
                            *graphics,
                            audio)) {
                        continue;
                    }
                    SyncEntAttachs(hanging_vid.id, state, *graphics);
                }
            }
            if (graphics != nullptr) {
                TryCarryEntsOnTopByOnePixel(
                    ent_idx,
                    IVec2::New(-1, 0),
                    state,
                    *graphics,
                    audio
                );
            }
            if (DispatchPostSweepEntOverlapContacts(
                    ent_idx,
                    state,
                    graphics,
                    audio,
                    BlockingImpactAxis::Horizontal,
                    -1)) {
                StoreDistanceTraveled(ent_idx, state, start_pos);
                return;
            }
        }
    }

    if (move_y > 0) {
        for (int i = 0; i < move_y; ++i) {
            std::vector<VID> hanging_carry_vids;
            AppendHangCarryTargetsOnMoverSide(
                ent_idx,
                Side::Left,
                state,
                hanging_carry_vids
            );
            AppendHangCarryTargetsOnMoverSide(
                ent_idx,
                Side::Right,
                state,
                hanging_carry_vids
            );
            const sim::Vec2 next_pos = ent.pos + sim::Vec2::from_pixels(0, 1);
            const sim::AABB current_aabb = ent.GetSimAABB();
            const sim::AABB next_aabb = GetAabbAtPosition(ent, next_pos);
            const BlockingContactSet contacts = GatherBlockingContactsForMovement(
                ent_idx, current_aabb, next_aabb, state, check_tiles, check_ents,
                BlockingImpactAxis::Vertical, 1);
            const ContactResult contact_result =
                ResolveBlockingContactSet(ent_idx, contacts, state);
            if (contact_result.stop_sweep) {
                StoreDistanceTraveled(ent_idx, state, start_pos);
                return;
            }
            if (contact_result.blocks_movement) {
                const ContactContext blocked_context = {
                    .phase = ContactPhase::AttemptedBlocked,
                    .has_impact = true,
                    .impact_axis = BlockingImpactAxis::Vertical,
                    .impact_surface = GetImpactSurfaceForBlockedContacts(contacts),
                    .impact_velocity = sim::ToRenderScalar(ent.vel.y),
                    .direction = 1,
                    .mover_vid = ent.vid,
                };
                const ContactResult ent_resolution = TryDispatchEntEntContacts(
                    ent_idx,
                    contacts.ent_vids,
                    blocked_context,
                    state,
                    graphics,
                    audio
                );
                if (ent_resolution.stop_sweep) {
                    ent.collided = true;
                    ent.vel.y = sim::Scalar::zero();
                    (void)TrySnapToDownwardBlockingSurface(ent, contacts, state.stage, current_aabb);
                    StoreDistanceTraveled(ent_idx, state, start_pos);
                    return;
                }
                const ContactResult tile_resolution =
                    TryDispatchEntTileContacts(ent_idx, contacts, blocked_context, state, audio);
                if (tile_resolution.stop_sweep || ent_resolution.stop_sweep) {
                    ent.collided = true;
                    ent.vel.y = sim::Scalar::zero();
                    (void)TrySnapToDownwardBlockingSurface(ent, contacts, state.stage, current_aabb);
                    StoreDistanceTraveled(ent_idx, state, start_pos);
                    return;
                }
                (void)TrySnapToDownwardBlockingSurface(ent, contacts, state.stage, current_aabb);
                ent.vel.y = sim::Scalar::zero();
                ent.collided = true;
                break;
            }
            ent.pos = next_pos;
            if (graphics != nullptr) {
                for (const VID& hanging_vid : hanging_carry_vids) {
                    if (!TryDisplaceEntByOnePixel(
                            hanging_vid.id,
                            IVec2::New(0, 1),
                            state,
                            *graphics,
                            audio)) {
                        continue;
                    }
                    SyncEntAttachs(hanging_vid.id, state, *graphics);
                }
            }
            if (graphics != nullptr) {
                TryCarryEntsOnTopByOnePixel(
                    ent_idx,
                    IVec2::New(0, 1),
                    state,
                    *graphics,
                    audio
                );
            }
            if (DispatchPostSweepEntOverlapContacts(
                    ent_idx,
                    state,
                    graphics,
                    audio,
                    BlockingImpactAxis::Vertical,
                    1)) {
                StoreDistanceTraveled(ent_idx, state, start_pos);
                return;
            }
        }
    } else if (move_y < 0) {
        for (int i = 0; i < -move_y; ++i) {
            std::vector<VID> hanging_carry_vids;
            AppendHangCarryTargetsOnMoverSide(
                ent_idx,
                Side::Left,
                state,
                hanging_carry_vids
            );
            AppendHangCarryTargetsOnMoverSide(
                ent_idx,
                Side::Right,
                state,
                hanging_carry_vids
            );
            const sim::Vec2 next_pos = ent.pos + sim::Vec2::from_pixels(0, -1);
            const sim::AABB current_aabb = ent.GetSimAABB();
            const sim::AABB next_aabb = GetAabbAtPosition(ent, next_pos);
            const BlockingContactSet contacts = GatherBlockingContactsForMovement(
                ent_idx, current_aabb, next_aabb, state, check_tiles, check_ents,
                BlockingImpactAxis::Vertical, -1);
            const ContactResult contact_result =
                ResolveBlockingContactSet(ent_idx, contacts, state);
            if (contact_result.stop_sweep) {
                StoreDistanceTraveled(ent_idx, state, start_pos);
                return;
            }
            if (contact_result.blocks_movement) {
                const ContactContext blocked_context = {
                    .phase = ContactPhase::AttemptedBlocked,
                    .has_impact = true,
                    .impact_axis = BlockingImpactAxis::Vertical,
                    .impact_surface = GetImpactSurfaceForBlockedContacts(contacts),
                    .impact_velocity = sim::ToRenderScalar(ent.vel.y),
                    .direction = -1,
                    .mover_vid = ent.vid,
                };
                const ContactResult ent_resolution = TryDispatchEntEntContacts(
                    ent_idx,
                    contacts.ent_vids,
                    blocked_context,
                    state,
                    graphics,
                    audio
                );
                if (ent_resolution.stop_sweep) {
                    ent.collided = true;
                    ent.vel.y = sim::Scalar::zero();
                    StoreDistanceTraveled(ent_idx, state, start_pos);
                    return;
                }
                const ContactResult tile_resolution =
                    TryDispatchEntTileContacts(ent_idx, contacts, blocked_context, state, audio);
                if (tile_resolution.stop_sweep || ent_resolution.stop_sweep) {
                    ent.collided = true;
                    ent.vel.y = sim::Scalar::zero();
                    StoreDistanceTraveled(ent_idx, state, start_pos);
                    return;
                }
                ent.vel.y = sim::Scalar::zero();
                ent.collided = true;
                break;
            }
            ent.pos = next_pos;
            if (graphics != nullptr) {
                for (const VID& hanging_vid : hanging_carry_vids) {
                    if (!TryDisplaceEntByOnePixel(
                            hanging_vid.id,
                            IVec2::New(0, -1),
                            state,
                            *graphics,
                            audio)) {
                        continue;
                    }
                    SyncEntAttachs(hanging_vid.id, state, *graphics);
                }
            }
            if (DispatchPostSweepEntOverlapContacts(
                    ent_idx,
                    state,
                    graphics,
                    audio,
                    BlockingImpactAxis::Vertical,
                    -1)) {
                StoreDistanceTraveled(ent_idx, state, start_pos);
                return;
            }
        }
    }

    if (DispatchPostSweepEntOverlapContacts(
            ent_idx,
            state,
            graphics,
            audio,
            BlockingImpactAxis::Horizontal,
            0)) {
        StoreDistanceTraveled(ent_idx, state, start_pos);
        return;
    }

    StoreDistanceTraveled(ent_idx, state, start_pos);
}

bool IsGroundedOnEnts(std::size_t ent_idx, State& state) {
    const Ent& ent = state.ents.ents[ent_idx];
    const sim::AABB ent_aabb = ent.GetSimAABB();
    const VID vid = ent.vid;
    const sim::AABB feet_aabb = {
        .tl = sim::Vec2{ent_aabb.tl.x, ent_aabb.br.y},
        .br = ent_aabb.br + sim::PixelVec2(0, 1),
    };
    const std::vector<VID> ents_at_feet =
        QueryEntsInAabb(state, feet_aabb, vid);

    const bool impassable_ents = std::any_of(
        ents_at_feet.begin(),
        ents_at_feet.end(),
        [&](const VID& test_vid) { return state.ents.ents[test_vid.id].impassable; });

    return impassable_ents;
}

} // namespace

void EulerStep(std::size_t ent_idx, State& state, float dt) {
    PrePartialEulerStep(ent_idx, state, dt);
    MoveEntPixelStep(ent_idx, state, false, false, nullptr, nullptr);
    PostPartialEulerStep(ent_idx, state, dt);
}

void PrePartialEulerStep(std::size_t ent_idx, State& state, float dt) {
    (void)dt;
    Ent& ent = state.ents.ents[ent_idx];
    ent.vel += ent.acc;
}

void ApplyGravity(std::size_t ent_idx, State& state, float dt) {
    (void)dt;
    Ent& ent = state.ents.ents[ent_idx];
    if (ent.grounded) {
        ApplyEffectHookToEnt(ent, state, nullptr, EffectHookContext{.type = EffectHookType::Grounded});
        if (ent.vel.y > sim::Scalar::zero()) {
            ent.vel.y = sim::Scalar::zero();
        }
        return;
    }
    if (ent.collided_last_frame) {
        ApplyEffectHookToEnt(ent, state, nullptr, EffectHookContext{.type = EffectHookType::BlockingContact});
    }
    const float gravity_scale =
        GetModifiedEffectValue(ent, EffectModifierTarget::GravityScale, 1.0F, &state);
    if (gravity_scale != 0.0F) {
        ent.acc.y += state.stage.gravity * sim::ToSimScalar(gravity_scale);
    }
    const float buoyancy_strength =
        GetModifiedEffectValue(ent, EffectModifierTarget::BuoyancyStrength, 0.0F, &state);
    const float buoyancy = sim::ToRenderScalar(ent.buoyancy);
    if (buoyancy > 0.0F && buoyancy_strength > 0.0F) {
        ent.acc.y -= state.stage.gravity * ent.buoyancy * sim::ToSimScalar(buoyancy_strength);
    }
}

void ApplyEffectVelocityModifiers(Ent& ent, const State& state) {
    const float damping_x =
        GetModifiedEffectValue(ent, EffectModifierTarget::VelocityDampingX, 1.0F, &state);
    const float damping_y =
        GetModifiedEffectValue(ent, EffectModifierTarget::VelocityDampingY, 1.0F, &state);
    ent.vel.x *= sim::ToSimScalar(std::clamp(damping_x, 0.0F, 1.0F));
    ent.vel.y *= sim::ToSimScalar(std::clamp(damping_y, 0.0F, 1.0F));

    const float max_fall_speed =
        GetModifiedEffectValue(
            ent,
            EffectModifierTarget::MaxFallSpeed,
            sim::ToRenderScalar(ent.max_speed),
            &state
        );
    ent.vel.y = std::min(ent.vel.y, sim::ToSimScalar(max_fall_speed));
}

void PostPartialEulerStep(std::size_t ent_idx, State& state, float dt) {
    (void)dt;
    Ent& ent = state.ents.ents[ent_idx];
    ApplyEffectVelocityModifiers(ent, state);
    ent.vel.x = gfxp::clamp(ent.vel.x, -ent.max_speed, ent.max_speed);
    ent.vel.y = gfxp::clamp(ent.vel.y, -ent.max_speed, ent.max_speed);
    ent.acc = sim::Vec2::zero();
}

void ApplyGroundFriction(std::size_t ent_idx, State& state) {
    ApplyGroundFriction(ent_idx, state, 1.0F);
}

void ApplyGroundFriction(std::size_t ent_idx, State& state, float friction_scale) {
    {
        Ent& ent = state.ents.ents[ent_idx];
        ent.grounded = false;
    }

    if (IsGroundedOnEnts(ent_idx, state)) {
        state.ents.ents[ent_idx].grounded |= true;
    }
    Ent& ent = state.ents.ents[ent_idx];
    ent.SetGrounded(state.stage);
    if (ent.grounded) {
        ent.vel.x *= sim::ToSimScalar(std::clamp(
            GetGroundFrictionMultiplier(ent_idx, state) * friction_scale,
            0.0F,
            1.0F
        ));
    }
}

void ApplySpecGroundFriction(std::size_t ent_idx, State& state) {
    ApplySpecGroundFriction(ent_idx, state, 1.0F);
}

void ApplySpecGroundFriction(std::size_t ent_idx, State& state, float friction_scale) {
    const Ent& ent = state.ents.ents[ent_idx];
    if (!ent.affected_by_ground_friction) {
        return;
    }
    ApplyGroundFriction(ent_idx, state, friction_scale);
}

void StepStandardPhysics(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    ApplyGravity(ent_idx, state, dt);
    PrePartialEulerStep(ent_idx, state, dt);
    DoTileAndEntCollisions(ent_idx, state, graphics, audio);
    ApplySpecGroundFriction(ent_idx, state);
    PostPartialEulerStep(ent_idx, state, dt);
}

void GroundedCheck(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    bool check_tiles,
    bool check_ents,
    std::uint32_t coyote_time_frames
) {
    (void)audio;
    bool grounded = false;
    if (check_tiles) {
        grounded |= IsGroundedOnTiles(ent_idx, state);
    }
    if (check_ents) {
        grounded |= IsGroundedOnEnts(ent_idx, state);
    }

    Ent& ent = state.ents.ents[ent_idx];

    ent.grounded = grounded;
    if (ent.grounded) {
        if (ent.vel.y > sim::Scalar::zero()) {
            ent.vel.y = sim::Scalar::zero();
        }
        ent.coyote_time = coyote_time_frames;
    } else if (ent.coyote_time > 0) {
        ent.coyote_time -= 1;
    }
}

bool IsGroundedOnTiles(std::size_t ent_idx, State& state) {
    Ent& ent = state.ents.ents[ent_idx];

    const sim::AABB feet_aabb = ent.GetSimGroundProbe();
    if (ent.TrySnapToBlockingStageBottom(state.stage)) {
        return true;
    }

    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, feet_aabb)) {
        if (tile_query.tile != nullptr &&
            (IsTileCollidable(*tile_query.tile) ||
             (!ent.IsClimbing() && IsOneWayTopTileSupportingAabb(state.stage, tile_query, feet_aabb)))) {
            return true;
        }
    }
    return false;
}

void DoTileAndEntCollisions(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    Ent& ent = state.ents.ents[ent_idx];

    ent.collided_last_frame = ent.collided;
    ent.collided = false;
    MoveEntPixelStep(ent_idx, state, true, true, &graphics, &audio);
    ent.collided |= ent.grounded;
}

void DoTileCollisions(std::size_t ent_idx, State& state) {
    Ent& ent = state.ents.ents[ent_idx];
    ent.collided_last_frame = ent.collided;
    ent.collided = false;
    MoveEntPixelStep(ent_idx, state, true, false, nullptr, nullptr);
}

void DoEntCollisions(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    Ent& ent = state.ents.ents[ent_idx];
    ent.collided_last_frame = ent.collided;
    ent.collided = false;
    MoveEntPixelStep(ent_idx, state, false, true, &graphics, &audio);
}

} // namespace splonks::ents::common
