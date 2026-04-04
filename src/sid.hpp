#pragma once

#include "entity_vid.hpp"
#include "math_types.hpp"
#include "utils.hpp"

#include <vector>

namespace splonks {

struct SIDNode {
    VID vid;
    Vec2 pos;
    Vec2 size;

    AABB GetAABB() const;
};

struct VIDAABB {
    VID vid;
    AABB aabb;
};

class SID {
  public:
    static SID New();

    void Insert(const VID& vid, const Vec2& pos, const Vec2& size);
    std::vector<VID> Query(const Vec2& top_left, const Vec2& bottom_right) const;
    std::vector<VID> QueryExclude(const Vec2& top_left, const Vec2& bottom_right,
                                  const VID& exclude_vid) const;
    std::vector<VIDAABB> QueryForVIDAABBsExclude(const Vec2& top_left, const Vec2& bottom_right,
                                                 const VID& exclude_vid) const;

  private:
    std::vector<SIDNode> nodes_;
};

} // namespace splonks
