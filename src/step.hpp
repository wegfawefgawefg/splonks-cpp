#pragma once

#include "audio.hpp"
#include "graphics.hpp"
#include "state.hpp"

namespace splonks {

constexpr unsigned int kFramesPerSecond = 60;
constexpr float kTimestep = 1.0F / static_cast<float>(kFramesPerSecond);

void Step(State& state, Audio& audio, Graphics& graphics, float dt);
void StepSingleTick(State& state, Audio& audio, Graphics& graphics);
void StepTitle(State& state, Audio& audio);
void StepPlaying(State& state, Audio& audio, Graphics& graphics, float dt);
void StepStageTransition(State& state, Audio& audio, Graphics& graphics);
void StepGameOver(State& state, Audio& audio, Graphics& graphics, float dt);
void StepWin(State& state, Audio& audio, Graphics& graphics);

} // namespace splonks
