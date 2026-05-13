#include "hud/entries.hpp"

#include "effects.hpp"
#include "ent/spec.hpp"
#include "ent.hpp"
#include "state.hpp"

namespace splonks {

namespace {

void AddEffectHudEntries(std::vector<HudEntry>& entries, const Ent& player) {
    const EntEffects* const effects = player.effects.get();
    const std::uint8_t effect_count = effects != nullptr ? effects->count : 0;
    for (std::size_t i = 0; i < effect_count; ++i) {
        const EffectInstance& effect = effects->effects[i];
        const EffectSpec& spec = GetEffectSpec(effect.id);
        if (spec.ui_kind == EffectUiKind::Hidden ||
            spec.icon_anim_id == kInvalidAFrameId) {
            continue;
        }

        HudEntry entry{};
        entry.key = HudEntryKey{
            .source = HudEntrySource::Effect,
            .id = static_cast<std::uint32_t>(effect.id),
        };
        entry.icon_anim_id = spec.icon_anim_id;
        entry.count_text = spec.hud_count_text != nullptr ? spec.hud_count_text(effect)
                                                               : std::nullopt;
        entry.count_anchor = spec.hud_count_anchor;
        entry.extra_right_padding = entry.count_text.has_value() ? 12 : 0;
        entry.style = spec.ui_kind == EffectUiKind::Temporary ? HudEntryStyle::Flashing
                                                                   : HudEntryStyle::Normal;
        entries.push_back(std::move(entry));
    }
}

AFrameId GetEntHudIcon(const Ent& ent) {
    return ent.aframe_animator.anim_id;
}

HudEntry BuildItemHudEntry(const State& state, HudEntrySource source, const Ent& item) {
    HudEntry entry{};
    entry.key = HudEntryKey{
        .source = source,
        .id = item.vid.id,
    };
    entry.icon_anim_id = GetEntHudIcon(item);

    const EntSpec& spec = GetEntSpec(item.type_);
    if (spec.build_hud_entry != nullptr) {
        spec.build_hud_entry(item, state, source, entry);
    }

    return entry;
}

void AddItemHudEntry(
    std::vector<HudEntry>& entries,
    const State& state,
    HudEntrySource source,
    const Ent* item
) {
    if (item == nullptr || !item->active) {
        return;
    }

    HudEntry entry = BuildItemHudEntry(state, source, *item);
    if (entry.icon_anim_id != kInvalidAFrameId) {
        entries.push_back(std::move(entry));
    }
}

void AddHeldItemHudEntries(std::vector<HudEntry>& entries, const State& state, const Ent& player) {
    const Ent* const held =
        player.holding_vid.has_value() ? state.ents.GetEnt(*player.holding_vid) : nullptr;
    AddItemHudEntry(entries, state, HudEntrySource::HeldItem, held);
}

void AddBackItemHudEntries(std::vector<HudEntry>& entries, const State& state, const Ent& player) {
    const Ent* const back =
        player.back_vid.has_value() ? state.ents.GetEnt(*player.back_vid) : nullptr;
    AddItemHudEntry(entries, state, HudEntrySource::BackItem, back);
}

} // namespace

std::vector<HudEntry> BuildEffectHudEntries(const State& state, const Ent& player) {
    (void)state;
    std::vector<HudEntry> entries;
    AddEffectHudEntries(entries, player);
    return entries;
}

std::vector<HudEntry> BuildEquipmentHudEntries(const State& state, const Ent& player) {
    std::vector<HudEntry> entries;
    AddHeldItemHudEntries(entries, state, player);
    AddBackItemHudEntries(entries, state, player);
    return entries;
}

} // namespace splonks
