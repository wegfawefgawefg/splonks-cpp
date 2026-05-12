#include "network/net_entity_interpolation.hpp"

#include "entity.hpp"
#include "network/net_session.hpp"
#include "network/net_transport.hpp"
#include "state.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::network {

namespace {

float DistanceSquared(const Vec2& a, const Vec2& b) {
    const Vec2 delta = a - b;
    return (delta.x * delta.x) + (delta.y * delta.y);
}

Vec2 InterpolateTarget(
    const NetRemoteEntityRenderTarget& target,
    const NetTransportRuntime& transport,
    std::uint64_t frame
) {
    const Vec2 start = Vec2::New(target.start_pos_x, target.start_pos_y);
    const Vec2 end = Vec2::New(target.target_pos_x, target.target_pos_y);
    const float snap_distance = std::max(0.0F, transport.remote_snap_distance);
    const float snap_distance_sq = snap_distance * snap_distance;
    if (DistanceSquared(start, end) > snap_distance_sq) {
        return end;
    }

    const float delay_frames = static_cast<float>(transport.remote_interpolation_delay_frames);
    if (delay_frames <= 0.0F) {
        return end;
    }

    const float age_frames = static_cast<float>(frame - target.interpolation_start_frame);
    const float t = std::clamp(age_frames / delay_frames, 0.0F, 1.0F);
    return start + ((end - start) * t);
}

} // namespace

void SetRemoteEntityRenderTarget(
    State& state,
    NetEntityId entity_id,
    const Vec2& start_pos,
    const Vec2& target_pos
) {
    if (!state.net_transport || entity_id == kInvalidNetEntityId) {
        return;
    }

    for (NetRemoteEntityRenderTarget& target : state.net_transport->remote_entity_render_targets) {
        if (target.entity_id != entity_id) {
            continue;
        }
        target.start_pos_x = start_pos.x;
        target.start_pos_y = start_pos.y;
        target.target_pos_x = target_pos.x;
        target.target_pos_y = target_pos.y;
        target.interpolation_start_frame = state.frame;
        return;
    }

    state.net_transport->remote_entity_render_targets.push_back(NetRemoteEntityRenderTarget{
        .entity_id = entity_id,
        .start_pos_x = start_pos.x,
        .start_pos_y = start_pos.y,
        .target_pos_x = target_pos.x,
        .target_pos_y = target_pos.y,
        .interpolation_start_frame = state.frame,
    });
}

void ClearRemoteEntityRenderTarget(State& state, NetEntityId entity_id) {
    if (!state.net_transport || entity_id == kInvalidNetEntityId) {
        return;
    }

    auto& targets = state.net_transport->remote_entity_render_targets;
    targets.erase(
        std::remove_if(
            targets.begin(),
            targets.end(),
            [entity_id](const NetRemoteEntityRenderTarget& target) {
                return target.entity_id == entity_id;
            }
        ),
        targets.end()
    );
}

std::optional<Vec2> GetRemoteEntityRenderPosition(const State& state, const Entity& entity) {
    if (!state.net_transport || !entity.active) {
        return std::nullopt;
    }
    if (entity.held_by_vid.has_value() || entity.attachment_mode != AttachmentMode::None) {
        return std::nullopt;
    }
    const std::optional<NetEntityId> entity_id = state.net_session.FindNetEntityId(entity.vid);
    if (!entity_id.has_value() || IsPlayerNetEntityId(*entity_id)) {
        return std::nullopt;
    }

    for (const NetRemoteEntityRenderTarget& target : state.net_transport->remote_entity_render_targets) {
        if (target.entity_id == *entity_id) {
            return InterpolateTarget(target, *state.net_transport, state.frame);
        }
    }
    return std::nullopt;
}

} // namespace splonks::network
