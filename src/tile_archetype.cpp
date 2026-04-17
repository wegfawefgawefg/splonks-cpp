#include "tile_archetype.hpp"

#include "entity/archetype.hpp"
#include "state.hpp"

#include <array>
#include <stdexcept>

namespace splonks {

namespace {

constexpr std::size_t TileIndex(Tile tile) {
    return static_cast<std::size_t>(tile);
}

void SpawnEntityAtCenter(EntityType type_, const Vec2& center, State& state) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    SetEntityAs(*entity, type_);
    entity->SetCenter(center);
    entity->vel = Vec2::New(0.0F, 0.0F);
}

void OnBreakAsGoldVein(const IVec2& tile_pos, State& state, Audio& audio) {
    (void)audio;
    const Vec2 center = Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
    );
    SpawnEntityAtCenter(EntityType::GoldChunk, center + Vec2::New(-3.0F, -1.0F), state);
    SpawnEntityAtCenter(EntityType::GoldChunk, center + Vec2::New(0.0F, 0.0F), state);
    SpawnEntityAtCenter(EntityType::GoldChunk, center + Vec2::New(3.0F, 1.0F), state);
}

void OnBreakAsBigGoldVein(const IVec2& tile_pos, State& state, Audio& audio) {
    (void)audio;
    const Vec2 center = Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
    );
    SpawnEntityAtCenter(EntityType::GoldChunk, center + Vec2::New(-4.0F, -1.0F), state);
    SpawnEntityAtCenter(EntityType::GoldChunk, center + Vec2::New(0.0F, 1.0F), state);
    SpawnEntityAtCenter(EntityType::GoldChunk, center + Vec2::New(4.0F, -1.0F), state);
    SpawnEntityAtCenter(EntityType::GoldNugget, center, state);
}

TileArchetype MakeSolidTileArchetype(
    Tile tile,
    TileFamily family,
    FrameDataId break_animation,
    const char* debug_name,
    std::optional<SoundEffect> break_sound = SoundEffect::Thud,
    TileOnBreak on_break = nullptr
) {
    return TileArchetype{
        .tile = tile,
        .solid = true,
        .climbable = false,
        .transparent = false,
        .hangable = true,
        .family = family,
        .collide_sound = SoundEffect::Thud,
        .break_sound = break_sound,
        .break_animation = break_animation,
        .on_break = on_break,
        .debug_name = debug_name,
    };
}

static_assert(TileIndex(Tile::Exit) + 1 <= kTileCount);

const std::array<TileArchetype, kTileCount> kTileArchetypes{{
    TileArchetype{
        .tile = Tile::Air,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Air",
    },
    TileArchetype{
        .tile = Tile::CaveAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Cave,
        .debug_name = "CaveAir0",
    },
    TileArchetype{
        .tile = Tile::CaveAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Cave,
        .debug_name = "CaveAir1",
    },
    TileArchetype{
        .tile = Tile::CaveAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Cave,
        .debug_name = "CaveAir2",
    },
    MakeSolidTileArchetype(Tile::CaveDirt, TileFamily::Cave, HashFrameDataIdConstexpr("cave_dirt_0"), "CaveDirt"),
    MakeSolidTileArchetype(
        Tile::CaveGold,
        TileFamily::Cave,
        HashFrameDataIdConstexpr("cave_gold_0"),
        "CaveGold",
        SoundEffect::MoneySmashed,
        OnBreakAsGoldVein
    ),
    MakeSolidTileArchetype(
        Tile::CaveGoldBig,
        TileFamily::Cave,
        HashFrameDataIdConstexpr("cave_gold_1"),
        "CaveGoldBig",
        SoundEffect::MoneySmashed,
        OnBreakAsBigGoldVein
    ),
    MakeSolidTileArchetype(Tile::CaveBlock, TileFamily::Cave, HashFrameDataIdConstexpr("cave_block_0"), "CaveBlock"),
    MakeSolidTileArchetype(Tile::CaveShopWall, TileFamily::Cave, HashFrameDataIdConstexpr("cave_shop_wall"), "CaveShopWall"),
    MakeSolidTileArchetype(Tile::CaveSmoothWall, TileFamily::Cave, HashFrameDataIdConstexpr("cave_smooth_wall"), "CaveSmoothWall"),
    MakeSolidTileArchetype(Tile::Glass, TileFamily::Neutral, HashFrameDataIdConstexpr("glass"), "Glass"),
    MakeSolidTileArchetype(Tile::LawsonWall, TileFamily::Cave, HashFrameDataIdConstexpr("lawson_wall"), "LawsonWall"),
    TileArchetype{
        .tile = Tile::LawsonInside,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Cave,
        .debug_name = "LawsonInside",
    },
    MakeSolidTileArchetype(Tile::LawsonLeftTopper, TileFamily::Cave, HashFrameDataIdConstexpr("lawson_left_topper"), "LawsonLeftTopper"),
    MakeSolidTileArchetype(Tile::LawsonMiddleTopper, TileFamily::Cave, HashFrameDataIdConstexpr("lawson_middle_topper"), "LawsonMiddleTopper"),
    MakeSolidTileArchetype(Tile::LawsonRightTopper, TileFamily::Cave, HashFrameDataIdConstexpr("lawson_right_topper"), "LawsonRightTopper"),
    MakeSolidTileArchetype(Tile::LawsonFloor, TileFamily::Cave, HashFrameDataIdConstexpr("lawson_floor"), "LawsonFloor"),
    TileArchetype{
        .tile = Tile::IceAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Ice,
        .debug_name = "IceAir0",
    },
    TileArchetype{
        .tile = Tile::IceAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Ice,
        .debug_name = "IceAir1",
    },
    TileArchetype{
        .tile = Tile::IceAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Ice,
        .debug_name = "IceAir2",
    },
    MakeSolidTileArchetype(Tile::IceDirt, TileFamily::Ice, HashFrameDataIdConstexpr("ice_dirt_0"), "IceDirt"),
    MakeSolidTileArchetype(
        Tile::IceGold,
        TileFamily::Ice,
        HashFrameDataIdConstexpr("ice_gold"),
        "IceGold",
        SoundEffect::MoneySmashed,
        OnBreakAsGoldVein
    ),
    MakeSolidTileArchetype(
        Tile::IceGoldBig,
        TileFamily::Ice,
        HashFrameDataIdConstexpr("ice_gold"),
        "IceGoldBig",
        SoundEffect::MoneySmashed,
        OnBreakAsBigGoldVein
    ),
    MakeSolidTileArchetype(Tile::IceBlock, TileFamily::Ice, HashFrameDataIdConstexpr("ice_block_0"), "IceBlock"),
    TileArchetype{
        .tile = Tile::JungleAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Jungle,
        .debug_name = "JungleAir0",
    },
    TileArchetype{
        .tile = Tile::JungleAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Jungle,
        .debug_name = "JungleAir1",
    },
    TileArchetype{
        .tile = Tile::JungleAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Jungle,
        .debug_name = "JungleAir2",
    },
    MakeSolidTileArchetype(Tile::JungleDirt, TileFamily::Jungle, HashFrameDataIdConstexpr("jungle_dirt_0"), "JungleDirt"),
    MakeSolidTileArchetype(
        Tile::JungleGold,
        TileFamily::Jungle,
        HashFrameDataIdConstexpr("jungle_gold_0"),
        "JungleGold",
        SoundEffect::MoneySmashed,
        OnBreakAsGoldVein
    ),
    MakeSolidTileArchetype(
        Tile::JungleGoldBig,
        TileFamily::Jungle,
        HashFrameDataIdConstexpr("jungle_gold_0"),
        "JungleGoldBig",
        SoundEffect::MoneySmashed,
        OnBreakAsBigGoldVein
    ),
    MakeSolidTileArchetype(Tile::JungleBlock, TileFamily::Jungle, HashFrameDataIdConstexpr("jungle_block_0"), "JungleBlock"),
    TileArchetype{
        .tile = Tile::TempleAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Temple,
        .debug_name = "TempleAir0",
    },
    TileArchetype{
        .tile = Tile::TempleAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Temple,
        .debug_name = "TempleAir1",
    },
    TileArchetype{
        .tile = Tile::TempleAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Temple,
        .debug_name = "TempleAir2",
    },
    MakeSolidTileArchetype(Tile::TempleDirt, TileFamily::Temple, HashFrameDataIdConstexpr("temple_dirt_0"), "TempleDirt"),
    MakeSolidTileArchetype(
        Tile::TempleGold,
        TileFamily::Temple,
        HashFrameDataIdConstexpr("temple_gold"),
        "TempleGold",
        SoundEffect::MoneySmashed,
        OnBreakAsGoldVein
    ),
    MakeSolidTileArchetype(
        Tile::TempleGoldBig,
        TileFamily::Temple,
        HashFrameDataIdConstexpr("temple_gold"),
        "TempleGoldBig",
        SoundEffect::MoneySmashed,
        OnBreakAsBigGoldVein
    ),
    MakeSolidTileArchetype(Tile::TempleBlock, TileFamily::Temple, HashFrameDataIdConstexpr("temple_block_0"), "TempleBlock"),
    TileArchetype{
        .tile = Tile::BossAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Boss,
        .debug_name = "BossAir0",
    },
    TileArchetype{
        .tile = Tile::BossAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Boss,
        .debug_name = "BossAir1",
    },
    TileArchetype{
        .tile = Tile::BossAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Boss,
        .debug_name = "BossAir2",
    },
    MakeSolidTileArchetype(Tile::BossDirt, TileFamily::Boss, HashFrameDataIdConstexpr("boss_dirt_0"), "BossDirt"),
    MakeSolidTileArchetype(
        Tile::BossGold,
        TileFamily::Boss,
        HashFrameDataIdConstexpr("boss_gold"),
        "BossGold",
        SoundEffect::MoneySmashed,
        OnBreakAsGoldVein
    ),
    MakeSolidTileArchetype(
        Tile::BossGoldBig,
        TileFamily::Boss,
        HashFrameDataIdConstexpr("boss_gold"),
        "BossGoldBig",
        SoundEffect::MoneySmashed,
        OnBreakAsBigGoldVein
    ),
    MakeSolidTileArchetype(Tile::BossBlock, TileFamily::Boss, HashFrameDataIdConstexpr("boss_block_0"), "BossBlock"),
    TileArchetype{
        .tile = Tile::LadderTop,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .debug_name = "LadderTop",
    },
    TileArchetype{
        .tile = Tile::Ladder,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .debug_name = "Ladder",
    },
    TileArchetype{
        .tile = Tile::Spikes,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Spikes",
    },
    TileArchetype{
        .tile = Tile::Rope,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .debug_name = "Rope",
    },
    TileArchetype{
        .tile = Tile::Entrance,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Entrance",
    },
    TileArchetype{
        .tile = Tile::Exit,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Exit",
    },
}};

} // namespace

const TileArchetype& GetTileArchetype(Tile tile) {
    const std::size_t index = TileIndex(tile);
    if (index >= kTileCount) {
        throw std::runtime_error("GetTileArchetype received invalid tile");
    }
    return kTileArchetypes[index];
}

bool IsTileTransparent(Tile tile) {
    return GetTileArchetype(tile).transparent;
}

bool IsTileCollidable(Tile tile) {
    return GetTileArchetype(tile).solid;
}

bool IsTileHangable(Tile tile) {
    return GetTileArchetype(tile).hangable;
}

} // namespace splonks
