#include "sid.hpp"

#include "tile.hpp"

#include <algorithm>

namespace splonks {

namespace {

int FloorDiv(int value, int divisor) {
    if (divisor == 0) {
        return 0;
    }
    int result = value / divisor;
    const int remainder = value % divisor;
    if ((remainder != 0) && ((value < 0) != (divisor < 0))) {
        --result;
    }
    return result;
}

int GetCellCoord(FxScalar value) {
    return FloorDiv(value.floor_int(), static_cast<int>(kTileSize));
}

std::int64_t MakeCellKey(int x, int y) {
    return (static_cast<std::int64_t>(x) << 32) ^
           static_cast<std::uint32_t>(y);
}

std::vector<SIDCell> BuildCoveredCells(FxAABB aabb) {
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

void SID::Upsert(const VID& vid, FxAABB aabb) {
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

std::vector<VID> SID::Query(FxAABB area) const {
    const int min_x = GetCellCoord(area.tl.x);
    const int min_y = GetCellCoord(area.tl.y);
    const int max_x = GetCellCoord(area.br.x);
    const int max_y = GetCellCoord(area.br.y);

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
                if (!record.active || record.vid != vid ||
                    !gfxp::aabbs_intersect(record.aabb, area)) {
                    continue;
                }

                already_added[vid.id] = true;
                result.push_back(vid);
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const VID& left, const VID& right) {
        if (left.id != right.id) {
            return left.id < right.id;
        }
        return left.version < right.version;
    });
    return result;
}

std::vector<VID> SID::QueryExclude(FxAABB area, const VID& exclude_vid) const {
    std::vector<VID> result = Query(area);
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

} // namespace splonks
