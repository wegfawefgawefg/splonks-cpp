#include "render/render.hpp"

#include "audio.hpp"
#include "graphics.hpp"
#include "render/debug.hpp"
#include "render/gameplay.hpp"
#include "render/menus.hpp"
#include "render/postfx.hpp"
#include "render/ui.hpp"
#include "state.hpp"
#include "text.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>
#include <string>

namespace splonks {

namespace {

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
        center_x - (static_cast<float>(text_width) * 0.5F),
        y - (static_cast<float>(text_height) * 0.5F),
        color
    );
}

const char* JoinBarrierPhaseText(network::JoinBarrierPhase phase) {
    switch (phase) {
    case network::JoinBarrierPhase::WaitingForCatchup:
        return "Waiting for catchup turn";
    case network::JoinBarrierPhase::SendingSnapshot:
        return "Sending world snapshot";
    case network::JoinBarrierPhase::WaitingForAck:
        return "Waiting for client apply";
    case network::JoinBarrierPhase::ReadyToResume:
        return "Resuming simulation";
    case network::JoinBarrierPhase::WaitingForResume:
        return "Waiting for resume";
    case network::JoinBarrierPhase::None:
        return "Idle";
    }
    return "Unknown";
}

bool IsLocalJoinBarrierPeer(const State& state) {
    const PlayerId active_peer_id = state.net_session.join_barrier_active_peer_id;
    if (active_peer_id == kInvalidPlayerId) {
        return false;
    }
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected &&
            slot.connection_kind == PlayerConnectionKind::Local &&
            slot.player_id == active_peer_id) {
            return true;
        }
    }
    return false;
}

bool IsQueuedJoinBarrierPeer(const State& state) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.connection_kind != PlayerConnectionKind::Local) {
            continue;
        }
        if (std::find(
                state.net_session.join_barrier_queue.begin(),
                state.net_session.join_barrier_queue.end(),
                slot.player_id
            ) != state.net_session.join_barrier_queue.end()) {
            return true;
        }
    }
    return false;
}

std::string LocalJoinBarrierClientText(const State& state) {
    const PlayerId local_player_id = state.net_session.local_player_id;
    if (state.net_session.role == network::NetRole::Host) {
        return "You: host / client " + std::to_string(local_player_id);
    }
    return "You: client " + std::to_string(local_player_id);
}

std::string ActiveJoinBarrierClientText(const State& state) {
    const PlayerId active_peer_id = state.net_session.join_barrier_active_peer_id;
    if (active_peer_id == kInvalidPlayerId) {
        return "Syncing: none";
    }
    return "Syncing: client " + std::to_string(active_peer_id);
}

std::string FormatClientList(const std::vector<PlayerId>& player_ids) {
    if (player_ids.empty()) {
        return "";
    }
    std::string result;
    for (std::size_t i = 0; i < player_ids.size(); ++i) {
        if (i > 0) {
            result += i + 1 == player_ids.size() ? " and " : ", ";
        }
        result += "client " + std::to_string(player_ids[i]);
    }
    return result;
}

std::string JoinBarrierChangeText(const State& state) {
    const network::NetSessionState& session = state.net_session;
    const std::string joined = FormatClientList(session.join_barrier_joined_player_ids);
    const std::string removed = FormatClientList(session.join_barrier_removed_player_ids);
    if (!joined.empty() && !removed.empty()) {
        return joined + " joined, " + removed + " left";
    }
    if (!joined.empty()) {
        return joined + " joined";
    }
    if (!removed.empty()) {
        return removed + " left";
    }
    return "";
}

std::string JoinBarrierStatusText(const State& state) {
    const network::NetSessionState& session = state.net_session;
    const PlayerId active_peer_id = session.join_barrier_active_peer_id;
    const bool has_active_peer = active_peer_id != kInvalidPlayerId;
    const std::string change_text = JoinBarrierChangeText(state);

    if (session.role == network::NetRole::Host) {
        if (!change_text.empty() && !has_active_peer) {
            return change_text;
        }
        return has_active_peer
            ? "Sending world snapshot"
            : "Preparing catchup";
    }

    if (IsLocalJoinBarrierPeer(state)) {
        return "Receiving world snapshot";
    }

    if (IsQueuedJoinBarrierPeer(state)) {
        return "Waiting for your turn";
    }

    if (!change_text.empty() && !has_active_peer) {
        return change_text;
    }

    return has_active_peer
        ? "Waiting for active sync"
        : "Waiting for catchup";
}

void RenderJoinBarrierOverlay(SDL_Renderer* renderer, State& state, Graphics& graphics) {
    const network::NetSessionState& session = state.net_session;
    if (!session.join_barrier_active ||
        session.join_barrier_phase == network::JoinBarrierPhase::None) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    const SDL_FRect overlay{
        0.0F,
        0.0F,
        static_cast<float>(graphics.window_dims.x),
        static_cast<float>(graphics.window_dims.y),
    };
    SDL_RenderFillRect(renderer, &overlay);

    const float center_x = static_cast<float>(graphics.window_dims.x) * 0.5F;
    const float center_y = static_cast<float>(graphics.window_dims.y) * 0.5F;
    const std::string local_client_text = LocalJoinBarrierClientText(state);
    const std::string active_client_text = ActiveJoinBarrierClientText(state);
    const std::string status_text = JoinBarrierStatusText(state);

    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuItem,
        local_client_text.c_str(),
        center_x,
        center_y - 76.0F,
        SDL_Color{255, 255, 255, 255}
    );
    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuItem,
        active_client_text.c_str(),
        center_x,
        center_y - 50.0F,
        SDL_Color{235, 240, 255, 255}
    );
    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuItem,
        status_text.c_str(),
        center_x,
        center_y - 24.0F,
        SDL_Color{225, 225, 225, 255}
    );

    constexpr float kBarWidth = 420.0F;
    constexpr float kBarHeight = 20.0F;
    const float denom = session.join_barrier_total_bytes > 0
        ? static_cast<float>(session.join_barrier_total_bytes)
        : static_cast<float>(std::max(1U, session.join_barrier_chunk_count));
    const float numer = session.join_barrier_total_bytes > 0
        ? static_cast<float>(session.join_barrier_bytes_done)
        : static_cast<float>(session.join_barrier_chunks_done);
    const float progress = std::clamp(numer / denom, 0.0F, 1.0F);
    const SDL_FRect bar_bg{
        center_x - kBarWidth * 0.5F,
        center_y - kBarHeight * 0.5F,
        kBarWidth,
        kBarHeight,
    };
    SDL_SetRenderDrawColor(renderer, 35, 40, 50, 240);
    SDL_RenderFillRect(renderer, &bar_bg);
    SDL_SetRenderDrawColor(renderer, 220, 220, 230, 255);
    SDL_RenderRect(renderer, &bar_bg);
    const SDL_FRect bar_fill{
        bar_bg.x + 2.0F,
        bar_bg.y + 2.0F,
        (bar_bg.w - 4.0F) * progress,
        bar_bg.h - 4.0F,
    };
    SDL_SetRenderDrawColor(renderer, 110, 170, 255, 255);
    SDL_RenderFillRect(renderer, &bar_fill);

    const std::string change_text = JoinBarrierChangeText(state);
    const std::string phase_text =
        std::string(JoinBarrierPhaseText(session.join_barrier_phase)) +
        " - world snapshot " +
        std::to_string(session.join_barrier_chunks_done) + "/" +
        std::to_string(session.join_barrier_chunk_count) + " chunks, " +
        std::to_string(session.join_barrier_bytes_done / 1024U) + "/" +
        std::to_string(session.join_barrier_total_bytes / 1024U) + " KB";
    const std::string detail =
        change_text.empty() ? phase_text : change_text + " - " + phase_text;
    DrawCenteredText(
        renderer,
        graphics,
        TextType::MenuItem,
        detail.c_str(),
        center_x,
        center_y + 42.0F,
        SDL_Color{225, 225, 225, 255}
    );
}

} // namespace

void Render(
    SDL_Renderer* renderer,
    SDL_Texture* render_texture,
    const RenderPostFx& post_fx,
    State& state,
    const Audio& audio,
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
        RenderGameOver(renderer, state, graphics);
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
    const SDL_FRect dst = GetPresRect(graphics, output_width, output_height);
    SDL_GPURenderState* const post_fx_state = GetActivePostFxState(post_fx, state);
    if (post_fx_state != nullptr) {
        SDL_SetGPURenderState(renderer, post_fx_state);
    }
    SDL_RenderTexture(renderer, render_texture, &src, &dst);
    if (post_fx_state != nullptr) {
        SDL_SetGPURenderState(renderer, nullptr);
    }

    if (state.mode == Mode::Playing || state.mode == Mode::GameOver) {
        RenderPlayingHud(renderer, state, graphics);
        if (state.mode == Mode::Playing) {
            RenderWorldPrompts(renderer, state, graphics);
        }
        RenderDebugOverlay(renderer, graphics, state, audio);
        RenderJoinBarrierOverlay(renderer, state, graphics);
    }
}

} // namespace splonks
