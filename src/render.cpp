#include "render.hpp"

#include "graphics.hpp"
#include "menu_settings.hpp"
#include "menu_title.hpp"
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
    RenderEntities(renderer, state, graphics);
    RenderSpecialEffects(renderer, state, graphics);
    RenderHealthRopeBombs(renderer, state, graphics);
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

void Render(SDL_Renderer* renderer, SDL_Texture* render_texture, State& state, Graphics& graphics) {
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
    SDL_RenderTexture(renderer, render_texture, &src, &dst);

    if (state.mode == Mode::Playing) {
        if (state.show_entity_collision_boxes) {
            RenderEntityCollisionBoxes(renderer, graphics, state);
        }
        PrintCtrlsHelp(
            renderer,
            graphics,
            static_cast<unsigned int>(output_height > 56 ? output_height - 56 : 0)
        );
    }
}

} // namespace splonks
