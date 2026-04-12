#include "render/menus.hpp"

#include "graphics.hpp"
#include "menu/postfx.hpp"
#include "menu/ui.hpp"
#include "render/menu/common.hpp"
#include "state.hpp"
#include "text.hpp"

#include <SDL3/SDL.h>

#include <cstdio>

namespace splonks {

namespace {

const char* GetPostProcessEffectName(PostProcessEffect effect) {
    switch (effect) {
    case PostProcessEffect::None:
        return "None";
    case PostProcessEffect::Crt:
        return "CRT";
    }

    return "";
}

void RenderPostFxLine(
    SDL_Renderer* renderer,
    Graphics& graphics,
    float x,
    float y,
    const char* line,
    bool selected
) {
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        selected ? 230 : 255,
        selected ? 41 : 255,
        selected ? 55 : 255
    );
}

} // namespace

void RenderUiSettingsMenu(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 44, 50, 36, 255);
    SDL_RenderClear(renderer);
    DrawMenuTitle(renderer, graphics, "UI Settings");

    const float line_spacing = static_cast<float>(graphics.dims.y) * 0.07F;
    const float x = static_cast<float>(graphics.dims.x) * 0.15F;
    float y = static_cast<float>(graphics.dims.y) * 0.22F;

    char line[128];
    std::snprintf(line, sizeof(line), "Icon Scale: %.2fx", state.settings.ui.icon_scale);
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.ui_settings_menu_selection == UiSettingsMenuOption::IconScale ? 230 : 255,
        state.ui_settings_menu_selection == UiSettingsMenuOption::IconScale ? 41 : 255,
        state.ui_settings_menu_selection == UiSettingsMenuOption::IconScale ? 55 : 255
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Status Icon Scale: %.2fx",
        state.settings.ui.status_icon_scale
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.ui_settings_menu_selection == UiSettingsMenuOption::StatusIconScale ? 230 : 255,
        state.ui_settings_menu_selection == UiSettingsMenuOption::StatusIconScale ? 41 : 255,
        state.ui_settings_menu_selection == UiSettingsMenuOption::StatusIconScale ? 55 : 255
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Tool Slot Scale: %.2fx",
        state.settings.ui.tool_slot_scale
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.ui_settings_menu_selection == UiSettingsMenuOption::ToolSlotScale ? 230 : 255,
        state.ui_settings_menu_selection == UiSettingsMenuOption::ToolSlotScale ? 41 : 255,
        state.ui_settings_menu_selection == UiSettingsMenuOption::ToolSlotScale ? 55 : 255
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Tool Icon Scale: %.2fx",
        state.settings.ui.tool_icon_scale
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.ui_settings_menu_selection == UiSettingsMenuOption::ToolIconScale ? 230 : 255,
        state.ui_settings_menu_selection == UiSettingsMenuOption::ToolIconScale ? 41 : 255,
        state.ui_settings_menu_selection == UiSettingsMenuOption::ToolIconScale ? 55 : 255
    );
    y += line_spacing;

    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Back",
        state.ui_settings_menu_selection == UiSettingsMenuOption::Back ? 230 : 255,
        state.ui_settings_menu_selection == UiSettingsMenuOption::Back ? 41 : 255,
        state.ui_settings_menu_selection == UiSettingsMenuOption::Back ? 55 : 255
    );
}

void RenderPostFxSettingsMenu(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 28, 36, 48, 255);
    SDL_RenderClear(renderer);
    DrawMenuTitle(renderer, graphics, "Post FX");

    const float line_spacing = static_cast<float>(graphics.dims.y) * 0.07F;
    const float x = static_cast<float>(graphics.dims.x) * 0.15F;
    float y = static_cast<float>(graphics.dims.y) * 0.22F;

    char line[128];
    std::snprintf(
        line,
        sizeof(line),
        "Effect: %s",
        GetPostProcessEffectName(state.settings.post_process.effect)
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::Effect ? 230 : 255,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::Effect ? 41 : 255,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::Effect ? 55 : 255
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Scanlines: %.2f",
        state.settings.post_process.crt_scanline_amount
    );
    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::CrtScanlines
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Scanline Edge Start: %.2f",
        state.settings.post_process.crt_scanline_edge_start
    );
    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::CrtScanlineEdgeStart
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Scanline Edge Falloff: %.2f",
        state.settings.post_process.crt_scanline_edge_falloff
    );
    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::CrtScanlineEdgeFalloff
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Scanline Edge Strength: %.2f",
        state.settings.post_process.crt_scanline_edge_strength
    );
    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::CrtScanlineEdgeStrength
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Zoom: %.2f",
        state.settings.post_process.crt_zoom
    );
    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::CrtZoom
    );
    y += line_spacing;

    std::snprintf(line, sizeof(line), "Warp: %.2f", state.settings.post_process.crt_warp_amount);
    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::CrtWarp
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Vignette: %.2f",
        state.settings.post_process.crt_vignette_amount
    );
    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::CrtVignette
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Vignette Intensity: %.2f",
        state.settings.post_process.crt_vignette_intensity
    );
    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::CrtVignetteIntensity
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Grille: %.2f",
        state.settings.post_process.crt_grille_amount
    );
    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::CrtGrille
    );
    y += line_spacing;

    std::snprintf(
        line,
        sizeof(line),
        "Brightness: %.2f",
        state.settings.post_process.crt_brightness_boost
    );
    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::CrtBrightness
    );
    y += line_spacing;

    RenderPostFxLine(
        renderer,
        graphics,
        x,
        y,
        "Back",
        state.post_fx_settings_menu_selection == PostFxSettingsMenuOption::Back
    );

    y += line_spacing * 1.5F;
    DrawText(
        renderer,
        graphics,
        TextType::MenuItem,
        graphics.gpu_renderer_active ? "Renderer: SDL GPU" : "Renderer: fallback (post FX unavailable)",
        x,
        y,
        SDL_Color{220, 220, 220, 255}
    );
}

} // namespace splonks
