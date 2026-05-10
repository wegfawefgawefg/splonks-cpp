#include "network/net_message_apply_internal.hpp"

#include "graphics.hpp"
#include "presentation_commands.hpp"
#include "state.hpp"

#include <optional>

namespace splonks::network {

void ApplyPresentationCommandMessage(
    NetSessionState& session,
    State& state,
    Graphics* graphics,
    const PresentationCommandMessage& payload
) {
    if (graphics == nullptr) {
        return;
    }

    PresentationCommand command{
        .kind = static_cast<PresentationCommandKind>(payload.kind),
        .effect_id = static_cast<ScriptedPresentationEffectId>(payload.effect_id),
        .audio_asset_id = payload.audio_asset_id,
        .source_vid = payload.source_entity_id != kInvalidNetEntityId
            ? FindEntityVidForMessage(session, state, payload.source_entity_id)
            : std::nullopt,
        .target_vid = payload.target_entity_id != kInvalidNetEntityId
            ? FindEntityVidForMessage(session, state, payload.target_entity_id)
            : std::nullopt,
        .source_pos = payload.source_pos,
        .target_pos = payload.target_pos,
        .direction = IVec2::New(payload.direction_x, payload.direction_y),
        .entity_shake_amount = payload.entity_shake_amount,
        .foreground_shake_amount = payload.foreground_shake_amount,
        .background_shake_amount = payload.background_shake_amount,
        .area_entity_shake_amount = payload.area_entity_shake_amount,
        .shake_radius_tiles = payload.shake_radius_tiles,
    };
    PlayPresentationCommand(state, *graphics, command);
}

} // namespace splonks::network
