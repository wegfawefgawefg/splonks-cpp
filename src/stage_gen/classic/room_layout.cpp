#include "stage_gen/classic/room_layout.hpp"

#include "utils.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace splonks::stage_gen::classic {

namespace {

UVec2 GetPathLayoutSize(const StageConfig& stage_config) {
    return stage_config.path_layout_size.x == 0 ? stage_config.layout_size
                                                : stage_config.path_layout_size;
}

RoomCodeGrid MakeBlankRoomCodes(const UVec2& dims) {
    return RoomCodeGrid(
        static_cast<std::size_t>(dims.y),
        std::vector<int>(static_cast<std::size_t>(dims.x), 0));
}

bool IsRoomCoordInside(const StageLayout& layout, int x, int y) {
    return x >= 0 && y >= 0 && x < static_cast<int>(layout.layout_size.x) &&
           y < static_cast<int>(layout.layout_size.y);
}

int GetRoomCode(const StageLayout& layout, int x, int y) {
    if (!IsRoomCoordInside(layout, x, y)) {
        return -1;
    }
    return layout.room_codes[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

void SetRoomCode(StageLayout& layout, int x, int y, int code) {
    if (!IsRoomCoordInside(layout, x, y)) {
        return;
    }
    layout.room_codes[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = code;
}

void ResizeRoomCodeGrid(StageLayout& layout, const UVec2& new_size) {
    RoomCodeGrid resized = MakeBlankRoomCodes(new_size);
    const unsigned int copy_h = std::min(layout.layout_size.y, new_size.y);
    const unsigned int copy_w = std::min(layout.layout_size.x, new_size.x);
    for (unsigned int y = 0; y < copy_h; ++y) {
        for (unsigned int x = 0; x < copy_w; ++x) {
            resized[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                layout.room_codes[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
        }
    }
    layout.layout_size = new_size;
    layout.room_codes = std::move(resized);
}

void TryPlaceSnakePit(StageLayout& layout, int chance_denominator) {
    if (layout.path_layout_size.y < 3) {
        return;
    }
    for (int y = 0; y + 2 < static_cast<int>(layout.path_layout_size.y); ++y) {
        for (int x = 0; x < static_cast<int>(layout.path_layout_size.x); ++x) {
            if (GetRoomCode(layout, x, y) != 0 || GetRoomCode(layout, x, y + 1) != 0 ||
                GetRoomCode(layout, x, y + 2) != 0) {
                continue;
            }
            if (chance_denominator <= 0 || rng::RandomIntInclusive(1, chance_denominator) != 1) {
                continue;
            }

            SetRoomCode(layout, x, y, static_cast<int>(RoomCode::Drop));
            SetRoomCode(layout, x, y + 1, static_cast<int>(RoomCode::SnakePitTop));

            if (y == 0 && y + 3 < static_cast<int>(layout.path_layout_size.y) &&
                GetRoomCode(layout, x, y + 3) == 0) {
                SetRoomCode(layout, x, y + 2, static_cast<int>(RoomCode::SnakePitTop));
                SetRoomCode(layout, x, y + 3, static_cast<int>(RoomCode::SnakePitBottom));
            } else {
                SetRoomCode(layout, x, y + 2, static_cast<int>(RoomCode::SnakePitBottom));
            }
            return;
        }
    }
}

void TryPlaceShop(StageLayout& layout, int level_number, const StagePassConfig& pass) {
    const int shop_min_level = pass.GetInt("min_level_number", 2);
    const bool shop_uses_level_number = pass.GetBool("chance_uses_level_number", true);
    const bool allow_shop = pass.enabled && level_number >= shop_min_level;
    const int shop_chance_denominator = shop_uses_level_number ? level_number : shop_min_level;
    if (!allow_shop || shop_chance_denominator <= 0 ||
        rng::RandomIntInclusive(1, shop_chance_denominator) > 2) {
        return;
    }

    RoomCodeGrid room_poss = MakeBlankRoomCodes(layout.layout_size);
    int candidate_count = 0;

    for (int y = 0; y < static_cast<int>(layout.path_layout_size.y); ++y) {
        for (int x = 0; x < static_cast<int>(layout.path_layout_size.x); ++x) {
            if (GetRoomCode(layout, x, y) != 0) {
                continue;
            }

            if (x + 1 < static_cast<int>(layout.path_layout_size.x)) {
                const int right_code = GetRoomCode(layout, x + 1, y);
                if (right_code == static_cast<int>(RoomCode::Main) ||
                    right_code == static_cast<int>(RoomCode::Drop)) {
                    room_poss[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                        static_cast<int>(RoomCode::ShopLeft);
                    candidate_count += 1;
                    continue;
                }
            }

            if (x > 0) {
                const int left_code = GetRoomCode(layout, x - 1, y);
                if (left_code == static_cast<int>(RoomCode::Main) ||
                    left_code == static_cast<int>(RoomCode::Drop)) {
                    room_poss[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                        static_cast<int>(RoomCode::ShopRight);
                    candidate_count += 1;
                }
            }
        }
    }

    if (candidate_count <= 0) {
        return;
    }

    int target = rng::RandomIntInclusive(0, candidate_count - 1);
    for (int y = 0; y < static_cast<int>(layout.layout_size.y); ++y) {
        for (int x = 0; x < static_cast<int>(layout.layout_size.x); ++x) {
            if (room_poss[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == 0) {
                continue;
            }
            if (target == 0) {
                SetRoomCode(layout, x, y,
                            room_poss[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]);
                return;
            }
            target -= 1;
        }
    }
}

using ClassicRoomLayoutPassFn = void (*)(StageLayout&, int, const StagePassConfig&);

struct ClassicRoomLayoutPassDefinition {
    std::string_view name;
    ClassicRoomLayoutPassFn run = nullptr;
};

void RunSnakePitLayoutPass(StageLayout& layout, int, const StagePassConfig& pass) {
    if (pass.enabled) {
        TryPlaceSnakePit(layout, pass.GetInt("chance_denominator", 8));
    }
}

void RunShopLayoutPass(StageLayout& layout, int level_number, const StagePassConfig& pass) {
    TryPlaceShop(layout, level_number, pass);
}

void RunJungleLakeLayoutPass(StageLayout& layout, int, const StagePassConfig& pass) {
    if (!pass.enabled) {
        return;
    }
    if (layout.layout_size.x == 0 || layout.path_layout_size.y == 0) {
        return;
    }
    if (layout.layout_size.y <= layout.path_layout_size.y) {
        ResizeRoomCodeGrid(layout, UVec2::New(layout.layout_size.x, layout.path_layout_size.y + 1));
    }

    const int lake_top_row = static_cast<int>(layout.path_layout_size.y) - 1;
    const int lake_bottom_row = static_cast<int>(layout.path_layout_size.y);
    for (int x = 0; x < static_cast<int>(layout.layout_size.x); ++x) {
        if (!(layout.end_room.y == lake_top_row && layout.end_room.x == x)) {
            SetRoomCode(layout, x, lake_top_row, static_cast<int>(RoomCode::Special8));
        }
        SetRoomCode(layout, x, lake_bottom_row, static_cast<int>(RoomCode::Special7));
    }

    int mega_mouth_x = rng::RandomIntInclusive(0, static_cast<int>(layout.layout_size.x) - 1);
    while (layout.layout_size.x > 1 && mega_mouth_x == layout.end_room.x) {
        mega_mouth_x = rng::RandomIntInclusive(0, static_cast<int>(layout.layout_size.x) - 1);
    }
    SetRoomCode(layout, mega_mouth_x, lake_bottom_row, static_cast<int>(RoomCode::Special9));
    layout.jungle_lake = true;
}

void RunIceMoaiLayoutPass(StageLayout& layout, int level_number, const StagePassConfig& pass) {
    if (!pass.enabled || layout.path_layout_size.y < 2 || layout.path_layout_size.x == 0) {
        return;
    }
    int chance_denominator = 0;
    if (level_number == 9) chance_denominator = 4;
    else if (level_number == 10) chance_denominator = 3;
    else if (level_number == 11) chance_denominator = 2;
    else if (level_number == 12) chance_denominator = 1;
    if (chance_denominator <= 0 || rng::RandomIntInclusive(1, chance_denominator) != 1) {
        return;
    }
    const int max_row = std::min(static_cast<int>(layout.path_layout_size.y) - 1, 2);
    SetRoomCode(layout, rng::RandomIntInclusive(0, static_cast<int>(layout.path_layout_size.x) - 1),
                rng::RandomIntInclusive(1, max_row), static_cast<int>(RoomCode::Special6));
}

void RunIceAlienCraftLayoutPass(StageLayout& layout, int, const StagePassConfig& pass) {
    if (!pass.enabled || layout.path_layout_size.y < 2 || layout.path_layout_size.x < 2) {
        return;
    }
    const int chance_denominator = pass.GetInt("chance_denominator", 10);
    if (chance_denominator <= 0 || rng::RandomIntInclusive(1, chance_denominator) != 1) {
        return;
    }
    const int start_x = rng::RandomIntInclusive(0, static_cast<int>(layout.path_layout_size.x) - 2);
    const int max_row = std::min(static_cast<int>(layout.path_layout_size.y) - 1, 2);
    const int row = rng::RandomIntInclusive(1, max_row);
    for (int x = start_x; x < static_cast<int>(layout.path_layout_size.x); ++x) {
        SetRoomCode(layout, x, row,
                    x == start_x ? static_cast<int>(RoomCode::Special7)
                                 : x == static_cast<int>(layout.path_layout_size.x) - 1
                                       ? static_cast<int>(RoomCode::Special9)
                                       : static_cast<int>(RoomCode::Special8));
    }
}

void RunTempleSacPitLayoutPass(StageLayout& layout, int, const StagePassConfig& pass) {
    if (!pass.enabled || layout.path_layout_size.y < 4 || layout.path_layout_size.x == 0) {
        return;
    }
    const int chance_denominator = pass.GetInt("chance_denominator", 8);
    if (chance_denominator <= 0 || rng::RandomIntInclusive(1, chance_denominator) != 1) {
        return;
    }
    int column = rng::RandomIntInclusive(0, static_cast<int>(layout.path_layout_size.x) - 1);
    while (layout.path_layout_size.x > 1 && column == layout.end_room.x) {
        column = rng::RandomIntInclusive(0, static_cast<int>(layout.path_layout_size.x) - 1);
    }
    SetRoomCode(layout, column, 0, static_cast<int>(RoomCode::Special7));
    for (int y = 1; y < static_cast<int>(layout.path_layout_size.y) - 1; ++y) {
        SetRoomCode(layout, column, y, static_cast<int>(RoomCode::Special8));
    }
    SetRoomCode(layout, column, static_cast<int>(layout.path_layout_size.y) - 1,
                static_cast<int>(RoomCode::Special9));
}

void RunCityOfGoldXocLayoutPass(StageLayout& layout, int, const StagePassConfig& pass) {
    if (!pass.enabled || layout.path_layout_size.y == 0 || layout.path_layout_size.x == 0) {
        return;
    }
    const int row = std::min(2, static_cast<int>(layout.path_layout_size.y) - 1);
    SetRoomCode(layout, rng::RandomIntInclusive(0, static_cast<int>(layout.path_layout_size.x) - 1),
                row, static_cast<int>(RoomCode::Special6));
}

constexpr std::array<ClassicRoomLayoutPassDefinition, 7> kClassicRoomLayoutPasses = {{
    {"snake_pit", RunSnakePitLayoutPass},
    {"shop", RunShopLayoutPass},
    {"jungle_lake", RunJungleLakeLayoutPass},
    {"ice_moai", RunIceMoaiLayoutPass},
    {"ice_alien_craft", RunIceAlienCraftLayoutPass},
    {"temple_sac_pit", RunTempleSacPitLayoutPass},
    {"city_of_gold_xoc", RunCityOfGoldXocLayoutPass},
}};

const ClassicRoomLayoutPassDefinition* FindClassicRoomLayoutPass(std::string_view name) {
    for (const ClassicRoomLayoutPassDefinition& pass : kClassicRoomLayoutPasses) {
        if (pass.name == name) {
            return &pass;
        }
    }
    return nullptr;
}

void RunClassicRoomLayoutPass(StageLayout& layout, int level_number,
                              const StagePassConfig& pass) {
    const ClassicRoomLayoutPassDefinition* definition = FindClassicRoomLayoutPass(pass.name);
    if (definition == nullptr) {
        throw std::runtime_error("Unknown classic room layout pass: " + pass.name);
    }
    definition->run(layout, level_number, pass);
}

std::vector<IVec2> BuildPathFromRoomCodes(const StageLayout& layout,
                                          const IVec2& start_room, const IVec2& end_room) {
    std::vector<IVec2> path;
    IVec2 current = start_room;
    path.push_back(current);

    while (current != end_room) {
        const int code = GetRoomCode(layout, current.x, current.y);

        if ((code == static_cast<int>(RoomCode::Main) ||
             code == static_cast<int>(RoomCode::ShopLeft)) &&
            current.x + 1 < static_cast<int>(layout.path_layout_size.x)) {
            const int right_code = GetRoomCode(layout, current.x + 1, current.y);
            if (right_code == static_cast<int>(RoomCode::Main) ||
                right_code == static_cast<int>(RoomCode::Drop) ||
                right_code == static_cast<int>(RoomCode::Exit) ||
                right_code == static_cast<int>(RoomCode::ShopRight)) {
                current.x += 1;
                path.push_back(current);
                continue;
            }
        }

        if (current.y + 1 < static_cast<int>(layout.path_layout_size.y) &&
            (code == static_cast<int>(RoomCode::Main) || code == static_cast<int>(RoomCode::Drop) ||
             code == static_cast<int>(RoomCode::ShopLeft) ||
             code == static_cast<int>(RoomCode::ShopRight))) {
            const int below_code = GetRoomCode(layout, current.x, current.y + 1);
            if (below_code == static_cast<int>(RoomCode::Main) ||
                below_code == static_cast<int>(RoomCode::Drop) ||
                below_code == static_cast<int>(RoomCode::Exit)) {
                current.y += 1;
                path.push_back(current);
                continue;
            }
        }

        if ((code == static_cast<int>(RoomCode::Main) ||
             code == static_cast<int>(RoomCode::ShopRight)) &&
            current.x > 0) {
            const int left_code = GetRoomCode(layout, current.x - 1, current.y);
            if (left_code == static_cast<int>(RoomCode::Main) ||
                left_code == static_cast<int>(RoomCode::Drop) ||
                left_code == static_cast<int>(RoomCode::Exit) ||
                left_code == static_cast<int>(RoomCode::ShopLeft)) {
                current.x -= 1;
                path.push_back(current);
                continue;
            }
        }

        break;
    }

    return path;
}

} // namespace

StageLayout GenerateClassicRoomLayout(int level_number, const StageConfig& stage_config) {
    StageLayout layout;
    layout.layout_size = stage_config.layout_size;
    layout.path_layout_size = GetPathLayoutSize(stage_config);
    layout.room_codes = MakeBlankRoomCodes(layout.layout_size);

    int room_x = 0;
    int room_y = 0;
    int prev_x = 0;
    int prev_y = 0;
    SetRoomCode(layout, 0, 0, static_cast<int>(RoomCode::Main));

    while (room_y < static_cast<int>(layout.path_layout_size.y)) {
        bool moved_down = false;
        int move_roll = 5;

        if (room_x == 0) {
            move_roll = rng::RandomIntInclusive(3, 5);
        } else if (room_x == static_cast<int>(layout.path_layout_size.x) - 1) {
            move_roll = rng::RandomIntInclusive(5, 7);
        } else {
            move_roll = rng::RandomIntInclusive(1, 5);
        }

        if (move_roll < 3 || move_roll > 5) {
            if (room_x > 0 && GetRoomCode(layout, room_x - 1, room_y) == 0) {
                room_x -= 1;
            } else if (room_x + 1 < static_cast<int>(layout.path_layout_size.x) &&
                       GetRoomCode(layout, room_x + 1, room_y) == 0) {
                room_x += 1;
            } else {
                move_roll = 5;
            }
        } else if (move_roll == 3 || move_roll == 4) {
            if (room_x + 1 < static_cast<int>(layout.path_layout_size.x) &&
                GetRoomCode(layout, room_x + 1, room_y) == 0) {
                room_x += 1;
            } else if (room_x > 0 && GetRoomCode(layout, room_x - 1, room_y) == 0) {
                room_x -= 1;
            } else {
                move_roll = 5;
            }
        }

        if (move_roll == 5) {
            room_y += 1;
            moved_down = true;
            if (room_y < static_cast<int>(layout.path_layout_size.y)) {
                SetRoomCode(layout, prev_x, prev_y, static_cast<int>(RoomCode::Drop));
                SetRoomCode(layout, room_x, room_y, static_cast<int>(RoomCode::Exit));
            } else {
                layout.end_room = IVec2::New(room_x, room_y - 1);
            }
        }

        if (!moved_down) {
            SetRoomCode(layout, room_x, room_y, static_cast<int>(RoomCode::Main));
        }

        prev_x = room_x;
        prev_y = room_y;
    }

    for (const StagePassConfig& pass : stage_config.layout_passes) {
        RunClassicRoomLayoutPass(layout, level_number, pass);
    }

    layout.path = BuildPathFromRoomCodes(layout, layout.start_room, layout.end_room);
    return layout;
}

void ValidateClassicRoomLayoutPasses(const std::vector<StagePassConfig>& passes) {
    for (const StagePassConfig& pass : passes) {
        if (FindClassicRoomLayoutPass(pass.name) == nullptr) {
            throw std::runtime_error("Unknown classic room layout pass: " + pass.name);
        }
    }
}

const char* GetClassicRoomCodeDebugLabel(int room_code) {
    switch (room_code) {
    case 0:
        return "side";
    case 1:
        return "main";
    case 2:
        return "drop";
    case 3:
        return "exit";
    case 4:
        return "shop<";
    case 5:
        return "shop>";
    case 6:
        return "special6";
    case 7:
        return "special7";
    case 8:
        return "special8";
    case 9:
        return "special9";
    case 10:
        return "pit";
    case 11:
        return "pit!";
    default:
        return "?";
    }
}

} // namespace splonks::stage_gen::classic

