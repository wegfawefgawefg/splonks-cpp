#include "text.hpp"

#include <SDL3_ttf/SDL_ttf.h>

#include <stdexcept>
#include <string>

namespace splonks {

namespace {

[[noreturn]] void ThrowTextError(const char* message) {
    throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

TTF_Font* EnsureFont(Graphics& graphics, int point_size, LoadedFont& loaded_font) {
    if (loaded_font.font != nullptr && loaded_font.point_size == point_size) {
        return loaded_font.font;
    }

    if (loaded_font.font != nullptr) {
        TTF_CloseFont(loaded_font.font);
        loaded_font = {};
    }

    loaded_font.font = TTF_OpenFont(graphics.font_path.c_str(), static_cast<float>(point_size));
    if (loaded_font.font == nullptr) {
        ThrowTextError("TTF_OpenFont failed");
    }
    loaded_font.point_size = point_size;
    return loaded_font.font;
}

void DrawTextInternal(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const char* text,
    float x,
    float y,
    SDL_Color color
) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, 0, color);
    if (surface == nullptr) {
        ThrowTextError("TTF_RenderText_Blended failed");
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr) {
        SDL_DestroySurface(surface);
        ThrowTextError("SDL_CreateTextureFromSurface failed");
    }

    const SDL_FRect dst{
        x,
        y,
        static_cast<float>(surface->w),
        static_cast<float>(surface->h),
    };
    SDL_RenderTexture(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

} // namespace

bool InitTextSubsystem() {
    return TTF_Init();
}

void ShutdownTextSubsystem() {
    TTF_Quit();
}

void DrawText(
    SDL_Renderer* renderer,
    Graphics& graphics,
    TextType text_type,
    const char* text,
    float x,
    float y,
    SDL_Color color
) {
    const int point_size = GetReasonableFontScale(graphics.dims, text_type);
    switch (text_type) {
    case TextType::MenuTitle:
        DrawText(renderer, graphics, point_size, graphics.menu_title_font, text, x, y, color);
        break;
    case TextType::MenuItem:
        DrawText(renderer, graphics, point_size, graphics.menu_item_font, text, x, y, color);
        break;
    }
}

void DrawText(
    SDL_Renderer* renderer,
    Graphics& graphics,
    int point_size,
    LoadedFont& loaded_font,
    const char* text,
    float x,
    float y,
    SDL_Color color
) {
    TTF_Font* font = EnsureFont(graphics, point_size, loaded_font);
    DrawTextInternal(renderer, font, text, x, y, color);
}

bool MeasureText(
    Graphics& graphics,
    TextType text_type,
    const char* text,
    int* width,
    int* height
) {
    const int point_size = GetReasonableFontScale(graphics.dims, text_type);
    LoadedFont* loaded_font = nullptr;
    switch (text_type) {
    case TextType::MenuTitle:
        loaded_font = &graphics.menu_title_font;
        break;
    case TextType::MenuItem:
        loaded_font = &graphics.menu_item_font;
        break;
    }

    TTF_Font* font = EnsureFont(graphics, point_size, *loaded_font);
    return TTF_GetStringSize(font, text, 0, width, height);
}

} // namespace splonks
