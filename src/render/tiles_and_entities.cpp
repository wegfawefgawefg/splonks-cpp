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
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
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

std::optional<IVec2> ResolveWrappedTileCoord(const Stage& stage, int tile_x, int tile_y) {
    const IVec2 wrapped = stage.WrapTileCoord(IVec2::New(tile_x, tile_y));
    if (!stage.IsTileCoordInside(wrapped.x, wrapped.y)) {
        return std::nullopt;
    }
    return wrapped;
}

float Dot(const Vec2& left, const Vec2& right) {
    return (left.x * right.x) + (left.y * right.y);
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

Tile GetWrappedFluidTile(const Stage& stage, int tile_x, int tile_y) {
    const std::optional<IVec2> wrapped = ResolveWrappedTileCoord(stage, tile_x, tile_y);
    if (!wrapped.has_value()) {
        return Tile::Air;
    }
    return stage.GetFluidTile(
        static_cast<unsigned int>(wrapped->x),
        static_cast<unsigned int>(wrapped->y)
    );
}

const FrameData* GetAnimationFrameForTick(
    const FrameDataDb& frame_data_db,
    FrameDataId animation_id,
    std::uint64_t tick
) {
    const FrameDataAnimation* const animation = frame_data_db.FindAnimation(animation_id);
    if (animation == nullptr || animation->frame_indices.empty()) {
        return nullptr;
    }

    std::uint64_t total_duration = 0;
    for (const std::size_t frame_index : animation->frame_indices) {
        total_duration += static_cast<std::uint64_t>(
            std::max(frame_data_db.frames[frame_index].duration, 1)
        );
    }
    if (total_duration == 0) {
        return nullptr;
    }

    std::uint64_t local_tick = tick % total_duration;
    for (const std::size_t frame_index : animation->frame_indices) {
        const FrameData& frame_data = frame_data_db.frames[frame_index];
        const std::uint64_t duration = static_cast<std::uint64_t>(std::max(frame_data.duration, 1));
        if (local_tick < duration) {
            return &frame_data;
        }
        local_tick -= duration;
    }

    return &frame_data_db.frames[animation->frame_indices.front()];
}

std::optional<TileSourceData> GetAnimatedWaterTopSourceData(
    Graphics& graphics,
    const IVec2& tile_pos,
    std::uint64_t tick
) {
    const TileSourceData* const fallback_top_source_data =
        GetTileSourceData(graphics, Tile::WaterTop, tile_pos);
    if (fallback_top_source_data == nullptr) {
        return std::nullopt;
    }

    TileSourceData top_source_data = *fallback_top_source_data;
    const FrameData* const top_frame = GetAnimationFrameForTick(
        graphics.frame_data_db,
        HashFrameDataIdConstexpr("watertop"),
        tick
    );
    if (top_frame != nullptr) {
        top_source_data.image_id = top_frame->image_id;
        top_source_data.sample_rect = top_frame->sample_rect;
        top_source_data.cbox = top_frame->cbox;
    }
    return top_source_data;
}

Vec2 WorldPointToScreenForGeometry(const Graphics& graphics, const Vec2& world_pos) {
    Vec2 screen = WorldToScreen(graphics, world_pos);
    if (!graphics.world_rotation_active) {
        return screen;
    }

    const Vec2 pivot_screen = WorldToScreen(graphics, graphics.world_rotation_pivot);
    constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
    const double radians = static_cast<double>(graphics.world_rotation_degrees) * kDegreesToRadians;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    const Vec2 delta = screen - pivot_screen;
    screen = pivot_screen + Vec2::New(
        static_cast<float>((static_cast<double>(delta.x) * c) - (static_cast<double>(delta.y) * s)),
        static_cast<float>((static_cast<double>(delta.x) * s) + (static_cast<double>(delta.y) * c))
    );
    return screen;
}

SDL_FColor MakeFluidVertexColor(float brightness, std::uint8_t alpha) {
    const float factor = std::clamp(brightness, 0.0F, 2.0F);
    return SDL_FColor{
        factor,
        factor,
        factor,
        static_cast<float>(alpha) / 255.0F,
    };
}

struct FluidContourSegment {
    Vec2 a;
    Vec2 b;
};

void RenderWorldTextureQuad(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    SDL_Texture* texture,
    const SDL_FRect& src,
    const std::array<Vec2, 4>& world_points,
    const SDL_FColor& color
) {
    if (texture == nullptr || src.w <= 0.0F || src.h <= 0.0F) {
        return;
    }

    float texture_width = 0.0F;
    float texture_height = 0.0F;
    if (!SDL_GetTextureSize(texture, &texture_width, &texture_height) ||
        texture_width <= 0.0F || texture_height <= 0.0F) {
        return;
    }

    const Vec2 tl = WorldPointToScreenForGeometry(graphics, world_points[0]);
    const Vec2 tr = WorldPointToScreenForGeometry(graphics, world_points[1]);
    const Vec2 br = WorldPointToScreenForGeometry(graphics, world_points[2]);
    const Vec2 bl = WorldPointToScreenForGeometry(graphics, world_points[3]);

    const float u0 = src.x / texture_width;
    const float v0 = src.y / texture_height;
    const float u1 = (src.x + src.w) / texture_width;
    const float v1 = (src.y + src.h) / texture_height;
    const std::array<SDL_Vertex, 4> vertices{
        SDL_Vertex{SDL_FPoint{tl.x, tl.y}, color, SDL_FPoint{u0, v0}},
        SDL_Vertex{SDL_FPoint{tr.x, tr.y}, color, SDL_FPoint{u1, v0}},
        SDL_Vertex{SDL_FPoint{br.x, br.y}, color, SDL_FPoint{u1, v1}},
        SDL_Vertex{SDL_FPoint{bl.x, bl.y}, color, SDL_FPoint{u0, v1}},
    };
    constexpr std::array<int, 6> indices{0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(
        renderer,
        texture,
        vertices.data(),
        static_cast<int>(vertices.size()),
        indices.data(),
        static_cast<int>(indices.size())
    );
}

void RenderWorldTextureRibbon(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    SDL_Texture* texture,
    const SDL_FRect& src,
    const Vec2& start,
    const Vec2& end,
    float thickness,
    bool use_right_normal,
    const SDL_FColor& color
) {
    const Vec2 delta = end - start;
    const float length = std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
    if (length <= 0.01F || thickness <= 0.0F) {
        return;
    }

    const Vec2 dir = delta / length;
    const Vec2 normal = use_right_normal
        ? Vec2::New(dir.y, -dir.x)
        : Vec2::New(-dir.y, dir.x);
    const float max_piece_len = std::max(src.w, 1.0F);
    float consumed = 0.0F;
    while (consumed < length - 0.01F) {
        const float piece_len = std::min(max_piece_len, length - consumed);
        const float t0 = consumed / length;
        const float t1 = (consumed + piece_len) / length;
        const Vec2 piece_start = start + (delta * t0);
        const Vec2 piece_end = start + (delta * t1);
        SDL_FRect piece_src = src;
        piece_src.w = src.w * (piece_len / max_piece_len);
        RenderWorldTextureQuad(
            renderer,
            graphics,
            texture,
            piece_src,
            std::array<Vec2, 4>{
                piece_start,
                piece_end,
                piece_end + (normal * thickness),
                piece_start + (normal * thickness),
            },
            color
        );
        consumed += piece_len;
    }
}

struct RibbonPlacement {
    Vec2 start = Vec2::New(0.0F, 0.0F);
    Vec2 end = Vec2::New(0.0F, 0.0F);
    bool use_right_normal = false;
};

RibbonPlacement MakeCenteredContourRibbonPlacement(
    const FluidContourSegment& segment,
    bool tile_center_is_inside_fluid,
    float thickness
) {
    const Vec2 delta = segment.b - segment.a;
    const Vec2 left_normal = NormalizeOrZero(Vec2::New(-delta.y, delta.x));
    const Vec2 right_normal = Vec2::New(left_normal.x * -1.0F, left_normal.y * -1.0F);
    const Vec2 midpoint = (segment.a + segment.b) * 0.5F;
    Vec2 desired_normal = NormalizeOrZero(
        Vec2::New(static_cast<float>(kTileSize) * 0.5F, static_cast<float>(kTileSize) * 0.5F) -
        midpoint
    );
    if (!tile_center_is_inside_fluid) {
        desired_normal = desired_normal * -1.0F;
    }
    if (Length(desired_normal) <= 0.001F) {
        desired_normal = left_normal;
    }

    const bool use_right_normal = Dot(right_normal, desired_normal) > Dot(left_normal, desired_normal);
    const Vec2 ribbon_normal = use_right_normal ? right_normal : left_normal;
    const Vec2 centered_offset = ribbon_normal * (thickness * -0.5F);
    return RibbonPlacement{
        .start = segment.a + centered_offset,
        .end = segment.b + centered_offset,
        .use_right_normal = use_right_normal,
    };
}

void RenderFluidFlowIndicator(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    const Vec2& tile_world,
    const Vec2& velocity,
    std::uint64_t tick,
    float opacity
) {
    const float speed = Length(velocity);
    if (speed <= 0.01F) {
        return;
    }

    const Vec2 direction = NormalizeOrZero(velocity);
    const float phase = std::fmod(
        (static_cast<float>(tick % 10000ULL) * (0.015F + (speed * 0.06F))),
        1.0F
    );
    const Vec2 point =
        tile_world +
        Vec2::New(static_cast<float>(kTileSize) * 0.5F, static_cast<float>(kTileSize) * 0.5F) +
        direction * ((phase - 0.5F) * static_cast<float>(kTileSize) * 0.8F);
    const Vec2 point_screen = WorldPointToScreenForGeometry(graphics, point);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const std::uint8_t alpha = static_cast<std::uint8_t>(
        std::clamp(static_cast<int>(std::round(220.0F * std::clamp(opacity, 0.0F, 1.0F))), 0, 220)
    );
    SDL_SetRenderDrawColor(renderer, 230, 250, 255, alpha);
    SDL_RenderPoint(renderer, point_screen.x, point_screen.y);
}

std::uint32_t HashFluidCell(int tile_x, int tile_y, std::uint32_t salt) {
    std::uint32_t value =
        (static_cast<std::uint32_t>(tile_x) * 0x9E3779B9U) ^
        (static_cast<std::uint32_t>(tile_y) * 0x85EBCA6BU) ^
        salt;
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    value ^= value >> 16U;
    return value;
}

void RenderFluidBubble(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    const Vec2& tile_world,
    int tile_x,
    int tile_y,
    std::uint64_t scene_frame,
    float display_level,
    float visible_cutoff,
    std::uint8_t alpha,
    float brightness
) {
    if (display_level <= visible_cutoff) {
        return;
    }
    const std::uint32_t seed = HashFluidCell(tile_x, tile_y, 0xB0B13U);
    const std::uint64_t period_frames = 150ULL + static_cast<std::uint64_t>(seed % 180U);
    const std::uint64_t active_frames = 36ULL + static_cast<std::uint64_t>((seed >> 8U) % 28U);
    const std::uint64_t local_tick =
        (scene_frame + static_cast<std::uint64_t>(seed % period_frames)) % period_frames;
    if (local_tick >= active_frames) {
        return;
    }

    const FrameData* const bubble_frame = GetAnimationFrameForTick(
        graphics.frame_data_db,
        HashFrameDataIdConstexpr("bubble"),
        scene_frame + static_cast<std::uint64_t>(seed & 31U)
    );
    if (bubble_frame == nullptr) {
        return;
    }
    SDL_Texture* const bubble_texture = graphics.GetFrameDataTexture(bubble_frame->image_id);
    if (bubble_texture == nullptr) {
        return;
    }

    const float progress =
        static_cast<float>(local_tick) / static_cast<float>(std::max<std::uint64_t>(active_frames, 1ULL));
    const float x_jitter = static_cast<float>((seed >> 16U) % 9U) - 4.0F;
    const float bubble_x =
        (static_cast<float>(kTileSize) * 0.5F) + x_jitter -
        (static_cast<float>(bubble_frame->sample_rect.w) * 0.5F);
    const float bubble_y =
        std::lerp(
            static_cast<float>(kTileSize) - 2.0F,
            1.0F,
            progress
        ) -
        (static_cast<float>(bubble_frame->sample_rect.h) * 0.5F);
    const Vec2 bubble_world =
        tile_world +
        Vec2::New(
            std::round(bubble_x),
            std::round(bubble_y)
        );
    const SDL_FRect src{
        static_cast<float>(bubble_frame->sample_rect.x),
        static_cast<float>(bubble_frame->sample_rect.y),
        static_cast<float>(bubble_frame->sample_rect.w),
        static_cast<float>(bubble_frame->sample_rect.h),
    };
    const SDL_FRect dst = WorldRectToScreen(
        graphics,
        bubble_world,
        Vec2::New(
            static_cast<float>(bubble_frame->sample_rect.w),
            static_cast<float>(bubble_frame->sample_rect.h)
        )
    );
    SDL_SetTextureColorModFloat(bubble_texture, brightness, brightness, brightness);
    SDL_SetTextureAlphaMod(bubble_texture, alpha);
    RenderWorldTexture(renderer, graphics, bubble_texture, &src, dst);
    SDL_SetTextureAlphaMod(bubble_texture, 255);
    SDL_SetTextureColorModFloat(bubble_texture, 1.0F, 1.0F, 1.0F);
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
    constexpr int kTileSizePx = static_cast<int>(kTileSize);
    constexpr float kMinVisibleDisplayLevel = 0.0001F;
    const float min_fluid_display_level =
        std::clamp(state.debug_fluid_brush.render_cutoff_amount, 0.0F, 1.0F);
    const float effective_display_cutoff =
        std::max(min_fluid_display_level, kMinVisibleDisplayLevel);
    const std::uint8_t body_alpha = static_cast<std::uint8_t>(
        std::clamp(
            static_cast<int>(std::round(
                255.0F * std::clamp(state.debug_fluid_brush.water_alpha, 0.0F, 1.0F)
            )),
            0,
            255
        )
    );
    const std::uint8_t top_alpha = static_cast<std::uint8_t>(
        std::clamp(
            static_cast<int>(std::round(static_cast<float>(body_alpha) * (224.0F / 176.0F))),
            0,
            255
        )
    );
    const std::uint8_t bubble_alpha = static_cast<std::uint8_t>(
        std::clamp(
            static_cast<int>(std::round(static_cast<float>(body_alpha) * (180.0F / 176.0F))),
            0,
            255
        )
    );

    struct FluidRenderCell {
        Tile visible_tile = Tile::Air;
        float liquid_level = 0.0F;
        float visible_level = 0.0F;
        float display_level = 0.0F;
        bool terrain_solid = false;
        bool has_liquid = false;
        bool has_visible_liquid = false;
        bool render_candidate = false;
    };

    const int stage_tile_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_tile_height = static_cast<int>(state.stage.GetTileHeight());
    if (stage_tile_width <= 0 || stage_tile_height <= 0) {
        return;
    }

    std::vector<FluidRenderCell> cells(
        static_cast<std::size_t>(stage_tile_width * stage_tile_height)
    );
    auto cell_at = [&](int tile_x, int tile_y) -> FluidRenderCell* {
        const std::optional<IVec2> wrapped = ResolveWrappedTileCoord(
            state.stage,
            tile_x,
            tile_y
        );
        if (!wrapped.has_value()) {
            return nullptr;
        }
        return &cells[
            static_cast<std::size_t>((wrapped->y * stage_tile_width) + wrapped->x)
        ];
    };
    auto const_cell_at = [&](int tile_x, int tile_y) -> const FluidRenderCell* {
        const std::optional<IVec2> wrapped = ResolveWrappedTileCoord(
            state.stage,
            tile_x,
            tile_y
        );
        if (!wrapped.has_value()) {
            return nullptr;
        }
        return &cells[
            static_cast<std::size_t>((wrapped->y * stage_tile_width) + wrapped->x)
        ];
    };
    for (int y = 0; y < stage_tile_height; ++y) {
        for (int x = 0; x < stage_tile_width; ++x) {
            FluidRenderCell& cell = *cell_at(x, y);
            cell.terrain_solid = GetTileArchetype(
                state.stage.GetTile(static_cast<unsigned int>(x), static_cast<unsigned int>(y))
            ).solid;
            const Tile fluid_tile = state.stage.GetFluidTile(
                static_cast<unsigned int>(x),
                static_cast<unsigned int>(y)
            );
            const float amount = state.stage.GetFluidAmount(
                static_cast<unsigned int>(x),
                static_cast<unsigned int>(y)
            );
            cell.has_liquid =
                GetTileArchetype(fluid_tile).simulated_fluid &&
                amount > kMinVisibleDisplayLevel;
            float& display_amount =
                state.stage.fluid_display_amount[static_cast<std::size_t>(y)]
                                                [static_cast<std::size_t>(x)];
            if (!cell.has_liquid) {
                if (state.debug_fluid_brush.temporal_smoothing_enabled) {
                    const float response = std::clamp(
                        state.debug_fluid_brush.temporal_smoothing_response,
                        0.0F,
                        1.0F
                    );
                    display_amount += (0.0F - display_amount) * response;
                } else {
                    display_amount = 0.0F;
                }
                if (display_amount <= effective_display_cutoff || cell.terrain_solid) {
                    display_amount = 0.0F;
                    continue;
                }

                Tile residual_visible_tile = Tile::WaterSwim;
                for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                        const Tile nearby_fluid_tile =
                            GetWrappedFluidTile(state.stage, x + offset_x, y + offset_y);
                        if (GetTileArchetype(nearby_fluid_tile).simulated_fluid) {
                            residual_visible_tile = nearby_fluid_tile;
                            break;
                        }
                    }
                    if (residual_visible_tile != Tile::WaterSwim) {
                        break;
                    }
                }

                cell.visible_tile = residual_visible_tile;
                cell.visible_level = std::clamp(display_amount, 0.0F, 1.0F);
                cell.display_level = cell.visible_level;
                cell.has_visible_liquid = true;
                cell.render_candidate = true;
                continue;
            }
            cell.visible_tile = fluid_tile;
            cell.liquid_level = std::clamp(amount, 0.0F, 1.0F);
            if (state.debug_fluid_brush.temporal_smoothing_enabled) {
                const float response = std::clamp(
                    state.debug_fluid_brush.temporal_smoothing_response,
                    0.0F,
                    1.0F
                );
                display_amount += (cell.liquid_level - display_amount) * response;
            } else {
                display_amount = cell.liquid_level;
            }
            cell.visible_level = std::clamp(display_amount, 0.0F, 1.0F);
            cell.display_level = cell.visible_level;
            cell.has_visible_liquid = true;
            cell.render_candidate = true;
        }
    }

    for (const Vec2& render_offset : render_offsets) {
        for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
            for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
                const int int_x = static_cast<int>(x);
                const int int_y = static_cast<int>(y);
                const FluidRenderCell* const cell = const_cell_at(int_x, int_y);
                if (cell == nullptr || !cell->render_candidate || cell->terrain_solid) {
                    continue;
                }

                const IVec2 tile_pos = IVec2::New(
                    static_cast<int>(x * kTileSize),
                    static_cast<int>(y * kTileSize)
                );

                const TileSourceData* const tile_source_data =
                    GetTileSourceData(graphics, cell->visible_tile, tile_pos);
                if (tile_source_data == nullptr) {
                    continue;
                }
                SDL_Texture* const tile_texture = GetTileTexture(graphics, *tile_source_data);
                if (tile_texture == nullptr) {
                    continue;
                }

                const std::uint64_t top_tick =
                    static_cast<std::uint64_t>(state.scene_frame) +
                    (static_cast<std::uint64_t>(x) * 13ULL) +
                    (static_cast<std::uint64_t>(y) * 7ULL);
                const std::optional<TileSourceData> top_source_data =
                    GetAnimatedWaterTopSourceData(graphics, tile_pos, top_tick);
                SDL_Texture* const top_texture = top_source_data.has_value()
                    ? GetTileTexture(graphics, *top_source_data)
                    : nullptr;
                const SDL_FRect top_src = top_source_data.has_value()
                    ? SDL_FRect{
                          static_cast<float>(top_source_data->sample_rect.x),
                          static_cast<float>(top_source_data->sample_rect.y),
                          static_cast<float>(top_source_data->sample_rect.w),
                          static_cast<float>(top_source_data->sample_rect.h),
                      }
                    : SDL_FRect{};

                const Vec2 tile_world = ToVec2(tile_pos) + render_offset;
                float brightness = 1.0F;
                if (state.debug_fluid_brush.lighting_enabled) {
                    const float sampled_brightness =
                        GetBackwallBrightnessForRender(state, static_cast<int>(x), static_cast<int>(y));
                    brightness = std::clamp(
                        std::lerp(
                            1.0F,
                            sampled_brightness,
                            std::clamp(state.debug_fluid_brush.lighting_strength, 0.0F, 2.0F)
                        ),
                        0.0F,
                        2.0F
                    );
                }

                const SDL_FRect body_src{
                    static_cast<float>(tile_source_data->sample_rect.x),
                    static_cast<float>(tile_source_data->sample_rect.y),
                    static_cast<float>(tile_source_data->sample_rect.w),
                    static_cast<float>(tile_source_data->sample_rect.h),
                };
                auto render_flow_indicator = [&]() {
                    if (!state.debug_fluid_brush.show_flow_indicators ||
                        y >= state.stage.fluid_velocity.size() ||
                        x >= state.stage.fluid_velocity[y].size()) {
                        return;
                    }
                    const Vec2 velocity = state.stage.fluid_velocity[y][x];
                    const std::uint64_t flow_tick =
                        static_cast<std::uint64_t>(state.scene_frame) +
                        (static_cast<std::uint64_t>(x) * 29ULL) +
                        (static_cast<std::uint64_t>(y) * 17ULL);
                    RenderFluidFlowIndicator(
                        renderer,
                        graphics,
                        tile_world,
                        velocity,
                        flow_tick,
                        cell->display_level
                    );
                };

                if (cell->display_level <= effective_display_cutoff) {
                    continue;
                }
                RenderWorldTextureQuad(
                    renderer,
                    graphics,
                    tile_texture,
                    body_src,
                    std::array<Vec2, 4>{
                        tile_world + Vec2::New(0.0F, 0.0F),
                        tile_world + Vec2::New(static_cast<float>(kTileSizePx), 0.0F),
                        tile_world + Vec2::New(
                            static_cast<float>(kTileSizePx),
                            static_cast<float>(kTileSizePx)
                        ),
                        tile_world + Vec2::New(0.0F, static_cast<float>(kTileSizePx)),
                    },
                    MakeFluidVertexColor(brightness, body_alpha)
                );

                auto neighbor_is_fluid_or_solid = [&](int offset_x, int offset_y) -> bool {
                    const FluidRenderCell* const nearby =
                        const_cell_at(int_x + offset_x, int_y + offset_y);
                    if (nearby == nullptr || nearby->terrain_solid) {
                        return true;
                    }
                    return nearby->has_visible_liquid &&
                           nearby->display_level > effective_display_cutoff;
                };
                auto render_cell_edge_topper = [&](const FluidContourSegment& segment) {
                    if (top_texture == nullptr || top_src.h <= 0.0F) {
                        return;
                    }
                    const RibbonPlacement ribbon = MakeCenteredContourRibbonPlacement(
                        segment,
                        true,
                        top_src.h
                    );
                    RenderWorldTextureRibbon(
                        renderer,
                        graphics,
                        top_texture,
                        top_src,
                        tile_world + ribbon.start,
                        tile_world + ribbon.end,
                        top_src.h,
                        ribbon.use_right_normal,
                        MakeFluidVertexColor(brightness, top_alpha)
                    );
                };
                if (!neighbor_is_fluid_or_solid(0, -1)) {
                    render_cell_edge_topper(FluidContourSegment{
                        .a = Vec2::New(0.0F, 0.0F),
                        .b = Vec2::New(static_cast<float>(kTileSizePx), 0.0F),
                    });
                }
                if (!neighbor_is_fluid_or_solid(1, 0)) {
                    render_cell_edge_topper(FluidContourSegment{
                        .a = Vec2::New(static_cast<float>(kTileSizePx), 0.0F),
                        .b = Vec2::New(
                            static_cast<float>(kTileSizePx),
                            static_cast<float>(kTileSizePx)
                        ),
                    });
                }
                if (!neighbor_is_fluid_or_solid(0, 1)) {
                    render_cell_edge_topper(FluidContourSegment{
                        .a = Vec2::New(0.0F, static_cast<float>(kTileSizePx)),
                        .b = Vec2::New(
                            static_cast<float>(kTileSizePx),
                            static_cast<float>(kTileSizePx)
                        ),
                    });
                }
                if (!neighbor_is_fluid_or_solid(-1, 0)) {
                    render_cell_edge_topper(FluidContourSegment{
                        .a = Vec2::New(0.0F, 0.0F),
                        .b = Vec2::New(0.0F, static_cast<float>(kTileSizePx)),
                    });
                }
                RenderFluidBubble(
                    renderer,
                    graphics,
                    tile_world,
                    int_x,
                    int_y,
                    static_cast<std::uint64_t>(state.scene_frame),
                    cell->display_level,
                    effective_display_cutoff,
                    bubble_alpha,
                    brightness
                );
                render_flow_indicator();
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
