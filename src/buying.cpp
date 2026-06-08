#include "buying.hpp"

#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace splonks {

namespace {

Vec2 GetAabbCenter(const AABB& aabb) {
    return (aabb.tl + aabb.br) / 2.0F;
}

std::int64_t GetBuyPromptDistanceSq(
    const Vec2& buyer_center,
    const Vec2& item_center,
    const Stage& stage
) {
    const Vec2 delta = GetNearestWorldDelta(stage, buyer_center, item_center);
    const sim::Vec2 fixed_delta = sim::ToSimVec2(delta);
    const std::int64_t dx = fixed_delta.x.raw_value();
    const std::int64_t dy = fixed_delta.y.raw_value();
    return (dx * dx) + (dy * dy);
}

struct OverlappingBuyableEnt {
    std::size_t ent_idx = 0;
    std::int64_t distance_sq = 0;
    AABB nearest_aabb = AABB::New(Vec2::New(0.0F, 0.0F), Vec2::New(0.0F, 0.0F));
};

std::vector<OverlappingBuyableEnt> FindOverlappingBuyableEnts(
    const State& state,
    const Graphics& graphics,
    std::size_t buyer_idx
) {
    if (buyer_idx >= state.ents.ents.size()) {
        return {};
    }

    const Ent& buyer = state.ents.ents[buyer_idx];
    if (!buyer.active) {
        return {};
    }

    const AABB buyer_aabb = ents::common::GetRenderContactAabbForEnt(buyer, graphics);
    const Vec2 buyer_center = GetAabbCenter(buyer_aabb);
    const std::vector<VID> results = QueryEntsInAabb(state, buyer_aabb, buyer.vid);

    std::vector<OverlappingBuyableEnt> overlaps;
    overlaps.reserve(results.size());
    for (const VID& vid : results) {
        const Ent* const item = state.ents.GetEnt(vid);
        if (item == nullptr || !item->active || !item->buyable.active) {
            continue;
        }

        const AABB item_aabb = GetNearestWorldAabb(
            state.stage,
            buyer_center,
            ents::common::GetRenderContactAabbForEnt(*item, graphics)
        );
        if (!AabbsIntersect(buyer_aabb, item_aabb)) {
            continue;
        }

        overlaps.push_back(OverlappingBuyableEnt{
            .ent_idx = vid.id,
            .distance_sq = GetBuyPromptDistanceSq(buyer_center, GetAabbCenter(item_aabb), state.stage),
            .nearest_aabb = item_aabb,
        });
    }

    std::sort(
        overlaps.begin(),
        overlaps.end(),
        [](const OverlappingBuyableEnt& a, const OverlappingBuyableEnt& b) {
            if (a.distance_sq != b.distance_sq) {
                return a.distance_sq < b.distance_sq;
            }
            return a.ent_idx < b.ent_idx;
        }
    );
    return overlaps;
}

} // namespace

std::optional<std::size_t> FindOverlappingBuyableEntIdx(
    const State& state,
    const Graphics& graphics,
    std::size_t buyer_idx
) {
    const std::vector<OverlappingBuyableEnt> overlaps =
        FindOverlappingBuyableEnts(state, graphics, buyer_idx);
    if (overlaps.empty()) {
        return std::nullopt;
    }
    return overlaps.front().ent_idx;
}

void ClearEntBuyableState(Ent& ent) {
    ent.buyable.active = false;
    ent.buyable.display_quantity = 0;
    ent.buyable.display_icon_anim_id.reset();
    ent.buyable.shop_owner_vid.reset();
    ent.buyable.on_try_buy = nullptr;
}

void ConfigureEntAsBuyable(Ent& ent, std::uint32_t price) {
    ent.buyable.active = true;
    ent.buyable.display_quantity = price;
    ent.buyable.display_icon_anim_id = aframe_ids::GoldIcon;
    if (ent.buyable.on_try_buy == nullptr) {
        ent.buyable.on_try_buy = TryBuyEntForMoney;
    }
}

bool TrySpendMoney(std::size_t buyer_idx, std::uint32_t amount, State& state, Audio& audio) {
    (void)audio;
    if (buyer_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& buyer = state.ents.ents[buyer_idx];
    if (!buyer.active || buyer.money < amount) {
        return false;
    }

    buyer.money -= amount;
    if (amount > 0) {
        (void)PlayEntCenterSoundEmitter(state, buyer, audio_asset_ids::Gold);
    }
    return true;
}

bool TryBuyEntForMoney(
    std::size_t ent_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    if (ent_idx >= state.ents.ents.size() ||
        buyer_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& buyer = state.ents.ents[buyer_idx];
    Ent& item = state.ents.ents[ent_idx];
    if (!buyer.active || !item.active || !item.buyable.active) {
        return false;
    }
    if (!TrySpendMoney(buyer_idx, item.buyable.display_quantity, state, audio)) {
        return false;
    }

    ClearEntBuyableState(item);
    if (TryCollectInventoryPickup(state, buyer, item)) {
        (void)world_ops::DeactivateEnt(state, item.vid);
        state.UpdateSidForEnt(ent_idx, graphics);
    }
    return true;
}

void AddBuyPromptsForPlayer(State& state, const Graphics& graphics) {
    const std::optional<VID> buyer_vid = FindPrimaryLocalPlayerVid(state);
    if (!buyer_vid.has_value()) {
        return;
    }
    const std::size_t buyer_idx = buyer_vid->id;
    if (buyer_idx >= state.ents.ents.size()) {
        return;
    }

    const Ent& buyer = state.ents.ents[buyer_idx];
    if (!buyer.active) {
        return;
    }

    const std::vector<OverlappingBuyableEnt> overlaps =
        FindOverlappingBuyableEnts(state, graphics, buyer_idx);
    for (const OverlappingBuyableEnt& overlap : overlaps) {
        const Ent& item = state.ents.ents[overlap.ent_idx];
        state.AddWorldPrompt(WorldPrompt{
            .world_pos = Vec2::New((overlap.nearest_aabb.tl.x + overlap.nearest_aabb.br.x) * 0.5F, overlap.nearest_aabb.tl.y - 6.0F),
            .action_text = "RB",
            .message_text = "",
            .show_down_arrow = false,
            .quantity = item.buyable.display_quantity,
            .icon_anim_id = item.buyable.display_icon_anim_id,
        });
    }
}

bool TryBuyEnt(
    std::size_t ent_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    if (ent_idx >= state.ents.ents.size() ||
        buyer_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& item = state.ents.ents[ent_idx];
    if (!item.active || !item.buyable.active || item.buyable.on_try_buy == nullptr) {
        return false;
    }

    return item.buyable.on_try_buy(ent_idx, buyer_idx, state, graphics, audio);
}

} // namespace splonks
