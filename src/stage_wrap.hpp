#pragma once

namespace splonks {

struct Graphics;
struct State;

void ApplyToroidalWrapSettings(
    State& state,
    Graphics& graphics,
    bool wrap_x,
    bool wrap_y,
    unsigned int padding_tiles,
    bool camera_clamp_enabled
);

} // namespace splonks
