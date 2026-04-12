#include "render/menus.hpp"

#include "graphics.hpp"
#include "menu/settings.hpp"
#include "menu/title.hpp"
#include "menu/video.hpp"
#include "render/menu/common.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>

namespace splonks {

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

} // namespace splonks
