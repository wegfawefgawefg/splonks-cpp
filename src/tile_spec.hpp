#pragma once

#include "audio.hpp"
#include "effects/effect_id.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "tile.hpp"

#include <optional>

namespace splonks {

struct State;

using TileOnBreak = void (*)(const IVec2& tile_pos, State& state, Audio& audio);

struct TileSpec {
    Tile tile = Tile::Air;
    bool solid = false;
    bool one_way_top_solid = false;
    bool climbable = false;
    std::uint8_t climbable_rotation_mask = kTileRotationBitAll;
    bool transparent = true;
    bool hangable = false;
    bool render_enabled = true;
    bool simulated_fluid = false;
    float friction = 0.85F;
    std::optional<AudioAssetId> collide_sound = std::nullopt;
    std::optional<AudioAssetId> break_sound = std::nullopt;
    std::optional<AFrameId> break_anim = std::nullopt;
    std::optional<EffectId> effect_while_inside = std::nullopt;
    TileOnBreak on_break = nullptr;
    const char* debug_name = "Unknown";
};

const TileSpec& GetTileSpec(Tile tile);
float GetTileFriction(Tile tile);

} // namespace splonks
