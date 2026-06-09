#include "debug/playback_internal.hpp"

#include "ent/spec.hpp"
#include "ent/spec_restore.hpp"
#include "aframe.hpp"
#include "player_queries.hpp"
#include "sim/fxp.hpp"
#include "tools/tool_spec.hpp"
#include "world_ops.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace splonks::debug_playback_internal {

namespace {

constexpr double kSpawnAtMouseDelaySeconds = 2.0;

bool IsPeerDebugWorldMutationDisabled(const State& state) {
    return state.net_session.role == network::NetRole::Peer;
}

bool SpawnSearchMatches(const char* query, const char* candidate) {
    if (query == nullptr || query[0] == '\0') {
        return true;
    }
    if (candidate == nullptr) {
        return false;
    }

    std::string normalized_query(query);
    normalized_query.erase(
        normalized_query.begin(),
        std::find_if(normalized_query.begin(), normalized_query.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        })
    );
    normalized_query.erase(
        std::find_if(normalized_query.rbegin(), normalized_query.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(),
        normalized_query.end()
    );
    if (normalized_query.empty()) {
        return true;
    }

    std::transform(
        normalized_query.begin(),
        normalized_query.end(),
        normalized_query.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
    );

    std::string normalized_candidate(candidate);
    std::transform(
        normalized_candidate.begin(),
        normalized_candidate.end(),
        normalized_candidate.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
    );
    return normalized_candidate.find(normalized_query) != std::string::npos;
}

bool HasStickyBombTool(const EntToolInventoryState& ent_tools, const VID& owner_vid) {
    const EntToolState* const tool_state = ent_tools.FindEntToolState(owner_vid);
    if (tool_state == nullptr) {
        return false;
    }
    return std::any_of(tool_state->slots.begin(), tool_state->slots.end(), [](const ToolSlot& slot) {
        return slot.active && slot.kind == ToolKind::ThrowStickyBomb;
    });
}

const char* EffectUiKindToString(EffectUiKind kind) {
    switch (kind) {
    case EffectUiKind::Hidden:
        return "hidden";
    case EffectUiKind::Passive:
        return "passive";
    case EffectUiKind::Temporary:
        return "temporary";
    }

    return "unknown";
}

void AddEffectFromDebug(Ent& ent, EffectId effect_id) {
    if (effect_id == EffectId::NoGravityUntilContact) {
        // Let the debug add survive stale contact state from the previous step.
        ent.grounded = false;
        ent.collided = false;
        ent.collided_last_frame = false;
    }
    (void)AddEffect(ent, effect_id, GetEffectSpec(effect_id).default_count);
}

void AddAllPersistentEffectsFromDebug(Ent& ent) {
    for (std::uint8_t i = 1; i < static_cast<std::uint8_t>(EffectId::Count); ++i) {
        const EffectId effect_id = static_cast<EffectId>(i);
        const EffectSpec& spec = GetEffectSpec(effect_id);
        if (spec.ui_kind != EffectUiKind::Passive) {
            continue;
        }
        AddEffectFromDebug(ent, effect_id);
    }
}

bool DrawEntEffectsEditor(Ent& ent) {
    bool changed = false;
    EntEffects* const effects = ent.effects.get();
    if (effects == nullptr || effects->count == 0) {
        ImGui::TextDisabled("No active effects.");
    } else {
        for (std::size_t effect_index = 0; effect_index < effects->count; ++effect_index) {
            EffectInstance& effect = effects->effects[effect_index];
            const EffectSpec& spec = GetEffectSpec(effect.id);
            ImGui::PushID("active_effect");
            ImGui::PushID(static_cast<int>(effect_index));
            ImGui::Separator();
            ImGui::Text("%s (%s)", spec.debug_name, EffectUiKindToString(spec.ui_kind));

            int count = effect.count;
            if (ImGui::InputInt("Count##effect_count", &count)) {
                effect.count = count;
                changed = true;
            }
            float effect_value = ToFloat(effect.value);
            if (ImGui::DragFloat(
                "Value##effect_value",
                &effect_value,
                0.05F,
                -1000.0F,
                1000.0F,
                "%.2f"
            )) {
                effect.value = ToFxScalar(effect_value);
                changed = true;
            }
            int frames_remaining = static_cast<int>(effect.frames_remaining);
            if (ImGui::InputInt("Frames##effect_frames", &frames_remaining)) {
                effect.frames_remaining = static_cast<std::uint32_t>(std::max(0, frames_remaining));
                changed = true;
            }
            if (ImGui::Button("Remove##effect_remove")) {
                RemoveEffect(ent, effect.id);
                changed = true;
                ImGui::PopID();
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
            ImGui::PopID();
        }
    }

    static EffectId selected_effect = EffectId::Gloves;
    if (selected_effect == EffectId::None || selected_effect == EffectId::Count) {
        selected_effect = EffectId::Gloves;
    }

    ImGui::Separator();
    if (ImGui::BeginCombo("Add Effect##ent_effect_add_combo", EffectIdToString(selected_effect))) {
        for (std::uint8_t i = 1; i < static_cast<std::uint8_t>(EffectId::Count); ++i) {
            const EffectId effect_id = static_cast<EffectId>(i);
            const bool selected = effect_id == selected_effect;
            char label[96];
            std::snprintf(label, sizeof(label), "%s##ent_effect_add_%u", EffectIdToString(effect_id), i);
            if (ImGui::Selectable(label, selected)) {
                selected_effect = effect_id;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Add Selected Effect##ent_effect_add_button")) {
        AddEffectFromDebug(ent, selected_effect);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add All Persistent##ent_effect_add_all_persistent")) {
        AddAllPersistentEffectsFromDebug(ent);
        changed = true;
    }
    if (selected_effect == EffectId::NoGravityUntilContact) {
        ImGui::TextDisabled("Expires on grounded or blocking contact.");
    }
    return changed;
}

std::vector<EntType> BuildSortedSpawnTypes() {
    std::vector<EntType> types;
    types.reserve(kEntTypeCount > 0 ? kEntTypeCount - 1 : 0);
    for (std::size_t type_index = 1; type_index < kEntTypeCount; ++type_index) {
        types.push_back(static_cast<EntType>(type_index));
    }

    std::sort(types.begin(), types.end(), [](EntType left, EntType right) {
        return std::strcmp(GetEntTypeName(left), GetEntTypeName(right)) < 0;
    });
    return types;
}


constexpr std::uint16_t kDebugPlayerInitialBombs = 400;
constexpr std::uint16_t kDebugPlayerInitialRopes = 400;

Ent* FindSwapSourceEnt(State& state, Ent* selected_ent) {
    if (state.controlled_ent_vid.has_value()) {
        if (Ent* const controlled = state.ents.GetEntMut(*state.controlled_ent_vid)) {
            return controlled;
        }
    }
    if (Ent* const player = GetPrimaryLocalPlayerMut(state)) {
        return player;
    }
    return selected_ent;
}

const Ent* FindSwapStatsEnt(const State& state, const Ent* source_ent) {
    if (const Ent* const player = GetPrimaryLocalPlayer(state)) {
        return player;
    }
    return source_ent;
}

std::optional<EntToolState> CopyToolStateForVid(const State& state, const VID& owner_vid) {
    if (const EntToolState* const tool_state = state.ent_tools.FindEntToolState(owner_vid)) {
        return *tool_state;
    }
    return std::nullopt;
}

void RemoveToolStateForVid(State& state, const VID& owner_vid) {
    auto& tool_states = state.ent_tools.tool_states;
    tool_states.erase(
        std::remove_if(
            tool_states.begin(),
            tool_states.end(),
            [&owner_vid](const EntToolState& tool_state) {
                return tool_state.owner_vid == owner_vid;
            }
        ),
        tool_states.end()
    );
}

void DetachEntsAttachedToVid(State& state, const VID& owner_vid, const Graphics& graphics) {
    if (Ent* const holder = state.ents.GetEntMut(owner_vid)) {
        holder->holding_vid.reset();
        holder->holding = false;
        holder->holding_timer = kDefaultHoldingTimer;
        holder->back_vid.reset();
    }

    for (Ent& attached : state.ents.ents) {
        if (!attached.active || !attached.held_by_vid.has_value() || *attached.held_by_vid != owner_vid) {
            continue;
        }

        attached.held_by_vid.reset();
        attached.attach_mode = AttachMode::None;
        StopUsingEnt(attached);
        RestoreEntHasPhysicsFromSpec(attached);
        RestoreEntCanCollideFromSpec(attached);
        RestoreEntDrawLayerFromSpec(attached);
        attached.grounded = false;
        state.UpdateSidForEnt(attached.vid.id, graphics);
    }
}

void GrantFreshStarterTools(State& state, const VID& owner_vid, EntType type_) {
    RemoveToolStateForVid(state, owner_vid);
    if (type_ != EntType::Player) {
        return;
    }

    if (const std::optional<ToolKind> bomb_tool_kind = FindPreferredToolKindForSlotIndex(0)) {
        FillToolSlot(
            state.ent_tools.EnsureToolSlot(owner_vid, 0),
            *bomb_tool_kind,
            kDebugPlayerInitialBombs,
            true
        );
    }
    if (const std::optional<ToolKind> rope_tool_kind = FindPreferredToolKindForSlotIndex(1)) {
        FillToolSlot(
            state.ent_tools.EnsureToolSlot(owner_vid, 1),
            *rope_tool_kind,
            kDebugPlayerInitialRopes,
            true
        );
    }
}

bool SwapControlledCharacter(
    DebugPlayback& debug,
    State& state,
    const Graphics& graphics,
    Ent* selected_ent
) {
    const EntType target_type = debug.character_swap_ent_type;
    if (target_type == EntType::None) {
        debug.character_swap_status = "Select a character type first.";
        return false;
    }

    Ent* const source_ent = FindSwapSourceEnt(state, selected_ent);
    if (source_ent == nullptr) {
        debug.character_swap_status = "No controlled, player, or selected ent to swap.";
        return false;
    }

    const Ent* const stats_ent = FindSwapStatsEnt(state, source_ent);
    const bool keep_passives = !debug.character_swap_fresh || debug.character_swap_keep_passives;
    const bool keep_money = !debug.character_swap_fresh || debug.character_swap_keep_money;
    const bool keep_health = !debug.character_swap_fresh || debug.character_swap_keep_health;
    const bool keep_tools = !debug.character_swap_fresh || debug.character_swap_keep_tools;

    const FVec2 spawn_center = source_ent->GetRenderCenter();
    const Side facing = source_ent->facing;
    const VID replacement_vid = source_ent->vid;
    const std::optional<VID> old_player_vid = FindPrimaryLocalPlayerVid(state);
    const EntEffects* const effects =
        stats_ent != nullptr ? stats_ent->effects.get() : nullptr;
    const std::uint32_t money = stats_ent != nullptr ? stats_ent->money : 0;
    const std::uint32_t health = stats_ent != nullptr ? stats_ent->health : 0;
    const std::optional<EntToolState> preserved_tools =
        stats_ent != nullptr ? CopyToolStateForVid(state, stats_ent->vid) : std::nullopt;

    DetachEntsAttachedToVid(state, replacement_vid, graphics);
    if (old_player_vid.has_value() && *old_player_vid != replacement_vid) {
        DetachEntsAttachedToVid(state, *old_player_vid, graphics);
        state.ents.SetInactiveVid(*old_player_vid);
        RemoveToolStateForVid(state, *old_player_vid);
        state.UpdateSidForEnt(old_player_vid->id, graphics);
    }

    SetEntAs(*source_ent, target_type);
    source_ent->vel = sim::FxVec2::zero();
    source_ent->acc = sim::FxVec2::zero();
    source_ent->rotation = sim::Scalar::zero();
    source_ent->facing = facing;
    source_ent->SetRenderCenter(spawn_center);

    if (keep_passives) {
        source_ent->effects.reset();
        if (effects != nullptr) {
            source_ent->effects.emplace() = *effects;
        }
    }
    if (keep_money) {
        source_ent->money = money;
    }
    if (keep_health) {
        source_ent->health = health;
    }

    RemoveToolStateForVid(state, replacement_vid);
    if (keep_tools && preserved_tools.has_value()) {
        EntToolState copied_tools = *preserved_tools;
        copied_tools.owner_vid = replacement_vid;
        state.ent_tools.tool_states.push_back(copied_tools);
    } else {
        GrantFreshStarterTools(state, replacement_vid, target_type);
    }

    if (PlayerSlot* const slot = state.players.FindPrimaryLocal()) {
        slot->ent_vid = replacement_vid;
    }
    state.controlled_ent_vid = replacement_vid;
    state.UpdateSidForEnt(replacement_vid.id, graphics);
    debug.selected_ent_id = replacement_vid.id;
    debug.character_swap_status = std::string("Swapped to ") + GetEntTypeName(target_type) + ".";
    if (source_ent->control_logic == nullptr) {
        debug.character_swap_status += " Warning: no control callback.";
    }
    return true;
}

void DrawCharacterSwapControls(
    DebugPlayback& debug,
    State& state,
    const Graphics& graphics,
    Ent* selected_ent
) {
    ImGui::SeparatorText("Character Swap");
    if (IsPeerDebugWorldMutationDisabled(state)) {
        ImGui::TextDisabled("Disabled on multiplayer peers until debug/admin commands are host-routed.");
        return;
    }
    if (debug.playback_active) {
        ImGui::TextDisabled("Character swapping disabled during playback.");
        return;
    }

    ImGui::TextDisabled("Source: controlled ent, then player, then selected ent.");
    ImGui::InputText(
        "Search##character_swap_search",
        debug.character_swap_search.data(),
        debug.character_swap_search.size()
    );
    ImGui::SameLine();
    if (ImGui::Button("Clear##character_swap_search_clear")) {
        debug.character_swap_search[0] = '\0';
    }

    const char* current_swap_name = GetEntTypeName(debug.character_swap_ent_type);
    if (ImGui::BeginCombo("Swap Type", current_swap_name)) {
        const std::vector<EntType> sorted_spawn_types = BuildSortedSpawnTypes();
        for (const EntType type_ : sorted_spawn_types) {
            const char* type_name = GetEntTypeName(type_);
            if (!SpawnSearchMatches(debug.character_swap_search.data(), type_name)) {
                continue;
            }
            const bool selected = debug.character_swap_ent_type == type_;
            if (ImGui::Selectable(type_name, selected)) {
                debug.character_swap_ent_type = type_;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Checkbox("Fresh Spawn", &debug.character_swap_fresh);
    ImGui::SameLine();
    ImGui::TextDisabled("fresh uses spec defaults; keep flags copy selected state after reset");
    ImGui::Checkbox("Keep Passives", &debug.character_swap_keep_passives);
    ImGui::SameLine();
    ImGui::Checkbox("Keep Money", &debug.character_swap_keep_money);
    ImGui::Checkbox("Keep Health", &debug.character_swap_keep_health);
    ImGui::SameLine();
    ImGui::Checkbox("Keep Tools", &debug.character_swap_keep_tools);

    if (ImGui::Button("Swap Controlled Character")) {
        SwapControlledCharacter(debug, state, graphics, selected_ent);
    }

    ImGui::SeparatorText("Default Spawn");
    ImGui::Checkbox("Spawn As Default Type", &debug.default_spawn_enabled);
    ImGui::SameLine();
    if (ImGui::Button("Use Swap Type##default_spawn_use_swap_type")) {
        debug.default_spawn_ent_type = debug.character_swap_ent_type;
    }

    const char* current_default_spawn_name = GetEntTypeName(debug.default_spawn_ent_type);
    if (ImGui::BeginCombo("Default Spawn Type", current_default_spawn_name)) {
        const std::vector<EntType> sorted_spawn_types = BuildSortedSpawnTypes();
        for (const EntType type_ : sorted_spawn_types) {
            const char* type_name = GetEntTypeName(type_);
            if (!SpawnSearchMatches(debug.character_swap_search.data(), type_name)) {
                continue;
            }
            const bool selected = debug.default_spawn_ent_type == type_;
            if (ImGui::Selectable(type_name, selected)) {
                debug.default_spawn_ent_type = type_;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::TextDisabled("Applied when a stage calls the normal player spawn path.");

    if (!debug.character_swap_status.empty()) {
        ImGui::TextWrapped("%s", debug.character_swap_status.c_str());
    }
}

bool SpawnDebugEnt(
    DebugPlayback& debug,
    State& state,
    const Graphics& graphics,
    EntType type_,
    const Ent* selected_ent
) {
    if (IsPeerDebugWorldMutationDisabled(state)) {
        debug.spawn_status =
            "Debug spawning is disabled on multiplayer peers until admin commands are host-routed.";
        return false;
    }
    if (type_ == EntType::None) {
        debug.spawn_status = "Select an ent type first.";
        return false;
    }

    if (debug.spawn_center_on_selected && selected_ent == nullptr) {
        debug.spawn_status = "No active selected ent to center spawn on.";
        return false;
    }

    std::optional<VID> holding_player_vid;
    if (debug.spawn_held_by_player) {
        Ent* const player = GetPrimaryLocalPlayerMut(state);
        if (player == nullptr) {
            debug.spawn_status = "No player to hold spawned ent.";
            return false;
        }
        if (player->holding_vid.has_value()) {
            debug.spawn_status = "Player is already holding something.";
            return false;
        }
        holding_player_vid = player->vid;
    }

    FVec2 spawn_center = graphics.ScreenToWc(state.playing_inputs.mouse_pos);
    if (debug.spawn_center_on_selected && selected_ent != nullptr) {
        spawn_center = selected_ent->GetRenderCenter();
    }

    Ent* const spawned = world_ops::SpawnEnt(state, type_, [spawn_center](Ent& ent) {
        ent.vel = sim::FxVec2::zero();
        ent.acc = sim::FxVec2::zero();
        ent.SetRenderCenter(spawn_center);
    });
    if (spawned == nullptr) {
        debug.spawn_status = "Spawn failed.";
        return false;
    }

    if (debug.spawn_held_by_player) {
        if (Ent* const player = state.ents.GetEntMut(*holding_player_vid)) {
            player->holding_vid = spawned->vid;
            player->holding = true;
            player->holding_timer = kDefaultHoldingTimer;
            spawned->held_by_vid = player->vid;
            spawned->attach_mode = AttachMode::Held;
            spawned->has_physics = false;
            spawned->can_collide = false;
            spawned->facing = player->facing;
            spawned->SetRenderCenter(player->GetRenderCenter());
            debug.spawn_status =
                std::string("Spawned and attached ") + GetEntTypeName(type_) + ".";
        }
    } else {
        debug.spawn_status = std::string("Spawned ") + GetEntTypeName(type_) + ".";
    }

    state.UpdateSidForEnt(spawned->vid.id, graphics);
    debug.selected_ent_id = spawned->vid.id;
    return true;
}

} // namespace

void DrawEntInspector(DebugPlayback& debug, State& state, const Graphics& graphics) {
    if (!debug.ent_inspector_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(12.0F, 300.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Ents", &debug.ent_inspector_visible)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginListBox("Ents", ImVec2(260.0F, 220.0F))) {
        for (std::size_t i = 0; i < state.ents.ents.size(); ++i) {
            const Ent& ent = state.ents.ents[i];
            if (!ent.active) {
                continue;
            }

            char label[128];
            std::snprintf(
                label,
                sizeof(label),
                "%zu: %s##ent_%zu",
                i,
                EntTypeToString(ent.type_),
                i
            );
            const bool selected = debug.selected_ent_id == i;
            if (ImGui::Selectable(label, selected)) {
                debug.selected_ent_id = i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }

    if (debug.selected_ent_id >= state.ents.ents.size()) {
        debug.selected_ent_id = 0;
    }

    Ent* selected_ent = nullptr;
    if (!state.ents.ents.empty()) {
        Ent& ent = state.ents.ents[debug.selected_ent_id];
        if (ent.active) {
            selected_ent = &ent;
        }
    }

    if (IsPeerDebugWorldMutationDisabled(state)) {
        debug.pending_spawn_at_mouse = false;
        ImGui::SeparatorText("Spawner");
        ImGui::TextDisabled("Ent spawning disabled on multiplayer peers.");
    } else if (debug.playback_active) {
        debug.pending_spawn_at_mouse = false;
        ImGui::SeparatorText("Spawner");
        ImGui::TextDisabled("Ent spawning disabled during playback.");
    } else {
        ImGui::SeparatorText("Spawner");
        ImGui::InputText("Search", debug.spawn_search.data(), debug.spawn_search.size());
        ImGui::SameLine();
        if (ImGui::Button("Clear Search")) {
            debug.spawn_search[0] = '\0';
        }

        const char* current_spawn_name = GetEntTypeName(debug.spawn_ent_type);
        if (ImGui::BeginCombo("Spawn Type", current_spawn_name)) {
            const std::vector<EntType> sorted_spawn_types = BuildSortedSpawnTypes();
            for (const EntType type_ : sorted_spawn_types) {
                const char* type_name = GetEntTypeName(type_);
                if (!SpawnSearchMatches(debug.spawn_search.data(), type_name)) {
                    continue;
                }
                const bool selected = debug.spawn_ent_type == type_;
                if (ImGui::Selectable(type_name, selected)) {
                    debug.spawn_ent_type = type_;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::RadioButton("Spawn At Mouse", !debug.spawn_center_on_selected)) {
            debug.spawn_center_on_selected = false;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Center On Selected", debug.spawn_center_on_selected)) {
            debug.spawn_center_on_selected = true;
            debug.pending_spawn_at_mouse = false;
        }
        ImGui::Checkbox("Spawn Held By Player", &debug.spawn_held_by_player);

        const FVec2 mouse_world = graphics.ScreenToWc(state.playing_inputs.mouse_pos);
        ImGui::Text("Mouse WC: (%.2f, %.2f)", mouse_world.x, mouse_world.y);

        if (debug.pending_spawn_at_mouse && debug.spawn_center_on_selected) {
            debug.pending_spawn_at_mouse = false;
        }

        if (debug.pending_spawn_at_mouse) {
            const double remaining_seconds =
                std::max(0.0, debug.pending_spawn_at_mouse_until - state.now);
            ImGui::Text("Pending mouse spawn in %.1fs", remaining_seconds);
            ImGui::SameLine();
            if (ImGui::Button("Cancel Pending Spawn")) {
                debug.pending_spawn_at_mouse = false;
                debug.spawn_status = "Cancelled pending mouse spawn.";
            }
            if (debug.pending_spawn_at_mouse && state.now >= debug.pending_spawn_at_mouse_until) {
                debug.pending_spawn_at_mouse = false;
                SpawnDebugEnt(debug, state, graphics, debug.spawn_ent_type, selected_ent);
            }
        } else {
            const char* spawn_button_label =
                debug.spawn_center_on_selected ? "Spawn Ent" : "Arm Mouse Spawn";
            if (ImGui::Button(spawn_button_label)) {
                if (debug.spawn_center_on_selected) {
                    SpawnDebugEnt(debug, state, graphics, debug.spawn_ent_type, selected_ent);
                } else {
                    debug.pending_spawn_at_mouse = true;
                    debug.pending_spawn_at_mouse_until = state.now + kSpawnAtMouseDelaySeconds;
                    debug.spawn_status = "Mouse spawn armed for 2.0 seconds.";
                }
            }
        }

        if (!debug.spawn_status.empty()) {
            ImGui::TextWrapped("%s", debug.spawn_status.c_str());
        }
    }

    DrawCharacterSwapControls(debug, state, graphics, selected_ent);

    if (selected_ent == nullptr) {
        ImGui::TextUnformatted("No active ent selected.");
        ImGui::End();
        SyncDebugUiSettings(debug, state);
        return;
    }

    Ent& ent = *selected_ent;
    const FAABB aabb = ent.GetRenderAABB();
    ImGui::Separator();
    ImGui::Text("Type: %s", EntTypeToString(ent.type_));
    ImGui::Text(
        "Controlled: %s",
        state.controlled_ent_vid.has_value() && ent.vid == *state.controlled_ent_vid
            ? "true"
            : "false"
    );
    if (ImGui::Button("Control Selected")) {
        state.controlled_ent_vid = ent.vid;
    }
    if (const std::optional<VID> player_vid = FindPrimaryLocalPlayerVid(state)) {
        ImGui::SameLine();
        if (ImGui::Button("Control Player")) {
            state.controlled_ent_vid = *player_vid;
        }
    }
    ImGui::Text("Anim Id: %u", ent.aframe_animator.anim_id);
    ImGui::Text("Condition: %s", ConditionToString(ent.condition));
    ImGui::Text("AI: %s", AiStateToString(ent.ai_state));
    const bool peer_mutation_disabled = IsPeerDebugWorldMutationDisabled(state);
    bool ent_state_changed = false;
    bool player_state_changed = false;
    if (peer_mutation_disabled) {
        ImGui::BeginDisabled();
    }
    bool stone = ent.stone;
    if (ImGui::Checkbox("Stone", &stone)) {
        if (stone) {
            EnableStone(ent);
        } else {
            DisableStone(ent);
        }
        ent_state_changed = true;
    }
    if (ImGui::Checkbox("Wanted", &ent.wanted)) {
        ent_state_changed = true;
        player_state_changed = true;
    }
    ent_state_changed |= ImGui::Checkbox("Crusher/Pusher", &ent.crusher_pusher);
    ent_state_changed |= ImGui::Checkbox("Pushable", &ent.pushable);
    float push_acc = ToFloat(ent.push_acc);
    if (ImGui::DragFloat("Push Acc", &push_acc, 0.01F, 0.0F, 5.0F, "%.2f")) {
        ent.push_acc = ToFxScalar(push_acc);
        ent_state_changed = true;
    }
    if (peer_mutation_disabled) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("Ent edits are disabled on multiplayer peers until admin commands are host-routed.");
    }
    ImGui::Text("Facing: %s", SideToString(ent.facing));
    ImGui::Text("Grounded: %s", ent.grounded ? "true" : "false");
    const FVec2 ent_pos = ent.GetRenderPos();
    const FVec2 ent_vel = ent.GetRenderVel();
    const FVec2 ent_acc = ent.GetRenderAcc();
    ImGui::Text("Pos: (%.2f, %.2f)", ent_pos.x, ent_pos.y);
    ImGui::Text("Vel: (%.2f, %.2f)", ent_vel.x, ent_vel.y);
    ImGui::Text("Acc: (%.2f, %.2f)", ent_acc.x, ent_acc.y);
    const FVec2 ent_size = ent.GetSize();
    ImGui::Text("Size: (%.2f, %.2f)", ent_size.x, ent_size.y);
    ImGui::Text("FAABB TL: (%.2f, %.2f)", aabb.tl.x, aabb.tl.y);
    ImGui::Text("FAABB BR: (%.2f, %.2f)", aabb.br.x, aabb.br.y);
    ImGui::Text("Coyote: %u", ent.coyote_time);
    ImGui::Text("Fall timer: %u", ent.fall_timer);
    ImGui::Text("Health: %u", ent.health);
    ImGui::Text("Money: %u", ent.money);
    if (!peer_mutation_disabled) {
        int money = static_cast<int>(ent.money);
        ImGui::SetNextItemWidth(120.0F);
        if (ImGui::InputInt("Edit Money", &money)) {
            ent.money = static_cast<std::uint32_t>(std::max(0, money));
            ent_state_changed = true;
            player_state_changed = true;
        }
    }
    ImGui::SeparatorText("Effects");
    if (peer_mutation_disabled) {
        ImGui::BeginDisabled();
    }
    if (DrawEntEffectsEditor(ent)) {
        ent_state_changed = true;
        player_state_changed = true;
    }
    if (peer_mutation_disabled) {
        ImGui::EndDisabled();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Tools");
    if (peer_mutation_disabled) {
        ImGui::TextDisabled("Tool editing disabled on multiplayer peers.");
    } else if (debug.playback_active) {
        ImGui::TextDisabled("Tool editing disabled during playback.");
    } else {
        const bool has_sticky_bombs = HasStickyBombTool(state.ent_tools, ent.vid);
        ImGui::Text("Sticky bombs: %s", has_sticky_bombs ? "true" : "false");
        ImGui::SameLine();
        if (ImGui::Button("Upgrade Bombs To Sticky")) {
            state.ent_tools.UpgradeBombsToSticky(ent.vid);
            player_state_changed = true;
        }
        for (std::size_t slot_index = 0; slot_index < kToolSlotCount; ++slot_index) {
            ToolSlot preview_slot{};
            if (const std::optional<ToolKind> preferred_tool_kind =
                    FindPreferredToolKindForSlotIndex(slot_index)) {
                FillToolSlot(preview_slot, *preferred_tool_kind, 0, false);
            }
            ToolSlot* slot = state.ent_tools.FindToolSlotMut(ent.vid, slot_index);
            if (slot == nullptr) {
                slot = &preview_slot;
            }
            ImGui::PushID(static_cast<int>(slot_index));
            ImGui::SeparatorText(slot_index == 0 ? "Tool Slot 1" : "Tool Slot 2");
            bool active = slot->active;
            if (ImGui::Checkbox("Active", &active)) {
                ToolSlot& owned_slot = state.ent_tools.EnsureToolSlot(ent.vid, slot_index);
                owned_slot = *slot;
                owned_slot.active = active;
                slot = &owned_slot;
                player_state_changed = true;
            }

            int kind_index = static_cast<int>(slot->kind);
            const char* current_kind_name = GetToolKindName(static_cast<ToolKind>(kind_index));
            if (ImGui::BeginCombo("Kind", current_kind_name)) {
                for (std::size_t tool_index = 0; tool_index < kToolKindCount; ++tool_index) {
                    const ToolKind tool_kind = static_cast<ToolKind>(tool_index);
                    const bool selected = static_cast<int>(tool_index) == kind_index;
                    if (ImGui::Selectable(GetToolKindName(tool_kind), selected)) {
                        ToolSlot& owned_slot = state.ent_tools.EnsureToolSlot(ent.vid, slot_index);
                        owned_slot = *slot;
                        owned_slot.kind = tool_kind;
                        slot = &owned_slot;
                        kind_index = static_cast<int>(tool_index);
                        player_state_changed = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            int count = static_cast<int>(slot->count);
            int cooldown = static_cast<int>(slot->cooldown);
            ImGui::SetNextItemWidth(120.0F);
            if (ImGui::InputInt("Count", &count)) {
                ToolSlot& owned_slot = state.ent_tools.EnsureToolSlot(ent.vid, slot_index);
                owned_slot = *slot;
                owned_slot.count = static_cast<std::uint16_t>(std::clamp(count, 0, 65535));
                slot = &owned_slot;
                player_state_changed = true;
            }
            ImGui::SetNextItemWidth(120.0F);
            if (ImGui::InputInt("Cooldown", &cooldown)) {
                ToolSlot& owned_slot = state.ent_tools.EnsureToolSlot(ent.vid, slot_index);
                owned_slot = *slot;
                owned_slot.cooldown = static_cast<std::uint16_t>(std::clamp(cooldown, 0, 65535));
                player_state_changed = true;
            }
            ImGui::PopID();
        }
    }
    (void)ent_state_changed;
    (void)player_state_changed;
    ImGui::Text("Climbing: %s", ent.IsClimbing() ? "true" : "false");
    ImGui::Text("Holding: %s", ent.holding ? "true" : "false");

    if (ent.aframe_animator.HasAnim()) {
        const AFrameAnim* anim =
            graphics.aframe_db.FindAnim(ent.aframe_animator.anim_id);
        if (anim != nullptr) {
            ImGui::Text("Anim: %s", anim->name.c_str());
            ImGui::Text(
                "Anim Frame: %u / %zu",
                ent.aframe_animator.current_frame,
                anim->frame_indices.empty() ? 0 : anim->frame_indices.size() - 1
            );
            const AFrame* aframe = graphics.aframe_db.FindFrame(
                ent.aframe_animator.anim_id,
                static_cast<std::size_t>(ent.aframe_animator.current_frame)
            );
            if (aframe != nullptr) {
                ImGui::Text("Frame Duration: %d", aframe->duration);
                ImGui::Text(
                    "Sample: (%d, %d, %d, %d)",
                    aframe->sample_rect.x,
                    aframe->sample_rect.y,
                    aframe->sample_rect.w,
                    aframe->sample_rect.h
                );
                ImGui::Text(
                    "Draw Offset: (%d, %d)",
                    aframe->draw_offset.x,
                    aframe->draw_offset.y
                );
                ImGui::Text(
                    "PBox: (%d, %d, %d, %d)",
                    aframe->pbox.x,
                    aframe->pbox.y,
                    aframe->pbox.w,
                    aframe->pbox.h
                );
                ImGui::Text(
                    "CBox: (%d, %d, %d, %d)",
                    aframe->cbox.x,
                    aframe->cbox.y,
                    aframe->cbox.w,
                    aframe->cbox.h
                );
            }
        }
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

} // namespace splonks::debug_playback_internal
