#pragma once

#include "math_types.hpp"
#include "network/net_ids.hpp"
#include "utils.hpp"

#include <optional>

namespace splonks {

struct Entity;
struct State;

namespace network {

void SetRemoteEntityRenderTarget(
    State& state,
    NetEntityId entity_id,
    const Vec2& start_pos,
    const Vec2& target_pos
);

void ClearRemoteEntityRenderTarget(State& state, NetEntityId entity_id);

std::optional<Vec2> GetRemoteEntityRenderPosition(const State& state, const Entity& entity);

} // namespace network

} // namespace splonks
