#include "graphics.hpp"

#include "raw_frame_data.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace splonks {

namespace {

std::uint64_t TileVariationCacheKey(const IVec2& tile_pos) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(tile_pos.x)) << 32U) |
           static_cast<std::uint32_t>(tile_pos.y);
}

[[noreturn]] void ThrowGraphicsError(const char* message) {
    throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

SDL_Texture* LoadTexture(SDL_Renderer* renderer, const std::string& path) {
    SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
    if (texture == nullptr) {
        ThrowGraphicsError(("IMG_LoadTexture failed for " + path).c_str());
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

SDL_Texture* LoadFrameDataTexture(SDL_Renderer* renderer, const std::string& filename) {
    const std::vector<std::string> candidate_paths = {
        "assets/graphics/sprites/" + filename,
        "assets/graphics/tiles/" + filename,
        "assets/graphics/images/" + filename,
        "assets/graphics/special_effects/" + filename,
    };

    for (const std::string& path : candidate_paths) {
        if (std::filesystem::exists(path)) {
            return LoadTexture(renderer, path);
        }
    }

    throw std::runtime_error("Missing frame data texture for " + filename);
}
} // namespace

Graphics Graphics::New(SDL_Renderer* renderer, const std::string& sprite_assets_folder) {
    (void)sprite_assets_folder;
    Graphics graphics;
    graphics.textures = {
        LoadTexture(renderer, "assets/graphics/images/title.png"),
        LoadTexture(renderer, "assets/graphics/images/title_layer_1.png"),
        LoadTexture(renderer, "assets/graphics/images/title_layer_2.png"),
        LoadTexture(renderer, "assets/graphics/images/title_layer_3.png"),
    };
    const RawFrameDataFile raw_frame_data_file =
        LoadRawFrameDataFile("assets/graphics/annotations.yaml");
    graphics.frame_data_db = FrameDataDb::FromRaw(raw_frame_data_file);
    graphics.frame_data_images.reserve(graphics.frame_data_db.image_paths.size());
    for (const std::string& image_path : graphics.frame_data_db.image_paths) {
        graphics.frame_data_images.push_back(LoadFrameDataTexture(renderer, image_path));
    }
    graphics.tile_source_db = BuildTileSourceDb(graphics.frame_data_db);
    graphics.special_effects_texture =
        LoadTexture(renderer, "assets/graphics/special_effects/special_effects.png");
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

SDL_Texture* Graphics::GetTexture(TextureName texture) const {
    const std::size_t index = static_cast<std::size_t>(texture);
    if (index >= textures.size()) {
        return nullptr;
    }
    return textures[index];
}

SDL_Texture* Graphics::GetFrameDataTexture(std::uint32_t image_id) const {
    const std::size_t index = static_cast<std::size_t>(image_id);
    if (index >= frame_data_images.size()) {
        return nullptr;
    }
    return frame_data_images[index];
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
    tile_variations_cache.erase(TileVariationCacheKey(tile_pos));
}

void Graphics::ResetTileVariations() {
    tile_variations_cache.clear();
}

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

void Graphics::ShutdownTextures() {
    for (SDL_Texture* texture : textures) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
    textures.clear();

    for (SDL_Texture* texture : frame_data_images) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
    frame_data_images.clear();

    if (special_effects_texture != nullptr) {
        SDL_DestroyTexture(special_effects_texture);
        special_effects_texture = nullptr;
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
