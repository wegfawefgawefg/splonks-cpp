#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::baseball_bat {

enum class SwingStage {
    Back,
    Above,
    Swing,
};

void SetEntityBaseballBat(Entity& entity);
void StepBaseballBat(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsBaseballBat(std::size_t entity_idx, State& state, Audio& audio, float dt);
bool IsStuff(EntityType type_);

} // namespace splonks::entities::baseball_bat
