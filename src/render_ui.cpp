#include "render_ui.hpp"

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

} // namespace

/**
 * Atlas sample row is determined by entity_type,
 * Atlas sample col is determined by entity display state, +1 if animation frame is B
 */
void RenderHealthRopeBombs(SDL_Renderer* renderer, const State& state, Graphics& graphics) {
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

            const ToolSlot* const slot = state.FindToolSlot(*player_vid, slot_index);
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

} // namespace splonks
