#include "state_fingerprint.hpp"

#include "ent.hpp"
#include "network/net_session.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <type_traits>
#include <vector>

namespace splonks {

namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

bool VidLess(const VID& lhs, const VID& rhs) {
    if (lhs.id != rhs.id) {
        return lhs.id < rhs.id;
    }
    return lhs.version < rhs.version;
}

struct FingerprintWriter {
    std::uint64_t value = kFnvOffsetBasis;

    void AddBytes(const void* bytes, std::size_t size) {
        const auto* data = static_cast<const std::uint8_t*>(bytes);
        for (std::size_t i = 0; i < size; ++i) {
            value ^= static_cast<std::uint64_t>(data[i]);
            value *= kFnvPrime;
        }
    }

    void AddByte(std::uint8_t byte) {
        value ^= static_cast<std::uint64_t>(byte);
        value *= kFnvPrime;
    }

    void AddUint16(std::uint16_t value_) {
        for (std::uint32_t shift = 0; shift < 16; shift += 8) {
            AddByte(static_cast<std::uint8_t>(
                (value_ >> shift) & static_cast<std::uint16_t>(0xFFU)
            ));
        }
    }

    void AddUint32(std::uint32_t value_) {
        for (std::uint32_t shift = 0; shift < 32; shift += 8) {
            AddByte(static_cast<std::uint8_t>(
                (value_ >> shift) & static_cast<std::uint32_t>(0xFFU)
            ));
        }
    }

    void AddUint64(std::uint64_t value_) {
        for (std::uint32_t shift = 0; shift < 64; shift += 8) {
            AddByte(static_cast<std::uint8_t>(
                (value_ >> shift) & static_cast<std::uint64_t>(0xFFU)
            ));
        }
    }

    template <typename T> void AddPod(const T& pod) {
        if constexpr (std::is_enum_v<T>) {
            AddPod(static_cast<std::underlying_type_t<T>>(pod));
        } else if constexpr (std::is_same_v<T, bool>) {
            AddByte(pod ? 1U : 0U);
        } else if constexpr (std::is_integral_v<T>) {
            using Unsigned = std::make_unsigned_t<T>;
            Unsigned bits = 0;
            static_assert(sizeof(bits) == sizeof(pod));
            std::memcpy(&bits, &pod, sizeof(bits));
            if constexpr (sizeof(bits) == 1) {
                AddByte(static_cast<std::uint8_t>(bits));
            } else if constexpr (sizeof(bits) == 2) {
                AddUint16(static_cast<std::uint16_t>(bits));
            } else if constexpr (sizeof(bits) == 4) {
                AddUint32(static_cast<std::uint32_t>(bits));
            } else if constexpr (sizeof(bits) == 8) {
                AddUint64(static_cast<std::uint64_t>(bits));
            } else {
                static_assert(sizeof(bits) <= 8, "Unsupported fingerprint integer width");
            }
        } else if constexpr (std::is_same_v<T, float>) {
            std::uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(pod));
            static_assert(std::numeric_limits<float>::is_iec559);
            std::memcpy(&bits, &pod, sizeof(bits));
            AddUint32(bits);
        } else if constexpr (std::is_same_v<T, double>) {
            std::uint64_t bits = 0;
            static_assert(sizeof(bits) == sizeof(pod));
            static_assert(std::numeric_limits<double>::is_iec559);
            std::memcpy(&bits, &pod, sizeof(bits));
            AddUint64(bits);
        } else {
            static_assert(std::is_integral_v<T>, "Fingerprint AddPod requires scalar values");
        }
    }

    void AddBool(bool value_) {
        AddByte(value_ ? 1U : 0U);
    }

    void AddCount(std::size_t count) {
        AddPod(static_cast<std::uint64_t>(count));
    }

    void AddFixedScalar(sim::Scalar value_) {
        AddPod(value_.raw_value());
    }

    void AddQuantizedFloat(float value_) {
        AddFixedScalar(sim::ToSimScalar(value_));
    }

    void AddString(const std::string& text) {
        AddCount(text.size());
        AddBytes(text.data(), text.size());
    }

    void AddVec2(const Vec2& vec) {
        const sim::Vec2 fixed = sim::ToSimVec2(vec);
        AddFixedScalar(fixed.x);
        AddFixedScalar(fixed.y);
    }

    void AddIVec2(const IVec2& vec) {
        AddPod(vec.x);
        AddPod(vec.y);
    }

    void AddVid(const VID& vid) {
        AddPod(vid.id);
        AddPod(vid.version);
    }

    void AddOptionalVid(const std::optional<VID>& vid) {
        AddBool(vid.has_value());
        if (vid.has_value()) {
            AddVid(*vid);
        }
    }
};

void AddStageFingerprint(FingerprintWriter& writer, const Stage& stage,
                         bool include_cache_generation) {
    writer.AddString(stage.quest_id);
    writer.AddString(stage.quest_stage_id);
    writer.AddString(stage.route_label);
    writer.AddPod(stage.quest_level_number);
    writer.AddBool(stage.generation_seed.has_value());
    if (stage.generation_seed.has_value()) {
        writer.AddPod(*stage.generation_seed);
    }
    writer.AddPod(static_cast<std::uint8_t>(stage.stage_type));
    writer.AddQuantizedFloat(stage.gravity);
    if (include_cache_generation) {
        writer.AddPod(stage.tile_change_generation);
    }

    const UVec2 dims = stage.GetStageDims();
    writer.AddPod(dims.x);
    writer.AddPod(dims.y);
    for (std::uint32_t y = 0; y < dims.y; ++y) {
        const std::size_t row_y = static_cast<std::size_t>(y);
        for (std::uint32_t x = 0; x < dims.x; ++x) {
            const std::size_t col_x = static_cast<std::size_t>(x);
            const auto read_tile_grid = [&](const std::vector<std::vector<Tile>>& grid,
                                            Tile fallback) {
                if (row_y >= grid.size() || col_x >= grid[row_y].size()) {
                    return fallback;
                }
                return grid[row_y][col_x];
            };
            const auto read_rotation_grid = [&]() {
                if (row_y >= stage.tile_rotations.size() ||
                    col_x >= stage.tile_rotations[row_y].size()) {
                    return kTileRotation0;
                }
                return stage.tile_rotations[row_y][col_x];
            };
            const auto read_float_grid = [](const std::vector<std::vector<float>>& grid,
                                            std::size_t y_, std::size_t x_) {
                if (y_ >= grid.size() || x_ >= grid[y_].size()) {
                    return 0.0F;
                }
                return grid[y_][x_];
            };

            writer.AddPod(static_cast<std::uint16_t>(read_tile_grid(stage.tiles, Tile::Air)));
            writer.AddPod(static_cast<std::uint8_t>(read_rotation_grid()));
            writer.AddPod(
                static_cast<std::uint16_t>(read_tile_grid(stage.backwall_tiles, Tile::Air)));
            writer.AddPod(static_cast<std::uint16_t>(read_tile_grid(stage.fluid_tiles, Tile::Air)));
            writer.AddQuantizedFloat(read_float_grid(stage.fluid_amount, row_y, col_x));

            EmbeddedTreasure embedded{};
            if (row_y < stage.embedded_treasures.size() &&
                col_x < stage.embedded_treasures[row_y].size()) {
                embedded = stage.embedded_treasures[row_y][col_x];
            }
            writer.AddPod(static_cast<std::uint8_t>(embedded.visibility));
            writer.AddPod(embedded.overlay_frame);
            for (const EmbeddedTreasureDrop& drop : embedded.drops) {
                writer.AddPod(static_cast<std::uint16_t>(drop.type_));
                writer.AddPod(drop.count);
            }
        }
    }

    std::vector<StageLight> lights = stage.lights;
    std::sort(lights.begin(), lights.end(),
              [](const StageLight& lhs, const StageLight& rhs) {
                  return VidLess(lhs.vid, rhs.vid);
              });
    writer.AddCount(lights.size());
    for (const StageLight& light : lights) {
        writer.AddVid(light.vid);
        writer.AddIVec2(light.tile_pos);
        writer.AddPod(light.radius);
    }
}

void AddEffectFingerprint(FingerprintWriter& writer, const BoxedEntEffects& effects_box) {
    const EntEffects* const effects = effects_box.get();
    const bool has_effects = effects != nullptr && effects->count > 0;
    writer.AddBool(has_effects);
    if (!has_effects) {
        return;
    }

    writer.AddPod(effects->count);
    for (std::uint8_t i = 0; i < effects->count && i < effects->effects.size(); ++i) {
        const EffectInstance& effect = effects->effects[i];
        writer.AddPod(static_cast<std::uint16_t>(effect.id));
        writer.AddPod(effect.count);
        writer.AddQuantizedFloat(effect.value);
        writer.AddPod(effect.frames_remaining);
    }
}

void AddEntFingerprint(FingerprintWriter& writer, const Ent& ent) {
    writer.AddBool(ent.active);
    writer.AddPod(static_cast<std::uint16_t>(ent.type_));
    writer.AddVid(ent.vid);
    if (!ent.active) {
        return;
    }

    writer.AddBool(ent.has_physics);
    writer.AddBool(ent.can_collide);
    writer.AddBool(ent.grounded);
    writer.AddBool(ent.holding);
    writer.AddBool(ent.wanted);
    writer.AddBool(ent.render_enabled);
    writer.AddVec2(ent.pos);
    writer.AddVec2(ent.vel);
    writer.AddVec2(ent.acc);
    writer.AddVec2(ent.size);
    writer.AddFixedScalar(ent.rotation);
    writer.AddPod(ent.coyote_time);
    writer.AddPod(ent.stun_timer);
    writer.AddPod(ent.fall_timer);
    writer.AddPod(static_cast<std::uint8_t>(ent.facing));
    writer.AddPod(static_cast<std::uint8_t>(ent.draw_layer));
    writer.AddPod(static_cast<std::uint8_t>(ent.condition));
    writer.AddPod(static_cast<std::uint8_t>(ent.ai_state));
    writer.AddPod(static_cast<std::uint8_t>(ent.damage_vuln));
    writer.AddPod(ent.movement_flags);
    writer.AddPod(ent.health);
    writer.AddOptionalVid(ent.back_vid);
    writer.AddOptionalVid(ent.holding_vid);
    writer.AddOptionalVid(ent.held_by_vid);
    writer.AddOptionalVid(ent.ent_a);
    writer.AddOptionalVid(ent.ent_b);
    writer.AddOptionalVid(ent.ent_c);
    writer.AddOptionalVid(ent.ent_d);
    writer.AddPod(ent.stage_exit_id);
    writer.AddPod(ent.money);
    writer.AddQuantizedFloat(ent.counter_a);
    writer.AddQuantizedFloat(ent.counter_b);
    writer.AddQuantizedFloat(ent.counter_c);
    writer.AddQuantizedFloat(ent.counter_d);
    writer.AddQuantizedFloat(ent.light_strength);
    writer.AddQuantizedFloat(ent.light_color.r);
    writer.AddQuantizedFloat(ent.light_color.g);
    writer.AddQuantizedFloat(ent.light_color.b);
    writer.AddPod(ent.light_radius);
    writer.AddIVec2(ent.point_a);
    writer.AddIVec2(ent.point_b);
    writer.AddIVec2(ent.point_c);
    writer.AddIVec2(ent.point_d);
    writer.AddPod(ent.aframe_animator.anim_id);
    writer.AddPod(ent.aframe_animator.current_frame);
    writer.AddQuantizedFloat(ent.aframe_animator.current_time);
    writer.AddQuantizedFloat(ent.aframe_animator.speed);
    writer.AddBool(ent.aframe_animator.animate);
    writer.AddBool(ent.aframe_animator.loop);
    writer.AddBool(ent.aframe_animator.finished);
    AddEffectFingerprint(writer, ent.effects);
}

void AddInputFrameFingerprint(FingerprintWriter& writer, const InputFrame& input_frame) {
    writer.AddBool(input_frame.left);
    writer.AddBool(input_frame.right);
    writer.AddBool(input_frame.up);
    writer.AddBool(input_frame.down);
    writer.AddBool(input_frame.jump);
    writer.AddBool(input_frame.run);
    writer.AddBool(input_frame.use_button);
    writer.AddBool(input_frame.equip_button);
    writer.AddBool(input_frame.pick_up_drop);
    writer.AddBool(input_frame.stop);
    writer.AddBool(input_frame.bomb);
    writer.AddBool(input_frame.rope);
    writer.AddBool(input_frame.attack);
    writer.AddBool(input_frame.buy_button);
    writer.AddBool(input_frame.emote_up);
    writer.AddBool(input_frame.emote_down);
    writer.AddBool(input_frame.quit);
    writer.AddBool(input_frame.toggle_collision_boxes);
    writer.AddBool(input_frame.regenerate_level);
    writer.AddPod(input_frame.mouse_pos.x);
    writer.AddPod(input_frame.mouse_pos.y);
}

void AddPlayerRegistryFingerprint(FingerprintWriter& writer, const PlayerRegistry& players) {
    writer.AddCount(players.slots.size());
    for (const PlayerSlot& slot : players.slots) {
        writer.AddPod(slot.player_id);
        writer.AddBool(slot.ent_vid.has_value());
        if (slot.ent_vid.has_value()) {
            writer.AddVid(*slot.ent_vid);
        }
        writer.AddBool(slot.connected);
        AddInputFrameFingerprint(writer, slot.input_frame);
        AddInputFrameFingerprint(writer, slot.previous_input_frame);
    }
}

void AddToolInventoryFingerprint(FingerprintWriter& writer,
                                 const EntToolInventoryState& inventory) {
    writer.AddCount(inventory.tool_states.size());
    for (const EntToolState& tool_state : inventory.tool_states) {
        writer.AddVid(tool_state.owner_vid);
        for (const ToolSlot& slot : tool_state.slots) {
            writer.AddPod(static_cast<std::uint16_t>(slot.kind));
            writer.AddPod(slot.count);
            writer.AddPod(slot.cooldown);
            writer.AddBool(slot.active);
        }
    }
}

network::NetEntId NetEntIdForVid(const State& state, VID vid) {
    return state.net_session.FindNetEntId(vid).value_or(network::kInvalidNetEntId);
}

void AddNetworkVid(FingerprintWriter& writer, const State& state, const VID& vid) {
    writer.AddPod(NetEntIdForVid(state, vid));
}

void AddNetworkOptionalVid(FingerprintWriter& writer, const State& state,
                           const std::optional<VID>& vid) {
    writer.AddBool(vid.has_value());
    if (vid.has_value()) {
        AddNetworkVid(writer, state, *vid);
    }
}

bool IsMotionIgnoredForPlayer(const State& state, const Ent& ent, PlayerId player_id) {
    const network::NetEntId ignored_player_ent_id = network::MakePlayerNetEntId(player_id);
    if (NetEntIdForVid(state, ent.vid) == ignored_player_ent_id) {
        return true;
    }

    std::optional<VID> holder_vid = ent.held_by_vid;
    for (int depth = 0; holder_vid.has_value() && depth < 8; ++depth) {
        if (NetEntIdForVid(state, *holder_vid) == ignored_player_ent_id) {
            return true;
        }
        const Ent* const holder = state.ents.GetEnt(*holder_vid);
        if (holder == nullptr) {
            return false;
        }
        holder_vid = holder->held_by_vid;
    }
    return false;
}

bool IsMotionIgnoredForAnyPlayer(const State& state, const Ent& ent) {
    if (ent.type_ == EntType::Player) {
        return true;
    }
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.player_id == kInvalidPlayerId) {
            continue;
        }
        if (IsMotionIgnoredForPlayer(state, ent, slot.player_id)) {
            return true;
        }
    }
    return false;
}

void AddNetworkEntFingerprint(FingerprintWriter& writer, const State& state, const Ent& ent) {
    const network::NetEntId ent_id = NetEntIdForVid(state, ent.vid);
    const bool ignore_motion = IsMotionIgnoredForAnyPlayer(state, ent);

    writer.AddPod(ent_id);
    writer.AddBool(ent.active);
    writer.AddPod(static_cast<std::uint16_t>(ent.type_));
    if (!ent.active) {
        return;
    }

    if (ignore_motion) {
        writer.AddBool(ent.holding);
        AddNetworkOptionalVid(writer, state, ent.back_vid);
        AddNetworkOptionalVid(writer, state, ent.holding_vid);
        AddNetworkOptionalVid(writer, state, ent.held_by_vid);
        writer.AddPod(ent.health);
        writer.AddPod(ent.money);
        AddEffectFingerprint(writer, ent.effects);
        return;
    }

    writer.AddBool(ent.has_physics);
    writer.AddBool(ent.can_collide);
    writer.AddBool(ent.grounded);
    writer.AddBool(ent.holding);
    writer.AddBool(ent.wanted);
    writer.AddVec2(ent.pos);
    writer.AddVec2(ent.vel);
    writer.AddVec2(ent.acc);
    writer.AddVec2(ent.size);
    writer.AddPod(ent.coyote_time);
    writer.AddPod(ent.stun_timer);
    writer.AddPod(ent.fall_timer);
    writer.AddPod(static_cast<std::uint8_t>(ent.facing));
    writer.AddPod(static_cast<std::uint8_t>(ent.condition));
    writer.AddPod(static_cast<std::uint8_t>(ent.ai_state));
    writer.AddPod(static_cast<std::uint8_t>(ent.damage_vuln));
    writer.AddPod(ent.movement_flags);
    writer.AddPod(ent.health);
    AddNetworkOptionalVid(writer, state, ent.back_vid);
    AddNetworkOptionalVid(writer, state, ent.holding_vid);
    AddNetworkOptionalVid(writer, state, ent.held_by_vid);
    AddNetworkOptionalVid(writer, state, ent.ent_a);
    AddNetworkOptionalVid(writer, state, ent.ent_b);
    AddNetworkOptionalVid(writer, state, ent.ent_c);
    AddNetworkOptionalVid(writer, state, ent.ent_d);
    writer.AddPod(ent.stage_exit_id);
    writer.AddPod(ent.money);
    writer.AddQuantizedFloat(ent.counter_a);
    writer.AddQuantizedFloat(ent.counter_b);
    writer.AddQuantizedFloat(ent.counter_c);
    writer.AddQuantizedFloat(ent.counter_d);
    writer.AddIVec2(ent.point_a);
    writer.AddIVec2(ent.point_b);
    writer.AddIVec2(ent.point_c);
    writer.AddIVec2(ent.point_d);
    AddEffectFingerprint(writer, ent.effects);
}

void AddNetworkPlayerRegistryFingerprint(FingerprintWriter& writer, const State& state) {
    std::vector<const PlayerSlot*> slots;
    slots.reserve(state.players.slots.size());
    for (const PlayerSlot& slot : state.players.slots) {
        slots.push_back(&slot);
    }
    std::sort(slots.begin(), slots.end(), [](const PlayerSlot* lhs, const PlayerSlot* rhs) {
        return lhs->player_id < rhs->player_id;
    });

    writer.AddCount(slots.size());
    for (const PlayerSlot* const slot : slots) {
        writer.AddPod(slot->player_id);
        writer.AddBool(slot->connected);
        writer.AddBool(slot->ent_vid.has_value());
        if (slot->ent_vid.has_value()) {
            AddNetworkVid(writer, state, *slot->ent_vid);
        }
    }
}

void AddNetworkToolInventoryFingerprint(FingerprintWriter& writer, const State& state) {
    std::vector<const EntToolState*> tool_states;
    tool_states.reserve(state.ent_tools.tool_states.size());
    for (const EntToolState& tool_state : state.ent_tools.tool_states) {
        if (NetEntIdForVid(state, tool_state.owner_vid) == network::kInvalidNetEntId) {
            continue;
        }
        tool_states.push_back(&tool_state);
    }
    std::sort(tool_states.begin(), tool_states.end(),
              [&state](const EntToolState* lhs, const EntToolState* rhs) {
                  const network::NetEntId lhs_id = NetEntIdForVid(state, lhs->owner_vid);
                  const network::NetEntId rhs_id = NetEntIdForVid(state, rhs->owner_vid);
                  if (lhs_id != rhs_id) {
                      return lhs_id < rhs_id;
                  }
                  return VidLess(lhs->owner_vid, rhs->owner_vid);
              });

    writer.AddCount(tool_states.size());
    for (const EntToolState* const tool_state : tool_states) {
        AddNetworkVid(writer, state, tool_state->owner_vid);
        for (const ToolSlot& slot : tool_state->slots) {
            writer.AddPod(static_cast<std::uint16_t>(slot.kind));
            writer.AddPod(slot.count);
            writer.AddPod(slot.cooldown);
            writer.AddBool(slot.active);
        }
    }
}

} // namespace

CanonicalStateFingerprint ComputeCanonicalStateFingerprintWithOptions(const State& state,
                                                                      bool include_drng) {
    FingerprintWriter writer;
    writer.AddPod(static_cast<std::uint8_t>(state.mode));
    writer.AddPod(state.frame);
    writer.AddPod(state.stage_frame);
    if (include_drng) {
        writer.AddPod(state.drng.state);
        writer.AddPod(state.stagegen_drng.state);
    }
    writer.AddPod(state.depth);
    writer.AddPod(state.points);
    writer.AddPod(state.deaths);
    writer.AddPod(static_cast<std::uint8_t>(state.multiplayer_respawn_mode));
    writer.AddPod(state.sac_altar_favor);
    writer.AddPod(state.sac_altar_reward_tier);
    writer.AddBool(state.game_over);
    writer.AddBool(state.win);
    writer.AddPod(static_cast<std::uint8_t>(state.quest_state.quest_id));
    writer.AddBool(state.quest_state.classic.made_black_market);
    writer.AddBool(state.quest_state.classic.made_udjat_eye);
    writer.AddBool(state.quest_state.classic.has_udjat_eye);
    writer.AddBool(state.quest_state.classic.made_moai);
    writer.AddBool(state.quest_state.classic.has_hedjet);
    writer.AddBool(state.quest_state.classic.has_sceptre);
    writer.AddBool(state.quest_state.classic.has_book_of_dead);
    AddStageFingerprint(writer, state.stage, true);
    AddPlayerRegistryFingerprint(writer, state.players);
    AddToolInventoryFingerprint(writer, state.ent_tools);

    writer.AddCount(state.ents.ents.size());
    for (const Ent& ent : state.ents.ents) {
        AddEntFingerprint(writer, ent);
    }

    int active_ents = 0;
    for (const Ent& ent : state.ents.ents) {
        if (ent.active) {
            ++active_ents;
        }
    }

    std::ostringstream summary;
    summary << "stage=" << state.stage.quest_stage_id << " frame=" << state.frame
            << " stage_frame=" << state.stage_frame << " ents=" << active_ents
            << " tiles=" << state.stage.GetTileWidth() << "x" << state.stage.GetTileHeight();
    return CanonicalStateFingerprint{
        .value = writer.value,
        .summary = summary.str(),
    };
}

CanonicalStateFingerprint ComputeCanonicalStateFingerprint(const State& state) {
    return ComputeCanonicalStateFingerprintWithOptions(state, false);
}

CanonicalStateFingerprint ComputeGameplayDeterminismFingerprint(const State& state) {
    return ComputeCanonicalStateFingerprintWithOptions(state, true);
}

std::uint64_t
CombineNetworkStateFingerprintComponents(const NetworkStateFingerprintComponents& components) {
    FingerprintWriter writer;
    writer.AddPod(components.root);
    writer.AddPod(components.stage);
    writer.AddPod(components.players);
    writer.AddPod(components.tools);
    writer.AddPod(components.ents);
    return writer.value;
}

CanonicalStateFingerprint ComputeNetworkStateFingerprint(const State& state) {
    const NetworkStateFingerprintComponents components =
        ComputeNetworkStateFingerprintComponents(state);
    int active_ents = 0;
    for (const Ent& ent : state.ents.ents) {
        if (ent.active) {
            ++active_ents;
        }
    }

    std::ostringstream summary;
    summary << "stage=" << state.stage.quest_stage_id << " frame=" << state.frame
            << " stage_frame=" << state.stage_frame << " active_ents=" << active_ents
            << " tiles=" << state.stage.GetTileWidth() << "x" << state.stage.GetTileHeight();
    return CanonicalStateFingerprint{
        .value = CombineNetworkStateFingerprintComponents(components),
        .summary = summary.str(),
    };
}

NetworkStateFingerprintComponents ComputeNetworkStateFingerprintComponents(const State& state) {
    FingerprintWriter root;
    root.AddPod(state.frame);
    root.AddPod(state.stage_frame);
    root.AddPod(state.drng.state);
    root.AddPod(state.stagegen_drng.state);
    root.AddPod(state.depth);
    root.AddPod(state.points);
    root.AddPod(state.deaths);
    root.AddPod(static_cast<std::uint8_t>(state.multiplayer_respawn_mode));
    root.AddPod(state.sac_altar_favor);
    root.AddPod(state.sac_altar_reward_tier);
    root.AddBool(state.game_over);
    root.AddBool(state.win);
    root.AddPod(static_cast<std::uint8_t>(state.quest_state.quest_id));
    root.AddBool(state.quest_state.classic.made_black_market);
    root.AddBool(state.quest_state.classic.made_udjat_eye);
    root.AddBool(state.quest_state.classic.has_udjat_eye);
    root.AddBool(state.quest_state.classic.made_moai);
    root.AddBool(state.quest_state.classic.has_hedjet);
    root.AddBool(state.quest_state.classic.has_sceptre);
    root.AddBool(state.quest_state.classic.has_book_of_dead);

    FingerprintWriter stage;
    AddStageFingerprint(stage, state.stage, false);

    FingerprintWriter players;
    AddNetworkPlayerRegistryFingerprint(players, state);

    FingerprintWriter tools;
    AddNetworkToolInventoryFingerprint(tools, state);

    std::vector<const Ent*> active_ents;
    active_ents.reserve(state.ents.ents.size());
    for (const Ent& ent : state.ents.ents) {
        if (ent.active) {
            active_ents.push_back(&ent);
        }
    }
    std::sort(active_ents.begin(), active_ents.end(), [&state](const Ent* lhs, const Ent* rhs) {
        const network::NetEntId lhs_id = NetEntIdForVid(state, lhs->vid);
        const network::NetEntId rhs_id = NetEntIdForVid(state, rhs->vid);
        if (lhs_id != rhs_id) {
            return lhs_id < rhs_id;
        }
        return VidLess(lhs->vid, rhs->vid);
    });
    FingerprintWriter ents;
    ents.AddCount(active_ents.size());
    for (const Ent* const ent : active_ents) {
        AddNetworkEntFingerprint(ents, state, *ent);
    }

    return NetworkStateFingerprintComponents{
        .root = root.value,
        .stage = stage.value,
        .players = players.value,
        .tools = tools.value,
        .ents = ents.value,
    };
}

std::vector<NetworkEntFingerprint> ComputeNetworkEntFingerprints(const State& state) {
    std::vector<const Ent*> active_ents;
    active_ents.reserve(state.ents.ents.size());
    for (const Ent& ent : state.ents.ents) {
        if (ent.active) {
            active_ents.push_back(&ent);
        }
    }
    std::sort(active_ents.begin(), active_ents.end(), [&state](const Ent* lhs, const Ent* rhs) {
        const network::NetEntId lhs_id = NetEntIdForVid(state, lhs->vid);
        const network::NetEntId rhs_id = NetEntIdForVid(state, rhs->vid);
        if (lhs_id != rhs_id) {
            return lhs_id < rhs_id;
        }
        return VidLess(lhs->vid, rhs->vid);
    });

    std::vector<NetworkEntFingerprint> result;
    result.reserve(active_ents.size());
    for (const Ent* const ent : active_ents) {
        FingerprintWriter writer;
        AddNetworkEntFingerprint(writer, state, *ent);
        result.push_back(NetworkEntFingerprint{
            .net_ent_id = NetEntIdForVid(state, ent->vid),
            .type = static_cast<std::uint16_t>(ent->type_),
            .hash = writer.value,
        });
    }
    return result;
}

} // namespace splonks
