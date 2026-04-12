#include "render/menus.hpp"

#include "graphics.hpp"
#include "menu/settings.hpp"
#include "render/menu/common.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>

#include <cstdio>

namespace splonks {

void RenderLightingSettingsMenu(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 34, 42, 30, 255);
    SDL_RenderClear(renderer);
    DrawMenuTitle(renderer, graphics, "Lighting");

    const float ten_percent = static_cast<float>(graphics.dims.y) * 0.10F;
    const float x = static_cast<float>(graphics.dims.x) * 0.15F;
    float y = static_cast<float>(graphics.dims.y) * 0.4F;

    char line[128];
    std::snprintf(
        line,
        sizeof(line),
        "Terrain Lighting: %s",
        state.settings.post_process.terrain_lighting ? "On" : "Off"
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainLighting ? 230
                                                                                              : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainLighting ? 41
                                                                                              : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainLighting ? 55
                                                                                              : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Face Shading: %s",
        state.settings.post_process.terrain_face_shading ? "On" : "Off"
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainFaceShading ? 230 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainFaceShading ? 41 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainFaceShading ? 55 : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Enclosed Stage Bounds: %s",
        state.settings.post_process.terrain_face_enclosed_stage_bounds ? "On" : "Off"
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainEnclosedStageBounds
            ? 230
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainEnclosedStageBounds
            ? 41
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainEnclosedStageBounds
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Top Highlight: %.2f",
        state.settings.post_process.terrain_face_top_highlight
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainTopHighlight ? 230 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainTopHighlight ? 41 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainTopHighlight ? 55 : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Side Shade: %.2f",
        state.settings.post_process.terrain_face_side_shade
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSideShade ? 230 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSideShade ? 41 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSideShade ? 55 : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Bottom Shade: %.2f",
        state.settings.post_process.terrain_face_bottom_shade
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainBottomShade ? 230 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainBottomShade ? 41 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainBottomShade ? 55 : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Band Size: %.2f",
        state.settings.post_process.terrain_face_band_size
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainBandSize ? 230 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainBandSize ? 41 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainBandSize ? 55 : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Gradient Softness: %.2f",
        state.settings.post_process.terrain_face_gradient_softness
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainGradientSoftness
            ? 230
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainGradientSoftness
            ? 41
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainGradientSoftness
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Corner Rounding: %.2f",
        state.settings.post_process.terrain_face_corner_rounding
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainCornerRounding
            ? 230
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainCornerRounding
            ? 41
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainCornerRounding
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Seam AO: %s",
        state.settings.post_process.terrain_seam_ao ? "On" : "Off"
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSeamAo ? 230
                                                                                             : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSeamAo ? 41
                                                                                             : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSeamAo ? 55
                                                                                             : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Seam AO Amount: %.2f",
        state.settings.post_process.terrain_seam_ao_amount
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSeamAoAmount
            ? 230
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSeamAoAmount
            ? 41
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSeamAoAmount
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Seam AO Size: %.2f",
        state.settings.post_process.terrain_seam_ao_size
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSeamAoSize
            ? 230
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSeamAoSize
            ? 41
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::TerrainSeamAoSize
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Exposure Lighting: %s",
        state.settings.post_process.terrain_exposure_lighting ? "On" : "Off"
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureLighting
            ? 230
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureLighting
            ? 41
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureLighting
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Exposure Amount: %.2f",
        state.settings.post_process.terrain_exposure_amount
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureAmount
            ? 230
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureAmount
            ? 41
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureAmount
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Exposure Min Brightness: %.2f",
        state.settings.post_process.terrain_exposure_min_brightness
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureMinBrightness
            ? 230
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureMinBrightness
            ? 41
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureMinBrightness
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Exposure Max Brightness: %.2f",
        state.settings.post_process.terrain_exposure_max_brightness
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureMaxBrightness
            ? 230
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureMaxBrightness
            ? 41
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureMaxBrightness
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Exposure Diagonal Weight: %.2f",
        state.settings.post_process.terrain_exposure_diagonal_weight
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureDiagonalWeight
            ? 230
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureDiagonalWeight
            ? 41
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureDiagonalWeight
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Terrain Exposure Smoothing: %.2f",
        state.settings.post_process.terrain_exposure_smoothing
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureSmoothing
            ? 230
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureSmoothing
            ? 41
            : 255,
        state.lighting_settings_menu_selection ==
                LightingSettingsMenuOption::TerrainExposureSmoothing
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Backwall Lighting: %s",
        state.settings.post_process.backwall_lighting ? "On" : "Off"
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallLighting
            ? 230
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallLighting
            ? 41
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallLighting
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Backwall Brightness: %.2f",
        state.settings.post_process.backwall_brightness
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallBrightness
            ? 230
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallBrightness
            ? 41
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallBrightness
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Backwall Min Brightness: %.2f",
        state.settings.post_process.backwall_min_brightness
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallMinBrightness
            ? 230
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallMinBrightness
            ? 41
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallMinBrightness
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Backwall Max Brightness: %.2f",
        state.settings.post_process.backwall_max_brightness
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallMaxBrightness
            ? 230
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallMaxBrightness
            ? 41
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallMaxBrightness
            ? 55
            : 255
    );
    y += ten_percent;

    std::snprintf(
        line,
        sizeof(line),
        "Backwall Smoothing: %.2f",
        state.settings.post_process.backwall_smoothing
    );
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallSmoothing
            ? 230
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallSmoothing
            ? 41
            : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::BackwallSmoothing
            ? 55
            : 255
    );
    y += ten_percent;

    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Back",
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::Back ? 230 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::Back ? 41 : 255,
        state.lighting_settings_menu_selection == LightingSettingsMenuOption::Back ? 55 : 255
    );
}

} // namespace splonks
