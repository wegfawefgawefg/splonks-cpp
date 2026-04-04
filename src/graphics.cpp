#include "graphics.hpp"

#include <SDL3_ttf/SDL_ttf.h>

namespace splonks {

Graphics Graphics::New(const std::string& sprite_assets_folder) {
    Graphics graphics;
    graphics.sprites = LoadSprites(sprite_assets_folder);
    graphics.window_dims = UVec2::New(1280, 720);
    graphics.dims = UVec2::New(1280, 720);
    graphics.fullscreen = false;

    const Vec2 screen_center = ToVec2(graphics.window_dims / 2U);
    graphics.camera.target = Vec2::New(0.0F, 0.0F);
    graphics.camera.offset = screen_center;
    graphics.camera.rotation = 0.0F;
    graphics.camera.zoom = 3.0F;

    graphics.play_cam.pos = Vec2::New(0.0F, 0.0F);
    graphics.play_cam.vel = Vec2::New(0.0F, 0.0F);
    graphics.play_cam.acc = Vec2::New(0.0F, 0.0F);
    return graphics;
}

const SpriteData& Graphics::GetSpriteData(Sprite sprite) const {
    return sprites[static_cast<std::size_t>(sprite)];
}

Vec2 Graphics::ScreenToWc(const UVec2& screen_pos) const {
    Vec2 screen = ToVec2(screen_pos);
    const Vec2 screen_center = ToVec2(window_dims) / 2.0F;
    screen = screen - screen_center;
    screen = screen / camera.zoom;
    return screen + camera.target;
}

IVec2 Graphics::ScreenToTileCoords(const UVec2& screen_pos) const {
    return ToIVec2(ScreenToWc(screen_pos)) / static_cast<int>(kTileSize);
}

void Graphics::ResetTileVariation(const IVec2& tile_pos) {
    (void)tile_pos;
}

void Graphics::ResetTileVariations() {}

void Graphics::ShutdownText() {
    if (menu_title_font.font != nullptr) {
        TTF_CloseFont(menu_title_font.font);
        menu_title_font = {};
    }
    if (menu_item_font.font != nullptr) {
        TTF_CloseFont(menu_item_font.font);
        menu_item_font = {};
    }
    if (ui_font.font != nullptr) {
        TTF_CloseFont(ui_font.font);
        ui_font = {};
    }
}

bool IsTileTransparent(Tile tile) {
    switch (tile) {
    case Tile::Air:
    case Tile::Ladder:
    case Tile::LadderTop:
    case Tile::Entrance:
    case Tile::Exit:
    case Tile::Spikes:
    case Tile::Rope:
        return true;
    case Tile::Dirt:
    case Tile::Gold:
    case Tile::Block:
        return false;
    }

    return false;
}

std::uint32_t TileNumVariations(Tile tile) {
    switch (tile) {
    case Tile::Dirt:
        return 3;
    case Tile::Air:
        return 2;
    default:
        return 1;
    }
}

UVec2 GetTileTextureSamplePosition(Tile tile) {
    switch (tile) {
    case Tile::Air:
        return UVec2::New(0, 0) * kTileSize;
    case Tile::Dirt:
        return UVec2::New(0, 1) * kTileSize;
    case Tile::Gold:
        return UVec2::New(0, 2) * kTileSize;
    case Tile::Block:
        return UVec2::New(0, 3) * kTileSize;
    case Tile::LadderTop:
        return UVec2::New(0, 4) * kTileSize;
    case Tile::Ladder:
        return UVec2::New(0, 5) * kTileSize;
    case Tile::Spikes:
        return UVec2::New(0, 6) * kTileSize;
    case Tile::Rope:
        return UVec2::New(0, 7) * kTileSize;
    case Tile::Entrance:
        return UVec2::New(0, 8) * kTileSize;
    case Tile::Exit:
        return UVec2::New(0, 9) * kTileSize;
    }

    return UVec2::New(0, 0);
}

int GetReasonableFontScale(const UVec2& dims, TextType text_type) {
    if (dims == UVec2::New(160, 144)) {
        switch (text_type) {
        case TextType::MenuTitle:
            return static_cast<int>(60U * dims.y / 720U);
        case TextType::MenuItem:
            return static_cast<int>(40U * dims.y / 720U);
        }
    }

    switch (text_type) {
    case TextType::MenuTitle:
        return static_cast<int>(100U * dims.y / 720U);
    case TextType::MenuItem:
        return static_cast<int>(60U * dims.y / 720U);
    }

    return 0;
}

} // namespace splonks
