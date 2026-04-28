#pragma once

#include "math_types.hpp"

namespace splonks {

struct Graphics;
struct State;
struct Audio;

constexpr int kDefaultStageRotationFrames = 180;

enum class StageRotationWrapPolicy {
    DoNotChangeWrap,
    SwapXYWrap,
};

bool IsStageRotationActive(const State& state);
void StartStageRotation(State& state, Graphics& graphics, Audio& audio, int quarter_turns);
void StepStageRotation(State& state, Graphics& graphics);

} // namespace splonks
