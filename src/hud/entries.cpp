#include "hud/entries.hpp"

#include "effects.hpp"
#include "entity/archetype.hpp"
#include "entity.hpp"
#include "state.hpp"

namespace splonks {

namespace {

void AddEffectHudEntries(std::vector<HudEntry>& entries, const Entity& player) {
    const EntityEffects* const effects = player.effects.get();
    const std::uint8_t effect_count = effects != nullptr ? effects->count : 0;
    for (std::size_t i = 0; i < effect_count; ++i) {
        const EffectInstance& effect = effects->effects[i];
        const EffectArchetype& archetype = GetEffectArchetype(effect.id);
        if (archetype.ui_kind == EffectUiKind::Hidden ||
            archetype.icon_animation_id == kInvalidFrameDataId) {
            continue;
        }

        HudEntry entry{};
        entry.key = HudEntryKey{
            .source = HudEntrySource::Effect,
            .id = static_cast<std::uint32_t>(effect.id),
        };
        entry.icon_animation_id = archetype.icon_animation_id;
        entry.count_text = archetype.hud_count_text != nullptr ? archetype.hud_count_text(effect)
                                                               : std::nullopt;
        entry.count_anchor = archetype.hud_count_anchor;
        entry.extra_right_padding = entry.count_text.has_value() ? 12 : 0;
        entry.style = archetype.ui_kind == EffectUiKind::Temporary ? HudEntryStyle::Flashing
                                                                   : HudEntryStyle::Normal;
        entries.push_back(std::move(entry));
    }
}

FrameDataId GetEntityHudIcon(const Entity& entity) {
    return entity.frame_data_animator.animation_id;
}

HudEntry BuildItemHudEntry(const State& state, HudEntrySource source, const Entity& item) {
    HudEntry entry{};
    entry.key = HudEntryKey{
        .source = source,
        .id = item.vid.id,
    };
    entry.icon_animation_id = GetEntityHudIcon(item);

    const EntityArchetype& archetype = GetEntityArchetype(item.type_);
    if (archetype.build_hud_entry != nullptr) {
        archetype.build_hud_entry(item, state, source, entry);
    }

    return entry;
}

void AddItemHudEntry(
    std::vector<HudEntry>& entries,
    const State& state,
    HudEntrySource source,
    const Entity* item
) {
    if (item == nullptr || !item->active) {
        return;
    }

    HudEntry entry = BuildItemHudEntry(state, source, *item);
    if (entry.icon_animation_id != kInvalidFrameDataId) {
        entries.push_back(std::move(entry));
    }
}

void AddHeldItemHudEntries(std::vector<HudEntry>& entries, const State& state, const Entity& player) {
    const Entity* const held =
        player.holding_vid.has_value() ? state.entity_manager.GetEntity(*player.holding_vid) : nullptr;
    AddItemHudEntry(entries, state, HudEntrySource::HeldItem, held);
}

void AddBackItemHudEntries(std::vector<HudEntry>& entries, const State& state, const Entity& player) {
    const Entity* const back =
        player.back_vid.has_value() ? state.entity_manager.GetEntity(*player.back_vid) : nullptr;
    AddItemHudEntry(entries, state, HudEntrySource::BackItem, back);
}

} // namespace

std::vector<HudEntry> BuildEffectHudEntries(const State& state, const Entity& player) {
    (void)state;
    std::vector<HudEntry> entries;
    AddEffectHudEntries(entries, player);
    return entries;
}

std::vector<HudEntry> BuildEquipmentHudEntries(const State& state, const Entity& player) {
    std::vector<HudEntry> entries;
    AddHeldItemHudEntries(entries, state, player);
    AddBackItemHudEntries(entries, state, player);
    return entries;
}

} // namespace splonks
