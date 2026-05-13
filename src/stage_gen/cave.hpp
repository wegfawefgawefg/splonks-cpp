#pragma once

#include "room.hpp"

#include <vector>

namespace splonks::stage_gen::cave {

std::vector<std::vector<MetaTile>> GetRoomTemplate(RoomType room_type);

} // namespace splonks::stage_gen::cave
