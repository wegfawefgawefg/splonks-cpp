#pragma once

#include <cstdint>

struct SDL_Renderer;

namespace splonks {

struct Graphics;

void DrawMenuTitle(SDL_Renderer* renderer, Graphics& graphics, const char* title);
void RenderMenuLine(
    SDL_Renderer* renderer,
    Graphics& graphics,
    float x,
    float y,
    const char* text,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b
);

} // namespace splonks
