#include "stage_gen/test.hpp"

#include "room.hpp"
#include "stage.hpp"
#include "stage_gen/cave.hpp"
#include "math_types.hpp"

#include <cstddef>
#include <vector>

namespace splonks::stage_gen::test {

namespace {

bool RandomBool(DetRng& det_rng) {
    return det_rng.RandomIntInclusive(0, 1) == 0;
}

UVec2 Fit(const UVec2& available_area, const UVec2& size, bool grounded, DetRng& det_rng) {
    if (available_area.x < size.x || available_area.y < size.y) {
        return UVec2::New(0, 0);
    }

    unsigned int x = 0;
    unsigned int y = 0;
    if (available_area.x > size.x) {
        x = static_cast<unsigned int>(
            det_rng.RandomIntInclusive(0, static_cast<int>(available_area.x - size.x - 1U)));
    }
    if (!grounded) {
        if (available_area.y > size.y) {
            y = static_cast<unsigned int>(
                det_rng.RandomIntInclusive(0, static_cast<int>(available_area.y - size.y - 1U)));
        }
    } else {
        y = available_area.y - size.y;
    }
    return UVec2::New(x, y);
}

UVec2 FitTemplate(const UVec2& available_area,
                  const std::vector<std::vector<MetaTile>>& room_template, bool grounded,
                  DetRng& det_rng) {
    const UVec2 size =
        UVec2::New(static_cast<std::uint32_t>(room_template[0].size()),
                   static_cast<std::uint32_t>(room_template.size()));
    return Fit(available_area, size, grounded, det_rng);
}

std::vector<std::vector<MetaTile>> BlankRoom() {
    return std::vector<std::vector<MetaTile>>(
        static_cast<std::size_t>(Stage::kRoomShape.y),
        std::vector<MetaTile>(static_cast<std::size_t>(Stage::kRoomShape.x), MetaTile::Air));
}

std::vector<std::vector<MetaTile>> StandinEntranceTemplate() {
    auto room = BlankRoom();
    const UVec2 middle = Stage::kRoomShape / 2U;
    const UVec2 other_middle = middle - UVec2::New(1, 1);

    for (unsigned int y = 0; y < Stage::kRoomShape.y; ++y) {
        for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
            const bool edge =
                y == 0 || y == Stage::kRoomShape.y - 1 || x == 0 || x == Stage::kRoomShape.x - 1;
            const bool open =
                y == middle.y || x == middle.x || y == other_middle.y || x == other_middle.x;
            if (edge && !open) {
                room[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = MetaTile::Solid;
            }
        }
    }

    room[static_cast<std::size_t>(middle.y)][static_cast<std::size_t>(middle.x)] =
        MetaTile::Entrance;
    return room;
}

std::vector<std::vector<MetaTile>> StandinFourWayTemplate() {
    auto room = BlankRoom();
    const UVec2 middle = Stage::kRoomShape / 2U;
    const UVec2 other_middle = middle - UVec2::New(1, 1);

    for (unsigned int y = 0; y < Stage::kRoomShape.y; ++y) {
        for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
            const bool edge =
                y == 0 || y == Stage::kRoomShape.y - 1 || x == 0 || x == Stage::kRoomShape.x - 1;
            const bool open =
                y == middle.y || x == middle.x || y == other_middle.y || x == other_middle.x;
            if (edge && !open) {
                room[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = MetaTile::Solid;
            }
        }
    }
    return room;
}

std::vector<std::vector<MetaTile>> StandinBoxTemplate() {
    auto room = BlankRoom();
    for (unsigned int y = 0; y < Stage::kRoomShape.y; ++y) {
        for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
            room[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = MetaTile::Solid;
        }
    }
    return room;
}

std::vector<std::vector<MetaTile>> StandinLeftDownRightTemplate() {
    auto room = BlankRoom();

    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[0][static_cast<std::size_t>(x)] = MetaTile::Solid;
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)]
        [static_cast<std::size_t>(Stage::kRoomShape.x / 2U)] = MetaTile::Air;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)]
        [static_cast<std::size_t>(Stage::kRoomShape.x / 2U - 1U)] = MetaTile::Air;
    return room;
}

std::vector<std::vector<MetaTile>> StandinLeftRightTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[0][static_cast<std::size_t>(x)] = MetaTile::Solid;
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }
    return room;
}

std::vector<std::vector<MetaTile>> StandinLeftUpRightTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }
    room[0][0] = MetaTile::Solid;
    room[0][static_cast<std::size_t>(Stage::kRoomShape.x - 1)] = MetaTile::Solid;
    return room;
}

std::vector<std::vector<MetaTile>> DoubleLadderTemplate() {
    return {
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Ladder, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Ladder,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::LadderTop, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::LadderTop,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Ladder, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Ladder,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::LadderTop, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::LadderTop,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Ladder, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Ladder,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Ladder, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Ladder,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
    };
}

std::vector<std::vector<MetaTile>> BoxDoubleLadderTemplate() {
    return {
        {MetaTile::Solid, MetaTile::MaybeSolid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::MaybeSolid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Ladder, MetaTile::MaybeSolid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid, MetaTile::Ladder,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::LadderTop, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::LadderTop,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Ladder, MetaTile::MaybeSolid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid, MetaTile::Ladder,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Ladder, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Ladder,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Ladder, MetaTile::Solid,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::Solid,
         MetaTile::Ladder, MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Ladder, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Ladder,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid},
    };
}

std::vector<std::vector<MetaTile>> SidewaysEtExitTemplate() {
    return {
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Solid, MetaTile::Air,
         MetaTile::Air, MetaTile::MaybeSolid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Solid, MetaTile::Air,
         MetaTile::Air, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Air, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Exit, MetaTile::Air, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::MaybeSolid, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
    };
}

void PasteOHalf(const UVec2& available_area, const UVec2& at,
                std::vector<std::vector<MetaTile>>& target, bool grounded, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::MaybeSolid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, false, RandomBool(det_rng));
}

void PasteFiveLong(const UVec2& available_area, const UVec2& at,
                   std::vector<std::vector<MetaTile>>& target, bool grounded, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteFourLong(const UVec2& available_area, const UVec2& at,
                   std::vector<std::vector<MetaTile>>& target, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, false, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteFourLongMaybe(const UVec2& available_area, const UVec2& at,
                        std::vector<std::vector<MetaTile>>& target, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, false, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteFiveLongMaybe(const UVec2& available_area, const UVec2& at,
                        std::vector<std::vector<MetaTile>>& target, bool grounded, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteFourLongWithTwoAboveMaybe(const UVec2& available_area, const UVec2& at,
                                    std::vector<std::vector<MetaTile>>& target, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Air, MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, false, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteThreeLong(const UVec2& available_area, const UVec2& at,
                    std::vector<std::vector<MetaTile>>& target, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air},
        {MetaTile::Air, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air},
    };
    const UVec2 position = FitTemplate(available_area, room_template, false, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteAirSubroom(const UVec2& subroom_shape, const UVec2& at,
                     std::vector<std::vector<MetaTile>>& target, DetRng& det_rng) {
    const int choice = det_rng.RandomIntInclusive(0, 5);
    if (choice == 0) {
        PasteOHalf(subroom_shape, at, target, false, det_rng);
    } else if (choice == 1) {
        PasteFiveLong(subroom_shape, at, target, false, det_rng);
    } else if (choice == 2) {
        PasteFiveLongMaybe(subroom_shape, at, target, false, det_rng);
    } else if (choice == 3) {
        PasteFourLongWithTwoAboveMaybe(subroom_shape, at, target, det_rng);
    } else if (choice == 4) {
        PasteThreeLong(subroom_shape, at, target, det_rng);
    }
}

void PasteHillsOnSpikes(const UVec2& available_area, const UVec2& at,
                        std::vector<std::vector<MetaTile>>& target, bool grounded, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Air, MetaTile::MaybeSolid, MetaTile::Air, MetaTile::MaybeSolid,
         MetaTile::Air},
        {MetaTile::MaybeSpikes, MetaTile::Solid, MetaTile::MaybeSpikes,
         MetaTile::Solid, MetaTile::MaybeSpikes},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteHillsOnSpikesAssymetrical(const UVec2& available_area, const UVec2& at,
                                    std::vector<std::vector<MetaTile>>& target,
                                    bool grounded, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid, MetaTile::Air,
         MetaTile::MaybeSolid},
        {MetaTile::MaybeSpikes, MetaTile::MaybeSpikes, MetaTile::Solid,
         MetaTile::MaybeSpikes, MetaTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, RandomBool(det_rng), false);
}

void PasteStepsAndFloatingBlockWithSpikes(const UVec2& available_area, const UVec2& at,
                                          std::vector<std::vector<MetaTile>>& target,
                                          bool grounded, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Air, MetaTile::Air, MetaTile::Solid, MetaTile::Air,
         MetaTile::MaybeSolid},
        {MetaTile::MaybeSpikes, MetaTile::Solid, MetaTile::Solid,
         MetaTile::MaybeSpikes, MetaTile::MaybeSpikes},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, RandomBool(det_rng), false);
}

void PasteMound(const UVec2& available_area, const UVec2& at,
                std::vector<std::vector<MetaTile>>& target, bool grounded, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Air, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::Air},
        {MetaTile::MaybeSolid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::MaybeSolid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteGroundSubroom(const UVec2& subroom_shape, const UVec2& at,
                        std::vector<std::vector<MetaTile>>& target, DetRng& det_rng) {
    const int choice = det_rng.RandomIntInclusive(0, 6);
    if (choice == 0) {
        PasteHillsOnSpikes(subroom_shape, at, target, true, det_rng);
    } else if (choice == 1) {
        PasteHillsOnSpikesAssymetrical(subroom_shape, at, target, true, det_rng);
    } else if (choice == 2) {
        PasteMound(subroom_shape, at, target, true, det_rng);
    } else if (choice == 3) {
        PasteStepsAndFloatingBlockWithSpikes(subroom_shape, at, target, true, det_rng);
    } else if (choice == 4) {
        PasteFourLong(subroom_shape, at, target, det_rng);
    } else if (choice == 5) {
        PasteOHalf(subroom_shape, at, target, true, det_rng);
    } else if (choice == 6) {
        PasteFiveLong(subroom_shape, at, target, false, det_rng);
    }
}

void PasteDoorHutAndPillar(const UVec2& available_area, const UVec2& at,
                           std::vector<std::vector<MetaTile>>& target, bool grounded,
                           bool entrance, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Air, MetaTile::Air,
         MetaTile::Solid},
        {MetaTile::Solid, entrance ? MetaTile::Entrance : MetaTile::Exit,
         MetaTile::Air, MetaTile::Air, MetaTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, RandomBool(det_rng), false);
}

void PasteDoorPyramid(const UVec2& available_area, const UVec2& at,
                      std::vector<std::vector<MetaTile>>& target, bool grounded,
                      bool entrance, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Air, MetaTile::Air, entrance ? MetaTile::Entrance : MetaTile::Exit,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteDoorPillared(const UVec2& available_area, const UVec2& at,
                       std::vector<std::vector<MetaTile>>& target, bool grounded,
                       bool entrance, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Solid, MetaTile::MaybeSolid, MetaTile::Air, MetaTile::MaybeSolid,
         MetaTile::Solid},
        {MetaTile::Solid, MetaTile::MaybeSolid,
         entrance ? MetaTile::Entrance : MetaTile::Exit, MetaTile::MaybeSolid,
         MetaTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteDoorHut(const UVec2& available_area, const UVec2& at,
                  std::vector<std::vector<MetaTile>>& target, bool grounded, bool entrance,
                  DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Solid},
        {MetaTile::MaybeBlock, MetaTile::Air, MetaTile::Air,
         entrance ? MetaTile::Entrance : MetaTile::Exit, MetaTile::MaybeBlock},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, RandomBool(det_rng), false);
}

void PasteDoorStilted(const UVec2& available_area, const UVec2& at,
                      std::vector<std::vector<MetaTile>>& target, bool grounded,
                      bool entrance, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::Air, MetaTile::Air, entrance ? MetaTile::Entrance : MetaTile::Exit,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::MaybeSolid, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::Air},
        {MetaTile::Air, MetaTile::MaybeSolid, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::Air},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteDoorFourblock(const UVec2& available_area, const UVec2& at,
                        std::vector<std::vector<MetaTile>>& target, bool grounded,
                        bool entrance, DetRng& det_rng) {
    const std::vector<std::vector<MetaTile>> room_template = {
        {MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, entrance ? MetaTile::Entrance : MetaTile::Exit},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded, det_rng) + at;
    PasteTemplate(target, room_template, position, RandomBool(det_rng), false);
}

void PasteBottomExitSubroom(const UVec2& subroom_shape, const UVec2& at,
                            std::vector<std::vector<MetaTile>>& target, DetRng& det_rng) {
    const int choice = det_rng.RandomIntInclusive(0, 5);
    if (choice == 0) {
        PasteDoorFourblock(subroom_shape, at, target, true, false, det_rng);
    } else if (choice == 1) {
        PasteDoorHut(subroom_shape, at, target, true, false, det_rng);
    } else if (choice == 2) {
        PasteDoorHutAndPillar(subroom_shape, at, target, true, false, det_rng);
    } else if (choice == 3) {
        PasteDoorPillared(subroom_shape, at, target, true, false, det_rng);
    } else if (choice == 4) {
        PasteDoorPyramid(subroom_shape, at, target, true, false, det_rng);
    } else if (choice == 5) {
        PasteDoorStilted(subroom_shape, at, target, true, false, det_rng);
    }
}

std::vector<std::vector<MetaTile>> TwoSubroomsAboveExitTemplate(DetRng& det_rng) {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }

    const UVec2 subroom_shape = UVec2::New(5, 3);
    const UVec2 tl_subroom_pos = UVec2::New(0, 0);
    const int choice_left = det_rng.RandomIntInclusive(0, 3);
    if (choice_left == 0) {
        PasteOHalf(subroom_shape, tl_subroom_pos, room, false, det_rng);
    } else if (choice_left == 1) {
        PasteFiveLong(subroom_shape, tl_subroom_pos, room, false, det_rng);
    } else if (choice_left == 2) {
        PasteFiveLongMaybe(subroom_shape, tl_subroom_pos, room, false, det_rng);
    } else {
        PasteFourLongWithTwoAboveMaybe(subroom_shape, tl_subroom_pos, room, det_rng);
    }

    const UVec2 tr_subroom_pos = UVec2::New(5, 0);
    const int choice_right = det_rng.RandomIntInclusive(0, 3);
    if (choice_right == 0) {
        PasteOHalf(subroom_shape, tr_subroom_pos, room, false, det_rng);
    } else if (choice_right == 1) {
        PasteFiveLong(subroom_shape, tr_subroom_pos, room, false, det_rng);
    } else if (choice_right == 2) {
        PasteFiveLongMaybe(subroom_shape, tr_subroom_pos, room, false, det_rng);
    } else {
        PasteFourLongWithTwoAboveMaybe(subroom_shape, tr_subroom_pos, room, det_rng);
    }

    PasteBottomExitSubroom(subroom_shape, UVec2::New(3, 4), room, det_rng);

    return room;
}

std::vector<std::vector<MetaTile>> BoxDigitEightTemplate() {
    return {
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Air,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::Air, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Air,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::Air, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid},
    };
}

std::vector<std::vector<MetaTile>> BoxFallenFloorTemplate() {
    return {
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::Solid, MetaTile::Solid},
        {MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Solid, MetaTile::Solid},
    };
}

std::vector<std::vector<MetaTile>> TwoLineOneSubroomTemplate(DetRng& det_rng) {
    auto room = BlankRoom();
    const std::vector<std::vector<MetaTile>> two_line_template = {
        {MetaTile::Solid, MetaTile::Air, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Solid, MetaTile::Solid, MetaTile::Air,
         MetaTile::Solid, MetaTile::Air},
    };
    PasteTemplate(room, two_line_template, UVec2::New(2, 1), false, false);
    const std::vector<std::vector<MetaTile>> bottom_row = {
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
    };
    PasteTemplate(room, bottom_row, UVec2::New(0, Stage::kRoomShape.y - 1), false, false);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 4), room, det_rng);
    return room;
}

std::vector<std::vector<MetaTile>> BoxOneSidedLadderTwoSubroomsTemplate(DetRng& det_rng) {
    auto room = std::vector<std::vector<MetaTile>>{
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Ladder,
         MetaTile::Air, MetaTile::MaybeBlock},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::LadderTop,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Ladder,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Ladder,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
    };
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 1), room, det_rng);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 4), room, det_rng);
    return room;
}

std::vector<std::vector<MetaTile>> BoxOneSidedLadderOneSubroomTemplate(DetRng& det_rng) {
    auto room = std::vector<std::vector<MetaTile>>{
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Ladder,
         MetaTile::Air, MetaTile::MaybeBlock},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::LadderTop,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Ladder,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Ladder,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid},
    };
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 1), room, det_rng);
    return room;
}

std::vector<std::vector<MetaTile>> BoxFingerHoleTemplate() {
    return {
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
        {MetaTile::Air, MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::Air, MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::MaybeSolid,
         MetaTile::Air, MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::MaybeSolid, MetaTile::Air},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
    };
}

std::vector<std::vector<MetaTile>> LurOneSubroomTemplate(DetRng& det_rng) {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }
    PasteGroundSubroom(UVec2::New(5, 3), UVec2::New(5, 4), room, det_rng);
    return room;
}

std::vector<std::vector<MetaTile>> AnthillTemplate() {
    return {
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::MaybeSolid, MetaTile::MaybeSolid, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid,
         MetaTile::Air, MetaTile::Air},
        {MetaTile::Air, MetaTile::Solid, MetaTile::Solid, MetaTile::MaybeSolid,
         MetaTile::Air, MetaTile::Air, MetaTile::MaybeSolid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Air},
        {MetaTile::Air, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Air, MetaTile::Air, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Air},
        {MetaTile::Solid, MetaTile::MaybeSolid, MetaTile::Air, MetaTile::Air,
         MetaTile::Air, MetaTile::Air, MetaTile::Air, MetaTile::Air,
         MetaTile::MaybeSolid, MetaTile::Solid},
        {MetaTile::Solid, MetaTile::Solid, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Air, MetaTile::Air, MetaTile::Solid, MetaTile::Solid,
         MetaTile::Solid, MetaTile::Solid},
    };
}

std::vector<std::vector<MetaTile>> ThreeCornerDropTemplate(DetRng& det_rng) {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][1] = MetaTile::MaybeSolid;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][2] = MetaTile::Air;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][3] = MetaTile::MaybeSolid;

    const UVec2 subroom_shape = UVec2::New(5, 3);
    PasteAirSubroom(subroom_shape, UVec2::New(0, 1), room, det_rng);
    PasteAirSubroom(subroom_shape, UVec2::New(5, 1), room, det_rng);
    PasteGroundSubroom(subroom_shape, UVec2::New(5, 4), room, det_rng);
    return room;
}

std::vector<std::vector<MetaTile>> TunnelAndSubroomTemplate(DetRng& det_rng) {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }
    const std::size_t tunnel_height = static_cast<std::size_t>(Stage::kRoomShape.y - 3);
    room[tunnel_height][2] = MetaTile::MaybeSolid;
    room[tunnel_height][3] = MetaTile::MaybeSolid;
    room[tunnel_height][4] = MetaTile::Solid;
    room[tunnel_height][5] = MetaTile::Solid;
    room[tunnel_height][6] = MetaTile::Solid;
    room[tunnel_height][7] = MetaTile::Solid;
    room[tunnel_height + 1][7] = MetaTile::Solid;
    room[tunnel_height + 1][8] = MetaTile::Solid;
    room[tunnel_height + 2][6] = MetaTile::Air;
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 1), room, det_rng);
    return room;
}

std::vector<std::vector<MetaTile>> DropWithTwoUpperSubroomsTemplate(DetRng& det_rng) {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][1] = MetaTile::MaybeSolid;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][2] = MetaTile::Air;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][3] = MetaTile::MaybeSolid;
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(0, 1), room, det_rng);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(5, 1), room, det_rng);
    return room;
}

std::vector<std::vector<MetaTile>> OpenWideDropTemplate(DetRng& det_rng) {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }
    const std::size_t crater_height = static_cast<std::size_t>(Stage::kRoomShape.y - 2);
    room[crater_height][1] = MetaTile::MaybeSolid;
    room[crater_height][2] = MetaTile::MaybeSolid;
    room[crater_height][7] = MetaTile::MaybeSolid;
    room[crater_height][8] = MetaTile::MaybeSolid;
    room[crater_height + 1][3] = MetaTile::MaybeSolid;
    room[crater_height + 1][4] = MetaTile::Air;
    room[crater_height + 1][5] = MetaTile::Air;
    room[crater_height + 1][6] = MetaTile::MaybeSolid;
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(0, 1), room, det_rng);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(5, 1), room, det_rng);
    return room;
}

std::vector<std::vector<MetaTile>> CrestedDropWithUpperSubroomTemplate(DetRng& det_rng) {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }
    const std::size_t crater_height = static_cast<std::size_t>(Stage::kRoomShape.y - 3);
    room[crater_height][2] = MetaTile::MaybeSolid;
    room[crater_height][4] = MetaTile::MaybeSolid;
    room[crater_height][5] = MetaTile::MaybeSolid;
    room[crater_height][7] = MetaTile::MaybeSolid;
    room[crater_height + 1][2] = MetaTile::Solid;
    room[crater_height + 1][7] = MetaTile::Solid;
    room[crater_height + 2][4] = MetaTile::Air;
    room[crater_height + 2][5] = MetaTile::Air;
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 1), room, det_rng);
    return room;
}

std::vector<std::vector<MetaTile>> OpenFourSubroomTemplate(DetRng& det_rng) {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            MetaTile::Solid;
    }
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][1] = MetaTile::MaybeSolid;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][2] = MetaTile::Air;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][3] = MetaTile::MaybeSolid;
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(0, 1), room, det_rng);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(5, 1), room, det_rng);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(0, 4), room, det_rng);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(5, 4), room, det_rng);
    return room;
}

} // namespace

std::vector<std::vector<MetaTile>> GetRoomTemplate(RoomType room_type, DetRng& det_rng) {
    switch (room_type) {
    case RoomType::Box: {
        const int choice = det_rng.RandomIntInclusive(0, 6);
        if (choice == 0) {
            return BoxDigitEightTemplate();
        }
        if (choice == 1) {
            return BoxFallenFloorTemplate();
        }
        if (choice == 2) {
            return BoxOneSidedLadderTwoSubroomsTemplate(det_rng);
        }
        if (choice == 3) {
            return BoxDoubleLadderTemplate();
        }
        if (choice == 4) {
            return BoxOneSidedLadderOneSubroomTemplate(det_rng);
        }
        if (choice == 5) {
            return TwoLineOneSubroomTemplate(det_rng);
        }
        return BoxFingerHoleTemplate();
    }
    case RoomType::FourWay:
        switch (det_rng.RandomIntInclusive(0, 6)) {
        case 0:
            return OpenFourSubroomTemplate(det_rng);
        case 1:
            return AnthillTemplate();
        case 2:
            return CrestedDropWithUpperSubroomTemplate(det_rng);
        case 3:
            return TunnelAndSubroomTemplate(det_rng);
        case 4:
            return OpenWideDropTemplate(det_rng);
        case 5:
            return ThreeCornerDropTemplate(det_rng);
        case 6:
            return DropWithTwoUpperSubroomsTemplate(det_rng);
        default:
            return StandinFourWayTemplate();
        }
    case RoomType::LeftDownRight:
        switch (det_rng.RandomIntInclusive(0, 2)) {
        case 0:
            return AnthillTemplate();
        case 1:
            return StandinLeftDownRightTemplate();
        case 2:
            return ThreeCornerDropTemplate(det_rng);
        default:
            return StandinLeftDownRightTemplate();
        }
    case RoomType::LeftRight:
        return DoubleLadderTemplate();
    case RoomType::LeftUpRight:
        return StandinLeftUpRightTemplate();
    case RoomType::Exit:
        return det_rng.RandomIntInclusive(0, 99) < 33 ? SidewaysEtExitTemplate() : TwoSubroomsAboveExitTemplate(det_rng);
    case RoomType::Entrance:
        return StandinEntranceTemplate();
    }

    return cave::GetRoomTemplate(room_type, det_rng);
}

} // namespace splonks::stage_gen::test
