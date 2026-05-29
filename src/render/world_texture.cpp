#include "render/world_texture.hpp"

#include "graphics.hpp"

#include <cmath>

namespace splonks {

namespace {

Vec2 WorldToScreenRaw(const Graphics& graphics, const Vec2& world_pos) {
    return ((world_pos - graphics.camera.target) * graphics.camera.zoom) + graphics.camera.offset;
}

} // namespace

Vec2 WorldToScreen(const Graphics& graphics, const Vec2& world_pos) {
    const Vec2 screen = WorldToScreenRaw(graphics, world_pos);
    return Vec2::New(std::round(screen.x), std::round(screen.y));
}

SDL_FRect WorldRectToScreen(const Graphics& graphics, const Vec2& world_pos, const Vec2& world_size) {
    const Vec2 screen_tl = WorldToScreenRaw(graphics, world_pos);
    const Vec2 screen_br = WorldToScreenRaw(graphics, world_pos + world_size);
    const float left = std::floor(std::min(screen_tl.x, screen_br.x));
    const float top = std::floor(std::min(screen_tl.y, screen_br.y));
    const float right = std::ceil(std::max(screen_tl.x, screen_br.x));
    const float bottom = std::ceil(std::max(screen_tl.y, screen_br.y));
    return SDL_FRect{
        left,
        top,
        right - left,
        bottom - top,
    };
}

void RenderWorldTextureRotated(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    SDL_Texture* texture,
    const SDL_FRect* src,
    const SDL_FRect& dst,
    double local_rotation,
    const SDL_FPoint* local_center,
    SDL_FlipMode flip
) {
    if (!graphics.world_rotation_active) {
        SDL_RenderTextureRotated(renderer, texture, src, &dst, local_rotation, local_center, flip);
        return;
    }

    const Vec2 pivot_screen = WorldToScreen(graphics, graphics.world_rotation_pivot);
    const SDL_FPoint rotation_center = local_center != nullptr
        ? *local_center
        : SDL_FPoint{dst.w * 0.5F, dst.h * 0.5F};

    constexpr double kDegreesToRadians = 3.14159265358979323846 / 180.0;
    const double radians = static_cast<double>(graphics.world_rotation_degrees) * kDegreesToRadians;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    const Vec2 sprite_center = Vec2::New(
        dst.x + rotation_center.x,
        dst.y + rotation_center.y
    );
    const Vec2 delta = sprite_center - pivot_screen;
    const Vec2 rotated_sprite_center = pivot_screen + Vec2::New(
        static_cast<float>((static_cast<double>(delta.x) * c) - (static_cast<double>(delta.y) * s)),
        static_cast<float>((static_cast<double>(delta.x) * s) + (static_cast<double>(delta.y) * c))
    );
    const SDL_FRect rotated_dst{
        rotated_sprite_center.x - rotation_center.x,
        rotated_sprite_center.y - rotation_center.y,
        dst.w,
        dst.h,
    };
    SDL_RenderTextureRotated(
        renderer,
        texture,
        src,
        &rotated_dst,
        static_cast<double>(graphics.world_rotation_degrees) + local_rotation,
        &rotation_center,
        flip
    );
}

void RenderWorldTexture(
    SDL_Renderer* renderer,
    const Graphics& graphics,
    SDL_Texture* texture,
    const SDL_FRect* src,
    const SDL_FRect& dst
) {
    RenderWorldTextureRotated(renderer, graphics, texture, src, dst, 0.0, nullptr, SDL_FLIP_NONE);
}

} // namespace splonks
