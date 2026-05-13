#pragma once

#include "ent/spec.hpp"

#include <cstddef>
#include <optional>

namespace splonks::ents::basic_exit {

struct ExitPrompt {
    std::size_t ent_idx = 0;
    const char* action_text = "";
    const char* message_text = "";
    bool show_down_arrow = false;
    bool allowed = false;
};

extern const EntSpec kBasicExitSpec;

std::optional<std::size_t> FindOverlappingBasicExitEntIdx(
    const Ent& ent,
    const State& state,
    const Graphics& graphics
);

bool IsEntTouchingBasicExit(
    const Ent& ent,
    const State& state,
    const Graphics& graphics
);

void StepEntLogicAsBasicExit(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::basic_exit
