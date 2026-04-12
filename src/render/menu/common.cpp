#include "render/menu/common.hpp"

#include "graphics.hpp"
#include "text.hpp"

#include <SDL3/SDL.h>

namespace splonks {

void DrawMenuTitle(SDL_Renderer* renderer, Graphics& graphics, const char* title) {
    const float title_x = static_cast<float>(graphics.dims.x) * 0.1F;
    const float title_y = static_cast<float>(graphics.dims.y) * 0.2F;
    const int font_size = GetReasonableFontScale(graphics.dims, TextType::MenuTitle);
    DrawText(
        renderer,
        graphics,
        font_size,
        graphics.menu_title_font,
        title,
        title_x,
        title_y + static_cast<float>(font_size / 8),
        SDL_Color{0, 0, 0, 255}
    );
    DrawText(
        renderer,
        graphics,
        font_size,
        graphics.menu_title_font,
        title,
        title_x,
        title_y,
        SDL_Color{255, 255, 255, 255}
    );
}

void RenderMenuLine(
    SDL_Renderer* renderer,
    Graphics& graphics,
    float x,
    float y,
    const char* text,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b
) {
    DrawText(renderer, graphics, TextType::MenuItem, text, x, y, SDL_Color{r, g, b, 255});
}

} // namespace splonks
