#include "render_tiles_and_entities.hpp"

#include "entity.hpp"
#include "graphics.hpp"
#include "special_effects/special_effect.hpp"
#include "state.hpp"
#include "tile.hpp"

namespace splonks {

namespace {

constexpr float kRenderScale = 2.0F;

SDL_Color GetTileColor(Tile tile) {
    switch (tile) {
    case Tile::Air:
        return SDL_Color{38, 43, 68, 255};
    case Tile::Dirt:
        return SDL_Color{110, 78, 52, 255};
    case Tile::Gold:
        return SDL_Color{212, 175, 55, 255};
    case Tile::Block:
        return SDL_Color{120, 120, 120, 255};
    case Tile::LadderTop:
    case Tile::Ladder:
        return SDL_Color{200, 160, 80, 255};
    case Tile::Spikes:
        return SDL_Color{230, 230, 230, 255};
    case Tile::Rope:
        return SDL_Color{140, 90, 50, 255};
    case Tile::Entrance:
        return SDL_Color{80, 180, 255, 255};
    case Tile::Exit:
        return SDL_Color{180, 80, 255, 255};
    }
    return SDL_Color{255, 255, 255, 255};
}

} // namespace

void RenderStageTiles(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    (void)graphics;
    for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
        for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
            const Tile tile = state.stage.tiles[y][x];
            const SDL_Color color = GetTileColor(tile);
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            const SDL_FRect rect{
                static_cast<float>(x) * static_cast<float>(kTileSize) * kRenderScale,
                static_cast<float>(y) * static_cast<float>(kTileSize) * kRenderScale,
                static_cast<float>(kTileSize) * kRenderScale,
                static_cast<float>(kTileSize) * kRenderScale,
            };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

void RenderEntities(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    (void)graphics;
    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }

        Uint8 r = 220;
        Uint8 g = 220;
        Uint8 b = 220;
        if (entity.type_ == EntityType::Player) {
            r = 255;
            g = 80;
            b = 80;
        }

        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        const SDL_FRect rect{
            entity.pos.x * kRenderScale,
            entity.pos.y * kRenderScale,
            entity.size.x * kRenderScale,
            entity.size.y * kRenderScale,
        };
        SDL_RenderFillRect(renderer, &rect);
    }
}

void RenderSpecialEffects(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    (void)graphics;
    SDL_SetRenderDrawColor(renderer, 255, 200, 120, 255);
    for (const auto& special_effect : state.special_effects) {
        const Vec2 pos = special_effect->GetPos();
        const Vec2 size = special_effect->GetSize();
        const SDL_FRect rect{
            (pos.x - (size.x * 0.5F)) * kRenderScale,
            (pos.y - (size.y * 0.5F)) * kRenderScale,
            size.x * kRenderScale,
            size.y * kRenderScale,
        };
        SDL_RenderRect(renderer, &rect);
    }
}

} // namespace splonks
