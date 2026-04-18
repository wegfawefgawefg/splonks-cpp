#pragma once

#include "entity/archetype.hpp"

#include <cstddef>
#include <optional>

namespace splonks::entities::basic_exit {

struct ExitPrompt {
    std::size_t entity_idx = 0;
    const char* action_text = "";
    const char* message_text = "";
    bool show_down_arrow = false;
    bool allowed = false;
};

extern const EntityArchetype kBasicExitArchetype;

std::optional<std::size_t> FindOverlappingBasicExitEntityIdx(
    const Entity& entity,
    const State& state,
    const Graphics& graphics
);

std::optional<std::size_t> FindOverlappingBasicExitEntityIdxForPlayer(
    const State& state,
    const Graphics& graphics
);

bool IsEntityTouchingBasicExit(
    const Entity& entity,
    const State& state,
    const Graphics& graphics
);

void StepEntityLogicAsBasicExit(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::basic_exit
