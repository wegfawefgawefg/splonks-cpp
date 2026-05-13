#include "graphics.hpp"

#include "raw_aframe.hpp"
#include "stage.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace splonks {

namespace {

struct LoadedAFrameResources {
    AFrameDb aframe_db;
    std::vector<SDL_Texture*> aframe_images;
    TileSourceDb tile_source_db;
    TileContactDb tile_contact_db;
    std::filesystem::file_time_type write_time{};
};

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

SDL_Texture* LoadAFrameTexture(SDL_Renderer* renderer, const std::string& filename) {
    const std::vector<std::string> candidate_paths = {
        "assets/graphics/sprites/" + filename,
        "assets/graphics/tiles/" + filename,
        "assets/graphics/images/" + filename,
        "assets/graphics/particles/" + filename,
    };

    for (const std::string& path : candidate_paths) {
        if (std::filesystem::exists(path)) {
            return LoadTexture(renderer, path);
        }
    }

    throw std::runtime_error("Missing frame data texture for " + filename);
}

void DestroyTextureList(std::vector<SDL_Texture*>& textures) {
    for (SDL_Texture* texture : textures) {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
    textures.clear();
}

std::filesystem::file_time_type GetFileWriteTimeOrThrow(const std::string& path) {
    std::error_code ec;
    const std::filesystem::file_time_type result = std::filesystem::last_write_time(path, ec);
    if (ec) {
        throw std::runtime_error("Failed to stat " + path + ": " + ec.message());
    }
    return result;
}

LoadedAFrameResources LoadAFrameResources(
    SDL_Renderer* renderer,
    const std::string& annotations_path
) {
    LoadedAFrameResources result;
    const RawAFrameFile raw_aframe_file = LoadRawAFrameFile(annotations_path);
    result.aframe_db = AFrameDb::FromRaw(raw_aframe_file);
    result.aframe_images.reserve(result.aframe_db.image_paths.size());

    try {
        for (const std::string& image_path : result.aframe_db.image_paths) {
            result.aframe_images.push_back(LoadAFrameTexture(renderer, image_path));
        }
    } catch (...) {
        DestroyTextureList(result.aframe_images);
        throw;
    }

    result.tile_source_db = BuildTileSourceDb(result.aframe_db);
    result.tile_contact_db = BuildTileContactDb(result.tile_source_db);
    result.write_time = GetFileWriteTimeOrThrow(annotations_path);
    return result;
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

    const LoadedAFrameResources aframe_resources =
        LoadAFrameResources(renderer, graphics.aframe_annotations_path);
    graphics.aframe_db = aframe_resources.aframe_db;
    graphics.aframe_images = aframe_resources.aframe_images;
    graphics.aframe_annotations_last_loaded_write_time = aframe_resources.write_time;
    graphics.aframe_annotations_last_seen_write_time = aframe_resources.write_time;
    graphics.tile_source_db = aframe_resources.tile_source_db;
    graphics.tile_contact_db = aframe_resources.tile_contact_db;
    graphics.window_dims = UVec2::New(1920, 540);
    graphics.dims = UVec2::New(1920, 540);
    graphics.fullscreen = false;
    graphics.gpu_renderer_active = false;
    graphics.world_rotation_active = false;
    graphics.world_rotation_pivot = Vec2::New(0.0F, 0.0F);
    graphics.world_rotation_degrees = 0.0F;

    const Vec2 screen_center = ToVec2(graphics.dims / 2U);
    graphics.camera.target = Vec2::New(0.0F, 0.0F);
    graphics.camera.offset = screen_center;
    graphics.camera.rotation = 0.0F;
    graphics.camera.zoom = 3.0F;

    graphics.play_cam.pos = Vec2::New(0.0F, 0.0F);
    graphics.play_cam.vel = Vec2::New(0.0F, 0.0F);
    graphics.play_cam.acc = Vec2::New(0.0F, 0.0F);
    return graphics;
}

SDL_FRect GetPresRect(const Graphics& graphics, int output_width, int output_height) {
    const int scale_x = output_width / static_cast<int>(graphics.dims.x);
    const int scale_y = output_height / static_cast<int>(graphics.dims.y);
    const int scale = std::max(1, std::min(scale_x, scale_y));

    const int presented_width = static_cast<int>(graphics.dims.x) * scale;
    const int presented_height = static_cast<int>(graphics.dims.y) * scale;
    const int dst_x = (output_width - presented_width) / 2;
    const int dst_y = (output_height - presented_height) / 2;

    return SDL_FRect{
        static_cast<float>(dst_x),
        static_cast<float>(dst_y),
        static_cast<float>(presented_width),
        static_cast<float>(presented_height),
    };
}

Vec2 GetStageCameraCenter(const Stage& stage) {
    return ToVec2(stage.GetStageDims()) / 2.0F;
}

float GetDefaultFollowCameraZoom(const Graphics& graphics) {
    const float base = graphics.follow_camera_zoom;
    if (graphics.dims.x < 1280U) {
        const float ratio = 1280.0F / static_cast<float>(graphics.dims.x);
        return base / ratio;
    }
    return base;
}

float GetStageFitCameraZoom(const Stage& stage, const Graphics& graphics) {
    const Vec2 stage_dims = ToVec2(stage.GetStageDims());
    const float padded_width = std::max(1.0F, stage_dims.x + (graphics.stage_fit_padding * 2.0F));
    const float padded_height = std::max(1.0F, stage_dims.y + (graphics.stage_fit_padding * 2.0F));
    const float zoom_x = static_cast<float>(graphics.dims.x) / padded_width;
    const float zoom_y = static_cast<float>(graphics.dims.y) / padded_height;
    return std::min(zoom_x, zoom_y);
}

SDL_Texture* Graphics::GetTexture(TextureName texture) const {
    const std::size_t index = static_cast<std::size_t>(texture);
    if (index >= textures.size()) {
        return nullptr;
    }
    return textures[index];
}

SDL_Texture* Graphics::GetAFrameTexture(std::uint32_t image_id) const {
    const std::size_t index = static_cast<std::size_t>(image_id);
    if (index >= aframe_images.size()) {
        return nullptr;
    }
    return aframe_images[index];
}

Vec2 Graphics::ScreenToWc(const UVec2& screen_pos) const {
    const SDL_FRect pres = GetPresRect(
        *this,
        static_cast<int>(window_dims.x),
        static_cast<int>(window_dims.y)
    );

    Vec2 screen = ToVec2(screen_pos);
    screen = screen - Vec2::New(pres.x, pres.y);

    const float pres_scale = pres.w / static_cast<float>(dims.x);
    if (pres_scale > 0.0F) {
        screen = screen / pres_scale;
    }

    screen = screen - camera.offset;
    screen = screen / camera.zoom;
    return screen + camera.target;
}

Vec2 Graphics::WcToScreen(const Vec2& world_pos) const {
    return ((world_pos - camera.target) * camera.zoom) + camera.offset;
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

bool Graphics::ReloadAFrame(SDL_Renderer* renderer, std::string* status_out) {
    try {
        LoadedAFrameResources aframe_resources =
            LoadAFrameResources(renderer, aframe_annotations_path);
        std::vector<SDL_Texture*> old_images = std::move(aframe_images);
        aframe_db = std::move(aframe_resources.aframe_db);
        aframe_images = std::move(aframe_resources.aframe_images);
        tile_source_db = std::move(aframe_resources.tile_source_db);
        tile_contact_db = std::move(aframe_resources.tile_contact_db);
        aframe_annotations_last_loaded_write_time = aframe_resources.write_time;
        aframe_annotations_last_seen_write_time = aframe_resources.write_time;
        ResetTileVariations();
        DestroyTextureList(old_images);
        if (status_out != nullptr) {
            *status_out = "Reloaded frame data from " + aframe_annotations_path;
        }
        return true;
    } catch (const std::exception& exception) {
        if (status_out != nullptr) {
            *status_out = "Frame data reload failed: " + std::string(exception.what());
        }
        return false;
    }
}

bool Graphics::ReloadAFrameIfChanged(SDL_Renderer* renderer, std::string* status_out) {
    std::error_code ec;
    const std::filesystem::file_time_type current_write_time =
        std::filesystem::last_write_time(aframe_annotations_path, ec);
    if (ec) {
        if (status_out != nullptr) {
            *status_out = "Frame data watch failed: " + ec.message();
        }
        return false;
    }
    if (current_write_time <= aframe_annotations_last_seen_write_time) {
        return false;
    }

    aframe_annotations_last_seen_write_time = current_write_time;
    return ReloadAFrame(renderer, status_out);
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
    DestroyTextureList(textures);
    DestroyTextureList(aframe_images);
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
