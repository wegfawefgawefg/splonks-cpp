#pragma once

#include "math_types.hpp"

#include <vector>

namespace splonks {

struct Graphics;
struct Stage;

struct VisibleWorldRect {
    FVec2 tl;
    FVec2 br;
};

VisibleWorldRect GetVisibleWorldRect(const Graphics& graphics);
std::vector<FVec2> GetVisibleWrappedRenderOffsets(const Stage& stage, const Graphics& graphics);

} // namespace splonks
