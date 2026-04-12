#include "stage_gen/test.hpp"

#include "room.hpp"
#include "stage.hpp"
#include "stage_gen/cave.hpp"
#include "math_types.hpp"

#include <cstddef>
#include <random>
#include <vector>

namespace splonks::stage_gen::test {

namespace {

int RandomIntInclusive(int minimum, int maximum) {
    static std::random_device device;
    static std::mt19937 generator(device());

    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(generator);
}

bool RandomBool() {
    return RandomIntInclusive(0, 1) == 0;
}

UVec2 Fit(const UVec2& available_area, const UVec2& size, bool grounded) {
    if (available_area.x < size.x || available_area.y < size.y) {
        return UVec2::New(0, 0);
    }

    unsigned int x = 0;
    unsigned int y = 0;
    if (available_area.x > size.x) {
        x = static_cast<unsigned int>(
            RandomIntInclusive(0, static_cast<int>(available_area.x - size.x - 1U)));
    }
    if (!grounded) {
        if (available_area.y > size.y) {
            y = static_cast<unsigned int>(
                RandomIntInclusive(0, static_cast<int>(available_area.y - size.y - 1U)));
        }
    } else {
        y = available_area.y - size.y;
    }
    return UVec2::New(x, y);
}

UVec2 FitTemplate(const UVec2& available_area,
                  const std::vector<std::vector<TemplateTile>>& room_template, bool grounded) {
    const UVec2 size =
        UVec2::New(static_cast<unsigned int>(room_template[0].size()),
                   static_cast<unsigned int>(room_template.size()));
    return Fit(available_area, size, grounded);
}

std::vector<std::vector<TemplateTile>> BlankRoom() {
    return std::vector<std::vector<TemplateTile>>(
        static_cast<std::size_t>(Stage::kRoomShape.y),
        std::vector<TemplateTile>(static_cast<std::size_t>(Stage::kRoomShape.x), TemplateTile::Air));
}

std::vector<std::vector<TemplateTile>> StandinEntranceTemplate() {
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
                room[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = TemplateTile::Solid;
            }
        }
    }

    room[static_cast<std::size_t>(middle.y)][static_cast<std::size_t>(middle.x)] =
        TemplateTile::Entrance;
    return room;
}

std::vector<std::vector<TemplateTile>> StandinFourWayTemplate() {
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
                room[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = TemplateTile::Solid;
            }
        }
    }
    return room;
}

std::vector<std::vector<TemplateTile>> StandinBoxTemplate() {
    auto room = BlankRoom();
    for (unsigned int y = 0; y < Stage::kRoomShape.y; ++y) {
        for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
            room[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = TemplateTile::Solid;
        }
    }
    return room;
}

std::vector<std::vector<TemplateTile>> StandinLeftDownRightTemplate() {
    auto room = BlankRoom();

    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[0][static_cast<std::size_t>(x)] = TemplateTile::Solid;
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)]
        [static_cast<std::size_t>(Stage::kRoomShape.x / 2U)] = TemplateTile::Air;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)]
        [static_cast<std::size_t>(Stage::kRoomShape.x / 2U - 1U)] = TemplateTile::Air;
    return room;
}

std::vector<std::vector<TemplateTile>> StandinLeftRightTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[0][static_cast<std::size_t>(x)] = TemplateTile::Solid;
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }
    return room;
}

std::vector<std::vector<TemplateTile>> StandinLeftUpRightTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }
    room[0][0] = TemplateTile::Solid;
    room[0][static_cast<std::size_t>(Stage::kRoomShape.x - 1)] = TemplateTile::Solid;
    return room;
}

std::vector<std::vector<TemplateTile>> DoubleLadderTemplate() {
    return {
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::LadderTop, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::LadderTop,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::LadderTop, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::LadderTop,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Ladder,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Ladder,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
    };
}

std::vector<std::vector<TemplateTile>> BoxDoubleLadderTemplate() {
    return {
        {TemplateTile::Solid, TemplateTile::MaybeSolid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::MaybeSolid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder, TemplateTile::MaybeSolid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid, TemplateTile::Ladder,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::LadderTop, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::LadderTop,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Ladder, TemplateTile::MaybeSolid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid, TemplateTile::Ladder,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Ladder, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Ladder,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Ladder, TemplateTile::Solid,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::Solid,
         TemplateTile::Ladder, TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid},
    };
}

std::vector<std::vector<TemplateTile>> SidewaysEtExitTemplate() {
    return {
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Solid, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Solid, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Exit, TemplateTile::Air, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::MaybeSolid, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
    };
}

void PasteOHalf(const UVec2& available_area, const UVec2& at,
                std::vector<std::vector<TemplateTile>>& target, bool grounded) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, false, RandomBool());
}

void PasteFiveLong(const UVec2& available_area, const UVec2& at,
                   std::vector<std::vector<TemplateTile>>& target, bool grounded) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteFourLong(const UVec2& available_area, const UVec2& at,
                   std::vector<std::vector<TemplateTile>>& target) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, false) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteFourLongMaybe(const UVec2& available_area, const UVec2& at,
                        std::vector<std::vector<TemplateTile>>& target) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, false) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteFiveLongMaybe(const UVec2& available_area, const UVec2& at,
                        std::vector<std::vector<TemplateTile>>& target, bool grounded) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteFourLongWithTwoAboveMaybe(const UVec2& available_area, const UVec2& at,
                                    std::vector<std::vector<TemplateTile>>& target) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, false) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteThreeLong(const UVec2& available_area, const UVec2& at,
                    std::vector<std::vector<TemplateTile>>& target) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air},
    };
    const UVec2 position = FitTemplate(available_area, room_template, false) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteAirSubroom(const UVec2& subroom_shape, const UVec2& at,
                     std::vector<std::vector<TemplateTile>>& target) {
    const int choice = RandomIntInclusive(0, 5);
    if (choice == 0) {
        PasteOHalf(subroom_shape, at, target, false);
    } else if (choice == 1) {
        PasteFiveLong(subroom_shape, at, target, false);
    } else if (choice == 2) {
        PasteFiveLongMaybe(subroom_shape, at, target, false);
    } else if (choice == 3) {
        PasteFourLongWithTwoAboveMaybe(subroom_shape, at, target);
    } else if (choice == 4) {
        PasteThreeLong(subroom_shape, at, target);
    }
}

void PasteHillsOnSpikes(const UVec2& available_area, const UVec2& at,
                        std::vector<std::vector<TemplateTile>>& target, bool grounded) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::Air, TemplateTile::MaybeSolid,
         TemplateTile::Air},
        {TemplateTile::MaybeSpikes, TemplateTile::Solid, TemplateTile::MaybeSpikes,
         TemplateTile::Solid, TemplateTile::MaybeSpikes},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteHillsOnSpikesAssymetrical(const UVec2& available_area, const UVec2& at,
                                    std::vector<std::vector<TemplateTile>>& target,
                                    bool grounded) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::Air,
         TemplateTile::MaybeSolid},
        {TemplateTile::MaybeSpikes, TemplateTile::MaybeSpikes, TemplateTile::Solid,
         TemplateTile::MaybeSpikes, TemplateTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, RandomBool(), false);
}

void PasteStepsAndFloatingBlockWithSpikes(const UVec2& available_area, const UVec2& at,
                                          std::vector<std::vector<TemplateTile>>& target,
                                          bool grounded) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Solid, TemplateTile::Air,
         TemplateTile::MaybeSolid},
        {TemplateTile::MaybeSpikes, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::MaybeSpikes, TemplateTile::MaybeSpikes},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, RandomBool(), false);
}

void PasteMound(const UVec2& available_area, const UVec2& at,
                std::vector<std::vector<TemplateTile>>& target, bool grounded) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::Air},
        {TemplateTile::MaybeSolid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::MaybeSolid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteGroundSubroom(const UVec2& subroom_shape, const UVec2& at,
                        std::vector<std::vector<TemplateTile>>& target) {
    const int choice = RandomIntInclusive(0, 6);
    if (choice == 0) {
        PasteHillsOnSpikes(subroom_shape, at, target, true);
    } else if (choice == 1) {
        PasteHillsOnSpikesAssymetrical(subroom_shape, at, target, true);
    } else if (choice == 2) {
        PasteMound(subroom_shape, at, target, true);
    } else if (choice == 3) {
        PasteStepsAndFloatingBlockWithSpikes(subroom_shape, at, target, true);
    } else if (choice == 4) {
        PasteFourLong(subroom_shape, at, target);
    } else if (choice == 5) {
        PasteOHalf(subroom_shape, at, target, true);
    } else if (choice == 6) {
        PasteFiveLong(subroom_shape, at, target, false);
    }
}

void PasteDoorHutAndPillar(const UVec2& available_area, const UVec2& at,
                           std::vector<std::vector<TemplateTile>>& target, bool grounded,
                           bool entrance) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Solid},
        {TemplateTile::Solid, entrance ? TemplateTile::Entrance : TemplateTile::Exit,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, RandomBool(), false);
}

void PasteDoorPyramid(const UVec2& available_area, const UVec2& at,
                      std::vector<std::vector<TemplateTile>>& target, bool grounded,
                      bool entrance) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Air, TemplateTile::Air, entrance ? TemplateTile::Entrance : TemplateTile::Exit,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteDoorPillared(const UVec2& available_area, const UVec2& at,
                       std::vector<std::vector<TemplateTile>>& target, bool grounded,
                       bool entrance) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Solid, TemplateTile::MaybeSolid, TemplateTile::Air, TemplateTile::MaybeSolid,
         TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::MaybeSolid,
         entrance ? TemplateTile::Entrance : TemplateTile::Exit, TemplateTile::MaybeSolid,
         TemplateTile::Solid},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteDoorHut(const UVec2& available_area, const UVec2& at,
                  std::vector<std::vector<TemplateTile>>& target, bool grounded, bool entrance) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Solid},
        {TemplateTile::MaybeBlock, TemplateTile::Air, TemplateTile::Air,
         entrance ? TemplateTile::Entrance : TemplateTile::Exit, TemplateTile::MaybeBlock},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, RandomBool(), false);
}

void PasteDoorStilted(const UVec2& available_area, const UVec2& at,
                      std::vector<std::vector<TemplateTile>>& target, bool grounded,
                      bool entrance) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::Air, TemplateTile::Air, entrance ? TemplateTile::Entrance : TemplateTile::Exit,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::Air},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, false, false);
}

void PasteDoorFourblock(const UVec2& available_area, const UVec2& at,
                        std::vector<std::vector<TemplateTile>>& target, bool grounded,
                        bool entrance) {
    const std::vector<std::vector<TemplateTile>> room_template = {
        {TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, entrance ? TemplateTile::Entrance : TemplateTile::Exit},
    };
    const UVec2 position = FitTemplate(available_area, room_template, grounded) + at;
    PasteTemplate(target, room_template, position, RandomBool(), false);
}

void PasteBottomExitSubroom(const UVec2& subroom_shape, const UVec2& at,
                            std::vector<std::vector<TemplateTile>>& target) {
    const int choice = RandomIntInclusive(0, 5);
    if (choice == 0) {
        PasteDoorFourblock(subroom_shape, at, target, true, false);
    } else if (choice == 1) {
        PasteDoorHut(subroom_shape, at, target, true, false);
    } else if (choice == 2) {
        PasteDoorHutAndPillar(subroom_shape, at, target, true, false);
    } else if (choice == 3) {
        PasteDoorPillared(subroom_shape, at, target, true, false);
    } else if (choice == 4) {
        PasteDoorPyramid(subroom_shape, at, target, true, false);
    } else if (choice == 5) {
        PasteDoorStilted(subroom_shape, at, target, true, false);
    }
}

std::vector<std::vector<TemplateTile>> TwoSubroomsAboveExitTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }

    const UVec2 subroom_shape = UVec2::New(5, 3);
    const UVec2 tl_subroom_pos = UVec2::New(0, 0);
    const int choice_left = RandomIntInclusive(0, 3);
    if (choice_left == 0) {
        PasteOHalf(subroom_shape, tl_subroom_pos, room, false);
    } else if (choice_left == 1) {
        PasteFiveLong(subroom_shape, tl_subroom_pos, room, false);
    } else if (choice_left == 2) {
        PasteFiveLongMaybe(subroom_shape, tl_subroom_pos, room, false);
    } else {
        PasteFourLongWithTwoAboveMaybe(subroom_shape, tl_subroom_pos, room);
    }

    const UVec2 tr_subroom_pos = UVec2::New(5, 0);
    const int choice_right = RandomIntInclusive(0, 3);
    if (choice_right == 0) {
        PasteOHalf(subroom_shape, tr_subroom_pos, room, false);
    } else if (choice_right == 1) {
        PasteFiveLong(subroom_shape, tr_subroom_pos, room, false);
    } else if (choice_right == 2) {
        PasteFiveLongMaybe(subroom_shape, tr_subroom_pos, room, false);
    } else {
        PasteFourLongWithTwoAboveMaybe(subroom_shape, tr_subroom_pos, room);
    }

    PasteBottomExitSubroom(subroom_shape, UVec2::New(3, 4), room);

    return room;
}

std::vector<std::vector<TemplateTile>> BoxDigitEightTemplate() {
    return {
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Air,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::Air, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Air,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::Air, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid},
    };
}

std::vector<std::vector<TemplateTile>> BoxFallenFloorTemplate() {
    return {
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Solid, TemplateTile::Solid},
    };
}

std::vector<std::vector<TemplateTile>> TwoLineOneSubroomTemplate() {
    auto room = BlankRoom();
    const std::vector<std::vector<TemplateTile>> two_line_template = {
        {TemplateTile::Solid, TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Air,
         TemplateTile::Solid, TemplateTile::Air},
    };
    PasteTemplate(room, two_line_template, UVec2::New(2, 1), false, false);
    const std::vector<std::vector<TemplateTile>> bottom_row = {
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
    };
    PasteTemplate(room, bottom_row, UVec2::New(0, Stage::kRoomShape.y - 1), false, false);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 4), room);
    return room;
}

std::vector<std::vector<TemplateTile>> BoxOneSidedLadderTwoSubroomsTemplate() {
    auto room = std::vector<std::vector<TemplateTile>>{
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder,
         TemplateTile::Air, TemplateTile::MaybeBlock},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::LadderTop,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
    };
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 1), room);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 4), room);
    return room;
}

std::vector<std::vector<TemplateTile>> BoxOneSidedLadderOneSubroomTemplate() {
    auto room = std::vector<std::vector<TemplateTile>>{
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder,
         TemplateTile::Air, TemplateTile::MaybeBlock},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::LadderTop,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Ladder,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid},
    };
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 1), room);
    return room;
}

std::vector<std::vector<TemplateTile>> BoxFingerHoleTemplate() {
    return {
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
        {TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::MaybeSolid, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
    };
}

std::vector<std::vector<TemplateTile>> LurOneSubroomTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }
    PasteGroundSubroom(UVec2::New(5, 3), UVec2::New(5, 4), room);
    return room;
}

std::vector<std::vector<TemplateTile>> AnthillTemplate() {
    return {
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::MaybeSolid, TemplateTile::MaybeSolid, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid,
         TemplateTile::Air, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::MaybeSolid,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::MaybeSolid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Air},
        {TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Air},
        {TemplateTile::Solid, TemplateTile::MaybeSolid, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Air, TemplateTile::Air,
         TemplateTile::MaybeSolid, TemplateTile::Solid},
        {TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Air, TemplateTile::Air, TemplateTile::Solid, TemplateTile::Solid,
         TemplateTile::Solid, TemplateTile::Solid},
    };
}

std::vector<std::vector<TemplateTile>> ThreeCornerDropTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][1] = TemplateTile::MaybeSolid;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][2] = TemplateTile::Air;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][3] = TemplateTile::MaybeSolid;

    const UVec2 subroom_shape = UVec2::New(5, 3);
    PasteAirSubroom(subroom_shape, UVec2::New(0, 1), room);
    PasteAirSubroom(subroom_shape, UVec2::New(5, 1), room);
    PasteGroundSubroom(subroom_shape, UVec2::New(5, 4), room);
    return room;
}

std::vector<std::vector<TemplateTile>> TunnelAndSubroomTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }
    const std::size_t tunnel_height = static_cast<std::size_t>(Stage::kRoomShape.y - 3);
    room[tunnel_height][2] = TemplateTile::MaybeSolid;
    room[tunnel_height][3] = TemplateTile::MaybeSolid;
    room[tunnel_height][4] = TemplateTile::Solid;
    room[tunnel_height][5] = TemplateTile::Solid;
    room[tunnel_height][6] = TemplateTile::Solid;
    room[tunnel_height][7] = TemplateTile::Solid;
    room[tunnel_height + 1][7] = TemplateTile::Solid;
    room[tunnel_height + 1][8] = TemplateTile::Solid;
    room[tunnel_height + 2][6] = TemplateTile::Air;
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 1), room);
    return room;
}

std::vector<std::vector<TemplateTile>> DropWithTwoUpperSubroomsTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][1] = TemplateTile::MaybeSolid;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][2] = TemplateTile::Air;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][3] = TemplateTile::MaybeSolid;
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(0, 1), room);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(5, 1), room);
    return room;
}

std::vector<std::vector<TemplateTile>> OpenWideDropTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }
    const std::size_t crater_height = static_cast<std::size_t>(Stage::kRoomShape.y - 2);
    room[crater_height][1] = TemplateTile::MaybeSolid;
    room[crater_height][2] = TemplateTile::MaybeSolid;
    room[crater_height][7] = TemplateTile::MaybeSolid;
    room[crater_height][8] = TemplateTile::MaybeSolid;
    room[crater_height + 1][3] = TemplateTile::MaybeSolid;
    room[crater_height + 1][4] = TemplateTile::Air;
    room[crater_height + 1][5] = TemplateTile::Air;
    room[crater_height + 1][6] = TemplateTile::MaybeSolid;
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(0, 1), room);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(5, 1), room);
    return room;
}

std::vector<std::vector<TemplateTile>> CrestedDropWithUpperSubroomTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }
    const std::size_t crater_height = static_cast<std::size_t>(Stage::kRoomShape.y - 3);
    room[crater_height][2] = TemplateTile::MaybeSolid;
    room[crater_height][4] = TemplateTile::MaybeSolid;
    room[crater_height][5] = TemplateTile::MaybeSolid;
    room[crater_height][7] = TemplateTile::MaybeSolid;
    room[crater_height + 1][2] = TemplateTile::Solid;
    room[crater_height + 1][7] = TemplateTile::Solid;
    room[crater_height + 2][4] = TemplateTile::Air;
    room[crater_height + 2][5] = TemplateTile::Air;
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(2, 1), room);
    return room;
}

std::vector<std::vector<TemplateTile>> OpenFourSubroomTemplate() {
    auto room = BlankRoom();
    for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
        room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][static_cast<std::size_t>(x)] =
            TemplateTile::Solid;
    }
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][1] = TemplateTile::MaybeSolid;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][2] = TemplateTile::Air;
    room[static_cast<std::size_t>(Stage::kRoomShape.y - 1)][3] = TemplateTile::MaybeSolid;
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(0, 1), room);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(5, 1), room);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(0, 4), room);
    PasteAirSubroom(UVec2::New(5, 3), UVec2::New(5, 4), room);
    return room;
}

} // namespace

std::vector<std::vector<TemplateTile>> GetRoomTemplate(RoomType room_type) {
    switch (room_type) {
    case RoomType::Box: {
        const int choice = RandomIntInclusive(0, 6);
        if (choice == 0) {
            return BoxDigitEightTemplate();
        }
        if (choice == 1) {
            return BoxFallenFloorTemplate();
        }
        if (choice == 2) {
            return BoxOneSidedLadderTwoSubroomsTemplate();
        }
        if (choice == 3) {
            return BoxDoubleLadderTemplate();
        }
        if (choice == 4) {
            return BoxOneSidedLadderOneSubroomTemplate();
        }
        if (choice == 5) {
            return TwoLineOneSubroomTemplate();
        }
        return BoxFingerHoleTemplate();
    }
    case RoomType::FourWay:
        switch (RandomIntInclusive(0, 6)) {
        case 0:
            return OpenFourSubroomTemplate();
        case 1:
            return AnthillTemplate();
        case 2:
            return CrestedDropWithUpperSubroomTemplate();
        case 3:
            return TunnelAndSubroomTemplate();
        case 4:
            return OpenWideDropTemplate();
        case 5:
            return ThreeCornerDropTemplate();
        case 6:
            return DropWithTwoUpperSubroomsTemplate();
        default:
            return StandinFourWayTemplate();
        }
    case RoomType::LeftDownRight:
        switch (RandomIntInclusive(0, 2)) {
        case 0:
            return AnthillTemplate();
        case 1:
            return StandinLeftDownRightTemplate();
        case 2:
            return ThreeCornerDropTemplate();
        default:
            return StandinLeftDownRightTemplate();
        }
    case RoomType::LeftRight:
        return DoubleLadderTemplate();
    case RoomType::LeftUpRight:
        return StandinLeftUpRightTemplate();
    case RoomType::Exit:
        return RandomIntInclusive(0, 99) < 33 ? SidewaysEtExitTemplate() : TwoSubroomsAboveExitTemplate();
    case RoomType::Entrance:
        return StandinEntranceTemplate();
    }

    return cave::GetRoomTemplate(room_type);
}

} // namespace splonks::stage_gen::test
