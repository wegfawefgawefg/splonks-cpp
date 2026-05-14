#pragma once

#include "room.hpp"

#include <vector>

namespace splonks::stage_gen::test {

std::vector<std::vector<MetaTile>> GetRoomTemplate(RoomType room_type, DetRng& det_rng);

} // namespace splonks::stage_gen::test
