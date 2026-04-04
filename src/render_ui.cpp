#include "render_ui.hpp"

#include "entity.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "text.hpp"

#include <cstdio>

namespace splonks {

/**
 * Atlas sample row is determined by entity_type,
 * Atlas sample col is determined by entity display state, +1 if animation frame is B
 */
void RenderHealthRopeBombs(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    (void)graphics;
    unsigned int health = 0;
    unsigned int bombs = 0;
    unsigned int ropes = 0;
    unsigned int money = 0;
    if (state.player_vid.has_value()) {
        if (const Entity* const player = state.entity_manager.GetEntity(*state.player_vid)) {
            health = player->health;
            bombs = player->bombs;
            ropes = player->ropes;
            money = player->money;
        }
    }

    char line[128];
    std::snprintf(line, sizeof(line), "HP %u   Bombs %u   Ropes %u   Gold %u", health, bombs, ropes, money);
    DrawText(
        renderer,
        graphics,
        24,
        graphics.ui_font,
        line,
        8.0F,
        8.0F,
        SDL_Color{255, 255, 255, 255}
    );
}

} // namespace splonks
