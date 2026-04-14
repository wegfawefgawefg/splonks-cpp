#include "debug/playback_internal.hpp"

#include "entity/archetype.hpp"
#include "frame_data.hpp"
#include "tools/tool_archetype.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace splonks::debug_playback_internal {

namespace {

constexpr double kSpawnAtMouseDelaySeconds = 2.0;

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

bool SpawnDebugEntity(
    DebugPlayback& debug,
    State& state,
    const Graphics& graphics,
    EntityType type_,
    const Entity* selected_entity
) {
    if (type_ == EntityType::None) {
        debug.spawn_status = "Select an entity type first.";
        return false;
    }

    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        debug.spawn_status = "Entity budget exhausted.";
        return false;
    }

    Entity* const spawned = state.entity_manager.GetEntityMut(*vid);
    if (spawned == nullptr) {
        debug.spawn_status = "Spawn failed.";
        return false;
    }
    if (debug.spawn_center_on_selected && selected_entity == nullptr) {
        state.entity_manager.SetInactive(vid->id);
        debug.spawn_status = "No active selected entity to center spawn on.";
        return false;
    }

    SetEntityAs(*spawned, type_);
    spawned->vel = Vec2::New(0.0F, 0.0F);
    spawned->acc = Vec2::New(0.0F, 0.0F);

    Vec2 spawn_center = graphics.ScreenToWc(state.playing_inputs.mouse_pos);
    if (debug.spawn_center_on_selected && selected_entity != nullptr) {
        spawn_center = selected_entity->GetCenter();
    }
    spawned->SetCenter(spawn_center);

    if (debug.spawn_held_by_player) {
        if (!state.player_vid.has_value()) {
            debug.spawn_status = "No player to hold spawned entity.";
        } else if (Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid)) {
            if (player->holding_vid.has_value()) {
                debug.spawn_status = "Player is already holding something.";
            } else {
                player->holding_vid = spawned->vid;
                player->holding = true;
                player->holding_timer = kDefaultHoldingTimer;
                spawned->held_by_vid = player->vid;
                spawned->attachment_mode = AttachmentMode::Held;
                spawned->has_physics = false;
                spawned->can_collide = false;
                spawned->facing = player->facing;
                spawned->SetCenter(player->GetCenter());
                debug.spawn_status =
                    std::string("Spawned and attached ") + GetEntityTypeName(type_) + ".";
            }
        }
    } else {
        debug.spawn_status = std::string("Spawned ") + GetEntityTypeName(type_) + ".";
    }

    state.UpdateSidForEntity(vid->id, graphics);
    debug.selected_entity_id = vid->id;
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

    if (debug.playback_active) {
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

    if (selected_entity == nullptr) {
        ImGui::TextUnformatted("No active entity selected.");
        ImGui::End();
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
    if (state.player_vid.has_value()) {
        ImGui::SameLine();
        if (ImGui::Button("Control Player")) {
            state.controlled_entity_vid = *state.player_vid;
        }
    }
    ImGui::Text("Animation Id: %u", entity.frame_data_animator.animation_id);
    ImGui::Text("Condition: %s", ConditionToString(entity.condition));
    ImGui::Text("AI: %s", AiStateToString(entity.ai_state));
    bool stone = entity.stone;
    if (ImGui::Checkbox("Stone", &stone)) {
        if (stone) {
            EnableStone(entity);
        } else {
            DisableStone(entity);
        }
    }
    ImGui::Checkbox("Wanted", &entity.wanted);
    ImGui::Checkbox("Crusher/Pusher", &entity.crusher_pusher);
    ImGui::Text("Facing: %s", LeftOrRightToString(entity.facing));
    ImGui::Text("Grounded: %s", entity.grounded ? "true" : "false");
    ImGui::Text("Pos: (%.2f, %.2f)", entity.pos.x, entity.pos.y);
    ImGui::Text("Vel: (%.2f, %.2f)", entity.vel.x, entity.vel.y);
    ImGui::Text("Acc: (%.2f, %.2f)", entity.acc.x, entity.acc.y);
    ImGui::Text("Size: (%.2f, %.2f)", entity.size.x, entity.size.y);
    ImGui::Text("AABB TL: (%.2f, %.2f)", aabb.tl.x, aabb.tl.y);
    ImGui::Text("AABB BR: (%.2f, %.2f)", aabb.br.x, aabb.br.y);
    ImGui::Text("Coyote: %u", entity.coyote_time);
    ImGui::Text("Health: %u", entity.health);
    ImGui::Text("Money: %u", entity.money);
    ImGui::SeparatorText("Passive Items");
    for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(EntityPassiveItem::Count); ++i) {
        const EntityPassiveItem passive_item = static_cast<EntityPassiveItem>(i);
        bool has_passive_item = HasPassiveItem(entity, passive_item);
        if (ImGui::Checkbox(PassiveItemToString(passive_item), &has_passive_item)) {
            SetPassiveItem(entity, passive_item, has_passive_item);
        }
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Tools");
    if (debug.playback_active) {
        ImGui::TextDisabled("Tool editing disabled during playback.");
    } else {
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
            }
            ImGui::SetNextItemWidth(120.0F);
            if (ImGui::InputInt("Cooldown", &cooldown)) {
                ToolSlot& owned_slot = state.entity_tools.EnsureToolSlot(entity.vid, slot_index);
                owned_slot = *slot;
                owned_slot.cooldown = static_cast<std::uint16_t>(std::clamp(cooldown, 0, 65535));
            }
            ImGui::PopID();
        }
    }
    ImGui::Text("Climbing: %s", entity.IsClimbing() ? "true" : "false");
    ImGui::Text("Holding: %s", entity.holding ? "true" : "false");
    ImGui::Text("Horiz Controlled: %s", entity.IsHorizontallyControlled() ? "true" : "false");

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
