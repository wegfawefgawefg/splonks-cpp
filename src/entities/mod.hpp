#pragma once

#include "entities/altar.hpp"
#include "entities/arrow_trap.hpp"
#include "entities/baseball_bat.hpp"
#include "entities/bat.hpp"
#include "entities/block.hpp"
#include "entities/bomb.hpp"
#include "entities/breakaway_container.hpp"
#include "entities/caveman.hpp"
#include "entities/chest.hpp"
#include "entities/common.hpp"
#include "entities/damsel.hpp"
#include "entities/dice.hpp"
#include "entities/emerald_big.hpp"
#include "entities/gear_items.hpp"
#include "entities/ghost_ball.hpp"
#include "entities/giant_tiki_head.hpp"
#include "entities/gold_idol.hpp"
#include "entities/jetpack.hpp"
#include "entities/kali_head.hpp"
#include "entities/lantern.hpp"
#include "entities/mattock.hpp"
#include "entities/money.hpp"
#include "entities/mouse_trailer.hpp"
#include "entities/player.hpp"
#include "entities/rock.hpp"
#include "entities/rope.hpp"
#include "entities/ruby_big.hpp"
#include "entities/sac_altar.hpp"
#include "entities/scarab.hpp"
#include "entities/shopkeeper.hpp"
#include "entities/sign.hpp"
#include "entities/sapphire_big.hpp"
#include "entities/snake.hpp"
#include "entities/spider_hang.hpp"
#include "entities/stomp_pad.hpp"

namespace splonks::entities {

using EntitySetupFn = void (*)(Entity&);

EntitySetupFn GetEntitySetupFn(EntityType type_);
void SetEntityByType(Entity& entity, EntityType type_);

} // namespace splonks::entities
