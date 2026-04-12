#include "render/render.hpp"

#include "graphics.hpp"
#include "render/debug.hpp"
#include "render/gameplay.hpp"
#include "render/menus.hpp"
#include "render/postfx.hpp"
#include "render/ui.hpp"
#include "state.hpp"

namespace splonks {

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
