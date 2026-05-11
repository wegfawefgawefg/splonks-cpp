#include "state_fingerprint.hpp"

#include "entity.hpp"
#include "network/net_session.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

namespace splonks {

namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

struct FingerprintWriter {
    std::uint64_t value = kFnvOffsetBasis;

    void AddBytes(const void* bytes, std::size_t size) {
        const auto* data = static_cast<const std::uint8_t*>(bytes);
        for (std::size_t i = 0; i < size; ++i) {
            value ^= static_cast<std::uint64_t>(data[i]);
            value *= kFnvPrime;
        }
    }

    template <typename T>
    void AddPod(const T& pod) {
        AddBytes(&pod, sizeof(T));
    }

    void AddBool(bool value_) {
        const std::uint8_t byte = value_ ? 1U : 0U;
        AddPod(byte);
    }

    void AddFloat(float value_) {
        std::uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value_));
        std::memcpy(&bits, &value_, sizeof(bits));
        AddPod(bits);
    }

    void AddString(const std::string& text) {
        AddPod(text.size());
        AddBytes(text.data(), text.size());
    }

    void AddVec2(const Vec2& vec) {
        AddFloat(vec.x);
        AddFloat(vec.y);
    }

    void AddIVec2(const IVec2& vec) {
        AddPod(vec.x);
        AddPod(vec.y);
    }

    void AddVid(const VID& vid) {
        AddPod(vid.id);
    }

    void AddOptionalVid(const std::optional<VID>& vid) {
        AddBool(vid.has_value());
        if (vid.has_value()) {
            AddVid(*vid);
        }
    }
};

void AddStageFingerprint(FingerprintWriter& writer, const Stage& stage, bool include_cache_generation) {
    writer.AddString(stage.quest_id);
    writer.AddString(stage.quest_stage_id);
    writer.AddString(stage.route_label);
    writer.AddPod(stage.quest_level_number);
    writer.AddBool(stage.generation_seed.has_value());
    if (stage.generation_seed.has_value()) {
        writer.AddPod(*stage.generation_seed);
    }
    writer.AddPod(static_cast<int>(stage.stage_type));
    writer.AddFloat(stage.gravity);
    if (include_cache_generation) {
        writer.AddPod(stage.tile_change_generation);
    }

    const UVec2 dims = stage.GetStageDims();
    writer.AddPod(dims.x);
    writer.AddPod(dims.y);
    for (unsigned int y = 0; y < dims.y; ++y) {
        const std::size_t row_y = static_cast<std::size_t>(y);
        for (unsigned int x = 0; x < dims.x; ++x) {
            const std::size_t col_x = static_cast<std::size_t>(x);
            const auto read_tile_grid = [&](const std::vector<std::vector<Tile>>& grid, Tile fallback) {
                if (row_y >= grid.size() || col_x >= grid[row_y].size()) {
                    return fallback;
                }
                return grid[row_y][col_x];
            };
            const auto read_rotation_grid = [&]() {
                if (row_y >= stage.tile_rotations.size() || col_x >= stage.tile_rotations[row_y].size()) {
                    return kTileRotation0;
                }
                return stage.tile_rotations[row_y][col_x];
            };
            const auto read_float_grid = [](const std::vector<std::vector<float>>& grid, std::size_t y_, std::size_t x_) {
                if (y_ >= grid.size() || x_ >= grid[y_].size()) {
                    return 0.0F;
                }
                return grid[y_][x_];
            };

            writer.AddPod(static_cast<std::uint16_t>(read_tile_grid(stage.tiles, Tile::Air)));
            writer.AddPod(static_cast<std::uint8_t>(read_rotation_grid()));
            writer.AddPod(static_cast<std::uint16_t>(read_tile_grid(stage.backwall_tiles, Tile::Air)));
            writer.AddPod(static_cast<std::uint16_t>(read_tile_grid(stage.fluid_tiles, Tile::Air)));
            writer.AddFloat(read_float_grid(stage.fluid_amount, row_y, col_x));

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
    std::sort(lights.begin(), lights.end(), [](const StageLight& lhs, const StageLight& rhs) {
        return lhs.vid.id < rhs.vid.id;
    });
    writer.AddPod(lights.size());
    for (const StageLight& light : lights) {
        writer.AddVid(light.vid);
        writer.AddIVec2(light.tile_pos);
        writer.AddPod(light.radius);
    }
}

void AddEffectFingerprint(FingerprintWriter& writer, const BoxedEntityEffects& effects_box) {
    const EntityEffects* const effects = effects_box.get();
    writer.AddBool(effects != nullptr);
    if (effects == nullptr) {
        return;
    }

    writer.AddPod(effects->count);
    for (std::uint8_t i = 0; i < effects->count && i < effects->effects.size(); ++i) {
        const EffectInstance& effect = effects->effects[i];
        writer.AddPod(static_cast<std::uint16_t>(effect.id));
        writer.AddPod(effect.count);
        writer.AddFloat(effect.value);
        writer.AddPod(effect.frames_remaining);
    }
}

void AddEntityFingerprint(FingerprintWriter& writer, const Entity& entity) {
    writer.AddBool(entity.active);
    writer.AddPod(static_cast<std::uint16_t>(entity.type_));
    writer.AddVid(entity.vid);
    if (!entity.active) {
        return;
    }

    writer.AddBool(entity.has_physics);
    writer.AddBool(entity.can_collide);
    writer.AddBool(entity.grounded);
    writer.AddBool(entity.holding);
    writer.AddBool(entity.wanted);
    writer.AddBool(entity.render_enabled);
    writer.AddVec2(entity.pos);
    writer.AddVec2(entity.vel);
    writer.AddVec2(entity.acc);
    writer.AddVec2(entity.size);
    writer.AddFloat(entity.rotation);
    writer.AddPod(entity.coyote_time);
    writer.AddPod(entity.stun_timer);
    writer.AddPod(entity.fall_timer);
    writer.AddPod(static_cast<std::uint8_t>(entity.facing));
    writer.AddPod(static_cast<std::uint8_t>(entity.draw_layer));
    writer.AddPod(static_cast<std::uint8_t>(entity.condition));
    writer.AddPod(static_cast<std::uint8_t>(entity.ai_state));
    writer.AddPod(static_cast<std::uint8_t>(entity.damage_vulnerability));
    writer.AddPod(entity.movement_flags);
    writer.AddPod(entity.health);
    writer.AddOptionalVid(entity.back_vid);
    writer.AddOptionalVid(entity.holding_vid);
    writer.AddOptionalVid(entity.held_by_vid);
    writer.AddOptionalVid(entity.entity_a);
    writer.AddOptionalVid(entity.entity_b);
    writer.AddOptionalVid(entity.entity_c);
    writer.AddOptionalVid(entity.entity_d);
    writer.AddPod(entity.stage_exit_id);
    writer.AddPod(entity.money);
    writer.AddFloat(entity.counter_a);
    writer.AddFloat(entity.counter_b);
    writer.AddFloat(entity.counter_c);
    writer.AddFloat(entity.counter_d);
    writer.AddFloat(entity.light_strength);
    writer.AddFloat(entity.light_color.r);
    writer.AddFloat(entity.light_color.g);
    writer.AddFloat(entity.light_color.b);
    writer.AddPod(entity.light_radius);
    writer.AddIVec2(entity.point_a);
    writer.AddIVec2(entity.point_b);
    writer.AddIVec2(entity.point_c);
    writer.AddIVec2(entity.point_d);
    writer.AddPod(entity.frame_data_animator.animation_id);
    writer.AddPod(entity.frame_data_animator.current_frame);
    writer.AddFloat(entity.frame_data_animator.current_time);
    writer.AddFloat(entity.frame_data_animator.speed);
    writer.AddBool(entity.frame_data_animator.animate);
    writer.AddBool(entity.frame_data_animator.loop);
    writer.AddBool(entity.frame_data_animator.finished);
    AddEffectFingerprint(writer, entity.effects);
}

void AddPlayerRegistryFingerprint(FingerprintWriter& writer, const PlayerRegistry& players) {
    writer.AddPod(players.slots.size());
    for (const PlayerSlot& slot : players.slots) {
        writer.AddPod(slot.player_id);
        writer.AddBool(slot.entity_vid.has_value());
        if (slot.entity_vid.has_value()) {
            writer.AddVid(*slot.entity_vid);
        }
        writer.AddPod(static_cast<std::uint8_t>(slot.connection_kind));
        writer.AddBool(slot.connected);
        writer.AddBool(slot.primary_local);
    }
}

void AddToolInventoryFingerprint(FingerprintWriter& writer, const EntityToolInventoryState& inventory) {
    writer.AddPod(inventory.tool_states.size());
    for (const EntityToolState& tool_state : inventory.tool_states) {
        writer.AddVid(tool_state.owner_vid);
        for (const ToolSlot& slot : tool_state.slots) {
            writer.AddPod(static_cast<std::uint16_t>(slot.kind));
            writer.AddPod(slot.count);
            writer.AddPod(slot.cooldown);
            writer.AddBool(slot.active);
        }
    }
}

network::NetEntityId NetEntityIdForVid(const State& state, VID vid) {
    return state.net_session.FindNetEntityId(vid).value_or(network::kInvalidNetEntityId);
}

void AddNetworkVid(FingerprintWriter& writer, const State& state, const VID& vid) {
    writer.AddPod(NetEntityIdForVid(state, vid));
}

void AddNetworkOptionalVid(
    FingerprintWriter& writer,
    const State& state,
    const std::optional<VID>& vid
) {
    writer.AddBool(vid.has_value());
    if (vid.has_value()) {
        AddNetworkVid(writer, state, *vid);
    }
}

void AddNetworkEntityFingerprint(FingerprintWriter& writer, const State& state, const Entity& entity) {
    writer.AddPod(NetEntityIdForVid(state, entity.vid));
    writer.AddBool(entity.active);
    writer.AddPod(static_cast<std::uint16_t>(entity.type_));
    if (!entity.active) {
        return;
    }

    writer.AddBool(entity.has_physics);
    writer.AddBool(entity.can_collide);
    writer.AddBool(entity.grounded);
    writer.AddBool(entity.holding);
    writer.AddBool(entity.wanted);
    writer.AddBool(entity.render_enabled);
    writer.AddVec2(entity.pos);
    writer.AddVec2(entity.vel);
    writer.AddVec2(entity.acc);
    writer.AddVec2(entity.size);
    writer.AddFloat(entity.rotation);
    writer.AddPod(entity.coyote_time);
    writer.AddPod(entity.stun_timer);
    writer.AddPod(entity.fall_timer);
    writer.AddPod(static_cast<std::uint8_t>(entity.facing));
    writer.AddPod(static_cast<std::uint8_t>(entity.draw_layer));
    writer.AddPod(static_cast<std::uint8_t>(entity.condition));
    writer.AddPod(static_cast<std::uint8_t>(entity.ai_state));
    writer.AddPod(static_cast<std::uint8_t>(entity.damage_vulnerability));
    writer.AddPod(entity.movement_flags);
    writer.AddPod(entity.health);
    AddNetworkOptionalVid(writer, state, entity.back_vid);
    AddNetworkOptionalVid(writer, state, entity.holding_vid);
    AddNetworkOptionalVid(writer, state, entity.held_by_vid);
    AddNetworkOptionalVid(writer, state, entity.entity_a);
    AddNetworkOptionalVid(writer, state, entity.entity_b);
    AddNetworkOptionalVid(writer, state, entity.entity_c);
    AddNetworkOptionalVid(writer, state, entity.entity_d);
    writer.AddPod(entity.stage_exit_id);
    writer.AddPod(entity.money);
    writer.AddFloat(entity.counter_a);
    writer.AddFloat(entity.counter_b);
    writer.AddFloat(entity.counter_c);
    writer.AddFloat(entity.counter_d);
    writer.AddFloat(entity.light_strength);
    writer.AddFloat(entity.light_color.r);
    writer.AddFloat(entity.light_color.g);
    writer.AddFloat(entity.light_color.b);
    writer.AddPod(entity.light_radius);
    writer.AddIVec2(entity.point_a);
    writer.AddIVec2(entity.point_b);
    writer.AddIVec2(entity.point_c);
    writer.AddIVec2(entity.point_d);
    writer.AddPod(entity.frame_data_animator.animation_id);
    writer.AddPod(entity.frame_data_animator.current_frame);
    writer.AddFloat(entity.frame_data_animator.current_time);
    writer.AddFloat(entity.frame_data_animator.speed);
    writer.AddBool(entity.frame_data_animator.animate);
    writer.AddBool(entity.frame_data_animator.loop);
    writer.AddBool(entity.frame_data_animator.finished);
    AddEffectFingerprint(writer, entity.effects);
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

    writer.AddPod(slots.size());
    for (const PlayerSlot* const slot : slots) {
        writer.AddPod(slot->player_id);
        writer.AddBool(slot->connected);
        writer.AddBool(slot->entity_vid.has_value());
        if (slot->entity_vid.has_value()) {
            AddNetworkVid(writer, state, *slot->entity_vid);
        }
    }
}

void AddNetworkToolInventoryFingerprint(FingerprintWriter& writer, const State& state) {
    std::vector<const EntityToolState*> tool_states;
    tool_states.reserve(state.entity_tools.tool_states.size());
    for (const EntityToolState& tool_state : state.entity_tools.tool_states) {
        if (NetEntityIdForVid(state, tool_state.owner_vid) == network::kInvalidNetEntityId) {
            continue;
        }
        tool_states.push_back(&tool_state);
    }
    std::sort(
        tool_states.begin(),
        tool_states.end(),
        [&state](const EntityToolState* lhs, const EntityToolState* rhs) {
            return NetEntityIdForVid(state, lhs->owner_vid) <
                   NetEntityIdForVid(state, rhs->owner_vid);
        }
    );

    writer.AddPod(tool_states.size());
    for (const EntityToolState* const tool_state : tool_states) {
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

CanonicalStateFingerprint ComputeCanonicalStateFingerprint(const State& state) {
    FingerprintWriter writer;
    writer.AddPod(static_cast<std::uint8_t>(state.mode));
    writer.AddPod(state.frame);
    writer.AddPod(state.stage_frame);
    writer.AddPod(state.depth);
    writer.AddPod(state.points);
    writer.AddPod(state.deaths);
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
    AddToolInventoryFingerprint(writer, state.entity_tools);

    writer.AddPod(state.entity_manager.entities.size());
    for (const Entity& entity : state.entity_manager.entities) {
        AddEntityFingerprint(writer, entity);
    }

    int active_entities = 0;
    for (const Entity& entity : state.entity_manager.entities) {
        if (entity.active) {
            ++active_entities;
        }
    }

    std::ostringstream summary;
    summary << "stage=" << state.stage.quest_stage_id
            << " frame=" << state.frame
            << " stage_frame=" << state.stage_frame
            << " entities=" << active_entities
            << " tiles=" << state.stage.GetTileWidth() << "x" << state.stage.GetTileHeight();
    return CanonicalStateFingerprint{
        .value = writer.value,
        .summary = summary.str(),
    };
}

CanonicalStateFingerprint ComputeNetworkStateFingerprint(const State& state) {
    FingerprintWriter writer;
    writer.AddPod(state.frame);
    writer.AddPod(state.stage_frame);
    writer.AddPod(state.depth);
    writer.AddPod(state.points);
    writer.AddPod(state.deaths);
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
    AddStageFingerprint(writer, state.stage, false);
    AddNetworkPlayerRegistryFingerprint(writer, state);
    AddNetworkToolInventoryFingerprint(writer, state);

    std::vector<const Entity*> active_entities;
    active_entities.reserve(state.entity_manager.entities.size());
    for (const Entity& entity : state.entity_manager.entities) {
        if (entity.active) {
            active_entities.push_back(&entity);
        }
    }
    std::sort(
        active_entities.begin(),
        active_entities.end(),
        [&state](const Entity* lhs, const Entity* rhs) {
            const network::NetEntityId lhs_id = NetEntityIdForVid(state, lhs->vid);
            const network::NetEntityId rhs_id = NetEntityIdForVid(state, rhs->vid);
            if (lhs_id != rhs_id) {
                return lhs_id < rhs_id;
            }
            return lhs->vid.id < rhs->vid.id;
        }
    );

    writer.AddPod(active_entities.size());
    for (const Entity* const entity : active_entities) {
        AddNetworkEntityFingerprint(writer, state, *entity);
    }

    std::ostringstream summary;
    summary << "stage=" << state.stage.quest_stage_id
            << " frame=" << state.frame
            << " stage_frame=" << state.stage_frame
            << " active_entities=" << active_entities.size()
            << " tiles=" << state.stage.GetTileWidth() << "x" << state.stage.GetTileHeight();
    return CanonicalStateFingerprint{
        .value = writer.value,
        .summary = summary.str(),
    };
}

} // namespace splonks
