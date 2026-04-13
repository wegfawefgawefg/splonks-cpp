#pragma once

#include "frame_data.hpp"
#include "tile.hpp"
#include "tile_source_data.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct TTF_Font;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_FRect;

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
    UVec2 window_dims = UVec2::New(1920, 1080);
    UVec2 dims = UVec2::New(1920, 1080);
    bool fullscreen = true;
    bool gpu_renderer_active = false;
    bool debug_lock_play_camera = false;
    Vec2 debug_baseball_bat_hold_offset = Vec2::New(5.0F, -10.0F);
    Camera2D camera;
    PlayCam play_cam;
    std::vector<SDL_Texture*> textures;
    FrameDataDb frame_data_db;
    std::vector<SDL_Texture*> frame_data_images;
    TileSourceDb tile_source_db;
    std::unordered_map<std::uint64_t, std::uint32_t> tile_variations_cache;
    std::string font_path = "assets/fonts/DejaVuSans.ttf";
    LoadedFont menu_title_font;
    LoadedFont menu_item_font;
    LoadedFont ui_font;

    static Graphics New(SDL_Renderer* renderer, const std::string& sprite_assets_folder);
    SDL_Texture* GetTexture(TextureName texture) const;
    SDL_Texture* GetFrameDataTexture(std::uint32_t image_id) const;
    Vec2 ScreenToWc(const UVec2& screen_pos) const;
    IVec2 ScreenToTileCoords(const UVec2& screen_pos) const;
    void ResetTileVariation(const IVec2& tile_pos);
    void ResetTileVariations();
    void ShutdownText();
    void ShutdownTextures();
};

SDL_FRect GetPresentationRect(const Graphics& graphics, int output_width, int output_height);

bool IsTileTransparent(Tile tile);

enum class TextType {
    MenuTitle,
    MenuItem,
};

int GetReasonableFontScale(const UVec2& dims, TextType text_type);

} // namespace splonks
