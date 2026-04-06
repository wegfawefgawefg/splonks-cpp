#pragma once

#include "frame_data.hpp"
#include "sprite.hpp"
#include "tile.hpp"
#include "tile_source_data.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct TTF_Font;
struct SDL_Renderer;
struct SDL_Texture;

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
    std::vector<SDL_Texture*> sprite_textures;
    std::vector<bool> sprite_uses_fallback;
    std::vector<SDL_Texture*> textures;
    FrameDataDb frame_data_db;
    TileSourceDb tile_source_db;
    std::vector<SDL_Texture*> tile_source_images;
    SDL_Texture* special_effects_texture = nullptr;
    std::unordered_map<std::uint64_t, std::uint32_t> tile_variations_cache;
    std::string font_path = "assets/fonts/DejaVuSans.ttf";
    LoadedFont menu_title_font;
    LoadedFont menu_item_font;
    LoadedFont ui_font;

    static Graphics New(SDL_Renderer* renderer, const std::string& sprite_assets_folder);
    const SpriteData& GetSpriteData(Sprite sprite) const;
    SDL_Texture* GetSpriteTexture(Sprite sprite) const;
    bool SpriteUsesFallback(Sprite sprite) const;
    SDL_Texture* GetTexture(TextureName texture) const;
    Vec2 ScreenToWc(const UVec2& screen_pos) const;
    IVec2 ScreenToTileCoords(const UVec2& screen_pos) const;
    void ResetTileVariation(const IVec2& tile_pos);
    void ResetTileVariations();
    void ShutdownText();
    void ShutdownTextures();
};

bool IsTileTransparent(Tile tile);

enum class TextType {
    MenuTitle,
    MenuItem,
};

int GetReasonableFontScale(const UVec2& dims, TextType text_type);

} // namespace splonks
