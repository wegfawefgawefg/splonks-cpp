#include "menu/lighting.hpp"

#include "audio.hpp"
#include "graphics.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "render/terrain_lighting.hpp"

#include <algorithm>

namespace splonks {

namespace {

constexpr float kLightingStep = 0.05F;

void AdjustFloatSetting(
    float& value,
    float delta,
    float min_value,
    float max_value,
    State& state,
    Audio& audio
) {
    const float adjusted = std::clamp(value + delta, min_value, max_value);
    if (adjusted == value) {
        PlayMenuSoundCant(audio);
        return;
    }

    value = adjusted;
    SaveSettings(state.settings);
    PlayMenuSoundCursorMove(audio);
}

} // namespace

const char* GetLightingSettingsMenuOptionName(LightingSettingsMenuOption option) {
    switch (option) {
    case LightingSettingsMenuOption::TerrainLighting:
        return "Terrain Lighting";
    case LightingSettingsMenuOption::TerrainFaceShading:
        return "Terrain Face Shading";
    case LightingSettingsMenuOption::TerrainEnclosedStageBounds:
        return "Enclosed Stage Bounds";
    case LightingSettingsMenuOption::TerrainTopHighlight:
        return "Terrain Top Highlight";
    case LightingSettingsMenuOption::TerrainSideShade:
        return "Terrain Side Shade";
    case LightingSettingsMenuOption::TerrainBottomShade:
        return "Terrain Bottom Shade";
    case LightingSettingsMenuOption::TerrainBandSize:
        return "Terrain Band Size";
    case LightingSettingsMenuOption::TerrainGradientSoftness:
        return "Terrain Gradient Softness";
    case LightingSettingsMenuOption::TerrainCornerRounding:
        return "Terrain Corner Rounding";
    case LightingSettingsMenuOption::TerrainSeamAo:
        return "Terrain Seam AO";
    case LightingSettingsMenuOption::TerrainSeamAoAmount:
        return "Terrain Seam AO Amount";
    case LightingSettingsMenuOption::TerrainSeamAoSize:
        return "Terrain Seam AO Size";
    case LightingSettingsMenuOption::TerrainExposureLighting:
        return "Terrain Exposure Lighting";
    case LightingSettingsMenuOption::TerrainExposureAmount:
        return "Terrain Exposure Amount";
    case LightingSettingsMenuOption::TerrainExposureMinBrightness:
        return "Terrain Exposure Min Brightness";
    case LightingSettingsMenuOption::TerrainExposureMaxBrightness:
        return "Terrain Exposure Max Brightness";
    case LightingSettingsMenuOption::TerrainExposureDiagonalWeight:
        return "Terrain Exposure Diagonal Weight";
    case LightingSettingsMenuOption::TerrainExposureSmoothing:
        return "Terrain Exposure Smoothing";
    case LightingSettingsMenuOption::BackwallLighting:
        return "Backwall Lighting";
    case LightingSettingsMenuOption::BackwallBrightness:
        return "Backwall Brightness";
    case LightingSettingsMenuOption::BackwallMinBrightness:
        return "Backwall Min Brightness";
    case LightingSettingsMenuOption::BackwallMaxBrightness:
        return "Backwall Max Brightness";
    case LightingSettingsMenuOption::BackwallSmoothing:
        return "Backwall Smoothing";
    case LightingSettingsMenuOption::Back:
        return "Back";
    }

    return "";
}

void ProcessInputLightingSettingsMenu(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
) {
    (void)window;
    (void)graphics;
    (void)dt;

    const bool confirm_pressed = state.menu_inputs.confirm.pressed;
    const bool up_pressed = state.menu_inputs.up.pressed;
    const bool down_pressed = state.menu_inputs.down.pressed;
    const bool left_pressed = state.menu_inputs.left.pressed;
    const bool right_pressed = state.menu_inputs.right.pressed;
    bool invalidate_terrain_lighting_cache = false;

    switch (state.lighting_settings_menu_selection) {
    case LightingSettingsMenuOption::TerrainLighting:
        if (up_pressed) {
            PlayMenuSoundCant(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainFaceShading;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed || right_pressed || confirm_pressed) {
            state.settings.post_process.terrain_lighting =
                !state.settings.post_process.terrain_lighting;
            SaveSettings(state.settings);
            PlayMenuSoundCursorMove(audio);
        }
        break;
    case LightingSettingsMenuOption::TerrainFaceShading:
        if (up_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainLighting;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainEnclosedStageBounds;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed || right_pressed || confirm_pressed) {
            state.settings.post_process.terrain_face_shading =
                !state.settings.post_process.terrain_face_shading;
            SaveSettings(state.settings);
            PlayMenuSoundCursorMove(audio);
        }
        break;
    case LightingSettingsMenuOption::TerrainEnclosedStageBounds:
        if (up_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainFaceShading;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainTopHighlight;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed || right_pressed || confirm_pressed) {
            state.settings.post_process.terrain_face_enclosed_stage_bounds =
                !state.settings.post_process.terrain_face_enclosed_stage_bounds;
            SaveSettings(state.settings);
            PlayMenuSoundCursorMove(audio);
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::TerrainTopHighlight:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainEnclosedStageBounds;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainSideShade;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_top_highlight,
                -kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_top_highlight,
                kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case LightingSettingsMenuOption::TerrainSideShade:
        if (up_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainTopHighlight;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainBottomShade;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_side_shade,
                -kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_side_shade,
                kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case LightingSettingsMenuOption::TerrainBottomShade:
        if (up_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainSideShade;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainBandSize;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_bottom_shade,
                -kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_bottom_shade,
                kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case LightingSettingsMenuOption::TerrainBandSize:
        if (up_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainBottomShade;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainGradientSoftness;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_band_size,
                -kLightingStep,
                0.05F,
                0.50F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_band_size,
                kLightingStep,
                0.05F,
                0.50F,
                state,
                audio
            );
        }
        break;
    case LightingSettingsMenuOption::TerrainGradientSoftness:
        if (up_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainBandSize;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainCornerRounding;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_gradient_softness,
                -kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_gradient_softness,
                kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case LightingSettingsMenuOption::TerrainCornerRounding:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainGradientSoftness;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainSeamAo;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_corner_rounding,
                -kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_face_corner_rounding,
                kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case LightingSettingsMenuOption::TerrainSeamAo:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainCornerRounding;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainSeamAoAmount;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed || right_pressed || confirm_pressed) {
            state.settings.post_process.terrain_seam_ao =
                !state.settings.post_process.terrain_seam_ao;
            SaveSettings(state.settings);
            PlayMenuSoundCursorMove(audio);
        }
        break;
    case LightingSettingsMenuOption::TerrainSeamAoAmount:
        if (up_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainSeamAo;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainSeamAoSize;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_seam_ao_amount,
                -kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_seam_ao_amount,
                kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case LightingSettingsMenuOption::TerrainSeamAoSize:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainSeamAoAmount;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureLighting;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_seam_ao_size,
                -kLightingStep,
                0.05F,
                0.50F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_seam_ao_size,
                kLightingStep,
                0.05F,
                0.50F,
                state,
                audio
            );
        }
        break;
    case LightingSettingsMenuOption::TerrainExposureLighting:
        if (up_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainSeamAoSize;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureAmount;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed || right_pressed || confirm_pressed) {
            state.settings.post_process.terrain_exposure_lighting =
                !state.settings.post_process.terrain_exposure_lighting;
            SaveSettings(state.settings);
            PlayMenuSoundCursorMove(audio);
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::TerrainExposureAmount:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureLighting;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureMinBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_exposure_amount,
                -kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_exposure_amount,
                kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::TerrainExposureMinBrightness:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureAmount;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureMaxBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_exposure_min_brightness,
                -kLightingStep,
                0.0F,
                2.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_exposure_min_brightness,
                kLightingStep,
                0.0F,
                2.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::TerrainExposureMaxBrightness:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureMinBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_exposure_max_brightness,
                -kLightingStep,
                0.0F,
                2.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_exposure_max_brightness,
                kLightingStep,
                0.0F,
                2.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureDiagonalWeight;
            PlayMenuSoundCursorMove(audio);
        }
        break;
    case LightingSettingsMenuOption::TerrainExposureDiagonalWeight:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureMaxBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureSmoothing;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_exposure_diagonal_weight,
                -kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_exposure_diagonal_weight,
                kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::TerrainExposureSmoothing:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureDiagonalWeight;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::BackwallLighting;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_exposure_smoothing,
                -kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.terrain_exposure_smoothing,
                kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::BackwallLighting:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::TerrainExposureSmoothing;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::BackwallBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed || right_pressed || confirm_pressed) {
            state.settings.post_process.backwall_lighting =
                !state.settings.post_process.backwall_lighting;
            SaveSettings(state.settings);
            PlayMenuSoundCursorMove(audio);
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::BackwallBrightness:
        if (up_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::BackwallLighting;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::BackwallMinBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.backwall_brightness,
                -kLightingStep,
                0.0F,
                2.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.backwall_brightness,
                kLightingStep,
                0.0F,
                2.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::BackwallMinBrightness:
        if (up_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::BackwallBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::BackwallMaxBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.backwall_min_brightness,
                -kLightingStep,
                0.0F,
                2.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.backwall_min_brightness,
                kLightingStep,
                0.0F,
                2.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::BackwallMaxBrightness:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::BackwallMinBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::BackwallSmoothing;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.backwall_max_brightness,
                -kLightingStep,
                0.0F,
                2.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.backwall_max_brightness,
                kLightingStep,
                0.0F,
                2.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::BackwallSmoothing:
        if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::BackwallMaxBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.lighting_settings_menu_selection = LightingSettingsMenuOption::Back;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.backwall_smoothing,
                -kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.backwall_smoothing,
                kLightingStep,
                0.0F,
                1.0F,
                state,
                audio
            );
            invalidate_terrain_lighting_cache = true;
        }
        break;
    case LightingSettingsMenuOption::Back:
        if (confirm_pressed) {
            state.SetMode(Mode::Settings);
            PlayMenuSoundCant(audio);
        } else if (up_pressed) {
            state.lighting_settings_menu_selection =
                LightingSettingsMenuOption::BackwallSmoothing;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            PlayMenuSoundCant(audio);
        }
        break;
    }

    if (invalidate_terrain_lighting_cache) {
        InvalidateTerrainLightingCache(state);
    }
}

} // namespace splonks
