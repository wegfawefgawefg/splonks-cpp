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

static_assert(TileIndex(Tile::Exit) + 1 == kTileCount);

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
        .tile = Tile::Dirt,
        .solid = true,
        .climbable = false,
        .transparent = false,
        .hangable = true,
        .collide_sound = SoundEffect::Thud,
        .break_sound = SoundEffect::Thud,
        .break_animation = frame_data_ids::LittleBrownShard,
        .debug_name = "Dirt",
    },
    TileArchetype{
        .tile = Tile::Gold,
        .solid = true,
        .climbable = false,
        .transparent = false,
        .hangable = true,
        .collide_sound = SoundEffect::Thud,
        .break_sound = SoundEffect::MoneySmashed,
        .break_animation = frame_data_ids::LittleBrownShard,
        .on_break = OnBreakAsGoldVein,
        .debug_name = "Gold",
    },
    TileArchetype{
        .tile = Tile::GoldBig,
        .solid = true,
        .climbable = false,
        .transparent = false,
        .hangable = true,
        .collide_sound = SoundEffect::Thud,
        .break_sound = SoundEffect::MoneySmashed,
        .break_animation = frame_data_ids::LittleBrownShard,
        .on_break = OnBreakAsBigGoldVein,
        .debug_name = "GoldBig",
    },
    TileArchetype{
        .tile = Tile::Block,
        .solid = true,
        .climbable = false,
        .transparent = false,
        .hangable = true,
        .collide_sound = SoundEffect::Thud,
        .break_sound = SoundEffect::Thud,
        .break_animation = frame_data_ids::LittleBrownShard,
        .debug_name = "Block",
    },
    TileArchetype{
        .tile = Tile::ShopWall,
        .solid = true,
        .climbable = false,
        .transparent = false,
        .hangable = true,
        .collide_sound = SoundEffect::Thud,
        .break_sound = SoundEffect::Thud,
        .break_animation = frame_data_ids::LittleBrownShard,
        .debug_name = "ShopWall",
    },
    TileArchetype{
        .tile = Tile::SmoothWall,
        .solid = true,
        .climbable = false,
        .transparent = false,
        .hangable = true,
        .collide_sound = SoundEffect::Thud,
        .break_sound = SoundEffect::Thud,
        .break_animation = frame_data_ids::LittleBrownShard,
        .debug_name = "SmoothWall",
    },
    TileArchetype{
        .tile = Tile::Glass,
        .solid = true,
        .climbable = false,
        .transparent = false,
        .hangable = true,
        .collide_sound = SoundEffect::Thud,
        .break_sound = SoundEffect::Thud,
        .break_animation = frame_data_ids::LittleBrownShard,
        .debug_name = "Glass",
    },
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
