#include "state.hpp"
#include "stage_init.hpp"

namespace splonks {

State State::New() {
    State state;
    state.mode = Mode::Playing;
    state.settings = LoadSettings();
    state.menu_inputs = MenuInputs::New();
    state.menu_input_snapshot = MenuInputSnapshot::New();
    state.previous_menu_input_snapshot = MenuInputSnapshot::New();
    state.menu_input_debounce_timers = MenuInputDebounceTimers::New();
    state.playing_inputs = PlayingInputs::New();
    state.immediate_playing_inputs = PlayingInputs::New();
    state.playing_input_snapshot = PlayingInputSnapshot::New();
    state.previous_playing_input_snapshot = PlayingInputSnapshot::New();
    state.previous_immediate_playing_input_snapshot = PlayingInputSnapshot::New();
    state.title_menu_selection = TitleMenuOption::Start;
    state.settings_menu_selection = SettingsMenuOption::Video;
    state.video_settings_menu_selection = VideoSettingsMenuOption::Resolution;
    state.ui_settings_menu_selection = UiSettingsMenuOption::IconScale;
    state.video_settings_target_window_size_index.reset();
    state.video_settings_target_resolution_index.reset();
    state.video_settings_target_fullscreen.reset();
    state.choosing_control_binding = false;
    state.rebuild_render_texture = false;
    state.scene_frame = 0;
    state.time_since_last_update = 0.0F;
    state.now = 0.0;
    state.running = true;
    state.frame = 0;
    state.stage_frame = 0;
    state.menu_return_to = Mode::Title;
    state.game_over = false;
    state.pause = false;
    state.win = false;
    state.points = 0;
    state.deaths = 0;
    state.frame_pause = 0;
    state.entity_manager = EntityManager::New();
    state.special_effects.clear();
    state.sid = SID::New();
    state.next_stage = StageType::Test1;
    state.player_vid.reset();
    state.controlled_entity_vid.reset();
    state.mouse_trailer_vid.reset();
    state.contact_cooldowns.clear();
    state.entity_tool_states.clear();
    InitDebugLevel(state);
    return state;
}

void State::SetMode(Mode new_mode) {
    mode = new_mode;
    scene_frame = 0;
}

void State::RebuildSid() {
    sid = SID::New();

    for (const Entity& entity : entity_manager.entities) {
        if (entity.active) {
            sid.Insert(entity.vid, entity.pos, entity.size);
        }
    }
}

void State::StepContactCooldowns() {
    std::vector<ContactCooldownEntry> kept_cooldowns;
    kept_cooldowns.reserve(contact_cooldowns.size());
    for (const ContactCooldownEntry& entry : contact_cooldowns) {
        if (entry.expires_on_stage_frame > stage_frame) {
            kept_cooldowns.push_back(entry);
        }
    }
    contact_cooldowns = std::move(kept_cooldowns);
}

bool State::HasContactCooldown(
    const VID& source_vid,
    const VID& target_vid,
    ContactInteractionKind kind
) const {
    for (const ContactCooldownEntry& entry : contact_cooldowns) {
        if (entry.source_vid == source_vid && entry.target_vid == target_vid && entry.kind == kind) {
            return true;
        }
    }
    return false;
}

void State::AddContactCooldown(
    const VID& source_vid,
    const VID& target_vid,
    ContactInteractionKind kind,
    std::uint32_t duration
) {
    for (ContactCooldownEntry& entry : contact_cooldowns) {
        if (entry.source_vid == source_vid && entry.target_vid == target_vid && entry.kind == kind) {
            entry.expires_on_stage_frame = stage_frame + duration;
            return;
        }
    }

    contact_cooldowns.push_back(ContactCooldownEntry{
        .source_vid = source_vid,
        .target_vid = target_vid,
        .kind = kind,
        .expires_on_stage_frame = stage_frame + duration,
    });
}

void State::StepEntityToolStates() {
    for (EntityToolState& tool_state : entity_tool_states) {
        for (ToolSlot& slot : tool_state.slots) {
            if (!slot.active) {
                continue;
            }
            if (slot.cooldown > 0) {
                slot.cooldown -= 1;
            }
        }
    }
}

EntityToolState* State::FindEntityToolStateMut(const VID& owner_vid) {
    for (EntityToolState& tool_state : entity_tool_states) {
        if (tool_state.owner_vid == owner_vid) {
            return &tool_state;
        }
    }
    return nullptr;
}

const EntityToolState* State::FindEntityToolState(const VID& owner_vid) const {
    for (const EntityToolState& tool_state : entity_tool_states) {
        if (tool_state.owner_vid == owner_vid) {
            return &tool_state;
        }
    }
    return nullptr;
}

ToolSlot* State::FindToolSlotMut(const VID& owner_vid, std::size_t slot_index) {
    EntityToolState* const tool_state = FindEntityToolStateMut(owner_vid);
    if (tool_state == nullptr || slot_index >= tool_state->slots.size()) {
        return nullptr;
    }
    return &tool_state->slots[slot_index];
}

const ToolSlot* State::FindToolSlot(const VID& owner_vid, std::size_t slot_index) const {
    const EntityToolState* const tool_state = FindEntityToolState(owner_vid);
    if (tool_state == nullptr || slot_index >= tool_state->slots.size()) {
        return nullptr;
    }
    return &tool_state->slots[slot_index];
}

ToolSlot& State::EnsureToolSlot(const VID& owner_vid, std::size_t slot_index) {
    if (EntityToolState* existing = FindEntityToolStateMut(owner_vid)) {
        return existing->slots[slot_index];
    }
    EntityToolState tool_state{};
    tool_state.owner_vid = owner_vid;
    entity_tool_states.push_back(tool_state);
    return entity_tool_states.back().slots[slot_index];
}

bool IsStageWon(const State& state) {
    if (!state.player_vid.has_value()) {
        return false;
    }

    const Entity* player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr) {
        return false;
    }

    const auto [player_tl, player_br] = player->GetBounds();
    const IVec2 player_tl_tile_pos = ToIVec2(player_tl) / static_cast<int>(kTileSize);
    const IVec2 player_br_tile_pos = ToIVec2(player_br) / static_cast<int>(kTileSize);
    const std::vector<const Tile*> player_tiles =
        state.stage.GetTilesInRect(player_tl_tile_pos, player_br_tile_pos);

    for (const Tile* tile : player_tiles) {
        if (*tile == Tile::Exit) {
            return true;
        }
    }

    return false;
}

void StepSpecialEffects(State& state) {
    for (auto& effect : state.special_effects) {
        effect->Step();
    }

    std::vector<std::unique_ptr<SpecialEffect>> kept_effects;
    kept_effects.reserve(state.special_effects.size());
    for (auto& effect : state.special_effects) {
        if (!effect->IsFinished()) {
            kept_effects.push_back(std::move(effect));
        }
    }
    state.special_effects = std::move(kept_effects);
}

} // namespace splonks
