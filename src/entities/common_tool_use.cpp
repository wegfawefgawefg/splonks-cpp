#include "entities/common.hpp"

#include "entities/bomb.hpp"
#include "entities/breakaway_container.hpp"
#include "entities/rope.hpp"

namespace splonks::entities::common {

namespace {

constexpr std::uint16_t kBombThrowDelayFrames = 8;
constexpr std::uint16_t kRopeThrowDelayFrames = 8;
constexpr std::uint16_t kPotThrowDelayFrames = 12;

} // namespace

bool TryUseToolSlot(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const ToolSlot* const tool_slot = state.FindToolSlot(entity.vid, tool_slot_index);
    if (tool_slot == nullptr || !tool_slot->active) {
        return false;
    }

    switch (tool_slot->kind) {
    case ToolKind::ThrowBomb:
        return TrySpawnAndThrowEntityFromTool(
            entity_idx,
            state,
            graphics,
            audio,
            tool_slot_index,
            trigger_pressed,
            kBombThrowDelayFrames,
            kThrownByImmunityDuration,
            [](Entity& spawned_entity) { bomb::SetEntityBomb(spawned_entity); }
        );
    case ToolKind::ThrowRope:
        return TrySpawnAndThrowEntityFromTool(
            entity_idx,
            state,
            graphics,
            audio,
            tool_slot_index,
            trigger_pressed,
            kRopeThrowDelayFrames,
            kThrownByImmunityDuration * 2,
            [](Entity& spawned_entity) { rope::SetEntityRope(spawned_entity); }
        );
    case ToolKind::ThrowPot:
        return TrySpawnAndThrowEntityFromTool(
            entity_idx,
            state,
            graphics,
            audio,
            tool_slot_index,
            trigger_pressed,
            kPotThrowDelayFrames,
            kThrownByImmunityDuration,
            [](Entity& spawned_entity) {
                breakaway_container::SetEntityBreakawayContainer(
                    spawned_entity,
                    EntityType::Pot
                );
            }
        );
    }

    return false;
}

} // namespace splonks::entities::common
