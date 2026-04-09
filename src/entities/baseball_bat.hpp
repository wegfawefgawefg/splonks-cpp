#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::baseball_bat {

constexpr std::uint32_t kBatContactCooldownFrames = 9;

enum class SwingStage {
    Back,
    Above,
    Swing,
};

void SetEntityBaseballBat(Entity& entity);
bool TryApplyBatContactToEntity(
    std::size_t bat_entity_idx,
    std::size_t other_entity_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
void StepBaseballBat(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio);
void StepEntityPhysicsAsBaseballBat(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
bool IsStuff(EntityType type_);

} // namespace splonks::entities::baseball_bat
