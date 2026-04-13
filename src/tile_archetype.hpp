#pragma once

#include "audio.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "tile.hpp"

#include <optional>

namespace splonks {

struct State;

enum class TileFamily {
    Neutral,
    Cave,
    Ice,
    Jungle,
    Temple,
    Boss,
};

using TileOnBreak = void (*)(const IVec2& tile_pos, State& state, Audio& audio);

struct TileArchetype {
    Tile tile = Tile::Air;
    bool solid = false;
    bool climbable = false;
    bool transparent = true;
    bool hangable = false;
    TileFamily family = TileFamily::Neutral;
    std::optional<SoundEffect> collide_sound = std::nullopt;
    std::optional<SoundEffect> break_sound = std::nullopt;
    std::optional<FrameDataId> break_animation = std::nullopt;
    TileOnBreak on_break = nullptr;
    const char* debug_name = "Unknown";
};

const TileArchetype& GetTileArchetype(Tile tile);

} // namespace splonks
