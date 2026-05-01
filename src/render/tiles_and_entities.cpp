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

std::uint8_t GetWrappedFluidAmount(const Stage& stage, int tile_x, int tile_y) {
    const std::optional<IVec2> wrapped = ResolveWrappedTileCoord(stage, tile_x, tile_y);
    if (!wrapped.has_value()) {
        return 0;
    }
    const auto x = static_cast<unsigned int>(wrapped->x);
    const auto y = static_cast<unsigned int>(wrapped->y);
    if (!GetTileArchetype(stage.GetFluidTile(x, y)).simulated_fluid) {
        return 0;
    }
    return stage.GetFluidAmount(x, y);
}

bool HasWrappedFluid(const Stage& stage, int tile_x, int tile_y) {
    return GetWrappedFluidAmount(stage, tile_x, tile_y) > 0;
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

Tile GetWrappedTerrainTile(const Stage& stage, int tile_x, int tile_y) {
    const std::optional<IVec2> wrapped = ResolveWrappedTileCoord(stage, tile_x, tile_y);
    if (!wrapped.has_value()) {
        return Tile::Air;
    }
    return stage.GetTile(
        static_cast<unsigned int>(wrapped->x),
        static_cast<unsigned int>(wrapped->y)
    );
}

bool IsWrappedTerrainSolid(const Stage& stage, int tile_x, int tile_y) {
    return GetTileArchetype(GetWrappedTerrainTile(stage, tile_x, tile_y)).solid;
}

bool CanDrawFluidFallThrough(const Stage& stage, int tile_x, int tile_y) {
    const std::optional<IVec2> wrapped = ResolveWrappedTileCoord(stage, tile_x, tile_y);
    if (!wrapped.has_value()) {
        return false;
    }
    return !GetTileArchetype(GetWrappedTerrainTile(stage, tile_x, tile_y)).solid;
}

int GetFluidFillHeightPx(std::uint8_t amount) {
    constexpr std::uint8_t kMaxFluidAmount = 255;
    constexpr int kTileSizePx = static_cast<int>(kTileSize);
    if (amount == 0) {
        return 0;
    }
    return std::clamp(
        static_cast<int>(std::ceil(
            (static_cast<float>(amount) / static_cast<float>(kMaxFluidAmount)) *
            static_cast<float>(kTileSizePx)
        )),
        1,
        kTileSizePx
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

struct FluidMarchingVertex {
    Vec2 local;
    float signed_distance = 0.0F;
};

struct FluidContourSegment {
    Vec2 a;
    Vec2 b;
};

Vec2 InterpolateFluidVertex(
    const FluidMarchingVertex& a,
    const FluidMarchingVertex& b
) {
    const float denom = a.signed_distance - b.signed_distance;
    if (std::abs(denom) <= 0.0001F) {
        return (a.local + b.local) * 0.5F;
    }
    const float t = std::clamp(a.signed_distance / denom, 0.0F, 1.0F);
    return a.local + ((b.local - a.local) * t);
}

struct FluidMarchingResult {
    std::vector<std::vector<Vec2>> polygons;
    std::vector<FluidContourSegment> contour_segments;
};

FluidMarchingResult BuildFilledFluidMarchingSquares(
    const std::array<FluidMarchingVertex, 4>& corners,
    float center_signed_distance
) {
    const bool tl_inside = corners[0].signed_distance >= 0.0F;
    const bool tr_inside = corners[1].signed_distance >= 0.0F;
    const bool br_inside = corners[2].signed_distance >= 0.0F;
    const bool bl_inside = corners[3].signed_distance >= 0.0F;
    const int case_id =
        (tl_inside ? 1 : 0) |
        (tr_inside ? 2 : 0) |
        (br_inside ? 4 : 0) |
        (bl_inside ? 8 : 0);

    const Vec2 p0 = corners[0].local;
    const Vec2 p1 = corners[1].local;
    const Vec2 p2 = corners[2].local;
    const Vec2 p3 = corners[3].local;
    const Vec2 e0 = InterpolateFluidVertex(corners[0], corners[1]);
    const Vec2 e1 = InterpolateFluidVertex(corners[1], corners[2]);
    const Vec2 e2 = InterpolateFluidVertex(corners[2], corners[3]);
    const Vec2 e3 = InterpolateFluidVertex(corners[3], corners[0]);

    FluidMarchingResult result;
    auto add_polygon = [&](std::initializer_list<Vec2> points) {
        if (points.size() < 3) {
            return;
        }
        result.polygons.emplace_back(points);
    };
    auto add_segment = [&](const Vec2& a, const Vec2& b) {
        result.contour_segments.push_back(FluidContourSegment{.a = a, .b = b});
    };

    switch (case_id) {
    case 0:
        break;
    case 1:
        add_polygon({p0, e0, e3});
        add_segment(e0, e3);
        break;
    case 2:
        add_polygon({e0, p1, e1});
        add_segment(e0, e1);
        break;
    case 3:
        add_polygon({p0, p1, e1, e3});
        add_segment(e1, e3);
        break;
    case 4:
        add_polygon({e1, p2, e2});
        add_segment(e1, e2);
        break;
    case 5:
        if (center_signed_distance >= 0.0F) {
            add_polygon({p0, e0, e1, p2, e2, e3});
            add_segment(e0, e1);
            add_segment(e2, e3);
        } else {
            add_polygon({p0, e0, e3});
            add_polygon({e1, p2, e2});
            add_segment(e0, e3);
            add_segment(e1, e2);
        }
        break;
    case 6:
        add_polygon({e0, p1, p2, e2});
        add_segment(e0, e2);
        break;
    case 7:
        add_polygon({p0, p1, p2, e2, e3});
        add_segment(e2, e3);
        break;
    case 8:
        add_polygon({e2, p3, e3});
        add_segment(e2, e3);
        break;
    case 9:
        add_polygon({p0, e0, e2, p3});
        add_segment(e0, e2);
        break;
    case 10:
        if (center_signed_distance >= 0.0F) {
            add_polygon({e0, p1, e1, e2, p3, e3});
            add_segment(e0, e3);
            add_segment(e1, e2);
        } else {
            add_polygon({e0, p1, e1});
            add_polygon({e2, p3, e3});
            add_segment(e0, e1);
            add_segment(e2, e3);
        }
        break;
    case 11:
        add_polygon({p0, p1, e1, e2, p3});
        add_segment(e1, e2);
        break;
    case 12:
        add_polygon({e1, p2, p3, e3});
        add_segment(e1, e3);
        break;
    case 13:
        add_polygon({p0, e0, e1, p2, p3});
        add_segment(e0, e1);
        break;
    case 14:
        add_polygon({e0, p1, p2, p3, e3});
        add_segment(e0, e3);
        break;
    case 15:
        add_polygon({p0, p1, p2, p3});
        break;
    default:
        break;
    }

    return result;
}

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

void RenderWorldTexturePolygon(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    SDL_Texture* texture,
    const SDL_FRect& src,
    const Vec2& tile_world,
    const std::vector<Vec2>& local_points,
    const SDL_FColor& color
) {
    if (texture == nullptr || local_points.size() < 3 || src.w <= 0.0F || src.h <= 0.0F) {
        return;
    }

    float texture_width = 0.0F;
    float texture_height = 0.0F;
    if (!SDL_GetTextureSize(texture, &texture_width, &texture_height) ||
        texture_width <= 0.0F || texture_height <= 0.0F) {
        return;
    }

    std::vector<SDL_Vertex> vertices;
    vertices.reserve(local_points.size());
    for (const Vec2& local : local_points) {
        const Vec2 screen = WorldPointToScreenForGeometry(graphics, tile_world + local);
        const float u =
            (src.x + (std::clamp(local.x, 0.0F, static_cast<float>(kTileSize)) /
                      static_cast<float>(kTileSize)) *
                         src.w) /
            texture_width;
        const float v =
            (src.y + (std::clamp(local.y, 0.0F, static_cast<float>(kTileSize)) /
                      static_cast<float>(kTileSize)) *
                         src.h) /
            texture_height;
        vertices.push_back(SDL_Vertex{
            SDL_FPoint{screen.x, screen.y},
            color,
            SDL_FPoint{u, v},
        });
    }

    std::vector<int> indices;
    indices.reserve((local_points.size() - 2) * 3);
    for (std::size_t i = 1; i + 1 < local_points.size(); ++i) {
        indices.push_back(0);
        indices.push_back(static_cast<int>(i));
        indices.push_back(static_cast<int>(i + 1));
    }
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
    constexpr std::uint8_t kFluidAlpha = 176;
    constexpr std::uint8_t kFluidTopAlpha = 224;
    constexpr int kTileSizePx = static_cast<int>(kTileSize);
    constexpr float kWaterTopRaisePx = 1.0F;

    struct FluidRenderCell {
        Tile visible_tile = Tile::Air;
        float liquid_level = 0.0F;
        float visible_level = 0.0F;
        float display_level = 0.0F;
        float opacity = 1.0F;
        bool terrain_solid = false;
        bool has_liquid = false;
        bool has_visible_liquid = false;
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
    auto sample_fluid_signed_distance = [&](int vertex_x, int vertex_y) -> float {
        float best_signed_distance = -static_cast<float>(kTileSizePx);
        for (int adjacent_y = vertex_y - 1; adjacent_y <= vertex_y; ++adjacent_y) {
            for (int adjacent_x = vertex_x - 1; adjacent_x <= vertex_x; ++adjacent_x) {
                const FluidRenderCell* const adjacent_cell =
                    const_cell_at(adjacent_x, adjacent_y);
                if (adjacent_cell == nullptr || !adjacent_cell->has_visible_liquid ||
                    adjacent_cell->terrain_solid) {
                    continue;
                }

                const float cell_surface_world_y =
                    (static_cast<float>(adjacent_y) * static_cast<float>(kTileSizePx)) +
                    (static_cast<float>(kTileSizePx) *
                     (1.0F - std::clamp(adjacent_cell->display_level, 0.0F, 1.0F)));
                const float vertex_world_y =
                    static_cast<float>(vertex_y) * static_cast<float>(kTileSizePx);
                best_signed_distance = std::max(
                    best_signed_distance,
                    vertex_world_y - cell_surface_world_y
                );
            }
        }
        return best_signed_distance;
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
            const std::uint8_t amount = state.stage.GetFluidAmount(
                static_cast<unsigned int>(x),
                static_cast<unsigned int>(y)
            );
            cell.has_liquid = GetTileArchetype(fluid_tile).simulated_fluid && amount > 0;
            if (!cell.has_liquid) {
                continue;
            }
            cell.visible_tile = fluid_tile;
            cell.liquid_level = static_cast<float>(amount) / 255.0F;
            cell.visible_level = cell.liquid_level;
            cell.display_level = cell.visible_level;
            cell.has_visible_liquid = true;
        }
    }

    for (int y = 0; y < stage_tile_height; ++y) {
        for (int x = 0; x < stage_tile_width; ++x) {
            FluidRenderCell* const cell = cell_at(x, y);
            if (cell == nullptr || !cell->has_visible_liquid || cell->terrain_solid) {
                continue;
            }

            float weighted_level = 0.0F;
            float total_weight = 0.0F;
            for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                    const FluidRenderCell* const nearby =
                        const_cell_at(x + offset_x, y + offset_y);
                    if (nearby == nullptr || nearby->terrain_solid) {
                        continue;
                    }
                    const bool center = offset_x == 0 && offset_y == 0;
                    const bool diagonal = offset_x != 0 && offset_y != 0;
                    const float weight = center ? 4.0F : (diagonal ? 1.0F : 2.0F);
                    weighted_level += nearby->visible_level * weight;
                    total_weight += weight;
                }
            }

            if (total_weight > 0.0F) {
                cell->display_level = std::clamp(
                    (cell->visible_level + (weighted_level / total_weight)) * 0.5F,
                    0.0F,
                    1.0F
                );
            }
        }
    }

    for (const Vec2& render_offset : render_offsets) {
        for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
            for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
                const auto tile_x = static_cast<unsigned int>(x);
                const auto tile_y = static_cast<unsigned int>(y);
                const int int_x = static_cast<int>(x);
                const int int_y = static_cast<int>(y);
                const FluidRenderCell* const cell = const_cell_at(int_x, int_y);
                if (cell == nullptr || !cell->has_visible_liquid || cell->terrain_solid) {
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
                const float brightness = GetTileArchetype(state.stage.GetTile(tile_x, tile_y)).solid
                    ? GetForegroundBrightnessForRender(
                          state,
                          static_cast<int>(x),
                          static_cast<int>(y)
                      )
                    : 1.0F;

                const SDL_FRect body_src{
                    static_cast<float>(tile_source_data->sample_rect.x),
                    static_cast<float>(tile_source_data->sample_rect.y),
                    static_cast<float>(tile_source_data->sample_rect.w),
                    static_cast<float>(tile_source_data->sample_rect.h),
                };
                const std::uint8_t body_alpha = static_cast<std::uint8_t>(
                    std::clamp(
                        static_cast<int>(std::round(
                            static_cast<float>(kFluidAlpha) * std::clamp(cell->opacity, 0.0F, 1.0F)
                        )),
                        0,
                        static_cast<int>(kFluidAlpha)
                    )
                );

                const std::array<FluidMarchingVertex, 4> samples{
                    FluidMarchingVertex{
                        .local = Vec2::New(0.0F, 0.0F),
                        .signed_distance = sample_fluid_signed_distance(int_x, int_y),
                    },
                    FluidMarchingVertex{
                        .local = Vec2::New(static_cast<float>(kTileSizePx), 0.0F),
                        .signed_distance = sample_fluid_signed_distance(int_x + 1, int_y),
                    },
                    FluidMarchingVertex{
                        .local = Vec2::New(
                            static_cast<float>(kTileSizePx),
                            static_cast<float>(kTileSizePx)
                        ),
                        .signed_distance = sample_fluid_signed_distance(int_x + 1, int_y + 1),
                    },
                    FluidMarchingVertex{
                        .local = Vec2::New(0.0F, static_cast<float>(kTileSizePx)),
                        .signed_distance = sample_fluid_signed_distance(int_x, int_y + 1),
                    },
                };
                const float center_signed_distance = std::max(
                    -static_cast<float>(kTileSizePx),
                    static_cast<float>(kTileSizePx) *
                        (std::clamp(cell->display_level, 0.0F, 1.0F) - 0.5F)
                );
                const FluidMarchingResult marching_result =
                    BuildFilledFluidMarchingSquares(samples, center_signed_distance);

                for (const std::vector<Vec2>& polygon : marching_result.polygons) {
                    RenderWorldTexturePolygon(
                        renderer,
                        graphics,
                        tile_texture,
                        body_src,
                        tile_world,
                        polygon,
                        MakeFluidVertexColor(brightness, body_alpha)
                    );
                }

                if (top_texture != nullptr) {
                    for (const FluidContourSegment& segment : marching_result.contour_segments) {
                        Vec2 segment_start = segment.a;
                        Vec2 segment_end = segment.b;
                        if (segment_start.x > segment_end.x) {
                            std::swap(segment_start, segment_end);
                        }

                        const Vec2 delta = segment_end - segment_start;
                        if (std::abs(delta.x) < std::abs(delta.y) * 0.35F) {
                            continue;
                        }
                        RenderWorldTextureRibbon(
                            renderer,
                            graphics,
                            top_texture,
                            top_src,
                            tile_world + segment_start + Vec2::New(0.0F, -kWaterTopRaisePx),
                            tile_world + segment_end + Vec2::New(0.0F, -kWaterTopRaisePx),
                            top_src.h,
                            false,
                            MakeFluidVertexColor(brightness, kFluidTopAlpha)
                        );
                    }
                }
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
