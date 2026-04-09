#include "menu_postfx.hpp"

#include "audio.hpp"
#include "graphics.hpp"
#include "settings.hpp"
#include "state.hpp"

#include <algorithm>

namespace splonks {

namespace {

constexpr float kCrtNormalizedStep = 0.05F;
constexpr float kCrtBrightnessStep = 0.05F;

void CycleEffect(State& state, int delta, Audio& audio) {
    int effect = static_cast<int>(state.settings.post_process.effect);
    effect += delta;
    if (effect < static_cast<int>(PostProcessEffect::None)) {
        effect = static_cast<int>(PostProcessEffect::Crt);
    }
    if (effect > static_cast<int>(PostProcessEffect::Crt)) {
        effect = static_cast<int>(PostProcessEffect::None);
    }
    state.settings.post_process.effect = static_cast<PostProcessEffect>(effect);
    SaveSettings(state.settings);
    PlayMenuSoundCursorMove(audio);
}

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

const char* GetPostFxSettingsMenuOptionName(PostFxSettingsMenuOption option) {
    switch (option) {
    case PostFxSettingsMenuOption::Effect:
        return "Effect";
    case PostFxSettingsMenuOption::TerrainFaceShading:
    case PostFxSettingsMenuOption::TerrainTopHighlight:
    case PostFxSettingsMenuOption::TerrainSideShade:
    case PostFxSettingsMenuOption::TerrainBottomShade:
    case PostFxSettingsMenuOption::TerrainBandSize:
        return "Moved To Lighting";
    case PostFxSettingsMenuOption::CrtScanlines:
        return "CRT Scanlines";
    case PostFxSettingsMenuOption::CrtScanlineEdgeStart:
        return "CRT Scanline Edge Start";
    case PostFxSettingsMenuOption::CrtScanlineEdgeFalloff:
        return "CRT Scanline Edge Falloff";
    case PostFxSettingsMenuOption::CrtScanlineEdgeStrength:
        return "CRT Scanline Edge Strength";
    case PostFxSettingsMenuOption::CrtZoom:
        return "CRT Zoom";
    case PostFxSettingsMenuOption::CrtWarp:
        return "CRT Warp";
    case PostFxSettingsMenuOption::CrtVignette:
        return "CRT Vignette";
    case PostFxSettingsMenuOption::CrtVignetteIntensity:
        return "CRT Vignette Intensity";
    case PostFxSettingsMenuOption::CrtGrille:
        return "CRT Grille";
    case PostFxSettingsMenuOption::CrtBrightness:
        return "CRT Brightness";
    case PostFxSettingsMenuOption::Back:
        return "Back";
    }

    return "";
}

void ProcessInputPostFxSettingsMenu(
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

    switch (state.post_fx_settings_menu_selection) {
    case PostFxSettingsMenuOption::TerrainFaceShading:
    case PostFxSettingsMenuOption::TerrainTopHighlight:
    case PostFxSettingsMenuOption::TerrainSideShade:
    case PostFxSettingsMenuOption::TerrainBottomShade:
    case PostFxSettingsMenuOption::TerrainBandSize:
        state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::Effect;
        PlayMenuSoundCursorMove(audio);
        break;
    case PostFxSettingsMenuOption::Effect:
        if (up_pressed) {
            PlayMenuSoundCant(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtScanlines;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            CycleEffect(state, -1, audio);
        } else if (right_pressed) {
            CycleEffect(state, 1, audio);
        }
        break;
    case PostFxSettingsMenuOption::CrtScanlines:
        if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::Effect;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtScanlineEdgeStart;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_scanline_amount,
                -kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_scanline_amount,
                kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case PostFxSettingsMenuOption::CrtScanlineEdgeStart:
        if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtScanlines;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtScanlineEdgeFalloff;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_scanline_edge_start,
                -kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_scanline_edge_start,
                kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case PostFxSettingsMenuOption::CrtScanlineEdgeFalloff:
        if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtScanlineEdgeStart;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtScanlineEdgeStrength;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_scanline_edge_falloff,
                -kCrtNormalizedStep,
                0.01F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_scanline_edge_falloff,
                kCrtNormalizedStep,
                0.01F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case PostFxSettingsMenuOption::CrtScanlineEdgeStrength:
        if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtScanlineEdgeFalloff;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtZoom;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_scanline_edge_strength,
                -kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_scanline_edge_strength,
                kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case PostFxSettingsMenuOption::CrtZoom:
        if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtScanlineEdgeStrength;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtWarp;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_zoom,
                -kCrtNormalizedStep,
                0.50F,
                1.50F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_zoom,
                kCrtNormalizedStep,
                0.50F,
                1.50F,
                state,
                audio
            );
        }
        break;
    case PostFxSettingsMenuOption::CrtWarp:
        if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtZoom;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtVignette;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_warp_amount,
                -kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_warp_amount,
                kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case PostFxSettingsMenuOption::CrtVignette:
        if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtWarp;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtVignetteIntensity;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_vignette_amount,
                -kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_vignette_amount,
                kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case PostFxSettingsMenuOption::CrtVignetteIntensity:
        if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtVignette;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtGrille;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_vignette_intensity,
                -kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_vignette_intensity,
                kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case PostFxSettingsMenuOption::CrtGrille:
        if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtVignetteIntensity;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_grille_amount,
                -kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_grille_amount,
                kCrtNormalizedStep,
                0.0F,
                1.0F,
                state,
                audio
            );
        }
        break;
    case PostFxSettingsMenuOption::CrtBrightness:
        if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtGrille;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::Back;
            PlayMenuSoundCursorMove(audio);
        } else if (left_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_brightness_boost,
                -kCrtBrightnessStep,
                1.0F,
                2.0F,
                state,
                audio
            );
        } else if (right_pressed) {
            AdjustFloatSetting(
                state.settings.post_process.crt_brightness_boost,
                kCrtBrightnessStep,
                1.0F,
                2.0F,
                state,
                audio
            );
        }
        break;
    case PostFxSettingsMenuOption::Back:
        if (confirm_pressed) {
            state.SetMode(Mode::Settings);
            PlayMenuSoundCant(audio);
        } else if (up_pressed) {
            state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::CrtBrightness;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            PlayMenuSoundCant(audio);
        }
        break;
    }
}

} // namespace splonks
