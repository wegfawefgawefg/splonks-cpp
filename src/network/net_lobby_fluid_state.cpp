#include "network/net_lobby_internal.hpp"

#include "state.hpp"
#include "tile_archetype.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace splonks::network {

namespace {

constexpr float kReplicatedFluidMinAmount = 0.0001F;
constexpr float kReplicatedFluidQuantize = 1000.0F;
constexpr std::uint32_t kFluidFullRefreshIntervalFrames = 60;
constexpr std::uint8_t kFluidEmptyResendFrames = 20;

std::int32_t QuantizeFluidFloat(float value) {
    return static_cast<std::int32_t>(std::lround(value * kReplicatedFluidQuantize));
}

bool IsSimulatedFluidTile(Tile tile) {
    return tile != Tile::Air && GetTileArchetype(tile).simulated_fluid;
}

NetReplicatedFluidCellSignature MakeFluidSignature(
    const Stage& stage,
    std::int32_t x,
    std::int32_t y
) {
    const std::size_t row = static_cast<std::size_t>(y);
    const std::size_t col = static_cast<std::size_t>(x);
    Tile tile = stage.fluid_tiles[row][col];
    float amount = std::clamp(stage.fluid_amount[row][col], 0.0F, 1.0F);
    if (!IsSimulatedFluidTile(tile) || amount <= kReplicatedFluidMinAmount) {
        tile = Tile::Air;
        amount = 0.0F;
    }

    const Vec2 velocity = amount > 0.0F ? stage.fluid_velocity[row][col] : Vec2::New(0.0F, 0.0F);
    const Vec2 temp_gravity =
        amount > 0.0F ? stage.fluid_temp_gravity[row][col] : Vec2::New(0.0F, 0.0F);
    return NetReplicatedFluidCellSignature{
        .tile_x = x,
        .tile_y = y,
        .tile = static_cast<std::uint16_t>(tile),
        .amount = QuantizeFluidFloat(amount),
        .velocity_x = QuantizeFluidFloat(velocity.x),
        .velocity_y = QuantizeFluidFloat(velocity.y),
        .gravity_x = QuantizeFluidFloat(stage.fluid_gravity[row][col].x),
        .gravity_y = QuantizeFluidFloat(stage.fluid_gravity[row][col].y),
        .temp_gravity_x = QuantizeFluidFloat(temp_gravity.x),
        .temp_gravity_y = QuantizeFluidFloat(temp_gravity.y),
        .gravity_strength = QuantizeFluidFloat(std::max(0.0F, stage.fluid_gravity_strength[row][col])),
    };
}

bool FluidSignaturesEqual(
    const NetReplicatedFluidCellSignature& a,
    const NetReplicatedFluidCellSignature& b
) {
    return a.tile_x == b.tile_x &&
           a.tile_y == b.tile_y &&
           a.tile == b.tile &&
           a.amount == b.amount &&
           a.velocity_x == b.velocity_x &&
           a.velocity_y == b.velocity_y &&
           a.gravity_x == b.gravity_x &&
           a.gravity_y == b.gravity_y &&
           a.temp_gravity_x == b.temp_gravity_x &&
           a.temp_gravity_y == b.temp_gravity_y &&
           a.gravity_strength == b.gravity_strength;
}

bool IsNonEmptyFluidSignature(const NetReplicatedFluidCellSignature& signature) {
    return signature.tile != static_cast<std::uint16_t>(Tile::Air) && signature.amount > 0;
}

bool UpdateFluidCacheAndShouldSend(
    NetTransportRuntime& transport,
    const NetReplicatedFluidCellSignature& signature
) {
    for (NetReplicatedFluidCellCache& cache : transport.replicated_fluid_cell_cache) {
        if (cache.signature.tile_x != signature.tile_x ||
            cache.signature.tile_y != signature.tile_y) {
            continue;
        }
        if (FluidSignaturesEqual(cache.signature, signature)) {
            if (!IsNonEmptyFluidSignature(signature) &&
                cache.empty_resend_frames_remaining > 0) {
                cache.empty_resend_frames_remaining -= 1;
                return true;
            }
            return false;
        }
        const bool became_empty =
            IsNonEmptyFluidSignature(cache.signature) && !IsNonEmptyFluidSignature(signature);
        cache.signature = signature;
        cache.empty_resend_frames_remaining = became_empty ? kFluidEmptyResendFrames : 0;
        return true;
    }

    if (!IsNonEmptyFluidSignature(signature)) {
        return false;
    }
    transport.replicated_fluid_cell_cache.push_back(NetReplicatedFluidCellCache{
        .signature = signature,
    });
    return true;
}

void PruneOutOfBoundsFluidCacheEntries(const Stage& stage, NetTransportRuntime& transport) {
    const int width = static_cast<int>(stage.GetTileWidth());
    const int height = static_cast<int>(stage.GetTileHeight());
    transport.replicated_fluid_cell_cache.erase(
        std::remove_if(
            transport.replicated_fluid_cell_cache.begin(),
            transport.replicated_fluid_cell_cache.end(),
            [&](const NetReplicatedFluidCellCache& cache) {
                return cache.signature.tile_x < 0 ||
                       cache.signature.tile_y < 0 ||
                       cache.signature.tile_x >= width ||
                       cache.signature.tile_y >= height;
            }
        ),
        transport.replicated_fluid_cell_cache.end()
    );
}

FluidCellPatchedEvent MakeFluidCellPayload(const Stage& stage, const IVec2& tile_pos) {
    const std::size_t y = static_cast<std::size_t>(tile_pos.y);
    const std::size_t x = static_cast<std::size_t>(tile_pos.x);
    Tile tile = stage.fluid_tiles[y][x];
    float amount = std::clamp(stage.fluid_amount[y][x], 0.0F, 1.0F);
    Vec2 velocity = stage.fluid_velocity[y][x];
    Vec2 temp_gravity = stage.fluid_temp_gravity[y][x];
    if (!IsSimulatedFluidTile(tile) || amount <= kReplicatedFluidMinAmount) {
        tile = Tile::Air;
        amount = 0.0F;
        velocity = Vec2::New(0.0F, 0.0F);
        temp_gravity = Vec2::New(0.0F, 0.0F);
    }
    return FluidCellPatchedEvent{
        .tile_pos = tile_pos,
        .tile = tile,
        .amount = amount,
        .velocity = velocity,
        .gravity = stage.fluid_gravity[y][x],
        .temp_gravity = temp_gravity,
        .gravity_strength = std::max(0.0F, stage.fluid_gravity_strength[y][x]),
    };
}

std::vector<NetEvent> BuildReplicatedFluidCellPatchEvents(
    State& state,
    NetTransportRuntime& transport
) {
    std::vector<NetEvent> events;
    Stage& stage = state.stage;
    if (stage.tiles.empty()) {
        return events;
    }

    stage.SyncTileInstanceMetadataGrid();
    const bool full_refresh = (state.frame % kFluidFullRefreshIntervalFrames) == 0;
    for (int y = 0; y < static_cast<int>(stage.GetTileHeight()); ++y) {
        for (int x = 0; x < static_cast<int>(stage.GetTileWidth()); ++x) {
            const NetReplicatedFluidCellSignature signature = MakeFluidSignature(stage, x, y);
            const bool should_send = UpdateFluidCacheAndShouldSend(transport, signature);
            const bool refresh_non_empty = full_refresh && IsNonEmptyFluidSignature(signature);
            if (!should_send && !refresh_non_empty) {
                continue;
            }

            NetEvent event;
            event.header = state.net_session.MakeLocalTransientEventHeader(state.frame);
            event.type = NetEventType::FluidCellPatched;
            event.payload = MakeFluidCellPayload(stage, IVec2::New(x, y));
            events.push_back(event);
        }
    }

    PruneOutOfBoundsFluidCacheEntries(stage, transport);
    return events;
}

} // namespace

void SendReplicatedFluidCellPatchesToAllRemotes(State& state, NetTransportRuntime& transport) {
    if (state.net_session.role != NetRole::Coordinator || transport.remotes.empty()) {
        return;
    }
    const std::vector<NetEvent> events = BuildReplicatedFluidCellPatchEvents(state, transport);
    if (events.empty()) {
        return;
    }
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendFluidCellEvents(transport, remote.endpoint, events);
    }
}

} // namespace splonks::network
