#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::spider {

extern const EntSpec kSpiderSpec;
extern const EntSpec kRageSpiderSpec;
extern const EntSpec kGiantSpiderSpec;

void OnDeathAsGiantSpider(std::size_t ent_idx, State& state, Audio& audio);

} // namespace splonks::ents::spider
