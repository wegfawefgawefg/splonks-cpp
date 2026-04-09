#pragma once

#include "entity_vid.hpp"
#include "math_types.hpp"
#include "utils.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace splonks {

struct VIDAABB {
    VID vid;
    AABB aabb;
};

struct SIDCell {
    int x = 0;
    int y = 0;
};

struct SIDRecord {
    bool active = false;
    VID vid;
    AABB aabb;
    std::vector<SIDCell> cells;
};

class SID {
  public:
    static SID New();

    void Clear();
    void Insert(const VID& vid, const Vec2& pos, const Vec2& size);
    void Upsert(const VID& vid, const AABB& aabb);
    void Remove(const VID& vid);
    std::vector<VID> Query(const Vec2& top_left, const Vec2& bottom_right) const;
    std::vector<VID> QueryExclude(const Vec2& top_left, const Vec2& bottom_right,
                                  const VID& exclude_vid) const;
    std::vector<VIDAABB> QueryForVIDAABBsExclude(const Vec2& top_left, const Vec2& bottom_right,
                                                 const VID& exclude_vid) const;

  private:
    std::unordered_map<std::int64_t, std::vector<VID>> buckets_;
    std::vector<SIDRecord> records_;
};

} // namespace splonks
