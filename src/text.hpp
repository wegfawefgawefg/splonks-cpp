#pragma once

#include "graphics.hpp"

#include <SDL3/SDL.h>

namespace splonks {

bool InitTextSubsystem();
void ShutdownTextSubsystem();

void DrawText(
    SDL_Renderer* renderer,
    Graphics& graphics,
    TextType text_type,
    const char* text,
    float x,
    float y,
    SDL_Color color
);

void DrawText(
    SDL_Renderer* renderer,
    Graphics& graphics,
    int point_size,
    LoadedFont& loaded_font,
    const char* text,
    float x,
    float y,
    SDL_Color color
);

bool MeasureText(
    Graphics& graphics,
    TextType text_type,
    const char* text,
    int* width,
    int* height
);

bool MeasureText(
    Graphics& graphics,
    int point_size,
    LoadedFont& loaded_font,
    const char* text,
    int* width,
    int* height
);

} // namespace splonks
