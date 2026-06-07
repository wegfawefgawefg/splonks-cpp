#include "ents/shop_tile_triggers.hpp"

#include "ents/shop.hpp"

namespace splonks::ents::shop {

namespace {

constexpr const char* kShopVandalismTriggerLabel = "shop vandalism trigger";

} // namespace

StageTileTrigger MakeShopVandalismTileTrigger(const IVec2& tile_pos, std::uint32_t target_spawn_index) {
    return StageTileTrigger{
        .tile_pos = tile_pos,
        .kind = StageTileTriggerKind::Destroyed,
        .target_spawn_index = target_spawn_index,
        .on_triggered = OnShopVandalismTileDestroyed,
        .debug_label = kShopVandalismTriggerLabel,
    };
}

StageTileTrigger MakeShopVandalismTileTrigger(const IVec2& tile_pos, VID target_vid) {
    return StageTileTrigger{
        .tile_pos = tile_pos,
        .kind = StageTileTriggerKind::Destroyed,
        .target_vid = target_vid,
        .on_triggered = OnShopVandalismTileDestroyed,
        .debug_label = kShopVandalismTriggerLabel,
    };
}

void OnShopVandalismTileDestroyed(
    const StageTileTrigger& trigger,
    const IVec2&,
    State& state,
    Audio& audio
) {
    DisturbShopByVid(trigger.target_vid, state, audio);
}

} // namespace splonks::ents::shop
