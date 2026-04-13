#include "stage_gen/hd_mines.hpp"

#include "frame_data_id.hpp"
#include "utils.hpp"

#include <array>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace splonks::stage_gen::hd_mines {

namespace {

enum class RoomCode : int {
    Side = 0,
    Main = 1,
    Drop = 2,
    Exit = 3,
    ShopLeft = 4,
    ShopRight = 5,
    SnakePitTop = 8,
    SnakePitBottom = 9,
};

using GlyphPatch = std::string_view;
using GlyphTemplate = std::string_view;

int PickIndex(std::size_t size) {
    return rng::RandomIntInclusive(0, static_cast<int>(size) - 1);
}

int GetLevelNumber(StageType stage_type) {
    switch (stage_type) {
    case StageType::Cave1:
        return 1;
    case StageType::Cave2:
        return 2;
    case StageType::Cave3:
        return 3;
    default:
        return 1;
    }
}

std::array<std::array<int, 4>, 4> MakeBlankRoomCodes() {
    std::array<std::array<int, 4>, 4> codes{};
    for (std::array<int, 4>& row : codes) {
        row.fill(0);
    }
    return codes;
}

struct StageLayout {
    IVec2 start_room = IVec2::New(0, 0);
    IVec2 end_room = IVec2::New(0, 0);
    std::array<std::array<int, 4>, 4> room_codes = MakeBlankRoomCodes();
    std::vector<IVec2> path;
};

enum class ShopType {
    None,
    General,
    Bomb,
    Weapon,
    Rare,
    Clothing,
    Craps,
    Kissing,
};

struct RoomTemplateSelection {
    GlyphTemplate glyphs = "";
    ShopType shop_type = ShopType::None;
};

struct ResolvedRoom {
    std::vector<std::vector<Tile>> tiles;
    std::vector<StageEntitySpawn> entity_spawns;
    std::vector<BackgroundStamp> background_stamps;
};

std::vector<IVec2> BuildPathFromRoomCodes(const std::array<std::array<int, 4>, 4>& room_codes,
                                          const IVec2& start_room, const IVec2& end_room) {
    std::vector<IVec2> path;
    IVec2 current = start_room;
    path.push_back(current);

    while (current != end_room) {
        const int code = room_codes[static_cast<std::size_t>(current.y)]
                                  [static_cast<std::size_t>(current.x)];

        if ((code == static_cast<int>(RoomCode::Main) ||
             code == static_cast<int>(RoomCode::ShopLeft)) &&
            current.x < 3) {
            const int right_code = room_codes[static_cast<std::size_t>(current.y)]
                                             [static_cast<std::size_t>(current.x + 1)];
            if (right_code == static_cast<int>(RoomCode::Main) ||
                right_code == static_cast<int>(RoomCode::Drop) ||
                right_code == static_cast<int>(RoomCode::Exit) ||
                right_code == static_cast<int>(RoomCode::ShopRight)) {
                current.x += 1;
                path.push_back(current);
                continue;
            }
        }

        if (current.y < 3 &&
            (code == static_cast<int>(RoomCode::Main) ||
             code == static_cast<int>(RoomCode::Drop) ||
             code == static_cast<int>(RoomCode::ShopLeft) ||
             code == static_cast<int>(RoomCode::ShopRight))) {
            const int below_code = room_codes[static_cast<std::size_t>(current.y + 1)]
                                             [static_cast<std::size_t>(current.x)];
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
            const int left_code = room_codes[static_cast<std::size_t>(current.y)]
                                            [static_cast<std::size_t>(current.x - 1)];
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

StageLayout GenerateLayout(int level_number) {
    StageLayout layout;

    int room_x = 0;
    int room_y = 0;
    int prev_x = 0;
    int prev_y = 0;
    layout.room_codes[0][0] = static_cast<int>(RoomCode::Main);

    while (room_y < 4) {
        bool moved_down = false;
        int move_roll = 5;

        if (room_x == 0) {
            move_roll = rng::RandomIntInclusive(3, 5);
        } else if (room_x == 3) {
            move_roll = rng::RandomIntInclusive(5, 7);
        } else {
            move_roll = rng::RandomIntInclusive(1, 5);
        }

        if (move_roll < 3 || move_roll > 5) {
            if (room_x > 0 && layout.room_codes[static_cast<std::size_t>(room_y)]
                                             [static_cast<std::size_t>(room_x - 1)] == 0) {
                room_x -= 1;
            } else if (room_x < 3 &&
                       layout.room_codes[static_cast<std::size_t>(room_y)]
                                        [static_cast<std::size_t>(room_x + 1)] == 0) {
                room_x += 1;
            } else {
                move_roll = 5;
            }
        } else if (move_roll == 3 || move_roll == 4) {
            if (room_x < 3 && layout.room_codes[static_cast<std::size_t>(room_y)]
                                             [static_cast<std::size_t>(room_x + 1)] == 0) {
                room_x += 1;
            } else if (room_x > 0 &&
                       layout.room_codes[static_cast<std::size_t>(room_y)]
                                        [static_cast<std::size_t>(room_x - 1)] == 0) {
                room_x -= 1;
            } else {
                move_roll = 5;
            }
        }

        if (move_roll == 5) {
            room_y += 1;
            moved_down = true;
            if (room_y < 4) {
                layout.room_codes[static_cast<std::size_t>(prev_y)]
                                 [static_cast<std::size_t>(prev_x)] =
                                     static_cast<int>(RoomCode::Drop);
                layout.room_codes[static_cast<std::size_t>(room_y)]
                                 [static_cast<std::size_t>(room_x)] =
                                     static_cast<int>(RoomCode::Exit);
            } else {
                layout.end_room = IVec2::New(room_x, room_y - 1);
            }
        }

        if (!moved_down) {
            layout.room_codes[static_cast<std::size_t>(room_y)]
                             [static_cast<std::size_t>(room_x)] =
                                 static_cast<int>(RoomCode::Main);
        }

        prev_x = room_x;
        prev_y = room_y;
    }

    if (level_number > 1 && rng::RandomIntInclusive(1, level_number) <= 2) {
        std::array<std::array<int, 4>, 4> room_poss = MakeBlankRoomCodes();
        int candidate_count = 0;

        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                if (layout.room_codes[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] != 0) {
                    continue;
                }

                if (x < 3) {
                    const int right_code =
                        layout.room_codes[static_cast<std::size_t>(y)][static_cast<std::size_t>(x + 1)];
                    if (right_code == static_cast<int>(RoomCode::Main) ||
                        right_code == static_cast<int>(RoomCode::Drop)) {
                        room_poss[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                            static_cast<int>(RoomCode::ShopLeft);
                        candidate_count += 1;
                        continue;
                    }
                }

                if (x > 0) {
                    const int left_code =
                        layout.room_codes[static_cast<std::size_t>(y)][static_cast<std::size_t>(x - 1)];
                    if (left_code == static_cast<int>(RoomCode::Main) ||
                        left_code == static_cast<int>(RoomCode::Drop)) {
                        room_poss[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                            static_cast<int>(RoomCode::ShopRight);
                        candidate_count += 1;
                    }
                }
            }
        }

        if (candidate_count > 0) {
            int target = rng::RandomIntInclusive(0, candidate_count - 1);
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    if (room_poss[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == 0) {
                        continue;
                    }
                    if (target == 0) {
                        layout.room_codes[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                            room_poss[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                        y = 4;
                        break;
                    }
                    target -= 1;
                }
            }
        }
    }

    layout.path = BuildPathFromRoomCodes(layout.room_codes, layout.start_room, layout.end_room);
    return layout;
}

const std::array<GlyphTemplate, 8> kStartTemplates = {
    "60000600000000000000000000000000000000000008000000000000000000000000001111111111",
    "11111111112222222222000000000000000000000008000000000000000000000000001111111111",
    "00000000000008000000000000000000L000000000P111111000L111111000L00111111111111111",
    "0000000000008000000000000000000000000L000111111P000111111L001111100L001111111111",
    "60000600000000000000000000000000000000000008000000000000000000000000002021111120",
    "11111111112222222222000000000000000000000008000000000000000000000000002021111120",
    "00000000000008000000000000000000L000000000P111111000L111111000L00011111111101111",
    "0000000000008000000000000000000000000L000111111P000111111L001111000L001111011111",
};

const std::array<GlyphTemplate, 6> kEndTemplates = {
    "00000000006000060000000000000000000000000008000000000000000000000000001111111111",
    "00000000000000000000000000000000000000000008000000000000000000000000001111111111",
    "00000000000010021110001001111000110111129012000000111111111021111111201111111111",
    "00000000000111200100011110010021111011000000002109011111111102111111121111111111",
    "60000600000000000000000000000000000000000008000000000000000000000000001111111111",
    "11111111112222222222000000000000000000000008000000000000000000000000001111111111",
};

const std::array<GlyphTemplate, 11> kSideTemplates = {
    "00000000000010111100000000000000011010000050000000000000000000000000001111111111",
    "110000000040L600000011P000000011L000000011L5000000110000000011000000001111111111",
    "00000000110060000L040000000P110000000L110050000L11000000001100000000111111111111",
    "110000000040L600000011P000000011L000000011L0000000110000000011000000001112222111",
    "00000000110060000L040000000P110000000L110000000L11000000001100000000111112222111",
    "11111111110221111220002111120000022220000002222000002111120002211112201111111111",
    "11111111111112222111112000021111102201111120000211111022011111200002111112222111",
    "11111111110000000000110000001111222222111111111111112222221122000000221100000011",
    "121111112100L2112L0011P1111P1111L2112L1111L1111L1111L1221L1100L0000L001111221111",
    "22000000220000B0000000000000000000000000000000000000000000000000I000001111A01111",
    "220000002200000000000000000000000000000000000000000000x0000002211112201111111111",
};

const std::array<GlyphTemplate, 12> kMainTemplates = {
    "60000600000000000000000000000000000000000050000000000000000000000000001111111111",
    "60000600000000000000000000000000000000005000050000000000000000000000001111111111",
    "60000600000000000000000000000000050000000000000000000000000011111111111111111111",
    "60000600000000000000000600000000000000000000000000000222220000111111001111111111",
    "11111111112222222222000000000000000000000050000000000000000000000000001111111111",
    "11111111112111111112022222222000000000000050000000000000000000000000001111111111",
    "11111111112111111112211111111221111111120111111110022222222000000000001111111111",
    "1111111111000000000L111111111P000000000L5000050000000000000000000000001111111111",
    "000000000000L0000L0000P1111P0000L0000L0000P1111P0000L1111L0000L1111L001111111111",
    "00000000000111111110001111110000000000005000050000000000000000000000001111111111",
    "00000000000000000000000000000000000000000021111200021111112021111111121111111111",
    "2222222222000000000000000000L001111111P001050000L011000000L010000000L01111111111",
};

const std::array<GlyphTemplate, 8> kExitMainTemplates = {
    "00000000000000000000000000000000000000000050000000000000000000000000001111111111",
    "00000000000000000000000000000000000000005000050000000000000000000000001111111111",
    "00000000000000000000000000000050000500000000000000000000000011111111111111111111",
    "00000000000000000000000600000000000000000000000000000111110000111111001111111111",
    "00000000000111111110001111110000000000005000050000000000000000000000001111111111",
    "00000000000000000000000000000000000000000021111200021111112021111111121111111111",
    "10000000011112002111111200211100000000000022222000111111111111111111111111111111",
    "0000000000000000000000000000L001111111P001050000L011000000L010000000L01111111111",
};

const std::array<GlyphTemplate, 7> kShopLeftTemplates = {
    "111111111111111111111111221111111l000211...000W010...00000k0..Kiiii000bbbbbbbbbb",
    "111111111111111111111111221111111l000211...000W010...00000k0..Kiiii000bbbbbbbbbb",
    "111111111111111111111111221111111l000211...000W010...00000k0..Kiiii000bbbbbbbbbb",
    "111111111111111111111111221111111l000211...000W010...00000k0..Kiiii000bbbbbbbbbb",
    "111111111111111111111111221111111l000211...000W010...00000k0..Kiiii000bbbbbbbbbb",
    "11111111111111111111111122111111Kl000211..bQ00W010.0+00000k0.q+dd00000bbbbbbbbbb",
    "111111111111111111111111221111111l000211...000W010...00000k0..K00D0000bbbbbbbbbb",
};

const std::array<GlyphTemplate, 7> kShopRightTemplates = {
    "111111111111111111111111221111112000l11101W0000...0k00000...000iiiiK..bbbbbbbbbb",
    "111111111111111111111111221111112000l11101W0000...0k00000...000iiiiK..bbbbbbbbbb",
    "111111111111111111111111221111112000l11101W0000...0k00000...000iiiiK..bbbbbbbbbb",
    "111111111111111111111111221111112000l11101W0000...0k00000...000iiiiK..bbbbbbbbbb",
    "111111111111111111111111221111112000l11101W0000...0k00000...000iiiiK..bbbbbbbbbb",
    "111111111111111111111111221111112000lK1101W0Q00b..0k00000+0.00000dd+q.bbbbbbbbbb",
    "111111111111111111111111221111112000l11101W0000...0k00000...0000D00K..bbbbbbbbbb",
};

const std::array<GlyphTemplate, 1> kSnakePitTemplates = {
    "111000011111s0000s11111200211111s0000s11111200211111s0000s11111200211111s0000s11",
};

const std::array<GlyphTemplate, 1> kSnakePitBottomTemplates = {
    "111000011111s0000s1111100001111100S0001111S0110S11111STTS1111111M111111111111111",
};

const std::array<GlyphTemplate, 12> kDropTemplates = {
    "00000000006000060000000000000000000000006000060000000000000000000000000000000000",
    "00000000006000060000000000000000000000000000050000000000000000000000001202111111",
    "00000000006000060000000000000000000000050000000000000000000000000000001111112021",
    "00000000006000060000000000000000000000000000000000000000000002200002201112002111",
    "00000000000000220000000000000000200002000112002110011100111012000000211111001111",
    "00000000000060000000000000000000000000000000000000001112220002100000001110111111",
    "00000000000060000000000000000000000000000000000000002221110000000001201111110111",
    "00000000000060000000000000000000000000000000000000002022020000100001001111001111",
    "11111111112222222222000000000000000000000000000000000000000000000000001120000211",
    "11111111112222111111000002211100000002110000000000200000000000000000211120000211",
    "11111111111111112222111220000011200000000000000000000000000012000000001120000211",
    "11111111112111111112021111112000211112000002112000000022000002200002201111001111",
};

const std::array<GlyphPatch, 8> kDoorwayObstaclePatches = {
    "009002111221112",
    "009000212002120",
    "000000000092222",
    "000000000022229",
    "000001100119001",
    "000001001110091",
    "111111000140094",
    "000001202112921",
};

const std::array<GlyphPatch, 16> kGroundObstaclePatches = {
    "111110000000000",
    "000000111100000",
    "000000111100000",
    "000000000011111",
    "000002020017177",
    "000000202071717",
    "000000020277171",
    "000002220011100",
    "000000222001110",
    "000000022200111",
    "111002220000000",
    "011100222000000",
    "001110022200000",
    "000000222021112",
    "000002010077117",
    "000000010271177",
};

const std::array<GlyphPatch, 10> kAirObstaclePatches = {
    "111110000000000",
    "222220000000000",
    "111002220000000",
    "011100222000000",
    "001110022200000",
    "000000111000000",
    "000000111002220",
    "000000222001110",
    "000000022001111",
    "000002220011100",
};

GlyphPatch PickObstaclePatch(char glyph) {
    switch (glyph) {
    case '8':
        return kDoorwayObstaclePatches[static_cast<std::size_t>(PickIndex(kDoorwayObstaclePatches.size()))];
    case '5':
        return kGroundObstaclePatches[static_cast<std::size_t>(PickIndex(kGroundObstaclePatches.size()))];
    case '6':
        return kAirObstaclePatches[static_cast<std::size_t>(PickIndex(kAirObstaclePatches.size()))];
    default:
        return "";
    }
}

void ApplyPatchAt(std::string& glyphs, int top_left_index, GlyphPatch patch) {
    const int start_x = top_left_index % 10;
    const int start_y = top_left_index / 10;

    for (int patch_y = 0; patch_y < 3; ++patch_y) {
        for (int patch_x = 0; patch_x < 5; ++patch_x) {
            const int x = start_x + patch_x;
            const int y = start_y + patch_y;
            if (x >= 10 || y >= 8) {
                continue;
            }
            glyphs[static_cast<std::size_t>(y * 10 + x)] =
                patch[static_cast<std::size_t>(patch_y * 5 + patch_x)];
        }
    }
}

std::string ExpandObstacles(GlyphTemplate template_glyphs) {
    std::string glyphs(template_glyphs);
    for (int index = 0; index < 80; ++index) {
        const char glyph = glyphs[static_cast<std::size_t>(index)];
        if (glyph != '5' && glyph != '6' && glyph != '8') {
            continue;
        }
        ApplyPatchAt(glyphs, index, PickObstaclePatch(glyph));
    }
    return glyphs;
}

RoomTemplateSelection SelectRoomTemplate(int room_code, bool is_start_room, bool is_end_room,
                                         int room_code_above) {
    if (is_start_room) {
        if (room_code == static_cast<int>(RoomCode::Drop)) {
            return RoomTemplateSelection{
                .glyphs = kStartTemplates[static_cast<std::size_t>(rng::RandomIntInclusive(4, 7))],
            };
        }
        return RoomTemplateSelection{
            .glyphs = kStartTemplates[static_cast<std::size_t>(rng::RandomIntInclusive(0, 3))],
        };
    }

    if (is_end_room) {
        if (room_code_above == static_cast<int>(RoomCode::Drop)) {
            return RoomTemplateSelection{
                .glyphs = kEndTemplates[static_cast<std::size_t>(rng::RandomIntInclusive(1, 3))],
            };
        }
        return RoomTemplateSelection{
            .glyphs = kEndTemplates[static_cast<std::size_t>(rng::RandomIntInclusive(2, 5))],
        };
    }

    switch (room_code) {
    case static_cast<int>(RoomCode::Side):
        return RoomTemplateSelection{
            .glyphs = kSideTemplates[static_cast<std::size_t>(PickIndex(kSideTemplates.size()))],
        };
    case static_cast<int>(RoomCode::Main):
        return RoomTemplateSelection{
            .glyphs = kMainTemplates[static_cast<std::size_t>(PickIndex(kMainTemplates.size()))],
        };
    case static_cast<int>(RoomCode::Exit):
        return RoomTemplateSelection{
            .glyphs = kExitMainTemplates[static_cast<std::size_t>(PickIndex(kExitMainTemplates.size()))],
        };
    case static_cast<int>(RoomCode::ShopLeft):
    {
        const int shop_index = PickIndex(kShopLeftTemplates.size());
        const ShopType shop_type = shop_index == 0 ? ShopType::General :
                                   shop_index == 1 ? ShopType::Bomb :
                                   shop_index == 2 ? ShopType::Weapon :
                                   shop_index == 3 ? ShopType::Rare :
                                   shop_index == 4 ? ShopType::Clothing :
                                   shop_index == 5 ? ShopType::Craps :
                                                     ShopType::Kissing;
        return RoomTemplateSelection{
            .glyphs = kShopLeftTemplates[static_cast<std::size_t>(shop_index)],
            .shop_type = shop_type,
        };
    }
    case static_cast<int>(RoomCode::ShopRight):
    {
        const int shop_index = PickIndex(kShopRightTemplates.size());
        const ShopType shop_type = shop_index == 0 ? ShopType::General :
                                   shop_index == 1 ? ShopType::Bomb :
                                   shop_index == 2 ? ShopType::Weapon :
                                   shop_index == 3 ? ShopType::Rare :
                                   shop_index == 4 ? ShopType::Clothing :
                                   shop_index == 5 ? ShopType::Craps :
                                                     ShopType::Kissing;
        return RoomTemplateSelection{
            .glyphs = kShopRightTemplates[static_cast<std::size_t>(shop_index)],
            .shop_type = shop_type,
        };
    }
    case static_cast<int>(RoomCode::SnakePitTop):
        return RoomTemplateSelection{
            .glyphs = kSnakePitTemplates[0],
        };
    case static_cast<int>(RoomCode::SnakePitBottom):
        return RoomTemplateSelection{
            .glyphs = kSnakePitBottomTemplates[0],
        };
    default:
        return RoomTemplateSelection{
            .glyphs = kDropTemplates[static_cast<std::size_t>(PickIndex(kDropTemplates.size()))],
        };
    }
}

Tile RandomBrickOrBlockTile(Tile family_tile) {
    return rng::RandomIntInclusive(1, 10) == 1 ? BlockTileForFamilyTile(family_tile)
                                              : DirtTileForFamilyTile(family_tile);
}

EntityType GetShopSignEntityType(ShopType shop_type) {
    switch (shop_type) {
    case ShopType::General:
        return EntityType::SignGeneral;
    case ShopType::Bomb:
        return EntityType::SignBomb;
    case ShopType::Weapon:
        return EntityType::SignWeapon;
    case ShopType::Rare:
        return EntityType::SignRare;
    case ShopType::Clothing:
        return EntityType::SignClothing;
    case ShopType::Craps:
        return EntityType::SignCraps;
    case ShopType::Kissing:
        return EntityType::SignKissing;
    case ShopType::None:
        return EntityType::None;
    }

    return EntityType::None;
}

const char* PickRightTikiArmFrameName() {
    switch (rng::RandomIntInclusive(0, 2)) {
    case 0:
        return "tiki_arm_right_0";
    case 1:
        return "tiki_arm_right_1";
    default:
        return "tiki_arm_right_2";
    }
}

const char* PickLeftTikiArmFrameName() {
    switch (rng::RandomIntInclusive(0, 2)) {
    case 0:
        return "tiki_arm_left_0";
    case 1:
        return "tiki_arm_left_1";
    default:
        return "tiki_arm_left_2";
    }
}

bool TileCoordExists(const Stage& stage, int tile_x, int tile_y) {
    return tile_x >= 0 && tile_y >= 0 &&
           tile_x < static_cast<int>(stage.GetTileWidth()) &&
           tile_y < static_cast<int>(stage.GetTileHeight());
}

bool IsCollidableTileAt(const Stage& stage, int tile_x, int tile_y) {
    if (!TileCoordExists(stage, tile_x, tile_y)) {
        return false;
    }
    return IsTileCollidable(stage.GetTile(static_cast<unsigned int>(tile_x),
                                          static_cast<unsigned int>(tile_y)));
}

IVec2 GetRoomAtTileCoord(int tile_x, int tile_y) {
    return IVec2::New(
        tile_x / static_cast<int>(Stage::kRoomShape.x),
        tile_y / static_cast<int>(Stage::kRoomShape.y)
    );
}

bool IsShopRoomCode(int room_code) {
    return room_code == static_cast<int>(RoomCode::ShopLeft) ||
           room_code == static_cast<int>(RoomCode::ShopRight);
}

bool IsShopRoomAt(const Stage& stage, int tile_x, int tile_y) {
    const IVec2 room = GetRoomAtTileCoord(tile_x, tile_y);
    if (room.x < 0 || room.y < 0 ||
        room.x >= static_cast<int>(Stage::kRoomLayout.x) ||
        room.y >= static_cast<int>(Stage::kRoomLayout.y)) {
        return false;
    }
    return IsShopRoomCode(
        stage.rooms[static_cast<std::size_t>(room.y)][static_cast<std::size_t>(room.x)]
    );
}

bool IsStartRoomAt(const Stage& stage, int tile_x, int tile_y) {
    return GetRoomAtTileCoord(tile_x, tile_y) == stage.GetStartingRoom();
}

bool HasSpawnAtWorldPos(const Stage& stage, const Vec2& pos) {
    for (const StageEntitySpawn& spawn : stage.entity_spawns) {
        if (spawn.pos == pos) {
            return true;
        }
    }
    return false;
}

std::optional<Vec2> FindEntrancePos(const Stage& stage) {
    for (unsigned int y = 0; y < stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < stage.GetTileWidth(); ++x) {
            if (stage.GetTile(x, y) != Tile::Entrance) {
                continue;
            }
            return Vec2::New(
                static_cast<float>(x * kTileSize),
                static_cast<float>(y * kTileSize)
            );
        }
    }
    return std::nullopt;
}

std::optional<Vec2> FindExitPos(const Stage& stage) {
    for (unsigned int y = 0; y < stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < stage.GetTileWidth(); ++x) {
            if (stage.GetTile(x, y) != Tile::Exit) {
                continue;
            }
            return Vec2::New(
                static_cast<float>(x * kTileSize),
                static_cast<float>(y * kTileSize)
            );
        }
    }
    return std::nullopt;
}

bool HasSpawnType(const Stage& stage, EntityType type_) {
    for (const StageEntitySpawn& spawn : stage.entity_spawns) {
        if (spawn.type_ == type_) {
            return true;
        }
    }
    return false;
}

float DistanceToNearestSpawnType(const Stage& stage, EntityType type_, const Vec2& pos) {
    float nearest = std::numeric_limits<float>::infinity();
    for (const StageEntitySpawn& spawn : stage.entity_spawns) {
        if (spawn.type_ != type_) {
            continue;
        }
        nearest = std::min(nearest, Length(spawn.pos - pos));
    }
    return nearest;
}

bool HasSpawnType(const std::vector<StageEntitySpawn>& spawns, EntityType type_) {
    for (const StageEntitySpawn& spawn : spawns) {
        if (spawn.type_ == type_) {
            return true;
        }
    }
    return false;
}

bool HasSpawnType(const Stage& stage, const std::vector<StageEntitySpawn>& spawns, EntityType type_) {
    return HasSpawnType(stage, type_) || HasSpawnType(spawns, type_);
}

void AddAmbientSpawn(Stage& stage, EntityType type_, const Vec2& pos,
                     LeftOrRight facing = LeftOrRight::Left) {
    if (HasSpawnAtWorldPos(stage, pos)) {
        return;
    }
    stage.entity_spawns.push_back(StageEntitySpawn{
        .type_ = type_,
        .pos = pos,
        .facing = facing,
    });
}

EntityType PickUndergroundItemType() {
    switch (rng::RandomIntInclusive(0, 18)) {
    case 0:
        return EntityType::JetPack;
    case 1:
        return EntityType::Cape;
    case 2:
        return EntityType::Shotgun;
    case 3:
        return EntityType::Mattock;
    case 4:
        return EntityType::Teleporter;
    case 5:
        return EntityType::Gloves;
    case 6:
        return EntityType::Spectacles;
    case 7:
        return EntityType::WebCannon;
    case 8:
        return EntityType::Pistol;
    case 9:
        return EntityType::Mitt;
    case 10:
        return EntityType::Paste;
    case 11:
        return EntityType::SpringShoes;
    case 12:
        return EntityType::SpikeShoes;
    case 13:
        return EntityType::Machete;
    case 14:
        return EntityType::BombBox;
    case 15:
        return EntityType::Bow;
    case 16:
        return EntityType::Compass;
    case 17:
        return EntityType::Parachute;
    default:
        return EntityType::RopePile;
    }
}

EntityType PickHighEndShopItemType() {
    if (rng::RandomIntInclusive(1, 40) == 1) {
        return EntityType::JetPack;
    }
    if (rng::RandomIntInclusive(1, 25) == 1) {
        return EntityType::Cape;
    }
    if (rng::RandomIntInclusive(1, 20) == 1) {
        return EntityType::Shotgun;
    }
    if (rng::RandomIntInclusive(1, 10) == 1) {
        return EntityType::Gloves;
    }
    if (rng::RandomIntInclusive(1, 10) == 1) {
        return EntityType::Teleporter;
    }
    if (rng::RandomIntInclusive(1, 8) == 1) {
        return EntityType::Mattock;
    }
    if (rng::RandomIntInclusive(1, 8) == 1) {
        return EntityType::Paste;
    }
    if (rng::RandomIntInclusive(1, 8) == 1) {
        return EntityType::SpringShoes;
    }
    if (rng::RandomIntInclusive(1, 8) == 1) {
        return EntityType::SpikeShoes;
    }
    if (rng::RandomIntInclusive(1, 8) == 1) {
        return EntityType::Compass;
    }
    if (rng::RandomIntInclusive(1, 8) == 1) {
        return EntityType::Pistol;
    }
    if (rng::RandomIntInclusive(1, 8) == 1) {
        return EntityType::Machete;
    }
    return EntityType::BombBox;
}

EntityType PickShopItemType(
    ShopType shop_type,
    const Stage& stage,
    const std::vector<StageEntitySpawn>& room_spawns
) {
    if (shop_type == ShopType::Bomb) {
        while (true) {
            if (rng::RandomIntInclusive(1, 5) == 1) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Paste)) {
                    return EntityType::Paste;
                }
            } else if (rng::RandomIntInclusive(1, 4) == 1) {
                return EntityType::BombBox;
            } else {
                return EntityType::BombBag;
            }
        }
    }

    if (shop_type == ShopType::Weapon) {
        int attempts_remaining = 20;
        while (true) {
            const int n = rng::RandomIntInclusive(1, 4);
            if (attempts_remaining <= 0) {
                return EntityType::BombBag;
            }
            if (rng::RandomIntInclusive(1, 12) == 1) {
                if (!HasSpawnType(stage, room_spawns, EntityType::WebCannon)) {
                    return EntityType::WebCannon;
                }
            } else if (rng::RandomIntInclusive(1, 10) == 1) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Shotgun)) {
                    return EntityType::Shotgun;
                }
            } else if (rng::RandomIntInclusive(1, 6) == 1) {
                return EntityType::BombBox;
            } else if (n == 1) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Pistol)) {
                    return EntityType::Pistol;
                }
            } else if (n == 2) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Machete)) {
                    return EntityType::Machete;
                }
            } else if (n == 3) {
                return EntityType::BombBag;
            } else if (n == 4) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Bow)) {
                    return EntityType::Bow;
                }
            }
            attempts_remaining -= 1;
        }
    }

    if (shop_type == ShopType::Clothing) {
        int attempts_remaining = 20;
        while (true) {
            const int n = rng::RandomIntInclusive(1, 6);
            if (rng::RandomIntInclusive(1, attempts_remaining) == 1) {
                return EntityType::RopePile;
            }
            if (n == 1) {
                if (!HasSpawnType(stage, room_spawns, EntityType::SpringShoes)) {
                    return EntityType::SpringShoes;
                }
            } else if (n == 2) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Spectacles)) {
                    return EntityType::Spectacles;
                }
            } else if (n == 3) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Gloves)) {
                    return EntityType::Gloves;
                }
            } else if (n == 4) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Mitt)) {
                    return EntityType::Mitt;
                }
            } else if (n == 5) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Cape)) {
                    return EntityType::Cape;
                }
            } else if (n == 6) {
                if (!HasSpawnType(stage, room_spawns, EntityType::SpikeShoes)) {
                    return EntityType::SpikeShoes;
                }
            }
            attempts_remaining -= 1;
        }
    }

    if (shop_type == ShopType::Rare) {
        int attempts_remaining = 20;
        while (true) {
            const int n = rng::RandomIntInclusive(1, 11);
            if (rng::RandomIntInclusive(1, attempts_remaining) == 1) {
                return EntityType::BombBox;
            }
            if (n == 1) {
                if (!HasSpawnType(stage, room_spawns, EntityType::SpringShoes)) {
                    return EntityType::SpringShoes;
                }
            } else if (n == 2) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Compass)) {
                    return EntityType::Compass;
                }
            } else if (n == 3) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Mattock)) {
                    return EntityType::Mattock;
                }
            } else if (n == 4) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Spectacles)) {
                    return EntityType::Spectacles;
                }
            } else if (n == 5) {
                if (!HasSpawnType(stage, room_spawns, EntityType::JetPack)) {
                    return EntityType::JetPack;
                }
            } else if (n == 6) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Gloves)) {
                    return EntityType::Gloves;
                }
            } else if (n == 7) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Mitt)) {
                    return EntityType::Mitt;
                }
            } else if (n == 8) {
                if (!HasSpawnType(stage, room_spawns, EntityType::WebCannon)) {
                    return EntityType::WebCannon;
                }
            } else if (n == 9) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Cape)) {
                    return EntityType::Cape;
                }
            } else if (n == 10) {
                if (!HasSpawnType(stage, room_spawns, EntityType::Teleporter)) {
                    return EntityType::Teleporter;
                }
            } else if (n == 11) {
                if (!HasSpawnType(stage, room_spawns, EntityType::SpikeShoes)) {
                    return EntityType::SpikeShoes;
                }
            }
            attempts_remaining -= 1;
        }
    }

    while (true) {
        const int n = rng::RandomIntInclusive(1, 3);
        if (rng::RandomIntInclusive(1, 20) == 1) {
            if (!HasSpawnType(stage, room_spawns, EntityType::Mattock)) {
                return EntityType::Mattock;
            }
        } else if (rng::RandomIntInclusive(1, 10) == 1) {
            if (!HasSpawnType(stage, room_spawns, EntityType::Gloves)) {
                return EntityType::Gloves;
            }
        } else if (rng::RandomIntInclusive(1, 10) == 1) {
            if (!HasSpawnType(stage, room_spawns, EntityType::Compass)) {
                return EntityType::Compass;
            }
        } else if (n == 1) {
            return EntityType::BombBag;
        } else if (n == 2) {
            return EntityType::RopePile;
        } else {
            return EntityType::Parachute;
        }
    }
}

void AddMinesEmbeddedTreasure(Stage& stage) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            const IVec2 tile_pos = IVec2::New(tile_x, tile_y);
            if (stage.GetTile(static_cast<unsigned int>(tile_x), static_cast<unsigned int>(tile_y)) !=
                DirtTileForFamilyTile(stage.stage_border_tile)) {
                continue;
            }

            const int visible_gold_roll = rng::RandomIntInclusive(1, 100);
            if (visible_gold_roll < 20) {
                stage.SetTile(tile_pos, GoldTileForFamilyTile(stage.stage_border_tile));
                continue;
            }
            if (visible_gold_roll < 30) {
                stage.SetTile(tile_pos, GoldBigTileForFamilyTile(stage.stage_border_tile));
                continue;
            }

            const bool interior_tile =
                tile_x > 0 && tile_x < stage_width - 1 &&
                tile_y > 0 && tile_y < stage_height - 1;
            if (!interior_tile) {
                continue;
            }

            if (rng::RandomIntInclusive(1, 100) == 1) {
                stage.SetEmbeddedTreasure(tile_pos, EntityType::SapphireBig);
            } else if (rng::RandomIntInclusive(1, 120) == 1) {
                stage.SetEmbeddedTreasure(tile_pos, EntityType::EmeraldBig);
            } else if (rng::RandomIntInclusive(1, 140) == 1) {
                stage.SetEmbeddedTreasure(tile_pos, EntityType::RubyBig);
            } else if (rng::RandomIntInclusive(1, 1200) == 1) {
                stage.SetEmbeddedTreasure(tile_pos, PickUndergroundItemType());
            }
        }
    }
}

void AddMinesTreasure(Stage& stage) {
    const std::optional<Vec2> entrance_pos = FindEntrancePos(stage);
    const std::optional<Vec2> exit_pos = FindExitPos(stage);
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            if (!IsCollidableTileAt(stage, tile_x, tile_y) ||
                IsShopRoomAt(stage, tile_x, tile_y)) {
                continue;
            }

            const Vec2 tile_pos = Vec2::New(
                static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                static_cast<float>(tile_y * static_cast<int>(kTileSize))
            );

            if (entrance_pos.has_value() && Length(tile_pos - *entrance_pos) < 32.0F) {
                continue;
            }
            if (exit_pos.has_value() && Length(tile_pos - *exit_pos) < 32.0F) {
                continue;
            }
            if (DistanceToNearestSpawnType(stage, EntityType::GoldIdol, tile_pos) < 64.0F) {
                continue;
            }

            if (tile_y <= 0 || IsCollidableTileAt(stage, tile_x, tile_y - 1)) {
                continue;
            }

            const Tile tile_above = stage.GetTile(
                static_cast<unsigned int>(tile_x),
                static_cast<unsigned int>(tile_y - 1)
            );
            if (tile_above == Tile::Spikes || tile_above == Tile::Entrance ||
                tile_above == Tile::Exit) {
                continue;
            }

            const Vec2 item_pos = tile_pos + Vec2::New(8.0F, -4.0F);
            const Vec2 stack_pos = tile_pos + Vec2::New(8.0F, -8.0F);
            if (HasSpawnAtWorldPos(stage, item_pos) || HasSpawnAtWorldPos(stage, stack_pos)) {
                continue;
            }

            if (rng::RandomIntInclusive(1, 100) == 1) {
                AddAmbientSpawn(stage, EntityType::Rock, item_pos);
                continue;
            }

            const bool ceiling_above = tile_y >= 2 && IsCollidableTileAt(stage, tile_x, tile_y - 2);
            const bool side_support = IsCollidableTileAt(stage, tile_x - 1, tile_y - 1) ||
                                      IsCollidableTileAt(stage, tile_x + 1, tile_y - 1);
            const bool tunnel_support = IsCollidableTileAt(stage, tile_x - 1, tile_y - 1) &&
                                        IsCollidableTileAt(stage, tile_x + 1, tile_y - 1);

            if (ceiling_above && side_support) {
                if (rng::RandomIntInclusive(1, 15) == 1) {
                    AddAmbientSpawn(stage, EntityType::Chest, stack_pos);
                } else if (!HasSpawnType(stage, EntityType::Damsel) &&
                           rng::RandomIntInclusive(1, 8) == 1) {
                    AddAmbientSpawn(stage, EntityType::Damsel, stack_pos);
                } else if (rng::RandomIntInclusive(1, 3) == 1) {
                    AddAmbientSpawn(stage, EntityType::Gold, item_pos);
                } else if (rng::RandomIntInclusive(1, 6) == 1) {
                    AddAmbientSpawn(stage, EntityType::GoldStack, stack_pos);
                } else if (rng::RandomIntInclusive(1, 6) == 1) {
                    AddAmbientSpawn(stage, EntityType::EmeraldBig, item_pos);
                } else if (rng::RandomIntInclusive(1, 8) == 1) {
                    AddAmbientSpawn(stage, EntityType::SapphireBig, item_pos);
                } else if (rng::RandomIntInclusive(1, 10) == 1) {
                    AddAmbientSpawn(stage, EntityType::RubyBig, item_pos);
                }
                continue;
            }

            if (tunnel_support) {
                if (rng::RandomIntInclusive(1, 4) == 1) {
                    AddAmbientSpawn(stage, EntityType::Gold, item_pos);
                } else if (rng::RandomIntInclusive(1, 8) == 1) {
                    AddAmbientSpawn(stage, EntityType::GoldStack, stack_pos);
                } else if (rng::RandomIntInclusive(1, 8) == 1) {
                    AddAmbientSpawn(stage, EntityType::EmeraldBig, item_pos);
                } else if (rng::RandomIntInclusive(1, 9) == 1) {
                    AddAmbientSpawn(stage, EntityType::SapphireBig, item_pos);
                } else if (rng::RandomIntInclusive(1, 10) == 1) {
                    AddAmbientSpawn(stage, EntityType::RubyBig, item_pos);
                }
                continue;
            }

            if (rng::RandomIntInclusive(1, 40) == 1) {
                AddAmbientSpawn(stage, EntityType::Gold, item_pos);
            } else if (rng::RandomIntInclusive(1, 50) == 1) {
                AddAmbientSpawn(stage, EntityType::GoldStack, stack_pos);
            }
        }
    }
}

void ConvertBlocksToArrowTraps(Stage& stage) {
    const std::optional<Vec2> entrance_pos = FindEntrancePos(stage);

    for (StageEntitySpawn& spawn : stage.entity_spawns) {
        if (spawn.type_ != EntityType::Block) {
            continue;
        }

        const int tile_x = static_cast<int>(spawn.pos.x) / static_cast<int>(kTileSize);
        const int tile_y = static_cast<int>(spawn.pos.y) / static_cast<int>(kTileSize);

        if (IsShopRoomAt(stage, tile_x, tile_y)) {
            continue;
        }
        if (rng::RandomIntInclusive(1, 4) != 1) {
            continue;
        }

        if (entrance_pos.has_value()) {
            const float dist = Length(spawn.pos - *entrance_pos);
            if (dist <= 48.0F) {
                continue;
            }
            if (static_cast<int>(spawn.pos.y) == static_cast<int>(entrance_pos->y) && dist < 144.0F) {
                continue;
            }
        }

        const bool solid_right = IsCollidableTileAt(stage, tile_x + 1, tile_y);
        const bool left_open = !IsCollidableTileAt(stage, tile_x - 1, tile_y) &&
                               !IsCollidableTileAt(stage, tile_x - 2, tile_y);
        if (solid_right && left_open) {
            spawn.type_ = EntityType::ArrowTrap;
            spawn.facing = LeftOrRight::Left;
            continue;
        }

        const bool solid_left = IsCollidableTileAt(stage, tile_x - 1, tile_y);
        const bool right_open = !IsCollidableTileAt(stage, tile_x + 1, tile_y) &&
                                !IsCollidableTileAt(stage, tile_x + 2, tile_y);
        if (solid_left && right_open) {
            spawn.type_ = EntityType::ArrowTrap;
            spawn.facing = LeftOrRight::Right;
        }
    }
}

void AddAmbientMinesEntities(Stage& stage) {
    const int stage_width = static_cast<int>(stage.GetTileWidth());
    const int stage_height = static_cast<int>(stage.GetTileHeight());
    const bool gen_giant_spider = rng::RandomIntInclusive(1, 6) == 1;
    bool giant_spider_spawned = false;

    for (int tile_y = 0; tile_y < stage_height; ++tile_y) {
        for (int tile_x = 0; tile_x < stage_width; ++tile_x) {
            if (!IsCollidableTileAt(stage, tile_x, tile_y)) {
                continue;
            }
            if (IsShopRoomAt(stage, tile_x, tile_y)) {
                continue;
            }

            const Vec2 tile_pos = Vec2::New(
                static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                static_cast<float>(tile_y * static_cast<int>(kTileSize))
            );

            if (!IsStartRoomAt(stage, tile_x, tile_y)) {
                const bool open_below = !IsCollidableTileAt(stage, tile_x, tile_y + 1) &&
                                        !IsCollidableTileAt(stage, tile_x, tile_y + 2);
                if (tile_y < stage_height - 4 && open_below) {
                    const Vec2 ceiling_spawn_pos =
                        tile_pos + Vec2::New(0.0F, static_cast<float>(kTileSize));
                    const Vec2 ceiling_spawn_pos_2 =
                        tile_pos + Vec2::New(static_cast<float>(kTileSize),
                                             static_cast<float>(kTileSize));
                    const bool open_below_right =
                        !IsCollidableTileAt(stage, tile_x + 1, tile_y + 1) &&
                        !IsCollidableTileAt(stage, tile_x + 1, tile_y + 2);

                    if (gen_giant_spider && !giant_spider_spawned && open_below_right &&
                        !HasSpawnAtWorldPos(stage, ceiling_spawn_pos) &&
                        !HasSpawnAtWorldPos(stage, ceiling_spawn_pos_2) &&
                        rng::RandomIntInclusive(1, 40) == 1) {
                        AddAmbientSpawn(stage, EntityType::GiantSpiderHang, ceiling_spawn_pos);
                        giant_spider_spawned = true;
                    } else if (rng::RandomIntInclusive(1, 60) == 1) {
                        AddAmbientSpawn(stage, EntityType::Bat, ceiling_spawn_pos);
                    } else if (rng::RandomIntInclusive(1, 80) == 1) {
                        AddAmbientSpawn(stage, EntityType::SpiderHang, ceiling_spawn_pos);
                    }
                }

                const bool open_above = !IsCollidableTileAt(stage, tile_x, tile_y - 1);
                const bool spikes_above = tile_y > 0 &&
                                          stage.GetTile(
                                              static_cast<unsigned int>(tile_x),
                                              static_cast<unsigned int>(tile_y - 1)
                                          ) == Tile::Spikes;
                if (tile_y > 0 && open_above) {
                    const Vec2 floor_spawn_pos =
                        tile_pos - Vec2::New(0.0F, static_cast<float>(kTileSize));
                    if (!spikes_above && rng::RandomIntInclusive(1, 60) == 1) {
                        AddAmbientSpawn(stage, EntityType::Snake, floor_spawn_pos);
                    } else if (rng::RandomIntInclusive(1, 800) == 1) {
                        AddAmbientSpawn(stage, EntityType::Caveman, floor_spawn_pos);
                    }
                }
            }
        }
    }
}

ResolvedRoom ResolveRoom(
    int room_code,
    bool is_start_room,
    bool is_end_room,
    int room_code_above,
    const Stage& existing_stage
) {
    const RoomTemplateSelection selection =
        SelectRoomTemplate(room_code, is_start_room, is_end_room, room_code_above);
    const std::string glyphs = ExpandObstacles(selection.glyphs);

    // Glyph markers are authored on the room's 10x8 tile grid. We anchor imported
    // entities and background stamps from that tile grid directly into our top-left
    // world-space positions, then only add explicit whole-tile layout offsets that
    // come from the room design itself, like the right altar half living one tile to
    // the right. We do not port HD sprite-origin offsets directly here.
    ResolvedRoom room;
    room.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(Stage::kRoomShape.y),
        std::vector<Tile>(static_cast<std::size_t>(Stage::kRoomShape.x), Tile::Air)
    );

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x) {
            const char glyph = glyphs[static_cast<std::size_t>(y * 10 + x)];
            Tile tile = Tile::Air;
            const Vec2 tile_pos = Vec2::New(
                static_cast<float>(x * static_cast<int>(kTileSize)),
                static_cast<float>(y * static_cast<int>(kTileSize))
            );

            switch (glyph) {
            case '1':
                tile = RandomBrickOrBlockTile(existing_stage.stage_border_tile);
                break;
            case '2':
                tile = rng::RandomIntInclusive(1, 2) == 1
                           ? RandomBrickOrBlockTile(existing_stage.stage_border_tile)
                           : Tile::Air;
                break;
            case '4':
                if (rng::RandomIntInclusive(1, 4) == 1) {
                    room.entity_spawns.push_back(StageEntitySpawn{
                        .type_ = EntityType::Block,
                        .pos = tile_pos,
                    });
                }
                break;
            case '7':
                tile = rng::RandomIntInclusive(1, 3) == 1 ? Tile::Spikes : Tile::Air;
                break;
            case '9':
                tile = is_start_room ? Tile::Entrance : (is_end_room ? Tile::Exit : Tile::Air);
                break;
            case 'L':
                tile = Tile::Ladder;
                break;
            case 'P':
                tile = Tile::LadderTop;
                break;
            case '.':
                tile = ShopWallTileForFamilyTile(existing_stage.stage_border_tile);
                break;
            case 'b':
                tile = SmoothWallTileForFamilyTile(existing_stage.stage_border_tile);
                break;
            case '+':
                tile = GlassTileForFamilyTile(existing_stage.stage_border_tile);
                break;
            case 'A':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::Altar,
                    .pos = tile_pos,
                    .animation_id = frame_data_ids::AltarLeft,
                });
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::Altar,
                    .pos = tile_pos + Vec2::New(static_cast<float>(kTileSize), 0.0F),
                    .animation_id = frame_data_ids::AltarRight,
                });
                break;
            case 'x':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::SacAltar,
                    .pos = tile_pos,
                    .animation_id = frame_data_ids::SacAltarLeft,
                });
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::SacAltar,
                    .pos = tile_pos + Vec2::New(static_cast<float>(kTileSize), 0.0F),
                    .animation_id = frame_data_ids::SacAltarRight,
                });
                room.background_stamps.push_back(BackgroundStamp{
                    .animation_id = frame_data_ids::KaliBody,
                    .pos = tile_pos + Vec2::New(
                        -static_cast<float>(kTileSize),
                        -static_cast<float>(kTileSize * 3)
                    ),
                });
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::KaliHead,
                    .pos = tile_pos + Vec2::New(
                        static_cast<float>(kTileSize),
                        -static_cast<float>(kTileSize * 4)
                    ),
                });
                break;
            case 'a':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::Chest,
                    .pos = tile_pos,
                });
                break;
            case 'I':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::GoldIdol,
                    .pos = tile_pos,
                });
                break;
            case 'B':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::GiantTikiHead,
                    .pos = tile_pos,
                });
                room.background_stamps.push_back(BackgroundStamp{
                    .animation_id = HashFrameDataIdConstexpr("tiki_body"),
                    .pos = tile_pos + Vec2::New(0.0F, static_cast<float>(kTileSize * 2)),
                });
                room.background_stamps.push_back(BackgroundStamp{
                    .animation_id = HashFrameDataIdConstexpr(PickRightTikiArmFrameName()),
                    .pos = tile_pos + Vec2::New(
                        static_cast<float>(kTileSize * 2),
                        static_cast<float>(kTileSize * 2)
                    ),
                });
                room.background_stamps.push_back(BackgroundStamp{
                    .animation_id = HashFrameDataIdConstexpr(PickLeftTikiArmFrameName()),
                    .pos = tile_pos + Vec2::New(
                        -static_cast<float>(kTileSize),
                        static_cast<float>(kTileSize * 2)
                    ),
                });
                break;
            case 'Q':
                if (selection.shop_type == ShopType::Craps) {
                    room.background_stamps.push_back(BackgroundStamp{
                        .animation_id = HashFrameDataIdConstexpr("dice_sign"),
                        .pos = tile_pos,
                    });
                }
                break;
            case 'q':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = PickHighEndShopItemType(),
                    .pos = tile_pos,
                });
                break;
            case 'W':
                room.background_stamps.push_back(BackgroundStamp{
                    .animation_id = HashFrameDataIdConstexpr("wanted_poster"),
                    .pos = tile_pos,
                    .condition = BackgroundStampCondition::Wanted,
                });
                break;
            case 'l':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = selection.shop_type == ShopType::Kissing
                                 ? EntityType::LanternRed
                                 : EntityType::Lantern,
                    .pos = tile_pos,
                });
                break;
            case 'K':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::Shopkeeper,
                    .pos = tile_pos,
                });
                break;
            case 'k':
            {
                const EntityType sign_type = GetShopSignEntityType(selection.shop_type);
                if (sign_type != EntityType::None) {
                    room.entity_spawns.push_back(StageEntitySpawn{
                        .type_ = sign_type,
                        .pos = tile_pos,
                    });
                }
                break;
            }
            case 'i':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = PickShopItemType(selection.shop_type, existing_stage, room.entity_spawns),
                    .pos = tile_pos,
                });
                break;
            case 'd':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::Dice,
                    .pos = tile_pos,
                });
                break;
            case 'D':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::Damsel,
                    .pos = tile_pos,
                });
                break;
            case 's':
                if (rng::RandomIntInclusive(1, 10) != 1 && rng::RandomIntInclusive(1, 2) == 1) {
                    tile = DirtTileForFamilyTile(existing_stage.stage_border_tile);
                }
                break;
            case 'S':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::Snake,
                    .pos = tile_pos,
                });
                break;
            case 'T':
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::RubyBig,
                    .pos = tile_pos,
                });
                break;
            case 'M':
                tile = DirtTileForFamilyTile(existing_stage.stage_border_tile);
                room.entity_spawns.push_back(StageEntitySpawn{
                    .type_ = EntityType::Mattock,
                    .pos = tile_pos,
                });
                break;
            default:
                break;
            }

            room.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile;
        }
    }

    return room;
}

} // namespace

bool UsesHdMinesGenerator(StageType stage_type) {
    return stage_type == StageType::Cave1 || stage_type == StageType::Cave2 ||
           stage_type == StageType::Cave3;
}

Stage GenerateStage(StageType stage_type) {
    const StageLayout layout = GenerateLayout(GetLevelNumber(stage_type));
    Stage stage;

    std::vector<std::vector<Tile>> tiles(
        static_cast<std::size_t>(Stage::kShape.y),
        std::vector<Tile>(static_cast<std::size_t>(Stage::kShape.x), Tile::Air));
    std::vector<std::vector<int>> room_codes(
        static_cast<std::size_t>(Stage::kRoomLayout.y),
        std::vector<int>(static_cast<std::size_t>(Stage::kRoomLayout.x), 0));

    for (unsigned int room_y = 0; room_y < Stage::kRoomLayout.y; ++room_y) {
        for (unsigned int room_x = 0; room_x < Stage::kRoomLayout.x; ++room_x) {
            const int room_code = layout.room_codes[room_y][room_x];
            room_codes[static_cast<std::size_t>(room_y)][static_cast<std::size_t>(room_x)] =
                room_code;

            const bool is_start_room =
                room_x == static_cast<unsigned int>(layout.start_room.x) &&
                room_y == static_cast<unsigned int>(layout.start_room.y);
            const bool is_end_room =
                room_x == static_cast<unsigned int>(layout.end_room.x) &&
                room_y == static_cast<unsigned int>(layout.end_room.y);
            const int room_code_above =
                room_y == 0 ? -1 : layout.room_codes[room_y - 1][room_x];

            ResolvedRoom room = ResolveRoom(
                room_code,
                is_start_room,
                is_end_room,
                room_code_above,
                stage
            );

            const UVec2 room_pos = UVec2::New(room_x, room_y) * Stage::kRoomShape;
            const Vec2 room_pos_wc = Vec2::New(
                static_cast<float>(room_pos.x * kTileSize),
                static_cast<float>(room_pos.y * kTileSize)
            );
            for (unsigned int tile_y = 0; tile_y < Stage::kRoomShape.y; ++tile_y) {
                for (unsigned int tile_x = 0; tile_x < Stage::kRoomShape.x; ++tile_x) {
                    const UVec2 tile_pos = room_pos + UVec2::New(tile_x, tile_y);
                    tiles[static_cast<std::size_t>(tile_pos.y)]
                         [static_cast<std::size_t>(tile_pos.x)] =
                             room.tiles[static_cast<std::size_t>(tile_y)]
                                      [static_cast<std::size_t>(tile_x)];
                }
            }
            for (StageEntitySpawn& spawn : room.entity_spawns) {
                spawn.pos += room_pos_wc;
                stage.entity_spawns.push_back(std::move(spawn));
            }
            for (BackgroundStamp& stamp : room.background_stamps) {
                stamp.pos += room_pos_wc;
                stage.background_stamps.push_back(std::move(stamp));
            }
        }
    }

    stage.stage_type = stage_type;
    stage.tiles = std::move(tiles);
    stage.embedded_treasures = std::vector<std::vector<EntityType>>(
        stage.tiles.size(),
        std::vector<EntityType>(
            stage.tiles.empty() ? 0U : stage.tiles.front().size(),
            EntityType::None
        )
    );
    stage.rooms = std::move(room_codes);
    stage.path = layout.path;
    stage.gravity = 0.3F;
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    AddMinesEmbeddedTreasure(stage);
    AddMinesTreasure(stage);
    ConvertBlocksToArrowTraps(stage);
    AddAmbientMinesEntities(stage);
    return stage;
}

const char* GetRoomCodeDebugLabel(int room_code) {
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
    case 8:
        return "pit";
    case 9:
        return "pit!";
    default:
        return "?";
    }
}

} // namespace splonks::stage_gen::hd_mines
