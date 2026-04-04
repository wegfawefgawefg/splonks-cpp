#pragma once

#include "sprite.hpp"
#include "tile.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct TTF_Font;

namespace splonks {

enum class ShaderName {
    Grayscale,
};

enum class TextureName {
    Title,
    TitleLayer1,
    TitleLayer2,
    TitleLayer3,
};

enum class SpriteTexture {
    Entities,
};

enum class TileSet {
    Cave,
    Ice,
    Jungle,
    Temple,
    Boss,
};

struct PlayCam {
    Vec2 pos;
    Vec2 vel;
    Vec2 acc;
};

struct Camera2D {
    Vec2 target;
    Vec2 offset;
    float rotation = 0.0F;
    float zoom = 1.0F;
};

struct LoadedFont {
    TTF_Font* font = nullptr;
    int point_size = 0;
};

struct Graphics {
    UVec2 window_dims = UVec2::New(1280, 720);
    UVec2 dims = UVec2::New(1280, 720);
    bool fullscreen = false;
    Camera2D camera;
    PlayCam play_cam;
    std::vector<SpriteData> sprites;
    std::string font_path = "assets/fonts/DejaVuSans.ttf";
    LoadedFont menu_title_font;
    LoadedFont menu_item_font;
    LoadedFont ui_font;

    static Graphics New(const std::string& sprite_assets_folder);
    const SpriteData& GetSpriteData(Sprite sprite) const;
    Vec2 ScreenToWc(const UVec2& screen_pos) const;
    IVec2 ScreenToTileCoords(const UVec2& screen_pos) const;
    void ResetTileVariation(const IVec2& tile_pos);
    void ResetTileVariations();
    void ShutdownText();
};

bool IsTileTransparent(Tile tile);
std::uint32_t TileNumVariations(Tile tile);
UVec2 GetTileTextureSamplePosition(Tile tile);

enum class TextType {
    MenuTitle,
    MenuItem,
};

int GetReasonableFontScale(const UVec2& dims, TextType text_type);

} // namespace splonks
