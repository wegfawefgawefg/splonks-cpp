#include "render_tiles_and_entities.hpp"

#include "entity.hpp"
#include "graphics.hpp"
#include "special_effects/special_effect.hpp"
#include "state.hpp"
#include "tile.hpp"

#include <cmath>
#include <cstdint>

namespace splonks {

namespace {

std::uint64_t TileVariationCacheKey(const IVec2& tile_pos) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(tile_pos.x)) << 32U) |
           static_cast<std::uint32_t>(tile_pos.y);
}

Vec2 WorldToScreen(const Graphics& graphics, const Vec2& world_pos) {
    return ((world_pos - graphics.camera.target) * graphics.camera.zoom) + graphics.camera.offset;
}

SDL_FRect WorldRectToScreen(const Graphics& graphics, const Vec2& world_pos, const Vec2& world_size) {
    const Vec2 screen_pos = WorldToScreen(graphics, world_pos);
    return SDL_FRect{
        screen_pos.x,
        screen_pos.y,
        world_size.x * graphics.camera.zoom,
        world_size.y * graphics.camera.zoom,
    };
}

TileSet WhichTileSet(StageType stage_type) {
    switch (stage_type) {
    case StageType::Ice1:
    case StageType::Ice2:
    case StageType::Ice3:
        return TileSet::Ice;
    case StageType::Desert1:
    case StageType::Desert2:
    case StageType::Desert3:
        return TileSet::Jungle;
    case StageType::Temple1:
    case StageType::Temple2:
    case StageType::Temple3:
        return TileSet::Temple;
    case StageType::Boss:
        return TileSet::Boss;
    case StageType::Blank:
    case StageType::Test1:
    case StageType::Cave1:
    case StageType::Cave2:
    case StageType::Cave3:
        return TileSet::Cave;
    }
    return TileSet::Cave;
}

UVec2 GetTileTextureSamplePositionWithVariation(Graphics& graphics, Tile tile, const IVec2& tile_pos) {
    UVec2 sample_pos = GetTileTextureSamplePosition(tile);
    const std::uint32_t num_variations = TileNumVariations(tile);
    if (num_variations > 1U) {
        const std::uint64_t key = TileVariationCacheKey(tile_pos);
        std::uint32_t coordinate_variation = 0;
        const auto found = graphics.tile_variations_cache.find(key);
        if (found != graphics.tile_variations_cache.end()) {
            coordinate_variation = found->second;
        } else {
            const std::uint32_t seed =
                static_cast<std::uint32_t>(static_cast<std::uint32_t>(tile_pos.x) * 73856093U) ^
                static_cast<std::uint32_t>(static_cast<std::uint32_t>(tile_pos.y) * 19349663U);
            coordinate_variation = seed % num_variations;
            graphics.tile_variations_cache.insert({key, coordinate_variation});
        }
        sample_pos.x += coordinate_variation * kTileSize;
    }
    return sample_pos;
}

} // namespace

void RenderStageTiles(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    SDL_Texture* tile_set_texture = graphics.GetTileSetTexture(WhichTileSet(state.stage.stage_type));
    if (tile_set_texture == nullptr) {
        return;
    }
    for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
        for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
            const Tile tile = state.stage.tiles[y][x];
            const IVec2 tile_pos = IVec2::New(
                static_cast<int>(x * kTileSize),
                static_cast<int>(y * kTileSize)
            );
            const UVec2 sample_pos = GetTileTextureSamplePositionWithVariation(graphics, tile, tile_pos);
            const SDL_FRect src{
                static_cast<float>(sample_pos.x),
                static_cast<float>(sample_pos.y),
                static_cast<float>(kTileSize),
                static_cast<float>(kTileSize),
            };
            const SDL_FRect dst = WorldRectToScreen(
                graphics,
                ToVec2(tile_pos),
                Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
            );

            if (IsTileTransparent(tile)) {
                const UVec2 air_sample_pos =
                    GetTileTextureSamplePositionWithVariation(graphics, Tile::Air, tile_pos);
                const SDL_FRect air_src{
                    static_cast<float>(air_sample_pos.x),
                    static_cast<float>(air_sample_pos.y),
                    static_cast<float>(kTileSize),
                    static_cast<float>(kTileSize),
                };
                SDL_RenderTexture(renderer, tile_set_texture, &air_src, &dst);
            }

            SDL_RenderTexture(renderer, tile_set_texture, &src, &dst);
        }
    }
}

void RenderStageTileWrapper(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    SDL_Texture* tile_set_texture = graphics.GetTileSetTexture(WhichTileSet(state.stage.stage_type));
    if (tile_set_texture == nullptr) {
        return;
    }

    const Vec2 visible_tl_wc = graphics.camera.target - (graphics.camera.offset / graphics.camera.zoom);
    const Vec2 visible_br_wc =
        graphics.camera.target +
        ((ToVec2(graphics.dims) - graphics.camera.offset) / graphics.camera.zoom);

    const int visible_tl_tile_x =
        static_cast<int>(std::floor(visible_tl_wc.x / static_cast<float>(kTileSize))) - 1;
    const int visible_tl_tile_y =
        static_cast<int>(std::floor(visible_tl_wc.y / static_cast<float>(kTileSize))) - 1;
    const int visible_br_tile_x =
        static_cast<int>(std::ceil(visible_br_wc.x / static_cast<float>(kTileSize))) + 1;
    const int visible_br_tile_y =
        static_cast<int>(std::ceil(visible_br_wc.y / static_cast<float>(kTileSize))) + 1;

    const int stage_tile_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_tile_height = static_cast<int>(state.stage.GetTileHeight());

    for (int tile_y = visible_tl_tile_y; tile_y <= visible_br_tile_y; ++tile_y) {
        for (int tile_x = visible_tl_tile_x; tile_x <= visible_br_tile_x; ++tile_x) {
            const bool inside_stage = tile_x >= 0 && tile_y >= 0 && tile_x < stage_tile_width &&
                                      tile_y < stage_tile_height;
            if (!inside_stage) {
                const IVec2 tile_pos = IVec2::New(
                    tile_x * static_cast<int>(kTileSize),
                    tile_y * static_cast<int>(kTileSize)
                );
                const UVec2 sample_pos =
                    GetTileTextureSamplePositionWithVariation(graphics, Tile::Dirt, tile_pos);
                const SDL_FRect src{
                    static_cast<float>(sample_pos.x),
                    static_cast<float>(sample_pos.y),
                    static_cast<float>(kTileSize),
                    static_cast<float>(kTileSize),
                };
                const SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos),
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );
                SDL_RenderTexture(renderer, tile_set_texture, &src, &dst);
            }
        }
    }
}

void RenderEntities(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    std::vector<std::size_t> draw_queue;
    std::vector<std::size_t> next_draw_queue;
    next_draw_queue.reserve(state.entity_manager.entities.size());
    for (std::size_t i = 0; i < state.entity_manager.entities.size(); ++i) {
        next_draw_queue.push_back(i);
    }

    for (DrawLayer layer : {DrawLayer::Background, DrawLayer::Middle, DrawLayer::Foreground}) {
        draw_queue.clear();
        draw_queue.insert(draw_queue.end(), next_draw_queue.begin(), next_draw_queue.end());
        next_draw_queue.clear();
        for (std::size_t entity_id : draw_queue) {
            const Entity& entity = state.entity_manager.entities[entity_id];
            if (!entity.active) {
                continue;
            }
            if (entity.draw_layer != layer) {
                next_draw_queue.push_back(entity_id);
                continue;
            }

            if (entity.type_ == EntityType::Block) {
                const UVec2 sample_pos = GetTileTextureSamplePosition(Tile::Block);
                const SDL_FRect src{
                    static_cast<float>(sample_pos.x),
                    static_cast<float>(sample_pos.y),
                    static_cast<float>(kTileSize),
                    static_cast<float>(kTileSize),
                };
                const SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    entity.pos,
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );
                SDL_RenderTexture(
                    renderer,
                    graphics.GetTileSetTexture(WhichTileSet(state.stage.stage_type)),
                    &src,
                    &dst
                );
                continue;
            }

            const SpriteData& sprite_data = graphics.GetSpriteData(entity.sprite_animator.sprite);
            SDL_Texture* sprite_texture = graphics.GetSpriteTexture(entity.sprite_animator.sprite);
            if (sprite_texture == nullptr || sprite_data.frames.empty()) {
                continue;
            }
            const Frame& frame = sprite_data.frames[entity.sprite_animator.current_frame];
            const bool sprite_uses_fallback = graphics.SpriteUsesFallback(entity.sprite_animator.sprite);
            const Vec2 sprite_world_size = sprite_uses_fallback ? entity.size : ToVec2(sprite_data.size);
            const Vec2 sprite_scaled_size = sprite_world_size * entity.sprite_animator.scale;
            Vec2 render_position = entity.pos;
            switch (entity.origin) {
            case Origin::Center:
                render_position = entity.pos - (sprite_scaled_size / 2.0F);
                break;
            case Origin::TopLeft:
                render_position = entity.pos;
                break;
            case Origin::Foot:
                render_position = Vec2::New(
                    entity.pos.x - (sprite_scaled_size.x / 2.0F),
                    entity.pos.y - sprite_scaled_size.y
                );
                break;
            }

            SDL_FRect src{
                static_cast<float>(frame.sample_position.x),
                static_cast<float>(frame.sample_position.y),
                static_cast<float>(sprite_data.size.x),
                static_cast<float>(sprite_data.size.y),
            };
            SDL_FRect dst = WorldRectToScreen(
                graphics,
                render_position,
                Vec2::New(
                    sprite_scaled_size.x,
                    sprite_scaled_size.y
                )
            );
            const SDL_FlipMode flip =
                entity.facing == LeftOrRight::Right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
            SDL_RenderTextureRotated(
                renderer,
                sprite_texture,
                &src,
                &dst,
                0.0,
                nullptr,
                flip
            );
        }
    }
}

void RenderSpecialEffects(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    if (graphics.special_effects_texture == nullptr) {
        return;
    }
    for (const auto& special_effect : state.special_effects) {
        const Vec2 pos = special_effect->GetPos();
        const Vec2 size = special_effect->GetSize();
        const float rotation = special_effect->GetRot();
        const SampleRegion& sample_region = special_effect->GetSampleRegion();
        const float alpha = special_effect->GetAlpha();
        const Vec2 half_size = size / 2.0F;
        SDL_SetTextureAlphaMod(
            graphics.special_effects_texture,
            static_cast<Uint8>(alpha * 255.0F)
        );
        const SDL_FRect src{
            static_cast<float>(sample_region.pos.x),
                static_cast<float>(sample_region.pos.y),
                static_cast<float>(sample_region.size.x),
                static_cast<float>(sample_region.size.y),
        };
        const SDL_FRect dst = WorldRectToScreen(graphics, pos - half_size, size);
        const SDL_FPoint center{dst.w / 2.0F, dst.h / 2.0F};
        SDL_RenderTextureRotated(
            renderer,
            graphics.special_effects_texture,
            &src,
            &dst,
            rotation,
            &center,
            SDL_FLIP_NONE
        );
    }
}

} // namespace splonks
