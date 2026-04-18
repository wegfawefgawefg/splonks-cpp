#include "render/ui.hpp"

#include "buying.hpp"
#include "entities/basic_exit.hpp"
#include "entity.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "tools/tool_archetype.hpp"
#include "state.hpp"
#include "step.hpp"
#include "text.hpp"

#include <algorithm>
#include <cstdio>

namespace splonks {

namespace {

FrameDataId GetToolIconAnimationId(ToolKind kind) {
    return GetToolArchetype(kind).icon_animation_id;
}

FrameDataId GetToolSlotBackgroundAnimationId(std::size_t slot_index) {
    switch (slot_index) {
    case 0:
        return frame_data_ids::ToolSlot1;
    case 1:
        return frame_data_ids::ToolSlot2;
    default:
        return kInvalidFrameDataId;
    }
}

const char* GetHudStageLabel(StageType stage_type) {
    switch (stage_type) {
    case StageType::Blank:
        return "Blank";
    case StageType::Test1:
        return "Test1";
    case StageType::SplkMines1:
        return "Mines";
    case StageType::SplkMines2:
        return "Mines 2";
    case StageType::SplkMines3:
        return "Mines 3";
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

void FormatHudTimer(char* out, std::size_t out_size, std::uint32_t frame_count) {
    const std::uint32_t total_tenths = (frame_count * 10U) / kFramesPerSecond;
    const std::uint32_t minutes = total_tenths / 600U;
    const std::uint32_t seconds = (total_tenths / 10U) % 60U;
    const std::uint32_t tenths = total_tenths % 10U;
    std::snprintf(out, out_size, "%02u:%02u.%u", minutes, seconds, tenths);
}

std::size_t GetUiAnimationFrameIndex(
    const FrameDataAnimation& animation,
    const FrameDataDb& frame_data_db,
    std::uint64_t tick
) {
    if (animation.frame_indices.empty()) {
        return 0;
    }

    std::uint64_t total_duration = 0;
    for (std::size_t frame_index : animation.frame_indices) {
        total_duration += static_cast<std::uint64_t>(
            std::max(frame_data_db.frames[frame_index].duration, 1)
        );
    }
    if (total_duration == 0) {
        return 0;
    }

    std::uint64_t local_tick = tick % total_duration;
    for (std::size_t ordered_index = 0; ordered_index < animation.frame_indices.size(); ++ordered_index) {
        const FrameData& frame_data = frame_data_db.frames[animation.frame_indices[ordered_index]];
        const std::uint64_t duration = static_cast<std::uint64_t>(std::max(frame_data.duration, 1));
        if (local_tick < duration) {
            return ordered_index;
        }
        local_tick -= duration;
    }

    return 0;
}

void DrawFrameDataIcon(
    SDL_Renderer* renderer,
    const State& state,
    Graphics& graphics,
    FrameDataId animation_id,
    const IVec2& cursor,
    const IVec2& size
) {
    const FrameDataAnimation* const animation = graphics.frame_data_db.FindAnimation(animation_id);
    if (animation == nullptr || animation->frame_indices.empty()) {
        return;
    }

    const std::size_t ordered_frame_index =
        GetUiAnimationFrameIndex(*animation, graphics.frame_data_db, state.scene_frame);
    if (ordered_frame_index >= animation->frame_indices.size()) {
        return;
    }

    const FrameData& frame_data =
        graphics.frame_data_db.frames[animation->frame_indices[ordered_frame_index]];
    SDL_Texture* const texture = graphics.GetFrameDataTexture(frame_data.image_id);
    if (texture == nullptr) {
        return;
    }

    const SDL_FRect src{
        static_cast<float>(frame_data.sample_rect.x),
        static_cast<float>(frame_data.sample_rect.y),
        static_cast<float>(frame_data.sample_rect.w),
        static_cast<float>(frame_data.sample_rect.h),
    };
    const SDL_FRect dst{
        static_cast<float>(cursor.x),
        static_cast<float>(cursor.y),
        static_cast<float>(size.x),
        static_cast<float>(size.y),
    };
    SDL_RenderTexture(renderer, texture, &src, &dst);
}

Vec2 GetUiCountTextPosition(const IVec2& icon_cursor, const IVec2& icon_size) {
    return Vec2::New(
        static_cast<float>(icon_cursor.x + icon_size.x),
        static_cast<float>(icon_cursor.y)
    );
}

void DrawRightAlignedUiText(
    SDL_Renderer* renderer,
    Graphics& graphics,
    const char* text,
    float right_x,
    float y,
    SDL_Color color
) {
    int text_width = 0;
    int text_height = 0;
    MeasureText(graphics, 40, graphics.ui_font, text, &text_width, &text_height);
    DrawText(
        renderer,
        graphics,
        40,
        graphics.ui_font,
        text,
        right_x - static_cast<float>(text_width),
        y,
        color
    );
}

void DrawDownArrowIcon(
    SDL_Renderer* renderer,
    float x,
    float y,
    float width,
    float height,
    SDL_Color color
) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    const float shaft_width = std::max(2.0F, width * 0.16F);
    const float head_y = y + (height * 0.58F);
    const SDL_FRect shaft = SDL_FRect{
        x + ((width - shaft_width) * 0.5F),
        y,
        shaft_width,
        std::max(2.0F, head_y - y)
    };
    SDL_RenderFillRect(renderer, &shaft);

    const float center_x = x + (width * 0.5F);
    const float left_x = x;
    const float right_x = x + width;
    const float tip_y = y + height;
    SDL_RenderLine(renderer, left_x, head_y, center_x, tip_y);
    SDL_RenderLine(renderer, center_x, tip_y, right_x, head_y);
}

void DrawPromptBubble(
    SDL_Renderer* renderer,
    const SDL_FRect& bubble_rect,
    float tip_x,
    float tip_y,
    SDL_Color fill_color,
    SDL_Color outline_color
) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b, fill_color.a);
    SDL_RenderFillRect(renderer, &bubble_rect);

    const float half_base = std::max(6.0F, bubble_rect.h * 0.18F);
    const float base_y = bubble_rect.y + bubble_rect.h;
    const int fill_steps = std::max(1, static_cast<int>(std::ceil(tip_y - base_y)));
    for (int step = 0; step <= fill_steps; ++step) {
        const float t = fill_steps > 0 ? static_cast<float>(step) / static_cast<float>(fill_steps) : 0.0F;
        const float y = base_y + ((tip_y - base_y) * t);
        const float left_x = (tip_x - half_base) + (half_base * t);
        const float right_x = (tip_x + half_base) - (half_base * t);
        SDL_RenderLine(renderer, left_x, y, right_x, y);
    }

    SDL_SetRenderDrawColor(renderer, outline_color.r, outline_color.g, outline_color.b, outline_color.a);
    SDL_RenderRect(renderer, &bubble_rect);
    SDL_RenderLine(renderer, tip_x - half_base, base_y, tip_x, tip_y);
    SDL_RenderLine(renderer, tip_x, tip_y, tip_x + half_base, base_y);
}

} // namespace

/**
 * Atlas sample row is determined by entity_type,
 * Atlas sample col is determined by entity display state, +1 if animation frame is B
 */
void RenderPlayingHud(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    unsigned int health = 0;
    unsigned int money = 0;
    std::optional<VID> player_vid;
    if (state.player_vid.has_value()) {
        if (const Entity* const player = state.entity_manager.GetEntity(*state.player_vid)) {
            health = player->health;
            money = player->money;
            player_vid = player->vid;
        }
    }

    const float hud_icon_scale = std::clamp(state.settings.ui.icon_scale, 0.25F, 1.50F);
    const float status_icon_scale =
        std::clamp(state.settings.ui.status_icon_scale, 0.25F, 1.50F) * hud_icon_scale;
    const float tool_slot_scale =
        std::clamp(state.settings.ui.tool_slot_scale, 0.25F, 1.50F) * hud_icon_scale;
    const float tool_icon_scale =
        std::clamp(state.settings.ui.tool_icon_scale, 0.25F, 1.50F) * hud_icon_scale;
    const int base_icon_size = std::max(
        1,
        static_cast<int>((static_cast<float>(graphics.dims.x) / 20.0F) * hud_icon_scale)
    );
    const int hud_margin = std::max(8, static_cast<int>(graphics.dims.y) / 60);
    const int hud_gap = std::max(6, hud_margin / 2);
    const IVec2 status_icon_size = IVec2::New(
        std::max(1, static_cast<int>(static_cast<float>(base_icon_size) * status_icon_scale)),
        std::max(1, static_cast<int>(static_cast<float>(base_icon_size) * status_icon_scale))
    );
    const IVec2 tool_slot_size = IVec2::New(
        std::max(1, static_cast<int>(static_cast<float>(base_icon_size) * tool_slot_scale)),
        std::max(1, static_cast<int>(static_cast<float>(base_icon_size) * tool_slot_scale))
    );
    const IVec2 tool_icon_size = IVec2::New(
        std::max(1, static_cast<int>(static_cast<float>(base_icon_size) * tool_icon_scale)),
        std::max(1, static_cast<int>(static_cast<float>(base_icon_size) * tool_icon_scale))
    );
    const IVec2 status_cursor_advance = IVec2::New(status_icon_size.x + hud_gap, 0);
    const IVec2 tool_cursor_advance = IVec2::New(tool_slot_size.x + hud_gap, 0);
    IVec2 cursor = IVec2::New(hud_margin, hud_margin);

    {
        // HEALTH
        // draw heart
        const IVec2 icon_cursor = cursor;
        DrawFrameDataIcon(
            renderer,
            state,
            graphics,
            frame_data_ids::HeartUiIcon,
            icon_cursor,
            status_icon_size
        );

        // draw health
        char text[32];
        std::snprintf(text, sizeof(text), "%u", health);
        const Vec2 text_pos = GetUiCountTextPosition(icon_cursor, status_icon_size);
        DrawText(
            renderer,
            graphics,
            40,
            graphics.ui_font,
            text,
            text_pos.x,
            text_pos.y,
            SDL_Color{255, 255, 255, 255}
        );
    }
    cursor = cursor + (status_cursor_advance * 3);

    // TOOLS
    if (player_vid.has_value()) {
        for (std::size_t slot_index = 0; slot_index < kToolSlotCount; ++slot_index) {
            const IVec2 icon_cursor = cursor;
            DrawFrameDataIcon(
                renderer,
                state,
                graphics,
                GetToolSlotBackgroundAnimationId(slot_index),
                icon_cursor,
                tool_slot_size
            );

            const ToolSlot* const slot = state.entity_tools.FindToolSlot(*player_vid, slot_index);
            if (slot != nullptr && slot->active) {
                const IVec2 tool_icon_cursor = IVec2::New(
                    icon_cursor.x + (tool_slot_size.x - tool_icon_size.x) / 2,
                    icon_cursor.y + (tool_slot_size.y - tool_icon_size.y) / 2
                );
                DrawFrameDataIcon(
                    renderer,
                    state,
                    graphics,
                    GetToolIconAnimationId(slot->kind),
                    tool_icon_cursor,
                    tool_icon_size
                );

                char text[32];
                std::snprintf(text, sizeof(text), "%u", static_cast<unsigned int>(slot->count));
                const Vec2 text_pos = GetUiCountTextPosition(icon_cursor, tool_slot_size);
                DrawText(
                    renderer,
                    graphics,
                    40,
                    graphics.ui_font,
                    text,
                    text_pos.x,
                    text_pos.y,
                    SDL_Color{255, 255, 255, 255}
                );
            }

            cursor = cursor + (tool_cursor_advance * 3);
        }
    }

    // Top-right HUD block: money, timer, stage
    char money_text[32];
    std::snprintf(money_text, sizeof(money_text), "%u", money);
    int money_text_width = 0;
    int money_text_height = 0;
    MeasureText(graphics, 40, graphics.ui_font, money_text, &money_text_width, &money_text_height);

    const int money_block_width = status_icon_size.x + hud_gap + money_text_width;
    const IVec2 gold_icon_cursor = IVec2::New(
        static_cast<int>(graphics.dims.x) - hud_margin - money_block_width,
        hud_margin
    );
    DrawFrameDataIcon(
        renderer,
        state,
        graphics,
        frame_data_ids::GoldIcon,
        gold_icon_cursor,
        status_icon_size
    );
    {
        const Vec2 text_pos = GetUiCountTextPosition(gold_icon_cursor, status_icon_size);
        DrawText(
            renderer,
            graphics,
            40,
            graphics.ui_font,
            money_text,
            text_pos.x,
            text_pos.y,
            SDL_Color{255, 255, 255, 255}
        );
    }

    char stage_text[32];
    std::snprintf(stage_text, sizeof(stage_text), "%s", GetHudStageLabel(state.stage.stage_type));
    DrawRightAlignedUiText(
        renderer,
        graphics,
        stage_text,
        static_cast<float>(static_cast<int>(graphics.dims.x) - hud_margin),
        static_cast<float>(hud_margin + status_icon_size.y + hud_gap),
        SDL_Color{255, 255, 255, 255}
    );

    char stage_timer_text[32];
    FormatHudTimer(stage_timer_text, sizeof(stage_timer_text), state.stage_frame);
    DrawRightAlignedUiText(
        renderer,
        graphics,
        stage_timer_text,
        static_cast<float>(static_cast<int>(graphics.dims.x) - hud_margin),
        static_cast<float>(hud_margin + status_icon_size.y + hud_gap + 34),
        SDL_Color{255, 255, 255, 255}
    );

}

void RenderWorldPrompts(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
    const float hud_icon_scale = std::clamp(state.settings.ui.icon_scale, 0.25F, 1.50F);
    const float status_icon_scale =
        std::clamp(state.settings.ui.status_icon_scale, 0.25F, 1.50F) * hud_icon_scale;
    const int base_icon_size = std::max(
        1,
        static_cast<int>((static_cast<float>(graphics.dims.x) / 20.0F) * hud_icon_scale)
    );
    const int hud_margin = std::max(8, static_cast<int>(graphics.dims.y) / 60);
    const int hud_gap = std::max(6, hud_margin / 2);
    const IVec2 status_icon_size = IVec2::New(
        std::max(1, static_cast<int>(static_cast<float>(base_icon_size) * status_icon_scale)),
        std::max(1, static_cast<int>(static_cast<float>(base_icon_size) * status_icon_scale))
    );
    const SDL_Color prompt_text_color = SDL_Color{255, 255, 255, 255};
    const SDL_Color prompt_fill_color = SDL_Color{18, 20, 28, 220};
    const SDL_Color prompt_outline_color = SDL_Color{220, 225, 235, 255};
    constexpr int kPromptFontSize = 28;
    constexpr float kPromptIconYOffset = 4.0F;
    constexpr float kPromptWorldYOffset = 20.0F;
    constexpr float kPromptPaddingX = 10.0F;
    constexpr float kPromptPaddingY = 6.0F;
    constexpr float kPromptTipHeight = 8.0F;
    const int prompt_gap = std::max(8, hud_gap);

    for (std::size_t prompt_idx = 0; prompt_idx < state.world_prompts.size(); ++prompt_idx) {
        const WorldPrompt& prompt = state.world_prompts[prompt_idx];
        const bool show_action = prompt.action_text[0] != '\0';
        const bool show_message = prompt.message_text[0] != '\0';
        const bool show_arrow = prompt.show_down_arrow;
        const bool show_quantity = prompt.quantity > 0;
        const bool show_icon = prompt.icon_animation_id.has_value();
        if (!show_action && !show_message && !show_arrow && !show_quantity && !show_icon) {
            continue;
        }

        char quantity_text[32];
        quantity_text[0] = '\0';
        if (show_quantity) {
            std::snprintf(quantity_text, sizeof(quantity_text), "%u", prompt.quantity);
        }

        int action_width = 0;
        int action_height = 0;
        int message_width = 0;
        int message_height = 0;
        int quantity_width = 0;
        int quantity_height = 0;
        if (show_action) {
            MeasureText(
                graphics,
                kPromptFontSize,
                graphics.ui_font,
                prompt.action_text,
                &action_width,
                &action_height
            );
        }
        if (show_message) {
            MeasureText(
                graphics,
                kPromptFontSize,
                graphics.ui_font,
                prompt.message_text,
                &message_width,
                &message_height
            );
        }
        if (show_quantity) {
            MeasureText(
                graphics,
                kPromptFontSize,
                graphics.ui_font,
                quantity_text,
                &quantity_width,
                &quantity_height
            );
        }

        const IVec2 prompt_icon_size = IVec2::New(
            std::max(1, status_icon_size.x / 2),
            std::max(1, status_icon_size.y / 2)
        );
        const int content_height = std::max(
            {action_height, message_height, quantity_height, prompt_icon_size.y, 16}
        );
        const float down_arrow_width =
            show_arrow ? std::max(14.0F, static_cast<float>(content_height) * 0.75F) : 0.0F;
        const float down_arrow_height =
            show_arrow ? std::max(16.0F, static_cast<float>(content_height)) : 0.0F;

        float content_width = 0.0F;
        auto add_segment = [&](float width) {
            if (width <= 0.0F) {
                return;
            }
            if (content_width > 0.0F) {
                content_width += static_cast<float>(prompt_gap);
            }
            content_width += width;
        };
        if (show_action) {
            add_segment(static_cast<float>(action_width));
        }
        if (show_arrow) {
            add_segment(down_arrow_width);
        }
        if (show_icon) {
            add_segment(static_cast<float>(prompt_icon_size.x));
        }
        if (show_quantity) {
            add_segment(static_cast<float>(quantity_width));
        }
        if (show_message) {
            add_segment(static_cast<float>(message_width));
        }

        const float bob =
            std::sin((static_cast<float>(state.scene_frame) * 0.08F) +
                     static_cast<float>(prompt_idx) * 0.7F) *
            3.0F;
        const Vec2 screen_anchor = graphics.WcToScreen(prompt.world_pos) + Vec2::New(0.0F, bob);
        const float bubble_width = content_width + (kPromptPaddingX * 2.0F);
        const float bubble_height = static_cast<float>(content_height) + (kPromptPaddingY * 2.0F);
        const SDL_FRect bubble_rect{
            std::round(screen_anchor.x - (bubble_width * 0.5F)),
            std::round(screen_anchor.y - bubble_height - kPromptWorldYOffset),
            std::round(bubble_width),
            std::round(bubble_height),
        };
        const float tip_x = std::round(screen_anchor.x);
        const float tip_y = std::round(bubble_rect.y + bubble_rect.h + kPromptTipHeight);

        DrawPromptBubble(renderer, bubble_rect, tip_x, tip_y, prompt_fill_color, prompt_outline_color);

        float cursor_x = bubble_rect.x + kPromptPaddingX;
        const float prompt_y = bubble_rect.y + kPromptPaddingY;
        auto advance_gap_if_needed = [&](bool& first_segment) {
            if (!first_segment) {
                cursor_x += static_cast<float>(prompt_gap);
            }
            first_segment = false;
        };

        bool first_segment = true;
        if (show_action) {
            advance_gap_if_needed(first_segment);
            DrawText(
                renderer,
                graphics,
                kPromptFontSize,
                graphics.ui_font,
                prompt.action_text,
                cursor_x,
                prompt_y,
                prompt_text_color
            );
            cursor_x += static_cast<float>(action_width);
        }
        if (show_arrow) {
            advance_gap_if_needed(first_segment);
            DrawDownArrowIcon(
                renderer,
                cursor_x,
                prompt_y + 2.0F,
                down_arrow_width,
                down_arrow_height,
                prompt_text_color
            );
            cursor_x += down_arrow_width;
        }
        if (show_icon) {
            advance_gap_if_needed(first_segment);
            DrawFrameDataIcon(
                renderer,
                state,
                graphics,
                *prompt.icon_animation_id,
                IVec2::New(
                    static_cast<int>(cursor_x),
                    static_cast<int>(prompt_y + kPromptIconYOffset)
                ),
                prompt_icon_size
            );
            cursor_x += static_cast<float>(prompt_icon_size.x);
        }
        if (show_quantity) {
            advance_gap_if_needed(first_segment);
            DrawText(
                renderer,
                graphics,
                kPromptFontSize,
                graphics.ui_font,
                quantity_text,
                cursor_x,
                prompt_y,
                prompt_text_color
            );
            cursor_x += static_cast<float>(quantity_width);
        }
        if (show_message) {
            advance_gap_if_needed(first_segment);
            DrawText(
                renderer,
                graphics,
                kPromptFontSize,
                graphics.ui_font,
                prompt.message_text,
                cursor_x,
                prompt_y,
                prompt_text_color
            );
        }
    }
}

} // namespace splonks
