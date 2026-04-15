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
        "assets/graphics/particles/" + filename,
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
    graphics.window_dims = UVec2::New(1920, 1080);
    graphics.dims = UVec2::New(1920, 1080);
    graphics.fullscreen = true;
    graphics.gpu_renderer_active = false;

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

SDL_FRect GetPresentationRect(const Graphics& graphics, int output_width, int output_height) {
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

SDL_Texture* Graphics::GetFrameDataTexture(std::uint32_t image_id) const {
    const std::size_t index = static_cast<std::size_t>(image_id);
    if (index >= frame_data_images.size()) {
        return nullptr;
    }
    return frame_data_images[index];
}

Vec2 Graphics::ScreenToWc(const UVec2& screen_pos) const {
    const SDL_FRect presentation = GetPresentationRect(
        *this,
        static_cast<int>(window_dims.x),
        static_cast<int>(window_dims.y)
    );

    Vec2 screen = ToVec2(screen_pos);
    screen = screen - Vec2::New(presentation.x, presentation.y);

    const float presentation_scale = presentation.w / static_cast<float>(dims.x);
    if (presentation_scale > 0.0F) {
        screen = screen / presentation_scale;
    }

    screen = screen - camera.offset;
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
