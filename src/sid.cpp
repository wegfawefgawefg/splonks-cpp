#include "sid.hpp"

#include "tile.hpp"

#include <algorithm>
#include <cmath>

namespace splonks {

namespace {

constexpr float kSidCellSize = static_cast<float>(kTileSize);

int GetCellCoord(float value) {
    return static_cast<int>(std::floor(value / kSidCellSize));
}

std::int64_t MakeCellKey(int x, int y) {
    return (static_cast<std::int64_t>(x) << 32) ^
           static_cast<std::uint32_t>(y);
}

AABB BuildAabb(const Vec2& pos, const Vec2& size) {
    return AABB::New(pos, pos + size - Vec2::New(1.0F, 1.0F));
}

std::vector<SIDCell> BuildCoveredCells(const AABB& aabb) {
    const int min_x = GetCellCoord(aabb.tl.x);
    const int min_y = GetCellCoord(aabb.tl.y);
    const int max_x = GetCellCoord(aabb.br.x);
    const int max_y = GetCellCoord(aabb.br.y);

    std::vector<SIDCell> cells;
    cells.reserve(static_cast<std::size_t>((max_x - min_x + 1) * (max_y - min_y + 1)));
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            cells.push_back(SIDCell{.x = x, .y = y});
        }
    }
    return cells;
}

} // namespace

SID SID::New() {
    SID sid;
    return sid;
}

void SID::Clear() {
    buckets_.clear();
    records_.clear();
}

void SID::Insert(const VID& vid, const Vec2& pos, const Vec2& size) {
    Upsert(vid, BuildAabb(pos, size));
}

void SID::Upsert(const VID& vid, const AABB& aabb) {
    if (records_.size() <= vid.id) {
        records_.resize(vid.id + 1);
    }

    Remove(vid);

    SIDRecord& record = records_[vid.id];
    record.active = true;
    record.vid = vid;
    record.aabb = aabb;
    record.cells = BuildCoveredCells(aabb);
    for (const SIDCell& cell : record.cells) {
        buckets_[MakeCellKey(cell.x, cell.y)].push_back(vid);
    }
}

void SID::Remove(const VID& vid) {
    if (vid.id >= records_.size()) {
        return;
    }

    SIDRecord& record = records_[vid.id];
    if (!record.active) {
        return;
    }

    for (const SIDCell& cell : record.cells) {
        const std::int64_t key = MakeCellKey(cell.x, cell.y);
        auto bucket_it = buckets_.find(key);
        if (bucket_it == buckets_.end()) {
            continue;
        }

        std::vector<VID>& bucket = bucket_it->second;
        bucket.erase(
            std::remove_if(
                bucket.begin(),
                bucket.end(),
                [&](const VID& candidate) {
                    return candidate.id == record.vid.id &&
                           candidate.version == record.vid.version;
                }
            ),
            bucket.end()
        );
        if (bucket.empty()) {
            buckets_.erase(bucket_it);
        }
    }

    record.active = false;
    record.cells.clear();
}

std::vector<VID> SID::Query(const Vec2& top_left, const Vec2& bottom_right) const {
    const AABB query_box = AABB::New(top_left, bottom_right);
    const int min_x = GetCellCoord(query_box.tl.x);
    const int min_y = GetCellCoord(query_box.tl.y);
    const int max_x = GetCellCoord(query_box.br.x);
    const int max_y = GetCellCoord(query_box.br.y);

    std::vector<VID> result;
    std::vector<bool> already_added(records_.size(), false);
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const auto bucket_it = buckets_.find(MakeCellKey(x, y));
            if (bucket_it == buckets_.end()) {
                continue;
            }

            for (const VID& vid : bucket_it->second) {
                if (vid.id >= records_.size() || already_added[vid.id]) {
                    continue;
                }

                const SIDRecord& record = records_[vid.id];
                if (!record.active || record.vid != vid || !AabbsIntersect(record.aabb, query_box)) {
                    continue;
                }

                already_added[vid.id] = true;
                result.push_back(vid);
            }
        }
    }
    return result;
}

std::vector<VID> SID::QueryExclude(const Vec2& top_left, const Vec2& bottom_right,
                                   const VID& exclude_vid) const {
    std::vector<VID> result = Query(top_left, bottom_right);
    result.erase(
        std::remove_if(
            result.begin(),
            result.end(),
            [&](const VID& candidate) { return candidate == exclude_vid; }
        ),
        result.end()
    );
    return result;
}

std::vector<VIDAABB> SID::QueryForVIDAABBsExclude(const Vec2& top_left, const Vec2& bottom_right,
                                                  const VID& exclude_vid) const {
    const std::vector<VID> vids = QueryExclude(top_left, bottom_right, exclude_vid);
    std::vector<VIDAABB> result;
    result.reserve(vids.size());
    for (const VID& vid : vids) {
        if (vid.id >= records_.size()) {
            continue;
        }
        const SIDRecord& record = records_[vid.id];
        if (!record.active || record.vid != vid) {
            continue;
        }
        result.push_back(VIDAABB{
            .vid = vid,
            .aabb = record.aabb,
        });
    }
    return result;
}

} // namespace splonks
