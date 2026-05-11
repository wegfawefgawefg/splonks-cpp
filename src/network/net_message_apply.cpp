#include "network/net_message_apply.hpp"

#include "network/net_message_apply_internal.hpp"
#include "network/net_message.hpp"
#include "network/net_session.hpp"
#include "state.hpp"
#include "state_fingerprint.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace splonks::network {

namespace {

bool IsImmediateLocalNetResult(NetMessageType type) {
    switch (type) {
    case NetMessageType::EntitySpawned:
    case NetMessageType::EntityDeactivated:
    case NetMessageType::TileBroken:
    case NetMessageType::TileChanged:
    case NetMessageType::FluidCellPatched:
    case NetMessageType::StageLightAdded:
    case NetMessageType::StageLightRemoved:
    case NetMessageType::PlayerStatePatched:
    case NetMessageType::PresentationCommand:
        return true;
    default:
        return false;
    }
}

bool IsTransientStateRepairMessage(const NetMessage& message) {
    if (message.type == NetMessageType::FluidCellPatched) {
        return true;
    }
    if (message.type != NetMessageType::EntityStatePatched) {
        return false;
    }
    const auto* const payload = std::get_if<EntityStatePatchedMessage>(&message.payload);
    return payload == nullptr || payload->source_entity_id == kInvalidNetEntityId;
}

bool ShouldSkipImmediateLocalApply(const NetSessionState& session, const NetMessage& message) {
    if (message.header.source_player_id != session.local_player_id ||
        !IsImmediateLocalNetResult(message.type)) {
        return false;
    }
    if (message.type == NetMessageType::EntityStatePatched) {
        const auto* payload = std::get_if<EntityStatePatchedMessage>(&message.payload);
        return payload == nullptr || payload->source_entity_id == kInvalidNetEntityId;
    }
    return true;
}

void NoteAppliedCoordinatorOrder(NetSessionState& session, const NetMessage& message) {
    session.MarkCoordinatorOrderApplied(message);
}

} // namespace

std::size_t ApplyOrderedMessages(
    NetSessionState& session,
    State& state,
    Audio* audio,
    Graphics* graphics
) {
    std::size_t applied_count = 0;
    std::vector<NetMessageId> transient_applied_message_ids;
    std::optional<std::uint64_t> pending_snapshot_fingerprint;
    std::stable_sort(
        session.ordered_messages.begin(),
        session.ordered_messages.end(),
        [](const NetMessage& a, const NetMessage& b) {
            if (a.header.coordinator_order == b.header.coordinator_order) {
                return a.header.message_id < b.header.message_id;
            }
            if (a.header.coordinator_order == 0) {
                return false;
            }
            if (b.header.coordinator_order == 0) {
                return true;
            }
            return a.header.coordinator_order < b.header.coordinator_order;
        }
    );
    for (const NetMessage& message : session.ordered_messages) {
        if (session.HasAppliedMessage(message.header.message_id)) {
            continue;
        }
        if (session.role == NetRole::Peer &&
            message.header.coordinator_order > session.next_expected_coordinator_order) {
            continue;
        }
        if (ShouldSkipImmediateLocalApply(session, message)) {
            if (session.MarkMessageApplied(message.header.message_id)) {
                NoteAppliedCoordinatorOrder(session, message);
                session.AddMessageLog(NetMessageLogPhase::SkippedLocalApply, message);
                ++applied_count;
                if (IsTransientStateRepairMessage(message)) {
                    transient_applied_message_ids.push_back(message.header.message_id);
                }
            }
            continue;
        }

        switch (message.type) {
        case NetMessageType::EntitySpawned:
            if (const auto* payload = std::get_if<EntitySpawnedMessage>(&message.payload)) {
                ApplyEntitySpawnedMessage(session, state, *payload, graphics);
            }
            break;
        case NetMessageType::EntityDamaged:
            if (const auto* payload = std::get_if<EntityDamagedMessage>(&message.payload)) {
                ApplyEntityDamagedMessage(session, state, audio, message.header.source_player_id, *payload);
            }
            break;
        case NetMessageType::EntityDeactivated:
            if (const auto* payload = std::get_if<EntityIdMessage>(&message.payload)) {
                ApplyEntityDeactivatedMessage(session, state, graphics, *payload);
            }
            break;
        case NetMessageType::EntityStatePatched:
            if (const auto* payload = std::get_if<EntityStatePatchedMessage>(&message.payload)) {
                ApplyEntityStatePatchedMessage(
                    session,
                    state,
                    graphics,
                    message.header.source_player_id,
                    *payload
                );
            }
            break;
        case NetMessageType::EntityHeld:
            if (const auto* payload = std::get_if<EntityHeldMessage>(&message.payload)) {
                ApplyEntityHeldMessage(session, state, graphics, *payload);
            }
            break;
        case NetMessageType::EntityDropped:
            if (const auto* payload = std::get_if<EntityDroppedMessage>(&message.payload)) {
                ApplyEntityDroppedMessage(session, state, graphics, *payload);
            }
            break;
        case NetMessageType::EntityThrown:
            if (const auto* payload = std::get_if<EntityThrownMessage>(&message.payload)) {
                ApplyEntityThrownMessage(session, state, graphics, *payload);
            }
            break;
        case NetMessageType::PlayerStatePatched:
            if (const auto* payload = std::get_if<PlayerStatePatchedMessage>(&message.payload)) {
                ApplyPlayerStatePatchedMessage(session, state, *payload);
            }
            break;
        case NetMessageType::RunStatePatched:
            if (const auto* payload = std::get_if<RunStatePatchedMessage>(&message.payload)) {
                ApplyRunStatePatchedMessage(state, *payload, pending_snapshot_fingerprint);
            }
            break;
        case NetMessageType::TileBroken:
            if (const auto* payload = std::get_if<TileBrokenMessage>(&message.payload)) {
                ApplyTileBrokenMessage(state, audio, *payload);
            }
            break;
        case NetMessageType::PresentationCommand:
            if (const auto* payload = std::get_if<PresentationCommandMessage>(&message.payload)) {
                ApplyPresentationCommandMessage(session, state, graphics, *payload);
            }
            break;
        case NetMessageType::TileChanged:
            if (const auto* payload = std::get_if<TileChangedMessage>(&message.payload)) {
                ApplyTileChangedMessage(state, *payload);
            }
            break;
        case NetMessageType::FluidCellPatched:
            if (const auto* payload = std::get_if<FluidCellPatchedMessage>(&message.payload)) {
                ApplyFluidCellPatchedMessage(state, *payload);
            }
            break;
        case NetMessageType::StageLightAdded:
            if (const auto* payload = std::get_if<StageLightAddedMessage>(&message.payload)) {
                ApplyStageLightAddedMessage(state, *payload);
            }
            break;
        case NetMessageType::StageLightRemoved:
            if (const auto* payload = std::get_if<StageLightRemovedMessage>(&message.payload)) {
                ApplyStageLightRemovedMessage(state, *payload);
            }
            break;
        default:
            break;
        }

        if (session.MarkMessageApplied(message.header.message_id)) {
            NoteAppliedCoordinatorOrder(session, message);
            session.AddMessageLog(NetMessageLogPhase::Applied, message);
            ++applied_count;
            if (IsTransientStateRepairMessage(message)) {
                transient_applied_message_ids.push_back(message.header.message_id);
            }
        }
    }
    if (!transient_applied_message_ids.empty()) {
        session.ordered_messages.erase(
            std::remove_if(
                session.ordered_messages.begin(),
                session.ordered_messages.end(),
                [&](const NetMessage& message) {
                    return IsTransientStateRepairMessage(message) &&
                           std::find(
                               transient_applied_message_ids.begin(),
                               transient_applied_message_ids.end(),
                               message.header.message_id
                           ) != transient_applied_message_ids.end();
                }
            ),
            session.ordered_messages.end()
        );
        session.applied_message_ids.erase(
            std::remove_if(
                session.applied_message_ids.begin(),
                session.applied_message_ids.end(),
                [&](NetMessageId message_id) {
                    return std::find(
                               transient_applied_message_ids.begin(),
                               transient_applied_message_ids.end(),
                               message_id
                           ) != transient_applied_message_ids.end();
                }
            ),
            session.applied_message_ids.end()
        );
    }
    if (pending_snapshot_fingerprint.has_value()) {
        const std::uint64_t actual_fingerprint = ComputeNetworkStateFingerprint(state).value;
        state.net_session.last_snapshot_expected_fingerprint = *pending_snapshot_fingerprint;
        state.net_session.last_snapshot_actual_fingerprint = actual_fingerprint;
        state.net_session.last_snapshot_fingerprint_valid =
            actual_fingerprint == *pending_snapshot_fingerprint;
    }
    return applied_count;
}

} // namespace splonks::network
