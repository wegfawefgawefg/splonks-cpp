#include "debug/playback_internal.hpp"

#include "entity/archetype.hpp"
#include "entity/archetype_restore.hpp"
#include "frame_data.hpp"
#include "player_queries.hpp"
#include "tools/tool_archetype.hpp"
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

bool HasStickyBombTool(const EntityToolInventoryState& entity_tools, const VID& owner_vid) {
    const EntityToolState* const tool_state = entity_tools.FindEntityToolState(owner_vid);
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

void AddEffectFromDebug(Entity& entity, EffectId effect_id) {
    if (effect_id == EffectId::NoGravityUntilContact) {
        // Let the debug add survive stale contact state from the previous step.
        entity.grounded = false;
        entity.collided = false;
        entity.collided_last_frame = false;
    }
    (void)AddEffect(entity, effect_id, GetEffectArchetype(effect_id).default_count);
}

void AddAllPersistentEffectsFromDebug(Entity& entity) {
    for (std::uint8_t i = 1; i < static_cast<std::uint8_t>(EffectId::Count); ++i) {
        const EffectId effect_id = static_cast<EffectId>(i);
        const EffectArchetype& archetype = GetEffectArchetype(effect_id);
        if (archetype.ui_kind != EffectUiKind::Passive) {
            continue;
        }
        AddEffectFromDebug(entity, effect_id);
    }
}

bool IsPlayerEntity(const State& state, const Entity& entity) {
    return state.players.FindByEntityVid(entity.vid) != nullptr;
}

void ReplicateDebugEntityEdit(
    State& state,
    const Entity& entity,
    bool entity_state_changed,
    bool player_state_changed
) {
    if (state.net_session.role == network::NetRole::Peer) {
        return;
    }
    if (entity_state_changed) {
        world_ops::PatchEntityState(state, entity, entity);
    }
    if ((entity_state_changed || player_state_changed) && IsPlayerEntity(state, entity)) {
        world_ops::PatchPlayerState(state, entity);
    }
}

bool DrawEntityEffectsEditor(Entity& entity) {
    bool changed = false;
    EntityEffects* const effects = entity.effects.get();
    if (effects == nullptr || effects->count == 0) {
        ImGui::TextDisabled("No active effects.");
    } else {
        for (std::size_t effect_index = 0; effect_index < effects->count; ++effect_index) {
            EffectInstance& effect = effects->effects[effect_index];
            const EffectArchetype& archetype = GetEffectArchetype(effect.id);
            ImGui::PushID("active_effect");
            ImGui::PushID(static_cast<int>(effect_index));
            ImGui::Separator();
            ImGui::Text("%s (%s)", archetype.debug_name, EffectUiKindToString(archetype.ui_kind));

            int count = effect.count;
            if (ImGui::InputInt("Count##effect_count", &count)) {
                effect.count = count;
                changed = true;
            }
            changed |= ImGui::DragFloat(
                "Value##effect_value",
                &effect.value,
                0.05F,
                -1000.0F,
                1000.0F,
                "%.2f"
            );
            int frames_remaining = static_cast<int>(effect.frames_remaining);
            if (ImGui::InputInt("Frames##effect_frames", &frames_remaining)) {
                effect.frames_remaining = static_cast<std::uint32_t>(std::max(0, frames_remaining));
                changed = true;
            }
            if (ImGui::Button("Remove##effect_remove")) {
                RemoveEffect(entity, effect.id);
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
    if (ImGui::BeginCombo("Add Effect##entity_effect_add_combo", EffectIdToString(selected_effect))) {
        for (std::uint8_t i = 1; i < static_cast<std::uint8_t>(EffectId::Count); ++i) {
            const EffectId effect_id = static_cast<EffectId>(i);
            const bool selected = effect_id == selected_effect;
            char label[96];
            std::snprintf(label, sizeof(label), "%s##entity_effect_add_%u", EffectIdToString(effect_id), i);
            if (ImGui::Selectable(label, selected)) {
                selected_effect = effect_id;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Add Selected Effect##entity_effect_add_button")) {
        AddEffectFromDebug(entity, selected_effect);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add All Persistent##entity_effect_add_all_persistent")) {
        AddAllPersistentEffectsFromDebug(entity);
        changed = true;
    }
    if (selected_effect == EffectId::NoGravityUntilContact) {
        ImGui::TextDisabled("Expires on grounded or blocking contact.");
    }
    return changed;
}

std::vector<EntityType> BuildSortedSpawnTypes() {
    std::vector<EntityType> types;
    types.reserve(kEntityTypeCount > 0 ? kEntityTypeCount - 1 : 0);
    for (std::size_t type_index = 1; type_index < kEntityTypeCount; ++type_index) {
        types.push_back(static_cast<EntityType>(type_index));
    }

    std::sort(types.begin(), types.end(), [](EntityType left, EntityType right) {
        return std::strcmp(GetEntityTypeName(left), GetEntityTypeName(right)) < 0;
    });
    return types;
}


constexpr std::uint16_t kDebugPlayerInitialBombs = 400;
constexpr std::uint16_t kDebugPlayerInitialRopes = 400;

Entity* FindSwapSourceEntity(State& state, Entity* selected_entity) {
    if (state.controlled_entity_vid.has_value()) {
        if (Entity* const controlled = state.entity_manager.GetEntityMut(*state.controlled_entity_vid)) {
            return controlled;
        }
    }
    if (Entity* const player = GetPrimaryLocalPlayerMut(state)) {
        return player;
    }
    return selected_entity;
}

const Entity* FindSwapStatsEntity(const State& state, const Entity* source_entity) {
    if (const Entity* const player = GetPrimaryLocalPlayer(state)) {
        return player;
    }
    return source_entity;
}

std::optional<EntityToolState> CopyToolStateForVid(const State& state, const VID& owner_vid) {
    if (const EntityToolState* const tool_state = state.entity_tools.FindEntityToolState(owner_vid)) {
        return *tool_state;
    }
    return std::nullopt;
}

void RemoveToolStateForVid(State& state, const VID& owner_vid) {
    auto& tool_states = state.entity_tools.tool_states;
    tool_states.erase(
        std::remove_if(
            tool_states.begin(),
            tool_states.end(),
            [&owner_vid](const EntityToolState& tool_state) {
                return tool_state.owner_vid == owner_vid;
            }
        ),
        tool_states.end()
    );
}

void DetachEntitiesAttachedToVid(State& state, const VID& owner_vid, const Graphics& graphics) {
    if (Entity* const holder = state.entity_manager.GetEntityMut(owner_vid)) {
        holder->holding_vid.reset();
        holder->holding = false;
        holder->holding_timer = kDefaultHoldingTimer;
        holder->back_vid.reset();
    }

    for (Entity& attached : state.entity_manager.entities) {
        if (!attached.active || !attached.held_by_vid.has_value() || *attached.held_by_vid != owner_vid) {
            continue;
        }

        attached.held_by_vid.reset();
        attached.attachment_mode = AttachmentMode::None;
        StopUsingEntity(attached);
        RestoreEntityHasPhysicsFromArchetype(attached);
        RestoreEntityCanCollideFromArchetype(attached);
        RestoreEntityDrawLayerFromArchetype(attached);
        attached.grounded = false;
        state.UpdateSidForEntity(attached.vid.id, graphics);
    }
}

void GrantFreshStarterTools(State& state, const VID& owner_vid, EntityType type_) {
    RemoveToolStateForVid(state, owner_vid);
    if (type_ != EntityType::Player) {
        return;
    }

    if (const std::optional<ToolKind> bomb_tool_kind = FindPreferredToolKindForSlotIndex(0)) {
        FillToolSlot(
            state.entity_tools.EnsureToolSlot(owner_vid, 0),
            *bomb_tool_kind,
            kDebugPlayerInitialBombs,
            true
        );
    }
    if (const std::optional<ToolKind> rope_tool_kind = FindPreferredToolKindForSlotIndex(1)) {
        FillToolSlot(
            state.entity_tools.EnsureToolSlot(owner_vid, 1),
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
    Entity* selected_entity
) {
    const EntityType target_type = debug.character_swap_entity_type;
    if (target_type == EntityType::None) {
        debug.character_swap_status = "Select a character type first.";
        return false;
    }

    Entity* const source_entity = FindSwapSourceEntity(state, selected_entity);
    if (source_entity == nullptr) {
        debug.character_swap_status = "No controlled, player, or selected entity to swap.";
        return false;
    }

    const Entity* const stats_entity = FindSwapStatsEntity(state, source_entity);
    const bool keep_passives = !debug.character_swap_fresh || debug.character_swap_keep_passives;
    const bool keep_money = !debug.character_swap_fresh || debug.character_swap_keep_money;
    const bool keep_health = !debug.character_swap_fresh || debug.character_swap_keep_health;
    const bool keep_tools = !debug.character_swap_fresh || debug.character_swap_keep_tools;

    const Vec2 spawn_center = source_entity->GetCenter();
    const LeftOrRight facing = source_entity->facing;
    const VID replacement_vid = source_entity->vid;
    const std::optional<VID> old_player_vid = FindPrimaryLocalPlayerVid(state);
    const EntityEffects* const effects =
        stats_entity != nullptr ? stats_entity->effects.get() : nullptr;
    const std::uint32_t money = stats_entity != nullptr ? stats_entity->money : 0;
    const std::uint32_t health = stats_entity != nullptr ? stats_entity->health : 0;
    const std::optional<EntityToolState> preserved_tools =
        stats_entity != nullptr ? CopyToolStateForVid(state, stats_entity->vid) : std::nullopt;

    DetachEntitiesAttachedToVid(state, replacement_vid, graphics);
    if (old_player_vid.has_value() && *old_player_vid != replacement_vid) {
        DetachEntitiesAttachedToVid(state, *old_player_vid, graphics);
        state.entity_manager.SetInactiveVid(*old_player_vid);
        RemoveToolStateForVid(state, *old_player_vid);
        state.UpdateSidForEntity(old_player_vid->id, graphics);
    }

    SetEntityAs(*source_entity, target_type);
    source_entity->vel = Vec2::New(0.0F, 0.0F);
    source_entity->acc = Vec2::New(0.0F, 0.0F);
    source_entity->rotation = 0.0F;
    source_entity->facing = facing;
    source_entity->SetCenter(spawn_center);

    if (keep_passives) {
        source_entity->effects.reset();
        if (effects != nullptr) {
            source_entity->effects.emplace() = *effects;
        }
    }
    if (keep_money) {
        source_entity->money = money;
    }
    if (keep_health) {
        source_entity->health = health;
    }

    RemoveToolStateForVid(state, replacement_vid);
    if (keep_tools && preserved_tools.has_value()) {
        EntityToolState copied_tools = *preserved_tools;
        copied_tools.owner_vid = replacement_vid;
        state.entity_tools.tool_states.push_back(copied_tools);
    } else {
        GrantFreshStarterTools(state, replacement_vid, target_type);
    }

    if (PlayerSlot* const slot = state.players.FindPrimaryLocal()) {
        slot->entity_vid = replacement_vid;
    }
    state.controlled_entity_vid = replacement_vid;
    state.UpdateSidForEntity(replacement_vid.id, graphics);
    debug.selected_entity_id = replacement_vid.id;
    debug.character_swap_status = std::string("Swapped to ") + GetEntityTypeName(target_type) + ".";
    if (source_entity->control_logic == nullptr) {
        debug.character_swap_status += " Warning: no control callback.";
    }
    return true;
}

void DrawCharacterSwapControls(
    DebugPlayback& debug,
    State& state,
    const Graphics& graphics,
    Entity* selected_entity
) {
    ImGui::SeparatorText("Character Swap");
    if (IsPeerDebugWorldMutationDisabled(state)) {
        ImGui::TextDisabled("Disabled on multiplayer peers until debug/admin commands are coordinator-routed.");
        return;
    }
    if (debug.playback_active) {
        ImGui::TextDisabled("Character swapping disabled during playback.");
        return;
    }

    ImGui::TextDisabled("Source: controlled entity, then player, then selected entity.");
    ImGui::InputText(
        "Search##character_swap_search",
        debug.character_swap_search.data(),
        debug.character_swap_search.size()
    );
    ImGui::SameLine();
    if (ImGui::Button("Clear##character_swap_search_clear")) {
        debug.character_swap_search[0] = '\0';
    }

    const char* current_swap_name = GetEntityTypeName(debug.character_swap_entity_type);
    if (ImGui::BeginCombo("Swap Type", current_swap_name)) {
        const std::vector<EntityType> sorted_spawn_types = BuildSortedSpawnTypes();
        for (const EntityType type_ : sorted_spawn_types) {
            const char* type_name = GetEntityTypeName(type_);
            if (!SpawnSearchMatches(debug.character_swap_search.data(), type_name)) {
                continue;
            }
            const bool selected = debug.character_swap_entity_type == type_;
            if (ImGui::Selectable(type_name, selected)) {
                debug.character_swap_entity_type = type_;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Checkbox("Fresh Spawn", &debug.character_swap_fresh);
    ImGui::SameLine();
    ImGui::TextDisabled("fresh uses archetype defaults; keep flags copy selected state after reset");
    ImGui::Checkbox("Keep Passives", &debug.character_swap_keep_passives);
    ImGui::SameLine();
    ImGui::Checkbox("Keep Money", &debug.character_swap_keep_money);
    ImGui::Checkbox("Keep Health", &debug.character_swap_keep_health);
    ImGui::SameLine();
    ImGui::Checkbox("Keep Tools", &debug.character_swap_keep_tools);

    if (ImGui::Button("Swap Controlled Character")) {
        SwapControlledCharacter(debug, state, graphics, selected_entity);
    }

    ImGui::SeparatorText("Default Spawn");
    ImGui::Checkbox("Spawn As Default Type", &debug.default_spawn_enabled);
    ImGui::SameLine();
    if (ImGui::Button("Use Swap Type##default_spawn_use_swap_type")) {
        debug.default_spawn_entity_type = debug.character_swap_entity_type;
    }

    const char* current_default_spawn_name = GetEntityTypeName(debug.default_spawn_entity_type);
    if (ImGui::BeginCombo("Default Spawn Type", current_default_spawn_name)) {
        const std::vector<EntityType> sorted_spawn_types = BuildSortedSpawnTypes();
        for (const EntityType type_ : sorted_spawn_types) {
            const char* type_name = GetEntityTypeName(type_);
            if (!SpawnSearchMatches(debug.character_swap_search.data(), type_name)) {
                continue;
            }
            const bool selected = debug.default_spawn_entity_type == type_;
            if (ImGui::Selectable(type_name, selected)) {
                debug.default_spawn_entity_type = type_;
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

bool SpawnDebugEntity(
    DebugPlayback& debug,
    State& state,
    const Graphics& graphics,
    EntityType type_,
    const Entity* selected_entity
) {
    if (IsPeerDebugWorldMutationDisabled(state)) {
        debug.spawn_status =
            "Debug spawning is disabled on multiplayer peers until admin commands are coordinator-routed.";
        return false;
    }
    if (type_ == EntityType::None) {
        debug.spawn_status = "Select an entity type first.";
        return false;
    }

    if (debug.spawn_center_on_selected && selected_entity == nullptr) {
        debug.spawn_status = "No active selected entity to center spawn on.";
        return false;
    }

    std::optional<VID> holding_player_vid;
    if (debug.spawn_held_by_player) {
        Entity* const player = GetPrimaryLocalPlayerMut(state);
        if (player == nullptr) {
            debug.spawn_status = "No player to hold spawned entity.";
            return false;
        }
        if (player->holding_vid.has_value()) {
            debug.spawn_status = "Player is already holding something.";
            return false;
        }
        holding_player_vid = player->vid;
    }

    Vec2 spawn_center = graphics.ScreenToWc(state.playing_inputs.mouse_pos);
    if (debug.spawn_center_on_selected && selected_entity != nullptr) {
        spawn_center = selected_entity->GetCenter();
    }

    Entity* const spawned = world_ops::SpawnEntity(state, type_, [spawn_center](Entity& entity) {
        entity.vel = Vec2::New(0.0F, 0.0F);
        entity.acc = Vec2::New(0.0F, 0.0F);
        entity.SetCenter(spawn_center);
    });
    if (spawned == nullptr) {
        debug.spawn_status = "Spawn failed.";
        return false;
    }

    if (debug.spawn_held_by_player) {
        if (Entity* const player = state.entity_manager.GetEntityMut(*holding_player_vid)) {
            player->holding_vid = spawned->vid;
            player->holding = true;
            player->holding_timer = kDefaultHoldingTimer;
            spawned->held_by_vid = player->vid;
            spawned->attachment_mode = AttachmentMode::Held;
            spawned->has_physics = false;
            spawned->can_collide = false;
            spawned->facing = player->facing;
            spawned->SetCenter(player->GetCenter());
            world_ops::MarkEntityHeld(state, *player, *spawned);
            world_ops::PatchEntityState(state, *player, *player);
            world_ops::PatchEntityState(state, *player, *spawned);
            debug.spawn_status =
                std::string("Spawned and attached ") + GetEntityTypeName(type_) + ".";
        }
    } else {
        debug.spawn_status = std::string("Spawned ") + GetEntityTypeName(type_) + ".";
    }

    state.UpdateSidForEntity(spawned->vid.id, graphics);
    debug.selected_entity_id = spawned->vid.id;
    return true;
}

} // namespace

void DrawEntityInspector(DebugPlayback& debug, State& state, const Graphics& graphics) {
    if (!debug.entity_inspector_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(12.0F, 300.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Entities", &debug.entity_inspector_visible)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginListBox("Entities", ImVec2(260.0F, 220.0F))) {
        for (std::size_t i = 0; i < state.entity_manager.entities.size(); ++i) {
            const Entity& entity = state.entity_manager.entities[i];
            if (!entity.active) {
                continue;
            }

            char label[128];
            std::snprintf(
                label,
                sizeof(label),
                "%zu: %s##entity_%zu",
                i,
                EntityTypeToString(entity.type_),
                i
            );
            const bool selected = debug.selected_entity_id == i;
            if (ImGui::Selectable(label, selected)) {
                debug.selected_entity_id = i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }

    if (debug.selected_entity_id >= state.entity_manager.entities.size()) {
        debug.selected_entity_id = 0;
    }

    Entity* selected_entity = nullptr;
    if (!state.entity_manager.entities.empty()) {
        Entity& entity = state.entity_manager.entities[debug.selected_entity_id];
        if (entity.active) {
            selected_entity = &entity;
        }
    }

    if (IsPeerDebugWorldMutationDisabled(state)) {
        debug.pending_spawn_at_mouse = false;
        ImGui::SeparatorText("Spawner");
        ImGui::TextDisabled("Entity spawning disabled on multiplayer peers.");
    } else if (debug.playback_active) {
        debug.pending_spawn_at_mouse = false;
        ImGui::SeparatorText("Spawner");
        ImGui::TextDisabled("Entity spawning disabled during playback.");
    } else {
        ImGui::SeparatorText("Spawner");
        ImGui::InputText("Search", debug.spawn_search.data(), debug.spawn_search.size());
        ImGui::SameLine();
        if (ImGui::Button("Clear Search")) {
            debug.spawn_search[0] = '\0';
        }

        const char* current_spawn_name = GetEntityTypeName(debug.spawn_entity_type);
        if (ImGui::BeginCombo("Spawn Type", current_spawn_name)) {
            const std::vector<EntityType> sorted_spawn_types = BuildSortedSpawnTypes();
            for (const EntityType type_ : sorted_spawn_types) {
                const char* type_name = GetEntityTypeName(type_);
                if (!SpawnSearchMatches(debug.spawn_search.data(), type_name)) {
                    continue;
                }
                const bool selected = debug.spawn_entity_type == type_;
                if (ImGui::Selectable(type_name, selected)) {
                    debug.spawn_entity_type = type_;
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

        const Vec2 mouse_world = graphics.ScreenToWc(state.playing_inputs.mouse_pos);
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
                SpawnDebugEntity(debug, state, graphics, debug.spawn_entity_type, selected_entity);
            }
        } else {
            const char* spawn_button_label =
                debug.spawn_center_on_selected ? "Spawn Entity" : "Arm Mouse Spawn";
            if (ImGui::Button(spawn_button_label)) {
                if (debug.spawn_center_on_selected) {
                    SpawnDebugEntity(debug, state, graphics, debug.spawn_entity_type, selected_entity);
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

    DrawCharacterSwapControls(debug, state, graphics, selected_entity);

    if (selected_entity == nullptr) {
        ImGui::TextUnformatted("No active entity selected.");
        ImGui::End();
        SyncDebugUiSettings(debug, state);
        return;
    }

    Entity& entity = *selected_entity;
    const AABB aabb = entity.GetAABB();
    ImGui::Separator();
    ImGui::Text("Type: %s", EntityTypeToString(entity.type_));
    ImGui::Text(
        "Controlled: %s",
        state.controlled_entity_vid.has_value() && entity.vid == *state.controlled_entity_vid
            ? "true"
            : "false"
    );
    if (ImGui::Button("Control Selected")) {
        state.controlled_entity_vid = entity.vid;
    }
    if (const std::optional<VID> player_vid = FindPrimaryLocalPlayerVid(state)) {
        ImGui::SameLine();
        if (ImGui::Button("Control Player")) {
            state.controlled_entity_vid = *player_vid;
        }
    }
    ImGui::Text("Animation Id: %u", entity.frame_data_animator.animation_id);
    ImGui::Text("Condition: %s", ConditionToString(entity.condition));
    ImGui::Text("AI: %s", AiStateToString(entity.ai_state));
    const bool peer_mutation_disabled = IsPeerDebugWorldMutationDisabled(state);
    bool entity_state_changed = false;
    bool player_state_changed = false;
    if (peer_mutation_disabled) {
        ImGui::BeginDisabled();
    }
    bool stone = entity.stone;
    if (ImGui::Checkbox("Stone", &stone)) {
        if (stone) {
            EnableStone(entity);
        } else {
            DisableStone(entity);
        }
        entity_state_changed = true;
    }
    if (ImGui::Checkbox("Wanted", &entity.wanted)) {
        entity_state_changed = true;
        player_state_changed = true;
    }
    entity_state_changed |= ImGui::Checkbox("Crusher/Pusher", &entity.crusher_pusher);
    entity_state_changed |= ImGui::Checkbox("Pushable", &entity.pushable);
    entity_state_changed |= ImGui::DragFloat("Push Acc", &entity.push_acc, 0.01F, 0.0F, 5.0F, "%.2f");
    if (peer_mutation_disabled) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("Entity edits are disabled on multiplayer peers until admin commands are coordinator-routed.");
    }
    ImGui::Text("Facing: %s", LeftOrRightToString(entity.facing));
    ImGui::Text("Grounded: %s", entity.grounded ? "true" : "false");
    ImGui::Text("Pos: (%.2f, %.2f)", entity.pos.x, entity.pos.y);
    ImGui::Text("Vel: (%.2f, %.2f)", entity.vel.x, entity.vel.y);
    ImGui::Text("Acc: (%.2f, %.2f)", entity.acc.x, entity.acc.y);
    ImGui::Text("Size: (%.2f, %.2f)", entity.size.x, entity.size.y);
    ImGui::Text("AABB TL: (%.2f, %.2f)", aabb.tl.x, aabb.tl.y);
    ImGui::Text("AABB BR: (%.2f, %.2f)", aabb.br.x, aabb.br.y);
    ImGui::Text("Coyote: %u", entity.coyote_time);
    ImGui::Text("Fall timer: %u", entity.fall_timer);
    ImGui::Text("Health: %u", entity.health);
    ImGui::Text("Money: %u", entity.money);
    if (!peer_mutation_disabled) {
        int money = static_cast<int>(entity.money);
        ImGui::SetNextItemWidth(120.0F);
        if (ImGui::InputInt("Edit Money", &money)) {
            entity.money = static_cast<std::uint32_t>(std::max(0, money));
            entity_state_changed = true;
            player_state_changed = true;
        }
    }
    ImGui::SeparatorText("Effects");
    if (peer_mutation_disabled) {
        ImGui::BeginDisabled();
    }
    if (DrawEntityEffectsEditor(entity)) {
        entity_state_changed = true;
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
        const bool has_sticky_bombs = HasStickyBombTool(state.entity_tools, entity.vid);
        ImGui::Text("Sticky bombs: %s", has_sticky_bombs ? "true" : "false");
        ImGui::SameLine();
        if (ImGui::Button("Upgrade Bombs To Sticky")) {
            state.entity_tools.UpgradeBombsToSticky(entity.vid);
            player_state_changed = true;
        }
        for (std::size_t slot_index = 0; slot_index < kToolSlotCount; ++slot_index) {
            ToolSlot preview_slot{};
            if (const std::optional<ToolKind> preferred_tool_kind =
                    FindPreferredToolKindForSlotIndex(slot_index)) {
                FillToolSlot(preview_slot, *preferred_tool_kind, 0, false);
            }
            ToolSlot* slot = state.entity_tools.FindToolSlotMut(entity.vid, slot_index);
            if (slot == nullptr) {
                slot = &preview_slot;
            }
            ImGui::PushID(static_cast<int>(slot_index));
            ImGui::SeparatorText(slot_index == 0 ? "Tool Slot 1" : "Tool Slot 2");
            bool active = slot->active;
            if (ImGui::Checkbox("Active", &active)) {
                ToolSlot& owned_slot = state.entity_tools.EnsureToolSlot(entity.vid, slot_index);
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
                        ToolSlot& owned_slot = state.entity_tools.EnsureToolSlot(entity.vid, slot_index);
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
                ToolSlot& owned_slot = state.entity_tools.EnsureToolSlot(entity.vid, slot_index);
                owned_slot = *slot;
                owned_slot.count = static_cast<std::uint16_t>(std::clamp(count, 0, 65535));
                slot = &owned_slot;
                player_state_changed = true;
            }
            ImGui::SetNextItemWidth(120.0F);
            if (ImGui::InputInt("Cooldown", &cooldown)) {
                ToolSlot& owned_slot = state.entity_tools.EnsureToolSlot(entity.vid, slot_index);
                owned_slot = *slot;
                owned_slot.cooldown = static_cast<std::uint16_t>(std::clamp(cooldown, 0, 65535));
                player_state_changed = true;
            }
            ImGui::PopID();
        }
    }
    ReplicateDebugEntityEdit(state, entity, entity_state_changed, player_state_changed);
    ImGui::Text("Climbing: %s", entity.IsClimbing() ? "true" : "false");
    ImGui::Text("Holding: %s", entity.holding ? "true" : "false");

    if (entity.frame_data_animator.HasAnimation()) {
        const FrameDataAnimation* animation =
            graphics.frame_data_db.FindAnimation(entity.frame_data_animator.animation_id);
        if (animation != nullptr) {
            ImGui::Text("Anim: %s", animation->name.c_str());
            ImGui::Text(
                "Anim Frame: %zu / %zu",
                entity.frame_data_animator.current_frame,
                animation->frame_indices.empty() ? 0 : animation->frame_indices.size() - 1
            );
            const FrameData* frame_data = graphics.frame_data_db.FindFrame(
                entity.frame_data_animator.animation_id,
                entity.frame_data_animator.current_frame
            );
            if (frame_data != nullptr) {
                ImGui::Text("Frame Duration: %d", frame_data->duration);
                ImGui::Text(
                    "Sample: (%d, %d, %d, %d)",
                    frame_data->sample_rect.x,
                    frame_data->sample_rect.y,
                    frame_data->sample_rect.w,
                    frame_data->sample_rect.h
                );
                ImGui::Text(
                    "Draw Offset: (%d, %d)",
                    frame_data->draw_offset.x,
                    frame_data->draw_offset.y
                );
                ImGui::Text(
                    "PBox: (%d, %d, %d, %d)",
                    frame_data->pbox.x,
                    frame_data->pbox.y,
                    frame_data->pbox.w,
                    frame_data->pbox.h
                );
                ImGui::Text(
                    "CBox: (%d, %d, %d, %d)",
                    frame_data->cbox.x,
                    frame_data->cbox.y,
                    frame_data->cbox.w,
                    frame_data->cbox.h
                );
            }
        }
    }

    ImGui::End();
    SyncDebugUiSettings(debug, state);
}

} // namespace splonks::debug_playback_internal
