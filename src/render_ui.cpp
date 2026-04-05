#include "render_ui.hpp"

#include "entity.hpp"
#include "graphics.hpp"
#include "state.hpp"
#include "text.hpp"

#include <cstdio>

namespace splonks {

namespace {

const char* ToDebugName(StageType stage_type) {
    switch (stage_type) {
    case StageType::Blank:
        return "Blank";
    case StageType::Test1:
        return "Test1";
    case StageType::Cave1:
        return "Cave1";
    case StageType::Cave2:
        return "Cave2";
    case StageType::Cave3:
        return "Cave3";
    case StageType::Ice1:
        return "Ice1";
    case StageType::Ice2:
        return "Ice2";
    case StageType::Ice3:
        return "Ice3";
    case StageType::Desert1:
        return "Desert1";
    case StageType::Desert2:
        return "Desert2";
    case StageType::Desert3:
        return "Desert3";
    case StageType::Temple1:
        return "Temple1";
    case StageType::Temple2:
        return "Temple2";
    case StageType::Temple3:
        return "Temple3";
    case StageType::Boss:
        return "Boss";
    }

    return "Unknown";
}

void DrawSpriteIcon(
    SDL_Renderer* renderer,
    Graphics& graphics,
    Sprite sprite,
    const IVec2& cursor,
    const IVec2& size
) {
    const SpriteData& sprite_data = graphics.GetSpriteData(sprite);
    SDL_Texture* texture = graphics.GetSpriteTexture(sprite);
    if (texture == nullptr || sprite_data.frames.empty()) {
        return;
    }
    const Frame& first_frame = sprite_data.frames[0];
    const SDL_FRect src{
        static_cast<float>(first_frame.sample_position.x),
        static_cast<float>(first_frame.sample_position.y),
        static_cast<float>(sprite_data.size.x),
        static_cast<float>(sprite_data.size.y),
    };
    const SDL_FRect dst{
        static_cast<float>(cursor.x),
        static_cast<float>(cursor.y),
        static_cast<float>(size.x),
        static_cast<float>(size.y),
    };
    SDL_RenderTexture(renderer, texture, &src, &dst);
}

} // namespace

/**
 * Atlas sample row is determined by entity_type,
 * Atlas sample col is determined by entity display state, +1 if animation frame is B
 */
void RenderHealthRopeBombs(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
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

    const int five_percent = static_cast<int>(graphics.dims.x / 20U);
    const int two_point_five_percent = five_percent / 2;
    const IVec2 size = IVec2::New(five_percent, five_percent);
    const IVec2 cursor_advance = IVec2::New(five_percent, 0);
    IVec2 cursor = IVec2::New(0, five_percent);
    cursor = cursor + cursor_advance;

    {
        // HEALTH
        // draw heart
        DrawSpriteIcon(renderer, graphics, Sprite::Heart, cursor, size);
    }
    cursor = cursor + cursor_advance;
    {
        // draw health
        char text[32];
        std::snprintf(text, sizeof(text), "%u", health);
        DrawText(
            renderer,
            graphics,
            40,
            graphics.ui_font,
            text,
            static_cast<float>(cursor.x),
            static_cast<float>(cursor.y + two_point_five_percent),
            SDL_Color{255, 255, 255, 255}
        );
    }
    cursor = cursor + (cursor_advance * 2);

    // BOMB
    // draw bomb
    DrawSpriteIcon(renderer, graphics, Sprite::Bomb, cursor, size);
    cursor = cursor + cursor_advance;
    {
        // draw num boms
        char text[32];
        std::snprintf(text, sizeof(text), "%u", bombs);
        DrawText(
            renderer,
            graphics,
            40,
            graphics.ui_font,
            text,
            static_cast<float>(cursor.x),
            static_cast<float>(cursor.y + two_point_five_percent),
            SDL_Color{255, 255, 255, 255}
        );
    }
    cursor = cursor + (cursor_advance * 2);

    // ROPE
    // draw rope
    DrawSpriteIcon(renderer, graphics, Sprite::RopeIcon, cursor, size);
    cursor = cursor + cursor_advance;
    {
        // draw num ropes
        char text[32];
        std::snprintf(text, sizeof(text), "%u", ropes);
        DrawText(
            renderer,
            graphics,
            40,
            graphics.ui_font,
            text,
            static_cast<float>(cursor.x),
            static_cast<float>(cursor.y + two_point_five_percent),
            SDL_Color{255, 255, 255, 255}
        );
    }

    // Money
    // draw gold
    cursor = IVec2::New(static_cast<int>(graphics.dims.x) - (five_percent * 3), five_percent);
    DrawSpriteIcon(renderer, graphics, Sprite::GoldIcon, cursor, size);
    cursor = cursor + cursor_advance;
    {
        // draw num ropes
        char text[32];
        std::snprintf(text, sizeof(text), "%u", money);
        DrawText(
            renderer,
            graphics,
            40,
            graphics.ui_font,
            text,
            static_cast<float>(cursor.x),
            static_cast<float>(cursor.y + two_point_five_percent),
            SDL_Color{255, 255, 255, 255}
        );
    }

    // draw the stage type in the bottom left corner
    // put the cursor at the bottom left
    cursor = IVec2::New(five_percent, static_cast<int>(graphics.dims.y) - five_percent);
    {
        // draw the stage type
        char text[64];
        std::snprintf(text, sizeof(text), "Stage Type: %s", ToDebugName(state.stage.stage_type));
        DrawText(
            renderer,
            graphics,
            40,
            graphics.ui_font,
            text,
            static_cast<float>(cursor.x),
            static_cast<float>(cursor.y),
            SDL_Color{255, 255, 255, 255}
        );
    }
}

} // namespace splonks
