#pragma once

#include "entities/baseball_bat.hpp"
#include "entities/bat.hpp"
#include "entities/block.hpp"
#include "entities/bomb.hpp"
#include "entities/breakaway_container.hpp"
#include "entities/common.hpp"
#include "entities/ghost_ball.hpp"
#include "entities/jetpack.hpp"
#include "entities/money.hpp"
#include "entities/mouse_trailer.hpp"
#include "entities/player.hpp"
#include "entities/rock.hpp"
#include "entities/rope.hpp"
#include "entities/stomp_pad.hpp"

namespace splonks::entities {

using EntitySetupFn = void (*)(Entity&);

EntitySetupFn GetEntitySetupFn(EntityType type_);
void SetEntityByType(Entity& entity, EntityType type_);

} // namespace splonks::entities
