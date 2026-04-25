#pragma once

#include "quest.hpp"

#include <vector>

namespace splonks::stage_gen::classic {

enum class RoomCode : int {
    Side = 0,
    Main = 1,
    Drop = 2,
    Exit = 3,
    ShopLeft = 4,
    ShopRight = 5,
    Special6 = 6,
    Special7 = 7,
    Special8 = 8,
    Special9 = 9,
    SnakePitTop = 10,
    SnakePitBottom = 11,
};

using RoomCodeGrid = std::vector<std::vector<int>>;

struct StageLayout {
    UVec2 layout_size = UVec2::New(0, 0);
    UVec2 path_layout_size = UVec2::New(0, 0);
    IVec2 start_room = IVec2::New(0, 0);
    IVec2 end_room = IVec2::New(0, 0);
    RoomCodeGrid room_codes;
    std::vector<IVec2> path;
    bool jungle_lake = false;
};

StageLayout GenerateClassicRoomLayout(int level_number, const StageConfig& stage_config);
void ValidateClassicRoomLayoutPasses(const std::vector<StagePassConfig>& passes);
const char* GetClassicRoomCodeDebugLabel(int room_code);

} // namespace splonks::stage_gen::classic

