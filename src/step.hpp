#pragma once

#include "audio.hpp"
#include "graphics.hpp"
#include "state.hpp"

#include <cstdint>

namespace splonks {

constexpr std::uint32_t kFramesPerSecond = 60;
constexpr float kTimestep = 1.0F / static_cast<float>(kFramesPerSecond);

enum class SimulationTickMode {
    Normal,
    ReplayNoNetwork,
};

void Step(State& state, Audio& audio, Graphics& graphics, float dt);
void StepSingleTick(State& state, Audio& audio, Graphics& graphics);
void StepSingleTickWithMode(
    State& state,
    Audio& audio,
    Graphics& graphics,
    SimulationTickMode mode
);
void StepTitle(State& state, Audio& audio);
void StepPlaying(
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt,
    SimulationTickMode mode = SimulationTickMode::Normal
);
void StepStageTransition(
    State& state,
    Audio& audio,
    Graphics& graphics,
    SimulationTickMode mode = SimulationTickMode::Normal
);
void StepGameOver(
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt,
    SimulationTickMode mode = SimulationTickMode::Normal
);
void StepWin(State& state, Audio& audio, Graphics& graphics);

} // namespace splonks
