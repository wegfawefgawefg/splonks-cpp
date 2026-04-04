#pragma once

#include "room.hpp"

#include <vector>

namespace splonks::stage_gen::test {

std::vector<std::vector<TemplateTile>> GetRoomTemplate(RoomType room_type);

} // namespace splonks::stage_gen::test
