#include "render/tiles_and_entities.hpp"

#include "entity/archetype.hpp"
#include "entity.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "render/stone_overlay.hpp"
#include "render/tile_lighting.hpp"
#include "render/world_texture.hpp"
#include "particles/particle_archetypes.hpp"
#include "state.hpp"
#include "stage_lighting.hpp"
#include "tile.hpp"
#include "tile_archetype.hpp"
#include "world_query.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace splonks {

namespace {

struct VisibleWorldRect {
    Vec2 tl;
    Vec2 br;
};

int WrapCoordinate(int value, int size) {
    if (size <= 0) {
        return 0;
    }
    int wrapped = value % size;
    if (wrapped < 0) {
        wrapped += size;
    }
    return wrapped;
}

VisibleWorldRect GetVisibleWorldRect(const Graphics& graphics) {
    return VisibleWorldRect{
        .tl = graphics.camera.target - (graphics.camera.offset / graphics.camera.zoom),
        .br = graphics.camera.target +
              ((ToVec2(graphics.dims) - graphics.camera.offset) / graphics.camera.zoom),
    };
}

bool IsImmediateBorderRingTile(const Stage& stage, int tile_x, int tile_y) {
    const int width = static_cast<int>(stage.GetTileWidth());
    const int height = static_cast<int>(stage.GetTileHeight());
    if (width <= 0 || height <= 0) {
        return false;
    }

    const bool immediate_x = !stage.WrapsX() && (tile_x == -1 || tile_x == width);
    const bool immediate_y = !stage.WrapsY() && (tile_y == -1 || tile_y == height);
    const bool inside_x = tile_x >= 0 && tile_x < width;
    const bool inside_y = tile_y >= 0 && tile_y < height;
    return (immediate_x && (inside_y || immediate_y)) || (immediate_y && (inside_x || immediate_x));
}

float GetBorderTileShake(const Stage& stage, int tile_x, int tile_y) {
    const int width = static_cast<int>(stage.GetTileWidth());
    const int height = static_cast<int>(stage.GetTileHeight());
    if (width <= 0 || height <= 0) {
        return 0.0F;
    }
    if (!IsImmediateBorderRingTile(stage, tile_x, tile_y)) {
        return 0.0F;
    }

    int resolved_x = tile_x;
    int resolved_y = tile_y;
    if (stage.WrapsX()) {
        resolved_x = WrapCoordinate(resolved_x, width);
    } else {
        resolved_x = std::clamp(resolved_x, 0, width - 1);
    }
    if (stage.WrapsY()) {
        resolved_y = WrapCoordinate(resolved_y, height);
    } else {
        resolved_y = std::clamp(resolved_y, 0, height - 1);
    }

    return stage.GetForegroundTileShake(
        static_cast<unsigned int>(resolved_x),
        static_cast<unsigned int>(resolved_y)
    );
}

bool ShouldRenderImmediateBorderBacking(const Stage& stage, int tile_x, int tile_y) {
    return IsImmediateBorderRingTile(stage, tile_x, tile_y);
}

int FloorDivByFloat(float value, float divisor) {
    if (divisor <= 0.0F) {
        return 0;
    }
    return static_cast<int>(std::floor(value / divisor));
}

std::vector<Vec2> GetVisibleWrappedRenderOffsets(const Stage& stage, const Graphics& graphics) {
    std::vector<Vec2> offsets;
    offsets.push_back(Vec2::New(0.0F, 0.0F));

    const float stage_width = static_cast<float>(stage.GetWidth());
    const float stage_height = static_cast<float>(stage.GetHeight());
    if ((!stage.WrapsX() || stage_width <= 0.0F) && (!stage.WrapsY() || stage_height <= 0.0F)) {
        return offsets;
    }

    const VisibleWorldRect visible = GetVisibleWorldRect(graphics);
    const int min_copy_x = stage.WrapsX() ? FloorDivByFloat(visible.tl.x, stage_width) : 0;
    const int max_copy_x = stage.WrapsX() ? FloorDivByFloat(visible.br.x, stage_width) : 0;
    const int min_copy_y = stage.WrapsY() ? FloorDivByFloat(visible.tl.y, stage_height) : 0;
    const int max_copy_y = stage.WrapsY() ? FloorDivByFloat(visible.br.y, stage_height) : 0;

    offsets.clear();
    for (int copy_y = min_copy_y; copy_y <= max_copy_y; ++copy_y) {
        for (int copy_x = min_copy_x; copy_x <= max_copy_x; ++copy_x) {
            offsets.push_back(Vec2::New(
                static_cast<float>(copy_x) * stage_width,
                static_cast<float>(copy_y) * stage_height
            ));
        }
    }
    return offsets;
}

Vec2 GetShakeOffset(float shake_pixels) {
    if (shake_pixels <= 0.0F) {
        return Vec2::New(0.0F, 0.0F);
    }

    return Vec2::New(
        rng::RandomFloat(-shake_pixels, shake_pixels),
        rng::RandomFloat(-shake_pixels, shake_pixels)
    );
}

bool ShouldRenderBackgroundStamp(const State& state, const BackgroundStamp& stamp) {
    switch (stamp.condition) {
    case BackgroundStampCondition::None:
        return true;
    case BackgroundStampCondition::Wanted:
        for (const Entity& entity : state.entity_manager.entities) {
            if (entity.active && entity.wanted) {
                return true;
            }
        }
        return false;
    }

    return true;
}

bool ShouldRevealEmbeddedTreasure(const State& state) {
    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }
        if (CanRevealEmbeddedTreasure(entity)) {
            return true;
        }
    }
    return false;
}

const FrameData* GetFirstFrameForAnimationOrFallback(
    const Graphics& graphics,
    FrameDataId animation_id
) {
    const FrameDataAnimation* animation = graphics.frame_data_db.FindAnimation(animation_id);
    if (animation == nullptr || animation->frame_indices.empty()) {
        animation = graphics.frame_data_db.FindAnimation(frame_data_ids::NoSprite);
        if (animation == nullptr || animation->frame_indices.empty()) {
            return nullptr;
        }
    }
    return &graphics.frame_data_db.frames[animation->frame_indices[0]];
}

const FrameData* GetFirstFrameForAnimation(
    const Graphics& graphics,
    FrameDataId animation_id
) {
    const FrameDataAnimation* animation = graphics.frame_data_db.FindAnimation(animation_id);
    if (animation == nullptr || animation->frame_indices.empty()) {
        return nullptr;
    }
    return &graphics.frame_data_db.frames[animation->frame_indices[0]];
}

Tile GetBackwallFillTileForTileCoord(const Stage& stage, int tile_x, int tile_y) {
    if (stage.backwall_fill_tiles.empty()) {
        return Tile::Air;
    }

    const std::uint32_t seed =
        (static_cast<std::uint32_t>(tile_x) * 73856093U) ^
        (static_cast<std::uint32_t>(tile_y) * 19349663U);
    return stage.backwall_fill_tiles[seed % stage.backwall_fill_tiles.size()];
}

} // namespace

void RenderStageTiles(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureStageLighting(state);
    state.stage.SyncTileShakeGrid();
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const Vec2& render_offset : render_offsets) {
        for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
            for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
                const Tile backwall_tile = state.stage.backwall_tiles[y][x];
                if (backwall_tile == Tile::Air) {
                    continue;
                }

                const IVec2 tile_pos = IVec2::New(
                    static_cast<int>(x * kTileSize),
                    static_cast<int>(y * kTileSize)
                );
                const float background_shake = state.stage.GetBackgroundTileShake(
                    static_cast<unsigned int>(x),
                    static_cast<unsigned int>(y)
                );
                const Vec2 background_shake_offset = GetShakeOffset(background_shake);
                const SDL_FRect unshaken_dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos) + render_offset,
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );
                const SDL_FRect background_dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos) + render_offset + background_shake_offset,
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );

                const TileSourceData* const backwall_source_data =
                    GetTileSourceData(graphics, backwall_tile, tile_pos);
                if (backwall_source_data == nullptr) {
                    continue;
                }
                SDL_Texture* const backwall_texture = GetTileTexture(graphics, *backwall_source_data);
                if (backwall_texture == nullptr) {
                    continue;
                }
                const SDL_FRect backwall_src{
                    static_cast<float>(backwall_source_data->sample_rect.x),
                    static_cast<float>(backwall_source_data->sample_rect.y),
                    static_cast<float>(backwall_source_data->sample_rect.w),
                    static_cast<float>(backwall_source_data->sample_rect.h),
                };
                ApplyBackwallTileBrightness(
                    backwall_texture,
                    state,
                    graphics,
                    static_cast<int>(x),
                    static_cast<int>(y)
                );
                if (background_shake > 0.0F) {
                    RenderWorldTexture(renderer, graphics, backwall_texture, &backwall_src, unshaken_dst);
                }
                RenderWorldTexture(renderer, graphics, backwall_texture, &backwall_src, background_dst);
                ResetTerrainTileBrightness(backwall_texture);
            }
        }

        for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
            for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
                const Tile tile = state.stage.tiles[y][x];
                if (tile == Tile::Air) {
                    continue;
                }

                const IVec2 tile_pos = IVec2::New(
                    static_cast<int>(x * kTileSize),
                    static_cast<int>(y * kTileSize)
                );
                const float foreground_shake = state.stage.GetForegroundTileShake(
                    static_cast<unsigned int>(x),
                    static_cast<unsigned int>(y)
                );
                const Vec2 foreground_shake_offset = GetShakeOffset(foreground_shake);
                const SDL_FRect foreground_dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos) + render_offset + foreground_shake_offset,
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );

                const TileSourceData* const tile_source_data =
                    GetTileSourceDataForStage(graphics, state.stage, tile, tile_pos);
                if (tile_source_data == nullptr) {
                    continue;
                }
                SDL_Texture* const tile_texture = GetTileTexture(graphics, *tile_source_data);
                if (tile_texture == nullptr) {
                    continue;
                }
                const SDL_FRect src{
                    static_cast<float>(tile_source_data->sample_rect.x),
                    static_cast<float>(tile_source_data->sample_rect.y),
                    static_cast<float>(tile_source_data->sample_rect.w),
                    static_cast<float>(tile_source_data->sample_rect.h),
                };

                ApplyTerrainTileBrightness(
                    tile_texture,
                    state,
                    graphics,
                    static_cast<int>(x),
                    static_cast<int>(y)
                );
                const TileRotation tile_rotation = state.stage.GetTileRotation(
                    static_cast<unsigned int>(x),
                    static_cast<unsigned int>(y)
                );
                const SDL_FPoint tile_center{
                    foreground_dst.w * 0.5F,
                    foreground_dst.h * 0.5F,
                };
                RenderWorldTextureRotated(
                    renderer,
                    graphics,
                    tile_texture,
                    &src,
                    foreground_dst,
                    static_cast<double>(tile_rotation) * 90.0,
                    &tile_center,
                    SDL_FLIP_NONE
                );
                ResetTerrainTileBrightness(tile_texture);
                RenderTerrainTileLighting(
                    renderer,
                    state,
                    graphics,
                    static_cast<int>(x),
                    static_cast<int>(y),
                    foreground_dst
                );
            }
        }
    }
}

void RenderStageFluids(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureStageLighting(state);
    state.stage.SyncTileInstanceMetadataGrid();
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const Vec2& render_offset : render_offsets) {
        for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
            for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
                const Tile fluid_tile = state.stage.GetFluidTile(
                    static_cast<unsigned int>(x),
                    static_cast<unsigned int>(y)
                );
                if (!GetTileArchetype(fluid_tile).simulated_fluid) {
                    continue;
                }

                const IVec2 tile_pos = IVec2::New(
                    static_cast<int>(x * kTileSize),
                    static_cast<int>(y * kTileSize)
                );
                const SDL_FRect fluid_dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos) + render_offset,
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );

                const TileSourceData* const tile_source_data =
                    GetTileSourceDataForStage(graphics, state.stage, fluid_tile, tile_pos);
                if (tile_source_data == nullptr) {
                    continue;
                }
                SDL_Texture* const tile_texture = GetTileTexture(graphics, *tile_source_data);
                if (tile_texture == nullptr) {
                    continue;
                }
                const SDL_FRect src{
                    static_cast<float>(tile_source_data->sample_rect.x),
                    static_cast<float>(tile_source_data->sample_rect.y),
                    static_cast<float>(tile_source_data->sample_rect.w),
                    static_cast<float>(tile_source_data->sample_rect.h),
                };

                ApplyTerrainTileBrightness(
                    tile_texture,
                    state,
                    graphics,
                    static_cast<int>(x),
                    static_cast<int>(y)
                );
                RenderWorldTexture(renderer, graphics, tile_texture, &src, fluid_dst);
                ResetTerrainTileBrightness(tile_texture);
            }
        }
    }
}

void RenderStageTileWrapper(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureStageLighting(state);

    const VisibleWorldRect visible = GetVisibleWorldRect(graphics);
    const Vec2 visible_tl_wc = visible.tl;
    const Vec2 visible_br_wc = visible.br;

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

    for (int border_pass = 0; border_pass < 2; ++border_pass) {
        const bool render_immediate_border_ring = border_pass == 1;
        for (int tile_y = visible_tl_tile_y; tile_y <= visible_br_tile_y; ++tile_y) {
            for (int tile_x = visible_tl_tile_x; tile_x <= visible_br_tile_x; ++tile_x) {
                const bool inside_stage = tile_x >= 0 && tile_y >= 0 && tile_x < stage_tile_width &&
                                          tile_y < stage_tile_height;
                if (inside_stage) {
                    continue;
                }

                const bool is_immediate_border_ring =
                    IsImmediateBorderRingTile(state.stage, tile_x, tile_y);
                if (render_immediate_border_ring != is_immediate_border_ring) {
                    continue;
                }

                const std::optional<StageBorderSideKind> side =
                    state.stage.GetOutOfBoundsSideForTileCoord(tile_x, tile_y);
                if (!side.has_value()) {
                    continue;
                }
                const Tile border_tile = state.stage.GetBorderTile(*side);
                const IVec2 tile_pos = IVec2::New(
                    tile_x * static_cast<int>(kTileSize),
                    tile_y * static_cast<int>(kTileSize)
                );
                if (border_tile == Tile::Air) {
                    const Tile air_tile = GetBackwallFillTileForTileCoord(state.stage, tile_x, tile_y);
                    const TileSourceData* const air_source_data =
                        GetTileSourceData(graphics, air_tile, tile_pos);
                    if (air_source_data == nullptr) {
                        continue;
                    }
                    SDL_Texture* const air_texture = GetTileTexture(graphics, *air_source_data);
                    if (air_texture == nullptr) {
                        continue;
                    }
                    const SDL_FRect air_src{
                        static_cast<float>(air_source_data->sample_rect.x),
                        static_cast<float>(air_source_data->sample_rect.y),
                        static_cast<float>(air_source_data->sample_rect.w),
                        static_cast<float>(air_source_data->sample_rect.h),
                    };
                    const float border_shake = GetBorderTileShake(state.stage, tile_x, tile_y);
                    const Vec2 border_shake_offset = GetShakeOffset(border_shake);
                    const SDL_FRect dst = WorldRectToScreen(
                        graphics,
                        ToVec2(tile_pos) + border_shake_offset,
                        Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                    );
                    ApplyBackwallTileBrightness(air_texture, state, graphics, tile_x, tile_y);
                    RenderWorldTexture(renderer, graphics, air_texture, &air_src, dst);
                    ResetTerrainTileBrightness(air_texture);
                    continue;
                }
                const TileSourceData* const tile_source_data =
                    GetTileSourceData(graphics, border_tile, tile_pos);
                if (tile_source_data == nullptr) {
                    continue;
                }
                SDL_Texture* const tile_texture = GetTileTexture(graphics, *tile_source_data);
                if (tile_texture == nullptr) {
                    continue;
                }
                const SDL_FRect src{
                    static_cast<float>(tile_source_data->sample_rect.x),
                    static_cast<float>(tile_source_data->sample_rect.y),
                    static_cast<float>(tile_source_data->sample_rect.w),
                    static_cast<float>(tile_source_data->sample_rect.h),
                };
                const float border_shake = GetBorderTileShake(state.stage, tile_x, tile_y);
                const Vec2 border_shake_offset = GetShakeOffset(border_shake);
                if (border_shake > 0.0F &&
                    ShouldRenderImmediateBorderBacking(state.stage, tile_x, tile_y)) {
                    const Tile backing_tile =
                        GetBackwallFillTileForTileCoord(state.stage, tile_x, tile_y);
                    const TileSourceData* const backing_source_data =
                        GetTileSourceData(graphics, backing_tile, tile_pos);
                    if (backing_source_data != nullptr) {
                        SDL_Texture* const backing_texture =
                            GetTileTexture(graphics, *backing_source_data);
                        if (backing_texture != nullptr) {
                            const SDL_FRect backing_src{
                                static_cast<float>(backing_source_data->sample_rect.x),
                                static_cast<float>(backing_source_data->sample_rect.y),
                                static_cast<float>(backing_source_data->sample_rect.w),
                                static_cast<float>(backing_source_data->sample_rect.h),
                            };
                            const SDL_FRect backing_dst = WorldRectToScreen(
                                graphics,
                                ToVec2(tile_pos),
                                Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                            );
                            ApplyBackwallTileBrightness(
                                backing_texture,
                                state,
                                graphics,
                                tile_x,
                                tile_y
                            );
                            RenderWorldTexture(renderer, graphics, backing_texture, &backing_src, backing_dst);
                            ResetTerrainTileBrightness(backing_texture);
                        }
                    }
                }
                const SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos) + border_shake_offset,
                    Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );
                ApplyTerrainTileBrightness(tile_texture, state, graphics, tile_x, tile_y);
                RenderWorldTexture(renderer, graphics, tile_texture, &src, dst);
                ResetTerrainTileBrightness(tile_texture);
                RenderTerrainTileLighting(renderer, state, graphics, tile_x, tile_y, dst);
            }
        }
    }
}

void RenderBackgroundStamps(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureStageLighting(state);
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const BackgroundStamp& stamp : state.stage.background_stamps) {
        if (stamp.animation_id == kInvalidFrameDataId) {
            continue;
        }
        if (!ShouldRenderBackgroundStamp(state, stamp)) {
            continue;
        }

        const FrameData* const frame_data =
            GetFirstFrameForAnimationOrFallback(graphics, stamp.animation_id);
        if (frame_data == nullptr) {
            continue;
        }
        SDL_Texture* const sprite_texture = graphics.GetFrameDataTexture(frame_data->image_id);
        if (sprite_texture == nullptr) {
            continue;
        }

        const SDL_FRect src{
            static_cast<float>(frame_data->sample_rect.x),
            static_cast<float>(frame_data->sample_rect.y),
            static_cast<float>(frame_data->sample_rect.w),
            static_cast<float>(frame_data->sample_rect.h),
        };

        const int tile_x =
            static_cast<int>((stamp.pos.x + (static_cast<float>(frame_data->sample_rect.w) * 0.5F)) /
                             static_cast<float>(kTileSize));
        const int tile_y =
            static_cast<int>((stamp.pos.y + (static_cast<float>(frame_data->sample_rect.h) * 0.5F)) /
                             static_cast<float>(kTileSize));
        for (const Vec2& render_offset : render_offsets) {
            const SDL_FRect dst = WorldRectToScreen(
                graphics,
                stamp.pos + render_offset,
                Vec2::New(
                    static_cast<float>(frame_data->sample_rect.w),
                    static_cast<float>(frame_data->sample_rect.h)
                )
            );
            ApplyBackwallTileBrightness(sprite_texture, state, graphics, tile_x, tile_y);
            RenderWorldTexture(renderer, graphics, sprite_texture, &src, dst);
            ResetTerrainTileBrightness(sprite_texture);
        }
    }
}

void RenderEmbeddedTreasureOverlays(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    const bool reveal_hidden_embeds = ShouldRevealEmbeddedTreasure(state);
    EnsureStageLighting(state);
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (std::size_t y = 0; y < state.stage.embedded_treasures.size(); ++y) {
        for (std::size_t x = 0; x < state.stage.embedded_treasures[y].size(); ++x) {
            const EmbeddedTreasure& embedded_treasure = state.stage.embedded_treasures[y][x];
            if (embedded_treasure.IsEmpty()) {
                continue;
            }
            if (!embedded_treasure.IsVisible() && !reveal_hidden_embeds) {
                continue;
            }

            std::optional<FrameDataId> overlay_frame = embedded_treasure.GetOverlayFrame();
            if (!overlay_frame.has_value()) {
                for (const EmbeddedTreasureDrop& drop : embedded_treasure.drops) {
                    if (drop.type_ != EntityType::None && drop.count > 0) {
                        overlay_frame = GetDefaultAnimationIdForArchetype(drop.type_);
                        break;
                    }
                }
            }
            if (!overlay_frame.has_value()) {
                continue;
            }

            const FrameData* const frame_data = GetFirstFrameForAnimation(graphics, *overlay_frame);
            if (frame_data == nullptr) {
                continue;
            }

            SDL_Texture* const sprite_texture = graphics.GetFrameDataTexture(frame_data->image_id);
            if (sprite_texture == nullptr) {
                continue;
            }

            const Vec2 tile_world_pos = Vec2::New(
                static_cast<float>(x * kTileSize),
                static_cast<float>(y * kTileSize)
            );
            const int render_offset_x =
                (static_cast<int>(kTileSize) - frame_data->sample_rect.w) / 2;
            const int render_offset_y =
                (static_cast<int>(kTileSize) - frame_data->sample_rect.h) / 2;
            const Vec2 render_world_pos = tile_world_pos + Vec2::New(
                static_cast<float>(render_offset_x),
                static_cast<float>(render_offset_y)
            );
            const SDL_FRect src{
                static_cast<float>(frame_data->sample_rect.x),
                static_cast<float>(frame_data->sample_rect.y),
                static_cast<float>(frame_data->sample_rect.w),
                static_cast<float>(frame_data->sample_rect.h),
            };
            ApplyTerrainTileBrightness(
                sprite_texture,
                state,
                graphics,
                static_cast<int>(x),
                static_cast<int>(y)
            );
            SDL_SetTextureAlphaMod(sprite_texture, 224);
            for (const Vec2& render_offset : render_offsets) {
                const SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    render_world_pos + render_offset,
                    Vec2::New(
                        static_cast<float>(frame_data->sample_rect.w),
                        static_cast<float>(frame_data->sample_rect.h)
                    )
                );
                RenderWorldTexture(renderer, graphics, sprite_texture, &src, dst);
            }
            SDL_SetTextureAlphaMod(sprite_texture, 255);
            ResetTerrainTileBrightness(sprite_texture);
        }
    }
}

namespace {

const FrameData* GetAnimatedParticleFrameData(
    Graphics& graphics,
    const FrameDataAnimator& animator,
    FrameDataId fallback_animation_id
) {
    const FrameDataId animation_id = animator.HasAnimation() ? animator.animation_id : fallback_animation_id;
    const std::size_t frame_index = animator.HasAnimation() ? animator.current_frame : 0;
    if (animation_id == kInvalidFrameDataId) {
        return nullptr;
    }
    return graphics.frame_data_db.FindFrame(animation_id, frame_index);
}

void RenderAnimatedParticleSprite(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    const std::vector<Vec2>& render_offsets,
    const Vec2& pos,
    const Vec2& size,
    float rotation,
    float alpha,
    bool horizontal_flip,
    const FrameDataAnimator& animator,
    FrameDataId fallback_animation_id = kInvalidFrameDataId,
    float tint_r = 1.0F,
    float tint_g = 1.0F,
    float tint_b = 1.0F
) {
    const FrameData* const frame_data =
        GetAnimatedParticleFrameData(graphics, animator, fallback_animation_id);
    if (frame_data == nullptr) {
        return;
    }

    SDL_Texture* const texture = graphics.GetFrameDataTexture(frame_data->image_id);
    if (texture == nullptr) {
        return;
    }

    (void)state;
    const SDL_FlipMode flip = horizontal_flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    const Vec2 half_size = size / 2.0F;
    SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(alpha * 255.0F));
    SDL_SetTextureColorModFloat(texture, std::clamp(tint_r, 0.0F, 1.0F), std::clamp(tint_g, 0.0F, 1.0F), std::clamp(tint_b, 0.0F, 1.0F));
    const SDL_FRect src{
        static_cast<float>(frame_data->sample_rect.x),
        static_cast<float>(frame_data->sample_rect.y),
        static_cast<float>(frame_data->sample_rect.w),
        static_cast<float>(frame_data->sample_rect.h),
    };
    for (const Vec2& render_offset : render_offsets) {
        const SDL_FRect dst = WorldRectToScreen(graphics, (pos - half_size) + render_offset, size);
        const SDL_FPoint center{dst.w / 2.0F, dst.h / 2.0F};
        RenderWorldTextureRotated(renderer, graphics, texture, &src, dst, rotation, &center, flip);
    }
    SDL_SetTextureAlphaMod(texture, 255);
    SDL_SetTextureColorModFloat(texture, 1.0F, 1.0F, 1.0F);
}

void RenderSpriteParticlesForLayer(SDL_Renderer* renderer, const State& state, Graphics& graphics, DrawLayer layer) {
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const SpriteParticle& particle : state.particles.sprite_particles) {
        if (particle.draw_layer != layer || particle.IsFinished()) {
            continue;
        }
        RenderAnimatedParticleSprite(
            renderer,
            state,
            graphics,
            render_offsets,
            particle.pos,
            particle.size,
            particle.rot,
            particle.alpha,
            particle.horizontal_flip,
            particle.frame_data_animator,
            kInvalidFrameDataId,
            particle.tint_r,
            particle.tint_g,
            particle.tint_b
        );
    }
}

void RenderScriptedParticlesForLayer(SDL_Renderer* renderer, const State& state, Graphics& graphics, DrawLayer layer) {
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const ScriptedParticle& particle : state.particles.scripted_particles) {
        if (particle.draw_layer != layer || particle.IsFinished()) {
            continue;
        }
        RenderAnimatedParticleSprite(
            renderer,
            state,
            graphics,
            render_offsets,
            particle.pos,
            particle.size,
            particle.rot,
            particle.alpha,
            particle.horizontal_flip,
            particle.frame_data_animator
        );
    }
}

void RenderRibbonParticlesForLayer(SDL_Renderer* renderer, const State& state, Graphics& graphics, DrawLayer layer) {
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const RibbonParticle& particle : state.particles.ribbon_particles) {
        if (particle.IsFinished()) {
            continue;
        }
        const RibbonParticleArchetype* const archetype = GetRibbonParticleArchetype(particle.archetype_id);
        if (archetype == nullptr || archetype->draw_layer != layer) {
            continue;
        }
        for (std::size_t i = 0; i + 1 < particle.point_count; ++i) {
            const Vec2 a = particle.points[i];
            const Vec2 b = particle.points[i + 1];
            const Vec2 diff = b - a;
            const float length = Length(diff);
            if (length <= 0.01F) {
                continue;
            }
            const float rotation = std::atan2(diff.y, diff.x) * (180.0F / 3.14159265F);
            RenderAnimatedParticleSprite(
                renderer,
                state,
                graphics,
                render_offsets,
                (a + b) * 0.5F,
                Vec2::New(length, archetype->width),
                rotation,
                particle.alpha,
                false,
                particle.frame_data_animator,
                archetype->animation_id
            );
        }
    }
}

void RenderSegmentedSpriteParticlesForLayer(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    DrawLayer layer
) {
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const SegmentedSpriteParticle& particle : state.particles.segmented_sprite_particles) {
        if (particle.IsFinished()) {
            continue;
        }
        const SegmentedSpriteParticleArchetype* const archetype =
            GetSegmentedSpriteParticleArchetype(particle.archetype_id);
        if (archetype == nullptr || archetype->draw_layer != layer) {
            continue;
        }
        const float spacing = archetype->spacing > 0.0F ? archetype->spacing : Max(archetype->segment_size.x, 1.0F);
        for (std::size_t i = 0; i + 1 < particle.point_count; ++i) {
            const Vec2 a = particle.points[i];
            const Vec2 b = particle.points[i + 1];
            const Vec2 diff = b - a;
            const float length = Length(diff);
            if (length <= 0.01F) {
                continue;
            }
            const Vec2 dir = diff / length;
            const float rotation = std::atan2(diff.y, diff.x) * (180.0F / 3.14159265F);
            for (float distance_along = 0.0F; distance_along < length; distance_along += spacing) {
                const Vec2 center = a + (dir * distance_along);
                RenderAnimatedParticleSprite(
                    renderer,
                    state,
                    graphics,
                    render_offsets,
                    center,
                    archetype->segment_size,
                    rotation,
                    particle.alpha,
                    particle.horizontal_flip,
                    particle.frame_data_animator,
                    archetype->animation_id
                );
            }
        }
    }
}

void RenderParticlesForLayer(SDL_Renderer* renderer, const State& state, Graphics& graphics, DrawLayer layer) {
    RenderSpriteParticlesForLayer(renderer, state, graphics, layer);
    RenderScriptedParticlesForLayer(renderer, state, graphics, layer);
    RenderRibbonParticlesForLayer(renderer, state, graphics, layer);
    RenderSegmentedSpriteParticlesForLayer(renderer, state, graphics, layer);
}

} // namespace

void RenderEntities(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    const std::vector<Vec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
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
            if (!entity.active || !entity.render_enabled) {
                continue;
            }
            if (entity.draw_layer != layer) {
                next_draw_queue.push_back(entity_id);
                continue;
            }

            const FrameData* const frame_data =
                entities::common::GetCurrentFrameDataForEntity(entity, graphics);
            if (frame_data == nullptr) {
                continue;
            }

            SDL_Texture* const sprite_texture =
                graphics.GetFrameDataTexture(frame_data->image_id);
            if (sprite_texture == nullptr) {
                continue;
            }

            const Vec2 sprite_world_size = Vec2::New(
                static_cast<float>(frame_data->sample_rect.w),
                static_cast<float>(frame_data->sample_rect.h)
            );
            const Vec2 sprite_scaled_size =
                sprite_world_size * entity.frame_data_animator.scale;
            const Vec2 render_position =
                entities::common::GetSpriteTopLeftForEntity(entity, *frame_data);

            const SDL_FRect src{
                static_cast<float>(frame_data->sample_rect.x),
                static_cast<float>(frame_data->sample_rect.y),
                static_cast<float>(frame_data->sample_rect.w),
                static_cast<float>(frame_data->sample_rect.h),
            };
            const SDL_FlipMode flip =
                entity.facing == LeftOrRight::Right ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            const Uint8 entity_alpha = static_cast<Uint8>(std::clamp(entity.alpha, 0.0F, 1.0F) * 255.0F);
            SDL_SetTextureAlphaMod(sprite_texture, entity_alpha);
            if (entity.type_ == EntityType::BallAndChainBall && entity.entity_a.has_value()) {
                if (const Entity* const attached = state.entity_manager.GetEntity(*entity.entity_a)) {
                    if (attached->active) {
                        SDL_SetRenderDrawColor(renderer, 132, 132, 132, 255);
                        const Vec2 anchor_world = attached->GetCenter() +
                                                  Vec2::New(0.0F, (attached->size.y * 0.5F) - 1.0F);
                        const Vec2 ball_world = GetNearestWorldPoint(state.stage, anchor_world, entity.GetCenter());
                        for (const Vec2& render_offset : render_offsets) {
                            const Vec2 anchor_screen = WorldToScreen(graphics, anchor_world + render_offset);
                            const Vec2 ball_screen = WorldToScreen(graphics, ball_world + render_offset);
                            SDL_RenderLine(renderer, anchor_screen.x, anchor_screen.y, ball_screen.x, ball_screen.y);
                            SDL_RenderLine(renderer, anchor_screen.x, anchor_screen.y + 1.0F, ball_screen.x, ball_screen.y + 1.0F);
                        }
                    }
                }
            }
            for (const Vec2& render_offset : render_offsets) {
                const Vec2 shake_offset = GetShakeOffset(entity.shake);
                SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    render_position + render_offset + shake_offset,
                    sprite_scaled_size
                );
                if (std::abs(entity.rotation) <= 0.01F) {
                    RenderWorldTextureRotated(renderer, graphics, sprite_texture, &src, dst, 0.0, nullptr, flip);
                } else {
                    const Vec2 rotation_world =
                        entities::common::GetVisualCenterForEntity(entity, graphics, entity.GetCenter()) +
                        render_offset + shake_offset;
                    const Vec2 rotation_screen = WorldToScreen(graphics, rotation_world);
                    const SDL_FPoint rotation_center{
                        rotation_screen.x - dst.x,
                        rotation_screen.y - dst.y
                    };
                    RenderWorldTextureRotated(
                        renderer,
                        graphics,
                        sprite_texture,
                        &src,
                        dst,
                        entity.rotation,
                        &rotation_center,
                        flip
                    );
                }
                if (entity.stone) {
                    const AABB stone_overlay_aabb = entity.GetAABB();
                    RenderStoneEntityOverlay(
                        renderer,
                        state,
                        graphics,
                        stone_overlay_aabb.tl + render_offset,
                        stone_overlay_aabb.br - stone_overlay_aabb.tl + Vec2::New(1.0F, 1.0F)
                    );
                }
            }
            SDL_SetTextureAlphaMod(sprite_texture, 255);
        }
        RenderParticlesForLayer(renderer, state, graphics, layer);
    }
}

} // namespace splonks
