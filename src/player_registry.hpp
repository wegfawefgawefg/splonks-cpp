#pragma once

#include "inputs.hpp"
#include "player_id.hpp"
#include "vid.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace splonks {

enum class PlayerConnectionKind : std::uint8_t {
    Local,
    Remote,
};

struct PlayerSlot {
    PlayerId player_id = kInvalidPlayerId;
    std::optional<VID> ent_vid;
    PlayerConnectionKind connection_kind = PlayerConnectionKind::Local;
    bool connected = false;
    bool primary_local = false;
    std::string display_name;
    InputFrame input_frame = InputFrame::New();
    InputFrame previous_input_frame = InputFrame::New();
    PlayingInputs inputs = PlayingInputs::New();
    PlayingInputs immediate_inputs = PlayingInputs::New();
};

struct PlayerRegistry {
    std::vector<PlayerSlot> slots;

    static PlayerRegistry New();

    PlayerSlot* Find(PlayerId player_id);
    const PlayerSlot* Find(PlayerId player_id) const;
    PlayerSlot* FindByEntVid(VID ent_vid);
    const PlayerSlot* FindByEntVid(VID ent_vid) const;
    PlayerSlot* FindPrimaryLocal();
    const PlayerSlot* FindPrimaryLocal() const;

    PlayerSlot& EnsureLocalPlayer(PlayerId player_id, const std::string& display_name, bool primary);
    PlayerSlot& EnsurePrimaryLocalPlayer();
    PlayerSlot& EnsureRemotePlayer(PlayerId player_id, const std::string& display_name);
    void Remove(PlayerId player_id);

    void AssignEnt(PlayerId player_id, VID ent_vid);
    void ClearEntRefs();
    void ClearEntRef(VID ent_vid);
    std::optional<PlayerId> FindPlayerIdForEnt(VID ent_vid) const;
    const PlayingInputs* FindInputsForEnt(VID ent_vid) const;
    const PlayingInputs* FindInputsForPlayer(PlayerId player_id) const;
    void SetInputFrameForPlayer(PlayerId player_id, const InputFrame& input_frame);
    void SetInputFrameAndInputsForPlayer(
        PlayerId player_id,
        const InputFrame& input_frame,
        const PlayingInputs& inputs,
        const PlayingInputs& immediate_inputs
    );
    void SetInputsForPlayer(PlayerId player_id, const PlayingInputs& inputs, const PlayingInputs& immediate_inputs);
    void SetPrimaryLocalInputs(const PlayingInputs& inputs, const PlayingInputs& immediate_inputs);
};

} // namespace splonks
