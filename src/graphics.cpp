#include "graphics.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace splonks {

namespace {

std::uint64_t TileVariationCacheKey(const IVec2& tile_pos) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(tile_pos.x)) << 32U) |
           static_cast<std::uint32_t>(tile_pos.y);
}

const SpriteData& EmptySpriteData() {
    static const SpriteData sprite_data{
        std::vector<Frame>{Frame{UVec2::New(0, 0), 0.0F}},
        UVec2::New(0, 0),
    };
    return sprite_data;
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

SDL_Texture* CreateGeneratedMissingTexture(SDL_Renderer* renderer) {
    SDL_Surface* surface = SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBA8888);
    if (surface == nullptr) {
        ThrowGraphicsError("SDL_CreateSurface failed for generated missing texture");
    }

    SDL_FillSurfaceRect(surface, nullptr, SDL_MapSurfaceRGBA(surface, 0, 0, 0, 0));

    const Uint32 white = SDL_MapSurfaceRGBA(surface, 255, 255, 255, 255);
    const Uint32 red = SDL_MapSurfaceRGBA(surface, 255, 0, 0, 255);
    SDL_Rect top{0, 0, 16, 1};
    SDL_Rect bottom{0, 15, 16, 1};
    SDL_Rect left{0, 0, 1, 16};
    SDL_Rect right{15, 0, 1, 16};
    SDL_FillSurfaceRect(surface, &top, white);
    SDL_FillSurfaceRect(surface, &bottom, white);
    SDL_FillSurfaceRect(surface, &left, white);
    SDL_FillSurfaceRect(surface, &right, white);

    auto* pixels = static_cast<Uint32*>(surface->pixels);
    const std::size_t pitch =
        static_cast<std::size_t>(surface->pitch) / sizeof(Uint32);
    for (std::size_t i = 1; i <= 14; ++i) {
        pixels[i * pitch + i] = red;
        pixels[i * pitch + (15U - i)] = red;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture == nullptr) {
        ThrowGraphicsError("SDL_CreateTextureFromSurface failed for generated missing texture");
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

std::vector<SDL_Texture*> LoadSpriteTextures(
    SDL_Renderer* renderer,
    const std::string& sprite_assets_folder,
    std::vector<bool>& sprite_uses_fallback
) {
    const std::string fallback_texture_path =
        sprite_assets_folder + "/" + std::string(ToFilename(Sprite::NoSprite)) + ".png";
    std::vector<SDL_Texture*> textures;
    textures.reserve(kSpriteCount);
    for (Sprite sprite : AllSprites()) {
        const std::string filename = std::string(ToFilename(sprite));
        const std::string path =
            sprite_assets_folder + "/" + filename + ".png";
        if (std::filesystem::exists(path)) {
            sprite_uses_fallback.push_back(false);
            textures.push_back(LoadTexture(renderer, path));
        } else if (std::filesystem::exists(fallback_texture_path)) {
            std::cerr << "\033[33mwarning:\033[0m Missing sprite texture for " << filename
                      << ". Using " << fallback_texture_path << ".\n";
            sprite_uses_fallback.push_back(true);
            textures.push_back(LoadTexture(renderer, fallback_texture_path));
        } else {
            std::cerr << "\033[33mwarning:\033[0m Missing sprite texture for " << filename
                      << " and fallback texture is absent. Generating placeholder texture.\n";
            sprite_uses_fallback.push_back(true);
            textures.push_back(CreateGeneratedMissingTexture(renderer));
        }
    }
    return textures;
}

} // namespace

Graphics Graphics::New(SDL_Renderer* renderer, const std::string& sprite_assets_folder) {
    Graphics graphics;
    graphics.sprites = LoadSprites(sprite_assets_folder);
    graphics.sprite_uses_fallback.reserve(kSpriteCount);
    graphics.sprite_textures =
        LoadSpriteTextures(renderer, sprite_assets_folder, graphics.sprite_uses_fallback);
    for (std::size_t i = 0; i < graphics.sprites.size() && i < graphics.sprite_uses_fallback.size();
         ++i) {
        const SpriteData& sprite_data = graphics.sprites[i];
        if (sprite_data.frames.size() == 1 && sprite_data.frames[0].sample_position == UVec2::New(0, 0) &&
            sprite_data.size == UVec2::New(8, 8)) {
            graphics.sprite_uses_fallback[i] = true;
        }
    }
    graphics.textures = {
        LoadTexture(renderer, "assets/graphics/textures/title.png"),
        LoadTexture(renderer, "assets/graphics/textures/title_layer_1.png"),
        LoadTexture(renderer, "assets/graphics/textures/title_layer_2.png"),
        LoadTexture(renderer, "assets/graphics/textures/title_layer_3.png"),
    };
    graphics.tile_sets = {
        LoadTexture(renderer, "assets/graphics/textures/cave.png"),
        LoadTexture(renderer, "assets/graphics/textures/ice.png"),
        LoadTexture(renderer, "assets/graphics/textures/jungle.png"),
        LoadTexture(renderer, "assets/graphics/textures/temple.png"),
        LoadTexture(renderer, "assets/graphics/textures/boss.png"),
    };
    graphics.special_effects_texture =
        LoadTexture(renderer, "assets/graphics/textures/special_effects.png");
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
    const std::size_t index = static_cast<std::size_t>(sprite);
    if (index >= sprites.size()) {
        return EmptySpriteData();
    }
    return sprites[index];
}

SDL_Texture* Graphics::GetSpriteTexture(Sprite sprite) const {
    const std::size_t index = static_cast<std::size_t>(sprite);
    if (index >= sprite_textures.size()) {
        return nullptr;
    }
    return sprite_textures[index];
}

bool Graphics::SpriteUsesFallback(Sprite sprite) const {
    const std::size_t index = static_cast<std::size_t>(sprite);
    if (index >= sprite_uses_fallback.size()) {
        return true;
    }
    return sprite_uses_fallback[index];
}

SDL_Texture* Graphics::GetTexture(TextureName texture) const {
    const std::size_t index = static_cast<std::size_t>(texture);
    if (index >= textures.size()) {
        return nullptr;
    }
    return textures[index];
}

SDL_Texture* Graphics::GetTileSetTexture(TileSet tile_set) const {
    const std::size_t index = static_cast<std::size_t>(tile_set);
    if (index >= tile_sets.size()) {
        return nullptr;
    }
    return tile_sets[index];
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
    for (SDL_Texture* texture : sprite_textures) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
    sprite_textures.clear();

    for (SDL_Texture* texture : textures) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
    textures.clear();

    for (SDL_Texture* texture : tile_sets) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
    tile_sets.clear();

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
