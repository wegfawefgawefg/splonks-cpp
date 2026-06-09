#pragma once

#include "fxp.hpp"
#include "vid.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace splonks {

struct SIDCell {
    int x = 0;
    int y = 0;
};

struct SIDRecord {
    bool active = false;
    VID vid;
    FxAABB aabb;
    std::vector<SIDCell> cells;
};

class SID {
  public:
    static SID New();

    void Clear();
    void Upsert(const VID& vid, FxAABB aabb);
    void Remove(const VID& vid);
    std::vector<VID> Query(FxAABB area) const;
    std::vector<VID> QueryExclude(FxAABB area, const VID& exclude_vid) const;

  private:
    std::unordered_map<std::int64_t, std::vector<VID>> buckets_;
    std::vector<SIDRecord> records_;
};

} // namespace splonks
