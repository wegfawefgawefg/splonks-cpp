#include "render.hpp"

#include "graphics.hpp"
#include "menu_postfx.hpp"
#include "menu_settings.hpp"
#include "render_postfx.hpp"
#include "menu_title.hpp"
#include "menu_ui.hpp"
#include "menu_video.hpp"
#include "render_debug.hpp"
#include "render_tiles_and_entities.hpp"
#include "render_ui.hpp"
#include "state.hpp"
#include "text.hpp"

#include <cstdio>
#include <cmath>

namespace splonks {

namespace {

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

void RenderTitle(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 38, 43, 68, 255);
    SDL_RenderClear(renderer);

    const float t = static_cast<float>(SDL_GetTicks()) / 1000.0F;
    const float parallax_param = std::sin(t);

    const auto render_layer = [&](TextureName texture_name,
                                  float max_displacement,
                                  float expansion_frac_base) {
        SDL_Texture* texture = graphics.GetTexture(texture_name);
        if (texture == nullptr) {
            return;
        }
        float width = 0.0F;
        float height = 0.0F;
        SDL_GetTextureSize(texture, &width, &height);

        const float expansion_frac = expansion_frac_base + (4.0F * max_displacement);
        const Vec2 target_size = Vec2::New(
            static_cast<float>(graphics.dims.x) * expansion_frac,
            static_cast<float>(graphics.dims.y)
        );
        Vec2 pos = (ToVec2(graphics.dims) - target_size) / 2.0F;
        pos.x += parallax_param * static_cast<float>(graphics.dims.x) * max_displacement;

        const SDL_FRect src{0.0F, 0.0F, width, height};
        const SDL_FRect dst{pos.x, 0.0F, target_size.x, target_size.y};
        SDL_RenderTexture(renderer, texture, &src, &dst);
    };

    render_layer(TextureName::TitleLayer3, 0.0001F, 1.2F);
    render_layer(TextureName::TitleLayer2, 0.02F, 1.0F);
    render_layer(TextureName::TitleLayer1, 0.06F, 1.0F);

    DrawMenuTitle(renderer, graphics, "Splonks");

    const float ten_percent = static_cast<float>(graphics.dims.y) * 0.10F;
    const float x = static_cast<float>(graphics.dims.x) * 0.15F;
    float y = static_cast<float>(graphics.dims.y) * 0.6F;

    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Start",
        state.title_menu_selection == TitleMenuOption::Start ? 230 : 255,
        state.title_menu_selection == TitleMenuOption::Start ? 41 : 255,
        state.title_menu_selection == TitleMenuOption::Start ? 55 : 255
    );
    y += ten_percent;
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Lighting",
        state.settings_menu_selection == SettingsMenuOption::Lighting ? 230 : 255,
        state.settings_menu_selection == SettingsMenuOption::Lighting ? 41 : 255,
        state.settings_menu_selection == SettingsMenuOption::Lighting ? 55 : 255
    );
    y += ten_percent;
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Settings",
        state.title_menu_selection == TitleMenuOption::Settings ? 230 : 255,
        state.title_menu_selection == TitleMenuOption::Settings ? 41 : 255,
        state.title_menu_selection == TitleMenuOption::Settings ? 55 : 255
    );
    y += ten_percent;
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Quit",
        state.title_menu_selection == TitleMenuOption::Quit ? 230 : 255,
        state.title_menu_selection == TitleMenuOption::Quit ? 41 : 255,
        state.title_menu_selection == TitleMenuOption::Quit ? 55 : 255
    );
}

void RenderSettingsMenu(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 38, 43, 68, 255);
    SDL_RenderClear(renderer);

    DrawMenuTitle(renderer, graphics, "Settings");

    const float ten_percent = static_cast<float>(graphics.dims.y) * 0.10F;
    const float x = static_cast<float>(graphics.dims.x) * 0.15F;
    float y = static_cast<float>(graphics.dims.y) * 0.4F;

    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Video",
        state.settings_menu_selection == SettingsMenuOption::Video ? 230 : 255,
        state.settings_menu_selection == SettingsMenuOption::Video ? 41 : 255,
        state.settings_menu_selection == SettingsMenuOption::Video ? 55 : 255
    );
    y += ten_percent;
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Audio",
        state.settings_menu_selection == SettingsMenuOption::Audio ? 230 : 255,
        state.settings_menu_selection == SettingsMenuOption::Audio ? 41 : 255,
        state.settings_menu_selection == SettingsMenuOption::Audio ? 55 : 255
    );
    y += ten_percent;
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Controls",
        state.settings_menu_selection == SettingsMenuOption::Controls ? 230 : 255,
        state.settings_menu_selection == SettingsMenuOption::Controls ? 41 : 255,
        state.settings_menu_selection == SettingsMenuOption::Controls ? 55 : 255
    );
    y += ten_percent;
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "UI",
        state.settings_menu_selection == SettingsMenuOption::Ui ? 230 : 255,
        state.settings_menu_selection == SettingsMenuOption::Ui ? 41 : 255,
        state.settings_menu_selection == SettingsMenuOption::Ui ? 55 : 255
    );
    y += ten_percent;
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Post FX",
        state.settings_menu_selection == SettingsMenuOption::PostFx ? 230 : 255,
        state.settings_menu_selection == SettingsMenuOption::PostFx ? 41 : 255,
        state.settings_menu_selection == SettingsMenuOption::PostFx ? 55 : 255
    );
    y += ten_percent;
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Back",
        state.settings_menu_selection == SettingsMenuOption::Back ? 230 : 255,
        state.settings_menu_selection == SettingsMenuOption::Back ? 41 : 255,
        state.settings_menu_selection == SettingsMenuOption::Back ? 55 : 255
    );
}

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

void RenderVideoSettingsMenu(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 62, 39, 49, 255);
    SDL_RenderClear(renderer);
    DrawMenuTitle(renderer, graphics, "Video Settings");

    const float ten_percent = static_cast<float>(graphics.dims.y) * 0.10F;
    const float x = static_cast<float>(graphics.dims.x) * 0.15F;
    float y = static_cast<float>(graphics.dims.y) * 0.4F;

    char line[128];
    const UVec2 resolution = state.video_settings_target_resolution_index
                                 ? kResolutions[*state.video_settings_target_resolution_index]
                                 : graphics.dims;
    std::snprintf(line, sizeof(line), "Resolution: %u x %u", resolution.x, resolution.y);
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.video_settings_menu_selection == VideoSettingsMenuOption::Resolution ? 230 : 255,
        state.video_settings_menu_selection == VideoSettingsMenuOption::Resolution ? 41 : 255,
        state.video_settings_menu_selection == VideoSettingsMenuOption::Resolution ? 55 : 255
    );

    const UVec2 window_size = state.video_settings_target_window_size_index
                                  ? kResolutions[*state.video_settings_target_window_size_index]
                                  : graphics.window_dims;
    std::snprintf(line, sizeof(line), "Window Size: %u x %u", window_size.x, window_size.y);
    y += ten_percent;
    if (state.video_settings_menu_selection == VideoSettingsMenuOption::WindowSize) {
        RenderMenuLine(renderer, graphics, x, y, line, 230, 41, 55);
    } else if (WindowSizeAvailableToChange(state, graphics)) {
        RenderMenuLine(renderer, graphics, x, y, line, 255, 255, 255);
    } else {
        RenderMenuLine(renderer, graphics, x, y, line, 130, 130, 130);
    }

    const bool fullscreen =
        state.video_settings_target_fullscreen ? *state.video_settings_target_fullscreen : graphics.fullscreen;
    std::snprintf(line, sizeof(line), "Fullscreen: %s", fullscreen ? "Yes" : "No");
    y += ten_percent;
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        line,
        state.video_settings_menu_selection == VideoSettingsMenuOption::Fullscreen ? 230 : 255,
        state.video_settings_menu_selection == VideoSettingsMenuOption::Fullscreen ? 41 : 255,
        state.video_settings_menu_selection == VideoSettingsMenuOption::Fullscreen ? 55 : 255
    );

    y += ten_percent;
    if (state.video_settings_menu_selection == VideoSettingsMenuOption::Apply) {
        RenderMenuLine(renderer, graphics, x, y, "Apply", 230, 41, 55);
    } else if (ApplyShouldBeAvailable(state)) {
        RenderMenuLine(renderer, graphics, x, y, "Apply", 255, 203, 0);
    } else {
        RenderMenuLine(renderer, graphics, x, y, "Apply", 130, 130, 130);
    }

    y += ten_percent;
    RenderMenuLine(
        renderer,
        graphics,
        x,
        y,
        "Back",
        state.video_settings_menu_selection == VideoSettingsMenuOption::Back ? 230 : 255,
        state.video_settings_menu_selection == VideoSettingsMenuOption::Back ? 41 : 255,
        state.video_settings_menu_selection == VideoSettingsMenuOption::Back ? 55 : 255
    );
}

void RenderPlaying(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    const float base = 5.0F;
    if (graphics.dims.x < 1280U) {
        const float ratio = 1280.0F / static_cast<float>(graphics.dims.x);
        graphics.camera.zoom = base / ratio;
    } else {
        graphics.camera.zoom = base;
    }

    if (state.controlled_entity_vid.has_value()) {
        if (const Entity* const controlled = state.entity_manager.GetEntity(*state.controlled_entity_vid)) {
            if (!graphics.debug_lock_play_camera) {
                const Vec2 delta = controlled->pos - graphics.play_cam.pos;
                graphics.play_cam.pos += delta * 0.075F;

                const Vec2 stage_dims = ToVec2(state.stage.GetStageDims());
                const Vec2 margin = state.stage.camera_clamp_margin;
                const Vec2 map_tl_bound = margin;
                const Vec2 map_br_bound = stage_dims - margin;

                if (stage_dims.x <= margin.x * 2.0F) {
                    graphics.play_cam.pos.x = stage_dims.x / 2.0F;
                } else {
                    graphics.play_cam.pos.x = graphics.play_cam.pos.x < map_tl_bound.x
                                                  ? map_tl_bound.x
                                                  : (graphics.play_cam.pos.x > map_br_bound.x
                                                         ? map_br_bound.x
                                                         : graphics.play_cam.pos.x);
                }

                if (stage_dims.y <= margin.y * 2.0F) {
                    graphics.play_cam.pos.y = stage_dims.y / 2.0F;
                } else {
                    graphics.play_cam.pos.y = graphics.play_cam.pos.y < map_tl_bound.y
                                                  ? map_tl_bound.y
                                                  : (graphics.play_cam.pos.y > map_br_bound.y
                                                         ? map_br_bound.y
                                                         : graphics.play_cam.pos.y);
                }
            }

            graphics.camera.target = graphics.play_cam.pos;
        } else {
            graphics.camera.target = ToVec2(state.stage.GetStageDims()) / 2.0F;
        }
    } else {
        graphics.camera.target = ToVec2(state.stage.GetStageDims()) / 2.0F;
    }

    SDL_SetRenderDrawColor(renderer, 38, 43, 68, 255);
    SDL_RenderClear(renderer);
    RenderStageTileWrapper(renderer, state, graphics);
    RenderStageTiles(renderer, state, graphics);
    RenderEmbeddedTreasureOverlays(renderer, state, graphics);
    RenderBackgroundStamps(renderer, state, graphics);
    RenderEntities(renderer, state, graphics);
    RenderSpecialEffects(renderer, state, graphics);
}

void DrawCenteredText(
    SDL_Renderer* renderer,
    Graphics& graphics,
    TextType text_type,
    const char* text,
    float center_x,
    float y,
    SDL_Color color
) {
    int text_width = 0;
    int text_height = 0;
    if (!MeasureText(graphics, text_type, text, &text_width, &text_height)) {
        DrawText(renderer, graphics, text_type, text, center_x, y, color);
        return;
    }
    DrawText(
        renderer,
        graphics,
        text_type,
        text,
        center_x - (static_cast<float>(text_width) / 2.0F),
        y - (static_cast<float>(text_height) / 2.0F),
        color
    );
}

const char* GetStageTransitionTitle(const State& state) {
    switch (state.next_stage.value_or(StageType::Blank)) {
    case StageType::Test1:
        return "Test1";
    case StageType::Blank:
        return "Blank?? Expect crash";
    case StageType::Cave1:
        return "Cave";
    case StageType::Cave2:
        return "Cave 2";
    case StageType::Cave3:
        return "Cave 3";
    case StageType::Ice1:
        return "Ice";
    case StageType::Ice2:
        return "Ice 2";
    case StageType::Ice3:
        return "Ice 3";
    case StageType::Desert1:
        return "Desert";
    case StageType::Desert2:
        return "Desert 2";
    case StageType::Desert3:
        return "Desert 3";
    case StageType::Temple1:
        return "Temple";
    case StageType::Temple2:
        return "Temple 2";
    case StageType::Temple3:
        return "Temple 3";
    case StageType::Boss:
        return "Boss";
    }
    return "This shouldnt be possible...???";
}

const char* GetStageTransitionMessage(const State& state) {
    switch (state.next_stage.value_or(StageType::Blank)) {
    case StageType::Blank:
        return "!!!!expect a crash on a press!!!!";
    case StageType::Test1:
        return "You feel like figuring out bugs...";
    case StageType::Cave1:
        return "You enter the cave...";
    case StageType::Ice1:
        return "Its getting cold...";
    case StageType::Desert1:
        return "This place looks old...";
    case StageType::Temple1:
        return "Cant turn back now...";
    case StageType::Boss:
        return "The end is near...";
    default:
        return "Press [jump] to go deeper...";
    }
}

void RenderStageTransition(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    const float center_x = static_cast<float>(graphics.dims.x) / 2.0F;
    const float center_y = static_cast<float>(graphics.dims.y) / 2.0F;
    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuTitle,
        GetStageTransitionTitle(state),
        center_x,
        center_y,
        SDL_Color{255, 255, 255, 255}
    );
    DrawText(
        renderer,
        graphics,
        30,
        graphics.ui_font,
        GetStageTransitionMessage(state),
        center_x - (static_cast<float>(graphics.dims.x) * 0.16F),
        center_y + 75.0F,
        SDL_Color{255, 255, 255, 255}
    );
}

void RenderGameOver(SDL_Renderer* renderer, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 50, 0, 0, 255);
    SDL_RenderClear(renderer);

    const float center_x = static_cast<float>(graphics.dims.x) / 2.0F;
    const float center_y = static_cast<float>(graphics.dims.y) / 2.0F;
    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuTitle,
        "Game Over",
        center_x,
        center_y,
        SDL_Color{255, 255, 255, 255}
    );
    DrawText(
        renderer,
        graphics,
        30,
        graphics.ui_font,
        "Press [jump] to try again...",
        center_x - (static_cast<float>(graphics.dims.x) * 0.15F),
        center_y + 75.0F,
        SDL_Color{255, 255, 255, 255}
    );
}

void RenderWin(SDL_Renderer* renderer, Graphics& graphics) {
    SDL_SetRenderDrawColor(renderer, 0, 50, 20, 255);
    SDL_RenderClear(renderer);

    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuTitle,
        "Win! Nice!",
        static_cast<float>(graphics.dims.x) / 2.0F,
        static_cast<float>(graphics.dims.y) / 2.0F,
        SDL_Color{255, 255, 255, 255}
    );
}

} // namespace

void Render(
    SDL_Renderer* renderer,
    SDL_Texture* render_texture,
    const RenderPostFx& post_fx,
    State& state,
    Graphics& graphics
) {
    if (render_texture == nullptr) {
        return;
    }

    SDL_SetRenderTarget(renderer, render_texture);

    switch (state.mode) {
    case Mode::Title:
        RenderTitle(renderer, state, graphics);
        break;
    case Mode::Settings:
        RenderSettingsMenu(renderer, state, graphics);
        break;
    case Mode::VideoSettings:
        RenderVideoSettingsMenu(renderer, state, graphics);
        break;
    case Mode::UiSettings:
        RenderUiSettingsMenu(renderer, state, graphics);
        break;
    case Mode::PostFxSettings:
        RenderPostFxSettingsMenu(renderer, state, graphics);
        break;
    case Mode::LightingSettings:
        RenderLightingSettingsMenu(renderer, state, graphics);
        break;
    case Mode::Playing:
        RenderPlaying(renderer, state, graphics);
        break;
    case Mode::StageTransition:
        RenderStageTransition(renderer, state, graphics);
        break;
    case Mode::GameOver:
        RenderGameOver(renderer, graphics);
        break;
    case Mode::Win:
        RenderWin(renderer, graphics);
        break;
    }

    SDL_SetRenderTarget(renderer, nullptr);

    int output_width = static_cast<int>(graphics.window_dims.x);
    int output_height = static_cast<int>(graphics.window_dims.y);
    if (graphics.fullscreen) {
        SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height);
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    const SDL_FRect src{
        0.0F,
        0.0F,
        static_cast<float>(graphics.dims.x),
        static_cast<float>(graphics.dims.y),
    };
    const SDL_FRect dst = GetPresentationRect(graphics, output_width, output_height);
    SDL_GPURenderState* const post_fx_state = GetActivePostFxState(post_fx, state);
    if (post_fx_state != nullptr) {
        SDL_SetGPURenderState(renderer, post_fx_state);
    }
    SDL_RenderTexture(renderer, render_texture, &src, &dst);
    if (post_fx_state != nullptr) {
        SDL_SetGPURenderState(renderer, nullptr);
    }

    if (state.mode == Mode::Playing) {
        RenderHealthRopeBombs(renderer, state, graphics);
        if (state.show_entity_collision_boxes) {
            RenderEntityCollisionBoxes(renderer, graphics, state);
        }
        if (state.show_entity_ids) {
            RenderEntityIds(renderer, graphics, state);
        }
    }
}

} // namespace splonks
