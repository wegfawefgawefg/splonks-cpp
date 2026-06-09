#include "render/tiles_and_ents.hpp"

#include "ent/spec.hpp"
#include "ent.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "render/color.hpp"
#include "render/tile_lighting.hpp"
#include "render/world_wrap.hpp"
#include "render/world_texture.hpp"
#include "state.hpp"
#include "stage_lighting.hpp"
#include "tile.hpp"
#include "tile_spec.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace splonks {

namespace {

enum class TileCapSide {
    Top,
    Bottom,
    Left,
    Right,
};

struct TileCapSourceData {
    const AFrame* top = nullptr;
    const AFrame* bottom = nullptr;
    const AFrame* left = nullptr;
};

enum class ForegroundTileRenderPass {
    PreEnt,
    PostEnt,
};

std::optional<IVec2> ResolveWrappedTileCoord(const Stage& stage, int tile_x, int tile_y) {
    const IVec2 wrapped = stage.WrapTileCoord(IVec2::New(tile_x, tile_y));
    if (!stage.IsTileCoordInside(wrapped.x, wrapped.y)) {
        return std::nullopt;
    }
    return wrapped;
}

float Dot(const FVec2& left, const FVec2& right) {
    return (left.x * right.x) + (left.y * right.y);
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

bool IsSolidTileForCap(const Stage& stage, int tile_x, int tile_y) {
    return GetTileSpec(stage.GetTileOrBorder(tile_x, tile_y)).solid;
}

bool ShouldRenderTileCap(const Stage& stage, int tile_x, int tile_y, TileCapSide side) {
    if (!IsSolidTileForCap(stage, tile_x, tile_y)) {
        return false;
    }

    IVec2 neighbor_delta = IVec2::New(0, 0);
    switch (side) {
    case TileCapSide::Top:
        neighbor_delta = IVec2::New(0, -1);
        break;
    case TileCapSide::Bottom:
        neighbor_delta = IVec2::New(0, 1);
        break;
    case TileCapSide::Left:
        neighbor_delta = IVec2::New(-1, 0);
        break;
    case TileCapSide::Right:
        neighbor_delta = IVec2::New(1, 0);
        break;
    }

    return !IsSolidTileForCap(stage, tile_x + neighbor_delta.x, tile_y + neighbor_delta.y);
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
        resolved_x = PositiveModulo(resolved_x, width);
    } else {
        resolved_x = std::clamp(resolved_x, 0, width - 1);
    }
    if (stage.WrapsY()) {
        resolved_y = PositiveModulo(resolved_y, height);
    } else {
        resolved_y = std::clamp(resolved_y, 0, height - 1);
    }

    return stage.GetForegroundTileShake(
        static_cast<unsigned int>(resolved_x),
        static_cast<unsigned int>(resolved_y)
    );
}

float GetTileCapShake(const Stage& stage, int tile_x, int tile_y) {
    if (!stage.IsTileCoordInside(tile_x, tile_y)) {
        return GetBorderTileShake(stage, tile_x, tile_y);
    }
    return stage.GetForegroundTileShake(
        static_cast<unsigned int>(tile_x),
        static_cast<unsigned int>(tile_y)
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

const AFrame* GetAnimFrameForTick(
    const AFrameDb& aframe_db,
    AFrameId anim_id,
    std::uint64_t tick
) {
    const AFrameAnim* const anim = aframe_db.FindAnim(anim_id);
    if (anim == nullptr || anim->frame_indices.empty()) {
        return nullptr;
    }

    std::uint64_t total_duration = 0;
    for (const std::size_t frame_index : anim->frame_indices) {
        total_duration += static_cast<std::uint64_t>(
            std::max(aframe_db.frames[frame_index].duration, 1)
        );
    }
    if (total_duration == 0) {
        return nullptr;
    }

    std::uint64_t local_tick = tick % total_duration;
    for (const std::size_t frame_index : anim->frame_indices) {
        const AFrame& aframe = aframe_db.frames[frame_index];
        const std::uint64_t duration = static_cast<std::uint64_t>(std::max(aframe.duration, 1));
        if (local_tick < duration) {
            return &aframe;
        }
        local_tick -= duration;
    }

    return &aframe_db.frames[anim->frame_indices.front()];
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
    const AFrame* const top_frame = GetAnimFrameForTick(
        graphics.aframe_db,
        HashAFrameIdConstexpr("watertop"),
        tick
    );
    if (top_frame != nullptr) {
        top_source_data.image_id = top_frame->image_id;
        top_source_data.sample_rect = top_frame->sample_rect;
        top_source_data.cbox = top_frame->cbox;
    }
    return top_source_data;
}

FVec2 WorldPointToScreenForGeometry(const Graphics& graphics, const FVec2& world_pos) {
    FVec2 screen = WorldToScreen(graphics, world_pos);
    if (!graphics.world_rotation_active) {
        return screen;
    }

    const FVec2 pivot_screen = WorldToScreen(graphics, graphics.world_rotation_pivot);
    constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
    const double radians = static_cast<double>(graphics.world_rotation_degrees) * kDegreesToRadians;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    const FVec2 delta = screen - pivot_screen;
    screen = pivot_screen + FVec2::New(
        static_cast<float>((static_cast<double>(delta.x) * c) - (static_cast<double>(delta.y) * s)),
        static_cast<float>((static_cast<double>(delta.x) * s) + (static_cast<double>(delta.y) * c))
    );
    return screen;
}

SDL_FColor MakeFluidVertexColor(Color3 brightness, std::uint8_t alpha) {
    const Color3 factor = ClampRenderColor(brightness);
    return SDL_FColor{
        factor.r,
        factor.g,
        factor.b,
        static_cast<float>(alpha) / 255.0F,
    };
}

struct FluidContourSegment {
    FVec2 a;
    FVec2 b;
};

void RenderWorldTextureQuad(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    SDL_Texture* texture,
    const SDL_FRect& src,
    const std::array<FVec2, 4>& world_points,
    const std::array<SDL_FColor, 4>& colors
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

    const FVec2 tl = WorldPointToScreenForGeometry(graphics, world_points[0]);
    const FVec2 tr = WorldPointToScreenForGeometry(graphics, world_points[1]);
    const FVec2 br = WorldPointToScreenForGeometry(graphics, world_points[2]);
    const FVec2 bl = WorldPointToScreenForGeometry(graphics, world_points[3]);

    const float u0 = src.x / texture_width;
    const float v0 = src.y / texture_height;
    const float u1 = (src.x + src.w) / texture_width;
    const float v1 = (src.y + src.h) / texture_height;
    const std::array<SDL_Vertex, 4> vertices{
        SDL_Vertex{SDL_FPoint{tl.x, tl.y}, colors[0], SDL_FPoint{u0, v0}},
        SDL_Vertex{SDL_FPoint{tr.x, tr.y}, colors[1], SDL_FPoint{u1, v0}},
        SDL_Vertex{SDL_FPoint{br.x, br.y}, colors[2], SDL_FPoint{u1, v1}},
        SDL_Vertex{SDL_FPoint{bl.x, bl.y}, colors[3], SDL_FPoint{u0, v1}},
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

void RenderWorldTextureQuad(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    SDL_Texture* texture,
    const SDL_FRect& src,
    const std::array<FVec2, 4>& world_points,
    const SDL_FColor& color
) {
    RenderWorldTextureQuad(
        renderer,
        graphics,
        texture,
        src,
        world_points,
        std::array<SDL_FColor, 4>{color, color, color, color}
    );
}

std::array<SDL_FColor, 4> MakeFluidVertexColorsForWorldQuad(
    const State& state,
    const FluidSettings& fluid,
    const std::array<FVec2, 4>& world_points,
    std::uint8_t alpha
) {
    std::array<SDL_FColor, 4> colors{};
    for (std::size_t i = 0; i < world_points.size(); ++i) {
        Color3 brightness = Color3::White();
        if (fluid.lighting_enabled) {
            const Color3 sampled_brightness = SampleBackwallLightColorForRender(state, world_points[i]);
            brightness = ClampRenderColor(
                LerpRenderColor(
                    Color3::White(),
                    sampled_brightness,
                    std::clamp(ToFloat(fluid.lighting_strength), 0.0F, 2.0F)
                )
            );
        }
        colors[i] = MakeFluidVertexColor(brightness, alpha);
    }
    return colors;
}

void RenderWorldTextureRibbon(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    SDL_Texture* texture,
    const SDL_FRect& src,
    const FVec2& start,
    const FVec2& end,
    float thickness,
    bool use_right_normal,
    const SDL_FColor& color
) {
    const FVec2 delta = end - start;
    const float length = std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
    if (length <= 0.01F || thickness <= 0.0F) {
        return;
    }

    const FVec2 dir = delta / length;
    const FVec2 normal = use_right_normal
        ? FVec2::New(dir.y, -dir.x)
        : FVec2::New(-dir.y, dir.x);
    const float max_piece_len = std::max(src.w, 1.0F);
    float consumed = 0.0F;
    while (consumed < length - 0.01F) {
        const float piece_len = std::min(max_piece_len, length - consumed);
        const float t0 = consumed / length;
        const float t1 = (consumed + piece_len) / length;
        const FVec2 piece_start = start + (delta * t0);
        const FVec2 piece_end = start + (delta * t1);
        SDL_FRect piece_src = src;
        piece_src.w = src.w * (piece_len / max_piece_len);
        RenderWorldTextureQuad(
            renderer,
            graphics,
            texture,
            piece_src,
            std::array<FVec2, 4>{
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
    FVec2 start = FVec2::New(0.0F, 0.0F);
    FVec2 end = FVec2::New(0.0F, 0.0F);
    bool use_right_normal = false;
};

RibbonPlacement MakeCenteredContourRibbonPlacement(
    const FluidContourSegment& segment,
    bool tile_center_is_inside_fluid,
    float thickness
) {
    const FVec2 delta = segment.b - segment.a;
    const FVec2 left_normal = NormalizeOrZero(FVec2::New(-delta.y, delta.x));
    const FVec2 right_normal = FVec2::New(left_normal.x * -1.0F, left_normal.y * -1.0F);
    const FVec2 midpoint = (segment.a + segment.b) * 0.5F;
    FVec2 desired_normal = NormalizeOrZero(
        FVec2::New(static_cast<float>(kTileSize) * 0.5F, static_cast<float>(kTileSize) * 0.5F) -
        midpoint
    );
    if (!tile_center_is_inside_fluid) {
        desired_normal = desired_normal * -1.0F;
    }
    if (Length(desired_normal) <= 0.001F) {
        desired_normal = left_normal;
    }

    const bool use_right_normal = Dot(right_normal, desired_normal) > Dot(left_normal, desired_normal);
    const FVec2 ribbon_normal = use_right_normal ? right_normal : left_normal;
    const FVec2 centered_offset = ribbon_normal * (thickness * -0.5F);
    return RibbonPlacement{
        .start = segment.a + centered_offset,
        .end = segment.b + centered_offset,
        .use_right_normal = use_right_normal,
    };
}

void RenderFluidFlowIndicator(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    const FVec2& tile_world,
    const FVec2& velocity,
    std::uint64_t tick,
    float opacity
) {
    const float speed = Length(velocity);
    if (speed <= 0.01F) {
        return;
    }

    const FVec2 direction = NormalizeOrZero(velocity);
    const float phase = std::fmod(
        (static_cast<float>(tick % 10000ULL) * (0.015F + (speed * 0.06F))),
        1.0F
    );
    const FVec2 point =
        tile_world +
        FVec2::New(static_cast<float>(kTileSize) * 0.5F, static_cast<float>(kTileSize) * 0.5F) +
        direction * ((phase - 0.5F) * static_cast<float>(kTileSize) * 0.8F);
    const FVec2 point_screen = WorldPointToScreenForGeometry(graphics, point);

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
    const FVec2& tile_world,
    int tile_x,
    int tile_y,
    std::uint64_t scene_frame,
    float display_level,
    float visible_cutoff,
    std::uint8_t alpha,
    Color3 brightness
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

    const AFrame* const bubble_frame = GetAnimFrameForTick(
        graphics.aframe_db,
        HashAFrameIdConstexpr("bubble"),
        scene_frame + static_cast<std::uint64_t>(seed & 31U)
    );
    if (bubble_frame == nullptr) {
        return;
    }
    SDL_Texture* const bubble_texture = graphics.GetAFrameTexture(bubble_frame->image_id);
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
    const FVec2 bubble_world =
        tile_world +
        FVec2::New(
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
        FVec2::New(
            static_cast<float>(bubble_frame->sample_rect.w),
            static_cast<float>(bubble_frame->sample_rect.h)
        )
    );
    SDL_SetTextureColorModFloat(
        bubble_texture,
        std::clamp(brightness.r, 0.0F, 2.0F),
        std::clamp(brightness.g, 0.0F, 2.0F),
        std::clamp(brightness.b, 0.0F, 2.0F)
    );
    SDL_SetTextureAlphaMod(bubble_texture, alpha);
    RenderWorldTexture(renderer, graphics, bubble_texture, &src, dst);
    SDL_SetTextureAlphaMod(bubble_texture, 255);
    SDL_SetTextureColorModFloat(bubble_texture, 1.0F, 1.0F, 1.0F);
}

FVec2 GetShakeOffset(float shake_pixels) {
    if (shake_pixels <= 0.0F) {
        return FVec2::New(0.0F, 0.0F);
    }

    return FVec2::New(
        rng::RandomFloat(-shake_pixels, shake_pixels),
        rng::RandomFloat(-shake_pixels, shake_pixels)
    );
}

bool ShouldRenderBackgroundStamp(const State& state, const BackgroundStamp& stamp) {
    switch (stamp.condition) {
    case BackgroundStampCondition::None:
        return true;
    case BackgroundStampCondition::Wanted:
        for (const Ent& ent : state.ents.ents) {
            if (ent.active && ent.wanted) {
                return true;
            }
        }
        return false;
    }

    return true;
}

bool ShouldRenderForegroundTileInPass(Tile tile, ForegroundTileRenderPass pass) {
    const TileSpec& spec = GetTileSpec(tile);
    const bool pre_ent_tile = spec.climbable && !spec.solid;
    switch (pass) {
    case ForegroundTileRenderPass::PreEnt:
        return pre_ent_tile;
    case ForegroundTileRenderPass::PostEnt:
        return !pre_ent_tile;
    }
    return true;
}

bool ShouldRevealEmbeddedTreasure(const State& state) {
    for (const Ent& ent : state.ents.ents) {
        if (!ent.active) {
            continue;
        }
        if (CanRevealEmbeddedTreasure(ent)) {
            return true;
        }
    }
    return false;
}

const AFrame* GetFirstFrameForAnimOrFallback(
    const Graphics& graphics,
    AFrameId anim_id
) {
    const AFrameAnim* anim = graphics.aframe_db.FindAnim(anim_id);
    if (anim == nullptr || anim->frame_indices.empty()) {
        anim = graphics.aframe_db.FindAnim(aframe_ids::NoSprite);
        if (anim == nullptr || anim->frame_indices.empty()) {
            return nullptr;
        }
    }
    return &graphics.aframe_db.frames[anim->frame_indices[0]];
}

const AFrame* GetFirstFrameForAnim(
    const Graphics& graphics,
    AFrameId anim_id
) {
    const AFrameAnim* anim = graphics.aframe_db.FindAnim(anim_id);
    if (anim == nullptr || anim->frame_indices.empty()) {
        return nullptr;
    }
    return &graphics.aframe_db.frames[anim->frame_indices[0]];
}

TileCapSourceData GetTileCapSourceData(const Graphics& graphics) {
    return TileCapSourceData{
        .top = GetFirstFrameForAnim(graphics, HashAFrameIdConstexpr("dirt_cap_top")),
        .bottom = GetFirstFrameForAnim(graphics, HashAFrameIdConstexpr("dirt_cap_bottom")),
        .left = GetFirstFrameForAnim(graphics, HashAFrameIdConstexpr("dirt_cap_left")),
    };
}

const AFrame* GetTileCapAFrame(const TileCapSourceData& source_data, TileCapSide side) {
    switch (side) {
    case TileCapSide::Top:
        return source_data.top;
    case TileCapSide::Bottom:
        return source_data.bottom;
    case TileCapSide::Left:
    case TileCapSide::Right:
        return source_data.left;
    }
    return nullptr;
}

FVec2 GetTileCapWorldPos(const AFrame& aframe, int tile_x, int tile_y, TileCapSide side) {
    FVec2 pos = FVec2::New(
        static_cast<float>(tile_x * static_cast<int>(kTileSize)),
        static_cast<float>(tile_y * static_cast<int>(kTileSize))
    );

    switch (side) {
    case TileCapSide::Top:
        pos.y -= static_cast<float>(aframe.sample_rect.h);
        return pos;
    case TileCapSide::Left:
        pos.x -= static_cast<float>(aframe.sample_rect.w);
        return pos;
    case TileCapSide::Bottom:
        pos.y += static_cast<float>(kTileSize);
        return pos;
    case TileCapSide::Right:
        pos.x += static_cast<float>(kTileSize);
        return pos;
    }
    return pos;
}

bool RenderTileCapWithVertexLighting(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const State& state,
    const Graphics& graphics,
    const SDL_FRect& src,
    const SDL_FRect& dst,
    const FVec2& cap_world_pos,
    const FVec2& cap_size,
    bool horizontal_flip
) {
    if (renderer == nullptr || texture == nullptr || graphics.world_rotation_active ||
        src.w <= 0.0F || src.h <= 0.0F || dst.w <= 0.0F || dst.h <= 0.0F ||
        cap_size.x <= 0.0F || cap_size.y <= 0.0F) {
        return false;
    }
    if (!state.settings.post_process.terrain_lighting ||
        !state.settings.post_process.terrain_exposure_lighting) {
        return false;
    }

    float texture_width = 0.0F;
    float texture_height = 0.0F;
    if (!SDL_GetTextureSize(texture, &texture_width, &texture_height) ||
        texture_width <= 0.0F || texture_height <= 0.0F) {
        return false;
    }

    float u0 = src.x / texture_width;
    float u1 = (src.x + src.w) / texture_width;
    if (horizontal_flip) {
        std::swap(u0, u1);
    }
    const float v0 = src.y / texture_height;
    const float v1 = (src.y + src.h) / texture_height;

    const Color3 top_left = SampleForegroundLightColorForRender(state, cap_world_pos);
    const Color3 top_right =
        SampleForegroundLightColorForRender(state, cap_world_pos + FVec2::New(cap_size.x, 0.0F));
    const Color3 bottom_right = SampleForegroundLightColorForRender(state, cap_world_pos + cap_size);
    const Color3 bottom_left =
        SampleForegroundLightColorForRender(state, cap_world_pos + FVec2::New(0.0F, cap_size.y));

    const std::array<SDL_Vertex, 4> vertices{
        SDL_Vertex{SDL_FPoint{dst.x, dst.y}, MakeFluidVertexColor(top_left, 255), SDL_FPoint{u0, v0}},
        SDL_Vertex{
            SDL_FPoint{dst.x + dst.w, dst.y},
            MakeFluidVertexColor(top_right, 255),
            SDL_FPoint{u1, v0},
        },
        SDL_Vertex{
            SDL_FPoint{dst.x + dst.w, dst.y + dst.h},
            MakeFluidVertexColor(bottom_right, 255),
            SDL_FPoint{u1, v1},
        },
        SDL_Vertex{
            SDL_FPoint{dst.x, dst.y + dst.h},
            MakeFluidVertexColor(bottom_left, 255),
            SDL_FPoint{u0, v1},
        },
    };
    constexpr std::array<int, 6> indices{0, 1, 2, 0, 2, 3};
    SDL_SetTextureColorModFloat(texture, 1.0F, 1.0F, 1.0F);
    SDL_RenderGeometry(
        renderer,
        texture,
        vertices.data(),
        static_cast<int>(vertices.size()),
        indices.data(),
        static_cast<int>(indices.size())
    );
    return true;
}

void RenderTileCap(
    SDL_Renderer* renderer,
    State& state,
    Graphics& graphics,
    const TileCapSourceData& source_data,
    int tile_x,
    int tile_y,
    TileCapSide side,
    const FVec2& render_offset
) {
    if (!ShouldRenderTileCap(state.stage, tile_x, tile_y, side)) {
        return;
    }

    const AFrame* const aframe = GetTileCapAFrame(source_data, side);
    if (aframe == nullptr) {
        return;
    }
    SDL_Texture* const texture = graphics.GetAFrameTexture(aframe->image_id);
    if (texture == nullptr) {
        return;
    }

    const SDL_FRect src{
        static_cast<float>(aframe->sample_rect.x),
        static_cast<float>(aframe->sample_rect.y),
        static_cast<float>(aframe->sample_rect.w),
        static_cast<float>(aframe->sample_rect.h),
    };
    const FVec2 cap_size = FVec2::New(
        static_cast<float>(aframe->sample_rect.w),
        static_cast<float>(aframe->sample_rect.h)
    );
    const FVec2 shake_offset = GetShakeOffset(GetTileCapShake(state.stage, tile_x, tile_y));
    const FVec2 cap_world_pos =
        GetTileCapWorldPos(*aframe, tile_x, tile_y, side) + render_offset + shake_offset;
    const SDL_FRect dst = WorldRectToScreen(
        graphics,
        cap_world_pos,
        cap_size
    );

    const bool horizontal_flip = side == TileCapSide::Right;
    if (RenderTileCapWithVertexLighting(
            renderer,
            texture,
            state,
            graphics,
            src,
            dst,
            cap_world_pos,
            cap_size,
            horizontal_flip
        )) {
        return;
    }

    const Color3 brightness = GetForegroundLightColorForRender(state, tile_x, tile_y);
    SDL_SetTextureColorModFloat(texture, brightness.r, brightness.g, brightness.b);
    RenderWorldTextureRotated(
        renderer,
        graphics,
        texture,
        &src,
        dst,
        0.0,
        nullptr,
        horizontal_flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE
    );
    SDL_SetTextureColorModFloat(texture, 1.0F, 1.0F, 1.0F);
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
    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const FVec2& render_offset : render_offsets) {
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
                const FVec2 background_shake_offset = GetShakeOffset(background_shake);
                const SDL_FRect unshaken_dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos) + render_offset,
                    FVec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );
                const SDL_FRect background_dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos) + render_offset + background_shake_offset,
                    FVec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
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
                if (background_shake > 0.0F) {
                    const bool rendered_unshaken_with_vertex_lighting =
                        RenderBackwallTileWithVertexLighting(
                            renderer,
                            backwall_texture,
                            state,
                            graphics,
                            backwall_src,
                            unshaken_dst,
                            ToVec2(tile_pos) + render_offset
                        );
                    if (!rendered_unshaken_with_vertex_lighting) {
                        ApplyBackwallTileBrightness(
                            backwall_texture,
                            state,
                            graphics,
                            static_cast<int>(x),
                            static_cast<int>(y)
                        );
                        RenderWorldTexture(renderer, graphics, backwall_texture, &backwall_src, unshaken_dst);
                        ResetTerrainTileBrightness(backwall_texture);
                    }
                }
                const bool rendered_with_vertex_lighting =
                    RenderBackwallTileWithVertexLighting(
                        renderer,
                        backwall_texture,
                        state,
                        graphics,
                        backwall_src,
                        background_dst,
                        ToVec2(tile_pos) + render_offset
                    );
                if (!rendered_with_vertex_lighting) {
                    ApplyBackwallTileBrightness(
                        backwall_texture,
                        state,
                        graphics,
                        static_cast<int>(x),
                        static_cast<int>(y)
                    );
                    RenderWorldTexture(renderer, graphics, backwall_texture, &backwall_src, background_dst);
                    ResetTerrainTileBrightness(backwall_texture);
                }
            }
        }
    }
}

void RenderStageForegroundTilePass(
    SDL_Renderer* renderer,
    State& state,
    Graphics& graphics,
    ForegroundTileRenderPass pass
) {
    EnsureStageLighting(state);
    state.stage.SyncTileShakeGrid();
    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const FVec2& render_offset : render_offsets) {
        for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
            for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
                const Tile tile = state.stage.tiles[y][x];
                if (tile == Tile::Air) {
                    continue;
                }
                if (!GetTileSpec(tile).render_enabled) {
                    continue;
                }
                if (!ShouldRenderForegroundTileInPass(tile, pass)) {
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
                const FVec2 foreground_shake_offset = GetShakeOffset(foreground_shake);
                const SDL_FRect foreground_dst = WorldRectToScreen(
                    graphics,
                    ToVec2(tile_pos) + render_offset + foreground_shake_offset,
                    FVec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
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

                const TileRotation tile_rotation = state.stage.GetTileRotation(
                    static_cast<unsigned int>(x),
                    static_cast<unsigned int>(y)
                );
                const bool rendered_with_vertex_lighting =
                    RenderTerrainTileWithVertexLighting(
                        renderer,
                        tile_texture,
                        state,
                        graphics,
                        src,
                        foreground_dst,
                        ToVec2(tile_pos),
                        tile_rotation
                    );
                if (!rendered_with_vertex_lighting) {
                    ApplyTerrainTileBrightness(
                        tile_texture,
                        state,
                        graphics,
                        static_cast<int>(x),
                        static_cast<int>(y)
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
                }
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

void RenderStagePreEntForegroundTiles(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    RenderStageForegroundTilePass(renderer, state, graphics, ForegroundTileRenderPass::PreEnt);
}

void RenderStageForegroundTiles(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    RenderStageForegroundTilePass(renderer, state, graphics, ForegroundTileRenderPass::PostEnt);
}

void RenderStageFluids(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureStageLighting(state);
    state.stage.SyncTileInstanceMetadataGrid();
    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    constexpr int kTileSizePx = static_cast<int>(kTileSize);
    constexpr float kMinVisibleDisplayLevel = 0.0001F;
    const FluidSettings& fluid = state.settings.fluid;
    const float min_fluid_display_level =
        std::clamp(ToFloat(fluid.render_cutoff_amount), 0.0F, 1.0F);
    const float effective_display_cutoff =
        std::max(min_fluid_display_level, kMinVisibleDisplayLevel);
    const std::uint8_t body_alpha = static_cast<std::uint8_t>(
        std::clamp(
            static_cast<int>(std::round(
                255.0F * std::clamp(ToFloat(fluid.water_alpha), 0.0F, 1.0F)
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
            cell.terrain_solid = GetTileSpec(
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
                GetTileSpec(fluid_tile).simulated_fluid &&
                amount > kMinVisibleDisplayLevel;
            FxScalar& stored_display_amount =
                state.stage.fluid_display_amount[static_cast<std::size_t>(y)]
                                                [static_cast<std::size_t>(x)];
            float display_amount = ToFloat(stored_display_amount);
            if (!cell.has_liquid) {
                if (fluid.temporal_smoothing_enabled) {
                    const float response = std::clamp(
                        ToFloat(fluid.temporal_smoothing_response),
                        0.0F,
                        1.0F
                    );
                    display_amount += (0.0F - display_amount) * response;
                } else {
                    display_amount = 0.0F;
                }
                if (display_amount <= effective_display_cutoff || cell.terrain_solid) {
                    stored_display_amount = FxScalar::zero();
                    continue;
                }

                Tile residual_visible_tile = Tile::WaterSwim;
                for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                        const Tile nearby_fluid_tile =
                            GetWrappedFluidTile(state.stage, x + offset_x, y + offset_y);
                        if (GetTileSpec(nearby_fluid_tile).simulated_fluid) {
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
                stored_display_amount = ToFxScalar(cell.visible_level);
                cell.display_level = cell.visible_level;
                cell.has_visible_liquid = true;
                cell.render_candidate = true;
                continue;
            }
            cell.visible_tile = fluid_tile;
            cell.liquid_level = std::clamp(amount, 0.0F, 1.0F);
            if (fluid.temporal_smoothing_enabled) {
                const float response = std::clamp(
                    ToFloat(fluid.temporal_smoothing_response),
                    0.0F,
                    1.0F
                );
                display_amount += (cell.liquid_level - display_amount) * response;
            } else {
                display_amount = cell.liquid_level;
            }
            cell.visible_level = std::clamp(display_amount, 0.0F, 1.0F);
            stored_display_amount = ToFxScalar(cell.visible_level);
            cell.display_level = cell.visible_level;
            cell.has_visible_liquid = true;
            cell.render_candidate = true;
        }
    }

    for (const FVec2& render_offset : render_offsets) {
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

                const FVec2 tile_world = ToVec2(tile_pos) + render_offset;
                Color3 brightness = Color3::White();
                if (fluid.lighting_enabled) {
                    const Color3 sampled_brightness = SampleBackwallLightColorForRender(
                        state,
                        tile_world + FVec2::New(
                            static_cast<float>(kTileSizePx) * 0.5F,
                            static_cast<float>(kTileSizePx) * 0.5F
                        )
                    );
                    brightness = ClampRenderColor(
                        LerpRenderColor(
                            Color3::White(),
                            sampled_brightness,
                            std::clamp(ToFloat(fluid.lighting_strength), 0.0F, 2.0F)
                        )
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
                    const FVec2 velocity = ToFVec2(state.stage.fluid_velocity[y][x]);
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
                const std::array<FVec2, 4> body_world_points{
                    tile_world + FVec2::New(0.0F, 0.0F),
                    tile_world + FVec2::New(static_cast<float>(kTileSizePx), 0.0F),
                    tile_world + FVec2::New(
                        static_cast<float>(kTileSizePx),
                        static_cast<float>(kTileSizePx)
                    ),
                    tile_world + FVec2::New(0.0F, static_cast<float>(kTileSizePx)),
                };
                RenderWorldTextureQuad(
                    renderer,
                    graphics,
                    tile_texture,
                    body_src,
                    body_world_points,
                    MakeFluidVertexColorsForWorldQuad(state, fluid, body_world_points, body_alpha)
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
                        .a = FVec2::New(0.0F, 0.0F),
                        .b = FVec2::New(static_cast<float>(kTileSizePx), 0.0F),
                    });
                }
                if (!neighbor_is_fluid_or_solid(1, 0)) {
                    render_cell_edge_topper(FluidContourSegment{
                        .a = FVec2::New(static_cast<float>(kTileSizePx), 0.0F),
                        .b = FVec2::New(
                            static_cast<float>(kTileSizePx),
                            static_cast<float>(kTileSizePx)
                        ),
                    });
                }
                if (!neighbor_is_fluid_or_solid(0, 1)) {
                    render_cell_edge_topper(FluidContourSegment{
                        .a = FVec2::New(0.0F, static_cast<float>(kTileSizePx)),
                        .b = FVec2::New(
                            static_cast<float>(kTileSizePx),
                            static_cast<float>(kTileSizePx)
                        ),
                    });
                }
                if (!neighbor_is_fluid_or_solid(-1, 0)) {
                    render_cell_edge_topper(FluidContourSegment{
                        .a = FVec2::New(0.0F, 0.0F),
                        .b = FVec2::New(0.0F, static_cast<float>(kTileSizePx)),
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

void RenderStageTileCaps(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureStageLighting(state);
    state.stage.SyncTileShakeGrid();

    const TileCapSourceData cap_source_data = GetTileCapSourceData(graphics);
    if (cap_source_data.top == nullptr &&
        cap_source_data.bottom == nullptr &&
        cap_source_data.left == nullptr) {
        return;
    }

    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    constexpr std::array<TileCapSide, 4> kCapSides{
        TileCapSide::Top,
        TileCapSide::Bottom,
        TileCapSide::Left,
        TileCapSide::Right,
    };

    for (const FVec2& render_offset : render_offsets) {
        for (std::size_t y = 0; y < state.stage.tiles.size(); ++y) {
            for (std::size_t x = 0; x < state.stage.tiles[y].size(); ++x) {
                const int tile_x = static_cast<int>(x);
                const int tile_y = static_cast<int>(y);
                for (const TileCapSide side : kCapSides) {
                    RenderTileCap(
                        renderer,
                        state,
                        graphics,
                        cap_source_data,
                        tile_x,
                        tile_y,
                        side,
                        render_offset
                    );
                }
            }
        }
    }

    if (state.stage.WrapsX() && state.stage.WrapsY()) {
        return;
    }

    const VisibleWorldRect visible = GetVisibleWorldRect(graphics);
    const int visible_tl_tile_x =
        static_cast<int>(std::floor(visible.tl.x / static_cast<float>(kTileSize))) - 1;
    const int visible_tl_tile_y =
        static_cast<int>(std::floor(visible.tl.y / static_cast<float>(kTileSize))) - 1;
    const int visible_br_tile_x =
        static_cast<int>(std::ceil(visible.br.x / static_cast<float>(kTileSize))) + 1;
    const int visible_br_tile_y =
        static_cast<int>(std::ceil(visible.br.y / static_cast<float>(kTileSize))) + 1;
    const int stage_tile_width = static_cast<int>(state.stage.GetTileWidth());
    const int stage_tile_height = static_cast<int>(state.stage.GetTileHeight());

    for (int tile_y = visible_tl_tile_y; tile_y <= visible_br_tile_y; ++tile_y) {
        for (int tile_x = visible_tl_tile_x; tile_x <= visible_br_tile_x; ++tile_x) {
            const bool inside_stage =
                tile_x >= 0 && tile_y >= 0 &&
                tile_x < stage_tile_width && tile_y < stage_tile_height;
            if (inside_stage || !IsImmediateBorderRingTile(state.stage, tile_x, tile_y)) {
                continue;
            }

            for (const TileCapSide side : kCapSides) {
                RenderTileCap(
                    renderer,
                    state,
                    graphics,
                    cap_source_data,
                    tile_x,
                    tile_y,
                    side,
                    FVec2::New(0.0F, 0.0F)
                );
            }
        }
    }
}

void RenderStageTileWrapperLayer(
    SDL_Renderer* renderer,
    State& state,
    Graphics& graphics,
    bool render_foreground
) {
    EnsureStageLighting(state);

    const VisibleWorldRect visible = GetVisibleWorldRect(graphics);
    const FVec2 visible_tl_wc = visible.tl;
    const FVec2 visible_br_wc = visible.br;

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
                    if (render_foreground) {
                        continue;
                    }
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
                    const FVec2 border_shake_offset = GetShakeOffset(border_shake);
                    const SDL_FRect dst = WorldRectToScreen(
                        graphics,
                        ToVec2(tile_pos) + border_shake_offset,
                        FVec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                    );
                    ApplyBackwallTileBrightness(air_texture, state, graphics, tile_x, tile_y);
                    RenderWorldTexture(renderer, graphics, air_texture, &air_src, dst);
                    ResetTerrainTileBrightness(air_texture);
                    continue;
                }
                if (!render_foreground) {
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
                const FVec2 border_shake_offset = GetShakeOffset(border_shake);
                if (border_shake > 0.0F &&
                    IsImmediateBorderRingTile(state.stage, tile_x, tile_y)) {
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
                                FVec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
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
                    FVec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize))
                );
                ApplyTerrainTileBrightness(tile_texture, state, graphics, tile_x, tile_y);
                RenderWorldTexture(renderer, graphics, tile_texture, &src, dst);
                ResetTerrainTileBrightness(tile_texture);
                RenderTerrainTileLighting(renderer, state, graphics, tile_x, tile_y, dst);
            }
        }
    }
}

void RenderStageTileWrapper(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    RenderStageTileWrapperLayer(renderer, state, graphics, false);
}

void RenderStageForegroundTileWrapper(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    RenderStageTileWrapperLayer(renderer, state, graphics, true);
}

void RenderBackgroundStamps(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    EnsureStageLighting(state);
    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (const BackgroundStamp& stamp : state.stage.background_stamps) {
        if (stamp.anim_id == kInvalidAFrameId) {
            continue;
        }
        if (!ShouldRenderBackgroundStamp(state, stamp)) {
            continue;
        }

        const AFrame* const aframe =
            GetFirstFrameForAnimOrFallback(graphics, stamp.anim_id);
        if (aframe == nullptr) {
            continue;
        }
        SDL_Texture* const sprite_texture = graphics.GetAFrameTexture(aframe->image_id);
        if (sprite_texture == nullptr) {
            continue;
        }

        const SDL_FRect src{
            static_cast<float>(aframe->sample_rect.x),
            static_cast<float>(aframe->sample_rect.y),
            static_cast<float>(aframe->sample_rect.w),
            static_cast<float>(aframe->sample_rect.h),
        };

        const int tile_x =
            static_cast<int>((stamp.pos.x + (static_cast<float>(aframe->sample_rect.w) * 0.5F)) /
                             static_cast<float>(kTileSize));
        const int tile_y =
            static_cast<int>((stamp.pos.y + (static_cast<float>(aframe->sample_rect.h) * 0.5F)) /
                             static_cast<float>(kTileSize));
        for (const FVec2& render_offset : render_offsets) {
            const SDL_FRect dst = WorldRectToScreen(
                graphics,
                stamp.pos + render_offset,
                FVec2::New(
                    static_cast<float>(aframe->sample_rect.w),
                    static_cast<float>(aframe->sample_rect.h)
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
    const std::vector<FVec2> render_offsets = GetVisibleWrappedRenderOffsets(state.stage, graphics);
    for (std::size_t y = 0; y < state.stage.embedded_treasures.size(); ++y) {
        for (std::size_t x = 0; x < state.stage.embedded_treasures[y].size(); ++x) {
            const EmbeddedTreasure& embedded_treasure = state.stage.embedded_treasures[y][x];
            if (embedded_treasure.IsEmpty()) {
                continue;
            }
            if (!embedded_treasure.IsVisible() && !reveal_hidden_embeds) {
                continue;
            }

            std::optional<AFrameId> overlay_frame = embedded_treasure.GetOverlayFrame();
            if (!overlay_frame.has_value()) {
                for (const EmbeddedTreasureDrop& drop : embedded_treasure.drops) {
                    if (drop.type_ != EntType::None && drop.count > 0) {
                        overlay_frame = GetDefaultAnimIdForSpec(drop.type_);
                        break;
                    }
                }
            }
            if (!overlay_frame.has_value()) {
                continue;
            }

            const AFrame* const aframe = GetFirstFrameForAnim(graphics, *overlay_frame);
            if (aframe == nullptr) {
                continue;
            }

            SDL_Texture* const sprite_texture = graphics.GetAFrameTexture(aframe->image_id);
            if (sprite_texture == nullptr) {
                continue;
            }

            const FVec2 tile_world_pos = FVec2::New(
                static_cast<float>(x * kTileSize),
                static_cast<float>(y * kTileSize)
            );
            const int render_offset_x =
                (static_cast<int>(kTileSize) - aframe->sample_rect.w) / 2;
            const int render_offset_y =
                (static_cast<int>(kTileSize) - aframe->sample_rect.h) / 2;
            const FVec2 render_world_pos = tile_world_pos + FVec2::New(
                static_cast<float>(render_offset_x),
                static_cast<float>(render_offset_y)
            );
            const SDL_FRect src{
                static_cast<float>(aframe->sample_rect.x),
                static_cast<float>(aframe->sample_rect.y),
                static_cast<float>(aframe->sample_rect.w),
                static_cast<float>(aframe->sample_rect.h),
            };
            const FVec2 overlay_center = tile_world_pos + FVec2::New(
                static_cast<float>(kTileSize) * 0.5F,
                static_cast<float>(kTileSize) * 0.5F
            );
            const Color3 brightness = MaxRenderColor(
                SampleForegroundLightColorForRender(state, overlay_center),
                Color3::White(
                    embedded_treasure.IsVisible()
                        ? state.settings.post_process.embedded_treasure_brightness
                        : 0.0F
                )
            );
            SDL_SetTextureColorModFloat(sprite_texture, brightness.r, brightness.g, brightness.b);
            SDL_SetTextureAlphaMod(sprite_texture, 224);
            for (const FVec2& render_offset : render_offsets) {
                const SDL_FRect dst = WorldRectToScreen(
                    graphics,
                    render_world_pos + render_offset,
                    FVec2::New(
                        static_cast<float>(aframe->sample_rect.w),
                        static_cast<float>(aframe->sample_rect.h)
                    )
                );
                RenderWorldTexture(renderer, graphics, sprite_texture, &src, dst);
            }
            SDL_SetTextureAlphaMod(sprite_texture, 255);
            SDL_SetTextureColorModFloat(sprite_texture, 1.0F, 1.0F, 1.0F);
        }
    }
}

} // namespace splonks
