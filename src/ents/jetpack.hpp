#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::jetpack {

constexpr float kTravelSoundDistInterval = 8.0F;
constexpr float kFuel = 120.0F;

extern const EntSpec kJetPackSpec;

void OnDeathAsJetpack(std::size_t ent_idx, State& state, Audio& audio);
EntDamageEffectResult OnDamageAsJetpack(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
);
void OnUseAsJetpack(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio);
void StepEntLogicAsJetpack(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::jetpack
