#pragma once

#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "math_types.hpp"
#include "vid.hpp"

namespace splonks {
struct Graphics;
struct State;
} // namespace splonks

namespace splonks::entities::teleporter {

extern const EntityArchetype kTeleporterArchetype;
extern const EntityArchetype kTeleporterBackpackArchetype;

} // namespace splonks::entities::teleporter
